# Game loop and clock

The game's heartbeat: the world moves on in fixed game turns, ten a second, while the screen is drawn as often as it
can be. A game clock that runs at the game's speed and stops while paused decides when a turn is due; between turns the
frame works out how far through the turn it is. Two random number streams keep the game's rules repeatable.

**Progress: 33/38 done, 0 chosen, 5 partial, 0 inferred — 96%**

How the original does it, in our wiki: [The engine loop: game turns and frames (original and openblack)](../../../engine/loop.md), [Coordinates, terrain, object size, game clock, matrices and Zoomer](../../../engine/game-clock.md).

## Game turns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world moves on in game turns of a tenth of a second | done | `src/GameClock.cpp` (`game_clock`, 100 ms a turn); test `GameClockTest.FirstTurnRightAwayThenEveryHundredMs` |
| A turn is due once the game clock reaches the turn number times the turn's length, so no turn loses what was left of the one before | done | `game_clock::TurnDue`; test `GameClockTest.ThirtyFpsKeepsTheLeftover` |
| At most one turn is played in a frame in a single-player game | done | `Game::Update` plays at most one due turn a frame (`src/Game.cpp`). Network games play several to keep up: see ../multiplayer/; test `GameClockTest.MoreThanTwoSecondsBehindDropsTheLag` |
| When the game falls more than two seconds behind it gives up on the lost time rather than racing to catch up | done | `src/GameClock.cpp`; test `GameClockTest.MoreThanTwoSecondsBehindDropsTheLag` |
| The turn number starts again from 0 as each land starts | done | `game_clock::OnLoad` from `Game::LoadMap`; test `GameClockTest.OnLoadSetsTheVisualClockToTheTurn` |
| Each turn processes the world's parts in the game's fixed order (players, creatures, villagers, scripts, weather, magic …) | partial 80% | the turn's steps are a table, `game_turn::k_Turn` in `src/GameTurnOrder.h`, each step with the stage of the original's turn it does, which `Game::GameLogicLoop` plays through `game_turn::PlayTurn` (tests `GameTurnOrder.EachStepOnce`, `GameTurnOrder.FollowsTheGamesStages`; [the turn order](../../../engine/loop.md#openblacks-loop)); the six steps with no stage, read 2026-11-09 in bw1-decomp's turn (`src/Black/Game.cpp:2378-2465`, the frozen exe's in the same order): the towns' belief comes after the climates and before the hand, the dead list after the hand and before the reward chests, and the influence circles after the confirmations and before the camera's checks, each where ours has it; the other three (the particle and magic test hooks, the audio's test views and the reactions' end-of-turn mark) are openblack's own, with no stage in the original ([the turn order](../../../engine/loop.md#pending)); missing: what of the original's turn is not ported, listed on the loop's page ([pending](../../../engine/loop.md#pending)) |
| The per-frame (drawing-side) updates run in the game's order | partial 70% | the frame updaters run in a fixed order in `Game::Update` (physics interpolation, fields, trees, fireflies, rings, scenery, mobiles, animations, sharks, fish); the camera updates before the turns where the original updates it after |
| Physics is stepped by the game's turns, not by the wall clock | done | the game's own physics runs once a turn (`src/ECS/Physics`, "Turn: Physics" in `Game::GameLogicLoop`); tests `GameTurnOrder.EachStepOnce`, `GameTurnOrder.OpenblacksOrder` |
| The temple has turns of its own while the world outside is stopped | partial 60% | while the world is paused inside the citadel, a temple turn comes every 100 ms of the wall clock (`PausedTurnTimer`, tests `PausedTurnTimer.*`) and does the original's temple turn in its order: the temple's scripts looked in, the audio's turn (`audio::ProcessCitadelTurn`), then the help system's turn, with the room's tooltip (openblack's) before the help's, and nothing when the world is not paused (`temple_turn::Process` in `src/Temple/TempleTurn.cpp`, from `Game::ProcessTempleTurn`; tests `TempleTurn.WhilePausedTheScriptsThenTheAudioThenTheTooltipAndTheHelpTakeTheirTurn`, `TempleTurn.NotPausedTheTempleTurnDoesNothing`; [the world pauses inside the citadel](../../../interface/temple-interior.md#the-citadel-interior-in-openblack)); where the original submits the room's tooltip against the help's turn is not read; the temple's rooms: see ../temple/ |
| A game starts with a short pause before its loop: the logo plays once, then the clock is reset to the current turn | partial 60% | the logo's stills and the pre-intro play before the loop (`Game::PlayStartupScreens`, `src/GameStartup.cpp`; tests `Startup.*` in `test/gui/test_loading_screen_rules.cpp`); the game clock's reset after them read against the exe (2026-11-09): the loop's start takes the timer from 0, plays the logo on its first pass only, then `ResetLocalGameTimer` puts the timer at the turn times 100 from now, so the logo's time does not count; openblack's loop start does the same after the land's load (`game_clock::Start`, `TimeSystem::StartGameClock` then `ResetTimerToTurn`; test `GameClockTest.OnLoadSetsTheVisualClockToTheTurn`; [the game clock](../../../engine/game-clock.md#the-game-timer)); the logo of the loop's first pass ported 2026-11-10 (`Game::PlayInLoopLogo`, `src/GameInLoopLogo.cpp`; rules `in_loop_logo`, drawing `graphics::InLoopLogoDraw`; tests `InLoopLogo.*` in `test/gui/test_in_loop_logo.cpp`; [the in-loop logo](../../../engine/game-clock.md#the-in-loop-logo)): after the land's load and before the first frame, its sound around it, until a key, a click or its end, then the input cleared, the temple fade at rest and the timer reset to the turn; still to do: a frame check against the original's and a run through the real input path (no check run plays it: `--skip-logos`); the Pentium 4 boxes the original draws on CPUs of base family 15 are never shown, a chosen difference (user, 2026-11-10; [chosen differences](../../reconstruction.md#what-is-deliberately-different-chosen-differences), [the in-loop logo](../../../engine/game-clock.md#the-in-loop-logo)); the intro films: see ../story/ |

## Frame clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each frame has a game-time step: how far the game clock moved since the last frame, none while paused | done | `game_clock::UpdateFrameClock`, `FrameGameMs`; test `GameClockTest.RemainderFractionAndFrameMs` |
| Each frame knows how far it is through the current turn (0 to 0.99), so moving things can be drawn between their turn positions | done | `game_clock::TurnFraction` (clamped at 0.99; test `GameClockTest.RemainderClampedAt99AndFrameMsAtMost199`), used to draw villagers and animals between turns (`src/ECS/MobileDrawing`), fire, magic, particles and shields |
| The miracle globes' animation and the particle chains' scroll step by the frame's whole game milliseconds | done | `MiracleFxSystem::UpdateGlobes` and `chain_atoms::AdvanceScroll` from `magic::Update` (`src/Magic/MagicLoop.cpp`) take `game_clock::FrameGameMs()` as it is, not a round trip through seconds ([the game clock](../../../engine/game-clock.md#what-was-fixed)); test `MiracleFxSystemTest.TheGlobesStepByTheFramesWholeGameMillisecondsAsTheyAre` |
| The frame's real time is whole milliseconds of the wall clock and never zero | done | `game_clock::FrameRealMs`; test `GameClockTest.RealClockAndSelectors` |
| The hand and the camera move by real time, except during a script's cut scene, when they keep to game time | done | `game_clock::CameraFrameMs` (`src/GameClock.cpp`); test `GameClockTest.RealClockAndSelectors` |
| While the window is minimised the whole game waits (no turn, no frame, no drawing) until it is restored; the game clock is not stopped, so a long time away is dropped by the turn rule for lag | done | `Game::Update` waits on the window's events while `windowing::WaitsForWindow` (`src/Windowing/WindowAway.h`); tests `WindowAway.*` |
| Quitting is noticed between frames and ends the loop cleanly once help is not mid-sequence | done | `Game::Run` ends on quit; the window's close waits while a script holds the wide screen, as the original's loop (`game_frame::SystemQuitEndsLoop`, taken at the frame's stop check; test `GameFrameOrder.TheSystemsQuitWaitsForTheCinematicToEnd`; readout: the log's "Quit: the window's close waits for the cinematic to end"; [the engine loop](../../../engine/loop.md#the-windows-messages-and-minimising)) |

## Pausing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pausing stops the game clock; unpausing starts it again from where it stopped | done | `game_clock::Pause`; test `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| No turns are played while paused | done | `game_clock::TurnDue` while paused; test `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| Opening the game's menu pauses the game; Continue (leaving it) unpauses it, whatever paused it before (the temple's own pause is left alone) | done | `gui::StepMenuPause` (`src/Gui/GameMenu.h`), called from `src/Game.cpp`; the original's menu callback pauses on show and unpauses on close with nothing kept; test `GameMenu.TheMenuPausesTheGameAsItOpensAndContinueUnpausesItButNotInsideTheTemple` |
| A land starts unpaused | done | the game runs from the first frame by default (`game_clock::Start` in `src/Game.cpp`; `OPENBLACK_START_PAUSED=1` starts paused); no unit test: every fidelity and cycle run starts so |
| Entering the temple stops the world outside | done | the world's turns stand still in the citadel (`game_clock::IsInsideCitadel`, `creature_loop::FrozenInCitadel`); see ../temple/ for the temple's side; the temple's side is done: [temple_exterior.md](../temple/temple_exterior.md) "Inside, the world is paused, its ambience fades out and the temple's music plays" (tests `PausedTurnTimer.*`); the citadel's freeze is tested by `test/creature/test_creature_loop.cpp` (`FrozenInCitadel`) (2026-11-03) |

## Game speed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game speed scales how fast the game clock runs; time already gone keeps the speed it went at | done | `game_clock::SetSpeed`; test `GameClockTest.SpeedRebasesTheTimer` |
| Changing the speed while paused takes effect when the game is unpaused | done | `game_clock::SetSpeed` (`src/GameClock.cpp`); test `GameClockTest.SpeedRebasesTheTimer` (its paused part) |
| Challenge scripts can speed the game up for a section and set it back | done | START_GAME_SPEED, SET_GAMESPEED and END_GAME_SPEED through `help::script_control` (`src/Help/ScriptControl.cpp`, `src/CHLApi.cpp`); tests `ScriptControlSystem.GameSpeed`, `GameClockTest.SpeedRebasesTheTimer` |

## Calendar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game keeps a calendar date: a game year of 36 000 turns from a fixed start date, with months, days and seasons | done | `src/ECS/Weather/Calendar.cpp` (day of the year, month, season from the turn), read by the weather, births and fish farms; test `WeatherCalendar.StartDateAndSeasons` |
| The visual time of day follows its own clock that scripts can set and scale | done | `game_clock::VisualMs`; SET_GAME_TIME, GAME_TIME_ON_OFF, MOVE_GAME_TIME and SET_GAME_TIME_PROPERTIES in `src/CHLApi.cpp`; see ../sky/ for the day and night cycle; tests `DayNightClock.ScriptTimeFallsOnTheStandardHours`, `DayNightClock.MovesToAnHourInItsSeconds` |

## Random numbers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Game rules draw from one synchronised random stream, so the same game plays the same everywhere | done | `src/Common/GameRandom.cpp`; tests `GameRandom.SequenceFromInitialSeed`, `GameRandom.GameRandAfterInit` |
| Effects that don't change the game draw from a separate local stream | done | `game_random` local stream; test `GameRandom.LocalStream` |
| A random fraction scales a 16-bit draw | done | test `GameRandom.GameFloatRand` |
| Drawing zero gives zero without moving the stream | done | tests `GameRandom.ZeroForZero`, `GameRandomProduction.ZeroForZeroDoesNotDraw` |
| Particle effects draw from the game's streams only while they step | done | tests `GameRandom.ParticleOutsideAStep`, `GameRandom.ParticleScopeDoesNotNest` |
| Some things use the C runtime's own random numbers, which behave as Microsoft's | done | tests `GameRandom.Crt`, `GameRandomProduction.CrtRand` |
| Both streams start from a fixed seed when the game starts and are reset as each land loads | done | `game_random::Init` at the campaign's first land and `game_random::Reset` at every later land load (`Game::LoadMap`); tests `GameRandomProduction.InitAndReset` |
| The streams' positions are saved with a game and restored on load | done | loaded: a saved game's load puts both seeds back from its game record (`game_random::Load` from `ecs::saved_game_load::Apply`, `src/ECS/SavedGameLoad.cpp`; tests `GameRandom.SaveLoad`, `SavedGameLoad.TheFrozenLandOneAutosaveLoadsWithItsTurnSeedsAndTimeOfDay`); written: the game object's writer puts the synchronised and the local seed where the load reads them (`saved_game_save::CurrentValues`, `src/ECS/SavedWorld/WriteGame.cpp`; test `SavedWorldWrite.TheGameObjectsValuesGoWhereTheLoadReadsThem`); a save made in play loaded back and played on through the real input path (2026-11-11, joint594 exe, `tools/checks/realinput/quick_load.txt`): `refs/realinput/quick_load.log` (Land 1) saves into slot 19 in play and Ctrl+L loads it back (line 264 "loaded slot 19 at turn 764", the game record and its seeds put back by `Game::LoadSavedGame` through `saved_game_load::Apply`), and play goes on with no warning or error; `refs/realinput/quick_load_land2.log` the same on Land 2 (see saving_and_loading.md, whose own row for the streams is merged into this one) |

## Determinism and input

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's actions are queued and applied at the start of the next turn, not mid-frame | partial 70% | the action packet queue (`src/Input/GamePackets.cpp`; tests `GamePackets.*`) carries the hand's turn actions, the seeds and the creature hand packets; part of the hand still acts in the frame |
| Replaying the same inputs gives the same game | done | `test_replay_determinism` (`ReplayDeterminism.sameGameInTwoProcesses`), the per-turn state hash (`src/Debug/StateHash.cpp`, `OPENBLACK_STATE_HASH`) |
| Key presses are buffered so presses between frames are not lost | done | SDL events are drained each frame (`Game::ProcessEvents` in `src/Game.cpp`), and the game takes the frame's key downs as the original's key buffer does (2026-11-09): in the order they came, not a key down with the same key and Shift, Ctrl and Alt as the frame's last one taken, 128 a frame at most (`input::KeyBuffer`, `GameActionInterface::TakeKeyDown`; tests `KeyBuffer.*`, `GameActionMap.EachFrameTakesAHeldKeysRepeatOnce`; [the frame's key buffer](../../../interface/screen-and-readouts.md#the-frames-key-buffer)); before, every key down was taken, two repeats of a held key in one frame both acting |
