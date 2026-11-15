LAST LIGHT: FROSTWORKS TACTICAL SURVIVAL

WINDOWS BINARY: Extract the whole ZIP and double-click Play.bat. No compiler or
Godot editor required. Start a NEW mission: the level and mission-save format
changed. Your old mission save is preserved. Choose Peaceful for exploration,
Standard for five distractors, or Hostile for eight.

SOURCE: Keep vendor/godot and .tools when updating. Setup.bat, then
GenerateProjects.bat vs2022. Open build/premake/vs2022/LastLightGodot.sln.
Release | x64, build LastLight. BuildGame.bat --jobs 1 remains supported.
Premake drives the Godot SCons build. There is no CMake step.

MISSION
Recover the public access key; restore generator power using fuel or hand
cranking (tools shorten the crank); calibrate any two of three relays; restore
the radio. With a tool kit, lock the turbine service panel to block sabotage.
Activate either landing beacon with E. North airlift takes 22 seconds,
west 32. Raiders inside the perimeter pause the countdown/boarding. Clear the
perimeter, remain at the landing and evacuate. Radio activation can attract one
bounded reinforcement wave of unused bot identities.

ROUTES AND COMBAT
The industrial campus has multiple entrances, two outdoor gantry ramps, an
internal turbine ramp, roof cutaways, a motor pool, loading docks, machinery,
medical/storage/quarters rooms, a pipe yard, cold-water channel and bridge.
Use a quiet perimeter approach or exposed shorter service roads. Search caches
with E. Supplies are finite; weapons use compatible ammunition. Smoke blocks
sight, decoys create anonymous noise, doors change physical firing lanes.

WASD move; mouse look; Shift sprint; Ctrl crouch; Space jump; Alt roll.
RMB aim; LMB attack; R reload. 1 pistol, 2 sword, 3 SMG, 4 rifle, 5 shotgun.
E interact/revive; H medkit; B bandage; F eat; T drink; Q smoke; V decoy.
Z enter/leave nearby cover: A/D slide, RMB expose, W/S leave.
J inventory: inspect/use supplies or discard one permanently to free weight.
F2 tactical overhead view; mouse wheel zoom; RMB aims toward the cursor.
M tactical map; F3 squad orders; I survivor inspector; Tab spectate.
C close nearby door; G share compatible ammunition; Y invite teammate.
F5 save, F9 load, F10 autoplay, F1 performance/debug, Esc pause, X cancel.
The live inventory/map/inspector/squad panels do not pause the world.

ORDERS AND LEARNING
Maya is medic, Rafi scout, Lian guard, Omar engineer. Orders express an intent
for 45 seconds. Voice needs nearby sight; powered squad radio works across the
base. Distrust can cause refusal. Threat response, treatment and rescue take
precedence over ordinary orders. Independent mode restores normal task bidding.

I shows six social/personality scores, trust/fear, historical risk, private
contacts and their confidence/age, current task, resource estimate, alternative
routes, all 13 tactical choices and legality/rejection reasons. Persistent
contextual action values rank legal choices within a bounded influence; safety
rules always apply. This is bounded planning and learned utility, not an LLM.

Reset the selected profile in I or the pause menu; use the main-menu checkbox
to reset all four for a new mission. Health/inventory are preserved by live
reset. Learning resumes immediately. Mission loading does not rewind persistent
learning or resurrect a profile reset. Profiles migrate from survivors-v1.json
to survivors-v2.json. Frostworks mission: save-frostworks.json. Previous save.json
is preserved. Files live in %APPDATA%\Godot\app_userdata\Last Light\.

PERFORMANCE AND TESTING
Default 960x540 GL Compatibility targets integrated graphics. Static art is
batched, there is one bounded sun shadow, AI scans are staggered and plans have
explicit search limits. First loading/import can take longer. Actual Vega FPS
must be measured on your machine; F1 exposes the current FPS.

Verify-Game.bat runs isolated regression/mission checks using bundled Python;
it never uses your personal save directory. The source includes additional
portable/native MSVC policy, resource-budget and build-environment tests.
See docs/frostworks-update.md for implementation limits and validation evidence.
