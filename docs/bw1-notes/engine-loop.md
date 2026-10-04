# The engine loop: game turns and frames (original and openblack)

What runs once per game turn and what runs once per frame in the original, who may change the game state, how the
random streams are shared, and how openblack's loop compares; the plan to put the drawing on its own thread; the
tools to prove that two runs play the same game. Session Motor (2026-10-03). Everything is **faithful** (read in
runblack.exe W120 with `herramientas\dis\bwdis.py` / `callers.py`, or in the decomp `bw1-decomp/src/Black/Game.cpp`)
except what is marked **(inferred)**, **(approximate)**, **(pending)** or **(not ported)**. Working notes:
`dev\documentacion\motor\` (PHASE0.md, turn_order.md, dead_list.md).

- [1. The original loop](#1-the-original-loop)
- [2. The turn, call by call](#2-the-turn-call-by-call)
- [3. The interface: frames send packets, turns apply them](#3-the-interface-frames-send-packets-turns-apply-them)
- [4. Random streams between turn and frame](#4-random-streams-between-turn-and-frame)
- [5. openblack's loop](#5-openblacks-loop)
- [6. Threads (plan)](#6-threads-plan)
- [7. Proving two runs are the same game](#7-proving-two-runs-are-the-same-game)
- [8. Floating point](#8-floating-point)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## 1. The original loop

`GGame::Loop` 0x54CF20, each iteration (decomp Game.cpp:1902-1984):

1. `control_map->ProcessActionsPerformed`, `ProcessBufferedKeys`.
2. `ProcessNetworkPackets` 0x54CC30 (called at 0x54D28A): while `LocalTimerSaysDoATurn` 0x54C4A0 and fewer than
   1 turn this frame (10 in a network game), `ProcessOneGameTurn` 0x54D620. In a network game a turn runs only when
   the next packet is a superpacket: one superpacket is one turn (lockstep).
3. The frame clock (0x54D2A8..0x54D3A6: remainder, visual clock, `g_game_time_inc`, the fraction of the turn;
   `game_clock`, [engine-math.md](engine-math.md)).
4. `ProcessGraphicsEngine` 0x54D850: mouse delta, `GCamera::Update` 0x441F80 (at 0x54D879), `GInterface::PreDrawProcess`
   0x5CE9E0, `Process3dEngine` 0x54DA80 (the draw, [original-frame.md](original-frame.md)), `PostDrawProcess` 0x5CEAB0,
   `HelpSystem::PostDrawProcess`.
5. `NetworkTurnsThisFrame = 0` (0x54D3C3); `ProcessFrameInputs` 0x54C340 (`Mouse.ProcessButtons`,
   `GInterface::ProcessFrameUpdates` 0x5CEDB0), screenshot, `FlipScreen`.

`ProcessOneGameTurn` 0x54D620: `GameTimeMilliseconds += MsPerTurn`, **`ProcessGameInputs` 0x54C3D0**
(`ProcessBufferedKeys`, `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420, `GInterface::Process`
0x5CEC10), `ProcessGameCode` 0x54D820 (`StartTurn` 0x54E4F0 → `ProcessTurn` 0x54E5C0 unless paused → `EndTurn`
0x54E960), then `fn_005557D0`, `DoWallHuggerLookahead` 0x609A50, `RepairMissingMothers`.

The game runs on one thread; the window's input reaches it through critical sections (`LHKeyboard`, `LHScreen`,
decomp Game.cpp:1604-1618).

## 2. The turn, call by call

