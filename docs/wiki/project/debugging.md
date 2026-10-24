# Debugging openblack

## Summary

How openblack is built, tested, run and debugged: the build presets, the crash handler, every debug environment
variable (the `OPENBLACK_*` names the code's debug hooks read), the unit tests and their test data, the key bindings,
the debug windows and where a new control goes. The only part about the original is the tutorial's hand demos, which
replay recorded input through the real hand and which openblack plays as the original does; they are also the
scripted demos the checks compare. How a change is verified is in [Testing](testing.md).

## The original

### Hand demos

The tutorial's hand demos (`Data\HandDemo\*.hnd`) replay recorded interface input through the real hand, and set the
camera from each record. The script opcodes call them. A file's records are read once into the resources
(`HandDemoManager`, `hand_demo::ReadRecords`).

- `PLAY_HAND_DEMO(string, waitTrigger, keepHand)` (`GScript::PlayHandDemo` 0x6FDAD0): `.\Data\HandDemo\%s.hnd`, where the name is the CHL string
  itself (challenge.chl offset 279 = "drag"). Then `GInterface::StartPlayBack` 0x5DAD60:
  - without keepHand, it drops the held object (fn_005D4350) and cancels the spells being charged;
  - it sets the game speed to 1, turns the widescreen on (`HelpSystem::SetWideScreen(1, 0)`) and snaps it (fn_005C6C40);
  - it processes the first record at once.
  - The path is built with sprintf on [0xC0DA18] and opened with `LHOSFile::Open` mode 2.
  - Without keepHand: fn_005D4350 is ForceDropHeld's packet 0x4D; spells are cancelled with
    `GPlayer::CancelAllSpellsCharging` 0x64BC60; then, with the leash on (+0x14) and not tied (+0x24 null),
    `GLeashStatus::SetOn(player->creature, 0)` (0x5DADCD): the leash held in the hand comes off.
  - `GScript::PlayHandDemo` stores waitTrigger at 0x6FDB59 / 0x6FDB6D, whatever StartPlayBack does.
- `IS_PLAYING_HAND_DEMO` 0x6FDB80 pushes **not** `IsPlayBack(0)`. `HAND_DEMO_TRIGGER` 0x6FE280 reads and clears the
  pending trigger. `SET_HAND_DEMO_KEYS` 0x709540 is an empty `ret`.
- `GInterface::IsPlayBack` 0x5DB710: +0x15C playing; with task ≠ 0 it also compares +0x160.
- When a task stops, the callback 0x6EC72A..0x6EC748 calls EndPlayBack if the task owns the demo.
- **File:** no header. Records are 124 bytes:
  - +0 the message: 0 mouse move, 1/2 grab down/up, 3/4 action down/up; it is an index into the message table 0xD186B8 (0 MOUSE_MOVE, 1 GRAB_DOWN, 2 GRAB_UP, 3 ACTION_DOWN, 4 ACTION_UP);
  - +0x04..+0x33: the CHand throw block +0x48C8..+0x48F7 at recording time (velocity, angular velocity, HandPos, angles). Recorder fn_005DB4D0;
  - +0x34 the mouse (normalised to 0..1 with fn_0081E8E0);
  - +0x3C the camera eye and +0x48 its focus;
  - +0x54 the camera hints shown when recorded (1 tilting, 2 turning, 4 zooming; the CameraModeNew3 tricon draw's
    [0xC5B0F4]) and +0x58 the hint's amount ([0xC5B0AC]);
  - +0x5C the trigger: the byte [0xE853AD] if it changed since the previous record, else 0 (**🔍 inferred** the space key);
  - +0x60 the visual clock when recorded (g_game +0x25053C): turn × 100 + the remainder 0..99 (`GGame::Loop` 0x54D343,
    [coordinates, distances and angles](../engine/coordinates-and-angles.md)), not milliseconds of play.
  - +0x64 the distance of the object under the hand, +0x68/+0x6C its map place (x, z, int32), +0x70 its altitude, +0x74
    its script type and +0x78 its subtype (written by the recorder, read by the ACTION messages' search, see below).
  - Count of records read: [0xD18854] / 0x7C; the file's size is [0xD18858], from the seek to the end in StartPlayBack
    (0x5DAE67).
- **Playback** (fn_005DAEE0, from the interface's message pump fn_005D9A20, which runs both every frame
  (ProcessFrameUpdates 0x5CEDB0) and every turn (GInterface::Process 0x5CEC10).
  - every record that is due is applied, in order, at the visual clock g_game +0x25053C, read once per call
    (0x5DAF8C, kept in edi);
  - while waiting for the trigger with one pending, the time stands still;
  - message 0 moves the mouse; the camera goes through `GCamera::SetPositionAndFocus` 0x4438C0 and
    `LH3DTech::UpdateCamera` 0x819920;
  - the end of the file ends the demo.
  - The real buttons (0x54C390) and the real mouse are blocked meanwhile. The camera keys are not: they still go to
    the current camera mode (`ProcessMapKeys` 0x54C359; the playback makes the interface active, 0x5DAE3D), which
    is a script's in every demo the game plays, each one inside a cinema
    ([the player camera, pending](../camera/player-camera.md#pending)).
  - It runs inside the pump before the hand ray (`GInterface::Process` 0x5CEC1F → fn_005D9A20 0x5D9AB3).
  - Each read is one `LHOSFile::Read` of 0x7C bytes; a short read calls EndPlayBack (0x5DB33D). The speed is set to 1 once per call, after the
    first read and before its size is tested (`GGame::SetSpeed(1.0)` 0x5DAFC4); the reads after it in the same call loop
    back to the due test at 0x5DB00A (0x5DB335) and do not set it again.
  - Due time (0x5DB00A): the record is not due while (time − first time) > (now − start), unsigned; if it is not due, it steps back one record.
  - Trigger wait (0x5DB034 / 0x5DB3A4): the start is rebased, +0x168 = +0x164 − time + now.
  - MOUSE_MOVE (0x5DB054): fn_0081E920 and `LHMouse::SetPosition` 0x5DB081. fn_0081E920 puts the recorded (cx, cy) on
    the picture between the cinema bars: b = fn_0081E8B0 = `ftol((H − 0.5625·W) · [0xEB9950]) / 2` (signed, `cdq; sub;
    sar`, not clamped; [0xEB9950] is the bars' fraction, written by fn_005C6BB0 and 1 once a demo has snapped them in),
    then y = `ftol((H − 2b) · cy + b + 0.5)` and x = `ftol(W · cx + 0.5)` (W, H = [0xE839E4] / [0xE839E8]). At 1024×768
    with the bars in, b = 96: the picture is the 576 rows from 96 down. fn_0081E8E0, the recorder's inverse, uses the
    same b. Any other message first copies the recorded throw block into the render hand (`GPlayer::GetRenderHand`,
    CHand +0x48C8, rep movsd 0x5DB0A5..0x5DB0B6): a release throws with the recorded v and h.
  - Camera: `GCamera::SetPositionAndFocus` at 0x5DB10D and `LH3DTech::UpdateCamera` at 0x5DB120; the camera mode takes it from the records (`CameraModeNew3` [0x45D276, 0x46057A]).
  - A trigger in the record becomes the script's pending trigger (+0x88 = 1, 0x5DB125).
  - The message goes through the dispatcher fn_005D9BC0 (button bits from table 0xD186B8, 0x5DB2EE).
  - **The click found again** (0x5DB1B2-0x5DB2AD, before the dispatch): fn_005DB4B0 is true for messages 3, 4 and 6
    only (not 5, the double click). Then, with recorded type +0x74 = 0, the interface's collide object becomes null and
    its distance FLT_MAX, the point kept. Otherwise the object under the hand is kept when its `GetScriptObjectType`
    (vt+0x4E8) is the recorded type, its `GScript::GetSubType` equals the recorded subtype +0x78 exactly (a recorded
    5000 never keeps it) and it is under 3.0 m (`GetDistanceInMetres` from its MapCoords) from the recorded place
    (+0x68, +0x6C, +0x70). Else `GScript::FindNearPos(place, FindNearGeneralCheck, type, subtype, 3.0f)` 0x6F7280:
    nothing found, or a find that is not an `Object` (vt+0x460 0x5DB261: a forest, a flock, a planned object), leaves
    the click as it was; an `Object` resets the whole collide (the point zeroed, `SetToZero` 0x5DB26F, every distance
    FLT_MAX) and fn_005D5E40 sets it with the record's distance +0x64 only when `IsInteractable` (vt+0x190, 0x5D5E50)
    is 1. The press's near-object fallback then runs as for a click on nothing (xref note #484).
  - Tricon flags (0x5DB0BC..0x5DB0EA): [0xC5B0F4] (0 in the first and the last 20 records) and [0xC5B0AC]. The count
    they are tested with is taken once per call (0x5DAFC9..0x5DB000): n = [0xD18854] / 0x7C after the call's first read,
    against the file's size [0xD18858] / 0x7C (n < 20 or size − n < 20); the later reads of the call keep it. Only a
    record that is played writes them (the not-due and trigger-wait exits at 0x5DB02E / 0x5DB04E come before).
  - The hand reads them in ObtainRequiredHandPosition while `IsPlayBack` (0x5B68A9): 2 → its rotate hint 1, 1 → its
    pitch hint 2, 4 → its zoom hint 8, ORed into the CameraModeNew3's +0x90, and +0x4858 (the mouse's x moved) = 1
    (0x5B68B2..0x5B68CB).
- **EndPlayBack** (`GInterface::EndPlayBack` 0x5DB3F0): only acts if there is a demo: closes the file, +0x15C = +0x160 = 0, `SetTurnOffMouseMove(0)`, script +0x8C = 0, and `SetActive(0)` if HelpSystem +0x45EC is set (a script still has the bars). The bars and the pending trigger (+0x88) stay.

## openblack

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Hand demos**
  - The code: `src/Input/HandDemo.{h,cpp}`, namespace `openblack::hand_demo` (`hand_demo::Play / IsPlaying /
    ConsumeTrigger / End / EndIfTask / Update`). `Play` is `GScript::PlayHandDemo` with `StartPlayBack`, `End` is
    `GInterface::EndPlayBack`, `IsPlaying` is `IsPlayBack`, and `EndIfTask` is the task stop callback: it ends the
    demo when the stopped task owns it. `Update` is the playback, once a frame before the hand's ray.
  - Without keepHand, the leash held in the hand comes off with `creature_loop::ReleaseLeashHeldInHand`.
  - The record's visual clock (+0x60) is `Record::timeMs`, against `game_clock::VisualMs`.
  - The record's object fields (+0x64..+0x78) are all read into `hand_demo::Record` (`ParseRecords`).
  - The playback runs every frame only, where the original's pump runs every frame and every turn (⚠️ approximate).
  - The game speed is set to 1 once per call that reads a record (`hand_demo::StepRecords`, whose read count
    `ProcessRecords` tests).
  - MOUSE_MOVE: `hand_demo::CursorPixel`, with the cinema bars' fraction
    (`CinematicDirectorSystemInterface::GetWideScreenFraction`). The debug bar's Hand menu switch "Hand demo:
    whole-screen cursor (old)", off by default, maps the cursor over the whole screen instead when it is on.
  - The tricon flags: `StepRecords`' `StepResult::hints` (the last played record's, 0 near either end with
    `k_HintlessRecords` = 20; none when the step plays nothing, the hints shown staying as they were), kept by the
    playback and read through `hand_demo::PlayingHints`.
  - The hand's hints: `hand_demo::HintsAsTricons`, in `HandSystem::HoverPose`. Our camera does not work its own hints
    out while a demo plays, so the demo's are the hand's hints then. Not ported: the mouse's x forced as moved for the
    up's Zoomers.
  - ⚠️ approximate: openblack's hand reads the button state once a frame, so a press and a release in the same frame
    would be lost.
  - The click found again is `hand_demo::ClickCanRetarget` / `TargetOfClick` / `TakesRecordedClick` /
    `OutcomeOfFind` (pure, `test_hand_demo`). The playback keeps the last action record of the frame
    (`hand_demo::TakeClick`), and `HandSystem::RetargetDemoClick` applies it to the hand's object under the cursor
    before the screen object and the press read it; the search is `map_cells::FindNearForScript` measured to
    `ScriptDistancePoint`. ⚠️ approximate: the action's point is left where the hand put it (vanilla zeroes it for a
    found object it cannot act on). Unverified against v1.42. The debug bar's Hand menu "Hand demo: no click retarget
    (old)" (off) leaves the click on what is under the cursor.
  - **Test hook:** `OPENBLACK_TEST_HAND_DEMO=<name>` plays `Data\HandDemo\<name>.hnd` once the landscape exists.

### Build, test, run

- Build with the CMake presets (`ninja-multi-vcpkg`). The sources, headers and shaders are collected with GLOBs
  marked `CONFIGURE_DEPENDS`, so adding, removing or renaming one makes ninja run CMake again by itself; a test file
  is added by hand to its group in `test/CMakeLists.txt`. Each shader also depends on the files it `#include`s, so an
  edit to one rebuilds every shader that reaches it.
- vcpkg's install step runs only when what it installs from changed since the last install in the build folder (the
  manifest, the patched ports, the vcpkg baseline, the triplet or the compiler); the CMake cache variable
  `OPENBLACK_VCPKG_INSTALL=ALWAYS` (`-DOPENBLACK_VCPKG_INSTALL=ALWAYS`) runs it at every CMake run, as before
  (2026-10-21). An install that fails or is stopped leaves no stamp, so the next CMake run installs again instead of
  skipping on a half-removed tree, and a new build folder's second CMake run skips the install too (2026-10-23).
- vcpkg builds its ports with the CMake it finds. Configure from a `vcvars64` shell, where it finds Visual Studio's;
  if it ever downloads its own CMake into `vcpkg\downloads\tools`, every port's hash changes and none is restored from
  the binary cache, so delete that folder before configuring again. In CLion, the preset profiles take the default
  toolchain, which must be Visual Studio.
- MSVC builds are reproducible (`/Brepro`): objects, libraries and executables carry no time stamps, so two builds of
  one commit in one folder give byte-identical files, and an executable that did not change can be told by its bytes
  (2026-10-21).
- Tests in `cmake-build-presets\ninja-multi-vcpkg\bin\Release`: one `test_*.exe` per area and `test_mod_loader`
  ([Unit and integration tests](testing.md#unit-and-integration-tests)). Each test file is in its area's executable:
  `test_camera` (the 11 recordings of the original) in `test_camera`; `test_set_camera_pos`, `test_game_initialize`,
  `test_load_scene`, `test_mobile_wall_hug` and `test_fixed` in `test_game`; `test_zoomer`, `test_game_matrix` and
  `test_land_normal` in `test_core`… They must all pass.
- Without `-W/-H`, the window takes 85 % of the usable desktop and is centred; larger sizes are reduced.
- By default the game runs from the first frame, like the original (turns and scripts from the first frame);
  `OPENBLACK_START_PAUSED=1` starts paused like the old openblack.

### Debugging a crash

- `Common/CrashHandler` (raffclar's): failed assertions, C runtime errors, `abort`, `std::terminate`, fatal signals,
  unhandled system exceptions and exceptions reaching `main` write a report with the stack to stderr and to
  `crashes/openblack-crash-<date>-<time>-<pid>.txt`, plus a `.dmp` minidump on Windows. `crashes/` sits next to the log
  file (next to the exe when logging to the terminal); the report is also copied into the log. Exit codes: 3 for
  abort-like crashes, the exception code for system exceptions, 1 for an exception caught in `main`.
  `--crash-dialogs` leaves the system's own dialogs instead. The pure part (report text, file names, exit codes) is
  `Common/CrashReport`, tested in `test_crash_report`. Names and lines only if the `.pdb` is next to the exe: build
  `RelWithDebInfo` (output in `bin\RelWithDebInfo`).
- A crash once seen in a RelWithDebInfo build loading Land1, 0xC0000005 in Bullet's
  `btCollisionWorld::updateSingleAabb` (`stepSimulation`, from `Game::Update`), cannot come back in that form: Bullet
  has since left openblack (`fafd05625`,
  [Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)). Its
  cause was never found.
- `OPENBLACK_FLUSH_LOG=1`: the log is written line by line (the last lines before a crash are not lost).
- Scripts: `LHScriptX::Script` skips the line it does not understand (`ScriptError`, `LexerException`) and logs it
  ("line skipped"); the skirmish maps contain typos (double quote, empty argument, stray words) that the
  original tolerates. An integer is valid where a decimal is expected and vice versa; integers are saturated.
  `CREATE_BASE_WITH_ANGLE` does not exist yet (it is skipped). The gods' `.lnd` files store in `blockSize` the size of all
  the blocks together.

### Debug environment variables

`OPENBLACK_PROFILE=<s>` (profiler summary in the log), `OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz"` (the flight
takes a few thousand frames: use `-n 8000 --screenshot-frame 7900`; when the Land 1 intro plays it
fades to black at around 90 s of play (`SetAviSequence` / `ObjectDelete` in the log) and at 7900 the shots come out
black or half faded, so there use `-n 6000 --screenshot-frame 5900`, with the flight already still, and check that
the shot is not black; the clouds with `OPENBLACK_CLOUD_SEED` are born the same but advance with the real milliseconds,
so two shots cannot be compared pixel by pixel in the sky), `OPENBLACK_DUMP_COAST_ALPHA=1` (or `=<file>.png`: dumps the coastal alpha texture, x to the right and z
downwards from the island's first block, which is printed in the log), `OPENBLACK_DUMP_BLOCK_TEXTURE=1` (or `=<file>.png`: the RGBA block texture of the whole
island, colour and coastal alpha, same orientation),
`OPENBLACK_PRINT_ALTITUDE="x,z"` (game height, of the drawn mesh and unflattened,
physical terrain and nearby objects), `OPENBLACK_MARK_LOWEST=1` (marks the lowest vertex of the nearby rocks),
`OPENBLACK_SEA_TRACE=1` (every 500 frames, the sea rows: first row, n, 1/z and its step, smooth top row,
sea frame and drift; see [sea](../world/sea-coast-and-rivers.md#sea)),
`OPENBLACK_DUMP_STATIC_GAPS=1`,
`OPENBLACK_DUMP_LAND_AVOID=1` (or `=<file>.png`: the creature's `LandAvoid` mask when loading the landscape, one
pixel per cell, green 0, blue 6, red 1, grey 2; see [the creature's LandAvoid mask](../world/water.md#the-creatures-landavoid-mask)), `OPENBLACK_HAND_TRACE=1`, `OPENBLACK_HAND_TEST_ROCK="x,z"`
(+ `_FOOD`, `_NO_BOULDER`), `OPENBLACK_HAND_TEST_TREE="x,z[,dead][,roots][,alpha][,store]"` (with `roots` and
`alpha`, the meshes `roots` places beside the tree are drawn with the opacity `OPENBLACK_TEST_ALPHA`, 0.5 by default),
`OPENBLACK_TEST_TREE_GROWTH="x,z"` (two saplings there, one in a forest and another without a forest: only the first grows),
`OPENBLACK_TREE_TRACE=1` (each growth step, the trees a forest plants, the bending of crowns and the glow),
`OPENBLACK_TEST_REPLANT="x,z,degrees"` (drops there a tree tilted by those degrees and says whether it is replanted, falls with physics
or stays dead),
`OPENBLACK_HAND_TEST_STORE_TAKE="wood,food"`, `OPENBLACK_HAND_ANIM=<node>`, `OPENBLACK_NO_PICKUP_PSYS=1`,
`OPENBLACK_HAND_TEST_HOLD=<scale>` (the hand starts holding a rock),
`OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"` (the hand holds a rock 0, a pot of 300 food 1 or of wood 2,
the first villager 3, the first tree 4 or the first animal 5, and drops it gently there after those seconds of game time: over the sea it must fall with physics, see
[water in impacts and when dropping](../physics/physics-impacts.md#water-in-impacts-and-when-dropping)), `OPENBLACK_TEST_SEA="x,z,type[,height]"` (`villager|animal|tree|pot|rock`: creates that object at that height, 2 by default, above the point and puts it into physics with no velocity; the log gives the cell, the density, the radius and `GET_LAND_HEIGHT` there and on the reference land, and the drowning villager's counter every 100 turns; with `OPENBLACK_PHYSICS_TRACE=1` it can be seen sinking, see [sinking, drowning and being deleted](../world/water.md#sinking-drowning-and-being-deleted); with `OPENBLACK_TEST_CUT=1` the object also carries `components::CutByPlane` and its part under the water is drawn cut in 0xFF303070, see [cutting by the water plane](../rendering/meshes-and-lod.md#cutting-by-the-water-plane)), `OPENBLACK_TEST_SHARK=1` ([sharks](../world/sea-life-and-scenery.md#sharks); the sharks part of `FollowUs` in Land 1: two `SharkArchetype` in `CONVERT_CAMERA_FOCUS(221)` and `(230)` with `WALK_PATH` along tracks 21 and 20 of `camera.edt`; `="track,camera[,forward[,from[,to]]]"` a single one; see [the script camera and camera tracks](../camera/script-camera.md)), `OPENBLACK_WALK_PATH_TRACE=1` (every turn of every `WALK_PATH`: sample, segment, t, focus point and position set), `OPENBLACK_TEST_JC_SPECIAL="6[,mode[,frames[,ms]]]"` (the missionaries' boat, [the missionaries' boat (PetitNavire)](../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire), `PLAY_JC_SPECIAL(6)`, those frames after having a landscape; mode 1 starts on the crossing; `ms` advances the boat in one go in 33 ms steps for shots at a given moment, e.g. `6,0,7880,7200` with `-n 8000 --screenshot-frame 7900` and `OPENBLACK_CAMERA_FLY="1892,14,3176,1866,6,3161"` gives the splash; `OPENBLACK_BOAT_TRACE=1` writes mode, time, hull and sprites every 500 ms), `OPENBLACK_TEST_BUILT_PERCENTAGE="p"` (the Land 1 ArkDryDock where `TheMissionaries` creates it, with `BUILT_PERCENTAGE` = p; camera `1905,22,3180,1881,8,3154`), `OPENBLACK_TEST_FISH_PUZZLE="x,z[,inside]"` (the script's `PuzzleGame` 14 at (x, 0, z), processed once: bait, net of 7 floats and the 2 shoals; when the net closes the next turn writes `PuzzleGame 14 played`; the shoals only count with the camera closer than 300; in Land 4 `2497.9,3628.35`; with `inside` = 1 all the fish start at the bait and the net closes after 500 ms; with `OPENBLACK_HAND_TRACE=1` it writes `Fish puzzle: inside N/30` and `net closed`, see [fish puzzle](../world/sea-life-and-scenery.md#fish-puzzle)), `OPENBLACK_TEST_SPLASH="x,z"` (one hand
splash per second), `OPENBLACK_AUDIO_TRACE=1` (each start of `Audio/SamplePlay` with mode, owner, channel and gain, each channel stopped and why, each sample that ends and each 3D cut by distance; the `AudioManager` emitters no longer exist),
`OPENBLACK_HAND_TEST_FISH=1` (splash and
fishing in the first shoal with the action held for 3 s; with `OPENBLACK_HAND_TRACE=1` it writes `Fish trace`), `OPENBLACK_START_PAUSED=1` (starts paused like the old openblack; by default the game runs from the first frame, like the original: turns and
scripts from the first frame), `OPENBLACK_TEST_FADE="r,g,b,seconds"` (`SET_FADE`), `OPENBLACK_TEST_VIEW_VILLAGER="n[,distance[,angle]]"` (the camera looks at villager n from that distance and side; the villagers walk, so from far away they can leave the frame; beware: it sets the view only once, when there is a landscape, and the ~5 s intro of Land 1 then moves the camera, so in a late capture the view is no longer on the villager: it is not useful for BEFORE/AFTER captures, use `OPENBLACK_CAMERA_FLY`, which holds it every turn), `OPENBLACK_TEST_WIDESCREEN=1`, `OPENBLACK_TEST_CHIMNEY=all` (all the chimneys give off smoke even if nobody is at home; [chimney smoke (LH3DSmoke)](../world/objects.md#chimney-smoke-lh3dsmoke)), `OPENBLACK_CLOUD_SEED=<n>` (calls `game_random::crt::Srand(n)` once before the first sky: it seeds the CRT `rand()` stream that the whole game shares, not one of the clouds' own; 🔍 inferred the sky only comes out the same if the previous CRT draws are the same; there is no longer a time seed: the original's only `srand(time)`, 0x577721 in fn_005776E0, only runs when saving creature.lhp), `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (target of the sky alignment: −1 evil, 0 neutral, 1 good; instead of the debug slider), `OPENBLACK_LOG_ISOK=1` (one `isok:` line in the log for each script tree, pot or mobile object that `IsOkToCreateAtPos` does not allow to be created, with what blocks it; see [map script objects (street lanterns, bonfires, dead trees, gates)](../scripts/map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)), `OPENBLACK_SCENERY_TRACE=1` (the fixed scenery of Land 3/4, `ECS/DesignedScenery`: creation, land change and each ring of the waterfall with its V; see [fixed scenery per land: Land 3 waterfall, Land 4 ark and dinosaur](../world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)), `OPENBLACK_SOUND_TAG_TRACE=1` (`Audio/SoundTags`: each tag created, deleted, released or delayed, and every 50 turns the channel of each one), `OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` / `_LANTERN` / `_NO_WIDESCREEN` (camera on a villager or on a lantern, forced clip, without the widescreen filter: [test hooks](../audio/engine.md#test-hooks)), `OPENBLACK_ATMOS_TRACE=<n>` (every n turns the lines of the original `GSoundMap::Dump` — `Sound Map Calc Update X=%d Z=%d %s Count=%d`, `Sound Map At Hand ...`, `Sound Radius=%3.3f Distance=%3.3f DistanceAboveLand=%3.3f` — and of `ProcessAtmosBanks` — `%s Vol=%3.3f Sent=%d Step=%d` per bank —, plus an "(openblack)" line with the 14 volumes and their cells; and each loop start and release of the ambience), `OPENBLACK_LANTERN_SOUND_TRACE=1` (writes `Lantern sound:` in the log: start, cut and release of each lantern's loop with its distance, and every 50 turns the number of lanterns, whether it is night and the distance of the nearest one; test it with `OPENBLACK_TIME_OF_DAY=22` and the camera within 5 units of the tip of a lantern). `OPENBLACK_PSYS_SOUND_TRACE=1`: trace of the PSys sounds (start with action, size, surface, distance and sample from spells.sad; "too far" if the camera is farther than the sample's maxDist; loop release or cut when the atom dies; deletion), see [sound of the particles](../audio/magic-and-weather-sounds.md#sound-of-the-particles). Influence ([influence](../magic/influence-and-alignment.md#influence)): `OPENBLACK_TEST_INFLUENCE="x,z[;x,z...]"` writes in the log, on turns 2 and 100, what `GET_INFLUENCE(0, 0, pos)` would give at each point (with the raw value, whether there is an anti ring, and the radii of citadels and towns); `OPENBLACK_TEST_INFLUENCE_RING="x,z,radius[,anti[,player]]"` creates on turn 1 a ring like `INFLUENCE_POSITION`; `OPENBLACK_INFLUENCE_EVERYWHERE=1` is the original's "GatheringFlag" flag (influence 1 everywhere). Time and weather ([weather and climate](../world/weather.md#weather-and-climate)): `OPENBLACK_TEST_WEATHER="x,z,radius[,rain[,fade[,temperature[,sheetMin,sheetMax[,forkMin,forkMax[,snow]]]]]]"` (the last value makes the test storm snow, none unless given; the sheet lightning and forked lightning intervals of the descriptor +0x30..+0x3C; with them `GWeather::Update` starts the flash fn_00837290, which the miracle storm never requests: fn_006D5730 leaves them at 0) registers on turn 1 a static storm made like that of the storm miracle (inner `max(radius, 60)`, outer `max(2.5·radius, inner + 20, 80)`: the three `fcomp; test ah, 0x41; je` of fn_006D5730 keep the value only if it is larger, rain 100, cloudiness 80, 20 degrees, almost infinite lifetime, fade 1 s) and writes on turns 2 and 30 what `GClimate::ComputeWeather` returns at the centre, inside, at the two radii and outside (temperature, rain, snow, cloudiness, wind in bytes and in m/s, `GetMaxRainingOrSnowing` and `GetTemp`); the grid is 40 m, so points in the same cell give the same value. `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` writes the same at those points. `OPENBLACK_WEATHER_TRACE=1` writes each game day the climates (temperature and target, wind, rain desire, dry and raining days, storms), every 50 turns the storms, and once per second what the rain and the falling snow draw (`Rain: N tiles`, `Snow: N tiles`). Worship and one-shot miracles ([worship: where miracles come from](../magic/worship.md#worship-where-miracles-come-from)), all on turn 1 except where stated: `OPENBLACK_TEST_WORSHIP_SITE="<TRIBE>[,<SEED>...]"` creates that tribe's worship site at the player's citadel, like `CREATE_WORSHIP_SITE`, with an icon for each seed (the scripts do not give the human player any built town centre, so its citadel would never have its own site); `OPENBLACK_TEST_WORSHIP_PLAYER="<n>"` makes these hooks act as player n instead of the human; `OPENBLACK_TEST_TOWN_SPELL="<town>,<MAGIC>[;...]"` is `SET_MAGIC_IN_OBJECT(town, magic, 1)` (the town stores the magic and its owner enables it); `OPENBLACK_TEST_MANA="<chants>"` is `GAME_SET_MANA` at that player's first worship site; `OPENBLACK_TEST_WORSHIP="<town>,<fraction>"` is `Town::SetWorshipPercentage` (what dragging the totem does); `OPENBLACK_TEST_TAP_ICON="<SEED>[,turn...]"` taps the player's icon for that seed on those turns (by default turn 5) and writes the magic, what is needed, the result, the store and whether it stays charging; `OPENBLACK_TEST_TAP="x,z,turn"` taps the nearest tappable object to that point (an icon, a town centre icon or a one-shot ball); `OPENBLACK_TEST_DISPENSER="<ABODE>,x,z,<MAGIC>[,seconds]"` creates a dispenser like the Land1 challenge script (`GiveSpellDispenserReward`); `OPENBLACK_TEST_FIREFLY_REWARD="x,z[,n]"` rolls the fireflies reward n times there. `OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"` puts the camera there every turn (in Land 1 the script takes the camera with START_CAMERA_CONTROL and its intro waits for MOVE_GAME_THING; without `OPENBLACK_CAMERA_LOCK`, `OPENBLACK_CAMERA_FLY` also holds the camera at its end point every turn). `OPENBLACK_WORSHIP_TRACE=1` writes each worship site when it is created (player, tribe, slot, position and angle), its icons, and every turn its chant count (`icons`, `N` dancers, `C` capacity, `k` intensity, tension, battery and maximum, available and damage per dancer), as well as the charging of the icons, the dispensers' orbs and the villagers who go to worship. Physics: see [test hooks](../physics/physics.md#test-hooks).

Miracles ([spell core](../magic/spell-core.md#spell-core)), once on the
first turn with the map loaded: `OPENBLACK_TEST_SPELL="<magic>,x,z[,radius[,duration[,player[,curl]]]]"` casts like
`SPELL_AT_POS` (the creator is the neutral player, which replenishes the chants; with `player` 0..7 that player casts,
which does not replenish, and -1 is the neutral one; radius 10 and the duration of `timerWhenPlayerCasting` if not given
(-2 also requests it); "from" 30 m above the point; `curl` is the script's, PSysProcessInfo +0x34, from which the shields'
spins come) and writes what the class check (vt 0x30) would answer there; `OPENBLACK_TEST_SEED="<seed>[,pu]"` puts a
charged seed in the hand, like `OneOffSpellSeed::CreateSpellIntoHand`; `OPENBLACK_TEST_ONESHOT="<seed>,x,z[,pu[,tap]]"` creates
a one-shot ball on the ground (with `tap`, it taps it and it goes to the hand). `<magic>` is a MAGIC_TYPE number or the name
from info.dat (`FIRE`, `HEAL`, `STORM_PU2`...); `<seed>` a SPELL_SEED_TYPE number or its name (`FIRE`, `HEAL`,
`STORM`...). `OPENBLACK_SPELL_TRACE=1` writes every turn `Spell trace` per spell (chants, safety level,
strength, cost of the turn, age, closed, PSys and atoms), each applied event and the heal target searches.
With it (or with `OPENBLACK_HAND_TRACE=1`) `Pot::AddResourceToPos` writes `Pot trace` (which pile or store each
amount goes into, at what distance and with what radius, and the new piles with their 2D radius) and each grain of food or wood
`SpellResource event` (units, chants paid, dry land, strength), and `Grain trace` gives every turn the elevation of the
stream's hand (t, height, tilt and whether it is fixed); see
[food and wood](../magic/food-wood-water-heal.md#food-and-wood).

Heal ([heal](../magic/food-wood-water-heal.md#heal)):
`OPENBLACK_TEST_HURT_VILLAGERS="x,z,radius,life[,poisoned[,turn[,heal[,repeat]]]]"` sets to `life` (0..1) all the
villagers within `radius` of (x, z), poisons them with a 1, and with `heal` 1 (HEAL) or 2 (HEAL_PU_ONE) casts the
miracle right there like `SPELL_AT_POS`. It happens on turn `turn` (1 by default) and repeats every `repeat` turns (0 = only
once; useful so that there is a lit chakra in the capture). Then it writes each change: `Heal test: turn +N villager
E life a -> b, poisoned p, glow (specular) r,g,b` (the chakra's glow; `none` when there is no chakra).

Casting from the hand and gestures ([casting from the hand, gestures and hand effects](../magic/hand-casting.md#casting-from-the-hand-gestures-and-hand-effects)):
- `OPENBLACK_TEST_CAST="press@t0,release@t1[,press@t2,release@t3...][,shot@t]"` presses and releases the action (the right
  button) at those seconds from when the map exists, added to the real mouse. With a seed in the hand
  (`OPENBLACK_TEST_SEED`) it goes through the real path: arms and casts on release (HAND_GESTURE), casts on press
  (HAND_POSITION) or casts every turn while held (IN_HAND). The hand is wherever `OPENBLACK_MOUSE_AT` says.
  `shot@t` (at the end) requests a capture at that moment to `OPENBLACK_TEST_SHOT_PATH`, for captures at a game time and
  not at a frame (the frame rate varies).
- `OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`: during the first press the hand travels along that line over the terrain (the
  food, wood and water sprinkle only releases when it is moved).
- `OPENBLACK_TEST_THROW_VEL="vx,vy,vz"`: the hand velocity (ThrowVelocity of the interface state) that the
  spell receives when cast (the fireball leaves with it).
- `OPENBLACK_TEST_GESTURE="<GESTURE>[,sizePx[,cx,cy[,t]]][;<GESTURE>...]"` or `=<file.txt>` ("x y" pixels per line):
  draws the gesture's first template (GESTURE_TYPE name or number; a leading `-` mirrors it) `sizePx`
  wide (200) centred on the window fraction (cx, cy) (the centre), t seconds of game time after the map exists
  (0.5), as mouse messages every 28 ms. When it finishes it writes which gestures the buffer matches. The camera must not move
  (an `OPENBLACK_CAMERA_FLY` flight clears the buffer).
- `OPENBLACK_GESTURE_TRACE=1`: recognised gestures (template, mirror, segment), circles (position and size), help
  events and casts from the hand (also with `OPENBLACK_SPELL_TRACE=1`).

Fire and lightning bolt ([fire](../world/fire.md#fire) and [fireball and lightning](../magic/fireball-and-lightning.md#fireball-and-lightning)):
`OPENBLACK_TEST_FIRE="x,z,T[,class[,turn]]"` sets the object with fire data nearest to (x, z) to temperature T
(`SetTemperature`) or, if T ≤ 1, sets it on fire with that speed (`SetOnFire`); the class can be `any`, `tree`, `abode`,
`villager`, `field` or `fishfarm`, and the turn (counted from the first one with the map loaded) allows waiting for the camera to arrive.
`OPENBLACK_FIRE_TRACE=1` writes the new fires, the objects that are consumed, the deleted fires, the reactions and
the villagers who flee or put out fires, and every 20 turns what each burning object draws (`Fire: graphic of fire ...`:
flames, scale and alpha of the first one, steam and smoke), which serves to check the flames without a capture. The
lightning bolt and the fireball are cast with `OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,x,z,radius,duration"` and `"FIREBALL,x,z"`; with a long
duration (for example 300 s) the lightning keeps striking until the camera arrives. `OPENBLACK_SPELL_TRACE=1` writes each
type 3 event of the tips with their numbers and, per turn, `Lightning: N targets ... M of K forks struck` with the origin, the
heading of the cone and the scale of the forks. `OPENBLACK_PSYS_CHAIN_TRACE=1` writes per frame the PSys ribbons
(how many, with how many joints, their texture and where they go from and to), which is the way to tell "the ribbon is not
drawn" apart from "in that frame there were none" (the lightning flickers: a fork is only visible on the turn it strikes).

`OPENBLACK_ZSORTER_TRACE=1` writes once per second (every 60 drawn frames) a `ZSorter trace:` line with
what the frame's single transparent queue holds (`graphics::zsort`, `Renderer::DrawPass`): the total, the
entries lost to the 0x800 cap (`NewZObject` 0x83F31C), how many there are of each class (models, faded, clouds,
rain tiles, boat sprites, sprites, meshes and chains of the `Sorted` effects, `Queued` effects, mists,
smoke, sprites, hand) and the key (squared
distance) of the first and the last; see
[the single transparent queue (LH3DZSorter)](../rendering/transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter).

`OPENBLACK_SHADOW_TRACE=1` writes once per second the projected shadows (`graphics::shadow_list`): for each
`ShadowInfo` (`shadow <n> caster <entity> light <tipo> alpha <a> fade <f> box (...) kMin <k> t' <t> max n <n>
points <p>`, or why it is not drawn), the objects that receive one (`shadow receiver: instance <i> mesh <m> shadows <n>`)
and in which view it is drawn on them (`shadow on object: ... view <v>`: 4 = Main, immediately; 5 = MainBlended, inside their
queue entry). `OPENBLACK_DUMP_SHADOWS=<folder>` saves every 300 frames each texture ×8 as PNG
(`shadow_<frame>_<n>_<entity>.png`). See [projected shadows (ShadowInfo)](../rendering/shadows-and-reflections.md#projected-shadows-shadowinfo).

`OPENBLACK_ORB_TRACE=1` writes, **every drawn frame** and from `Renderer::DrawScene` (right after sorting the
list from back to front), two kinds of line in the log with the `graphics` logger:

- `Orb trace: surface (<texture>) path <p> sorted <k>/<n> origin (x, y, z)` for each `ZR_SurfRevol` surface (the
  dispenser disc, the teleport pool): the path of its effect (0 `Sorted`: drawn immediately, before
  the whole queue, `k` = −1; 1 `Queued`: `k` is the position of its effect's entry) and the effect's origin.
- `Orb trace: orb <entity> phase <p> frame <f> packed[1][3] <v> uv (u, v) alpha <a> sorted <k>/<n> key <d> sortPoint (x, y, z) inView <b>`
  for each `components::OneOffSpellSeed` (the bubble of a one-shot ball): the phase and the frame of its 4×4 sheet
  (`OneOffSpellSeed::UpdateFrame` 0x72A570), the packed value it carries to the shader in `[1][3]`
  (`frame_anim::PackUvOffset`) with the UV it represents, the alpha (1 − `[0][3]`), whether it entered the sorted list and at what
  position with what key (−1 = it did not enter: the list leaves out instances with alpha 0), the sort point of
  `OneOffSpellSeed::Draw` 0x518E90 and the result of the view volume test. If the entity has no instance
  this frame it writes `Orb trace: orb <entity> has no instance this frame`.

It serves two purposes: checking that the dispenser disc is drawn **before** the bubble and
following the bubble through the 15 → 0 wrap-around of its sheet. The scene is
`OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826,2670,10,2"` with
`OPENBLACK_CAMERA_LOCK="1816,52,2656,1826,37,2670"` (run it from a private copy of the exe). It is about 2
lines per frame, so it is advisable to limit the frames with `-n`.

Teleport ([teleport](../magic/teleport.md#teleport)): `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,player[,mode]]"` plants two teleport stones like `SPELL_AT_POS` (B at x1,z1 and A at x0,z0; player 7 = neutral and free, 0 = PLAYER_ONE spends chants). `mode`: `walk` (default, the villager nearest to A walks towards B two turns earlier and A's reaction diverts it through the stones), `drop` (a second later the villager is dropped onto A, forced jump like `fn_005FC4F0`), `none` (only the stones). `OPENBLACK_TEST_TELEPORT_TURN=<n>` delays the start (the flight of a capture takes ~160 turns). `OPENBLACK_TELEPORT_TRACE=1` (or `OPENBLACK_SPELL_TRACE=1`) writes the stones, the distribution of the reaction, the jumps (from which stone to which, the saving in metres) and the spell's `PayFor` (a useful jump adds chants, a forced backwards one costs, R13). The discs use `ZR_SurfRevol` (`RendererRevolvedSurface.cpp`).

Forest ([forest](../magic/forest.md#forest)):
`OPENBLACK_TEST_MAGIC_TURN=<n>` makes `OPENBLACK_TEST_SPELL`, `_SEED` and `_ONESHOT` wait for game turn n (so
that the `OPENBLACK_CAMERA_FLY` camera is already there); `OPENBLACK_TEST_FOREST_SHOT="<turns>,<path.png>[;<turns>,<path>...]"`
requests a capture those turns after the forest seed touches the ground (the frame number of
`--screenshot-frame` varies with the drawing speed; this request replaces the command-line one if they coincide).
Example: `OPENBLACK_TEST_MAGIC_TURN=330 OPENBLACK_TEST_SPELL=NATURE,1790,2625
OPENBLACK_CAMERA_FLY=1772,52,2604,1790,36,2625 OPENBLACK_TEST_FOREST_SHOT="3,a.png;120,b.png"` with `-n 11000`.

Water ([water](../magic/food-wood-water-heal.md#water)): `OPENBLACK_TEST_SPELL=WATER,x,z,10,6` (or
`WATER_PU1`; the cloud appears 30 m above the point, the drops fall around the point) and
`OPENBLACK_TEST_WATER_SHOT="<turns>,<path.png>[;...]"`: captures those game turns after the spell's first `Process`
(also while it is closing). With `OPENBLACK_SPELL_TRACE=1` each drop writes its point, its distance, the watered
objects and the last ring; each field, its crops, growth and food; a tree, the sapling it plants; a burning object,
reaction 34. With the hand: `OPENBLACK_TEST_SEED=WATER OPENBLACK_TEST_CAST="press@20,release@32"
OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`. Putting out a fire: `OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"` and the water
on turn 200 (`OPENBLACK_TEST_MAGIC_TURN=200`), camera `1810,36,2620,1818.6,31,2628.4`.

Flocks ([flocks](../magic/flocks.md#flocks)): `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` or `GROUND_FLOCK,x,z` (24 / 25; from 30 m above the point, the neutral player, so they head towards +x alternating sides) and `OPENBLACK_TEST_FLOCK_SHOT="<turns>,<path.png>[;...]"` (captures those game turns after the cast). With `OPENBLACK_SPELL_TRACE=1` each animal writes its departure and destination and, every 10 turns, each member its position, state and alpha. To make the camera follow them: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90" OPENBLACK_TEST_ANIMAL_SPECIES=20 OPENBLACK_TEST_VIEW_LOCK=1` (22 the wolves).

Shields ([shields](../magic/shields.md#shields)):
`OPENBLACK_TEST_SHIELD_SHOT="<turns>,<path.png>[;...]"` requests a capture those game turns after the first
MapShield is created (like the forest one). `OPENBLACK_TEST_SHIELD_FRAMES="<turns>,<n>,<prefix>[@<slow>]"` takes n captures
in consecutive frames (`<prefix>_<i>.png`, turn and fraction in the log; `@<slow>` lengthens the turn but **not** the
PSys interpolation, see [test hooks](../magic/shields.md#test-hooks)). With `OPENBLACK_SPELL_TRACE=1` it writes
`SpellShield::InitWithPos` (radius,
anti rings, reaction, town, cost per turn), every turn the physical shield (`t`, growth curve, drawn scale and
that of the object, angle, height, dying and whether it has a body in the physics) and each physical hit (momentum and chants
paid). Example of the physical shield growing: `OPENBLACK_TEST_MAGIC_TURN=300
OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,1826.8,2641.4,40,-2,-1,2" OPENBLACK_CAMERA_FLY=1720,90,2560,1826.8,40,2641.4
OPENBLACK_TEST_SHIELD_SHOT="7,a.png;10,b.png;13,c.png;25,d.png"` with `-n 16000`. A rock against it:
`OPENBLACK_TEST_PHYSICS="1745,2641.4,25,12,6,0,1.0,1"` (12 m/s: it bounces).

Lightning explosion ([lightning explosion and missing PSys classes](../magic/lightning-explosion.md#lightning-explosion-and-missing-psys-classes)):
`OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` (also `BEAM_EXPLOSION_PU1` and `_PU2`, which spread several explosions) and
`OPENBLACK_TEST_EXPLOSION_SHOT="<turns>,<path.png>[;...]"`, which requests captures those game turns after the first
step of the first explosion (like the shield one). With `OPENBLACK_SPELL_TRACE=1` it writes `Explosion: started ...`
(centre, shield margin, search radius and spiral cells) with each target (distance, radius and class), each
destroyed object (ring, exploded and deleted) and what is not ported (the meshes in pieces), and the ground mark it leaves (`Explosion: ground mark`).
Example: `OPENBLACK_TEST_MAGIC_TURN=300 OPENBLACK_TEST_SPELL="BEAM_EXPLOSION_PU2,1790,2600"
OPENBLACK_CAMERA_FLY=1700,140,2480,1790,40,2600 OPENBLACK_TEST_EXPLOSION_SHOT="12,a.png;40,b.png;80,c.png"` with
`-n 16000`.

Storm ([storm, electric storm and tornado](../magic/storm-and-tornado.md#storm-electric-storm-and-tornado)):
`OPENBLACK_TEST_SPELL="STORM,x,z,60"` (or `STORM_PU1` with lightning, `STORM_PU2` with a tornado; the radius is the spell's,
clamped to 20..1000) casts like `SPELL_AT_POS` (from 30 m above: without a heading, so the storm has no wind
of its own). `OPENBLACK_TEST_STORM_SHOT="<turns>,<path.png>[;...]"` requests a capture those game turns after the
first turn of the first storm; `OPENBLACK_TEST_STORM_STRIKE_SHOT="<n>,<path.png>[;...]"` requests it on the turn of
lightning strike no. n from the clouds (the strikes last 1 or 2 turns). `OPENBLACK_TEST_STORM_PILE="x,z,amount[,wood]"` puts a
pile of food (or of wood) there when the first storm is cast, for the tornado to pick up.
`OPENBLACK_TEST_STORM_CLOUDS="x,z,radius[,clouds[,blackness[,elevation]]]"` registers on the first frame with land a
storm with clouds (`GWeather::DrawClouds`: inner = radius, outer = 3 × radius, 8 clouds, blackness 0.5, elevation 160
by default; rain 100, fade 1 s). `OPENBLACK_STORM_TRACE=1` writes the registration of the miracle's storm (radii,
rain, cloudiness, wind, fade), each lightning strike (from which cloud, the next one, its lifetime), each thing the tornado picks up (what
it is, how high it rises) and, every 10 turns of each storm spell, its age, chants, the weather at its centre
(`ComputeWeather`), its atoms, the objects it carries and the flash at that point. Tornado example:
`OPENBLACK_TEST_MAGIC_TURN=200 OPENBLACK_TEST_SPELL="STORM_PU2,1826.8,2641.4,60"
OPENBLACK_TEST_STORM_PILE="1830,2650,400" OPENBLACK_CAMERA_FLY=1775,60,2595,1830,45,2650
OPENBLACK_TEST_STORM_SHOT="95,a.png;120,b.png"` with `-n 12000`.

The state hash's own switches: `OPENBLACK_STATE_HASH=<file>`, `OPENBLACK_STATE_HASH_ENTITIES=<part,...|all>` and
`OPENBLACK_STATE_HASH_SCRIPTS=1`, the last adding a `scripts` part (the script globals and each task's script and
instruction; [hashdiff](research-tools.md#hashdiff-which-entity-moved-first)). To drive a story land by commands rather
than by these run-wide variables (wait for a global, a task or a text; set a global, start a script; the camera to a
point; a thing of the land into the hand; the texts clicked), play a testbed timeline on it with a land entry's
`timeline=` ([Playing a timeline on a land](scenarios.md#playing-a-timeline-on-a-land)).

Useful points in Land1: `1464,2016` is **open sea** (altitude 0, water cell that is not drawn), not the beach;
shore at altitude 1 at `1485,2015`; dry land at altitude 44 at `1788.4,2710`; dry sand `1478,2129`; village store
`1826.8,2641.4` (camera `1818,75,2612,1824,44,2636`); tree next to the store `1818.6,2628.4` (the ground is at 29.4 m:
camera `1810,36,2620,1818.6,31,2628.4`). The `OPENBLACK_CAMERA_FLY` flight takes about 4000 frames, which with the map
loaded are about 150-180 game turns: to see something short-lived, it is advisable to cast it with the turn parameter.

Other switches, openblack's own:

- `OPENBLACK_FIXED_MOUSE="x,y"`: the mouse stays at that window position and the real mouse is ignored, so runs with
  a fixed time step draw the same hand and its light.
- `OPENBLACK_SCREENSHOT_GUI=1`: a requested screenshot shows the debug interface (menus, windows) too.
- `OPENBLACK_WINDOW_TAG=<tag>`: the window's title becomes `openblack [<tag>]`, so that several running copies can be
  told apart.
- `OPENBLACK_GPU_CALLS=1`: logs, once per call and resource, each GPU resource call of openblack's wrappers made by
  the game's logic rather than by the drawing (with the logic and the drawing on two threads, no such call may run on
  the logic's).
- `OPENBLACK_INSTANCE_VERIFY=1`: after each build of the frame's draw instances the full build runs once more, and
  the two are compared byte by byte (the instance rows, the draw ranges, the instance sets, the particle atoms and
  the entity-to-instance map). The first mismatch of each of the first 20 checks that have one is logged, and the
  count every 300 checks; what is drawn is the first build's.
- `OPENBLACK_TOOLTIP_TRACE=1`: every 10 turns a `Tooltip trace:` line (the text kept, whether it is alive, its display
  and after-focus turns, whether it has an icon).
- `OPENBLACK_DUMP_VILLAGER_HOMES=1`: once, at the first turn, a `Villager homes:` line for each villager (its creation
  index, its town or -1, its abode or -1, whether it is a vagrant), then the totals.
- `OPENBLACK_HAND_INFO=1`, when the hand's animations load: the `PotInfo` and `TreeInfo` rows of `info.dat` in the
  log. `OPENBLACK_HAND_DUMP=<file>`, then too: the hand's evaluated bone matrices of every clip, written to that file.
  `OPENBLACK_HAND_GRIP_PROBE=1`: logs where the fist of the hold poses (`Chold_side`, `Chold_above`, `Cwiggle`) is
  from the model's origin.
- `OPENBLACK_TEST_PICK_VILLAGER="[index[,delay]]"`: the index-th villager is put in the hand `delay` seconds in (2 by
  default), as the hand picks one up. `OPENBLACK_TEST_THROW_VILLAGER="n,vx,vy,vz"`: the n-th villager is thrown with
  that velocity (its flying and landing clips). `OPENBLACK_TEST_FELL_YAW=<yaw>`: the yaw of the beech
  `OPENBLACK_TEST_FELL` fells (0 by default).

#### On the pages of their systems

The other variables are described on the page of their system, under its "Test hooks" heading unless the link says
otherwise:

| Page | Variables |
|---|---|
| [Testing and verifying a change](testing.md#deterministic-conditions) | `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_MOUSE_AT`, `OPENBLACK_IGNORE_REAL_INPUT`, `OPENBLACK_TEST_SKIP_ANSWER`, `OPENBLACK_TIME_OF_DAY`, `OPENBLACK_TRACE_GAME_RAND`, `OPENBLACK_AUDIO_TEST_CITADEL` (the deterministic runs, what they compare and the shots) |
| [The engine loop: turns and frames](../engine/loop.md#test-hooks) | `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_TRACE_GAME_RAND` |
| [The game clock](../engine/game-clock.md#test-hooks) | `OPENBLACK_FIXED_FRAME_MS` |
| [Random numbers and streams](../engine/random.md#test-hooks) | `OPENBLACK_TRACE_GAME_RAND` |
| [Coordinates, distances and angles](../engine/coordinates-and-angles.md#test-hooks) | `OPENBLACK_CLOCK_TRACE`, `OPENBLACK_TEST_PSYS_RAND_OUTSIDE`, `OPENBLACK_TRACE_GAME_RAND` |
| [Object size and map cells](../engine/object-size-and-map-cells.md#test-hooks) | `OPENBLACK_MAPCELLS_CHECK` |
| [Media files and data packs](../formats/media-files.md#test-hooks) | `OPENBLACK_TEST_SPREAD_REACTIONS` |
| [The land script reader](../scripts/map-script-reader.md#test-hooks) | `OPENBLACK_DUMP_ENTITY_COUNTS`, `OPENBLACK_SCRIPT_THING_TRACE`, `OPENBLACK_TEST_SKIP_ANSWER`, `OPENBLACK_TEST_TEXT_CLICK` |
| [Land script objects](../scripts/map-script-objects.md#test-hooks) | `OPENBLACK_DUMP_ENTITY_COUNTS` |
| [CHL commands that move things](../scripts/script-commands.md#test-hooks) | `OPENBLACK_SCRIPT_THING_TRACE`, `OPENBLACK_TEST_TEXT_CLICK` |
| [Land changes, skipping the tutorial and saving](../scripts/land-change-and-saving.md#test-hooks) | `OPENBLACK_TEST_SKIP_ANSWER` |
| [The Land 1 intro](../scripts/land1-intro.md#test-hooks) | `OPENBLACK_HAND_DEMO_TRACE`, `OPENBLACK_TEST_TEXT_CLICK`, `OPENBLACK_TEST_TEXT_SHOT`, `OPENBLACK_TEXT_TRACE` |
| [Trees and forests](../world/trees.md#test-hooks) | `OPENBLACK_HAND_TEST_FOREST`, `OPENBLACK_TEST_FELL`, `OPENBLACK_TEST_TREE_QUERIES`, `OPENBLACK_TEST_TUG`, `OPENBLACK_TEST_TUG_MOUSE2` |
| [Objects](../world/objects.md#test-hooks) | `OPENBLACK_HAND_TEST_FIELD`, `OPENBLACK_TEST_FIELD_GROWTH` |
| [Buildings](../world/buildings.md#test-hooks) | `OPENBLACK_SFX_TRACE`, `OPENBLACK_TEST_ABODE_LIFE`, `OPENBLACK_TEST_FRAGMESH`, `OPENBLACK_TEST_HIT_ABODE`, `OPENBLACK_TEST_KNOCK` |
| [Plans and building sites](../world/building-sites.md#test-hooks) | `OPENBLACK_TEST_BUILD_AT` |
| [Fire](../world/fire.md#test-hooks) | `OPENBLACK_VILLAGER_TRACE` |
| [Day and night](../world/day-and-night.md#test-hooks) | `OPENBLACK_CLOCK_TRACE`, `OPENBLACK_TEST_MOVE_TIME` |
| [Villagers: data and states](../beings/villager-states.md#test-hooks) | `OPENBLACK_TEST_ABODE_LIFE`, `OPENBLACK_TEST_BUILD_AT`, `OPENBLACK_TEST_CORPSE_TURNS`, `OPENBLACK_TEST_HIT_ABODE`, `OPENBLACK_TEST_TOWN_DESIRE`, `OPENBLACK_TEST_TOWN_EMERGENCY`, `OPENBLACK_TEST_VILLAGER_BORN_IN_WATER`, `OPENBLACK_TEST_VILLAGER_KILL`, `OPENBLACK_TEST_VILLAGER_LIFE`, `OPENBLACK_TEST_VILLAGER_POISONED`, `OPENBLACK_TEST_VILLAGER_STATE`, `OPENBLACK_TOWN_TRACE`, `OPENBLACK_VILLAGER_TRACE` |
| [Villagers: deciding what to do](../beings/villager-decisions.md#test-hooks) | `OPENBLACK_TEST_VILLAGER_FOOD`, `OPENBLACK_TEST_VILLAGER_NOTHING`, `OPENBLACK_TEST_VILLAGER_SHOT` |
| [Villagers: jobs](../beings/villager-jobs.md#test-hooks) | `OPENBLACK_HAND_TEST_FOREST`, `OPENBLACK_TEST_BUILD_AT`, `OPENBLACK_TEST_FELL`, `OPENBLACK_TEST_FIELD_GROWTH`, `OPENBLACK_TEST_TOWN_DESIRE`, `OPENBLACK_TEST_TREE_QUERIES`, `OPENBLACK_TEST_VILLAGER_CARRY`, `OPENBLACK_TOWN_TRACE`, `OPENBLACK_VILLAGER_TRACE` |
| [Villagers: home, food, sleep, age and births](../beings/villager-life.md#test-hooks) | `OPENBLACK_TEST_HOMELESS`, `OPENBLACK_TEST_VILLAGER_AGE`, `OPENBLACK_TOWN_TRACE`, `OPENBLACK_VILLAGER_TRACE` |
| [Villagers: picked up, dropped, hurt and dead](../beings/villager-hand-and-death.md#test-hooks) | `OPENBLACK_ANIMAL_TRACE`, `OPENBLACK_VILLAGER_TRACE` |
| [Towns](../beings/towns.md#test-hooks) | `OPENBLACK_TEST_TOWN_DESIRE`, `OPENBLACK_TEST_TOWN_EMERGENCY`, `OPENBLACK_TOWN_TRACE` |
| [Animals: species and the turn](../beings/animals.md#test-hooks) | `OPENBLACK_ANIMAL_TRACE`, `OPENBLACK_TEST_CANNOT_BE_EATEN`, `OPENBLACK_TEST_CORPSE_TURNS`, `OPENBLACK_TEST_FOOD_BEHIND`, `OPENBLACK_TEST_FOOD_PILE`, `OPENBLACK_TEST_HUNGRY`, `OPENBLACK_TEST_HUNT_VILLAGER`, `OPENBLACK_TEST_KILL_ANIMAL`, `OPENBLACK_TEST_LAIRS`, `OPENBLACK_TEST_SCRIPT_HELD`, `OPENBLACK_TEST_SMOKE`, `OPENBLACK_TEST_SPREAD_REACTIONS`, `OPENBLACK_TEST_THROW_ANIMAL` |
| [Walking round obstacles](../beings/wall-hugging.md#test-hooks) | `OPENBLACK_TEST_FOOD_PILE` |
| [Skeletal animation](../beings/animation.md#test-hooks) | `OPENBLACK_ANIM_TRACE`, `OPENBLACK_DRAW_TRACE`, `OPENBLACK_OBJECT_INDEX_TRACE`, `OPENBLACK_TEST_ANIM`, `OPENBLACK_TEST_CARRY` |
| [Influence and alignment](../magic/influence-and-alignment.md#test-hooks) | `OPENBLACK_ALIGNMENT_TRACE` |
| [Shields](../magic/shields.md#test-hooks) | `OPENBLACK_TEST_SHIELD_REACTION` |
| [The falling spell](../magic/falling-spell.md#test-hooks) | `OPENBLACK_TEST_FALL_BURST_AT`, `OPENBLACK_TEST_FALL_LOG` |
| [Physics](../physics/physics.md#test-hooks) | `OPENBLACK_HAND_TEST_SPLIT`, `OPENBLACK_TEST_HIT_ABODE`, `OPENBLACK_TEST_HIT_VILLAGER`, `OPENBLACK_TEST_THROW_TREE` |
| [Physics impacts](../physics/physics-impacts.md#test-hooks) | `OPENBLACK_HAND_TEST_SPLIT`, `OPENBLACK_TEST_FRAGMESH`, `OPENBLACK_TEST_HIT_ABODE` |
| [Particles: the PSys core](../physics/particles.md#test-hooks) | `OPENBLACK_PSYS_TRACE`, `OPENBLACK_TEST_PSYS`, `OPENBLACK_TEST_PSYS_RAND_OUTSIDE` |
| [Particle rules](../physics/particle-rules.md#test-hooks) | `OPENBLACK_TEST_PSYS` |
| [The hand: animation and placement](../hand/animation-and-placement.md#test-hooks) | `OPENBLACK_TEST_HAND_ALIGNMENT` |
| [Grabbing, holding and throwing](../hand/grab-hold-and-throw.md#test-hooks) | `OPENBLACK_HAND_TEST_FOREST`, `OPENBLACK_TEST_TAP_SCROLL`, `OPENBLACK_TEST_TUG`, `OPENBLACK_TEST_TUG_MOUSE2` |
| [Screen, text and read-outs](../interface/screen-and-readouts.md#test-hooks) | `OPENBLACK_TEST_TOOLTIP` |
| [The temple interior](../interface/temple-interior.md#test-hooks) | `OPENBLACK_AUDIO_TEST_CITADEL` |
| [The frame and its draw stages](../rendering/frame.md#test-hooks) | `OPENBLACK_TEST_TOOLTIP` |
| [Model lighting and colour](../rendering/model-lighting.md#test-hooks) | `OPENBLACK_TEST_TUG` |
| [The audio engine](../audio/engine.md#test-hooks) | `OPENBLACK_ANIM_TRACE`, `OPENBLACK_AUDIO_TEST_ANIM`, `OPENBLACK_AUDIO_TEST_CITADEL`, `OPENBLACK_AUDIO_TEST_LANTERN`, `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN`, `OPENBLACK_GUIDANCE_TRACE`, `OPENBLACK_MUSIC_TRACE`, `OPENBLACK_PLAYER_NAME`, `OPENBLACK_SFX_TRACE`, `OPENBLACK_TEST_ALIGNMENT_MUSIC`, `OPENBLACK_TEST_GUIDANCE_SAY`, `OPENBLACK_TEST_MUSIC`, `OPENBLACK_TEST_MUSIC_VOLUME`, `OPENBLACK_TEST_SAMPLE_VOLUME`, `OPENBLACK_TEST_SCRIPT_MUSIC`, `OPENBLACK_TEST_TEXT_CLICK`, `OPENBLACK_TEXT_TRACE` |
| [Music](../audio/music.md#test-hooks) | `OPENBLACK_TEST_MUSIC` |
| [Bink video](../video/bink.md#test-hooks) | `OPENBLACK_TEST_VIDEO` |
| [Film playback](../video/playback.md#test-hooks) | `OPENBLACK_TEST_VIDEO` |
| [openblack internals](internals.md#render) (the Render section) | `OPENBLACK_DRAW_STATS`, `OPENBLACK_TEST_MAP_CYCLE` |
| [Testbed scenarios](scenarios.md#test-hooks) | `OPENBLACK_AUDIO_TEST_CITADEL`, `OPENBLACK_TEST_MENU_AT`, `OPENBLACK_TEST_SKIP_ANSWER` |
| [Research tools](research-tools.md#test-hooks) | `OPENBLACK_FIXED_FRAME_MS` |

### Tests and test data

- `test_food_wood`: the HandStateGrain spline and its loop, the cost of each grain, the pile sounds,
  `GetProportionRaised`, the emission of SF_Food (18 grains/s with the hand still, 1 per 2.5 m when moving it) and the reading of
  ARRAY with floats; with `OPENBLACK_GAME_PATH` the real food and wood rows.
- `test_spell_forest`: the forest spiral, the target scale, how many trees it wants, the cost and
  GetMaxObjectsToCreate; with `OPENBLACK_GAME_PATH` the real NATURE row and the `magicTreeTypes`.
- `test_shield` ([shields](../magic/shields.md#shields)):
  the clamping and the cost of the radius, the ProcessShield curves, the sphere helpers (inside, crossing, intersection,
  bounce), the DefensiveSphere registration with a real effect and `DoAnyShieldDeflections`, and the frame of the PSys
  hierarchies (the marked ancestors, with their scale); with `OPENBLACK_GAME_PATH`, the real rows of both shields.
- `test_influence`: the ring curve, the town and citadel radius, the anti rings and those that follow an object.
- `test_lightning` ([lightning](../magic/fireball-and-lightning.md#lightning)): that
  `UR_Lightning`, `UR_LightningStrike`, `ParticleChainCreator` and `ParticleLightMapCreator` are registered, the
  properties they read and the per-segment UV of the ribbon; with `OPENBLACK_GAME_PATH`, the real `SF_LightningBolt*` and
  `SF_LightningStrike` and the size of the light map `.raw`.
- `test_spell_chants` (the chant economy of [the spell core](../magic/spell-core.md), without a world): a player's lightning bolt and the
  neutral player's, the sustained shield and the tribal power, the edge cases of the strength and the alignment distribution;
  with `OPENBLACK_GAME_PATH` also the real rows of the lightning bolt and the shield.
- `test_worship` (the worship economy of [the spell core](../magic/spell-core.md), without a world except a registry): the capacity and
  battery of the site by dancers, its end-of-turn count (dance intensity, battery and damage per
  dancer), the tension, the failure of the maintenance reserve, `UseChants`, the excess of `AddToChantStore`,
  how many villagers a town requests, `SigmoidThreshold` and the fireflies' probabilities.
- `test_magic_tables` (miracle tables, [the spell core](../magic/spell-core.md)): with `OPENBLACK_GAME_PATH=<installation>` it also
  checks the real `Scripts\info.dat`; without it that test is skipped.
- `test_spell_sounds` (PSys sound, [sound of the particles](../audio/magic-and-weather-sounds.md#sound-of-the-particles)): likewise, with `OPENBLACK_GAME_PATH` it also reads
  `Data\SoundAction.h`, `spells.sad` and `SF_TeleportVortex`.
- `test_weather` (time and weather, [weather and climate](../world/weather.md#weather-and-climate)), without a world: the
  byte arithmetic, the date and the seasons, the fade and the `CalcAtmos` of a storm, the 40 m grid and the falloff with
  height, the deletion over two turns, `KILL_STORMS_IN_AREA`, the sum of the climates, the temperature oscillation, the
  storm that creates a climate with desire 1 and the bytes of `CREATE_WEATHER_STORM`.
- `test_water_queries` (water queries and `LandAvoid`) needs the original data: it is **skipped** if
  `OPENBLACK_TEST_GAME_PATH=<game folder>` is not defined. It loads `Scripts\Land1.txt` once for the whole suite
  (beware: the magic tests use `OPENBLACK_GAME_PATH`, this one `OPENBLACK_TEST_GAME_PATH`).
- A third variable: the tests that read installed audio, help, video and HelpSprite files (`test_audio_tables`,
  `test_music_bank`, `test_game_music`, `test_sample_play`, `test_help_system`, `test_help_dude_file`, `test_voices`,
  `test_ui_sfx`, `test_bik_file`, `test_villager_draw_golden`…) use `OPENBLACK_TEST_BW_ROOT=<game root>` and skip
  without it (`test_villager_draw_golden` then tries `OPENBLACK_GAME_PATH`, then the default installation). Full list
  in [test hooks](../audio/engine.md#test-hooks).
- `OPENBLACK_LAND1_LND=<file>`: the `Land1.lnd` that the tests reading Land 1's landscape take (`test_draw_list_blocks`,
  `test_land_clip`, `test_low_res_tile`, `test_sea_cells`), in place of `Data\Landscape\Land1.lnd` under
  `OPENBLACK_GAME_PATH`.
- `OPENBLACK_GOLDEN_DUMP=1`: `test_villager_draw_golden` prints its table of values (between its `GOLDEN-BEGIN` and
  `GOLDEN-END` markers, to paste into the test) instead of comparing with it.
- `OPENBLACK_CHL_HEADERS=<folder>` (the script headers) and `OPENBLACK_CHL_SOURCES=<folder>` (the original quest
  sources): with `OPENBLACK_GAME_PATH`, `test_chl_roundtrip` also decompiles and compiles `challenge.chl` again with
  the headers' constants, and compiles the sources back into the game's file, byte for byte; each of those tests
  skips without its data.
- `OPENBLACK_CREATURE_SAVES=<folder>`: the saved creature minds `test_creature_mind_file` checks against (none by
  default; [The creature](../creature/overview.md)). `OPENBLACK_BINK_FUZZ_SCALE=<n>` multiplies the rounds of
  `test_video_bink_robustness`, for longer runs by hand.
- The test terrain is generated by `lndtool` in `test/mock/CMakeLists.txt`. A bug that erased the previous points
  of the same block was fixed; the cell under the `test_set_camera_pos` camera is now flat.

### Committing part of a file

To commit only some hunks of a file, do **not** use `git apply --unidiff-zero`: it places the insertions according to
the working tree's lines and not HEAD's, and it broke HEAD twice (f9c0b08c, fixed in 6fde22cf; 09fc3b05, fixed in
6a0b1a4b). Assemble the commit in a temporary index on top of HEAD instead.

### Key bindings

- **The table.** The keys and mouse inputs come from one table in the options screen's order: `input::KeyBinding`,
  `k_DefaultKeyBindings` in `Input/KeyBindings.h`, from raffclar's tree.
- **How the action map uses it.** `input::GameActionMap` reads the table through its pure lookups:
  - `ActionsForKeyDown`: a binding with a held modifier takes the key from the plain one, so Ctrl+S quick saves and
    does not also show villager details. Either side's Ctrl, Shift or Alt counts.
  - `ActionsForKey`: letting go of a key ends all of its actions.
  - `ActionsForMouse`.
- **Rebinding.** `GetKeyBindings`, `SetKeyBinding` and `ResetKeyBindings` rebind keys in memory only. Nothing is saved,
  and the options screen does not offer it yet. `QueuePress` presses an action for one frame for the Key Bindings
  debug window.
- **⚠️ approximate A key held while its modifier changes.** A key press clears all of that key's actions before it sets
  the ones that match the modifiers held now. So with Ctrl+S held, letting go of Ctrl while S repeats ends Quick Save
  and starts Show Villager Details. This is raffclar's rule; the original's key path is not read.

### Mouse tests

Scripts that move the real mouse (SetCursorPos, mouse_event): **do not launch them if the user is using the PC**. To
verify without the mouse: captures with `OPENBLACK_CAMERA_FLY` and the test hooks.

### Debug windows

Every feature or fix that can be reached in play gets a debug control: a trigger (do it now), a readout (show the
state) or a switch (turn it on or off). The control goes in the debug window listed below for its area, in the same
series of commits as the feature. Windows are closed by default, and a closed window reads and writes nothing, so the
verification checks (state hash, random trace, screenshots, hand demos) stay identical. Changes under `src/Debug` are
reviewed by the owner of the debug tools area.

All windows derive from `debug::gui::Window` (`src/Debug/Window.h`), are created in `DebugGuiInterface::Create`
(`src/Debug/Gui.cpp`) and are listed, in that order, under **Debug > Windows** in the debug menu bar
(`Gui::ShowMenu`). A new window is one more `emplace_back` there; it then appears in the menu by its title.

The debug menu bar shows only while the game's own menu (Escape) is open, and always when the game runs without its
interface (`debug::gui::MenuBarShown`). The system's pointer stays hidden: the game draws the hand, and its menu its
own pointer.

#### The debug menu bar

| Menu | Controls |
|---|---|
| Load Island | Story Islands, Playground Islands: load any level; "Take your creature along" (on by default: as the game's land change, the creature is saved to the profile and the old land's scripts are stopped; a story land then starts its control script, which brings the creature back as its story does, and a playground makes the creature at the first player's temple, knowing every leash and fully grown; off: only the island is loaded); Creature Testbed (the flat testbed land, when it can be loaded) |
| World | Time of Day slider; visual time, sky type and sky dome readouts; what the creature may look at in the sky; the entities made since the land started; Sky alignment; Detail level (original); Towns (each town's owner and the player the scripts read for it, its belief in player one and in the neutral player, "Take for the local player", the players its centre's spell icons are drawn for); Missionaries' boat (Launch it, Set it sailing; the hull's place and the deck's colour) |
| Debug > Editor (F2) | Opens the Editor |
| Debug > Windows | Opens any window in the table below |
| Debug > Villager Names | Show, Show States, Debug: the villager overlays drawn over each villager |
| Debug > View | Game Detail Overlay (camera, mouse, hand, buffers, turn); Frame counter (`EngineConfig::showFrameCounter`, on by default, the 🎯 chosen difference of 2026-10-23: "<fps> FPS  <ms> ms" in the top-right corner every frame, bar or no bar; never drawn in a check run, that is with `OPENBLACK_IGNORE_REAL_INPUT=1`, `--testbed` or a scenario, or `-n`, so the reference shots are unchanged); draw Sky, Water, Island, Entities, Sprites; Object Tail Unplaced, Sunk Physics Objects, Flying Blended Objects, Creature Reflection, Graves, Totem Copy Takes Solid Light, Falling Creature; Wireframe, Bounding Boxes, Footpaths, Streams; VSync (waits for the screen's refresh; starts as the command line left it, unchecked with `--no-vsync`, and switches at once); Multisampling (MSAA x4) (off, as the original draws: the 🎯 chosen difference of 2026-10-21); Renderer Debug (F11) |
| Debug > Hand | Right Handed; "Hand demo: whole-screen cursor (old)" and "Hand demo: no click retarget (old)", both off: a hand demo plays as the original's ([Hand demos](#hand-demos)) |
| Debug > Input Recording | The frame; Record to `input_recording.obin`, Stop and write the recording; Play `input_recording.obin`, Stop playing (also `--record-input` / `--replay-input`; [Recording and replaying the input](../engine/input-recording.md)) |
| Debug > Start-up and Loading | Loading screens (starts as the command line left it, unchecked with `--skip-loading-screens`; the next land load reads it); Logos at start-up (a readout: whether this game played the logos, which `--skip-logos` leaves out) |
| Debug > Tattoo Editor | Opens the tattoo editor on the player's creature, as clicking it in the Creature Cave does |
| Debug > Fix Vanilla Bugs | Off by default (`EngineConfig::fixVanillaBugs`): the ported fixes of the original's game-logic bugs run only with it on, so the checks always see the original ([deviations](deviations.md#kept-as-vanilla-his-fix-beside-it)) |
| Debug > Field of View | Aspect ratio readout, Reset, Field of View slider |
| Debug > Game Speed | Turn duration readout; Slow, Normal, Fast; Multiplier; Indicator (the game speed's words near the top of the screen: their state, fade, depth and phase) and whether a script owns the speed |
| Debug > Players | Only with the player profiles: who is playing, the creature file, the profile count, whether the profiles are kept (not in a check run) and their folders; Show the new player's box |
| Debug > Mods | Only when `ModLoader.dll` is loaded: loader version, the result of reading the mods folder, and each mod's name, version, folder and switch |
| Capture | Capture a screenshot to the given file name |
| Quit (Esc) | Quits; the bar also shows the frame rate |

Keys outside the bar: F11 toggles the renderer's debug text (also the debug bar's View > Renderer Debug; F1 is the game's Help key), P pauses the game clock, F switches the window to full screen.

#### Windows

All open from **Debug > Windows > *title***.

| Window | Files | Controls | Area it serves |
|---|---|---|---|
| Profiler | `Profiler` | CPU and GPU timings per pass, bgfx counts; draw Sky, Water, Island, Entities, Sprites | Rendering, performance |
| MeshPack Viewer | `MeshViewer` | Browse meshes, submeshes, animations and frames; flag filter; bounding box, armature; Spawn at a location | Assets (meshes, animations) |
| Texture Viewer | `TextureViewer` | Browse loaded textures with size and format | Assets (textures) |
| Console | `Console` | Runs `help`, `history`, `clear` and the land script (`.txt`) commands; Copy, Clear | Land scripts |
| Land Island | `LandIsland` | Block and country counts, resolution; Bump, Small Bump; Dump Heightmap, Dump Textures; land textures; Object draw list (closed by default): Draw only what the list draws switch (`EngineConfig::drawFollowsList`), entries against the 2999 cap, full or still pass, last rebuild turn against the turn, rebuilds asked for, objects drawn by the last pass, and the objects left out of the last frame (entity, kind, not in the list or listed and not drawn) | Land, terrain, object draw list |
| LHVM Viewer | `LHVMViewer` | Read-only: scripts (code, locals), variables, data, tasks (locals, stack, exception handlers); the one control is a Start button for the selected script (one without parameters), which starts it as a new task, so a quest the land has not reached (the Lost Brother's `LostBrother`) can be run | Challenge scripts (VM state) |
| Path Finding | `PathFinding` | Select a villager; Teleport, Move To Point, Move On Footpath with speed; Execute | Villager movement |
| Audio Player | `Audio` | Switches Game sounds, Game music; Sound tab (banks, Play, Stop, Sample volume); Music; Channels (sample, voice, music channels); Atmos (loops, loose samples, climates) | Audio |
| Music | `Music` | Music type; Play, Play (sync, fade), Stop (cut), Stop (fade); Music volume; script and alignment music readouts | Music |
| Audio banks | `AudioBanks` | Every `.sad` bank's samples; play, Stop, Decode all with timings | Audio decoders |
| Citadel | `Temple` | Player; Alignment readout, Evil to good slider; Snap the outside now; Rooms opened readout and Open the rooms button; Room help (the help script of the room the player is in, whether the help system is on and whether the temple holds its help back); Leash posts (closed by default): Draw the posts switch (the drawing only), the player's temple's three posts (point, spin, smoke cell), which show now, which the hand feels now (radius 1 at each post's point now), the leash picked there, Pick Aggression / Learning / Compassion (the temple's pick alone), Tap Aggression / Learning / Compassion (the hand's tap as the local interface: the click, the pick, the leash's packet for the next turn) | Temple, temple leash posts |
| Vortices | `Vortices` | Create a vortex now, under the hand or at the next click; list with Fade out, Delete; the way to the next land | Land-to-land travel |
| Artefacts | `Artefacts` | The artefacts on the land: where each belongs, its owner, its worth, how it shows, its dance and whether its aura is drawn; how many each player owns; give one a little more worth (to see its glow and symbol); hide the aura (off by default); whether each town weighs a football pitch before its artefacts | Worship (artefacts) |
| Disciples | `Disciples` | The disciples with their work and the work they had when the hand picked them up; for the villager in the hand, the things round it in the order it looks at them, and make it a trader (of a town from the list), a craftsman, a missionary or a breeder; the towns' missionaries; each breeder's state, spot, partner and pregnancy; the villagers making love out of doors; draw or hide their glows and symbol | Villagers (disciples) |
| Animals | `Animals`, `AnimalsModel` | Read-only, for the animal nearest the hand (or the one kept with "Keep this animal", off by default): its species and place; its top, final and previous states with the state table's names and the turns since the state changed; its heading, speed, way of moving and goal; whether it is in a reaction's state, the reaction it takes (type, initiator, available or not), what it reacts to, and how long it has held that reaction with whether it may switch to one of the same type; its prey (reachable or not), where it saw prey and when the chase began, its food; its hunger, sleep, breed and eat counters and sleep cell; its flock (members, leader, calm, the leader's turns) and the flock's domain, which is a predator's lair. Closed, it reads nothing; open, it writes nothing | Animals ([the animals](../beings/animals.md)) |
| Computer hands | `ComputerHands` | For each player with a computer hand: its mind on, paused or started, whether a script holds the hand and gave it steps, where it is and goes, its speeds, its steps and the first one's stage, what it holds, the actions queued; each computer player's mind (its plan, rests, attitudes, and every node's weight, suppression, worth and choice); "Hold this mind" (off by default) | Computer players ([hands](../hand/computer-hands.md), [minds](../hand/computer-minds.md)) |
| Creature Spawner | `CreatureSpawner*` | Spawn (species, owner, size, strength, alignment, fatness); Creatures list; Selected tabs: actions and gestures, body and needs, fight, hands, leash, learning mind, mind files, appearance (tattoos, wounds), sounds, footprints; Pause mind, Leash works, Creatures faint, Mute creatures | Creature, leash |
| Testbed Scenarios | `TestbedScenarios*` (closed until the testbed is loaded) | Pick a facet and a scenario; Run, Restart, Stop, Empty testbed; Benchmark; Time; Camera; its creatures; Fixtures ([The Testbed Scenarios window](scenarios.md#the-testbed-scenarios-window)) | Testbed scenarios |
| Consciences | `Consciences` | Who speaks; Appear, Vanish, Home, Eject; Say a text or voice sample; Clip speed; message sets with Run | Advisor spirits |
| Scripts | `Scripts`, `Editor/Panels/ScriptsPanel` | Challenge script source, disassembly, globals, natives, handlers; Compile; breakpoints, Step, Next, Hold, Continue; start or stop tasks; edit values | Challenge scripts (debugger) |
| Camera | `Camera` | Camera paths: Run, Stop, Pause, Resume; points and duration | Camera |
| Weather | `Weather` | Climates, Climates breed storms, Lightning; made-to-measure storm (cloud, rain, temperature, wind, height, length); Force this storm, Clear every storm, End | Weather |
| Gestures | `Gestures` | Last recognised gesture, matches, what is awaited; Draw it, Forget the path; draw the hand's path | Hand gestures |
| Key Bindings | `KeyBindingsWindow` | Read-only: every action's key, mouse button and hold, and the picture the help shows for it | Input |
| File Dialog | `FileDialogWindow` | Open or Save; Debug browser, Platform dialog | Tools |
| Magic | `Magic`, `MiraclesCaster` | Running tab: cast any miracle (one-shot orb, in the hand, dispenser; where the camera looks, under the hand, at the next click); seeds, Charge; infinite prayer power; running miracles, dispensers, creatures' spells, villagers' reactions; Miracle tab: one magic type's `info.dat` values; All miracles table; Particles tab: Spawn any effect at the camera focus or the hand with height, magnitude and draw path; Close all, Delete all | Magic (miracles, worship), magic data, particles |
| Editor | `Editor/EditorWindow` (also Debug > Editor, F2) | Toolbar (Play or Pause, Step, speed, Snap, View, the camera's move speed, the panels shown, Close); Outliner and Inspector panels; Palette, Scenarios, Scripts and Log tabs | Level editing, entities |
| Advisors | `Advisors` | The two advisors: Both out, appear, home, vanish; cling at the bottom or the sides; each one's state, hover, alpha, emotion and position; Out, Appear, Home, Vanish; fly, cling or point at a place on the screen; point or look at the camera's focus; play an anim at a speed; set an emotion | Advisor spirits |
| Physics | `Physics` | Bodies moving and resting; drop a static object (type, height, scale) at the camera's focus, or an object already in the world, and throw it with a velocity | Physics |
| Moon | `Moon` | The moon entity's phase (picture, name, day of the moon month), the date it is taken from and what GET_MOON_PERCENTAGE tells the scripts; where it stands at the sky's hour; scrub the script hour, stop the clock; override the date (day slider, a day back or on, New, First quarter, Full, Last quarter); Reset to the game's values | Weather and sky (the moon) |
| Video | `VideoViewer` (only with `OPENBLACK_BINK_DECODER`) | Play any `.bik`; Restart, Step; frame and timing readouts; switch Game films | Video |

Other ImGui windows: the villager overlays and the Game Details Overlay (`Gui.cpp`, switched from Debug > Villager
Names and Debug > View), and the Creature Cave screen (`src/Gui/CreatureCaveScreen.cpp`), which is a game screen, not
a debug window.

#### Where new controls go

- **Camera:** Camera (paths), Debug > Field of View, Game Details Overlay (readouts).
- **Creature:** Creature Spawner, in the Selected tab of its subject (`CreatureSpawner<Subject>.cpp`).
- **Leash:** Creature Spawner, leash section (`CreatureSpawnerLeash.cpp`). Its mind line reads out the desire a leash
  or a village has made dominant and its seconds left; the creature window's mind section sets and clears one
  ("Timed dominant desire", closed by default). "Kept area" (closed by default) reads out the point the creature is
  kept near this turn, the radius, its distance, the walk back's speed above walking, arrival and restart distance, and
  where it is walking back to; "Keep at home" and "Let roam" set and clear a home radius to try it.
  "Tie with the hand" (closed by default) reads out what the hand's double click and its tap with the leash in
  the hand would do (tie, untie, tap or nothing) to the thing last clicked and to the nearest thing. The nearest is
  searched within 400 of the creature and among plain things only, never a leash post, a spell icon, a script
  highlight or a seed: a debug stand-in for the hand's own pick, the nearest thing within 5 of where the hand acts.
  "Tie to nearest (packet)" and "Untie (packet)" send the hand's tie packet, carried out at the next turn as the
  hand's would be (the hand does not send it yet).
- **Hand:** Gestures for gestures; Debug > Hand for hand switches (see gaps).
- **Magic and worship:** Magic (casting, prayer power and reactions on its Running tab; `info.dat` values);
  Artefacts for the artefacts.
- **Particles:** Magic, Particles tab.
- **Weather and sky:** Weather; Moon for the moon; World menu for time of day and sky.
- **Town and villagers:** Path Finding, Disciples, Debug > Villager Names, the World menu's Towns (see gaps).
- **Computer players:** Computer hands.
- **Objects and physics:** Physics.
- **Testbed:** Testbed Scenarios (also the editor's Scenarios tab).
- **Audio:** Audio Player (game sounds), Music (music), Audio banks (decoders).
- **Video:** Video.
- **Temple:** Citadel.
- **Scripts:** Scripts (challenge scripts), Console (land scripts), LHVM Viewer (read-only, but for its Start button).
- **Advisors:** Consciences, Advisors.
- **Land and travel:** Land Island, Vortices, Load Island menu.
- **Rendering:** Debug > View, Profiler; the object draw list in Land Island.
- **Assets:** MeshPack Viewer, Texture Viewer.
- **Game flow:** Debug > Game Speed.

#### Gaps

No window exists yet for these areas; the first lane that needs one adds it to `Gui.cpp` and to this page:

- **Hand:** picking up, holding and throwing (only the Hand menu's switches and the Game Details hand position
  exist).
- **Town, villagers and buildings:** jobs, needs, town state, building construction (only the villager overlays, Path
  Finding, Disciples and the World menu's Towns).
- **Animals.**
- **Interface and in-game screens:** menus, help texts, the land's interface states.
- **Players:** belief, alignment and influence outside the temple (only the World menu's Towns reads a town's belief
  in player one and in the neutral player).

### Test hooks

This page is the cross-page list: every variable is in [Debug environment variables](#debug-environment-variables),
described there or linked to the page that describes it
([On the pages of their systems](#on-the-pages-of-their-systems)), and each page keeps its own under its "Test hooks"
heading. The testbed's land timelines and their commands are in
[Testbed scenarios](scenarios.md#test-hooks).

## Pending

- The fork's open issues for debug controls: #190 (the footpath tab sends the selected villager along the footpath),
  #191 (the console shows the game's log), #192 (the villagers' overlay colours each villager by its owner), #194
  (controls for the scroll's activation click and the falling snow), #97 (the creature spawner window hosted by the
  editor) and #99 (the editor places, moves and inspects creatures; blocked). The areas with no window yet are under
  [Gaps](#gaps).
