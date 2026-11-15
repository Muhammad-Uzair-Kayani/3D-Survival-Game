# Runtime contract

Godot owns CharacterBody3D collision, navigation A*, scene assets and rendering.
The client owns authority over actor state, inventory, timed actions, objectives,
social evidence, observations, intent selection and scoring. Python returns an
intent; C++ rechecks its feasibility and executes the spatial skill. It cannot
set health, poses, inventory, rewards or engine nodes.

`domain.h` and `world_contract.h` contain portable C++17 action rules, social
memory, blends, gate timing, observation features/action masks and fitness.
`WorldPort::observe()` is the read boundary used by the Godot adapter and fake
world tests. Spatial skills in `decisions.cpp` and combat/navigation adapters
still use Godot APIs; this is not a claim that a second engine can run the
entire module unchanged. The earlier console backend and full physics replay
are separate from this Godot implementation.

The 16 normalized features, in order, are health, magazine fill, weapon owned,
visible loot, visible/heard threat, visible trust, destination proximity,
normalized destination X delta, normalized destination Z delta, gate open,
control proximity, visible contact count, elapsed time, supplies collected,
local memory/noise present, and public key recovered. Contacts and loot use
range plus collision LOS. Unseen help cannot add trust. Threat decays after six seconds without new witnessed aggression, so an old encounter does not permanently trap the authored survivor in avoidance. Gunfire may disclose an
anonymous location without identifying an actor behind a wall.

Six output logits select Explore, Acquire, Cooperate, Avoid, Defend, Progress.
Unavailable outputs are masked. Safety skills (self-care, valid revivals,
reload, immediate close threat) remain authored. Neural choices apply during
all three mission stages. This is one shared policy evaluated separately for
each survivor's own observation, not a central squad command.

Transport is authenticated loopback TCP, NDJSON schema `v=1`, with bounded
64 KiB messages and batches of at most 32 actors. Messages carry integer
`request_id` and `episode` correlation. `connected` authenticates; `load_policy`
loads all 374 weights; `infer_batch` returns one masked intent per active actor;
`shutdown` stops the worker. Diagnostic `hello`, `reset`, `episode_result` and
`save_checkpoint` messages are supported by the service. The Python training
orchestrator launches real C++ episodes, rather than asking the inference
service to invent fitness. Malformed/duplicate/stale replies, disconnects and
one-second inference timeouts clear the learned policy and resume authored AI.
A missing worker has a three-second connection deadline. Offline evaluation
waits for policy replies; interactive play remains asynchronous.

The MLP is 16-16-6: 256 input weights, 16 hidden biases, 96 output weights and
6 output biases. Hidden activation is tanh; masked argmax selects the output.
GA uses two elites, tournament selection, uniform crossover, Gaussian mutation
and bounded genes. Training seeds 101–104, validation seeds 501–504 and held-out
seeds 901–904 are disjoint. Validation selects the champion; held-out results
are only measured after training. A checkpoint preserves population, RNG,
generation, history and champion for reproducible continuation. World physics
and platform scheduling still limit cross-machine bitwise determinism.

Fitness = 1000 × arrivals + 50 × living survivors − elapsed seconds − 100 ×
unprovoked survivor attacks. Kills, pickups and repeated help do not produce
farmable rewards. Reports also include arrival health, damage received,
travel distance, cooperative uses and independent survivor outcome/state.
`--trace PATH` writes NDJSON observations, masks, selected intentions and actual
skills at decision ticks for diagnosis. Seed, state and recent social events
are included in the final report. Save files include validated game state and
RNG state, but not Godot's internal solver/contact caches.
