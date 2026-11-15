# Frostworks cycle: observed limitations and implementation targets

The previous build expanded area and feature count without a strong encounter
layout or sufficiently rich decision model. This cycle targets play quality.

| Limitation | Result in play | Change / verification |
|---|---|---|
| Repeated single-entry box rooms and empty travel | Generic spaces, bottlenecks, little route choice | Authored industrial campus, distinct silhouettes, crossing/service routes, multiple entrances, cover/visibility audit and rendered review |
| Weak visual hierarchy and flat lighting | Poor landmarks and little relation to reference | Three hero structures around a working yard, rooftop machinery, pipes, gantries, marked roads, props, material/weathering and bounded directional shadows |
| Macro equipment flags approximate action feasibility | Plans can assume supplies or underestimate risk | Resource-aware plan validation, perceived opportunities, explicit costs/confidence and executable partial plans |
| Tactical behaviour has no cover model | Stand-and-fire, predictable retreat | Physical cover sampling, threat influence, position reservations, scored tactical actions and firing-lane checks |
| Limited observation/recollection | Reactivity and hidden-world shortcuts | Last-confirmed contacts, anonymous hearing, decaying confidence, stale opportunity invalidation and bounded local information sharing |
| Team agreement is just a link | Little division of work or support | Committed roles, safe orders, guard/medic/engineer/scavenger tasks, task claims and refusal reasons |
| Completion is mostly walking to pads | Weak endgame pressure | Explicit extraction arrival/boarding, contested landing checks and bounded reinforcements |
| Weak action feedback | Generic gunplay/interaction feel | Weapon handling, readable contextual prompts, inventory decisions, effects and spatial feedback |
| Tests focus on viability | Completion can conceal foolish play | Scenario checks for occlusion, outgunned retreat, cover/reload, friendly lanes, knowledge, role conflicts and resource accounting; seeded evaluations |

Keep native Godot source module, Windows/MSVC/Premake frontend, optional separate
Python training/guide, persistent per-survivor profiles and live individual reset.
Migrate profiles; preserve old mission files and report geometry/schema changes.
Use the supplied UAL Standard animations and licensed existing art; add free
assets with recorded licenses as needed. Render and inspect the actual custom
engine. Ship source and a native tested Windows package with direct ZIP links.
