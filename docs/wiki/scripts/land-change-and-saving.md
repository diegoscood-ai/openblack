# Land changes, skipping the tutorial and saving

## Summary

Three moments when the scripts touch the whole land. At a new game the original always asks, in a requester (the
SkipBox), how much of the tutorial to skip, and the challenge script reads the answer. When a land is finished the
script calls `LOAD_MAP`: the old land is cleared (the player's creature saved to its file first), the scripts and
their variables are kept, and the new land's script runs inside the same call. And the original can write any object
back as the land-script line that makes it again: the vortex carries what it swallowed to the next land that way,
and the land editor saves with the same writers. A saved game does not use them: its world file is a binary stream
that each class writes with its own binary save ([Saved games](../engine/saved-games.md)).

openblack shows the SkipBox and sets the same answer bits, changes land inside the script's call as the original
does, and has the line writer and the writers of the physics classes, the dead tree, the one-shot spell seed, the
piece knocked off a building (as a rock) and the scaffold, which the vortex uses to write `vortex.txt` and the next
land's way in reads back. The reward chest's and the ball's writers (undefined in the original) are not ported. The vortex's own round trip of `vortex.txt` is on
[Vortexes](../world/vortex.md#the-land-script-round-trip-of-vortextxt).

## The original

**Faithful**, read in the v1.42 `runblack.exe` (the 1.20 symbols, and the patch's own bytes where it changed code),
except what is marked.

<a id="skipping-the-tutorial-skipbox-and-can_skip_tutorial"></a>
### Skipping the tutorial (SkipBox and CAN_SKIP_TUTORIAL)

#### The requester

- `GGame::OnNewGame` 0x553900: if there is a script and it is Land 1 it starts `LandControlAll` and, always, calls
  `DoYesNoSkipTutorialRequestersIfNecessary` 0x54CBD0 (0x55395B). This clears bits 23, 24 and 25 of g_game +0x14
  (0x54CBD5 / 0x54CBE1 / 0x54CBED); the v1.42 patch nulls out the test that came before showing the box with 25 NOPs
  (0x54CBF4..0x54CC0C), so it **always** does `PauseGame(1)` and `SkipBox::Show` (vt +0x0C,
  `DialogBoxBase::Show` 0x5135F0) on the SkipBox made by `FrontEnd::Init` (0x53B844, the pointer at 0xCD0634, the
  callback 0x544480 set at 0x53BB9D).
- `SkipBox::Init` 0x5441C0: a title, a button (id 0xB, text 0xA24 from `HelpTextDatabase`) and four checkboxes (ids
  0x3C..0x3F) with the texts 5..8 of the text table at 0xD17CA0 (not read); the selected one is at +0x20, 0 by default
  (0x544206; also set to 0 at 0x5442C2). `SkipBox::CanESCOut` 0x53BD60 returns 0: it does not close with Esc.
- The callback 0x544480: when button 0xB is pressed, by the checkbox (jump table 0x5445A0): 0 clears 23, 24 and 25;
  1 sets 23 and clears 24 and 25; 2 sets 23 and 24 and clears 25; 3 sets all three. Then `PauseGame(0)` and
  `DialogBoxBase::Hide`.
- The scripts read it with `CAN_SKIP_TUTORIAL` (460, `GScript::CanSkipTutorial` 0x6FFEF0, bit 23),
  `CAN_SKIP_CREATURE_TRAINING` (461, 0x6FFF10, bit 24) and `IS_KEEPING_OLD_CREATURE` (462, 0x6FFF30, bit 25), which
  push the bit as a boolean (VMType 6). Nothing else writes those bits (no `or` / `and` with 0x800000, 0x1000000 or
  0x2000000 in the whole `.text`).

#### What the script does with the answer

Read in the decompiled listing of `challenge.chl` (pseudo-assembly: the natives appear as `CALL <n>`; extents:
`SetupLand1` 25357-25622, `LandControl1` 74956-75239, `LandControlAll` 171106-171139, `CreaturesInGlade`
43862-46096, `CreatureDevSeeHome` 6932-7814, `FollowUs` 49528-53196):

- Only `SetupLand1` calls them (25398-25455): `CAN_SKIP_TUTORIAL` → the global `IsSkippingToCreatureSelect` = 1;
  `CAN_SKIP_CREATURE_TRAINING` → `IsSkippingCreatureGuide` = 1; `IsKeepingOldCreature` =
  `IS_KEEPING_OLD_CREATURE and CURRENT_PROFILE_HAS_CREATURE` (25432-25435) and, when it is set, it **forces** the other
  two to 1 (25440-25447). Neither `SetupLand1` nor `LandControlAll` has music, camera, dialogue or fade before
  `LandControl1` (the `SET_FADE_IN(3.0)` at listing line 171120, ip 155496, comes after
  `LOAD_MAP("scripts/land3.txt")`: it is the change to Land 3, not Land 2).
- `LandControl1`: with `IsSkippingToCreatureSelect` neither `FollowUs` (74982, the intro), `CitadelGuide` (74983) nor
  `ChooseYourCreature` (75037) runs. Instead it does `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)`, takes the created
  object (the citadel, 🔍 inferred from the type) with `CALL_NEAR(18, 5000, that point, 5)` and sets its
  `BUILT_PERCENTAGE` (`SET_PROPERTY(22, citadel, 1.0)`, 74988-75017), and deletes the three gate stones `GateKey1`,
  `GateKey2` and `QuarryRock` (75042-75083), which `ChooseYourCreature` would have made.
- `CreaturesInGlade` (75088), which takes the camera to choose a creature (and, when skipping, skips the guardian and
  does a 2 s `SET_FADE_IN` after 2 s), is skipped only with `IsKeepingOldCreature`: then the block 75093-75125 runs
  (`SET_GAME_TIME(15.4)`, `GAME_TIME_ON_OFF(1)`, `SET_CAMERA_ZONE(3436)`, `SET_OPEN_CLOSE` of the creature gates,
  `ChooseYourCreatureFinished` = 1, `LOAD_MY_CREATURE(1850, 1300)`).
- With `IsSkippingCreatureGuide` none of `MoveTheGuideAround`, `CreatureDevLearnToEat`, `CreatureDevPunishment`,
  `CreatureDevLeashIntro`, `CreatureDevLeashAttachToHouse`, `MeetTheGuide`, `GuideAsksToMeetYourCreature`,
  `GuideImpressTown*`, `CreatureDevGuideTeachesFight` or `TheStorm` runs (it waits for `LeaveLandNow`).
- **`CreatureDevSeeHome` (75141) always runs**, with every answer. Before anything, unconditionally,
  `SET_GAME_TIME(4.59)` + `GAME_TIME_ON_OFF(1)` (7000-7003): dawn breaks. With `IsSkippingCreatureGuide` it takes the
  branch 7056-7085: `loop { START_CAMERA_CONTROL }`, `loop { START_DIALOGUE }`, `START_GAME_SPEED`,
  `SET_WIDESCREEN(1)`, `SET_CAMERA_POSITION(1891.039, 31.693, 2520.674)`, `SET_CAMERA_FOCUS(1899.053, 30.312,
  2518.680)`, `SET_WIDESCREEN(0)`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`, `SET_FADE_IN(2.0)`, with no
  `SLEEP` between, so the lock lasts one turn and the camera **snaps** (does not glide) onto the village.
- **The player's creature on the skip path** (challenge.chl instruction pointers, read in the bytecode): `LandControl1`
  loads it at ip 68025 (`LOAD_MY_CREATURE(1850, 0, 1300)`, turn 7) and runs `CreatureDevSeeHome` at ip 68037. There,
  after the dawn and an asynchronous did-you-know, `MyCreature = CALL_PLAYER_CREATURE(1)`; the skip jumps over
  `SET_CREATURE_DEV_STAGE(0)` (ip 6043) to the camera block (6053-6080, above), then `SET_POSITION(MyCreature,
  HomePos)`, `SET_CREATURE_HOME(MyCreature, HomePos)` and `SET_FOCUS(MyCreature, GET_CAMERA_POSITION)` (turn 8), then
  `SET_FADE_IN(2)` and a looped `SLEEP 2` until the script ends. HomePos is the script's local marker made at ip 5951
  at (1896.5, 29.48, 2520.06). At turn 30, `LandControl1` ip 68047-68063: `MyCreature = CALL_PLAYER_CREATURE(1)`,
  `SET_CREATURE_DEV_STAGE(MyCreature, 13)`, `DEV_FUNCTION(2)` (the rope leash), `DEV_FUNCTION(3)` (the other leashes),
  `CREATURE_IN_DEV_SCRIPT(false, MyCreature)`, `RELEASE_FROM_SCRIPT(MyCreature)`. What each native does:
  [The player's creature](../creature/players-creature.md#the-players-creature).
- `CreaturesInGlade` (answers 2 and 3, not 4) holds the start: `START_CAMERA_CONTROL` 44028, `START_DIALOGUE` 44031,
  `SET_WIDESCREEN(1)` 44035, `SET_FADE(0,0,0,2.0)` 44295 (the skip branch), `SET_FADE_IN(2.0)` 44326,
  `SET_CAMERA_POSITION/FOCUS` 44303/44310 (1753.3, 49.5, 2811.1), `MOVE_CAMERA_*` 44339-44403, `HAS_CAMERA_ARRIVED`
  44404/44415/44422, `START_MUSIC(63)` 44419, `RUN_CAMERA_PATH(13)` 44421, and it lets go only at 44684-44691.
- `START_MUSIC(54)`, the intro music, appears **only once in the whole challenge.chl**: line 50114, inside
  `FollowUs`. With any skip answer it never plays. The others in Land 1: 67 in `ChooseYourCreature` (42493) and in
  `CreatureDevSeeHome` without skipping (7117), 63 and 65 in `CreaturesInGlade` (44419, 45644/45813/45985), 67 and 69
  in `TheStorm`. `SET_AVI_SEQUENCE` (51024), `CAMERA_PROPERTIES` (50444), `SET_FOCUS_AND_POSITION_FOLLOW` (50439) and
  `SET_INTERFACE_INTERACTION` of Land 1 are all inside `FollowUs`: with the skip they never run.
- The 13 background scripts (`SingingStoneCircle`, `ThrowingStones`, `TheLostFlock`, `TheMissionaries`,
  `MagicMushroom`, `HermitMain`, `CreatureSavingPeople`, `PiedPiper`, `CreatureGuardian`, `LeaveThroughVortexL1`…)
  wait on a `ChallengeHighlightNotify` / `QuestHighlightNotify` / `SingingStonesNotify` before touching the camera, so
  none gets in the way at the start. With `IsSkippingCreatureGuide`, `LeaveThroughVortexL1` opens the exit vortex
  from the start (53845-53876).
- There is no other way in the original to skip the intro: `FollowUs` does not call `KEY_DOWN` or check Esc; the
  click only moves the texts on.

<a id="changing-land-load_map"></a>
### Changing land (LOAD_MAP)

- **The command.** `LOAD_MAP` (native 152, `GScript::LoadMap` 0x6FB320) pops the path, sets [0xBF3BAC] = 1,
  [0xD318C8] = 0 and `LoadingFrameEnabled` = 2, draws the loading frame (`RenderLoadingFrame(1)` 0x5F4E40) and calls
  `GGame::LoadMap(path)` 0x552F40 (the call at 0x6FB36A) inside the command: `ClearMap` 0x552BB0 (0x552F4F), the
  landscape and island released, g_game +0x205A50 = 0, `GSetup::LoadMapFeatures(path)` 0x7180B0 (0x552F6E) and
  `Town::AsssignTownFeature` (0x552F76). The calling script goes on in the same step, on the new land. The other
  callers of `GGame::LoadMap`: 0x54C990, `GGame::StartPlaygroundGame` 0x555990 (at 0x5559A7, the skirmish start from
  `ResetAndStartPlaygroundGame`) and 0x641752. The seeds this resets:
  [The seeds](../engine/random.md#the-seeds).
- **What `ClearMap` keeps, saves and resets**, in its order:
  - it **saves** the local player's creature (0x552C3A..0x552D54,
    [A land change](../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again));
  - the camera exclusions, the physics objects, the hand, the music and the audio;
  - `ValidateScriptVariables` 0x6EB6F0 (0x552DA9; its first part, `fn_006EB680` 0x6EB683..0x6EB6A6, clears the
    script's `HitObject` (+0x70) and `ObjectWhichHit` (+0x74), the last hit's record, when their thing is no longer
    available, `IsAvailable` vt+0x2C, `GameThing::IsAvailable` 0x401810; bw1-decomp `Script.h`) [V], then `GlobalGameLists::ClearMap` 0x591520 (0x552DBF), which first
    deletes every creature (the creature list g_game +0x205BBC, `ToBeDeleted(0)` at 0x59154E): the creature's
    destructor `fn_00475170` clears its player's creature (+0xA4C, 0x475279), so after a land change **the player has
    no creature** and the next land's `LOAD_MY_CREATURE` makes it again from the file just saved;
  - the players are kept (`GPlayer::OnEndOfClearMap` 0x64CD00, from 0x552DE1: nine float pairs at +0x68 / +0xB8 back
    to 1.0), and so are their alignment and the scripts, which are not restarted (`ValidateScriptVariables` again at
    0x552E0E);
  - `GGame::ResetState` 0x5557A0 (0x552E62; `GData::Reset` 0x510750: the turn and both seeds) and
    `GameStats::ClearAll`.
- **Land 1's way out.** `LandControl1` runs `LeaveThroughVortexL1` at ip 68105 and, on the skip path, waits on
  `LeaveLandNow` (68114-68117). With `IsSkippingCreatureGuide` the script (ip 48110-48809) jumps to 48701:
  `VortexOpen = 1`, the vortex (`CREATE` 23 at 48713) and its highlight (`CREATE_HIGHLIGHT` 48721, property 24 = 20),
  then `RUN QuestHighlightNotify(highlight, vortex, 2, 3401, 3, 4460, 30 × 60)` (48745). **That script loops until the
  player clicks the highlight or the vortex** (`GAME_THING_CLICKED` of either, its exception handler at ip 1989-1994),
  giving a reminder every 1800 s of script time (ip 1936-1988). Only then the cinematic: the camera taken, a 6 s move
  above the vortex and `HAS_CAMERA_ARRIVED` (48771-48777), an 8 s dive (48781) with `SLEEP 5` (48783),
  `SET_FADE(0, 0, 0, 2)` and `SLEEP 2` (48789-48791), `HAS_CAMERA_ARRIVED` (48793),
  `STOP_SCRIPTS_IN_FILES("CreatureGuide.txt")` and `LeaveLandNow = 1` (48796-48800). `LandControlAll`
  (155483-155513) then calls `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING("LandControlAll.txt")`,
  `LOAD_MAP("scripts/land2.txt")` and `RUN LandControl2`; the later lands the same (only before Land 3 a
  `SET_FADE_IN(3.0)`). No hand or creature has to go into the vortex and no film plays, but the click is needed: the
  land changes at the earliest about 14 s (140 turns) after it.
- **STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING** (154, `GScript::StopScriptsExcludingFiles` 0x6F0C00) keeps the list at GScript +0x2C and stops every task whose
  source file is not in it (`StopScripts` with the filter 0x6F0A60): the names are split with `strtok` on space, comma
  and tab (0xC0C710) and compared with `_stricmp`.

<a id="what-a-land-change-keeps"></a>
#### What a land change keeps (and what only a script restart resets)

Read in the exe (2026-10-22, issue #206). `LOAD_MAP` resets only what `ClearMap` resets: it never restarts the scripts.

- **Who restarts the scripts.** `GScript::Reset` 0x6EB2D0 is called by `GGame::Init` (0x54F53A, a new game, before the
  map script), `GGame::StartPlaygroundGame` 0x555990 (0x55599B, then `GGame::LoadMap`), `GameOSFile::LoadAllGame`
  (0x558A62, a saved game), `GGame::Close` (0x54ECEE), `GScript::LoadBinary` (0x6EB618, the program read), the script
  library's creation (`fn_006EB2B0` 0x6EB2BC), the land script's script-load command (`fn_006EB220` 0x6EB242),
  `Config::ProcessOneGameTurn` (0x46B4AD) and `sub_641570` (0x6415F5). It calls `CleanGameForScriptReboot` 0x6EBBC0
  (0x6EB330, with a help system) and `HelpSystem::Reset` 0x5C5580 (0x6EB340), then sends both spirits home
  (`SpiritHome` 0x5C6670 with 2 and 1, 0x6EB354 / 0x6EB367).
- **The script-reboot key.** `CleanGameForScriptReboot`'s other caller is `GGame::ProcessTurn` (0x54E8D3), when
  `ScriptRebootRequested` [0xD019A9] is set: `DeleteAllScriptCreatedGameThings`, `StopAllTasks`, the clean, `StartScript
  ("KeepRunningControl")`, `TutorialState` = 0 and `SetInterfaceInteraction(0)` (0x54E8FA); the flag back to 0 at
  0x54E902. Only `GGame::ProcessKey` sets it (0x63F089): Escape (key 1) with g_game +0x205A10 == 0 (otherwise a
  requester), in a single-player game, in the game state +0x205A08 == 6, unless the help system's +0x45E8 and +0x45EC
  are both set, with `fn_006EBB50` true and [0xFA51DB] clear; after `SetupScreenFadeBackToNormal(0)`, only while
  `TutorialState` [0xD019A4] is not 0 (otherwise a text and `PauseGame(1)`). `InitStaticsValues` clears it
  (0x54A80D). openblack has no such key (Escape opens its menu), so nothing of it is ported.
- **Neither is reached from `LOAD_MAP`**: `GScript::LoadMap` 0x6FB320 calls only `RenderLoadingFrame` and
  `GGame::LoadMap` 0x552F40, which calls `ClearMap` 0x552BB0, releases the landscape and the island, puts the interact
  desire to 0 and reads the map script. What the script restart resets, and what a land change does with it:

| State | The script restart | A land change (`ClearMap`) |
|---|---|---|
| The help system: its texts, the message sets' sent turns and counts, the category turns, the banter count, the wide screen (the cinema bars), the icons, the text history, the help switch +0x45F8 and the speaker override | `HelpSystem::Reset` 0x5C5580 (callers: `HelpSystem::SetToZero` 0x5C54D5 and `GScript::Reset` 0x6EB340 only) | **kept** |
| The bars' fraction [0xEB9950] | 0 (0x6EBBCF), and `HelpSystem::Reset` then snaps the bars (`fn_005C6C40` at 0x5C55DD: the slide's timer −FLT_MAX after its `SetWideScreen(0, 0)`), so the help system's next frame (`fn_005C6BB0`, from its timer +0x45F0 through `GetWideScreenPercentage` 0x5C6B60, at 0x5C6C00) writes 0 again: the bars go at once, no slide ([screen fade and cinema bars](../interface/screen-and-readouts.md#screen-fade-and-cinema-bars)) | not written; the help system's frame writes it again from its own timer every frame, so it follows the help system's wide screen, which a land change keeps |
| The advisors (both spirits) | sent home right after `HelpSystem::Reset`, inside the same help-system test: `SpiritHome(2, 0)` (0x6EB354, the evil one first), then `SpiritHome(1, 0)` (0x6EB367); arg 0, so each flies off (`fn_005C3590`), as a non-Help script's release does | **kept** where they are: `SpiritHome` 0x5C6670's callers are `ClearDialogueControl`, `SaveTextOnTempleEntry`, `ResetFOV`, `GScript::Reset` and the spirit natives (`SPIRIT_HOME`, `SPIRIT_DISAPPEAR`, `START_` / `END_SPIRIT_CONTROL`); none is reached from `ClearMap` or `GGame::LoadMap` |
| The script fade: its step GScript +0xB0, its alpha +0xB4 and its colour +0xB8..+0xBB (the alpha byte at +0xBB), which `ProcessFade` 0x6EB9D0 copies to the drawn colour [0xFA51D8] every call | all 0: the colour's four bytes at 0x6EB3D2..0x6EB3E4, the step and the alpha at 0x6EB433 / 0x6EB439, at the end of `GScript::Reset`, after the help system's reset and the spirits | **kept**: `SetupScreenFadeTo` 0x6EBA90 is called only by `OnNewGame` (0x55397F) and `SET_FADE` (0x6FCDF6), `SetupScreenFadeBackToNormal` 0x6EBB00 only by `GGame::ProcessKey` (0x63F072), `PSysGlobal::StartAVISequence` (0x68F471, 0x68F4E9) and `SET_FADE_IN` (0x6FCE2B); none from `ClearMap` or `GGame::LoadMap` |
| The close clipping [0xD1A2F8] | 0 (0x6EBBEF) | **kept** (its only other writer is `SET_GRAPHICS_CLIPPING`, 0x708C77) |
| The camera's modes (a script mode, a dual mode) | `ClearCameraStack` 0x442FE0 (0x6EBCD8) | **reset** the same way: `ClearCameraStack` at 0x552C12 deletes every mode and makes a new `CameraModeNew3` |
| The camera's field of view (GCamera +0x1A8) | not written | **kept**: `SetCameraFov` 0x443680 is called only by the GCamera constructor (0x441A83, 70 degrees at once), `START_CAMERA_CONTROL` (0x6ECE48), `SET_CAMERA_LENS`, `RESET_CAMERA_LENS`, `SET_CAMERA_POS_FOC_LENS` and `MOVE_CAMERA_POS_FOC_LENS`; a land change keeps the camera (only `GGame::Init`'s new game deletes it and makes it again) |
| The interface interaction: the level GScript +0xA4, the interface's limit bits (GInterface +0x28), the hand's reach (CHand +0x4838), the ControlMap switches (+0x652C / +0x6530) | `SetInterfaceInteraction(0)` 0x70B220 (0x6EBD10), and the same fields written before it (0x6EBC77 .. 0x6EBCFE) | **kept**: `SetInterfaceInteraction`'s callers are only the native, the reboot, `GScript::Load` and a requester's answer (`sub_54C9F0` 0x54CA04); `CHand::OnClearMap` 0x46E8C0 does not write the reach |
| The camera features and the auto-pitch (`CameraHelp`) | `EnableCameraFeatures(0x1BF, -1)` (0x6EBCAE), then the level's | **reset** by `ClearMap` itself: `EnabledFeatures` = 0x1BF (0x552E74), `AutoPitchParam2` = 75.0 (0x552E7E), `AutoPitchParam1` = 0x3F060A92, 0.5235988 (0x552E88) |
| The script's switches: draw the leashes +0x78 (1), +0x7C (0), draw the highlights +0x80 (1), the creatures' sounds +0x84 (1), the dialogue-only sound +0x90 (0), the alignment music +0x94 (1), the music line +0x98 (0), the beats +0x9C (0), the citadel interface +0xA0 (1) | `GScript::Reset`'s own writes (0x6EB2F4 .. 0x6EB318, 0x6EB403 / 0x6EB409); +0x90 / +0x94 also in the clean (0x6EBD29 / 0x6EBD2F) | **kept** (the GScript is not made again) |
| A hand demo playing | `GInterface::EndPlayBack` 0x5DB3F0 (0x6EBCA2) | **kept** (`EndPlayBack`'s other callers: `StartPlayBack`, the demo's end `fn_005DAEE0` and the task-stop callback) |
| The intro's light, hand and debug camera | `Intro::ReleaseAll` 0x5DFC40 (0x6EBBFB) and g_camera_mode [0xEA9EC8] = 0 (0x6EBC0C) | **kept** (`Intro::ReleaseAll`'s other caller is `PLAY_JC_SPECIAL`'s `ThingJCSpecial` 0x70910E) |
| A sun a script hid (SET_SUN_DRAW, [0xFA2760]) | 0 (0x6EBBF5) | **kept** |
| The bookmarks' keys (`Bookmark::ShowBookmarks` [0x9CD384]) | 1 (0x6EBD04) | **reset** to 1 too: `InitStaticsValues` 0x54A81E, from `ClearMap` (0x552F18) |
| The SuperVillagers | all released (0x6EBBE0) | **reset** the same way (`ClearMap`'s own loop) |
| The challenge room's records | `ChallengeRoom::ChallengeClear` 0x784DE0 (0x6EBC00) | **reset** too, with a temple (0x552DD2) |
| The sound confirmation (START_ANGLE_SOUND / CHL 348) | `GConfirmation::Stop` 0x71A640 (0x6EBD18) | **kept** (`Stop`'s other callers are the two natives) |
| The game speed a script set | `ActualEndGameSpeed` 0x6FAA60 (0x6EBD0A) | kept with its owner (as openblack keeps it) |
| The dialogue control | `HelpSystem::ClearDialogueControl` 0x5C67E0 (0x6EBC27) | kept (as openblack keeps it) |
| The day / night clock: the visual time, its target and step, the scale (`GAME_TIME_ON_OFF`), the cycle and a `MOVE_GAME_TIME` going on | not written: `GLandAlignement::Open` 0x5E1D10, at the new game's `LOAD_LANDSCAPE`, sets noon and the default cycle once per game ([day and night](../world/day-and-night.md#clock)) | **kept**: `Open` does nothing until `GGame::Close`'s `GLandscape::Close` clears its flag [0xD1A264]; `GGame::LoadMap` only releases the landscape (`LH3DLandscape::Release`) |
| The hand's packets: the session's list and the send buffer (g_game +0x5318) | not written | the session's list **emptied** (`GGame::ResetState` 0x5557A0 → `LHSession::EmptyEventQ` 0x5557B9); the buffer **kept** (only the GGame constructor's `ClearVariables` 0x54BF20 and the flush write it) ([the packets](../hand/states.md#the-hands-state-and-packets-in-openblack)) |
| The interface status: the synced hand, its turn motion, the last-sent sync values and countdown, the status's throw | not written (`GInterfaceStatus::SetToZero` 0x5DBA00 runs only from `GGame::Init`'s players' set-up) | **kept**; only `CHand::OnClearMap` 0x46E8C0 (the last released object and its countdown, the held object) and `GInterface::Validate`'s conditional end of the action run ([clearing on a new land](../hand/states.md#clearing-on-a-new-land)) |
| The particles: every running effect and container, the town belief sprites, the explode queues, the fireball and lightning counts, the draw-offset decay | not written | **reset** the same way: `PSysGlobal::OnClearMap` 0x68F820 (0x552DFE) → `fn_0068F870` (0x68F843) → `fn_00672250` on the PSys manager [0xD4E0E8] (ten list clears), `fn_0069B720` (the belief sprites' count 0xD4ED08), `fn_006811D0` / `fn_00681200` (the explode queues), `fn_00692F30` / `fn_00693770` (the fireball and lightning counts), `fn_006A8460` (sprites), `fn_0068F660` (0xD4EBF8 / 0xD4EBFC 0); the ten inner list clears were not compared one by one |
| The magic: the players' magic types held (GPlayer +0x970, 42 dwords), the nine tribal powers +0x68 and the wonders' sums +0xB8, the spells, fires, shields, storms and influence circles | not written | **reset** the same way: `GPlayer::OnEndOfClearMap` 0x64CD00 for each player (0x552DE1: `ClearMagicTypesEnabled` 0x64C200, the floats back to 1.0 at 0x64CD19..0x64CD30), `InfluenceCircle::Reset` 0x826C50 (0x552C21), and the things themselves deleted by `GlobalGameLists::ClearMap` 0x591520 (its thing lists, `LH3DStorm::ReallyKillAll`, `GMap::Clean`); compared in kind, not field by field |
| The script-created things | `DeleteAllScriptCreatedGameThings` 0x70D360 (0x6EB381), then the slots emptied (`fn_0070CEB0`, 0x6EB396) | the things **deleted** (`GlobalGameLists::ClearMap` at 0x591929 calls `DeleteAllScriptCreatedGameThings`), the slots and the variables **kept** ([a variable holding a thing of the land before](#a-variable-holding-a-thing-of-the-land-before)) |
| A film playing | not written | **kept**: `DeleteVideo` 0x54A940 has no caller in `ClearMap` or `GGame::LoadMap` (its callers: the video's own poll and draw, `PlayFullScreenMovie`, `GGame::ToBeDeleted`); a film pauses the game, so no script reaches `LOAD_MAP` during one |
| The sequence mode (g_game +0x205A28: 1 inside the citadel, 2 the falling spell) | not written | **kept**: its only writers are `GGame::Init`, the falling spell's start and end and the citadel's entry and exit ([the game clock](../engine/game-clock.md#pause)) |
| The tooltip's icon (HelpSystem +0x2C) | deleted (`HelpSystem::Reset` → `ResetIcons` 0x5C5610) | **kept** drawn: `InitStaticsValues` puts only the tooltip's globals back; the new land's first tooltip turn (fn_005C9D00) deletes it, or keeps it when the same text comes back ([tooltips](../interface/tooltips.md#resets-and-a-new-land)) |

#### The scripts' restart's other writes

The rest of `GScript::Reset` 0x6EB2D0 and `CleanGameForScriptReboot` 0x6EBBC0 (read 2026-10-22, #206; the GScript
fields by bw1-decomp `Script.h`, after an 8-byte `Base`). None is written by a land change.

| Write | What it is | openblack |
|---|---|---|
| +0x08 = 0 (0x6EB2EB), +0x10 = 0 (0x6EB3EB) | the countdown timer's on (`CountDownTimerEnabled`, which `COUNTDOWN_TIMER_EXISTS` reads) and shown (`CountDownTimerVisible`); its turns left +0x0C stay | ported: `script_countdown::OnScriptsRestart` from `Game::ResetScripts` |
| +0x88 = 0 (0x6EB3FD) | a hand demo's pending trigger: set by the playback (fn_005DAEE0), read and cleared by `HAND_DEMO_TRIGGER` (`ScriptIsHandDemoTrigger`), cleared by `PLAY_HAND_DEMO` | ported: `hand_demo::ConsumeTrigger` from `ResetScripts` (`hand_demo::End` keeps it, as `EndPlayBack` does) |
| the local player's creature +0x10C0 = 0 (0x6EB3A5..0x6EB3CC: player +0xA4C, its creature, when it has one) | `CREATURE_IN_DEV_SCRIPT`'s flag, which `CreatureMental::SaveMind` writes to the mind file; `StartPlaygroundGame` restarts the scripts (0x55599B) before `GGame::LoadMap`'s map clear saves the creature (0x552CE9) | ported: `player_creature::EndMyCreaturesDevScript` in `Game::ChangeLand`, before the save, when the load restarts the scripts |
| [0xD95C04] = 1 (0x6EB2EE) | where `FindFreeScriptGameThingSlot` 0x70CEE0 starts looking for a free slot (its only other writer) | none: openblack's script objects have no slot numbers to search (`ScriptObjectsSystem::Register`) |
| [0xD95C0C] = 0 (0x6EB2DF) | the program-loaded flag (`Dat_00D95C0C`, `Reset` / `LoadBinary`) | goes with the program's reload |
| +0x3C, +0x4C, +0x50, +0x6C = 0 | per-native loop state: +0x3C the native's take-control flag, set before every native from the function table (sub_6EC750, registered by fn_006EB1D0), read by `GetScriptGameThing` and `LoopFnCheck`; +0x4C / +0x50 `ATTACH_TO_ID`'s and `CreateDance`'s; +0x6C the loop callback the `*Loop` natives set and `LoopFnCheck` calls | none needed: written again before each use (openblack's `ScriptObjects::EnterNative` for +0x3C) |
| +0x2C, +0x30 = 0 | the stop natives' file lists (`STOP_SCRIPT`, `STOP_SCRIPTS_IN_FILES`, `STOP_ALL_SCRIPTS_EXCLUDING`, ...: sub_6F09E0 and its callers) | none needed: `chlapi::InScriptFileList` keeps no list |
| `CreditTextItem::ResetClip(0)` 0x50B040 and `Reset(0)` 0x50AC50 of each item of the list [0xCADB68] | the script texts of `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT` (which add to the list) and `FADE_ALL_DRAW_TEXT`: each faded out at once (+0x814..+0x81C 0) | not ported: the three natives are not implemented |
| `fn_006F69A0` on `g_scriptDLL` (0x6EB37C) | ScriptDLL +0x24..+0x30 = 0 and its `Reboot` (+0x6C) with the transport (+0xC): the script engine starts again, every task and global | not ported (Pending) |
| `CleanGameForScriptReboot`: `g_b_we_are_inside_citadel@LH3DSky` [0xFA2760] = 0 (0x6EBBF5) | the same global `SET_SUN_DRAW` writes, despite its name (the row "A sun a script hid" above) | ported (`ecs::sun_draw::ShowAgain`) |
| `CleanGameForScriptReboot`: `CameraExclusion::ResetExclusionFile` 0x455320 (0x6EBCB4) | the camera zone back to none: `RemoveByID`, `UseExclusions` 1, the force field off and its points 0, the two max flags 0, both limits 500.0 | the same as the map's clear, which calls it too (0x552D5F, after `RemoveAll` 0x552D59): openblack makes the camera zones again with every land (`Locator::cameraZoneSystem`) |

#### A variable holding a thing of the land before

The original keeps the challenge and its variables: `GScript::LoadMap` 0x6FB320 calls only `RenderLoadingFrame` and
`GGame::LoadMap`, and the script things' table (0xD967F8, 511 slots of 0x14 bytes from slot 1) is emptied only by
`fn_0070CEB0`, whose one caller is `GScript::Reset` 0x6EB2D0 (the call at 0x6EB396), not reached from `LOAD_MAP`. A variable holds a
slot number; the slot keeps the thing as a `BaseInfo` {+4 the unique id, +8 the heap offset} (`BaseInfo::Set`
0x436BB0 → `Base::SetInfo` 0x436B20). The unique id comes from the object heap's counter
(`UniqueKeyHeap::UseUniqueId` 0x7E1900: +0xBC, then +1), a deleted object's key is set to 0
(`UniqueKeyHeap::Delete` 0x7E18B0), and `BaseInfo::GetBase` 0x436B80 gives the object only while the id at its
address is the same (`IsUniqueIdAtAddressTheSame` 0x7E19F0). So once the land's things are deleted
(`GlobalGameLists::ClearMap`) the variable's slot gives no thing (`GetScriptGameThing` 0x70D220 warns and returns 0),
and never a new land's thing, even one made at the same address. A slot is handed out again only when it has no
reference and an empty `BaseInfo` (`FindFreeScriptGameThingSlot` 0x70CEE0: +0x10, +4 and +8 all 0), so a slot a
variable still holds is never given to a new thing; the slots without references are freed by `fn_0070D480` at
`ClearMap`'s `ValidateScriptVariables`. The heap's counter goes back to 1 only in
`UniqueKeyHeap::SetToZero` 0x7E1780, called through the heap's table at 0x553358 (`sub_5532F0`, which has no caller), never on a land change.
The slots themselves: [The script's things](script-commands.md#the-scripts-things-scriptmanage-slots).

### The writers

The original can write an object back as the land-script line that makes it again. Each class has its own
`SaveObject` (vt +0x82C). The vortex uses it to carry what an In vortex swallows to the next land (`vortex.txt`,
[The vortex](../world/vortex.md)); the land editor's save uses the same writers. These are their only users: a saved
game's world file is written by each class's binary `Save` (vt +0xEC) and read by its `Load` (vt +0xE8), in the
layout of [Saved games](../engine/saved-games.md#the-world-file). The line format is in
[The line format](../formats/land-and-script-files.md#the-line-format); the reading side is
[The map script reader](map-script-reader.md#the-map-script-reader-lhscriptx).

#### The command text

- `GSetup::GetCommandAsText` 0x715130 → `LHScriptCommandX::GetCommandAsText` 0x7E7FB0 gives the printf format of a
  command: the name and "(" (`strcpy`, `strcat`, 0x7E7FBE..0x7E7FD5), then per type letter (the 12 letters,
  letter − 'A' through the byte table 0x7E8078 and the jump table 0x7E8064, 0x7E7FE1..0x7E802A) A `"%s, "` (0xC34DDC),
  L `"\"%s\", "` (0xC34DE4), N `"%d, "` (0xBE83AC), F `"%f, "` (0xC34DD4); the last ", " is cut when anything was added
  (0x7E8035 / 0x7E8042), then `")\n"` (0xC22098, 0x7E8047). The names come from the table 0xC20F00 (16 bytes per
  SCRIPT_FEATURE_COMMANDS entry, the name first).
- `MapCoords::ConvertToText` 0x602880: `sprintf "\"%0.2f,%0.2f\""` (0xBF4198, the quotes in the literal) of x, then z,
  each (double)(fixed × 10.0f × 2^-16) on the x87 (`fild`, `fmul [0x930050]`, `fmul [0x8AC41C]`; with the FPU at 24
  bits, as `fn_007DEE00` sets it (`and cw, 0xFCFF` at 0x7DEE0D), the first product is rounded to a float and the
  second is exact).
- The exe's CRT `sprintf` 0x7C57D2 rounds the digits of `%0.Nf` **half up** (0.625 → "0.63", 2^-7 → "0.007813"), where
  the UCRT and glibc round half to even.

#### Every writer

- Every writer first calls `CheckAndSetSaved` 0x56FEF0: an object whose GameThing +0xC already equals
  GlobalSaveCount [0xD99710] has been saved in this pass and is skipped. The count goes up (`inc word`) in
  `SaveAllMap` 0x7183AE, `fn_00718C10` (at 0x718C93, and near 0x719154) and the VortexSave constructor 0x76F84E.
- The position written is `MapCoords − at` when an `at` is given (the vortex's In, `MapCoords::operator-` 0x6055C0).
- A writer that returns 0 (`xor eax, eax`) does not save the object (VortexSave's write fails for it); one that
  returns 1 without writing leaves it to its parent, or nothing is needed.

#### The physics classes

| Writer | Used by | Skips when | Line |
|---|---|---|---|
| MobileStatic 0x6088E0 | MobileStatic, Rock, Fragment | +0x7C set | `CREATE_MOBILE_STATIC(pos, info, altitude, x, y, z, scale)` |
| MobileObject 0x607270 | MobileObject, Ball | +0x58 set | `CREATE_MOBILEOBJECT(pos, info, ftol(y × 1000), ftol(scale × 1000))` |
| Pot 0x66D550 | Pot, the piles | it is part of a structure (a storage pit's or a workshop's pile, 0x66DA00) | `CREATE_POT(pos, info, resource type, amount)` |

- MobileStatic and MobileObject then mark every identical object (the same class, info and MapCoords: x, z and the
  altitude) as saved, so a stack is written once: MobileStatic 0x608A20..0x608A9E, MobileObject 0x607366..0x6073E6
  (`FindType(MapCoords, OBJECT_TYPE)` 0x6045C0 over the cell). MobileObject's pass goes through `FindTypeOnMap`
  (0x601668) filtered on the info's object type (+0x10): MOBILE_OBJECT 20 only, so a Ball (type 15) never marks its
  twins.
- **MobileStatic** `SaveObject` 0x6088E0: `CheckAndSetSaved` at 0x6088F1; the position 0x6088FE..0x608929; +0x7C (an
  Object*: what it belongs to) checked at 0x60892D; `GetScale` (vt +0x120), `GetZAngle` (+0x50C), `GetYAngle`
  (+0x508), `GetXAngle` (+0x504), each as a double (0x608947..0x608977): the stored angles as they are. The info's
  index = (+0x28 − 0xD3A6D8) / 0x12C and the altitude +8 as the vertical offset (0x60897D..0x6089A5). An
  `IsTownArtifact` (vt +0x800) object writes `TownArtifact::SaveObject` 0x4269C0's own line instead
  (0x6089F2..0x608A1B).
- **MobileObject** `SaveObject` 0x607270: `CheckAndSetSaved` at 0x60727B; the position 0x60728C..0x6072BB; +0x58 (what
  holds it) checked at 0x6072BF; `ftol(GetYAngle × 1000.0f [0x8AB228])`, `ftol(GetScale × 1000.0f)`, the info's index
  (+0x28 − 0xD38448) / 0x114 (0x6072CF..0x607301).
- **Pot** `SaveObject` 0x66D550: `CheckAndSetSaved` at 0x66D55A; the position 0x66D569..0x66D59B; `IsPartOfStructure`
  (vt +0x860) at 0x66D59F → nothing (0x66D632). A plain Pot's (0x55D4B0) is 0; a PotStructure's (0x66DA00) is its
  +0x78 structure when that `IsAvailable` (vt +0x2C); 0x66DA00 also clears +0x78 when its owner is not available. The
  info's index (+0x28 − 0xD4C660) / 0x144, `GetResourceType` (vt +0x690), the amount +0x70 (0x66D5AD..0x66D5D7).
- **A fragment is written as a rock**: its constructor gives every fragment GMobileStaticInfo row 2 (0xD3A930), Rock
  (`Fragment::Fragment`, 0x76E9E4). It comes back on the next land as a plain rock.

#### Other writers

- **DeadTree** `SaveObject` 0x511430 (`DeadTree::SaveObject`): `CREATE_DEAD_TREE` (43, "ALNFFFF"). `CheckAndSetSaved` at 0x511439; the position
  0x511446..0x511478; the player `GetPlayer` (vt +0x1C), else the neutral player (g_game +0x205A5B, `NeutralPlayerIndex`,
  the byte `GameThing::GetPlayer` 0x570130 reads; 0x51148C..0x5114A6), its name from the
  table 0xBFF094 (`GetPlayerNumber` 0x64A790) (0x51147C..0x5114BB); `GetZAngle`, `GetYAngle`, `GetXAngle`, `GetLife`
  (vt +0x11C), each a double (0x5114C4..0x5114F4); the tree info's index (+0x94 − 0xDA3AD8) / 0x140
  (0x5114FA..0x511512).
- **OneOffSpellSeed** `SaveObject` 0x72AB80 (`OneOffSpellSeed::SaveObject`): `CREATE_ONE_SHOT_SPELL_PU` (84, "AL"; not 83). `CheckAndSetSaved` at
  0x72AB8A; the position 0x72AB99..0x72ABCA. The name (0x72ABCE..0x72ABDA) is `GetMagicInfo` 0x72A610 (the seed's info,
  `GetMagicInfoFromPULevel` 0x72AFE0 at its power-up level +0x78: the base one when that level has no magic type) →
  `GetMagicInfoText` 0x5FB3F0 = `GetMagicEffectInfo` 0x5FB680 of that GMagicInfo's magicType (+0x10, the table
  0xD37D10) + 0x34 (debugString), as `GetInfoFromText` reads it.
- **Scaffold** `SaveObject` 0x6EAF30: `CREATE_SCAFFOLD` (44, "NANNN"). `CheckAndSetSaved` at 0x6EAF3A; the position
  0x6EAF49..0x6EAF77; +0x58 (what holds it, as MobileObject's) set → nothing (0x6EAF7B, returns 0); the town's number
  (`GetTown` vt +0x48 = +0x8C, then its +0x5B4; −1 without a town, 0x6EAF8C..0x6EAFA2); `ftol(GetYAngle × 1000.0f)` and
  `ftol(GetScale × 1000.0f)`; the info's index (+0x28 − 0xD959D0) / 0x128 (0x6EAFDC..0x6EAFF3). The line is (town,
  position, row, angle, size). openblack writes it for the vortex (`vortex_save_lines`, `ScaffoldLine`).
- Bonfire (0x4398A0), Villager (0x751AF0), Abode, Tree, Feature, Creature and some forty other classes have their own
  writers.

#### The footpath file

The footpaths are also saved in their own file (`.fot`): `GFootpath::Save` 0x535E00 writes the nodes, then +0x20;
`GFootpathNode::Save` 0x538D70 writes its MapCoords +0x18, then the byte +0x14 (the flags); a node is 0x30 bytes
(constructor 0x534CF0(coords, a, b)). `GFootpath::LoadAll`
makes a `new GFootpath` per saved footpath (the constructor 0x534EB0 pushes it at the list's head), and
`GFootpathLink::LoadAll` (0x536D9E) resolves each link save as it is read (0x536FA0): to the MultiMapFixed or the
planned abode at its point, else deleted.

### Sources

- runblack.exe (v1.42, with the 1.20 symbols): `GSetup::GetCommandAsText` 0x715130 / 0x7E7FB0,
  `MapCoords::ConvertToText` 0x602880, the `SaveObject` writers above, `GGame::OnNewGame`, the SkipBox and
  `GScript::LoadMap`; the decompiled listing and the bytecode of `challenge.chl`.

## openblack

### In openblack

- **The SkipBox**: `Game::Run`, after starting `LandControlAll`, clears the three bits (the start-of-game answer's
  service, `Locator::tutorialSkipSystem`), pauses the game and shows the SkipBox (`Gui/SkipBox`, with the texts of the
  patch database and entry 0xA24 for the button). Its answer sets the bits (0 none, 1 bit 23, 2 bits 23 and 24, 3 the
  three) and unpauses the game. openblack has no profiles: `CURRENT_PROFILE_HAS_CREATURE` (CHL 463, through
  `Locator::playerProfileSystem`) answers whether the profile's creature file (the `--creature-file` setting,
  [The player's creature](../creature/players-creature.md#the-players-creature)) exists, as the original answers for
  the profile's. Before the script camera was ported, `MOVE_CAMERA_*` and `HAS_CAMERA_ARRIVED` were stubs and
  `CreaturesInGlade` **stuck there for ever with the camera, the dialogue and the widescreen held**.
- **LOAD_MAP** publishes `events::LandChangeRequested` with the path; its handler, `Game::ChangeLand`, loads the land
  at once through `Game::LoadMap` (the game folder and the path), inside the script's call as the original does:
  nothing iterates the registry above the VM's step, and `LoadMap` does not touch the VM. The calling script and the
  rest of the turn go on on the new land. **⚠️ approximate** The turn number the state hash is given at that turn's end
  is the old land's.
  - Before the load, `ChangeLand` saves the local player's creature to the profile's mind file and its physique
    beside it (`ecs::player_creature::SaveMyCreature`,
    [A land change](../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again)), as `ClearMap`
    does first; then the registry's reset takes the creature with the land, and the next land's `LOAD_MY_CREATURE`
    makes it again from the file. The map-cycle test hook saves nothing; the debug menu saves only with "Take your
    creature along" (below).
  - Then `ChangeLand` releases the script things no variable holds (`ecs::script_held::Process`, the original's
    `fn_0070D480`), on the old land and before the seeds go back, as `ValidateScriptVariables` does before
    `GlobalGameLists::ClearMap` and `ResetState`. Its second call (0x552E0E) has nothing left to release here: the
    registry's reset takes every `ScriptHeld` with its entity. The scripts' variables are not cleared, in neither
    game. The map-cycle test hook is a separate path into `Game::LoadMap`, unchanged.
  - **The scripts' hit record** ✅ (ours, from the exe; 2026-10-18). `ValidateScriptVariables`' first part,
    `fn_006EB680` 0x6EB680, tests the script's `HitObject` (+0x70) and then `ObjectWhichHit` (+0x74): each one that
    is not null and whose `IsAvailable` (vt +0x2C: `GameThing::IsAvailable` 0x401810, the unavailable flag;
    `Villager::IsAvailable` 0x751D50 also says no to a villager whose vt +0xB04 state is 0xE, the dying state
    openblack's `villager::IsAvailable` tests) is false is set to 0
    (0x6EB693 / 0x6EB6A6); the other is kept. It is not in `GScript::LoadMap` 0x6FB320 itself (which only pops the
    path, draws the loading frame and calls `GGame::LoadMap`): it runs in `ClearMap` at 0x552DA9 (after the physics
    objects, the hand and the audio, before `GlobalGameLists::ClearMap`) and again at 0x552E0E (after
    `GlobalGameLists::ClearMap`, the challenge clear, the players, bookmarks, highlights, fireflies and particles,
    before the dead list is drained). The second call finds every thing of the land marked unavailable, so the pair
    leaves `ClearMap` empty. The same function also runs at the start of every script turn, `GScript::Process`
    0x6EB6B0 (0x6EB6B3, before the countdown and `LookIn`; `GGame::ProcessTurn` calls it at 0x54E693, after
    `PhysicsObject::GameTurnUpdate` writes the pair at 0x6463E2), in `GScript::Reset` 0x6EB2D0 (0x6EB391) and in
    `GScript::Load` (0x6EC289). In both of those the check decides nothing for the pair:
    - `GScript::Reset` writes 0 to +0x70 and +0x74 itself at its start (0x6EB2D9 / 0x6EB2DC), before
      `ValidateScriptVariables` (which runs only with the script library loaded, after
      `DeleteAllScriptCreatedGameThings`), so the restart empties the pair whatever is still available. Its callers
      are the script library's creation (`fn_006EB2B0` from `GScript::Create`), `GScript::LoadBinary` 0x6EB610 (the
      program read), `GGame::Init`, `GGame::Close`, `GGame::StartPlaygroundGame`, `GameOSFile::LoadAllGame`, the land
      script's script-load command (`fn_006EB220` from `GSetup::MapCommandProcess`, single player only) and
      `Config::ProcessOneGameTurn`. openblack: `DynamicsSystemInterface::ForgetHits` empties both, where the game
      reads its program (`Game::LoadStartLand`, where no turn has run, so the pair is already empty) and where the
      editor's Scripts panel reads one or the game's again (`LetGoOfTheOldProgram`, with the script objects' reset).
      Test: `PhysicsHitRecord.TheScriptsRestartEmptiesTheRecordEvenOfWhatIsStillAvailable`.
    - `GScript::Load` checks first (0x6EC289) and then reads the pair from the saved game (`ReadPtr` into +0x70 and
      +0x74, 0x6EC576..0x6EC587), so the loaded pair is the saved one. openblack has no saved-game load yet; when it
      comes, the pair is read with the rest of the script state.

    The pair is read by `GET_HIT_OBJECT` 0x6FBD80, `GET_OBJECT_WHICH_HIT` 0x6FBDC0 and
    `GAME_THING_HIT` 0x6FBE20 ([the hit record](../physics/physics.md)). openblack: the pair is the physics'
    (`DynamicsSystemInterface::GetHitObject` / `GetObjectWhichHit`), and `ForgetUnavailableHits` (the pure
    `physics::turn::ForgetUnavailable`) forgets each one that is gone or marked unavailable. `Game::LoadMap` calls it
    right after the registry's reset, where entt's new versions make every old entity invalid (the second call's
    result; the first one's changes nothing a script can read in between), and the scripts' turn calls it before the
    countdown and `LookIn`. Test: `PhysicsHitRecord.*` in `test/test_physics_end_of_turn.cpp`. The Physics debug
    window shows the pair. The test of each thing is `physics::turn::IsAvailableToHitRecord`, with the villager's own
    rule (not dying), as `RecordHit`'s ([which classes override `IsAvailable`](../physics/physics.md)).
  - **A variable holding a thing of the land before**: a variable holds the entity's number (index and version).
    `Registry::Reset` empties the registry with entt's `clear`, which gives every entity a new version as it goes; a
    new land's entity may take an old index but not the old number, so `Registry::Valid` refuses the old one and the
    natives say the thing is gone, as the original's. Pinned by `LandLoadThings` in `test_land_load_globals`.
    **⚠️ approximate** entt's version has 12 bits: after 4095 deletions of entities at the same index the number comes
    back, and a variable still holding the first would find the latest; the original's 32-bit id does not wrap in a
    game.
  - `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING`: `chlapi::InScriptFileList`, the same split and compare.
- **What a land change keeps** ✅ ([the original](#what-a-land-change-keeps), 2026-10-22, #206). `Game::LoadMap` asks
  `land_load::ResetsFor(firstLand, inPlaygrounds)` (`src/LandLoad.h`): the game's first land (a new game) and a
  land of the Playgrounds folder (a playground's start, as `StartPlaygroundGame`, which only `GGame::Init` calls, at
  0x54F759: a new game of its own) restart the scripts and make the camera again, and so does the testbed (a game of
  its own); any other load is a land change. `Game::PrepareNewLand` does the map's clear at every
  load and `Game::ResetScripts`, the scripts' restart, only when they restart:
  - the help system's reset (`HelpSystem::Reset`: the texts, the message sets, the category turns, the cinema bars,
    which it snaps out at once (`Hooks::snapWideScreen`, 2026-10-22; before, they slid out over the wide screen time),
    the icons, the history, the help switch, the speaker override) is in `ResetScripts`: a land change keeps the bars a
    script still holds, and the help system's state. Before, every land load reset it.
  - the close clipping (`CinematicDirectorSystem::SetCloseClipping(false)`) is in `ResetScripts`: a land change keeps
    a script's `SET_GRAPHICS_CLIPPING`. Before, every land load cleared it.
  - both advisors go home (`HelpSystem::ResetForScriptsRestart`: the help system's reset, then `SpiritHome(2, 0)` and
    `SpiritHome(1, 0)`, flying off) only in `ResetScripts`: a land change leaves them where they are. Before, no load
    sent them home. Test: `HelpSystem.TheScriptsRestartResetsItThenSendsBothAdvisorsHome`.
  - the script fade (`CinematicDirectorSystem::ResetFade`: no fade going on, the colour and its alpha 0) is in
    `ResetScripts`, after the help system's reset: a land change keeps a `SET_FADE` the script made (Land 1's way out
    fades to black before its `LOAD_MAP`). Before, no load cleared it (a playground's start kept the last game's
    fade). Test: `ScriptFade.TheScriptsRestartStopsTheFadeAndClearsThePicture`.
  - the camera: every load ends the camera's modes (`script_camera::ClearModes`: no script mode, dual mode, path or
    follow, as `ClearCameraStack`); only a new game (`land_load::Resets::newGame`: the first land, a playground's start
    and the testbed, as `GGame::Init` makes the camera again, `GCamera`'s constructor at 0x54F6A4, before its start
    modes' jump at 0x54F6E9) also puts the lens back to 70 degrees (`script_camera::Reset`). A land change keeps the
    lens a script set. Before, every land load put it back; from the first commits of #206 to 2026-10-22 a playground's
    start kept it too.
  - the interface interaction (`interface_interaction::Set(0)`: the level, the interface's limit bits, the hand's
    reach and the control map switches) is in `ResetScripts`: a land change keeps a tutorial's level. The camera
    features and the self-tilt's angle and height go back at every load (`camera_help::Reset`, after the statistics'
    clear, as `ClearMap` writes them). Before, every land load set the level back to normal, and the self-tilt's angle
    and height were never put back.
  - the scripts' audio switches (`ScriptAudioState::Reset`: the creatures' sounds, the dialogue-only sound, the
    alignment music, the music line and beats) are in `ResetScripts`: a land change keeps them. Before, every land
    load reset them. The Music window shows them.
  - the camera control's switches (`CameraControl::Reset`: the leashes and the highlights drawn, +0x7C) are in
    `ResetScripts`: a land change keeps a script's `SET_DRAW_LEASH` / `SET_DRAW_HIGHLIGHT`. Before, every land load
    put them back on.
  - the citadel interface (`worship::citadel::ResetInterfaceCitadel`, `SET_INTERFACE_CITADEL`'s value back to 1) is in
    `ResetScripts`: a land change keeps a script's 0, and the entrance stays closed until a script opens it. Before,
    every land load put it back to 1.
  - a hand demo still playing ends (`hand_demo::End`) only in `ResetScripts`: a land change lets it play on. Before,
    every land load ended it.
  - the intro's things (`IntroSystem::ReleaseAll`: the intro light, the intro hand, the debug camera mode) are
    released only in `ResetScripts`: a land change keeps them. **⚠️ approximate** openblack's intro hand is an entity,
    which goes with the registry at a land change, where the original's stays drawn; no script changes land while the
    intro plays (it is Land 1's opening). Before, every land load released them.
  - a sun a script hid (`SET_SUN_DRAW`) shows again only in `ResetScripts`; a land change keeps it hidden: the sky
    keeps the sun's component while the registry is emptied, as it keeps the moon's. Before, every land load showed
    it (and the sun's new entity forgot the switch anyway).
  - the sound confirmation (`audio::confirmation::Stop`, a `START_ANGLE_SOUND` 285 / 348 still on) stops only in
    `ResetScripts`: a land change lets it go on. Before, every land load stopped it.
  - the bookmarks' keys go back on at every load (`CameraBookmarkSystem::SetEnabled(true)` in `PrepareNewLand`), as
    `ClearMap`'s `InitStaticsValues` does; that was already so.
  - the interface status (the synced hand, its turn motion, the last-sent values, the status's throw) is made again
    only at a new game (`HandSystem::ResetTurnState(newGame)`); every load clears the last released object, its
    countdown and the drawn hand's object, then runs the original's check of the action
    (`HandSystem::ValidateAtClearMap`, `ecs::hand_validate`), which ends it only when it has an object or needs one,
    or the interface is inactive, and switches an active interface off under a script's bars
    ([clearing on a new land](../hand/states.md#clearing-on-a-new-land)). Before, every load ended the locked select
    and the apply gate, and earlier cleared it all.
  - the particles (`ParticleSystem::Reset`, `particles::manager::Clear`) and the magic (`MagicSystem::Reset`,
    `magic::OnLoadMap`) are reset at every load, as the map's clear resets them (rows above; checked 2026-10-22).
    Of the hand's magic (compared 2026-10-22): the gestures' state and the last seed type the R repeats are kept by a
    land change and reset only at a new game (`magic::OnNewGame`, [gestures](../magic/gestures.md#a-land-change));
    before, every load reset them. The utility effects (`particles::utility::Reset`) are particles, which the map's clear
    stops (`PSysGlobal::OnClearMap`); the fireflies' weights (`fire_fly::Reset`) are `FireFly::OnClearMap` 0x52A1E0's;
    the gesture system's queued events, the icon provider's registration and the test hooks are openblack's own; the
    list of forests that lost a magic tree (`magic_tree::Clear`) goes with the forests, whose numbers start again at
    the map's clear (`Forest::OnClearMap` 0x53B2F0: [0xCD04C8] 0, [0xBEA238] 1). The hand's miracle effects: the
    original's map clear deletes only the effect in the hand (`CHand::OnClearMap` → fn_0046E780: the object +0x494C
    deleted, +0x4950 0) and keeps the hand's `PHandFX` (CHand +0x4948, made by the CHand constructor at a new game;
    none of its users is a callee of `ClearMap`); its bracelets and tribe's ring follow a held seed out of the hand
    through the map clear's status pass (`SetPULevel(0, 0)`, `StopTribalPowerRing`). openblack (2026-10-22): the land
    load's `hand_casting::OnLoadMap` calls `MiracleFxSystem::OnClearMap` (`hand_fx::OnClearMap`: the in-hand effect
    released) and only a new game resets the rest (`hand_fx::Reset` from `magic::OnNewGame`: the bracelets, the
    bands, the glow, the charge, the tribes' ring and columns); the bands a land change keeps get their mesh entity
    again once the new registry is made (`hand_fx::OnNewLand`, after the hand's `Initialize` in
    `ResetRegistryForNewLand`), since every entity of the last land goes with its registry. Before, every load reset
    them all. Test: `MiracleFxSystemTest.AMapClearKeepsTheRingTheColumnsAndTheBraceletsAndDeletesOnlyTheEffectInTheHand`.
    The Land Island window's "Kept by a land change" shows the hand's bracelets and tribes' names.
  - the map clear's status pass (`GInterface::Validate` → fn_005CEBB0 → `GInterfaceStatus::Process`, after
    `GlobalGameLists::ClearMap` and before `ProcessDeadList`) runs in `Game::PrepareNewLand` when the load clears the
    map (`land_load::Resets::mapClear`: every load but the campaign's first land), before the registry is emptied
    (`HandSystem::ProcessStatusAtClearMap`, 2026-10-22): the hand's movement steps once more, the virtual influence
    takes one more turn with the land's rings, towns and citadel deleted (out of influence and unshielded: its strength
    wanes; a new game's starts afresh; 2026-10-22), the guidance's heart beat takes one more step with no town and no
    citadel left (no sample; 2026-10-22), the locked select is let
    go, and a held object leaves the hand with its out-of-hand calls (a seed: the last seed type the R repeats, its
    icon's charge, the in-hand effect, the bracelets and the tribe's ring). A new game first empties the hands, as its
    new interface status has none. Before, a held object was dropped on the next turn, after its entity was gone,
    with none of those calls, the virtual influence took no turn at the clear and a new game kept the last game's
    ([clearing on a new land](../hand/states.md#clearing-on-a-new-land)).
  - the guidance (the advisors' remarks and the villagers' reactions) starts again only at a new game
    (`audio::guidance::StartLand`, from `Game::StartNewLand` with `Resets::newGame`; the testbed is a new game): a land
    change keeps each remark type's last play, the last samples, the heart beat, the one-offs and whether it is
    enabled, and draws none of the start's random numbers, as the original, whose `GGuidance::Init` runs only at a new
    game or a saved game's load. The remembered things stay in the list, each marked as from an older land, so that
    none matches a new land's thing; in the original a new thing given a freed thing's address matches it, which
    depends on its allocator and cannot be reproduced
    ([voices](../audio/voices.md#the-guidance-across-a-land-change), 2026-10-22). Before, every land started it again.
  - the day / night clock (`DayNightClock::Reset`: noon, the default cycle, running) starts again only at a new game
    (`Resets::newGame`); a land change keeps its time, cycle, switch and a time move going on. Before, every land
    load put it at noon.
  - a film still playing (`video::Get().Stop()`) stops only at a new game (`Resets::newGame`, a playground's start
    too: the old game's end, `GGame::ToBeDeleted`, deletes the film before any `GGame::Init`); a land change leaves it,
    as no `DeleteVideo` caller is reached from `LOAD_MAP`. A film pauses the game, so only the
    debug menu can change land during one. Before, every land load stopped it.
  - the tooltip's icon stays drawn through a load (`help::tooltips::Reset` puts only the fields back, 2026-10-22); the new land's first tooltip turn deletes it or, for the same text, keeps it with no new fade-in. Before, every load deleted it. The temple's exit still deletes it.
  - the sequence mode goes back to none only at a new game (`Resets::newGame`, 2026-10-22); a land change keeps the falling spell's (the temple's exit leaves the citadel's first, as before). Before, every load put it to none.
  - the scripts' restart's other writes ([above](#the-scripts-restarts-other-writes), 2026-10-22): the countdown timer goes off and hidden with its turns kept (`script_countdown::OnScriptsRestart`) and a hand demo's pending trigger is forgotten (`hand_demo::ConsumeTrigger`), in `ResetScripts`; a playground's start takes the local creature out of a lesson script before it is saved (`Game::ChangeLand`). A land change keeps all three. Before, no load wrote them. Test: `TimersEventsTest.TheScriptsRestartTurnsTheCountdownOffAndHidesItKeepingItsTurns`.
  - the readout: the Land Island window's "Kept by a land change". Tests:
    `LandLoadScripts.OnlyANewGameOrAPlaygroundsStartRestartsTheScripts` and
    `LandLoadScripts.OnlyANewGameMakesTheCameraAgain` (`test/test_land_load_globals.cpp`).
- **The debug menu's Load Island.** Without "Take your creature along" it only loads the island (`Game::LoadMap`).
  With it, it goes through `Game::ChangeLand` and does what the game does at that point:
  - a story land (`Land1.txt` … `Land5.txt`): every script task is stopped first, as `LandControlAll`'s
    `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING` does, and `LandControlAll` too (it waits on the old land's control script and
    would load the next land on top). After the load `LandControlK` is started; for Land 1 `LandControlAll`, as at a
    new game's start. The land's own script then brings the creature back where its story does (Land 2 in
    `Land2VortexEntry`, Land 3 in `SetupLand3`, Lands 4 and 5 in `VortexEntry`; Land 1 only with
    `IsKeepingOldCreature`);
  - a playground (a script in `Playgrounds`): every task is stopped, as `GScript::Reset(1)` reboots the scripts at the
    skirmish start; after the load the profile's creature is made at the first player's temple pen point, knows the
    evil, rope and good leashes and is set to development stage 13, as the skirmish start's packet does
    (`Citadel::GetCreatureHomePos`, then `Creature::Load`, the four leash ints at 1 and `MoveToDevelopmentPhase(13)`,
    [The leashes a creature knows](../creature/leash.md#the-leashes-a-creature-knows)). Without a built temple, or when the creature cannot be made, one log line says
    so and no creature is made;
  - any other script: only `ChangeLand`.
  - Limits: the globals the skipped lands would have set (`ArkSailed`, `LethysKilled`) stay 0, so the story goes on as
    if those events never happened; no `LandControlAll` runs after a story land K ≥ 2, so leaving it loads nothing;
    Land 2's creature comes only after its arrival cinematic and its camera and dialogue waits. On a playground the
    four creature flags the skirmish start's packet sets to 1 (`Creature` +0x1108..+0x1114) are not set, and stopping
    the tasks is not the full script reboot of `GScript::Reset(1)`: the scripts' globals keep their values. If the
    load fails after the tasks are stopped, the old land stays with its scripts stopped and one log line says so.
    Automated runs never use the menu.
- **Our runs.** With the start-up answer 3 the vortex opens at about turn 31, but no run clicks the highlight
  (`OPENBLACK_IGNORE_REAL_INPUT=1`, a fixed mouse), so `LOAD_MAP` is never reached in fidelity's 140 turns or the
  cycle's Land 1 segment; even with a click at once it would come at about turn 172.
- **The writers**: `lhscriptx::WriteCommand(name, {values})` (`LHScriptX/ScriptWriter`): the command table copied from
  the exe, the format of 0x7E7FB0 (`lhscriptx::CommandAsText`), `ConvertToText`'s position text (`PositionText`) and
  the half-up rounding (`FixedHalfUp`: the exact decimal expansion of the double, then rounded half away from zero).
  It gives nothing for an unknown command, a wrong number of values or a value of the wrong type for its letter. A
  test checks that every command it can write has a reader with the same parameters.
- `ecs::land_script_save` (`ECS/LandScriptSave`): the save pass (`BeginSavePass` raises the save count), a writer per
  class registered by its owner, and the physics, dead tree and one-shot seed writers above (`SaveMobileStatic`,
  `SaveMobileObject`, `SavePot`, `SaveDeadTree`, `SaveOneOffSpellSeed`). A pot is skipped when it is a storage pit's
  pile (`StoragePitStore::OwnerOf`) or a workshop's (`workshops::WorkshopOfPile`). The static's own altitude is written,
  without openblack's ground drop of statics.
  - The vortex's writers (`vortex_save_lines`) write the angles the thing keeps (`placed_turn::AnglesOf`), as the
    exe writes its stored angles as they are (since 2026-10-20; before, they were taken apart from the rotation).
  - **⚠️ approximate** the `land_script_save` writers (no game caller yet: the land editor's save, their caller in the
    original besides the vortex, is not ported; a saved game does not use them) still take
    the MobileStatic, MobileObject and dead-tree angles from the rotation (`GetYXZ` 0x7FAB30), equal to the stored
    ones to rounding but wrapped (4 rad comes out −2.28).
  - **⚠️ approximate** the identical-object pass walks the cell in openblack's own order, not the exe's `FindType`
    chain; the result (all of them marked) is the same.
  - Every openblack MobileObject is of type 20; a Ball is not ported.
  - The MobileStatic's +0x7C and the MobileObject's +0x58 tests have no counterpart. An artefact of a player is
    written as its static's line followed by `MAKE_LAST_OBJECT_ARTIFACT` (town, player, worth)
    ([Vortexes](../world/vortex.md#the-land-script-round-trip-of-vortextxt)).
  - The DeadTree's player is `DeadTree::owner`, as the exe's +0x90 (`Rock::GetPlayer`, never NULL, so the fallback
    is never taken): the player its CREATE_DEAD_TREE line named for a land script's, the neutral player for a
    challenge script's, a dropped log or a felled one, the thrower's for a thrown tree that landed dead. Before
    2026-10-19 openblack wrote PLAYER_ONE for every one, and until 2026-10-20 a land script's was the neutral player's.
- **Footpaths**: `FotFile::Load` (`src/Serializer/FotFile.cpp`) makes one footpath per saved one
  (`ecs::footpaths::Create`), its nodes from their saved MapCoords and flags, `active` from the saved +0x20, and
  resolves the links as it reads them.

### Test hooks

- `OPENBLACK_TEST_SKIP_ANSWER=<answer>,<frame>`: answers the SkipBox at that frame, as its OK would.

## Pending

- **Changing land**:
  - ⚠️ approximate a variable's dead thing can come back as another after entt's 12-bit version wraps (4095 deletions
    at the same index, [above](#changing-land-load_map)); the original's 32-bit id does not;
  - the loading frame and its tip image, and the tribute and prayer power, are not ported or not checked (the day and
    night clock is kept by a land change, as the original, since 2026-10-22); `vortex.txt` is written and read back, but a whole
    crossing in play is not checked against the original.
- **What a land change keeps**, not yet compared with openblack (2026-10-22):
  - the scripts' restart's own reboot of the script engine (`fn_006F69A0` 0x6EB37C, below: ScriptDLL +0x24..+0x30 0
    and its `Reboot`, vt+0x6C, with the transport +0xC). It needs a subsystem openblack lacks: a restart of the loaded
    program's tasks and globals from their start. openblack's `LHVM::Reboot` unloads the program instead (its
    instructions, scripts and variables cleared), and the DLL's own `Reboot` is not read. openblack's playground start
    restarts the scripts' state but not the engine (its tasks are stopped only by the debug menu's Load Island);
  - the scripts' restart's fade of the script texts (`CreditTextItem::ResetClip(0)` 0x50B040 and `Reset(0)` 0x50AC50
    on each item of the list [0xCADB68], above). It needs a subsystem openblack lacks: the script text list itself,
    which only `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT` and `FADE_ALL_DRAW_TEXT` fill, and those natives are not
    implemented (`NotImplemented` in `src/CHLApi.cpp`).
  - cannot be reproduced: a remembered thing of the guidance matching a new land's thing at a freed thing's address
    ([voices, Pending](../audio/voices.md#approximate-and--inferred-guidance-and-night-voices)).
- **The SkipBox's checkbox texts** (5..8 of the table at 0xD17CA0) are not read.
- The writers of the other classes (each class's owner): the full list of `SaveObject` writers and the commands they
  write is in the old land-script writers report, to be merged by each class's page.
- Saved games are not line writers: their files and their port are on [Saved games](../engine/saved-games.md#pending).
- A building site's pile is linked through +0x74: whether its +0x78 is set too is not read. 0x66DA00 also clears
  +0x78 when its owner is not available (not ported, minor).
- The MobileStatic writer's +0x7C test and the MobileObject writer's +0x58 test are not ported (the dead tree's
  player is: `DeadTree::owner`, above; an artefact's line is written after its static's, above).
- The `land_script_save` writers take their angles from the rotation, not from the angles the thing keeps (the
  vortex's writers keep them since 2026-10-20).