`GGame::ProcessTurn` 0x54E5C0 calls, in this order (call site → callee): 0x54E5C7 Whale::ProcessAll 0x775140;
0x54E5D7 LH3DAtmos::UpdateGame 0x8356E0; 0x54E62D PSysGlobal::GameLoopStart 0x68F590; 0x54E637 GGameInfo::Process
0x557B60; 0x54E63C InfluenceRing::ProcessRings 0x5CDB90; 0x54E641 GPlayer::ProcessPlayers 0x649A20; 0x54E646
Dance::ProcessDances 0x50BB60; 0x54E651 GlobalGameLists::Process 0x591370; 0x54E656 Forest::ProcessForests 0x539D70;
0x54E65B Living::ProcessLiving 0x5EC810; 0x54E660 FireEffect::ProcessList 0x730760; 0x54E665 Ball::ProcessBalls
0x435F30; 0x54E66A Reaction::ProcessReactions 0x6E3B50; 0x54E66F Spell::ProcessSpells 0x720300; 0x54E674
GParticleContainer::ProcessParticleContainers 0x63E090; 0x54E679 FireFly::ProcessAll 0x52B7A0; 0x54E67E
PhysicsObject::GameTurnUpdate 0x644FC0; 0x54E683 PSysEditorInterface::ProcessGameTurn 0x67D630; 0x54E688
PSysGlobal::GameLoopEnd 0x68F5B0; 0x54E693 GScript::Process 0x6EB6B0; 0x54E69E HelpSystem::Process 0x5C8FE0;
0x54E6A9 HelpProfile::Process 0x5C4660; 0x54E6C3 GLandAlignement::UpdateTime 0x5E1FE0; 0x54E6CB
WeatherThing::ProcessWeatherThings 0x7741A0; 0x54E6D0 Bookmark::ProcessAll 0x439DD0; 0x54E6D5
ScriptHighlight::ProcessHighlights 0x70A460; 0x54E6DA GClimate::ProcessAll 0x771BE0; 0x54E6DF
GBelief::ProcessOncePerTurn 0x4380B0; 0x54E6F1 CHand::GameTurnUpdate 0x46E4E0; 0x54E6F8 GGame::AddPlayerSparkles
0x552640; 0x54E6FD MobileObject::AddMobileObjectCheckSum 0x606FC0; 0x54E704 GameThing::ProcessDeadList 0x56FB10
(argument 0); 0x54E70C Reward::ProcessList 0x6E6890; 0x54E711 GSpookyVoices::Process 0x72E310; 0x54E716
GGuidance::HelpSpritesCheckMoonPhase 0x71D1C0; 0x54E729 GGuidance::ProcessTownDesireSFX 0x71B020; 0x54E731
GConfirmation::Process 0x71A650; 0x54E738 GGame::Update3DInfluence 0x555280; 0x54E743
GCamera::CheckStackedModesForValidity 0x441D40; 0x54E74E GCamera::Validate 0x441F50; 0x54E768
Fragment::ProcessTimer 0x76EAF0 (each fragment, the next taken first); 0x54E77A MusicMoodController::UpdateOnGameTurn
0x633EF0; then the memory packets and `ScriptRebootRequested`.

The dead list (`GameThing::ToBeDeleted` 0x56FB70 marks, `ProcessDeadList(0)` frees on the second pass) is described
in `dev\documentacion\motor\dead_list.md`; openblack still destroys at once **(pending)**.

## 3. The interface: frames send packets, turns apply them

- The interface's message pump and action state machine (fn_005D9A20 → fn_005D9BC0 → fn_005D1120
  InterfaceActionProcess) run **once per frame** (`ProcessFrameUpdates` 0x5CEDB0 is `jmp 0x5D9A20`, from
  `ProcessFrameInputs` at 0x54C3B6) **and once per turn** (`GInterface::Process` at 0x5CEC1F). The queue (+0x43E) is
  emptied after each pump, so a mouse message is handled once, almost always in the frame pass.
