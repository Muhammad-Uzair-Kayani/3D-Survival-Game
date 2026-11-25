"""Train and evaluate the native Frostworks policy using real Godot missions."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
from statistics import mean
import struct
import subprocess
import tempfile
import time

os.environ.setdefault("OPENBLAS_NUM_THREADS", "1")
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
FEATURES = tuple(
    line.removeprefix("FEATURE(").removesuffix(")")
    for line in (Path(__file__).with_name("neural_features.def")
                 if Path(__file__).with_name("neural_features.def").is_file()
                 else ROOT / "modules/last_light/neural_features.def").read_text().splitlines()
)
PRIOR = FEATURES.index("teacher_prior")
HIDDEN = 24
MODES = {"Peaceful": 0, "Standard": 5, "Hostile": 8}
SEEDS = {
    "train": (91, 127, 241),
    "validation": (1613, 1699),
    "test": (9721, 9829, 9931),
}
TEMPERATURE = 0.20
DEFAULT_MODEL = ROOT / "game/content/frostworks-policy.llnp"
HEADER = struct.pack("<4sIII", b"LLNP", 1, len(FEATURES), HIDDEN)


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as file:
        temporary = Path(file.name)
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    try:
        for attempt in range(5):
            try:
                temporary.replace(path)
                return
            except PermissionError:
                if attempt == 4:
                    raise
                time.sleep(.1)
    finally:
        temporary.unlink(missing_ok=True)


def write_json(path, value):
    atomic(path, (json.dumps(value, indent=2) + "\n").encode())


def checkpoint_id(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class TrainingStopped(Exception):
    pass


class Policy:
    def __init__(self, seed=42):
        rng = np.random.default_rng(seed)
        width = len(FEATURES)
        self.linear = np.zeros(width, dtype=np.float32)
        self.linear[PRIOR] = 1
        self.units = rng.normal(0, .05, (HIDDEN, width)).astype(np.float32)
        self.bias = np.zeros(HIDDEN, dtype=np.float32)
        self.output = np.zeros(HIDDEN, dtype=np.float32)
        self.constant = np.zeros(1, dtype=np.float32)

    def parameters(self):
        return self.linear, self.units, self.bias, self.output, self.constant

    def score(self, x):
        hidden = np.tanh(x @ self.units.T + self.bias)
        return x @ self.linear + hidden @ self.output + self.constant, hidden

    def save(self, path):
        rows = np.column_stack((self.bias, self.units, self.output))
        weights = np.concatenate((self.linear, rows.ravel(), self.constant))
        if not np.isfinite(weights).all() or np.abs(weights).max() > 32:
            raise ValueError("Policy contains invalid weights")
        atomic(path, HEADER + weights.astype("<f4").tobytes())

    @classmethod
    def load(cls, path):
        raw = path.read_bytes()
        width = len(FEATURES)
        if len(raw) != len(HEADER) + (width + HIDDEN * (width + 2) + 1) * 4 or not raw.startswith(HEADER):
            raise ValueError(f"Incompatible native policy {path}")
        weights = np.frombuffer(raw, dtype="<f4", offset=len(HEADER))
        if not np.isfinite(weights).all() or np.abs(weights).max() > 32:
            raise ValueError(f"Invalid native policy {path}")
        policy = cls()
        policy.linear[:] = weights[:width]
        rows = weights[width:-1].reshape(HIDDEN, width + 2)
        policy.bias[:] = rows[:, 0]
        policy.units[:] = rows[:, 1:-1]
        policy.output[:] = rows[:, -1]
        policy.constant[:] = weights[-1:]
        return policy


@dataclass
class Example:
    features: np.ndarray
    teacher: int
    selected: int
    temperature: float
    probability: float
    stage: int
    reward: float


def reward_at(before, after, actor):
    # Use both the individual and the squad's real state transitions.
    team_progress = after["progress"] - before["progress"]
    reward = .45 * team_progress
    for old, new in zip(before["actors"], after["actors"]):
        actor_id, hp, state, revives, _ = old
        _, next_hp, next_state, next_revives, _ = new
        own = actor_id == actor
        reward += (next_hp - hp) * (.012 if own else .002)
        reward += (next_revives - revives) * (.65 if own else .15)
        if state != next_state:
            if next_state == 14:  # Extracted.
                reward += 2.0 if own else .8
            if next_state in (12, 13):  # Downed or dead.
                reward -= .7 if own else .25
    return reward


def examples(path, coverage):
    with path.open(encoding="utf-8") as file:
        outcomes = [row for line in file if (row := json.loads(line))["type"] == "outcome"]
    if not outcomes:
        raise RuntimeError(f"No outcome observations in {path}")
    frames = np.array([row["tick"] for row in outcomes])
    previous = {}
    # Inspect every observation for coverage. Train on strategic choices and on
    # tactical changes plus a regular sample of repeated decisions.
    with path.open(encoding="utf-8") as file:
        for line in file:
            row = json.loads(line)
            if row["type"] != "decision":
                continue
            x = np.asarray(row["features"], dtype=np.float32)
            if x.ndim != 2 or x.shape[1] != len(FEATURES) or not np.isfinite(x).all():
                raise RuntimeError(f"Invalid feature matrix in {path}")
            coverage.observe(row, x)
            if len(x) < 2:
                continue
            key = (row["actor"], row["stage"])
            signature = (row["ids"][row["teacher"]], tuple(tuple(choice) for choice in row["ids"]))
            last_signature, last_tick = previous.get(key, (None, -100))
            if row["stage"] == 0 and row["temperature"] == 0 and signature == last_signature and row["tick"] - last_tick < 80:
                continue
            previous[key] = signature, row["tick"]
            before = max(0, np.searchsorted(frames, row["tick"], side="right") - 1)
            after = min(len(frames) - 1, np.searchsorted(frames, row["tick"] + 480))
            reward = reward_at(outcomes[before], outcomes[after], row["actor"])
            reward += .3 * reward_at(outcomes[before], outcomes[-1], row["actor"])
            yield Example(x, row["teacher"], row["choice"], row["temperature"],
                          row["probability"], row["stage"], reward)


class Coverage:
    def __init__(self):
        self.low = np.full(len(FEATURES), np.inf)
        self.high = np.full(len(FEATURES), -np.inf)
        self.action_counts = [{}, {}, {}]
        self.selected_counts = [{}, {}, {}]
        self.groups = [0, 0, 0]

    def observe(self, row, features):
        np.minimum(self.low, features.min(axis=0), out=self.low)
        np.maximum(self.high, features.max(axis=0), out=self.high)
        stage = row["stage"]
        self.groups[stage] += 1
        for kind, _ in row["ids"]:
            self.action_counts[stage][kind] = self.action_counts[stage].get(kind, 0) + 1
        kind, _ = row["ids"][row["choice"]]
        self.selected_counts[stage][kind] = self.selected_counts[stage].get(kind, 0) + 1

    def report(self):
        return {
            "decision_groups": self.groups,
            "candidate_counts_by_kind": self.action_counts,
            "chosen_counts_by_kind": self.selected_counts,
            "observed_range": {
                feature: [float(low), float(high)] if np.isfinite(low) else None
                for feature, low, high in zip(FEATURES, self.low, self.high)
            },
        }


def train(policy, data, *, epochs, seed, reinforcement=False):
    if not data:
        raise RuntimeError("No decisions with multiple legal choices")
    rng = np.random.default_rng(seed)
    parameters = policy.parameters()
    first = [np.zeros_like(weight) for weight in parameters]
    second = [np.zeros_like(weight) for weight in parameters]
    groups = [item for item in data if not reinforcement or item.temperature > 0]
    if not groups:
        raise RuntimeError("No sampled decisions for the live update")
    baselines = {s: mean([g.reward for g in groups if g.stage == s] or [0]) for s in range(3)}
    step = 0
    for _ in range(epochs):
        order = rng.permutation(len(groups))
        for batch_start in range(0, len(groups), 96):
            batch = [groups[i] for i in order[batch_start:batch_start + 96]]
            width = max(len(group.features) for group in batch)
            activation = np.zeros((len(batch), width, len(FEATURES)), dtype=np.float32)
            mask = np.zeros((len(batch), width), dtype=bool)
            for j, group in enumerate(batch):
                n = len(group.features)
                activation[j, :n] = group.features
                mask[j, :n] = True
            x = activation.reshape(-1, len(FEATURES))
            scores, hidden = policy.score(x)
            logits = scores.reshape(mask.shape)
            targets = activation[:, :, PRIOR]
            if reinforcement:
                temperature = np.array([group.temperature for group in batch], dtype=np.float32)[:, None]
                scaled = np.where(mask, logits / temperature, -np.inf)
                scaled -= scaled.max(axis=1, keepdims=True)
                probs = np.exp(scaled)
                probs /= probs.sum(axis=1, keepdims=True)
                selected = np.array([group.selected for group in batch])
                row = np.arange(len(batch))
                ratio = np.clip(probs[row, selected] / np.maximum(1e-9, [g.probability for g in batch]), .5, 1.5)
                advantage = np.clip([g.reward - baselines[g.stage] for g in batch], -2, 2)
                probs[row, selected] -= 1
                gradient = probs * (ratio * advantage)[:, None] / temperature
                # Anchor exploratory updates to the expert's score differences.
                gradient += .10 * (logits - targets) * mask
                rate = .00010
            else:
                # Distill relative teacher margins. A small preference resolves
                # exact rule ties while leaving meaningful score gaps intact.
                targets = targets.copy()
                targets[np.arange(len(batch)), [group.teacher for group in batch]] += .015
                gradient = (logits - targets) * mask
                rate = .00025
            delta = (gradient / max(1, mask.sum())).astype(np.float32).ravel()
            layer_delta = (delta[:, None] * policy.output) * (1 - hidden ** 2)
            gradients = (x.T @ delta, layer_delta.T @ x, layer_delta.sum(axis=0),
                         hidden.T @ delta, np.array([delta.sum()], dtype=np.float32))
            step += 1
            for weight, momentum, variance, grad in zip(parameters, first, second, gradients):
                grad = np.clip(grad, -.5, .5)
                momentum[:] = .9 * momentum + .1 * grad
                variance[:] = .999 * variance + .001 * grad ** 2
                weight -= rate * (momentum / (1 - .9 ** step)) / (np.sqrt(variance / (1 - .999 ** step)) + 1e-8)
    return len(groups)


def cases(split):
    return [(mode, seed) for mode in MODES for seed in SEEDS[split]]


def project_args(project):
    return ["--main-pack" if project.is_file() else "--path", str(project)]


def episode(args, workspace, label, mode, seed, model=None, *, trace=False, explore=False, fixture=None):
    directory = workspace / label / f"{mode.lower()}-{seed}"
    directory.mkdir(parents=True, exist_ok=True)
    outcome_path, decisions = directory / "mission.json", directory / "decisions.jsonl"
    outcome_path.unlink(missing_ok=True)
    frames = 1450 if fixture else args.frames
    user_args = ["--autoplay", "--world", "arctic", "--bots", str(MODES[mode]),
                 "--seed", str(seed), "--controller", "native" if model else "rule",
                 "--fixed-profiles", "--frames", str(frames), "--report", str(outcome_path)]
    if model:
        user_args += ["--checkpoint", str(model)]
    if trace:
        user_args += ["--decision-log", str(decisions)]
    if explore:
        user_args += ["--explore", str(TEMPERATURE)]
    if fixture:
        user_args += ["--smoke", "--case", fixture]
    env = os.environ.copy()
    env["XDG_DATA_HOME"] = str(directory / "user-data")
    if os.name == "nt":
        env["APPDATA"] = str(directory / "user-data")
        env["LOCALAPPDATA"] = str(directory / "local-data")
    command = [str(args.engine), "--headless", "--disable-render-loop", "--fixed-fps",
               str(args.process_fps), "--max-fps", "0",
               *project_args(args.project), "--", *user_args]
    with (directory / "engine.log").open("w", encoding="utf-8") as file:
        with subprocess.Popen(command, stdout=file, stderr=subprocess.STDOUT, env=env) as process:
            deadline = time.monotonic() + args.timeout
            try:
                while process.poll() is None:
                    if getattr(args, "play_process", None) and args.play_process.poll() is not None:
                        raise TrainingStopped
                    if time.monotonic() >= deadline:
                        raise subprocess.TimeoutExpired(command, args.timeout)
                    try:
                        process.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        pass
            finally:
                if process.poll() is None:
                    process.kill()
    if process.returncode not in (0, 2) or not outcome_path.is_file():
        raise RuntimeError(f"Godot failed ({process.returncode}); see {directory / 'engine.log'}")
    report = json.loads(outcome_path.read_text(encoding="utf-8"))
    if report["world"] != "frostworks" or not report["fixed_profiles"] or report["python_failures"] or report["neural_trace_failed"]:
        raise RuntimeError(f"Unexpected simulation configuration in {outcome_path}")
    if fixture and not report["test_passed"]:
        raise RuntimeError(f"Fixture failed; see {directory / 'engine.log'}")
    if model and (report["checkpoint"] != checkpoint_id(model) or not report["neural_decisions"][0]):
        raise RuntimeError(f"Native checkpoint did not control the mission {outcome_path}")
    metrics = {key: report[key] for key in ("phase", "arrived", "alive", "losses", "elapsed",
               "arrival_times", "damage_received", "physics_frames", "controller", "checkpoint",
               "neural_decisions", "neural_fallbacks", "mission", "power_on", "key", "radio")}
    metrics.update(mode=mode, seed=seed)
    return metrics, decisions


def run_cases(args, workspace, label, scenarios, model=None, **options):
    def run(case):
        return episode(args, workspace, label, *case, model, **options)

    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        for result in executor.map(run, scenarios):
            m, _ = result
            print(f"{label} {m['mode']} {m['seed']}: {m['arrived']}/5 extracted, "
                  f"{m['losses']} lost, {m['elapsed']:.1f}s", flush=True)
            yield result


def summarize(rows):
    def statistics(subset):
        arrivals = [time for row in subset for time in row["arrival_times"]]
        return {
            "missions": len(subset),
            "missions_with_extraction": sum(row["arrived"] > 0 for row in subset),
            "survivor_extraction_rate": sum(row["arrived"] for row in subset) / (5 * len(subset)),
            "survivor_losses": sum(row["losses"] for row in subset),
            "mean_time_to_extraction_seconds": mean(arrivals) if arrivals else None,
            "mean_damage": mean(row["damage_received"] for row in subset),
        }

    return {"overall": statistics(rows),
            "by_mode": {mode: statistics([row for row in rows if row["mode"] == mode]) for mode in MODES},
            "episodes": rows}


def selection_score(rows):
    # Sort lexicographically. Damage and extraction time can improve an
    # otherwise tied candidate. They cannot outweigh the loss of a survivor.
    return (sum(r["arrived"] for r in rows), -sum(r["losses"] for r in rows),
            sum(r["mission"] for r in rows), -sum(r["damage_received"] for r in rows),
            -sum(sum(r["arrival_times"]) for r in rows))


def collect(args, workspace, label, scenarios, coverage, model=None, *, explore=False, fixture=None):
    data = []
    for _, path in run_cases(args, workspace, label, scenarios, model, trace=True, explore=explore, fixture=fixture):
        data.extend(examples(path, coverage))
        if not args.keep_traces:
            path.unlink()
    return data


def evaluate(args, workspace, split, model, label):
    return [row for row, _ in run_cases(args, workspace, f"{label}-{split}", cases(split), model)]


def improve(args, workspace, active, round_id, scenarios, reference=None):
    snapshot = workspace / f"round-{round_id}" / "starting.llnp"
    atomic(snapshot, active.read_bytes())
    coverage = Coverage()
    data = collect(args, workspace, f"round-{round_id}/experience", scenarios, coverage, snapshot, explore=True)
    model = Policy.load(snapshot)
    updates = train(model, data, epochs=1, seed=round_id, reinforcement=True)
    candidate = snapshot.with_name("candidate.llnp")
    model.save(candidate)
    if reference is None:
        reference = evaluate(args, workspace, "validation", snapshot, f"round-{round_id}/starting")
    trial = evaluate(args, workspace, "validation", candidate, f"round-{round_id}/candidate")
    mode_totals = lambda rows, mode: (sum(row["arrived"] for row in rows if row["mode"] == mode),
                                     -sum(row["losses"] for row in rows if row["mode"] == mode))
    stable_modes = all(mode_totals(trial, mode) >= mode_totals(reference, mode) for mode in MODES)
    accepted = stable_modes and selection_score(trial) > selection_score(reference)
    decision = {
        "round": round_id,
        "training_examples": updates,
        "accepted": accepted,
        "no_difficulty_regression": stable_modes,
        "starting_id": checkpoint_id(snapshot),
        "candidate_id": checkpoint_id(candidate),
        "starting": summarize(reference),
        "candidate": summarize(trial),
        "coverage": coverage.report(),
    }
    write_json(snapshot.with_name("promotion.json"), decision)
    if accepted:
        atomic(active, candidate.read_bytes())
    print(f"Round {round_id}: {'promoted' if accepted else 'retained active checkpoint'}", flush=True)
    return trial if accepted else reference


def bootstrap(args):
    coverage = Coverage()
    data = collect(args, args.output, "expert", cases("train"), coverage)
    data.extend(collect(args, args.output, "fixtures", [("Standard", 43)], coverage, fixture="compound_neural_training"))
    write_json(args.output / "feature-coverage.json", coverage.report())
    policy = Policy(args.seed)
    examples_used = train(policy, data, epochs=3, seed=args.seed)
    model = args.output / "approved.llnp"
    policy.save(model)
    distance = mean(float(np.argmax(policy.score(g.features)[0]) == g.teacher) for g in data)
    print(f"Distilled {examples_used} decision groups; in-sample teacher agreement {distance:.1%}", flush=True)
    reference = evaluate(args, args.output, "validation", model, "imitation")
    for round_id in range(1, args.rounds + 1):
        scenarios = [(mode, SEEDS["train"][i] + 100_000 * round_id)
                     for i, mode in enumerate(MODES)]
        reference = improve(args, args.output, model, round_id, scenarios, reference)
    atomic(args.model, model.read_bytes())
    write_json(args.output / "training.json", {
        "engine_sha256": checkpoint_id(args.engine), "checkpoint": checkpoint_id(model),
        "training_groups": examples_used, "teacher_agreement_in_sample": distance,
        "validation": summarize(reference), "frames": args.frames, "process_fps": args.process_fps, "seeds": SEEDS,
    })


def comparison(args):
    report = {"engine_sha256": checkpoint_id(args.engine), "checkpoint": checkpoint_id(args.model),
              "frames": args.frames, "process_fps": args.process_fps, "held_out_seeds": list(SEEDS["test"])}
    for label, model in (("planner", None), ("frozen", args.model)):
        report[label] = summarize(evaluate(args, args.output, "test", model, label))
    write_json(args.output / "evaluation.json", report)


@contextmanager
def training_lock(workspace):
    # Operating system advisory locks are released even after a forced exit.
    with (workspace / "trainer.lock").open("a+b") as lock:
        lock.write(b"0")
        lock.flush()
        lock.seek(0)
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise RuntimeError(f"A trainer is already using {workspace}") from error
        yield


def live(args):
    active = args.output / "approved.llnp"
    if not active.exists():
        atomic(active, args.model.read_bytes())
    Policy.load(active)
    process = None
    if args.play:
        process = subprocess.Popen([str(args.engine), "--resolution", "960x540", "--max-fps", "30",
                                    *project_args(args.project), "--", "--controller", "native",
                                    "--checkpoint", str(active)])
        args.play_process = process
    try:
        # Each invocation/workspace has fresh exploration seeds. The validation
        # and test seeds occupy disjoint ranges.
        rounds_run = 0
        reference = None
        while (args.rounds == 0 or rounds_run < args.rounds) and (process is None or process.poll() is None):
            round_id = time.time_ns()
            rng = np.random.default_rng(round_id)
            scenarios = [(mode, int(rng.integers(1_000_000, 2_147_000_000))) for mode in MODES]
            reference = improve(args, args.output, active, round_id, scenarios, reference)
            rounds_run += 1
    except TrainingStopped:
        pass
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait()


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("command", choices=("bootstrap", "evaluate", "live"))
    cli.add_argument("--engine", type=Path, required=True)
    cli.add_argument("--project", type=Path, default=ROOT / "game")
    cli.add_argument("--output", type=Path, default=ROOT / "reports/native-policy")
    cli.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    cli.add_argument("--frames", type=int, default=36060)
    cli.add_argument("--workers", type=int, default=1)
    cli.add_argument("--rounds", type=int, default=1)
    cli.add_argument("--seed", type=int, default=42)
    cli.add_argument("--timeout", type=int, default=900)
    cli.add_argument("--process-fps", type=int, choices=(10, 15, 30, 60), default=60,
                     help="Headless processing cadence. Physics remains at 60 ticks per simulated second.")
    cli.add_argument("--keep-traces", action="store_true")
    cli.add_argument("--play", action="store_true", help="Run the playable game during live training")
    args = cli.parse_args()
    for name in ("engine", "project", "output", "model"):
        setattr(args, name, getattr(args, name).resolve())
    if not 60 <= args.frames <= 180000 or args.workers < 1 or args.rounds < 0 or args.timeout < 1:
        cli.error("Invalid frames, workers, rounds, or timeout")
    if not args.engine.is_file() or not args.project.exists():
        cli.error("An engine containing the LastLight module and its project/PCK are required")
    if args.command != "bootstrap":
        Policy.load(args.model)
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        with training_lock(args.output):
            {"bootstrap": bootstrap, "evaluate": comparison, "live": live}[args.command](args)
    except KeyboardInterrupt:
        cli.exit(130, "Training interrupted. The last approved checkpoint is still available.\n")
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        cli.exit(1, f"Frostworks training failed: {error}\n")


if __name__ == "__main__":
    main()
