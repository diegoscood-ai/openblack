/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTimer.h"

#include "ECS/Registry.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::ScriptTimer;

namespace
{
/// [0x8AA3B0] = 0.001f, the ms -> s of fn_00711670 / fn_007116D0
constexpr float k_SecondsPerMs = game_clock::k_SecondsPerMs;

/// `fild d; fimul [0xD01A38]; fmul [0x8AA3B0]` with the FPU at 24 bits (game_clock.h): each step rounded to a float
float TurnsToSeconds(int32_t turns, uint32_t msPerTurn)
{
	const float ms = static_cast<float>(turns) * static_cast<float>(static_cast<int32_t>(msPerTurn));
	return ms * k_SecondsPerMs;
}

ScriptTimer* TimerOf(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<ScriptTimer>(thing) : nullptr;
}

script_countdown::State g_Countdown;
} // namespace

void script_timer::SetTurns(ScriptTimer& timer, int32_t turns, uint32_t now)
{
	timer.startTurn = now;      // 0x711610..0x71161F: [g_game +0x205A40] -> +0x28
	timer.durationTurns = turns; // 0x711622: the argument -> +0x2C
}

void script_timer::SetSeconds(ScriptTimer& timer, float seconds, uint32_t now)
{
	SetTurns(timer, game_clock::TicksForSeconds(seconds), now);
}

uint32_t script_timer::ElapsedTurns(const ScriptTimer& timer, uint32_t now)
{
	return now - timer.startTurn; // 0x7116C5..0x7116CB
}

float script_timer::RemainingSeconds(const ScriptTimer& timer, uint32_t now, uint32_t msPerTurn)
{
	// 0x71167B..0x71169C: `sub ecx, eax; jns`: a timer past its end gives 0 (never negative)
	const int32_t left = timer.durationTurns - static_cast<int32_t>(ElapsedTurns(timer, now));
	return TurnsToSeconds(left < 0 ? 0 : left, msPerTurn);
}

float script_timer::SecondsSinceSet(const ScriptTimer& timer, uint32_t now, uint32_t msPerTurn)
{
	return TurnsToSeconds(static_cast<int32_t>(ElapsedTurns(timer, now)), msPerTurn); // 0x7116D3..0x7116F5
}

entt::entity script_timer::Create(float seconds)
{
	// fn_007115A0: Base::new(0x30, "ScriptTimer.cpp", 13); GameThing ctor, GameThingWithPos SetToZero, fn_00711610(0)
	// then 0x711630(seconds). (not ported) "out of memory" gives 0 there ("Thing not created")
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	auto& timer = registry.Assign<ScriptTimer>(thing);
	SetSeconds(timer, seconds, game_clock::Turn());
	return thing;
}

bool script_timer::IsTimer(entt::entity thing)
{
	return TimerOf(thing) != nullptr;
}

bool script_timer::SetTime(entt::entity thing, float seconds)
{
	auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return false;
	}
	SetSeconds(*timer, seconds, game_clock::Turn()); // 0x71131C: 0x711630(seconds): restarts it from this turn
	return true;
}

std::optional<float> script_timer::Remaining(entt::entity thing)
{
	const auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return std::nullopt;
	}
	return RemainingSeconds(*timer, game_clock::Turn(), game_clock::MsPerTurn());
}

std::optional<float> script_timer::SinceSet(entt::entity thing)
{
	const auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return std::nullopt;
	}
	return SecondsSinceSet(*timer, game_clock::Turn(), game_clock::MsPerTurn());
}

script_timer::SaveRecord script_timer::ToSave(const ScriptTimer& timer)
{
	return {timer.startTurn, timer.durationTurns};
}

ScriptTimer script_timer::FromSave(const SaveRecord& record)
{
	return {record.startTurn, record.durationTurns};
}

bool script_countdown::Start(float seconds)
{
	// 0x6EB8B0..0x6EB8C8: +8 = 1, +0x10 = 1 first; `fcomp 0; test ah, 0x41; je`: only above 0 is valid
	g_Countdown.on = true;
	g_Countdown.shown = true;
	const bool valid = seconds > 0.0f; // else ScriptErrorMessage "Invalid time for timer" (0xC0C0A4), not stopping
	if (seconds < 0.0f)               // 0x6EB8DF..0x6EB8F0
	{
		seconds = 0.0f;
	}
	g_Countdown.turnsLeft = game_clock::TicksForSeconds(seconds); // 0x6EB8F8..0x6EB91E
	return valid;
}

void script_countdown::Remove()
{
	g_Countdown.on = false; // 0x71118B: only +8 (the turns and +0x10 stay)
}

float script_countdown::RemainingSeconds()
{
	// 0x6EB953..0x6EB977: 1000 / [0xD01A38], then +0xC / that (both `div`, unsigned), fild qword. (inferred) a
	// [0xD01A38] above 1000 would divide by 0 in the original; openblack gives 0
	const uint32_t ms = game_clock::MsPerTurn();
	const uint32_t perSecond = ms != 0 ? 1000u / ms : 0u;
	if (perSecond == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(g_Countdown.turnsLeft) / perSecond);
}

bool script_countdown::Exists()
{
	return g_Countdown.on;
}

void script_countdown::SetShown(bool shown)
{
	g_Countdown.shown = shown;
}

void script_countdown::ProcessTurn()
{
	// fn_006EB930: `dec [ecx+0xC]; jne`: off when it reaches 0 (a 0-turn countdown goes on below 0, as the original)
	if (g_Countdown.on && --g_Countdown.turnsLeft == 0)
	{
		g_Countdown.on = false;
	}
}

script_countdown::Display script_countdown::GetDisplay()
{
	const float seconds = RemainingSeconds();
	// 0x54E44F..0x54E45B: +8 and +0x10; 0x54E47B: `fcomp [0x8AB414] 10.0; test ah, 1`: red below 10
	return {g_Countdown.on && g_Countdown.shown, seconds, seconds < 10.0f};
}

const script_countdown::State& script_countdown::Get()
{
	return g_Countdown;
}

void script_countdown::Reset()
{
	g_Countdown = {};
}

} // namespace openblack::ecs
