# Frostworks implementation

The default Arctic world is now an authored industrial campus. The reference
informed its three principal buildings, motor pool, service lanes, roof equipment,
pipe bridge and layered approaches. It is a stylized interpretation using CC0
Kenney assets and native geometry, retaining the Arctic setting. Previous Arctic
and courtyard maps remain available as `--world classic` and `--world lab`.

## What changed

- 300m terrain, 30 regional landmarks, multiple doors per principal building,
  traversable elevated gantry with two exterior ramps and an interior ramp,
  distinct roof silhouettes, skylights/fans/pipework, interior furnishing,
  vehicles and physical cover. Roofs cut away around the viewed indoor actor.
  Overlapping paved rectangles are unioned to avoid depth flicker. Static art
  uses material/mesh batching; weathering is generated in the material shader.
- 133 finite loot entries and 69 interactions: calibre-specific guns/ammunition,
  medicine, clothing, armour, tools, consumables, caches, mission controls,
  optional tool-secured generator panel and contested landing beacons.
- Native tactical policy evaluates 13 actions, with explicit legality vetoes,
  learned utility, commitment, cover distance/exposure, verified cover margins,
  peek/flank routes, suppression, compatible ammunition, safe reload/treatment,
  rescues, finite smoke and bounded investigation. Near friendly bodies can veto
  the actual weapon ray even after higher-level planning approves an attack.
  Known incapacitated targets cease being active firing threats.
- Contacts store last-confirmed positions, confidence, observation age and
  hostility. Hearing alone does not invent a target identity. Private pickup
  and cache knowledge becomes stale and is invalidated through observation.
  Tactical navigation uses the actual merged navigation map. Route measurements
  are refreshed when the Godot navigation map changes, including gate changes.
- Resource-aware bounded forward planning compares regional approaches and
  explicit prerequisites. It accounts for carry capacity, supplies and estimated
  health/ammunition costs; unresolved preparation is labelled partial. Role
  bids, task leases, failure backoff and local/powered-radio information sharing
  divide work. Learned region risk and intent preferences influence cost.
- F3 orders (independent/follow/hold/scavenge/mission) last 45 seconds, can be
  refused for distrust/no communications and yield to immediate survival needs.
  I shows private observations, social scores, resource forecasts, alternatives,
  legal/rejected tactical options and contextual learned values. J offers finite
  item use and explicit permanent discard to manage carrying capacity.
- F2 provides a cursor-aimed overhead view with wheel zoom; Z supports physical
  cover entry, sliding and exposure. UAL1/UAL2 native AnimationTree locomotion,
  stance/aim layers, one-shot actions and cross-fades remain, with corrected
  local-facing torso steering, equipment attachments and procedural support-hand
  placement for long guns. Spatial action audio and weapon spread/pellets are
  separate from deterministic cosmetic-free shot randomness.
- North/west airlifts take 22/32 seconds. A hostile perimeter presence pauses
  arrival/boarding. One bounded reinforcement wave uses unused bot identities;
  dead identities are not respawned. Generator tools can prevent repeated
  opportunistic sabotage instead of endlessly repairing its consequences.

## Learning and compatibility

Survivor-specific memories, social traits, intent preferences and historical risk
persist alongside bounded contextual action values. The latter use 216 discrete
situations (health, loaded ammunition, known threats, protection, allies and
suppression) and 13 action values. They update from real mission outcomes in
four-second windows; their influence is bounded and never overrides legality.
They are value estimates, not probabilities or human-like general reasoning.

The optional Python 16-16-6 neural guide and offline genetic training remain a
separate system. The inherited bundled neural weights are a historical courtyard
baseline; there is no claim that they outperform the native planner in Frostworks.
Fresh training checkpoints now include the exact engine SHA-256 and version 3;
older/different-build checkpoints are rejected instead of silently mixing worlds.
Use one training worker on an 8 GB machine. Mission planning works without Python.

Profiles migrate from survivors-v1.json to survivors-v2.json. Live individual or
new-game resets clear personal learning while preserving live inventory/health.
Mission schema 4 records contacts, suppression, orders, cloud/decoy lifetimes,
beacons, reinforcement timing and world-object state. Loading validates all fields
before mutation, clears transient reservations/plans and preserves current profile
revision/learning. A pre-reset mission cannot restore reset personal knowledge.
The new mission uses save-frostworks.json; old save.json is preserved.

## Scope and practical limits

This is a tactical survival MVP, not an open-ended reasoning model or a replica
of the reference game's art. Terrain remains a static authored layout with seeded
cache contents, not procedural terrain generation. Motion reuses the supplied
Standard clips; absent authored directional/rifle clips are not invented, and
there is no foot-plant IK, motion matching or destructible architecture. The
support hand uses procedural placement rather than a new animation pack.

Planning has explicit depth/beam/expansion limits. Prediction uses local evidence
and abstract resource estimates, not exhaustive future combat simulation. Hostile
missions can fail or remain incomplete. Orders express intent rather than taking
away survivor autonomy. There is no multiplayer. Native module and Godot are
statically compiled into the GUI executable; content remains in a companion PCK.

Default 960x540 GL Compatibility, batching, bounded shadows, staggered perception
and limited position/path queries target integrated graphics. Actual Windows 10
Vega FPS and thermal behaviour still require the user's hardware. Linux software
rendering is used for visual inspection, not a claim about target hardware FPS.

## Validation

See validation-frostworks.json for the recorded checks of this release. The suite
preserves earlier gameplay/transport regressions and adds actual capsule travel
across the gantry/portals, merged route availability, friendly firing-ray veto,
engagement, generator security, inventory accounting, profile value persistence,
reset-vs-old-save, transactional restore, orders and contested airlift timing.
The shipped Windows package is validated again with bundled Python and no SDK.
