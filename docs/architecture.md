# Godot source integration

The pinned Godot 4.5.1 source stays an unchanged submodule. Its SCons
`custom_modules` option compiles `modules/last_light` into the engine executable.
There is no separately loaded gameplay DLL. Premake generates the VS2022 build
frontend and invokes the validated MSVC/SCons wrapper.

`last_light_game.cpp` owns the spatial adapter and fixed 60 Hz authority loop.
`domain.h` owns portable action/life/inventory/social/blend rules.
`world_contract.h` owns the portable observation boundary, gate timing and
reward. `decisions.cpp` converts local perceptions into features and goals,
then requests engine navigation. `python_bridge.cpp` validates transport and
masked intentions. `persistence.cpp` validates a candidate save before committing
it. `frontend.cpp` exposes a small UI DTO and explicit commands; GDScript menus
cannot directly edit authoritative health or inventory.

`visuals.cpp` builds the skeletal blend graph and bone attachments.
`level_art.cpp` loads modular art and creates matching static collision.
Navigation is baked from the same world collision and rebaked after the gate
rises. Dynamic actors use local separation and stuck-path retries. Physics
runs at 60 Hz, AI at 5 Hz, HUD at 10 Hz; default rendering is 30 FPS, 960×540,
OpenGL Compatibility. Pausing stops world time; loss of focus also pauses.

The optional Python service is a separate process. `python/train.py` owns
real-engine evaluation, GA evolution, validation and held-out comparison.
`docs/contract.md` specifies the feature/action/protocol boundary. Protocol
failure switches to the authored controller and is reported explicitly.

Save schema 3 contains actor poses, inventory, action timers, local beliefs,
RNG state, objective/gate state and counters. Old schema 1/2 saves are
rejected. Contact/solver caches are not serialized, so restoration is valid
game state, not a promise of bit-identical physics replay. Decision traces and
reports are diagnostic records, not input recordings for deterministic replay.

Prepared meshes/textures reside in `game/game-content.pck`; build packaging
includes this data inside the game's resource PCK. Raw game sources do not
require an editor import pass. A stock Godot executable lacks the custom native
class and cannot run this main scene; use the built executable or source build.

The Arctic client adds `arctic_world.cpp` (world/asset construction),
`environment_gameplay.cpp` (transactional supplies, timed actions, smoke, doors
and conditions), `strategy.h` (portable bounded forward search),
`strategic_decisions.cpp` (perceived opportunities and local skills), and
`profiles.cpp` (independent durable survivor adaptation, inspection and reset).
Mission saves and reset revisions prevent an older mission from resurrecting
cleared personal memories. Profile writes are bounded, validated and atomic.
Snow and vegetation share MultiMesh geometry. Dynamic doors and low crates use
physical collision plus local skills/steering; permanent obstacles enter the
navigation bake. The gate triggers a rebake once opened.
