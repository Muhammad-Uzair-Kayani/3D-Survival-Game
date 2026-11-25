# Frostworks native neural policy

**Play.bat** uses the distributed frozen policy. The mission menu and F1 diagnostics display the checkpoint ID. The game uses the checkpoint entirely in C++. In frozen and live-training play, survivors use baseline profiles and their personality and learned preference parameters are fixed. Contacts, observation age, local trust, and knowledge of supplies can still change during a mission.

**Play-With-Live-Training.bat** opens the game and runs a background trainer using the same Frostworks physics, navigation and AI code in headless missions. The launchers are included in the Windows ZIP. The live launcher stores weights and evaluation results under `%LOCALAPPDATA%\Last Light\policy-training`. Each simulation has pinned starting weights. Live approval takes effect when the playable scene next starts from its mission menu. A saved mission tagged with a different neural checkpoint is kept on disk and can be loaded by starting the game with that checkpoint. The original launcher's packaged frozen weights can be used again any time.

## What the model controls

The same checkpoint scores one row per available choice at three decision points.

| Decision | C++ supplies | Network selects |
|---|---|---|
| Tactical | Legal actions, known loot, visible casualties with safe approaches and verified cover | Which action and target to use |
| Mission job | Tasks with known prerequisites and evidence, including resource and support tasks | Which job and destination to claim |
| Route | Results of the bounded resource-aware planner for the selected job | Which proposed plan to execute |

Movement, weapon and inventory validity, mission interactions, ally visibility and friendly-fire checks are enforced by the simulation. The model operates within the options exposed by the existing planner. A route that fails the planning budget leads to a safe hold and reassessment. The old rule and optional Python guide controllers remain selectable through `--controller rule` and `--python PATH`.

## Training and validation

- The 144 named columns in [neural_features.def](../modules/last_light/neural_features.def) are shared by export and inference. They include survival resources, inventory and compatible ammunition, personality, orders, mission state, tactical perception and readiness, target evidence and route quality. Presence and visibility flags accompany optional targets. A remembered enemy uses its last seen position and age. Candidate rows include the rule score margin as a prior.
- Offline imitation fits a 24-unit network with a linear skip connection to the C++ expert's ranking. The expert and candidate have access to the same legal choices. Rare-event fixtures exercise wounded and thirsty survivors, empty weapons, smoke, observed casualties, and alternate extraction jobs in the real engine alongside seeded missions.
- Live improvement samples legal choices in headless missions. Local health, rescues, incapacitation, extraction and mission progress provide reward. A bounded policy-gradient update uses logged behavior probabilities and regularization to the rule score. New weights are written as a candidate. Separate validation missions compare it with the pinned starting checkpoint before promotion. Promotion is atomic and the starting checkpoint remains in each round directory.
- Final test missions use untouched seeds in Peaceful, Standard and Hostile modes and run with greedy decisions and no profile learning. The evaluation includes per-seed results, survivor extraction rates, losses and time to extract. Validation scores order checkpoints by extraction count, fewer deaths, mission stage, damage received and time to extract. The default benchmark stops after 36,060 physics frames, about 601 simulated seconds. Use `--frames 108060` to evaluate through the full Frostworks deadline.
- The training run records observed ranges and action counts per feature in `feature-coverage.json`. The C++/Python parity test exercises every feature with individual perturbations, boundary values, and combined inputs. Continuous coordinates and combinations of observations make exhaustive mission-state enumeration impractical. The coverage report shows what the simulation actually exercised.

## Source commands

Build the custom engine using the repository's normal build setup and install NumPy in the training Python environment. The Windows ZIP includes its own NumPy and Python for the live launcher.

```text
python -m pip install -r requirements-training.txt
python python/frostworks_train.py bootstrap --engine dist/LastLight.exe
python python/frostworks_train.py evaluate --engine dist/LastLight.exe
python python/frostworks_train.py live --engine dist/LastLight.exe --rounds 0
```

Supply `--project dist/LastLight.pck` to run against a packaged project. The default source project is `game/`. The default output is `reports/native-policy/`. Use a separate `--output` for each experiment. Bootstrap replaces `game/content/frostworks-policy.llnp` after training and validation. Repack the PCK after replacing that file. Evaluation and live training read the frozen file. Live publishes to its separate `approved.llnp`.

A custom or previous checkpoint can be loaded with `-- --controller native --checkpoint PATH` after the standard Godot arguments. Add `--decision-log PATH` for candidate features, target IDs, selected action and periodic outcomes. Use `--keep-traces` when collecting training data if you need the full decision logs after training. Counter indices 0, 1 and 2 refer to the tactical, job and route stages respectively.

The released checkpoint results are in the [frozen policy evaluation](native-policy-evaluation.md).
