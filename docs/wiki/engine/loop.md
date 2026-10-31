# The engine loop: turns and frames

## Summary

The original runs on one thread. Each pass of its main loop runs at most one game turn (ten in a network game) when
the game timer says one is due, then computes the frame clock, then draws one frame and handles the window's messages.
The turn is the only place the game state changes: the interface's input becomes packets that a later turn applies.
The draw shares the local, CRT and particle random streams with the turn
([Random streams between turn and frame](random.md#random-streams-between-turn-and-frame)).

openblack's `Game::Update` follows the same order: input, the due turns (`GameLogicLoop`, named call by call after the
original's turn), the frame clock, the per-frame updaters, the hand, and the draw. The hand's actions go through the
game packets, applied at the start of the next turn, and the first animation clip of a new being is set as it is
made. The game statistics are ported. Two runs are proved to be the same game with a per-turn state hash and a fixed frame time.

Progress: [the progress index](../project/progress/INDEX.md) (what our tree does, row by row). The game clock itself
(timer, turn, frame clock, speed, pause) is on [The game clock](game-clock.md#game-clock).

## The original

Everything is read in `runblack.exe` and in bw1-decomp `src/Black/Game.cpp`, except what is marked **🔍 inferred**;
what openblack does differently is marked **⚠️ approximate**, **(pending)** or **(not ported)**.

### The original loop

`GGame::Loop` 0x54CF20, each iteration (bw1-decomp Game.cpp:1902-1984):

1. `control_map->ProcessActionsPerformed`, `ProcessBufferedKeys`.
2. `ProcessNetworkPackets` 0x54CC30 (called at 0x54D28A): while `LocalTimerSaysDoATurn` 0x54C4A0 and fewer than
   1 turn this frame (10 in a network game), `ProcessOneGameTurn` 0x54D620. In a network game a turn runs only when
   the next packet is a superpacket: one superpacket is one turn (lockstep). Then fn_005525E0 (called at 0x54D291)
   hands the packets sent up to that point to the session; a later turn applies them
   ([The interface](#the-interface-frames-send-packets-turns-apply-them)).
3. The frame clock (0x54D2A8..0x54D3A6: remainder, visual clock, `g_game_time_inc`, the fraction of the turn; see
   [The game clock](game-clock.md#game-clock)).
4. `ProcessGraphicsEngine` 0x54D850: mouse delta, `GCamera::Update` 0x441F80 (at 0x54D879),
   `GInterface::PreDrawProcess` 0x5CE9E0, `Process3dEngine` 0x54DA80 (the draw, [Frame loop](#frame-loop) and
   [The frame and its draw stages](../rendering/frame.md#draw-stages-in-order)), `PostDrawProcess` 0x5CEAB0,
   `HelpSystem::PostDrawProcess`.
5. `NetworkTurnsThisFrame = 0` (0x54D3C3); `ProcessFrameInputs` 0x54C340 (`Mouse.ProcessButtons`,
   `GInterface::ProcessFrameUpdates` 0x5CEDB0), screenshot, `FlipScreen`.

#### One game turn

`ProcessOneGameTurn` 0x54D620: `GameTimeMilliseconds += MsPerTurn`, **`ProcessGameInputs` 0x54C3D0**
(`ProcessBufferedKeys`, `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420, `GInterface::Process`
0x5CEC10), `ProcessGameCode` 0x54D820 (`StartTurn` 0x54E4F0 → `ProcessTurn` 0x54E5C0 unless paused → `EndTurn`
0x54E960), then `fn_005557D0`, `DoWallHuggerLookahead` 0x609A50, `RepairMissingMothers`.

Call sites: `ProcessOneGameTurn` → `ProcessGameInputs` [0x54D662]; `ProcessGameInputs` → `GInterface::Process`
[0x54C40D]; `ProcessGraphicsEngine` → `Process3dEngine` [0x54D88E]; `Process3dEngine` → `HelpSystem::Draw3D` 0x5C59A0
[0x54E2ED]; frame pass `GGame::Loop` [0x54D3E9] → `ProcessFrameUpdates` 0x5CEDB0 → fn_005D9A20.

#### The window's messages and minimising

The window's messages are handled at the end of the frame: `LHScreen::Flip`, after `LHFlip`, calls
`ProcessWindowMessages` 0x7DB940 when `AnyWindowMessages` 0x7DB910 finds one (0x7DE1D6..0x7DE1DF; the other caller is
fn_00641FE0 at 0x6422E7). It empties the queue with `PeekMessage` / `GetMessage` / `DispatchMessage`, calls
`AltTabReactivate` 0x7DE6F0 if a restore (WM_SYSCOMMAND SC_RESTORE) set [0xE8C0FB], and starts again while
`AltTabbedAway` [0xE8C5E0] is set (0x7DB9E0..0x7DB9E7).

- Minimising (WM_SIZE SIZE_MINIMIZED) sets it in `AltTabDeactivate` 0x7DE6D0, which first switches the audio off (the
  callback [0xE8C5D0] with 0) and releases the textures (fn_0085DB90); `AltTabReactivate` restores the surfaces and the
  textures (fn_0085DD60), switches the audio on (callback with 1) and clears it.
- Losing the focus (WM_ACTIVATEAPP 0x7DC073, WM_KILLFOCUS 0x7DC09B) only clears the keyboard and the mouse.
- While minimised nothing else runs: no turn, no frame, no draw, no network packet handled. Neither function touches
  the game timer, which keeps counting: on the way back `LocalTimerSaysDoATurn` finds more than 2 s of lag and resets
  the timer (the time away is dropped), and less is caught up one turn a frame; the frame clock clamps its remainder as
  always ([The game clock](game-clock.md#game-clock)).
- `GGame::Loop` also skips `ProcessGraphicsEngine` while `IsAppMinimized` 0x7DED50 ([0xE8C0FC], set by WM_SIZE
  SIZE_MINIMIZED at 0x7DC003 and cleared by SIZE_RESTORED or SIZE_MAXIMIZED at 0x7DC029; bw1-decomp Game.cpp:1962).
  Since the same minimise also sets `AltTabbedAway`, no frame reaches that test minimised in the normal path.

#### The hand before the landscape

In `Process3dEngine` 0x54DA80 the interface's hand (`GInterface` +0x3A0, from `GGame::MyInterface` 0x555850 at
0x54DE97) is prepared with `CHand::PrepareForDrawing` 0x46C550 (its virtual +0x18, called at 0x54DEAA): the hand's
state change and its hide (the invisible state's Enter), its placement and the held object's matrix. Then
`PSysLightMaps::AddDrawing` (0x54DEAD), `LH3DLandscape::TextureUpdateThread` (0x54DEB2), `LH3DAtmos::Update3D`
(0x54DEC7) and `GLandscape::Draw` 0x5E42E0 (0x54DEDE), which fills the trees' bend table (0x5DF1B0; bw1-decomp's name differs: `FlyingObjects::Add`, which its C bears out as the bend
sources: it keeps the point and radius in `FlyingObjects::g_list` and marks the trees of the 3 by 3 cells around it;
the hand's slot
at 0x5E4994..0x5E49CA, [Trees: drawing](../world/trees.md#drawing)) before the trees are drawn. So the bends of a
frame see that frame's hidden flag and that frame's held pose.

#### Speed opcodes and threads

The script's speed opcodes: `START_GAME_SPEED` 0x6FA9E0, `END_GAME_SPEED` 0x6FAAB0, `SET_GAME_SPEED` 0x6FAAE0 (the
speed itself is in [The game clock](game-clock.md#game-clock)).

The game runs on one thread; the window's input reaches it through critical sections (`LHKeyboard`, `LHScreen`,
bw1-decomp Game.cpp:1604-1618).

### Frame loop

`GGame::Loop` 0x54CF20 (bw1-decomp `src/Black/Game.cpp`), the draw side of each iteration:

- `ProcessGraphicsEngine` 0x54D850 → [mouse, camera->Update (sets the FOV and the dynamic near plane),
  `GInterface::PreDrawProcess` 0x5CE9E0 (hand collision, disciple icon), **`Process3dEngine` 0x54DA80**, BMan / debug
  camera editor, `GInterface::PostDrawProcess` (only updates leash and collision, does not draw),
  `HelpSystem::PostDrawProcess` (flag)] → ScriptedScreenShot → **`GGame::FlipScreen` 0x54D800 → `LHScreen::Flip`
  0x7DE090** (timing text, software `LHMouse::Draw`, `LHFlip` 0x7DE580 and then the frame *clear* fn_0082EE70, see
  [Global state](../rendering/frame.md#global-state)).
- `Process3dEngine`: `LH3DRender::StartFrame` 0x82F0E0 → video (Bink) → switch field_0x205a28 (0 = world,
  1 = citadel / temple, 2 = falling-spell video) → fade / help (below) / influence → **`LH3DRender::FinishFrame` 0x82F460**
  (Z-sort flush + callbacks + EndScene) → debug 2D (CreatureMentalEditor, leash info, DisplayHowImpressed, CPU
  players, countdown text) → `LH3DAtmos::Render2D` (debug weather map with Lock, **🔍 inferred**).
- `StartFrame`: fninit / FPU control, delta time (g_delta_time 0xC38134, smoothed fps 0xEC7FC0), **BeginScene**
  (vt+0x14), g_frame++, g_started_frame = 1, **zsorter reset fn_0083F3B0**, fn_00813770, fn_0085BF00, SetLight /
  SetProjMatrix only with hardware T&L (never: `start_system` forces [0xC386E4] = 1), fn_00821270.
- The help system's tip bubble (`HelpSystem::Draw3D` 0x5C5B38 → fn_005C83D0, which passes the callback sub_5C8620 at
  0x5C8473 → sub_5C8530) is built from three parts: part 0 is the item's own help text `[this+0x1C]` from
  `HelpTextDatabase` [0xD17CA8] (12-byte entries, text at +8; entry 0 when the index is 0 or past the count
  [0xD17CAC]; 0x5C8579..0x5C85BB); part 1 is the fixed wide string " " (0x9CEE0C, 0x5C85C7); part 2 is help text
  0x1016 (`cmp [0xd17cac], 0x1016`, entry at +0xC108, 0x5C85D6..0x5C8609; entry 0 when the database is shorter) [V].

#### The frame rate's cap

Read 2026-10-22 in the v1.42 exe (xref, all callers of the imports). The original holds its frame rate only through the
screen's vertical sync; it has no frame limiter of its own.
- **No wait in the play loop.** Of the ten calls of `Sleep` (import 0x8A9230) none is in `GGame::Loop`, its
  `ProcessGraphicsEngine` or `Process3dEngine` path: they are `CPUCheck::GetCPUSpeed` (once, at start), `RunAlexFX` (the
  logos), the multiplayer front end (sub_6239E0), `Good_sleep_us` 0x642610, the CRT's `_sleep`, `std::_Lockit`,
  `setlocale`, the mouse input thread (`MouseInputThreadProc` 0x7DBD78, its own thread) and GameSpy's peer code.
  `Good_sleep_us` (`QueryPerformanceCounter`, a `Sleep` once over 1000 µs are owed) is called only while a film waits for
  its next frame (`Process3dEngine` 0x54DBBD..0x54DBC2, 500 µs between `VideoPoll`s; the Bink timer sub_54AD00), and by
  `PlayPreIntroVideo`. `WaitForSingleObject` is never called; `timeBeginPeriod`, `MsgWaitForMultipleObjects` and
  `SleepEx` are not imported. No minimum frame time is kept: `StartFrame` takes the frame's ms as they come (1 when
  ≤ 0, 0x82F195).
- **The flip.** `GGame::FlipScreen` 0x54D800 → `LHScreen::Flip` 0x7DE090 → `LHFlip` 0x7DE580: full screen
  (`LHScreen +0x64` = 0) calls the primary surface's `Flip` (vt+0x2C, 0x7DE5D0..0x7DE5DF) with the flags at
  `LHScreen +0x84`; windowed (+0x64 ≠ 0) it `Blt`s the back buffer to the window (vt+0x14, 0x7DE5A7..0x7DE5CB) with
  0x1000000, `DDBLT_WAIT`, which waits for the blitter, not the refresh.
- **The flags.** The `LHScreen` constructor reads `VSync` (0xC31330) under `Software\Lionhead Studios Ltd\Black &
  White\BWSetup` (0x9CAFF8) through `RegistryRetrieveULong` (0x7DCF5C..0x7DCF93): missing → 1, non-zero → 1, zero →
  8. 1 is `DDFLIP_WAIT`, a flip that keeps to the vertical blank, so the frame rate is held at the monitor's refresh
  rate; 8 is `DDFLIP_NOVSYNC`, no cap.
- **Full screen** is the default: `start_system` reads `FullScreen` (0xBFEC84) from the same key (0x642EF4..0x642F32)
  and calls `SetFullscreenMode(1)` when it is missing or non-zero (the constructor leaves the screen windowed until then,
  0x7DCF4D).
- So a plain install (neither value set) runs full screen at the monitor's rate. With
  `VSync` 0, or windowed, the original draws as fast as it can, and its whole-step ages (the smoke's and the mists',
  issue #215) stand still above 250 fps there, as openblack's do with `--no-vsync`.

### The turn, call by call

`GGame::ProcessTurn` 0x54E5C0 calls, in this order (call site → callee):

| Site | Callee |
|---|---|
| 0x54E5C7 | `Whale::ProcessAll` 0x775140 |
| 0x54E5D7 | `LH3DAtmos::UpdateGame` 0x8356E0 |
| 0x54E62D | `PSysGlobal::GameLoopStart` 0x68F590 |
| 0x54E637 | `GGameInfo::Process` 0x557B60 |
| 0x54E63C | `InfluenceRing::ProcessRings` 0x5CDB90 |
| 0x54E641 | `GPlayer::ProcessPlayers` 0x649A20 |
| 0x54E646 | `Dance::ProcessDances` 0x50BB60 |
| 0x54E651 | `GlobalGameLists::Process` 0x591370 |
| 0x54E656 | `Forest::ProcessForests` 0x539D70 |
| 0x54E65B | `Living::ProcessLiving` 0x5EC810 |
| 0x54E660 | `FireEffect::ProcessList` 0x730760 |
| 0x54E665 | `Ball::ProcessBalls` 0x435F30 |
| 0x54E66A | `Reaction::ProcessReactions` 0x6E3B50 |
| 0x54E66F | `Spell::ProcessSpells` 0x720300 |
| 0x54E674 | `GParticleContainer::ProcessParticleContainers` 0x63E090 |
| 0x54E679 | `FireFly::ProcessAll` 0x52B7A0 |
| 0x54E67E | `PhysicsObject::GameTurnUpdate` 0x644FC0 |
| 0x54E683 | `PSysEditorInterface::ProcessGameTurn` 0x67D630 |
| 0x54E688 | `PSysGlobal::GameLoopEnd` 0x68F5B0 |
| 0x54E693 | `GScript::Process` 0x6EB6B0 |
| 0x54E69E | `HelpSystem::Process` 0x5C8FE0 |
| 0x54E6A9 | `HelpProfile::Process` 0x5C4660 |
| 0x54E6C3 | `GLandAlignement::UpdateTime` 0x5E1FE0 |
| 0x54E6CB | `WeatherThing::ProcessWeatherThings` 0x7741A0 |
| 0x54E6D0 | `Bookmark::ProcessAll` 0x439DD0 |
| 0x54E6D5 | `ScriptHighlight::ProcessHighlights` 0x70A460 |
| 0x54E6DA | `GClimate::ProcessAll` 0x771BE0 |
| 0x54E6DF | `GBelief::ProcessOncePerTurn` 0x4380B0 |
| 0x54E6F1 | `CHand::GameTurnUpdate` 0x46E4E0 |
| 0x54E6F8 | `GGame::AddPlayerSparkles` 0x552640 |
| 0x54E6FD | `MobileObject::AddMobileObjectCheckSum` 0x606FC0 |
| 0x54E704 | `GameThing::ProcessDeadList` 0x56FB10 (argument 0) |
| 0x54E70C | `Reward::ProcessList` 0x6E6890 |
| 0x54E711 | `GSpookyVoices::Process` 0x72E310 |
| 0x54E716 | `GGuidance::HelpSpritesCheckMoonPhase` 0x71D1C0 |
| 0x54E729 | `GGuidance::ProcessTownDesireSFX` 0x71B020 |
| 0x54E731 | `GConfirmation::Process` 0x71A650 |
| 0x54E738 | `GGame::Update3DInfluence` 0x555280 |
| 0x54E743 | `GCamera::CheckStackedModesForValidity` 0x441D40 |
| 0x54E74E | `GCamera::Validate` 0x441F50 |
| 0x54E768 | `Fragment::ProcessTimer` 0x76EAF0 (each fragment, the next taken first) |
| 0x54E77A | `MusicMoodController::UpdateOnGameTurn` 0x633EF0 |

Then the memory packets and `ScriptRebootRequested`.

- `GlobalGameLists::Process` 0x591370 calls, in this order: `Field::Process` [0x591379], `FishFarm::Process`
  [0x591398], `MoveAlongPath` of the WALK_PATH list [0x5913ED], `Mist::Process` 0x606880 of each mist (a script's
  fade, openblack's `ecs::mist_fade::ProcessTurn`) [0x59140B], fn_0066E1C0 [0x591429] and the PuzzleGame fn_006D7480
  [0x591449].
- `GScript::Process` 0x6EB6B0 starts with fn_006EB930 [0x6EB6BA], the countdown timer's turn, before running the
  scripts.
- `GGame::EndTurn` 0x54E960 keeps the statistics: every `UpdateInterval` (10) turns, every 5 while the game is paused
  (g_game+0x14 & 4, GAME_FLAG_PAUSED; 0x54EAE9) [V], it calls `GameStats::AddToTotalLinesOfCodeExecuted` 0x565110 [0x54EAE9..0x54EB07], which draws a
  number from the local stream (`GRand::LocalRand` 0x6DE570), and then the frames-per-second count [0x54EB0C..0x54EB39]
  (see [Game statistics](#game-statistics-gamestats)). The flag is the pause bit that `PauseGame` 0x54AE20 toggles
  (0x54AE5A..0x54AE68, single player only, 0x54AE46; bw1-decomp `GAME_FLAG_PAUSED = 0x4`): `UpdateInterval` [0x8FFDB8]
  = 10 (oracle peek) is halved by `shr 1` at 0x54EAF5, the turn is divided by it at 0x54EAFF, and the call at 0x54EB07
  runs when the remainder is 0. A paused `EndTurn` only happens on the turn `PauseGame(1)` ran in, or with the
  force-turn cheat (0x400000).

- Before its statistics `GGame::EndTurn` calls `GameOSFile::AutoSave(false)` at 0x54EA43 when the autosave flag is
  set and the game is not over, then checks for the game's end (Game.cpp:2521): openblack's step `AutoSave` before
  `GameOver` ([the saves in play](saved-games.md#the-saves-in-play)).

#### The dead list

`GameThing::ToBeDeleted` 0x56FB70 marks, `ProcessDeadList(0)` frees on the second pass:

- `GameThing::ToBeDeleted` 0x56FB70: already unavailable, nothing; with its `now` argument, deleted at once; else
  marked UNAVAILABLE (+0xA bit 0, bit 1 clear) and put at the head of the dead list (GameLists.GameThings, head
  g_game+0x205D1C, count +0x205D20). The physics body stays: `PhysicsObject::GameTurnUpdate` drops the unavailable
  ones at the start of the next turn (0x645018).
- `ProcessDeadList` 0x56FB10 / `GameThing::ProcessDead` 0x56FAA0: from the head (the last marked first), each thing
  marked before this pass; the first pass only notes it (+0xA bit 1), the next one frees it (`Delete`, vt +8: `delete
  this`, or the class's own, such as `Abode::Delete` 0x402C10). The turn calls it
  with 0 at 0x54E704; with 1 (`GGame::Close` 0x54EC80, `ClearMap` 0x552E58, `LoadAllGame` 0x558A9B) it frees them
  all, again until the list is empty.
- The classes' own parts (vt +0xC): `Villager::ToBeDeleted` 0x7521B0 (`DeleteDependancys` 0x74FD60, then
  `Living::ToBeDeleted` 0x5EC0A0's StopReacting); `Animal::ToBeDeleted` 0x417B60 → `DeleteDependancys` 0x417BA0 (out
  of its town's list, town = 0, then out of its flock; prey and hunter links stay, their readers ask IsAvailable);
  `Tree::ToBeDeleted` 0x74A210 (out of its forest, fn_0053A220, and of the tree list g_game+0x205CDC);
  `ScriptHighlight::ToBeDeleted` 0x709980 (out of the highlights' list, its two effects closed); `Object::ToBeDeleted`
  0x636670, whose fire (+0x44) is deleted after the mark (0x63667D..0x63668B); then `CleanupWhenDeleted` 0x6377F0
  takes the object out of its cells (vt +0x548).
- Run in the exe on fake things (oracle, GameThing's vtable, its Delete stubbed): T0, T1, T1 again, T2 marked give the
  list T2, T1, T0 (count 3, the second T1 not added); the first pass frees none and notes all three; T3 marked then
  goes to the head; the second pass frees T2, T1, T0 and notes T3; the third frees T3. `now` frees at once and leaves
  the list alone; a draining pass frees T2, T1, T0 at once.
- Who asks for a deletion: of the 527 `call [reg+0xC]` sites, about 360 push 0 (directly or through a register
  cleared before), the class overrides pass their own argument on, and no game code was found passing 1 (read by the
  instructions before each call; sites with no argument are other methods in the same slot). So in the game every
  thing deleted waits on the list; only the dead list's own pass, `GGame::Close`, `ClearMap` and the loading free at
  once. Among them: `Fragment::ProcessTimer`, `Fragment::EndPhysics`, `Object::DestroyedByEffect`,
  `Pot::JustRemoveResource`, `Rock::SplitInTwo`, `Living::HasSunk`, `Villager::Dead`, `Spell::ProcessSpells`,
  `PhysicsObject::GameTurnUpdate`, `ToBeDeletedAvailable` of the influence rings and the big forests.
- While it waits a thing is out of its cells (`CleanupWhenDeleted`), a mobile object is out of the mobile objects'
  list and so out of `AddMobileObjectCheckSum` (`MobileObject::ToBeDeleted`, before the mark), `GGame::EndTurn` drops
  it from the landscape's draw list (bw1-decomp Game.cpp 2563-2572), its physics body is stepped until the next turn
  starts; the other readers ask `IsAvailable`.

#### The first clip of a new being

Every `Create` calls `CallVirtualFunctionsForCreation` (vt +0x658) right after the constructor; for a Living it is
0x5EC9B0 → `Object::CallVirtualFunctionsForCreation` 0x636BE0 (the 3D object, `InsertMapObject`) and then vt +0x8FC(1)
= `Living::SetAnim(int)` 0x5ECB80 = `SetAnim(GetAnimId() (vt +0x900), 1)` (vt +0x8F8, 0x5ECBA0):

- A negative id sets nothing; the pack entry (id ≥ count → entry 0) is set only if it is not the 3D object's clip
  already, and then, with the flag and not `IsDancing` (vt +0x978), its time goes to 0.
- The birds (Dove, SpellDove, Crow, Swallow, Pigeon, Seagull, Bat, SpellBat, Vulture) have `Dove::SetAnim(int)`
  0x41E7E0 = `Living::SetAnim(int)` with flag 0: their first clip keeps its time.
- The id comes from the state the constructor set: a villager 85 CREATED (table clip 385 P_STAND) or 16 DROWNING on
  water (table clip 252 P_DROWNING), with no animation function in either row (`VillagerStateTableEntry::StateTable`
  +0x60 null; the static initialiser stores 0 at 0x59E0DE and 0x5A245B), after `SetStateCarriedObject` (0x750133); an
  animal 43 DECIDE_WHAT_TO_DO (`Animal::Animal` 0x416F24), whose slot is the thunk 0x5AFE50 → vt +0xC20
  `DecideAnimation`: `Animal`'s 0x417810 → vt +0x854 the species' stand clip, no draw; `Dove`'s 0x41BD20 → vt +0xBE4
  the bird's move clip: GameRand(2) for the dove (8/9, 0x41BD39), crow (3/4, 0x41BEC9), pigeon (13/14, 0x41BF39) and
  seagull (18/20, 0x41BFA9), GameRand(3) for the swallow (0 → 27, 1 → 26, 2 → 25, 0x41C019), 23 for the spell dove, 1
  for the bats, −1 for the vulture (oracle, six seeds each).
- So the only draw is a bird's, on the synchronised stream, inside the creation: after the constructor's draws (age,
  scale) and before whatever the creator does next. Every making goes this way: `Villager::Create` 0x74FBE0 (scripts,
  `PopulateTown`, `ChildBorn`, the vortex's `ProcessContentsOfVortex`, `CreateRandomVillagerOfTribe`), the animals'
  `Create`s through the class factory fn_00419E00. Loading a saved game uses the default constructors (not read
  further).

### The interface: frames send packets, turns apply them

- The interface's message pump and action state machine (fn_005D9A20 → fn_005D9BC0 → fn_005D1120
  InterfaceActionProcess) run **once per frame** (`ProcessFrameUpdates` 0x5CEDB0 is `jmp 0x5D9A20`, from
  `ProcessFrameInputs` at 0x54C3B6) **and once per turn** (`GInterface::Process` at 0x5CEC1F). The queue (+0x43E) is
  emptied after each pump, so a mouse message is handled once, almost always in the frame pass.
- They do not change the game: they send packets (0x13 pick up, 0x12 / 0x4D throw, 0x20 tap, 0x1B / 0x1C locked
  select, 0x1F give, 0x6A power-up, 0x65 a leash picked at the temple's posts
  ([The temple's leash posts](../creature/temple-leash-posts.md#the-temples-leash-posts)); 0x15 hand position,
  0x16 / 0x17 camera from fn_005D2250 per turn), applied by `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420
  at the start of a later turn.
- The hand's per-turn game writers: `GInterfaceStatus::Process` 0x5DC4E0 (the multi pick-up transfer,
  `ProcessInInteract` 0x66E520) and `CHand::GameTurnUpdate` 0x46E4E0. `CHand::GameTurnUpdate` = the HandStateGrain
  raise (`fn_005B2D70`), `CHand::ThrowObject` when the held object is no longer available, +0x490C = GetHoldType, the
  +0x48FC clean-up (+0x49B4 / +0x4950 not read), then the hold (0x46E505..0x46E557). The held object's
  `ProcessInHand` runs at the start of the turn instead (`GInterfaceStatus::Process` 0x5DC4E0 → `ProcessHands`
  0x5DC6A0).
- The packets: `GGame::SendPacketCompressed` 0x551690 writes into the buffer g_game +0x5318, which is only flushed at
  6 packets (0x551E0A). The loop flushes it once per iteration after the turns (fn_005525E0 at 0x54D291 →
  `LHSession::Write`). The next turn applies it first (ProcessGameInputs 0x54C3D0 → `ProcessOneSuperpacket` 0x63C3D0 →
  0x63C420): nothing is applied in the turn that sent it.
- Per frame without game writes: hand placement and spring (`HandStateHolding::Update` 0x5B3C70), the held object's
  pose (`CHand::UpdateHeldObject` 0x46E490), PreDrawProcess (`UpdateHandRenderCollide` 0x5D0610, disciple icon), and
  PostDrawProcess (leashes 0x5D9130, `UpdateInterfaceCollide` 0x5D5A70).
- The hand demos (fn_005DAEE0) play inside the pump, in both passes. During a hand demo, `GCamera::Update` skips its
  own update [0x442614]: the recording sets the camera with `GCamera::SetPositionAndFocus` 0x4438C0 and
  `LH3DTech::UpdateCamera` 0x819920, and it is the one in charge. The demo runs in the turn (via `ProcessGameInputs` →
  `GInterface::Process`), before the frame's graphics, so the spirits are placed (fn_005C3A30) and drawn with that
  camera.
- No synchronised random number is drawn in the hand and interface code (CHand 0x46B000-0x46F000, hand states
  0x5B0000-0x5BA000, GInterface 0x5CE000-0x5DD000, gestures 0x578000-0x57D000).

The draw writes a few game fields itself: the whale's heading (fn_00774E30, `mov [ebx+0x6c]` at 0x774FF2) and the
fish bait (fn_00824B90: +0x20 / +0x24 / +0x18 / +0x8C).

### Game statistics (GameStats)

`EndTurn` feeds it ([The turn, call by call](#the-turn-call-by-call)).

- One GameStats per player: GPlayer +0xA44, a GameThing of 0x1128 bytes, vtable 0x8FFDC0. It holds the data of the
  Statistics page of the in-game menu (StatsBox; a snapshot taken by fn_00566BC0).
- `GameStats::Init` 0x564B40(player): +0x1068 = `GPlayer::GetAlignmentValue` [0x564B50, stored at 0x564B55];
  +0x107C = creature (+0xA4C) +0x168 +8, or 0 without a creature [0x564B5B..0x564B7B]; [0xD0604C] = `_time()`
  [0x564B76..0x564B81] ("Time Played"); [0xD06050] = how many towns there are in GameLists.TownList (g_game+0x205C88,
  the `inc [esi+4]` of the Town ctor 0x739656) [0x564B86..0x564B96].
- `GameStats::ClearAll` 0x564D90 (from `GGame::ClearMap` 0x552E67): for each player and the neutral
  (`GetNextPlayerAndNeutral` 0x550980), `GameStats::SetToZero` 0x564BA0 on its stats (+0xA44), then the stats'
  `SetPlayer` (vt +0x20) with that player [0x564DA4..0x564DCB]. `SetToZero` (bw1-decomp agrees; xref note #965):
  - +0x14, +0x18, +0x1C and +0x48..+0x58 = 0; +0x5C, +0x60 and +0x64 (the three minimums) = −1; +0x68..+0xA8 = 0
    [0x564BA3..0x564C1B];
  - +0x1064..+0x1070 = 0, +0x1074 (the lowest world belief) = `_MaxFloat` [0x8FFDA8] (FLT_MAX), +0x1078..+0x1080 = 0
    [0x564C21..0x564C50];
  - +0x1090..+0x1124 = 0: the miracles cast, the chants used and `TotalNumSacrifices` [0x564C74..0x564D52];
  - the town totals +0x20..+0x44 (`TownStatsStructure::SetToZero` 0x564D70, called at 0x564D5B);
  - the statics: `TotalLinesOfCodeExecuted` [0xD0603C], `MaxFrameRate` [0xD06040], `MinFrameRate` [0xD06044],
    `NumBaseAllocations` [0xD06048], `StartTime` [0xD0604C] [0x564C56..0x564C6E], `NumPlayersThatHaveLeftTheGame`
    [0xD06054] [0x564BA6] and `NumTownsAtStart` [0xD06050] [0x564D60] = 0.
  - It leaves alone only the two histories (+0xAC..+0x1063) and +0x1084..+0x108C. The version (a constant 113) is
    never written.
  - openblack's `game_stats::ClearAll` does the same clear: every counter to its start (the minimums −1, the lowest
    belief FLT_MAX) and the statics, keeping the histories and the version. The miracles cast and the chants used
    live in openblack's `PlayerMagic`, which each land's new player entity starts at 0 (`PlayerArchetype::Create`).
    One harmless extra: it also clears `u1084` (+0x1084..+0x108C), which the exe leaves; nothing in openblack
    writes `u1084` (only `Serialize` reads it), so it stays 0 either way.
- Histories: two adaptive histories of 0x7DC bytes, influence at +0xAC (`GPlayer::CalculateInfluencePower` 0x64ADA3)
  and population at +0x888 (`GPlayer::Process` 0x649599); ctor 0x564A40. Each has 500 buckets, and each bucket is the
  mean of `period` samples. When the 500 are full, neighbouring buckets are averaged in pairs into the first half
  [0x64960D..0x649641] and the period doubles (`GPlayer::Process` 0x6495A5..0x64966D).
- Original bug that is kept: when emptying, `count` **bytes** are cleared starting at bucket[count] (`rep stosd`
  count/4 and `rep stosb` count&3), not `count` buckets [0x649643..0x64965A].
- +0x20..+0x44: sums over the player's towns stored by fn_00564DF0; the exe recalculates them every turn. Among them,
  the wonders = Town::stats.abodesByNumber[10] (`AbodeNumber::Wonder`, 10; `TownStats::Add` 0x74997C..0x749988; on removal, fn_00749990
  [0x749A45..0x749A56]).
- `AddToTotalLinesOfCodeExecuted` 0x565110: [0xD0603C] += ftol(LocalRand(ftol(v·10130·0.2)) + v·10130), with v =
  [0xD06050] and the doubles 10130 [0x8FFEC0] and 0.2 [0x8C7C68] [0x565113..0x56515C]. It is called by `EndTurn` and
  fn_00566890 (the Statistics page, once). "Total Lines of Code Executed".
- Frames per second (`GGame::EndTurn` 0x54EB0C..0x54EB39): g_frame_rate_stats [0xC386E0] goes into the maximum [0xD06040]
  or, otherwise, into the minimum [0xD06044]. A new maximum skips the minimum, and the minimum starts at 0, so it never
  changes.
- Objects created [0xD06048] (`GameStats::NumBaseAllocations`) += 1 in fn_00436870, which `Base::operator new`
  0x4366F0 calls on every allocation, unconditionally [0x4368CD..0x4368D4]; zeroed only by `GameStats::SetToZero`
  0x564BA0 (from the constructor and `ClearAll`, at a land's start) and read only by `GetPackage` 0x566905 ("Total Objects
  Created"). So it counts every object of the game's heap made since the land started: beliefs, agendas, footpaths,
  attribute tests and file objects as well as the things on the land (992 call sites of the allocator). Version [0xBEE568] =
  113, a constant nobody writes.
- Final places: [0xD06054] = places counted; +0x18 place and +0x1C value (2nd argument), both from fn_0056A6D0;
  +0x1064 [0x56A724]. bw1-decomp names them (`GameStats.h:222..264`, `GameStats::PlayerLostTheGame` 0x56A6D0):
  `finishPlace` is `Position`, the order in which the player left the game (`++NumPlayersThatHaveLeftTheGame`
  [0xD06054] when a human player loses); `finishValue` is `LossReason`, the `reason` argument; `playersOutBefore` is
  `NumGodsDefeated`, the gods this player has defeated (each other active human still in the game gets +1 when one
  loses; in 1.20, if the loser is human or it is a skirmish game, which v1.42 was not read for); `maxPopulation` is
  `MaxTotalPopulation`, the most villagers at once. `u1084` (`field_0x1084/0x1088/0x108C`) is unnamed in bw1-decomp
  too.
- Births +0x48 (`ChildBorn` [0x7623E5]). Deaths +0x4C and villagers killed +0x106C: fn_0073E440 [0x73E46A..0x73E479;
  the death at 0x73E470, the killer at 0x73E479] adds one to the town's player and another to the killer (3rd
  argument, not checked for null).
- Population: `GPlayer::Process` 0x649594 → fn_0056A320(men, women), summed over the player's towns (TownStats +0x54
  / +0x58). Keeps maximum and minimum of the sum (+0x50 / +0x5C, fn_0056A350), of the men (+0x54, fn_0056A370, town
  +0x664, `TownStats::males`) and of the women (+0x58, fn_0056A390, town +0x668). Uses `ja`, and otherwise goes to the minimum: a new
  maximum skips the minimum.
- Town taken over: `GPlayer::TakeOverTown` fn_00649810 adds to the new owner's +0x68 the town's adults +0x618 plus
  children +0x61C [0x6498F7..0x649910; the sum at 0x649909].
- Desire: +0x6C sum (`TownDesire::Process` 0x745C43) and +0x70 count [0x745C30]; the row shows +0x6C / +0x70
  [0x566C60].
- Buildings: `IncrementAllBuildingsBuilt` 0x56A3F0 adds one to +0x74 [0x56A3F3..0x56A3FC] and another to +0x78 /
  +0x7C / +0x80 according to the first set type bit of the abode, 2 / 4 / 0x100 [0x56A409, 0x56A423, 0x56A43D]. The
  type is given by `Abode::GetAbodeType` 0x4061F0 (vt +0x8C4, the info's ABODE_TYPE).
- Disciples: fn_0056A450 (from `Villager::Landed` 0x7607DF), table 0x56A4AC: VILLAGER_DISCIPLE 1..5 → +0x84..+0x94,
  6 is not counted, 7 → +0x9C, 8 → +0xA0; nobody writes +0x98.
- Food eaten +0xA4 (`Town::UseFood` 0x73B60F); wood used +0xA8 (fn_0073B620 [0x73B64F]).
- World belief: fn_0056A3B0 (from TownBelief [0x438177..0x438187], the player's summed belief). If it exceeds the
  maximum +0x1070, it is the new maximum and the minimum is skipped (`test ah, 0x41` / `jne 0x56A3CE`)
  [0x56A3B4..0x56A3BF]. Otherwise, if it is below the minimum +0x1074 (starts at FLT_MAX [0x8FFDA8]), it is the new
  minimum [0x56A3D2..0x56A3DD].
- Creature value +0x1078 (`Creature::ProcessState` 0x472E49).
- Buildings that stop working +0x1080: `Abode::StopBeingFunctional` 0x4073D1..0x4073E0 (the add at 0x4073D8..0x4073E0); the
  caller checks the player and that +0xB9 ≥ 200.
- +0x1124 `TotalNumSacrifices` (bw1-decomp `GameStats.h:301`, the last field; saved as "TotalNumSacrifices=%ld;"):
  ++ on the sacrificing player's stats when an animal is sacrificed (`Animal::ApplyThisToObject` 0x41B3F4, the `inc` in 0x41B3E4..0x41B3F4,
  right before `GAlignment::Update(player, animal, 7)` 0x41B3FE) or a villager dies by sacrifice (`VillagerDead`
  0x75090E `cmp ebx, 7`, 0x75091D; 7 = `DEATH_REASON_SACRIFICE`) [V]. Also cleared by `GameStats::SetToZero`
  0x564D52 and copied by `FillInGameStatsPerPlayer` 0x566FBC.
- Miracles cast: fn_0056A4D0 (from `Spell::InitWithPos` 0x71FE9B). MagicType 1..35 → +0x1090 + 4·(type − 1), 41 →
  +0x111C; for the rest the jump table 0x56A62C counts nothing.
- Saving: `GameStats::Save` 0x565E70 comes after the GameThing header and the player (0x56FBE0, 0x561E10). It writes
  the fields as 4-byte values, +0x20 as 0x28 bytes, each history as 0x7DC bytes (fn_0056A890), and the GameStats
  statics in place, 4 bytes each (`GameOSFile::WriteIt(long&)` 0x4E94F0 included).

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to
quote are kept here, next to the openblack symbol each one corresponds to. These rows are the script DLL's
(`ScriptLibraryR.dll`), which the turn reaches through `GScript::Process`.

| Address or name | What it is | openblack |
|---|---|---|
| `[0x1001F140]` | the script DLL's float constant the product is multiplied by before the fstp | `LHVM (float rounding)` |
| `ScriptDLL::LookIn 0x6F6840` | the exe's entry into the script DLL, called by GScript::Process on the game thread | `LHVM (script step)` |
| `ScriptLibraryR.dll TaskNumber 0x100025A0` | the script DLL's export that returns the id of the task running now | `LHVM::GetCurrentTaskNumber` |
| `GetCurrentScriptType 0x10002630` | the script DLL's export that returns the type of the task running now | `LHVM::GetCurrentTaskScriptType` |
| `GetScriptType 0x10002650` | the script DLL's export that returns the type of a task by its number | `LHVM::GetTaskScriptType` |
| `0x10008217` | the end of the script DLL's LookIn, where the tick count goes up by one | `LHVM::GetTicksCount` |
| `DLL_GETTIME handler 0x1000ABD0` | the script DLL's elapsed time: the tick count [0x1003BDB4] times 0.1f, pushed as type 2 | `LHVM::PushElaspedTime` |
| `TaskNumber -> 0x10008320, ScriptDLL::TaskNumber 0x6F69F0` | the DLL routine behind TaskNumber (reads the task's +0x14) and the exe's wrapper | `LHVM::GetCurrentTaskNumber` |
| `ScriptDLL::GetCurrentTaskScriptType 0x6F6A90` | the exe's wrapper of GetCurrentScriptType (reads the task's +0x158) | `LHVM::GetCurrentTaskScriptType` |
| `GetScriptType -> 0x100051F0, ScriptDLL::GetScriptType 0x6F6C50` | the DLL routine behind GetScriptType (the task's +0x158) and the exe's wrapper | `LHVM::GetTaskScriptType` |
| `GScript::Process 0x6EB6B0, fn_007DEE00` | the exe's script step on the game thread, and the routine that sets the FPU to 24 bits | `LHVM::GetTicksCount` |
| `LookIn 0x10008060..0x1000822F` (export 0x10002350) | the script DLL's turn: for each task it sets the current stack `[0x1002160C]` = task +0x28 and the current handler list `[0x1003BC40]` = task +0x138 (0x100080B6..0x100080C4), then a task not waiting (+0x20, 0x100080D4) and not in a handler starts its handlers: index +0x134 = 0, and if the list is not empty, +0x1C = ip, ip = handler(0), +0x14C = 1, CpuLoop | `LHVM::LookIn` |
| `0x1000A2C0`, `0x1000A2E0` | the handler count (of `[0x1003BC40]`, the iterated task's own list, not the running task `[0x1003BDC8]`) and handler(index) = `first[count − index − 1]`, innermost first | `LHVM::GetExceptionHandlersCount(task)`, `GetCurrentExceptionHandlerIp(task, index)` |
| `EXCEPT 0x10009FB0`, `RETEXCEPT 0x10009FC0`, `ITEREXCEPT 0x10009FE0`, `BRKEXCEPT 0x1000A030`, `ENDEXCEPT 0x1000A060` | the handler opcodes (22, 26, 27, 28, 25): push a handler; next handler or back to pev; BRKEXCEPT pops index + 1 handlers (0x1000A2A0), so the handlers outside the loop stay | `LHVM::Opcode22Except` … `Opcode28BrkExcept` (pops the index + 1 innermost) |
| `INTCAST opcode 23 = 0x10008EE0, switch table 0x100090E4` | the script DLL's cast, a switch on type - 1 | `LHVM (Mode::CAST)` |
| `0x10008F0A, __ftol 0x1001568C` | the cast to int: fistp qword with truncation, the low dword | `LHVM (Mode::CAST, Int)` |
| `0x10008F6F` | the cast to float: fild qword with a zero high dword | `LHVM (Mode::CAST, Float)` |
| `0x100090DE` | the cast's case for type 5: nothing | `LHVM (Mode::CAST, Unk5)` |
| `0x10008F32, 0x10008FA2, 0x10008F57` | the cast's cases for vector, object and boolean: retag only | `LHVM (Mode::CAST, default)` |
| `LookIn pass 3, 0x1000818F..0x10008212` | the end of the step, one walk in list order: a task whose stop flag (+0x14D) is set goes to StopTask 0x100065B0 (which unlinks only its own node, 0x10006612..0x10006656), then esi = 0 and the walk starts again from the head (0x100081D0..0x100081DB); any other task of the step's types gets ticks (+0x148) + 1 and, if it waits (+0x20) for a task that no longer exists (0x10006320), wait = 0 and ip (+0x18) + 1. So a task's ticks go up by 1 + the number of tasks later in the list that end that turn | `LHVM::LookIn` (the end-of-step pass) |
| `0x10006011..0x1000602A` | a new task's node is appended at the tail of `[0x1003BD58]`, so the list is in creation order, as openblack's task map ordered by id | `LHVM::StartScript` |
| `LookIn pass 2, 0x10008129..0x1000818D` | the normal pass: after each task's CpuLoop it reloads the list head (0x10008184) and finds the next node from the current one, so a task started during the pass, appended at the tail, runs in the same pass and turn, up to its first yield (a script's `start` compiles to a YIELD after its local initialisers). Read with capstone from `Plug Ins/ScriptLibraryR.dll`, 2026-10-29 | `LHVM::LookIn` (the task map ordered by id reaches a task added during the walk the same way) |

### Sources

- `runblack.exe` (disassembly and the xref index's callers); bw1-decomp `src/Black/Game.cpp` 1585-1620, 1704-1788,
  1902-1984, 2072-2101, 2105-2120, 2438-2527; bw1-decomp `src/Black/GameThing.cpp:55-131`.
- The oracle for the dead list's order and the birds' first clips.

## openblack

### openblack's loop

`Game::Update` (src/Game.cpp) per frame: input and events, ImGui, `while (game_clock::TurnDue()) GameLogicLoop();`,
the frame clock, the camera after the turns as in the original (`Step::Camera` after `Step::FrameTime` in
`src/GameFrameOrder.h`), the per-frame updaters (physics interpolation, fields, trees, fireflies, rings, scenery, mobiles, animations, sharks,
fish), the hand, `RenderingSystem::PrepareDraw`, audio; then `Game::Run` draws (`DrawScene`) and calls `bgfx::frame`.
bgfx is built multithreaded (vcpkg feature `multithreaded` → `BGFX_CONFIG_MULTITHREADED`): its render thread is
already separate; openblack's main thread is the bgfx API thread.

The turn's steps reach their systems through Locator services and one-way notifications go through `Locator::events`,
never globals; the clock and the random streams are pure modules that tests drive with fakes in `test/`
([openblack internals](../project/internals.md)).

Differences from the original's loop:

- **The turn order.** The turn steps follow [The turn, call by call](#the-turn-call-by-call). They are a table,
  `game_turn::k_Turn` in `src/GameTurnOrder.h` (raffclar's design: each step with the stage of the game it does, and a
  check that no step comes before an earlier stage), that `Game::GameLogicLoop` plays through `Game::PlayTurnStep`;
  `test_game_turn_order` pins the order. Several steps do what the game splits into more (the turn start, the living
  with the creatures after the list, the scripts with the help and the time of day); what is not ported is listed in
  Pending.
- **The frame order.** `Game::Update` plays the frame's table, `game_frame::k_Frame` in `src/GameFrameOrder.h`
  (raffclar's design, with the game's stages of a frame), through `Game::PlayFrameStep`; a step that returns false
  ends the frame, as the game's stop and the debug menu's Exit do. Where openblack's frame is out of the game's stages
  (the menus after the camera, the object draw list and the help's Draw3D before the hand) the step has no stage and
  a note. `test_game_frame_order` pins the order.
- **The hand** sends its actions (pick-up, release, throw, taps, applies, the seed's unlock) as game packets
  (`Input/GamePackets`): the loop flushes them once after the turns (`game_packets::Flush` in `Game::Update`) and the
  next turn applies them first thing (`game_packets::DispatchQueuedPackets` at the start of `Game::GameLogicLoop`),
  through the handlers in `ECS/Systems/Implementations/HandTurn.cpp`
  ([The interface](#the-interface-frames-send-packets-turns-apply-them),
  [Grab, hold and throw](../hand/grab-hold-and-throw.md#openblack)).
- **The first animation clip** of a new villager or animal is set as it is made, as the original's creation does
  ([The first clip of a new being](#the-first-clip-of-a-new-being)): the archetypes call `ecs::SetFirstAnimalClip`
  (`AnimalArchetype.cpp`) and `ecs::villager_animate::SetFirstClip` (`VillagerArchetype.cpp`), over the pure choice
  `ecs::living::AnimalFirstClip` / `VillagerFirstClip` / `BirdMoveClip` (`ECS/LivingFirstClip.h`,
  `test/test_living_first_clip.cpp`, pinned to the oracle); a bird's coin is drawn in its creation. An animal made
  outside the archetype still takes its first clip in its first animation step.
- **The dead list.** openblack has it (`ecs::ToBeDeleted` / `ecs::ProcessDeadList`, through the `ToBeDeletedSystem`
  service). The turn runs its pass, `ProcessDeadList(false)`, in the original's slot (after the hand's turn, before
  the rewards; the players' sparkles and the mobile checksum before it are not ported); the map's clear drains it
  before the registry is reset, and the game's close before the services go. The deferral stays off until every
  owner's readers ask `IsAvailable`, so an entity is still destroyed at once and the list stays empty **(pending)**.
  `ecs::DeadList` (src/ECS/DeadList.h, test/test_dead_list.cpp) keeps the original's rules with no registry, and the
  game's dead list (`ToBeDeletedSystem`) is one of them, of entities (test/test_villager_deadlist.cpp). In
  `src/ECS/ToBeDeleted.cpp`, `DeleteFire` does the object's part (its fire deleted and unlinked, as
  `Object::ToBeDeleted`); an entity deleted at once leaves the physics first (`DynamicsSystem::RemoveObject` would put a
  still valid entity back in its cells), then its cells. openblack has no per-class `Delete`: the entity is destroyed
  **⚠️ approximate**.
- **The hand before the landscape.** `ecs::UpdateTreeBends` runs in `Game::Update` after `HandSystem::Update`,
  `HandSystem::Place`, the influence crossings and the leash, and before `RenderingSystem::PrepareDraw`, as the
  original's landscape draw fills its bend table after `CHand::PrepareForDrawing`; the trees' brightness and rustle
  (`ecs::UpdateTrees`, with its `LocalRand` draws) stay with the frame updaters, before the hand.
- **The window's minimise** holds the loop as in the original (issue #9): `Game::Update`, right after it polls the
  events, waits for the window's events while `Windowing/WindowAway.h` says it is away (minimised, until restored or
  quit), emptying the queue before each check. The game clock is not stopped, so the way back goes through the turn
  scheduler's lag rule as in the original. The wait is a blocking `SDL_WaitEvent`, where the original spins on
  `PeekMessage`; the game does the same either way. A run with a fixed frame time is not moved by a minimise either:
  its ticks only advance per frame. The openblack-only delta time from the profiler sees the time away in the first
  frame after the restore, as the original's wall clock does. openblack has no separate guard for `IsAppMinimized`.
- **The hand demo** runs once per frame, before `Draw3D`; the original runs it in both passes, and the frame pass's
  one reaches the screen one frame later **⚠️ approximate**.
- **Vertical sync** is on by default, as the original's: its screen flips with flip flags 1 (wait for the refresh)
  when the setup program's VSync registry value is missing (bw1-decomp `LHScreen.cpp:158-163`), and it is missing in a
  plain install, so the original runs at the monitor's rate ([the frame rate's cap](#the-frame-rates-cap): no other
  limiter, so openblack's loop has none either). `--no-vsync` draws as fast as possible (the verification
  runs use it; their frame step is fixed, so the game is the same). Every "at least once a frame" step depends on it:
  the leash rope takes 1 + trunc(200·dt) steps of 0.005 s a frame, so at hundreds of frames a second it ran several
  times faster than real time (issue #135).
- **The intro light** (fn_005DF640, and the call of its Z object 0x8283D0) draws its numbers from the CRT `Random`
  when the Z queue is flushed, after the clouds, the night lights and the smoke; openblack draws them after updating
  the ship **⚠️ approximate**.

### Game statistics in openblack

`src/Game/GameStats.{h,cpp}`, namespace `openblack::game_stats`, reached through the `GameStatsSystem` service
(`ECS/Systems/GameStatsSystemInterface.h`); test `test/test_game_stats.cpp`. Every field above is kept with its offset's meaning, and the owners' code calls one
function where the exe writes a field: `Init`, `ClearAll`, `AddToTotalLinesOfCodeExecuted` (given the local stream's
`LocalRand`; at the turn's end, below), `TrackFrameRate` (at the turn's end), `ChildBorn`, `VillagerDied`, `TownTakenOver`, `TownDesire`, `BuildingBuilt`,
`FoodEaten`, `WoodUsed`, `BuildingStoppedFunctional`, `Sacrificed`, `WorldBelief`, `DiscipleMade`, `TrackPopulation`, `SpellsCast` (read from
`PlayerMagic::castCount`, filtered to the types the exe's jump table counts) and `Serialize` (in `GameStats::Save`'s
order). The town totals +0x20..+0x44 are not stored: `TownTotals` recomputes them from the player's towns, as the exe
does each turn. The kept original bug in `History::Add` (the `count` bytes cleared) is reproduced. Not ported: the
creature value +0x1078; and nothing calls `Serialize` yet: a save writes the statistics' object from the record its
load kept ([Writing the world file](saved-games.md#writing-the-world-file)).

- The step every 10 turns (ours, from the exe): last of the turn's end
  (`src/Game.cpp`, after the reactions' end of turn), when `turn % game_stats::UpdateInterval(paused)` is 0 (10, or 5
  when the turn ends paused, as `GGame::EndTurn` halves `GameStats::UpdateInterval` on `GAME_FLAG_PAUSED`, 0x54EAE9 /
  0x54EAF5), `AddToTotalLinesOfCodeExecuted(&game_random::LocalRand)` then `TrackFrameRate` with the frame rate of
  `statistics::FrameRateMeter` (raffclar's, fed once a frame from the machine clock in `Game::Run`). The town count
  it multiplies is the game's first land's (`player_statistics::Start`); every later land's `ClearAll` zeroes it, and
  `LocalRand(0)` draws nothing, so from the second land on nothing is drawn. The order among the other end-of-turn
  calls is unverified (`xref.py show 0x54EB07` at the audit).
- "Total Objects Created" is every entity the registry made since its last reset (`Registry::GetEntitiesMade`,
  raffclar's dev#60: EnTT entities, not the exe's object count). The original's count has no equivalent here: openblack
  keeps beliefs, agendas, footpaths and the rest as plain data, not as allocations of one heap, so the figure stays
  the registry's until those are counted where the original allocates them. The same count is a debug readout (the
  World menu, "Entities made since the land started"); the unused `ObjectCreated` counter is gone and
  `Serialize` writes the registry's count in that slot.
- The Game Statistics box (raffclar's PR #525, unverified against v1.42): `gui::StatisticsBox`, opened by the main
  menu's Statistics tab (`GameMenu::OpenStatistics`), its figures from `player_statistics::Box()` over `game_stats::`
  (taking them steps the lines of code once more, as the exe's box does). Time Played is `turn / 10`, the exe's
  `GameStats::GetTimePlayed`. "Script VM Instructions" is the script library's own count (`GetScriptInstructionCount`
  in `Plug Ins\ScriptLibraryR.dll` returns [0x1003BDC4]; its CPU loop at 0x10008230 adds one before each
  instruction it runs while the task is not waiting, the one that stops the task or ends its turn included), as
  openblack's `LHVM::CpuLoop`; test `LhvmInstructions.EachInstructionRunCountsOnceTheOneThatEndsTheTurnToo`. The gods
  defeated read 0 (not counted yet). The creature views on the Creature tab ask
  `RendererInterface::ShowInterfaceCreatures`, which draws nothing yet (the render pass is not written).
- The creature's tallies (`ecs::components::CreatureTallies`, raffclar's #507/#525): the people, animals and
  creatures killed by eating them, counted at our `EndMeal`; the poos, one per poo; the mushrooms eaten. The temple's
  creature scroll and the box read them through `player_statistics::CreatureTalliesOf`.
- The feeders and the turn (raffclar's code, unverified against v1.42): `ChildBorn` (`VillagerBirth.cpp`),
  `VillagerDied` and `Sacrificed` (`VillagerDeath.cpp`), `WoodUsed` (`VillagerBuild.cpp`); `Sacrificed` also for an
  animal given to an altar, by its giver, before its alignment, as `Animal::ApplyThisToObject` 0x41B3F4
  (`altar_offering::Offer`; test `VillagerDeathTest.AnAnimalSacrificedAtTheAltarCountsOneSacrificeForItsGiverAndATreeNone`,
  [a sacrifice at the altar](../magic/worship.md#a-sacrifice-at-the-altar)); `ecs::player_statistics`
  (`src/ECS/Statistics.{h,cpp}`): `ProcessTurn` after the players' influence power (`Magic/MagicLoop.cpp`: the
  population extremes, both histories and the creature's size the first time it is seen), `Start` on the game's first
  land after its map script (`Init` for each player) and `World` for the temple's world scroll, over his pure
  `ECS/StatisticsRules`. **⚠️ approximate** both histories take their sample in the one step, where the original adds
  the influence one in `GPlayer::CalculateInfluencePower` and the population one in `GPlayer::Process`. Debug: the
  temple window's Statistics header; testbed `statistics.counters`. `playersOutBefore` (`src/Game/GameStats.h`) is
  `NumGodsDefeated`, a rename.
- The tip bubble: `FillOverlayFrame` builds the same three parts (`help::BubbleParts`, in `src/Game.cpp`).

### Threads (plan)

User decision (2026-10-03): the game logic stays exactly the original's (turns, order, random numbers); the drawing
may run on its own thread, and **one frame of display latency is accepted by default**: a presentation difference,
not a logic one. A single-thread mode without latency stays for comparison and for the replay test.

Design "level 1": one logic thread runs every frame's game code in today's order (input, turns, frame updaters, hand,
the draw-side writers moved out of DrawScene); it publishes an immutable snapshot of what the draw reads; the main
thread (window, bgfx API) encodes the previous frame's snapshot. Turns in parallel with frames ("level 2") cannot be
identical, because the draw shares the random streams with the turn
([Random streams between turn and frame](random.md#random-streams-between-turn-and-frame)) **(pending)**.

### Proving two runs are the same game

- `OPENBLACK_STATE_HASH=<file>` (src/Debug/StateHash.*): at the end of each turn, one line `turn <n> <total>
  random=… crt=… clock=… pools=… transform=…` (FNV-1a over the GRand seeds and the PSys stream; the CRT seed; the
  turn, pause, speed, ms per turn; every registry storage's size and entity order, folded by storage id; every
  Transform's bits). Owners add their parts with `state_hash::Register`. It is not the original's network checksum
  (`AddMobileObjectCheckSum` 0x606FC0 / `SendNetworkChecksum` 0x635210, **not ported**).
- `OPENBLACK_FIXED_FRAME_MS=<ms>` (src/Debug/FixedClock.*): every frame is that many ms (game timer, engine timer,
  audio ticks, the frame's delta), so a run repeats (not covered: the 10 s wall-clock timeout of
  Particles/Rules/Lightning.cpp).
- `test_replay_determinism`: the mock land played twice, headless, with the fixed clock; the per-turn hashes must be
  equal (later: one thread against two).
- `OPENBLACK_PROFILE=<s>` also logs the stages "Frame Updaters", "Turn: Map Rebuild", "Turn: Magic", "Turn:
  Physics", "Turn: Particles" and the memory (RAM working set, peak and private bytes; bgfx texture and render-target
  memory, GPU memory when the backend says, transient buffers).
- How the checks use them: [Testing](../project/testing.md#behaviour-is-identical-the-fidelity-run).

### Test hooks

`OPENBLACK_STATE_HASH`, `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_PROFILE` (with memory), `OPENBLACK_TRACE_GAME_RAND`
([Random numbers](random.md#test-hooks)); headless runs with `-b Noop -n <frames>`. The cross-page list is
[Debugging](../project/debugging.md).

## Pending

- **Not ported yet**: the rest of the turn order
  (the players' sparkles, the mobile checksum), the deferred dead list, the threads, the per-frame registry entities of
  carried props and scenery.
- **The script handlers** (measured with the handlers running): on Land 1 every task with a handler checks it each
  turn from the first (78 tasks in the 900-frame run, 143 by turn 1895: CreatureDevelopmentAutoRun, LandControlAll,
  LandControl1, SetupLand1, the DidYouKnows, PlaySingingCircleNote and others), but no `until` comes true in the
  standard runs (the 900-frame Land 1 and the cycle, the six hand demos, a 12000-frame Land 1 with nobody playing):
  their conditions wait for the player (a click on a highlight, a thrown stone, a dead shepherd). Checking them draws
  no game random number, so those runs are identical with or without the handlers.
- A testbed scenario that runs a small script of its own (compiled with the CHL compiler) to show a loop's `until`
  and a block's handler firing; for now Land 1 shows it.
- The script DLL's LookIn walks its task list `[0x1003BD58]` again from the current node after every handler's
  CpuLoop in its first pass (0x10008124); openblack walks its task map once. Not measured. (The stop pass's restart
  is reproduced: the Addresses table.)
- `GameStats` `u1084` (three words at +0x1084..+0x108C): still unnamed, in bw1-decomp too.
