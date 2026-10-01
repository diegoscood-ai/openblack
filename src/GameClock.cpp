/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameClock.h"

#include <chrono>

namespace openblack::game_clock
{
namespace
{
uint32_t WallTicks()
{
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

TickSource g_TickSource = &WallTicks;

/// The fields of GGame and the statics of GGame::Loop the clock is made of
struct State
{
	Timer timer;                                  ///< g_game +0x205D68
	Timer engineTimer;                            ///< LH3DTech::g_timer 0xEA1B78
	uint32_t msPerTurn {k_MsPerTurn};             ///< [0xD01A38]
	uint32_t turn {0};                            ///< g_game +0x205A40
	bool paused {false};                          ///< g_game +0x14 bit 2
	float speed {1.0f};                           ///< g_game +0x205B74
	uint32_t turnsThisFrame {0};                  ///< GGame::NetworkTurnsThisFrame [0xD01978]
	int32_t previousLoopTimerSample {0};          ///< Loop::PreviousLoopTimerSample [0xD019B4]
	uint32_t previousLoopGameTurn {0};            ///< Loop::PreviousLoopGameTurn [0xD019B8]
	int32_t loopTimeRemainder {0};                ///< Loop::LoopTimeRemainder [0xD019BC]
	uint32_t visualMs {0};                        ///< g_game +0x25053C
	uint32_t frameGameMs {0};                     ///< g_game +0x250540 = g_game_time_inc [0xEA9EC0] = +0x205D48
	float fraction {0.0f};                        ///< g_game +0x205D64
	uint32_t previousEngineSample {0};            ///< [0xEA9EAC]
	uint32_t frameRealMs {1};                     ///< g_delta_time [0xC38134]
};

State g_State;
} // namespace

int32_t Timer::MSeconds(uint32_t now) const
{
	// fild qword (GetTickCount - base, unsigned); fmul [+0x108]; fiadd [+0x104]; __ftol 0x7A1400 (truncates)
	const auto ticks = static_cast<float>(now - tickCount);
	return static_cast<int32_t>(ticks * speedUpFactor + static_cast<float>(elapsedTime));
}

void Timer::Stop(uint32_t now)
{
	if (speedUpFactor != 0.0f)
	{
		savedFactor = speedUpFactor;
		elapsedTime = MSeconds(now);
		tickCount = now;
		speedUpFactor = 0.0f;
	}
}

void Timer::SetSpeedUpFactor(float factor, uint32_t now)
{
	if (speedUpFactor != 0.0f)
	{
		elapsedTime = MSeconds(now);
		tickCount = now;
		speedUpFactor = factor;
	}
	else
	{
		savedFactor = factor;
	}
}

void Timer::Start(uint32_t now)
{
	speedUpFactor = k_StartSpeed;
	SetSpeedUpFactor(savedFactor, now);
}

uint32_t TickCount()
{
	return g_TickSource();
}

void SetTickSource(TickSource source)
{
	g_TickSource = source != nullptr ? source : &WallTicks;
}

void Reset()
{
	g_State = {};
}

uint32_t MsPerTurn()
{
	return g_State.msPerTurn;
}

void SetMsPerTurn(uint32_t ms)
{
	g_State.msPerTurn = ms;
}

int32_t TicksForSeconds(float seconds)
{
	// 0x711635: `mov eax, 0x3E8; div [0xD01A38]` (unsigned integer division), fild qword, fmul s, __ftol
	const uint32_t perSecond = g_State.msPerTurn != 0 ? 1000u / g_State.msPerTurn : 0u;
	return static_cast<int32_t>(static_cast<float>(perSecond) * seconds);
}

uint32_t Turn()
{
	return g_State.turn;
}

void SetTurn(uint32_t turn)
{
	g_State.turn = turn;
}

bool TimerSaysDoATurn()
{
	auto& s = g_State;
	// 0x54C4A9..0x54C4DD: MSeconds inline; 0x54C4F0..0x54C4F9: turn * 100 by hand (lea x5, lea x5, shl 2)
	const int32_t local = s.timer.MSeconds(TickCount());
	const auto target = static_cast<int32_t>(s.turn * k_SchedulerMsPerTurn);
	// 0x54C528: single player and paused, no turn
	if (s.paused)
	{
		return false;
	}
	// 0x54C553 / 0x54C55B: more than 2 s behind, the timer is set back to the turn
	if (local - target > k_MaxLagMs)
	{
		ResetLocalTimer();
	}
	// 0x54C567 setge: the sample taken before the reset
	return local >= target;
}

bool TurnDue()
{
	// 0x54CD45: LocalTimerSaysDoATurn first, then NetworkTurnsThisFrame < limit (0x54CD52)
	return TimerSaysDoATurn() && g_State.turnsThisFrame < k_MaxTurnsPerFrame;
}

void StartTurn()
{
	auto& s = g_State;
	++s.turnsThisFrame; // ProcessNetworkPackets 0x54CE58
	// GGame::StartTurn 0x54E4FD..0x54E507: `inc [+0x205A40]` skipped when +0x14 & 4
	if (!s.paused)
	{
		++s.turn;
	}
}

void ResetLocalTimer()
{
	auto& timer = g_State.timer;
	const uint32_t now = TickCount();
	timer.Stop(now);
	// 0x54C615..0x54C621: the elapsed time = turn * 100, from now
	timer.tickCount = now;
	timer.elapsedTime = static_cast<int32_t>(g_State.turn * k_SchedulerMsPerTurn);
	timer.Stop(now);
	timer.Start(now);
}

void Start(bool paused)
{
	auto& s = g_State;
	const uint32_t now = TickCount();
	// 0x54CF6B..0x54CF9D: base = now, elapsed = 0, Stop, speed 1e-5, SetSpeedUpFactor(saved)
	s.timer.tickCount = now;
	s.timer.elapsedTime = 0;
	s.timer.Stop(now);
	s.timer.Start(now);
	// 0x54CFF7..0x54D003: g_game_time_inc, +0x205D48 and +0x205D64 at 0
	s.frameGameMs = 0;
	s.fraction = 0.0f;
	s.paused = paused;
	s.turnsThisFrame = 0;
	// 0x54D1F7: before the single player loop
	ResetLocalTimer();
}

void OnLoad()
{
	auto& s = g_State;
	s.frameGameMs = 0;                                 // 0x5550AC / 0x5550BC / 0x5550CC
	s.visualMs = s.turn * k_SchedulerMsPerTurn;        // 0x5550A3
	s.fraction = 0.0f;                                 // 0x5550D2
}

void Pause(bool paused)
{
	auto& s = g_State;
	// 0x54AE20: only when the flag changes
	if (paused == s.paused)
	{
		return;
	}
	s.paused = paused;
	const uint32_t now = TickCount();
	if (paused)
	{
		s.timer.Stop(now); // 0x54AE7C..0x54AEE7
	}
	else
	{
		s.timer.Start(now); // 0x54AEF3..
	}
}

bool IsPaused()
{
	return g_State.paused;
}

void SetSpeed(float speed)
{
	auto& s = g_State;
	s.speed = speed;                                   // 0x553800
	s.timer.SetSpeedUpFactor(speed, TickCount());      // 0x553835.., inline
}

float Speed()
{
	return g_State.speed;
}

void UpdateFrameClock()
{
	auto& s = g_State;
	if (!s.paused) // 0x54D2A8
	{
		// 0x54D2B2..0x54D2E2: MSeconds inline
		const int32_t sample = s.timer.MSeconds(TickCount());
		const int32_t delta = sample - s.previousLoopTimerSample;
		s.previousLoopTimerSample = sample;
		const uint32_t turn = s.turn;
		if (s.previousLoopGameTurn == turn)
		{
			s.loopTimeRemainder += delta;
		}
		else
		{
			// 0x54D316: lea edx, [edx + ecx - 0x64], 100 by hand
			s.loopTimeRemainder += delta - static_cast<int32_t>(k_SchedulerMsPerTurn);
			s.previousLoopGameTurn = turn;
		}
		// 0x54D325..0x54D337
		if (s.loopTimeRemainder > 0)
		{
			if (s.loopTimeRemainder >= k_MaxRemainderMs)
			{
				s.loopTimeRemainder = k_MaxRemainderMs;
			}
		}
		else
		{
			s.loopTimeRemainder = 0;
		}
		// 0x54D343..0x54D349: turn * 100 + remainder; 0x54D34E jae (unsigned): never backwards
		const uint32_t visual = turn * k_SchedulerMsPerTurn + static_cast<uint32_t>(s.loopTimeRemainder);
		if (visual < s.visualMs)
		{
			s.visualMs = visual;
			s.loopTimeRemainder = 0;
		}
		s.frameGameMs = visual - s.visualMs;          // 0x54D366 / 0x54D374 / 0x54D380
		s.visualMs = visual;
		// 0x54D386..0x54D392: fild [0xD019BC]; fmul [0x8C4B10]
		s.fraction = static_cast<float>(s.loopTimeRemainder) * k_FractionPerMs;
	}
	else
	{
		s.frameGameMs = 0; // 0x54D39A..0x54D3A6: the fraction is not touched
	}
	s.turnsThisFrame = 0; // 0x54D3C3
}

void UpdateRealClock()
{
	auto& s = g_State;
	// 0x82F14E..0x82F177: the engine timer's MSeconds inline
	const auto now = static_cast<uint32_t>(s.engineTimer.MSeconds(TickCount()));
	// 0x82F184..0x82F195: signed, <= 0 gives 1
	const auto delta = static_cast<int32_t>(now - s.previousEngineSample);
	s.frameRealMs = delta > 0 ? static_cast<uint32_t>(delta) : 1u;
	s.previousEngineSample = now;
}

uint32_t FrameGameMs()
{
	return g_State.frameGameMs;
}

float FrameGameSeconds()
{
	return static_cast<float>(g_State.frameGameMs) * k_SecondsPerMs;
}

float TurnFraction()
{
	return g_State.fraction;
}

uint32_t VisualMs()
{
	return g_State.visualMs;
}

uint32_t FrameRealMs()
{
	return g_State.frameRealMs;
}

uint32_t CameraFrameMs(bool playingBack)
{
	// 0x555820: interface +0x45E8 && +0x45EC -> g_game_time_inc, else g_delta_time
	return playingBack ? g_State.frameGameMs : g_State.frameRealMs;
}

uint32_t ClampedFrameMs(bool inTemple)
{
	// fn_005557E0: signed test, <= 0 -> 0 (0x55581A), >= 0x1F4 -> 500
	const auto ms = static_cast<int32_t>(inTemple ? g_State.frameRealMs : g_State.frameGameMs);
	if (ms <= 0)
	{
		return 0;
	}
	return ms >= static_cast<int32_t>(k_ClampedFrameMaxMs) ? k_ClampedFrameMaxMs : static_cast<uint32_t>(ms);
}

} // namespace openblack::game_clock