- They do not change the game: they send packets (0x13 pick up, 0x12 / 0x4D throw, 0x20 tap, 0x1B / 0x1C locked
  select, 0x1F give, 0x6A power-up; 0x15 hand position, 0x16 / 0x17 camera from fn_005D2250 per turn), applied by
  `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420 at the start of a later turn.
- The hand's per-turn game writers: `GInterfaceStatus::Process` 0x5DC4E0 (the multi pick-up transfer,
  `ProcessInInteract` 0x66E520) and `CHand::GameTurnUpdate` 0x46E4E0.
- Per frame without game writes: hand placement and spring (HandStateHolding::Update 0x5B3C70), the held object's
  pose (CHand::UpdateHeldObject 0x46E490), PreDrawProcess (UpdateHandRenderCollide 0x5D0610, disciple icon), and
  PostDrawProcess (leashes 0x5D9130, UpdateInterfaceCollide 0x5D5A70).
- The hand demos (fn_005DAEE0) play inside the pump, in both passes.
- No synced random number is drawn in the hand and interface code (CHand 0x46B000-0x46F000, hand states
  0x5B0000-0x5BA000, GInterface 0x5CE000-0x5DD000, gestures 0x578000-0x57D000).

The draw writes a few game fields itself: the whale's heading (fn_00774E30, `mov [ebx+0x6c]` at 0x774FF2) and the
fish bait (fn_00824B90: +0x20 / +0x24 / +0x18 / +0x8C).

## 4. Random streams between turn and frame

| Stream ([engine-math.md](engine-math.md) "Random numbers") | Turn side | Frame / draw side |
|---|---|---|
| Synced GRand (GameRand 0x6DE510) | the game logic | none found in the draw |
| Local GRand (LocalRand 0x6DE570: 115 call sites; LocalFloatRand 0x6DE590: 49) | PhysicsObject::AttemptToAddSoundEvent 0x6467D1, GGuidance, GSpookyVoices, PSys local fn_00672AA0, ParticleMistCreator 0x6AA5E0 | Tree::Draw 0x74B135, Abode::Draw 0x51615A, light maps fn_006CA7D0, ParticleLightMap::DrawAt 0x67B26A, HelpSystem draw, fire flames fn_00731AB0 |
| CRT (rand 0x7C8837: 48; Random 0x81D180: 167) | SmokyStuff::Create 0x823D39, LH3DMist, Reward and TempleLeash creation | GWeather::DrawClouds 0x83FD05, Tree::PreDraw 0x74A805, camera shake fn_008210C0, fish fn_00824DA0, PetitNavire::PostDraw |
| PSys selection ([0xD4E0C0] / [0xD4E0BC], per effect step fn_00673340) | effects stepped in the turn | effects stepped in the draw |

So all streams but the synced one are shared by the turn and the draw on the one game thread: running turns and
frames at the same time would change their sequence (and race on the PSys selection).

## 5. openblack's loop

`Game::Update` (src/Game.cpp) per frame: input and events, ImGui, the camera **before** the turns (the original
updates it after: **(approximate)**), `while (game_clock::TurnDue()) GameLogicLoop();`, the frame clock, the
per-frame updaters (physics interpolation, fields, trees, fireflies, rings, scenery, mobiles, animations, sharks,
fish), the hand, `RenderingSystem::PrepareDraw`, audio; then `Game::Run` draws (`DrawScene`) and calls `bgfx::frame`.
bgfx is built multithreaded (vcpkg feature `multithreaded` → `BGFX_CONFIG_MULTITHREADED`): its render thread is
already separate; openblack's main thread is the bgfx API thread.

Differences with §1-§3 (owners and order in `turn_order.md`):
- The hand acts on the game directly every frame (pick-up, release, taps, tug, seeds) instead of packets applied in
  the turn **(approximate, pending: Mano, Hito 3)**.
- The first animation clip of a new villager or animal is chosen per frame with synced GameRand; the original does
  it at creation in the turn (Living::CallVirtualFunctionsForCreation 0x5EC9B0 → SetStateAnim 0x5ECB10)
  **(pending: Personas)**.
- Bullet was stepped every frame with the wall clock (openblack-only); it only answers ray casts **(not original)**.
- The turn steps follow §2 since the turn-order commit (Game::GameLogicLoop names each call site); what is not
  ported is marked there and in `turn_order.md`.

## 6. Threads (plan)

User decision (2026-10-03): the game logic stays exactly the original's (turns, order, random numbers); the drawing
may run on its own thread, and **one frame of display latency is accepted by default** — a presentation difference,
not a logic one. A single-thread mode without latency stays for comparison and for the replay test.

Design "level 1" (PHASE0.md §c.2): one logic thread runs every frame's game code in today's order (input, turns,
frame updaters, hand, the draw-side writers moved out of DrawScene); it publishes an immutable snapshot of what the
draw reads; the main thread (window, bgfx API) encodes the previous frame's snapshot. Turns in parallel with frames
("level 2") cannot be identical (§4): only as a mod, off by default **(pending)**.

## 7. Proving two runs are the same game

- `OPENBLACK_STATE_HASH=<file>` (src/Debug/StateHash.*): at the end of each turn, one line `turn <n> <total>
  random=… crt=… clock=… pools=… transform=…` (FNV-1a over the GRand seeds and the PSys stream; the CRT seed; the
  turn, pause, speed, ms per turn; every registry storage's size and entity order, folded by storage id; every
  Transform's bits). Owners add their parts with `state_hash::Register`. Not the original's network checksum (`AddMobileObjectCheckSum`
  0x606FC0 / `SendNetworkChecksum` 0x635210, **not ported**).
- `OPENBLACK_FIXED_FRAME_MS=<ms>` (src/Debug/FixedClock.*): every frame is that many ms (game timer, engine timer,
  audio ticks, the frame's delta), so a run repeats (not covered: the 10 s wall-clock timeout of PSys/Rules/Lightning.cpp).
- `test_replay_determinism`: the mock land played twice, headless, with the fixed clock; the per-turn hashes must be
  equal (later: one thread against two).
- `OPENBLACK_PROFILE=<s>` now also logs the stages "Frame Updaters", "Turn: Map Rebuild", "Turn: Magic", "Turn:
  Physics", "Turn: Particles" and the memory (RAM working set, peak and private bytes; bgfx texture and render-target
  memory, GPU memory when the backend says, transient buffers).

## 8. Floating point

- The original sets the x87 FPU to 24-bit precision for the game logic (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D; also at
  the start of GGame::EndTurn): each + − × ÷ and sqrt rounds to a float. openblack is x64: SSE2 scalar float, one
  rounding per operation, MSVC's default `/fp:precise`, no FMA. For those operations the results are the same,
  **(inferred)** except where an intermediate would overflow, underflow or be denormal in float (the x87 keeps its
  extended exponent range).
- **(not verified)** The transcendental functions (`pow`, `sin`, `cos`, `atan2`, `exp`, `log`…) of the x64 CRT are not
  the original's x87 instructions (`fsin`, `fpatan`, `fyl2x`…) or its CRT's x87 code: their last bits can differ. To
  check case by case where it matters to the game's results (game_random already writes its arithmetic one operation
  per statement).
- The top CMakeLists.txt stops the configure on `/fp:fast`, `/fp:contract`, `/arch:AVX2` / `AVX512`, `-ffast-math`,
  `-mfma`, `-ffp-contract=fast` (they would fuse or reassociate float math).
- Threads: a new thread starts with the default MXCSR (0x1F80: round to nearest, no flush-to-zero); nothing in src
  changes it. The game logic always runs on one thread; the logic thread will check its MXCSR in debug builds (M3).

## Pending

- The hand as packets applied in the turn (Mano), the first clip at creation (Personas), the turn order
  (`turn_order.md`), the dead list (`dead_list.md`), the threads (Hito 3), the per-frame registry entities of carried
  props and scenery.

## Test hooks

`OPENBLACK_STATE_HASH`, `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_PROFILE` (with memory), `OPENBLACK_TRACE_GAME_RAND`
([engine-math.md](engine-math.md)); headless runs with `-b Noop -n <frames>`.

## Sources

runblack.exe W120 (`herramientas\dis\bwdis.py 0x54E5C0:0x200`, `callers.py`); `bw1-decomp/src/Black/Game.cpp`
1585-1620, 1704-1788, 1902-1984, 2072-2101, 2105-2120, 2438-2527; `bw1-decomp/src/Black/GameThing.cpp:55-131`;
`dev\documentacion\motor\PHASE0.md` (inventory §i).
