# Game loop and clock

The game's heartbeat: the world moves on in fixed game turns, ten a second, while the screen is drawn as often as it
can be. A game clock that runs at the game's speed and stops while paused decides when a turn is due; between turns the
frame works out how far through the turn it is. Two random number streams keep the game's rules repeatable.

**Progress: 27/39 done, 8 partial — 79%**

How the original does it, in our wiki: [The engine loop: game turns and frames (original and openblack)](../../bw1-notes/engine-loop.md), [Coordinates, terrain, object size, game clock, matrices and Zoomer](../../bw1-notes/engine-math.md).

## Game turns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world moves on in game turns of a tenth of a second | done | `src/GameClock.cpp` (`game_clock`, 100 ms a turn); test `GameClockTest.FirstTurnRightAwayThenEveryHundredMs` |
| A turn is due once the game clock reaches the turn number times the turn's length, so no turn loses what was left of the one before | done | `game_clock::TurnDue`; test `GameClockTest.ThirtyFpsKeepsTheLeftover` |
| At most one turn is played in a frame in a single-player game | done | `Game::Update` plays at most one due turn a frame (`src/Game.cpp`). Network games play several to keep up: see ../multiplayer/ |
| When the game falls more than two seconds behind it gives up on the lost time rather than racing to catch up | done | `src/GameClock.cpp`; test `GameClockTest.MoreThanTwoSecondsBehindDropsTheLag` |
| The turn number starts again from 0 as each land starts | done | `game_clock::OnLoad` from `Game::LoadMap`; test `GameClockTest.OnLoadSetsTheVisualClockToTheTurn` |
| Each turn processes the world's parts in the game's fixed order (players, creatures, villagers, scripts, weather, magic …) | partial | `Game::GameLogicLoop` names each step of the original's turn in order; the steps whose systems are not ported are marked there |
| The per-frame (drawing-side) updates run in the game's order | partial | the frame updaters run in a fixed order in `Game::Update` (physics interpolation, fields, trees, fireflies, rings, scenery, mobiles, animations, sharks, fish); the camera updates before the turns where the original updates it after |
| Physics is stepped by the game's turns, not by the wall clock | done | the game's own physics runs once a turn (`src/ECS/Physics`, "Turn: Physics" in `Game::GameLogicLoop`) |
| The temple has turns of its own while the world outside is stopped | partial | inside the citadel the audio has a turn of its own (`audio::ProcessCitadelTurn` from `src/Game.cpp`); the rest of the temple's turn: see ../temple/ |
| A game starts with a short pause before its loop: the logo plays once, then the clock is reset to the current turn | todo | openblack starts its loop straight away; logo and intro films: see ../story/ |

## Frame clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each frame has a game-time step: how far the game clock moved since the last frame, none while paused | done | `game_clock::UpdateFrameClock`, `FrameGameMs`; test `GameClockTest.RemainderFractionAndFrameMs` |
| Each frame knows how far it is through the current turn (0 to 0.99), so moving things can be drawn between their turn positions | done | `game_clock::TurnFraction` (clamped at 0.99; test `GameClockTest.RemainderClampedAt99AndFrameMsAtMost199`), used to draw villagers and animals between turns (`src/ECS/MobileDrawing`), fire, magic, particles and shields |
| The frame's real time is whole milliseconds of the wall clock and never zero | done | `game_clock::FrameRealMs`; test `GameClockTest.RealClockAndSelectors` |
| The hand and the camera move by real time, except during a script's cut scene, when they keep to game time | done | `game_clock::CameraFrameMs` (`src/GameClock.cpp`) |
| While the window is minimised the game keeps running but draws nothing | todo | nothing skips the drawing while minimised |
| Quitting is noticed between frames and ends the loop cleanly once help is not mid-sequence | partial | `Game::Run` ends on quit; the help-sequence guard is not there |

