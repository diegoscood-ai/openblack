# Script audio

## Summary

What the scripts can do with sound: the 37 CHL audio functions (texts with voices, sound effects, music, sound tags,
the game-sound switch), the GScript switches they set, and the dialogue, wide screen and script camera that silence or
filter sounds. On the openblack side, how every caller in the world reaches the channels and how the effects CHL is
written. openblack implements every audio function, START_ANGLE_SOUND and its pitch twin included.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Script: audio CHL

**Faithful.**
- The CHL table `g_scriptFunctionTable` 0xC0DB98 has 464 entries of 0x90 B. The **37 audio opcodes** match
  openblack's bindings.
- In the original there is no ENABLE_BANTER, IS_PLAYING_SOUND, music fades or audio STOP_ALL: they must not be
  invented.
- `ScriptErrorMessage` only warns: the function **continues** with the bad value.
- State.

| op | name | original | openblack |
|---|---|---|---|
| 13 | RUN_TEXT | 0x6F7D60 | done (text and voice) |
| 14 | TEMP_TEXT | 0x6F7E40 | done (no voice, as in the original) |
| 15 | TEXT_READ | 0x6F8260 → IsTextRead | done (with the voice branch) |
| 43 | PLAY_SOUND_EFFECT | 0x70F7F0 (owner = sample no.) | done |
| 44 | START_MUSIC | 0x70FB20 (error outside 0..0x55 and continues; +0x98 = +0x9C = 0) | done |
| 45 | STOP_MUSIC | 0x70FB90 | done |
| 46 | ATTACH_MUSIC | 0x70FBF0 (error outside 1..84 and adds it anyway) | done |
| 47 | DETACH_MUSIC | 0x70FC60 | done |
| 30 / 31 | START_CAMERA_CONTROL / END_CAMERA_CONTROL | 0x6ECCA0 / 0x6ECEF0 (on release: +0x84 = 1, 0x6ECE74) | state done; the script camera, pending ([below](#dialogue-widescreen-and-script-camera)) |
| 32 | SET_WIDESCREEN | 0x6F7BF0 → HelpSystem::SetWideScreen 0x5C6AD0 | done, with the owner |
| 120 / 122 | START_DIALOGUE / IS_DIALOGUE_READY | 0x710690 / 0x710830 (no audio) | done |
| 121 | END_DIALOGUE | 0x710780 (+0x84 = 1, +0x9C = 0 only if the task has the dialogue) | done |
| 149 | MOVE_MUSIC | 0x70FCA0 | done |
| 181 | ENABLE_DISABLE_MUSIC | 0x70FD10 | done |
| 182 | GET_MUSIC_OBJ_DISTANCE | 0x70FD70 | done |
| 183 | GET_MUSIC_ENUM_DISTANCE | 0x70FDE0 (with an invalid type it pushes twice) | done, with the bug |
| 184 | SET_MUSIC_PLAY_POSITION | 0x70FE60 | done |
| 190 | RESTART_MUSIC | 0x70FF00 | done |
| 191 | MUSIC_PLAYED (object) | 0x70FF40 (true if the object is not valid) | done |
| 231 / 232 | TEMP_TEXT_WITH_NUMBER / RUN_TEXT_WITH_NUMBER | 0x6F7F50 / 0x6F7C70 | done |
| 246 | SPIRIT_SPEAKS | 0x710C40 → ConvertScriptSpiritToHelpSpirit 0x710350, GetSpiritWhoTalks 0x5C6E20 (query) | done |
| 285 / 348 | START_ANGLE_SOUND / StartPitchSound | 0x70FFA0 / 0x70FFE0 | done (`StartAngleSound285` / `StartAngleSound348` in `src/CHLApi.cpp` → `audio::confirmation::StartAngleSound` / `StartPitchSound`, `src/Audio/Services/Confirmation.cpp`; tests `Confirmation.*`; the camera's feeds: [Voices and texts](voices.md#pending)) |
| 317 | SET_CREATURE_SOUND | 0x710020 | done |
| 335 | LAST_MUSIC_LINE | 0x710050 (without audio: TEXT_READ, 0x7100A6) | done |
| 340 | GAME_PLAY_SAY_SOUND_EFFECT | 0x70F9B0 → 0x70F8E0 | done |
| 350 | MUSIC_PLAYED (type) | 0x70FBA0 (GAudio+0x28 != type; without audio: TEXT_READ, 0x70FBE4) | done |
| 357 | SET_GAME_SOUND | 0x7100B0 | done (`audio::SetGameSound`) |
| 409 | SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | done |
| 411 / 412 | GAME_CLEAR_DIALOGUE / GAME_CLOSE_DIALOGUE | 0x6FF6F0 / 0x6FF700 | done |
| 424 | STOP_SOUND_EFFECT | 0x70FA50 (isSay: narrator 2 → 0x270C; otherwise, 0x270E + 0x270D; never 0x270F) | done (with isSay for the voices) |
| 445 | ENABLE_DISABLE_ALIGNMENT_MUSIC | 0x710120 | done |
| 447 / 448 | ATTACH / DETACH_SOUND_TAG | 0x710150 / 0x7101D0 → 0x71E840 / 0x71EBE0 | done |
| 450 | GAME_SOUND_PLAYING | 0x710230 → fn_0042A280 | done |
| 458 | SAY_SOUND_EFFECT_PLAYING | 0x710280 | done |

- `GScript::ScriptErrorMessage` is 0x6F62B0.
- LAST_MUSIC_LINE does ftol(line) and then tests +0x98 ≥ it, unsigned (0x71007A..0x71009C).
- GET_MUSIC_ENUM_DISTANCE: a type outside 1..0x54 prints "Invalid music man!" and pushes 0.0 (0x70FDF4..0x70FE28), then
  GetPlayDistance is always pushed (0x70FE34..0x70FE4A).
- `MOVE_MUSIC` 0x70FCA0: "to" is popped first, then "from"; both must be valid (0x70FCEE..0x70FCF4).
- `GET_MUSIC_OBJ_DISTANCE` 0x70FD70: 0 for an invalid thing (0x70FDA1), else fn_004293B0.
- `SET_MUSIC_PLAY_POSITION` 0x70FE60: z, y and x popped first (the LHPoint at 0x70FEDB), then the thing.
- `MUSIC_PLAYED` 0x70FF40: true for an invalid thing (0x70FF71), else IsMusicThingFinished (a bool).

`GetScriptGameThing` 0x70D220 is approximated in `MusicThing` (CHLApi.cpp): 0 is null and a valid entity is a live
object. The original looks the id up in its table 0xD967F8 (1..0x1FF) **⚠️ approximate**. `CHAR2WCHAR` 0x8300A0 is done byte by
byte, without the 0x7FF limit **⚠️ approximate**: it only changes in 0x80..0x9F of CP-1252.

- GScript::ScriptErrorMessage 0x6F62B0 / ScriptWarningMessage 0x6F62C0: a debug message, the script goes on (in the code: `ScriptMessage`)
- 0x71083B..0x71084A (in the code: `IsSpiritReady`)
- 0x6ECD71..0x6ECE2E, pending (openblack has no camera modes): the current mode of GGame::GetCamera() (+0x28[+0x58]); none -> "Script camera has been removed!" (0xC0C0CC); else GScript::ReleaseDualCamera 0x6ED410 and, when the mode is a CameraModeScript (__RTDynamicCast), its vt+0x30 and a new CameraModeNew3 (0x4572E0, 0x300 bytes), or "We are in the wrong camera mode! - exception happened?" (0xC0C14C). Pending too: GCamera::SetCameraFov(k_ScriptEndFov, k_ScriptEndFovTime) (0x6ECE48) Not original (mod game.skip-intro, "free start"): the land's opening task has given the camera back, so from here the script gets the player back too (in the code: `ReleaseCameraControl`)
- GScript::ActualEndGameSpeed 0x6FAA60: GCamera::SetScriptSlomoControl(0) when there is a camera (not ported, see StartGameSpeed), GGame::SetSpeed(1.0) 0x5537F0 and +0xAC = 0 (in the code: `ActualEndGameSpeed`)
- HelpSystem::RunMessage 0x5C8CE0(first, last, script): false for first > last or when StopRunningScripts refuses; else +0x560 = the turn, PUSH(float first), PUSH(float last) and GScript::StartScript(script) (GGuidance::HelpSpiritSay 0x71D2AC: "MultiHelpJustTalkWithText", whose two parameters WhichTextFirst / WhichTextLast are these); true (in the code: `RunMessage`)
- GScript::StartGameSpeed 0x6FA9E0 (CHL 128 START_GAME_SPEED): when nobody or this task controls the game speed, GCamera::SetScriptSlomoControl(1) 0x441F40 (GCamera+0x70 = 1: no reader found besides Save/Load, not ported) and the task takes it (in the code: `StartGameSpeed`)
- GScript::EndGameSpeed 0x6FAAB0 (CHL 129 END_GAME_SPEED): ActualEndGameSpeed when nobody or this task controls it (in the code: `EndGameSpeed`)
- GScript::SetGameSpeed 0x6FAAE0 (CHL 066 SET_GAMESPEED), after the pop: GGame::SetSpeed only for the task that controls the game speed (an owner of 0 matches no running task) (in the code: `SetGameSpeed`)

#### GScript switches

**Faithful.** They are at g_game+0x250090 and are atomic in openblack, because the music thread writes the line and the hits.
They live in `src/Audio/Services/ScriptAudioState.{h,cpp}`, and `GScript::Reset` 0x6EB2D0 (`ScriptAudioState::Reset`, from
`Game::ResetScripts`: a new game or a playground's start; a land change keeps them, as the original's `LOAD_MAP`,
[what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps)) leaves them like this:

| field | what it is | after Reset | who writes it and who reads it |
|---|---|---|---|
| +0x84 | creature sounds | 1 (0x6EB2F4) | `SET_CREATURE_SOUND`; `END_DIALOGUE` (0x71080A) and the release of the script camera (fn_006ECD70 0x6ECE74, from `END_CAMERA_CONTROL` and when the task stops) set it to 1. It is read by fn_00483290+0x16A: with 0, only the local creature sounds |
| +0x90 | dialogue only (SET_GAME_SOUND) | 0 (0x6EB403) | stored here for the reset; read by the GAudio filters |
| +0x94 | alignment music | 1 (0x6EB409) | `ProcessAlignmentMusic` does nothing with 0 (0x427A20) |
| +0x98 | last line `L<n>` of the script music | 0 (0x6EB306) | 0x426BD1; START_MUSIC sets it to 0 |
| +0x9C | hits | 0 (0x6EB30C) | 1 on `L`, +1 on `P`/`W`. It is read by fn_005CB590+0x62B, **🔍 inferred**: a song's text follows it |

`HelpSystem::Reset` (0x6EB340) goes with it, only when the scripts restart (a new game or a playground's start), never at a land change ([what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps)).

- SET_GAME_SOUND false sets +0x90 = 1 and calls LHSampleStopAll; true sets it to 0.
- Write sites: SET_CREATURE_SOUND +0x84 at 0x71003D, ENABLE_DISABLE_ALIGNMENT_MUSIC +0x94 at 0x71013D, START_MUSIC
  +0x98 = 0 at 0x70FB56 and +0x9C = 0 at 0x70FB6B, END_DIALOGUE +0x9C = 0 at 0x710820. LAST_MUSIC_LINE compares +0x98 at
  0x710098.
- END_DIALOGUE writes the audio fields only when the calling task owns the dialogue (HelpSystem+0x45CC ==
  ScriptDLL::TaskNumber, 0x71078C..0x71079F).

- 0x710820 (in the code: `ScriptAudioState::EndDialogue`)
- The audio part of END_DIALOGUE 0x710780 (0x7107FE..0x710820): +0x84 = 1, +0x9C = 0, done only when the calling task owns the dialogue (HelpSystem+0x45CC == ScriptDLL::TaskNumber, 0x71078C..0x71079F; Help/ScriptControl.cpp) (in the code: `EndDialogue`)
- 0x6EB2FA (edi = 1) (in the code: `CameraControl::Reset`)
- ScriptDLL::StopTasksOfType 0x6F68F0 (GScript::StopScriptsOfType 0x6F0CC0) (in the code: `void`)

#### Dialogue, widescreen and script camera

**Faithful** in the state (`src/Help/ScriptControl.{h,cpp}` and `HelpSystem`). The original stores which
script **task** owns each thing, with `ScriptDLL::TaskNumber` 0x6F69F0 (ScriptLibraryR.dll 0x10008320: the current
task, +0x14; 0 outside a task). openblack provides it with `LHVM::GetCurrentTaskNumber` (the task `_currentTask`
running the native function), `GetCurrentTaskScriptType` (0x6F6A90) and `GetTaskScriptType` (0x6F6C50 → 0x100051F0:
+0x158, 1 if the task does not exist).

| field | what it is | who writes it |
|---|---|---|
| HelpSystem+0x45CC | the task that has the dialogue | `DialogueControlRequest` 0x5C6790 (if nobody controls it), `ClearDialogueControl` 0x5C67E0 |
| HelpSystem+0x45E8 / +0x45EC | widescreen and the task that set it (0 = the game) | `SetWideScreen` 0x5C6AD0 (only if +0x45E8 changes; on turning off, +0x45EC = 0) |
| GScript+0xA8 | the task that has the camera | `START_CAMERA_CONTROL` 0x6ECCDF / 0x6ECD5B; 0 on release (0x6ECE59) |
| GScript+0x78 / +0x80 | the leashes / the highlights are drawn | 0 when taking the camera outside the citadel; 1 when releasing it and on Reset |

- `IsDialogueControlled` 0x5C6740 = +0x45CC != 0, or +0x45E8 and +0x45EC. `IS_DIALOGUE_READY` pushes its negation.
- `START_DIALOGUE`: with no owner, both advisors go home (`SpiritHome(1/2, 0)`) and the request; it pushes **true even if
  the request fails** because of another task's widescreen (0x710722..0x71072E). The same task: warning and true. Another one:
  false, unless the owner is Help (type 2) and the requester is Script (1): `StopHelpScripts` (mask 0x4A) and the
  owner is checked again.
- `END_DIALOGUE`: only the owning task. `SpiritHome(1/2, isHelp)`, fn_005C6800 (releases the dialogue, removes the
  widescreen, advisors home, clears the text) and +0x84 = 1, +0x9C = 0.
- `SET_WIDESCREEN`: only the owner, or anyone if there is no owner; if the owner turns it on again (value != 0)
  it warns and continues (0x6F7C23..0x6F7C32).
- `START_CAMERA_CONTROL`: in the citadel (g_game+0x205A28 == 1) only TempleHelp/TempleSpecial tasks (0x18), without
  a camera mode. Outside, if the `CameraModeScript` mode is created (fn_00461140; not if `CantExitCurrentMode`).
- `END_CAMERA_CONTROL`: if the task has it, fn_006ECD70: mode `CameraModeNew3`, FOV 70° (0x8C762C) in 0.5 s,
  +0xA8 = 0, +0x84 = +0x80 = +0x78 = 1, fn_0042A5F0(1), SuperVillagers off, +0x7C = 0.
- When **a task is stopped** (callback 0x6EC6D0, LHVM's `stopTaskCallback`): fn_005C6800(task) and fn_006ECF20(task)
  give back the dialogue and the camera. ScriptLibraryR.dll calls it before removing the task from the list (0x100065CD,
  the removal from 0x10006612): fn_005C6800 still sees its type, the same as in LHVM.

- RUN_TEXT_WITH_NUMBER also turns an out-of-range text into text 0 (0x6F7CE8).
- `HelpSystem::Reset` takes the wide screen away (0x5C55D6), but not the dialogue owner +0x45CC.
- The script camera's release fn_006ECD70 sets the draw-highlight switch to 1 (0x6ECE85) and the draw-leash switch to 1
  (0x6ECE97).

In game (Land 1, `FollowUs`): START_CAMERA_CONTROL → START_DIALOGUE → START_GAME_SPEED → SET_WIDESCREEN →
**START_MUSIC 54** (intro.sad plays). With `MOVE_GAME_THING` (033) `FollowUs_loop_4` passes, the
family walks and the singing stones sound (PLAY_SOUND_EFFECT 49/50/54). The camera blocks (035, 003/004) and
SET_AVI_SEQUENCE (203) have been ported since (script-camera.md, video.md); the intro now stops in `Drag` at the hand demo
(map-loading.md, "In game (Land 1)"). The intro speaks: see [Voices in game](voices.md#voices-in-game).

- 0x71069C (HelpSystem+0x45CC) (in the code: `StartDialogue`)
- 0xC20B18 (in the code: `StartDialogue`)
- 0xC0D3B4 (in the code: `SetWideScreen`)
- 0xC0C130 (in the code: `StartCameraControl`)
- 0x6ECEFC..0x6ECF10 (in the code: `EndCameraControl`)
- 0x6ECF20..0x6ECF2A (in the code: `ReleaseCameraOf`)
- 0x6EB7EE..0x6EB807 (in the code: `StartScriptIfNoDialogue`)
- 0x5C8CEE: ja (in the code: `RunMessage`)
- 0x6EC6D5 / 0x6EC6DF (in the code: `OnTaskStopped`)
- 0x6EC72A..0x6EC748: GInterface::EndPlayBack when IsPlayBack(task) (in the code: `OnTaskStopped`)
- fn_006ECD70 0x6ECD98 / 0x6ECE35: GCamera::SetCameraFov(fn_00443670 = 1.2217305 rad (0x8C762C, 70 degrees), 0.5) (in the code: `k_ScriptEndFov`)
- ScriptDLL::PUSH 0x6F6BA0 of a float (VMType 2) (in the code: `void`)
- ScriptDLL::StartScript 0x6F6880(name, VMScriptType mask): the parameters come from the stack (in the code: `void`)
- HelpSystem::StopHelpScriptsForNewHelp 0x5C8C40 (StopRunningScripts 0x5C8C80 jumps to it): a task that has the dialogue (+0x45CC) and whose type has neither Help (2) nor MultiplayerHelp (0x40) (`test al, 0x42`) keeps it: false; else GScript::StopHelpScripts 0x6EC780 (the types 0x4A) and true (in the code: `StopHelpScriptsForNewHelp`)
- fn_006FAA40(task) (from the task-stop callback 0x6EC71E): ActualEndGameSpeed when the task controls the game speed (in the code: `ReleaseGameSpeedOf`)
- The task-stop callback 0x6EC6D0 (fn_006EB1D0 passes it to ScriptDLL at 0x6EB1F1), for the parts ported: nothing without a game or HelpSystem (0x6EC6D5 / 0x6EC6DF); HelpSystem fn_005C6800(task) (0x6EC6E9), GScript fn_006ECF20(task) (0x6EC70C), fn_006FAA40(task) (0x6EC71E) and GInterface::EndPlayBack when IsPlayBack(task) (0x6EC72A..0x6EC748, Input/HandDemo.h). HelpSystem fn_005C78C0(task) (0x6EC6FA) is ReleaseWideScreenOf. ScriptLibraryR.dll calls it before the task leaves the list (StopTask 0x100065B0: the callback 0x1003BDD0 at 0x100065CD, the unlink from 0x10006612), so GetScriptType(task) in fn_005C6800 still sees it, as LHVM::StopTask does (InvokeStopTaskCallback before the erase) (in the code: `OnTaskStopped`)

## openblack

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**Script: audio CHL**

- **done** = implemented; **stub** = `NotImplemented`.

**Dialogue, widescreen and script camera**

- `ScriptVm` (`src/CHLApi.h`) is the script VM as GScript asks it (`TaskNumber` 0x6F69F0, `GetCurrentTaskScriptType`
  0x6F6A90, `GetScriptType` 0x6F6C50, `StopTasksOfType` 0x6F68F0, `PUSH` 0x6F6BA0, `StartScript` 0x6F6880), on
  `Locator::vm`.
- the visual part is ported in its own areas since this page was written: the script camera with its exit FOV
  (`src/Camera/ScriptCamera.cpp`, `help::script_control::k_ScriptEndFov`, [the script camera](../camera/script-camera.md))
  and the advisors going home (`SPIRIT_HOME` and `START_DIALOGUE` → `HelpSystem::SpiritHome`,
  [advisors and help](../interface/advisors-and-help.md)). Not checked again here: `DialogBoxBase::HideAll` /
  `GInterface::SetActive`. The citadel is openblack's temple interior **🔍 inferred**. The bars are there: the `SetWideScreen` hook moves the `CinemaBars` (in 16:9 they are not visible, as in the original). The alignment music already reads the script widescreen (0x4279E9).

### The world's callers and the effects CHL in openblack

#### The world's callers on the channels

Each site goes through `audio::` with what its original call passes (checked in the disassembly). Nobody outside
`src/Audio` calls OpenAL (`AudioManager` no longer exists, [below](engine.md#a-single-engine)).

| openblack site | original | what it does now |
|---|---|---|
| `HandHolding` `PickUp` | `GInterface::GenericPickup` 0x5D2800 (0x5D2881..0x5D295D) | `tags::Create(object point, 10 G_PickUpObject, 0, 3, 0, 0, 3D, InGame, 0)` (0x71EB60) except for a rooted tree; a live villager (`Object::IsAlive` 0x402610) screams with another identical tag: child 180, woman 194, man 187 + `GetRandomSample(7)`. A `DeadTree` also sounds (it has no `IsTree`: `GameThingWithPos::IsTree` 0x402320 = 0) |
| `HandHolding` (uprooting) | `Tree::InterfaceSetInMagicHand` 0x74B730 | `tags::Create(tree point, 32 + GetRandomSample(3), 0, 3, 0, 0, 3D, InGame, 0)` if it does not have +0x24 & 0x40 |
| `HandPhysics` (dropping a tree) | `Tree::DropSfx` 0x74BC60 | options: InGame, owner the tree, 3D, track 0, 83 + GetTickCount() % 3, at its point |
| `HandResources` (wood to the store) | `Object::DoDeleteObjectAndTakeResource` 0x63AA93 | options: InGame, owner the object, 3D, track 0, 155 + `Counter::TreeMulch` ((c + 1) & 3) |
| `HandFish` (grabbing land) | fn_005D1AB0 0x5D1FE4 | `tags::Create(point, 4 + GetRandomSample(6), 0, 3, 0, 0, 2D, InGame, 0)`: the tag is the owner (new channel on each grab) |
| `HandFish` (hand in the water) | fn_005D1AB0 0x5D2167 | options: 99 + `Counter::HandInWater`, 3D, track 0, at (x, 0.2, z) |
| `HandEffects` (multi-pickup) | fn_0068F930 0x68F9E8 / 0x68FA25, StopMultiPickup 0x68FABA / 0x68FAD4 | `PlaySoundEffect` 44/98 3D track 0 at the hand every turn, `SetPitch(InGame, 0, n, 60 + 180 t)`, `StopSoundEffect(44/98, 0, InGame)` |
| `hand_detail::PlaySample(id)` | `FailApply` fn_005D18F0 0x5D1941 | `PlaySoundEffect(NULL, n, 3, 0, 0, 0, bank)` 2D (used by `HandSpellSeed`) |
| `hand_detail::PlaySample3D` | 3D options form, track 0 | no callers; the signature is kept |
| `Trees.cpp` `PlayAt` (growing) | `Tree::ApplyWaterSpell` 0x74C500 | 3D options, track 0, at the tree, through the channels, owned by the tree (+0x20 = the tree, 0x74C4D4; since issue 22, test `ForestSystemTest.waterSpellSoundIsOwnedByTheTree`) |
| `object_physics::TapRock` (`src/ECS/ObjectPhysics.cpp`) | `Rock::InterfaceTap` 0x6E751D | options: InGame, 130 + `Counter::RockTap`, owner the rock, 3D, track 0, at the hand's point (state +0xC8) |
| `DefaultWorldCameraModel` | `CameraModeNew3::FlyToPosFoc` 0x45899B, `Update` 0x45E305 | `PlaySoundEffect(0, 46 + (GetTickCount() & 3), 3, 0, 0, 2D, InGame)`. `SetFlight` (markers, script and hook flights) only if the camera is more than [0x9CE640]·1.5 = 150 from the destination (0x458967); the double click (`Update`) sounds with its own mark, without that test |
| `DynamicsSystem.cpp` (G_RockPast) | `PhysicsObject::GameTurnUpdate` 0x645C12 | `PlaySoundEffect(0, 69 + GetTickCount() % 5, 2, 0, 0, 2D, InGame)` |
| `AttemptCollisionSound` (the key and the distance: [Collision sounds](../physics/physics.md#collision-sounds)) | 0x646919 | `PlayAnimationEffect(object, |g_camera − point|, {nivel, 0, A, B, 75}, 0, editor.sad, track = A ≠ 0x16, 0, 0)`: the row is chosen by the bank's table (`LHSampleGetAnimEffectNumber`), no longer by the resolved 99-row table; the channel belongs to the object and sounds at its point |
| `BuildingDamageSystem.cpp` | `Abode::ReactToPhysicsImpact` 0x406610, `ApplyEffectsDueToPhysicalDestruction` 0x40671D | `DynamicsSystem::PlayAnimEffect` with the building as owner: hit {2/3, 0, 0x16, 0x10, 75} (p > 1000 / > 300), collapse {1, 0, 0x16, 9, 75}, track 0 |
| `MissionaryBoat` (was `PetitNavire`) | `PostDraw` 0x5E0465..0x5E04B9 | options: ScriptSfx 62/61/60, 2D, mode 2 |
| `PotResource::PlayPileSound` | fn_0066D1A0 0x66D26A | options: InGame, owner the pile, 3D, track 0, sample by GetTickCount read twice, food then wood (signature with the pile) |
| `DesignedScenery` (waterfall) | 0x5E3921 / 0x5E3BD5 | unchanged: the older `sound_tags::Create(TagDesc)` is already a core `tags::Create` (the original's marker is emulated with a point tag that is repeated as a thing tag) |

`audio::TickCount()` is the original's `GetTickCount` (ms of the process clock) for those that choose a sample with it.

Audible changes (all because of the original): picking up, uprooting, planting, mulching and hitting rocks sound **in 3D** where
it happens (before 2D and, except water and pots, outside the channels); when picked up a villager screams; a dead tree sounds when
picked up; mulching uses the counter and not randomness; the markers' woosh no longer sounds on short jumps (≤ 150); the
collisions choose their sample with the bank's table and follow the object; everything counts in the 16 channels and respects
GAudio's filters (e.g. G_RockPast, TreeFall, KnockRoof, HandThroughInfluence are userParam 1: they go silent with the script
widescreen).

#### Effects CHL

`src/Audio/Services/ScriptSound.{h,cpp}` (`audio::script_sound`), called from `CHLApi.cpp` with the original's POPs:

| CHL | original | openblack |
|---|---|---|
| PLAY_SOUND_EFFECT(sample, bank, pos, withPos) | 0x70F7F0: default options, bank GAudio+0x3A8 + 4·bank, sample, **owner = the sample** (`Owner::Key`), is3D = withPos, track 0, point, +0x164 = 1 | `PlaySoundEffect` |
| STOP_SOUND_EFFECT(isSay, id, bank) | 0x70FA50: without isSay, `StopPlayingSoundEffect(id, id, bank)`; with isSay, the text's voice (table 0x942B38) on 0x270C (narrator 2) or on 0x270E and 0x270D; never 0x270F | `StopSoundEffect` |
| GAME_SOUND_PLAYING(sample, bank) | 0x710230 → fn_0042A280(sample, sample, bank) | `GameSoundPlaying` |
| ATTACH_SOUND_TAG(threeD, sample, bank, obj) | 0x710150 → `SoundTag::Create(thing, sample, track = threeD ≠ 0, 2, 0, 0, threeD, bank, 0)` 0x71E840 | `AttachSoundTag` |
| DETACH_SOUND_TAG(sample, bank, obj) | 0x7101D0 → `SoundTag::Remove(thing, sample, bank)` 0x71EBE0 | `DetachSoundTag` |
| SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | `audio::SoundExists()`: the audio is on an OpenAL device (`device::IsOpen()`) **⚠️ approximate** |
| SET_GAME_SOUND(on) | 0x7100B0 | `audio::SetGameSound` |

The object of ATTACH/DETACH is given by `MusicThing` (GetScriptGameThing 0x70D220, approximated the same as for music). A bank
outside 1..10 neither sounds nor stops **(approximate**: the original indexes GAudio+0x3A8 without checking).

Test `test_script_sound` (10): PLAY(93, 5, p, 1) → 3D channel with owner `Key(93)` without track; GAME_SOUND_PLAYING true and
false after STOP(0, 93, 5); 2D without distance cutoff; 3D beyond its maximum does not start; bank 11 → nothing;
SET_GAME_SOUND(false) leaves only HelpSprites/Villagers; ATTACH(1, 126, 5, thing) → mode 2 tag owning the channel, 3D with
track, does not re-trigger while playing, DETACH releases the loop and the tag goes away when it ends; ATTACH 2D without track; without a thing,
nothing; SOUND_EXISTS false without a device.

In game (Land 1, `OPENBLACK_SFX_TRACE=1`): the script
`FollowUs` plays the singing stones (`PLAY_SOUND_EFFECT(49/50/54, 5, point, 1)`: Stone1/2/6 3D, owner the sample);
far away they are cut by their maximum (100) and with the camera next to them (`OPENBLACK_CAMERA_LOCK`) they sound on a channel. Also
heard are the woosh of the `OPENBLACK_CAMERA_FLY` flight (2D), the rock hits (3D with the rock as owner), the water
grab and the pickup (tags), the collisions of the thrown tree ({1/2, 0, 20, 9/12/16, 75} → G_Crash_Tree_*) and the mulching.

**Audit.** Checked in the disassembly 0x70F7F0 (POPs and fields +0x04 / +0x08 / +0x0C / +0x20 / +0x24 / +0x30..+0x38 /
+0x164), 0x70FA50 (isSay, table 0x942B3C/+0x40, 0x270C/0x270E/0x270D), 0x710150 / 0x7101D0 (order of POPs and arguments
of SoundTag::Create / Remove), 0x5D2800 (0x5D2881..0x5D295D: tag 10, screams 180/194/187 + rand 7 with IsAlive
0x402610), 0x74B730, 0x74BC60, 0x63AA13, 0x6E74B8, 0x458967, 0x45E119..0x45E305, 0x645BEE, 0x406511 (B = ebp = 0x10 from
0x40626C) / 0x406640, 0x66D1A0, 0x5D1933, 0x68F9E8, 0x5D1FC4, 0x5E0413: they add up. DeadTree inherits from Rock
(bw1-decomp DeadTree.h), not from Tree: it is not IsTree and it sounds when picked up. Only one camera comment fixed (the
double-click woosh flag is [esp+0x23], not [esp+0x4B]; the [esp+0x4B] case, distance 1000, is the given-up land grip of
[Player camera: the land grip](../camera/player-camera.md#player-camera-the-land-grip), ported). In game:
PLAY_SOUND_EFFECT(49/50/54, 5, point, 1) with the camera next to it → Scriptsfx 3D, track 0, owner `key
0x31/0x32/0x36`, on a channel. (Approximate, no audible change) the pickup and uproot tags use the Transform's point,
the original GetAltitude + the height of its MapCoords (0x71EB60).

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

- START_ANGLE_SOUND (285) and its pitch twin (348) are done (above); what differs in GConfirmation's camera feeds is on
  [Voices and texts](voices.md#pending).
- `GetScriptGameThing` 0x70D220 is approximated in `MusicThing`: the original looks the id up in its table 0xD967F8
  (1..0x1FF) **⚠️ approximate**.
- `CHAR2WCHAR` 0x8300A0 is done byte by byte, without the 0x7FF limit **⚠️ approximate**: it only changes in 0x80..0x9F
  of CP-1252.
