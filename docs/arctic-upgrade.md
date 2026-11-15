# Arctic upgrade

The default world is 300x300m with 16 strategic regions, 124 loot entries,
36 interactable objects, seven furnished buildings, 22 supply crates, three
relays, two evacuation routes, shelters, cover and a cooperative shortcut.
The objective is key -> generator -> any two relays -> radio -> evacuation.
Fuel shortens the generator action; hand-cranking permits solo completion.
Health, hunger, hydration, cold, stamina, armour, carrying capacity, compatible
ammunition and medical/equipment supplies constrain actual available actions.

Four firearms have separate magazines, ammo pools, damage/range/cadence/reload
times; melee, smoke, decoys, rolling, door control, rescue and supply sharing
provide additional choices. Smoke blocks sight, not physical bullets. Five
distractor roles hunt, scavenge, flee noisily, sabotage power and ambush.
Bots use the same collision and resource actions as survivors.

Survivors search bounded action plans (prerequisites/effects) and compare
short, lower-exposure and quieter routes. Their own past regional risk and
current perceived danger affect costs. Traits, learned output preferences
and optional neural intentions influence decisions; local skills interrupt
plans for safety. Friendly obstruction causes a firing-lane reposition.
This is bounded game AI, with replanning and uncertainty, not human reasoning.

Personal profiles persist across launches in survivors-v1.json. The inspector
shows type, six trait/social scores, experience history, trust/fear, risk by
region, learned biases, resource condition, plan steps and alternative scores.
Reset is immediate per survivor, or all four at new game, preserving mission
health/inventory. Subsequent play can teach new experiences. Mission saves
use schema 3; restoring an older mission cannot undo a personal reset.
The shared pretrained base network is frozen during play; individual output
biases adapt online. Separate GA tools evolve base weights. Automated runs
use isolated temporary profiles and never train your personal game save.

UAL1 and UAL2 Standard clips drive a native AnimationTree with filtered aim,
stance, speed/direction, interaction/combat one-shots and state cross-fades.
Movement direction is relative to actor facing, with smoothed values and
procedural torso steering. Forward clips are reused: this does not invent
missing authored strafe/rifle clips or foot IK.

## Validation

The source-built Linux engine passes 28 gameplay cases, malformed CLI checks
and the original same-build seeded repeat. Cases include real capsule/nav
movement, combat/reload/melee, gate/save/load, profile file round-trip and
reset-vs-old-save, equipment accounting, smoke visibility, surplus sharing,
active animation sampling, neutral-crossfire avoidance and crouched-target
hits, a peaceful Arctic mission with all five evacuated, active distractors,
Python inference and all transport fault fallbacks. The C++ domain, world
contract and strategy tests also pass. Python tool tests include learned
output-bias causality and mask/validation checks.

The interim map's doorway bottleneck was fixed by keeping supply crates
clear of the entry, preserving the door passage in the navigation bake and
using local capsule steering for temporary obstruction. Permanent generator
collision is included in navigation. Reset clears stale perceptual knowledge
and starts a new reward baseline. The live tactical map refreshes as agents
move. The native build log/CI result for the exact source ZIP is delivered
alongside it when available. Target Windows 10 Vega FPS is a local check.

The earlier courtyard training statistics in validation.md/docs/samples are
historical. They are not an Arctic comparison or proof of advanced learning.
The short Arctic GA smoke run only tests integration and checkpoint metadata.
