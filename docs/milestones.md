# Current upgrade

See arctic-upgrade.md for the expanded world, durable survivor learning/reset,
planning, tactical skills, new CC0 art and executed checks. The table below
records the original MVP handoff; the Arctic revision extends those systems.

# Roadmap handoff

The original roadmap targeted a deterministic synthetic/console world while
PlagC was unfinished. The later request replaced that engine target with a
windowed, source-linked Godot game. The delivered MVP maps its systems as follows.

| Roadmap milestone | Delivered implementation / evidence |
| --- | --- |
| M0 contracts | Portable C++ domain and observation/gate/reward contract, versioned JSON protocol, fake-world C++ tests; see contract.md |
| M1 seeded world | Seeded item/objective variations and bot roles/spawns, common collision/nav geometry, event journal and decision NDJSON, same-build repeat check |
| M2 solo survival | Human movement/aim/crouch/sprint/jump, resource actions, real collision/combat, key/radio/extraction, downing/revival/death and deadline, solo completion check |
| M3 teamwork/bots | Two distinct control operators, latched opening, persistent identities, solo bypass, aggressor/scavenger/runner roles, peaceful/standard/hostile menu presets |
| M4 behavioural agents | Four independent survivors; local LOS observations, witness-based trust/threat, decaying old threat, self-care/rescue/combat skills, engine A* and stuck-path replanning, rule/random baselines |
| M5 Python bridge | Persistent authenticated loopback service, bounded/correlated messages, fault injection tests and authored fallback |
| M6 neural control | Real 16-16-6 masked MLP, all mission-stage intentions, model save/load, per-agent observations and trace output |
| M7 evolution | 374 genes, elitism/tournament/uniform crossover/Gaussian mutation, real-engine fitness, validation champion, RNG/population checkpoints and frame-limit compatibility |
| M8 evaluation | Disjoint training/validation/test seeds, measured rule/random/neural/memory-ablation comparison, episode metrics, CSV/plot artifacts; see validation.md |
| M9 engine handoff | Unmodified pinned Godot gitlink, external built-in C++ module, MSVC static runtime, windowed executable, portable optional Python and prepared resource pack |

The MVP also includes real UAL skeletal blend graphs, weapon attachments,
dungeon art, sound cues, menus/options, save/continue and survivor results.

The original synthetic backend's exact determinism and a promise to replace
engines without modifying any spatial adapter are not properties of Godot's
physics engine. The portable rule/observation layer is separated; Godot spatial
skills remain engine-specific. The measured ablation does not prove a social
learning benefit. Asset/animation limits and license terms are explicit in
assets.md. These boundaries prevent treating a tested MVP as an unmeasured
commercial-scale game.
