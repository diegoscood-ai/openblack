/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// The game clock of GGame, the one clock of the original (runblack.exe W120). There is no "get the time" routine in
/// the original: GGame::Loop 0x54CF20 works the clock out once a loop and publishes it in fields that hundreds of
/// readers read inline. This module keeps those fields and the routines that write them; Game drives it.
///
/// - The game timer is an LHTimer at g_game +0x205D68 (MSeconds 0x43EB70, Stop 0x43E9C0, SetSpeedUpFactor 0x43EBC0):
///   GetTickCount scaled by the game speed, stopped while paused (PauseGame 0x54AE20).
/// - A turn is due when that timer reaches turn * 100 (LocalTimerSaysDoATurn 0x54C4A0): an absolute comparison, so
///   no turn loses its leftover. More than 2 s behind, the lag is dropped (ResetLocalGameTimer 0x54C570). At most one
///   turn a frame in a single player game (ProcessNetworkPackets 0x54CD0F).
/// - The turn number g_game +0x205A40 goes up when the turn STARTS, and only unpaused (GGame::StartTurn 0x54E507).
/// - After the turns, the frame clock (GGame::Loop 0x54D2B2..0x54D3A6): the remainder of the turn in whole ms (0..99),
///   the visual clock turn * 100 + remainder (never backwards), the frame's game ms g_game_time_inc [0xEA9EC0] (whole
///   ms, 0 while paused, at most 199) and the fraction of the turn g_game +0x205D64 = remainder * 0.01 (kept, not
///   zeroed, while paused).
/// - The engine's wall clock g_delta_time [0xC38134] is another LHTimer (LH3DTech::g_timer 0xEA1B78), read in
///   LH3DRender::StartFrame 0x82F14E: whole ms, at least 1, it does not stop in pause.
///
/// Values that look alike and are NOT the same:
/// - k_MsPerTurn / MsPerTurn() ([0xD01A38], what the game logic reads) vs k_SchedulerMsPerTurn (the 100 written by
///   hand in the scheduler and the frame clock) vs k_TurnSeconds (the 0.1f GGame::ProcessTurn hands on by hand). They
///   are all 100 ms in the exe, but the original does not tie them: SET_GAME_TICK_TIME (0x714DBE) only changes the
///   first one (no map of the game uses it).
/// - FrameGameMs() (game time, stops in pause, follows the speed) vs FrameRealMs() (wall clock, never 0).
/// - Turn() (+0x205A40) vs the other counters of GGame (+0x205A44 every StartTurn, +0x205A4C loops, +0x205D44 frames
///   since the turn, +0x205D40 game ms by turn): only the first is the game's clock.
///
/// The game logic runs with the FPU at 24 bits (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D): the timer arithmetic is float.
namespace openblack::game_clock
{

/// [0xD01A38]: the ms of a turn that the game logic reads (GGame::Init 0x54F4A5 `mov [0xD01A38], 0x64`)
inline constexpr uint32_t k_MsPerTurn = 100;
/// The 100 the scheduler and the frame clock write by hand instead of reading [0xD01A38]: LocalTimerSaysDoATurn
/// 0x54C4F0, GGame::Loop 0x54D316 / 0x54D343, ResetLocalGameTimer 0x54C615, ResolveLoad 0x5550A3, SetSpeed 0x553810
inline constexpr uint32_t k_SchedulerMsPerTurn = 100;
/// The seconds of a turn GGame::ProcessTurn hands on by hand (push 0x3DCCCCCD): LH3DAtmos::UpdateGame 0x54E5D1,
/// GLandAlignement::UpdateTime 0x54E6C3, MusicMoodController::UpdateOnGameTurn 0x54E775
inline constexpr float k_TurnSeconds = 0.1f;
/// LocalTimerSaysDoATurn 0x54C553: more than 0x7D0 ms behind the turn, the local timer is reset to it
inline constexpr int32_t k_MaxLagMs = 2000;
/// ProcessNetworkPackets 0x54CD0F..0x54CD18 (`neg; sbb; and 9; inc`): 1 turn a frame in a single player game, 10 in a
/// network game
inline constexpr uint32_t k_MaxTurnsPerFrame = 1;
inline constexpr uint32_t k_MaxTurnsPerFrameNetwork = 10;
/// GGame::Loop 0x54D325..0x54D337: the remainder of the turn is clamped to 0..0x63
inline constexpr int32_t k_MaxRemainderMs = 99;
/// GGame::Loop 0x54D386: the fraction is the remainder times [0x8C4B10] = 0.01f
inline constexpr float k_FractionPerMs = 0.01f;
/// [0x8AA3B0] = 0.001f: the readers' `fild [0xEA9EC0]; fmul [0x8AA3B0]` (Process3dEngine 0x54DEB7, 0x54E037, 0x54E170)
inline constexpr float k_SecondsPerMs = 0.001f;
/// fn_005557E0 0x5557F7 / 0x55580D: the clamped frame ms are at most 0x1F4
inline constexpr uint32_t k_ClampedFrameMaxMs = 500;
/// GGame::Loop 0x54CF93 / ResetLocalGameTimer 0x54C690 / PauseGame: 0x3727C5AC, the speed the timer is given so that
/// SetSpeedUpFactor takes its running path before the saved speed goes back
inline constexpr float k_StartSpeed = 0.00001f;

/// LHTimer (0x110 bytes; the fields at +0x100..+0x10C), the same routines for every timer of the game
struct Timer
{
	uint32_t tickCount {0};     ///< +0x100: the GetTickCount of the last rebase
	int32_t elapsedTime {0};    ///< +0x104: the scaled ms gathered up to it
	float speedUpFactor {1.0f}; ///< +0x108: the speed (0 = stopped); LHTimer::Reset from the GGame ctor 0x54B58A: 1.0
	float savedFactor {1.0f};   ///< +0x10C: the speed kept while stopped

