# Arctic art additions

The default Arctic map now uses selected CC0 GLBs from Kenney Nature Kit 2.1,
Survival Kit 2.0, Blaster Kit 2.1, Furniture Kit 2.0 and City Kit Industrial 2.0.
`game/content/free-assets.json` records public source pages, versions, original
archive hashes and selected models. Original CC0 license texts are inside the
prepared content pack. Newly normalized SMG/rifle/shotgun models use Blaster art.
These additions need no separate user download or editor import.

Snow/vegetation use shared geometry and simple shaders. Furnished interiors,
containers/rocks, supply crates, doors and local warm lights dress the authored
300m world. The cap is now eight distractors, four fixed survivors and one
player. Resolution 960x540/30 FPS remains the default; Vega performance needs
local measurement. The older courtyard art described below remains in tests.

# Integrated assets

The game includes a prepared runtime content pack. No editor import or local
Blender/FBX tool is required to build or run the delivered project. GLB scenes
are loaded through the built-in Godot GLTFDocument module, cached and reused.
Raw source archives and private download URLs are not included.

* Character: Quaternius Universal Animation Library 1 and 2, Standard packs,
  CC0. UAL1 supplies locomotion/aim/reload; UAL2 supplies additional sword,
  consumption and recovery actions. Both use the same 65-bone mannequin rig.
  These are the free Standard clip sets, not the paid 130+ clip library.
* Weapons: Glock mesh from the Free3D secondary weapon pack by okokok123, and
  the user's sword mesh. Glock geometry was reduced for integrated graphics;
  both use neutral metallic materials. Unrelated reference photographs from
  the original weapon ZIP are not included.
* Environment: Dwarven Expedition Pack – Stylized Assets by tobyfredson /
  FlorianAlexandru from the newer CGTrader link. This replaces the earlier Halo
  map request. Actual modular gate, arch, pillars and structural decorations
  dress an authored playable courtyard with source stone/sand PBR textures.
  Collidable arches/pillars use mesh geometry; walls are solid textured boxes.
  The sample's entire preassembled scene is not the gameplay level.

Textures are capped at 512 pixels and mipmapped. Shadows, Vulkan, MSAA and
costly screen effects are disabled by default. Four fixed survivors and at
most four distractor bots cap simulation/animation work. These are design
limits, not measured Vega frame-rate guarantees.

UAL clips drive a real AnimationTree: speed blend space, crouch stance blend,
filtered upper-body pitch aim, timed action one-shots, airborne and death
poses. Weapons attach to the actual right-hand bone. Gameplay movement is
in-place, driven by CharacterBody3D; root motion does not move the actor twice.
The free library supplies forward movement clips; dedicated authored strafing
and foot IK are not present. Blend direction values remain available to an
expanded library. Collision and hits are authoritative, independent of a
presentation animation's sampling rate.

Third-party models are incorporated into `game/game-content.pck` as game
content, rather than provided as loose asset packs. Credit and license notes
are in `game/content/ASSET-CREDITS.txt`. Original licenses continue to apply;
Free3D identifies the weapon pack as Personal Use. The supplied sword's ZIP did
not contain a verifiable license grant. Do not treat these models as CC0 or as
an unrestricted commercial asset collection. Replace them or confirm rights
before commercial redistribution. All gameplay/build/Python/UI source is
available for editing independently of those art licenses.