## Pausing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pausing stops the game clock; unpausing starts it again from where it stopped | done | `game_clock::Pause`; test `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| No turns are played while paused | done | `game_clock::TurnDue` while paused; test `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| Opening the game's menu pauses the game; leaving it restores the pause it had before | done | the menu's pause in `src/Game.cpp` (`_menuPaused`), left alone inside the temple |
| A land starts unpaused | done | the game runs from the first frame by default (`OPENBLACK_START_PAUSED=1` starts paused) |
| Entering the temple stops the world outside | partial | the world's turns stand still in the citadel (`game_clock::IsInsideCitadel`, `creature_loop::FrozenInCitadel`); see ../temple/ for the temple's side |

## Game speed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game speed scales how fast the game clock runs; time already gone keeps the speed it went at | done | `game_clock::SetSpeed`; test `GameClockTest.SpeedRebasesTheTimer` |
| Changing the speed while paused takes effect when the game is unpaused | done | `game_clock::SetSpeed` (`src/GameClock.cpp`) |
| Challenge scripts can speed the game up for a section and set it back | done | START_GAME_SPEED, SET_GAMESPEED and END_GAME_SPEED through `help::script_control` (`src/Help/ScriptControl.cpp`, `src/CHLApi.cpp`) |
| A land's map script can set the length of a game turn | todo | the map command throws in `src/LHScriptX/MapScriptCommands.cpp` (and map commands are not run at all) |

## Calendar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game keeps a calendar date: a game year of 36 000 turns from a fixed start date, with months, days and seasons | done | `src/ECS/Weather/Calendar.cpp` (day of the year, month, season from the turn), read by the weather, births and fish farms |
| A land's map script can set the start date, start time and turns per year | todo | map commands throw in `MapScriptCommands.cpp`; the original lands do not use them |
| The visual time of day follows its own clock that scripts can set and scale | done | `game_clock::VisualMs`; SET_GAME_TIME, GAME_TIME_ON_OFF, MOVE_GAME_TIME and SET_GAME_TIME_PROPERTIES in `src/CHLApi.cpp`; see ../sky/ for the day and night cycle |

## Random numbers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Game rules draw from one synchronised random stream, so the same game plays the same everywhere | done | `src/Common/GameRandom.cpp`; tests `GameRandom.SequenceFromInitialSeed`, `GameRandom.GameRandAfterInit` |
| Effects that don't change the game draw from a separate local stream | done | `game_random` local stream; test `GameRandom.LocalStream` |
| A random fraction scales a 16-bit draw | done | test `GameRandom.GameFloatRand` |
| Drawing zero gives zero without moving the stream | done | tests `GameRandom.ZeroForZero`, `GameRandomProduction.ZeroForZeroDoesNotDraw` |
| Particle effects draw from the game's streams only while they step | done | tests `GameRandom.PSysOutsideAStep`, `GameRandom.PSysScopeDoesNotNest` |
| Some things use the C runtime's own random numbers, which behave as Microsoft's | done | tests `GameRandom.Crt`, `GameRandomProduction.CrtRand` |
| Both streams start from a fixed seed when the game starts and are reset as each land loads | done | `game_random::Init` at the campaign's first land and `game_random::Reset` at every later land load (`Game::LoadMap`); tests `GameRandomProduction.InitAndReset` |
| The streams' positions are saved with a game and restored on load | partial | the streams can be saved and restored (test `GameRandom.SaveLoad`), but there are no save games yet (see saving_and_loading.md) |

## Determinism and input

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's actions are queued and applied at the start of the next turn, not mid-frame | partial | the action packet queue (`src/Input/GamePackets.cpp`; tests `GamePackets.*`) carries the hand's turn actions, the seeds and the creature hand packets; part of the hand still acts in the frame |
| Replaying the same inputs gives the same game | done | `test_replay_determinism` (`ReplayDeterminism.sameGameInTwoProcesses`), the per-turn state hash (`src/Debug/StateHash.cpp`, `OPENBLACK_STATE_HASH`) |
| Key presses are buffered so presses between frames are not lost | partial | SDL events are drained each frame (`Game::ProcessEvents` in `src/Game.cpp`); not compared with the game's buffering |