	/// LHTimer::MSeconds 0x43EB70: ftol((GetTickCount - base) * speed + elapsed)
	[[nodiscard]] int32_t MSeconds(uint32_t now) const;
	/// LHTimer::Stop 0x43E9C0: when running, keep the speed, gather the time and stop
	void Stop(uint32_t now);
	/// LHTimer::SetSpeedUpFactor 0x43EBC0: stopped, only keep the new speed; running, rebase and take it
	void SetSpeedUpFactor(float factor, uint32_t now);
	/// The inline start of GGame::Loop 0x54CF93, ResetLocalGameTimer 0x54C690 and PauseGame (unpausing): speed 1e-5,
	/// then SetSpeedUpFactor(saved), which rebases (the time the timer ran stopped or paused is dropped)
	void Start(uint32_t now);
};

/// GetTickCount: the milliseconds of the wall clock, wrapping at 2^32
[[nodiscard]] uint32_t TickCount();
using TickSource = uint32_t (*)();
/// Tests: the clock reads the ticks from here (nullptr = the wall clock)
void SetTickSource(TickSource source);

/// The state at program start (the GGame ctor and the statics of GGame::Loop at 0), for the tests
void Reset();

/// [0xD01A38]: the ms of a turn for the game logic
[[nodiscard]] uint32_t MsPerTurn();
/// GSetup::MapCommandProcess 0x714DBE (SET_GAME_TICK_TIME): only [0xD01A38]; the scheduler keeps its 100
void SetMsPerTurn(uint32_t ms);
/// GGameInfo::NumGameTicksPerSecond 0x711630: ftol(1000 / [0xD01A38] (integer div) * s), the turns of s seconds
[[nodiscard]] int32_t TicksForSeconds(float seconds);

/// g_game +0x205A40: the turns played (not the paused ones)
[[nodiscard]] uint32_t Turn();
/// Loading a game sets it (GGame::Load); a new game starts at 0
void SetTurn(uint32_t turn);

/// GGame::LocalTimerSaysDoATurn 0x54C4A0: the timer has reached turn * 100. Paused, never. More than 2 s behind, the
/// timer is reset to the turn (ResetLocalGameTimer) and the answer is still the one of the sample taken before.
[[nodiscard]] bool TimerSaysDoATurn();
/// The loop of ProcessNetworkPackets 0x54CD45..0x54CD58: TimerSaysDoATurn() (asked first, so it is asked once more
/// after the last turn of the frame) and fewer than k_MaxTurnsPerFrame turns this frame
[[nodiscard]] bool TurnDue();
/// ProcessNetworkPackets 0x54CE58 (++NetworkTurnsThisFrame [0xD01978]) and GGame::StartTurn 0x54E4F0: the turn number
/// goes up at the start of the turn, unpaused only (0x54E4FD..0x54E507)
void StartTurn();
/// GGame::ResetLocalGameTimer 0x54C570: stop, the timer = turn * 100 from now, and start it again
void ResetLocalTimer();

/// GGame::Loop 0x54CF6B..0x54D003 and 0x54D1F7 (the start of the game loop): the timer from 0 and started, the frame
/// ms and the fraction at 0, then ResetLocalGameTimer. `paused` is the pause flag the loop starts with (+0x14 bit 2)
void Start(bool paused);
/// GGame::ResolveLoad 0x555080: the frame ms and the fraction at 0, the visual clock at turn * 100. The statics of
/// GGame::Loop (0xD019B4..0xD019BC) are not touched
void OnLoad();

/// PauseGame 0x54AE20 (single player, 0x54AE7C..0x54AEE7): the pause flag, and the game timer stopped (pausing) or
/// started again from now (unpausing: the paused time does not count)
void Pause(bool paused);
/// g_game +0x14 bit 2
[[nodiscard]] bool IsPaused();
/// GGame::SetSpeed 0x5537F0 (single player, 0x553835..): the speed-up factor of the timer (1 = normal, 2 = twice as
/// fast); running, the timer is rebased, so the time already gone keeps the old speed. Stopped (paused), the new
/// speed is kept for when it starts again
void SetSpeed(float speed);
/// g_game +0x205B74: the speed asked for (0x553800)
[[nodiscard]] float Speed();

/// GGame::Loop 0x54D2A8..0x54D3A6, once a loop after the turns: the remainder, the visual clock, the frame's game ms
/// and the fraction (unpaused); the frame ms at 0 (paused). Then NetworkTurnsThisFrame = 0 (0x54D3C3, after the draw)
void UpdateFrameClock();
/// LH3DRender::StartFrame 0x82F14E..0x82F195: g_delta_time = the engine timer's ms since the last frame, 1 if <= 0
void UpdateRealClock();
/// LH3DTech::g_timer's MSeconds inline (0xEA1C78 / 0xEA1C7C / 0xEA1C80): the engine timer in ms, the wall clock (read
/// by StartFrame and by the help texts' timing, fn_005C61B0 0x5C6250..0x5C6274)
[[nodiscard]] int32_t EngineMs();

/// g_game_time_inc [0xEA9EC0] = g_game +0x250540 = +0x205D48: the game ms of this frame (whole, 0 paused, <= 199)
[[nodiscard]] uint32_t FrameGameMs();
/// FrameGameMs() * [0x8AA3B0] (0.001f), the readers' seconds
[[nodiscard]] float FrameGameSeconds();
/// g_game +0x205D64 (0x54D392): the remainder * 0.01, 0..0.99; one turn behind (0 just after a turn) and kept in pause
[[nodiscard]] float TurnFraction();
/// g_game +0x25053C: turn * 100 + remainder, the visual clock in ms (it never goes backwards)
[[nodiscard]] uint32_t VisualMs();
/// g_delta_time [0xC38134]: the wall clock ms of this frame (>= 1, does not stop in pause)
[[nodiscard]] uint32_t FrameRealMs();
/// GGame::GetCameraTimeInc 0x555820: g_game_time_inc while the interface plays a recording back (interface +0x45E8
/// and +0x45EC), g_delta_time otherwise. (inferido) openblack has no playback: `playingBack` stands for those fields
[[nodiscard]] uint32_t CameraFrameMs(bool playingBack = false);
/// fn_005557E0: g_delta_time in the temple (g_game +0x205A28 == 1), g_game_time_inc otherwise; <= 0 gives 0 and it is
/// at most 500 (both ways). openblack has no temple: `inTemple` stands for +0x205A28
[[nodiscard]] uint32_t ClampedFrameMs(bool inTemple = false);

} // namespace openblack::game_clock
