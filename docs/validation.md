# Arctic revision validation

See arctic-upgrade.md for the current revision. The Windows CI links, training
statistics and screenshots described below belong to the previous courtyard
MVP. They do not establish native Windows build success, GPU performance or
neural superiority for the expanded Arctic revision.

# Executed MVP validation

Windows source acceptance:
https://github.com/Muhammad-Uzair-Kayani/PlagC-Public/actions/runs/43624382105

The delivered client is compiled with native VS2022/MSVC x64 against unchanged
Godot 4.5.1 source, commit f62fdbde15035c5576dad93e586201f4d41ef0cb. The CI source
path contains spaces. Premake generates the VS2022 solution and MSBuild runs
its actual batch wrapper. All 30 tool/native tests pass on Windows, including
standalone C++ domain/fake-world tests and compiler/resource probes with VS
registry enumeration disabled. Linux runs 24 portable checks; six native
Windows checks are skipped locally and executed in Windows CI.

The source-built game passes 20 engine scenarios plus CLI rejection and a
same-build seeded repeat: collision/floor handling; blocked and valid gunfire;
reload accounting; co-op activation; leaving a control while the gate rises;
post-gate save/restore and malformed-save rejection; a single occupied control;
actual rig/bone attachments and action blends; seen/unseen social evidence;
delayed single melee hit and cancellation; team loss; real Python inference;
missing Python, process exit, malformed, stale, duplicate and late replies;
peaceful five-survivor completion; solo bypass; and standard team completion.
The seeded repeat compares outcomes, event journal and millimeter-rounded
positions. It establishes same-build reproducibility for that scenario, not
cross-platform bit-exact physics or arbitrary human-input replay.

The source-only ZIP was extracted without loose models/import caches, packed,
and run through the actual GLB/animation/environment checks. Nested game content
loads successfully. Windows CI also tests separate PCK, embedded PCK, packaged
Python, GUI PE subsystem and absence of dynamic MSVC C++ runtime dependencies.
`tests/windows_full_build.py` is the reproducible acceptance procedure.

Rendered testing used the actual custom game executable in an X11 window with
Mesa OpenGL Compatibility, not a stock-editor substitute. Start/scenario menu,
options, HUD, movement, weapon pickup, save/load, pause, AI autoplay, gate/radio,
five-survivor victory/results and replay controls were inspected. The final
rendered mission reached all-five extraction. A later-game save rejection was
traced to a missing gate-helper identity; the gate now latches operator IDs,
and the operator-departure/save regression passes.

Screenshots validate layout and rendering, not Vega performance. Windows tests
run on a Windows Server 2022 CI host. Windows 10 AMD Vega frame rate, hardware
driver behavior and long play sessions on the user's 8 GB PC remain local
acceptance checks. Default settings are 960×540, 30 FPS, Compatibility renderer,
no dynamic shadows/MSAA and at most four distractors.

## Training and held-out evidence

The shipped policy was evolved with population 30 for three generations, using
360 real Godot training episodes and 12 separate validation episodes. Training
seeds 101–104 and validation seeds 501–504 never enter held-out evaluation.
Training horizon is 6,000 frames (100 simulated seconds); held-out horizon is
18,000 frames (300 seconds). These accelerated episodes use the same collision,
mission, sensor and decision rules; rendering/rig updates are omitted during
headless training. Live gameplay still has a 600-second mission deadline.

Held-out seeds 901–904 produced these observed survivor arrival rates:

| Controller | Arrivals | Mean elapsed seconds |
| --- | ---: | ---: |
| Evolved neural | 20 / 20 | 20.18 |
| Authored rules | 16 / 20 | 35.01 |
| Masked random | 0 / 20 | 299.95 |
| Neural without retained memory | 20 / 20 | 20.26 |

No evaluated neural episode used transport fallback. The neural policy mostly
prefers direct progress and the solo route in these scenarios. Removing memory
did not reduce arrival rate, so these results do **not** establish learned
social intelligence or a measured benefit from memory. Co-op and witness-based
social reactions are independently covered by gameplay tests. Four held-out
seeds are a small experiment, not a general performance guarantee.

Generation means improved from 2655.11 to 3991.96; best training fitness changed
from 5230.11 to 5230.64. CSVs, per-seed summary, champion/checkpoint metadata and
a plot are in `docs/samples`. A real-engine resume check matched an uninterrupted run exactly for population,
champion, generation, GA RNG and score history. Evaluation-frame mismatches are
rejected. The inference
service and training tools use Python's standard library; plotting optionally
requires matplotlib. The binary's portable Python requires no system install.

## Scope

This is a complete playable survival MVP, with an authored courtyard and the
supplied modular art. It is not a large authored campaign, a paid full animation
library, a foot-IK implementation, or a guarantee of identical trajectories on
every platform. The Godot adapter uses native spatial APIs; the portable domain
and observation/gate contract can be tested without the engine, but replacing
Godot still requires implementing its physics/navigation/visual adapter.
