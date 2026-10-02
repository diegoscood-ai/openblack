/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DayNightClock.h"

#include "SkyType.h"

#include <algorithm>
#include <cmath>

#include "GameClock.h"

using namespace openblack;

namespace
{
// Game turn length in seconds, as passed by GGame::ProcessTurn (0x54E6AE..0x54E6C3: [0xD01A3C] * 0.1, 0.1)
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;

float WrapHours(float t)
{
	while (t < 0.0f)
	{
		t += 24.0f;
	}
	while (t >= 24.0f)
	{
		t -= 24.0f;
	}
	return t;
}

/// fn_00869FD0: piecewise-linear map of the half day [0, 12] with thresholds `from` onto `to`, mirrored at 12
float MapHours(float t, const std::array<float, 4>& from, const std::array<float, 4>& to)
{
	const bool mirrored = t > 12.0f;
	if (mirrored)
	{
		t = 24.0f - t;
	}
	float lo0 = 0.0f;
	float lo1 = 0.0f;
	float hi0 = 12.0f;
	float hi1 = 12.0f;
	size_t i = 0;
	while (i < from.size() && !(t < from[i]))
	{
		++i;
	}
	if (i > 0)
	{
		lo0 = from[i - 1];
		lo1 = to[i - 1];
	}
	if (i < from.size())
	{
		hi0 = from[i];
		hi1 = to[i];
	}
	float r = (t - lo0) / (hi0 - lo0) * (hi1 - lo1) + lo1;
	return mirrored ? 24.0f - r : r;
}
} // namespace

void DayNightClock::Reset()
{
	// GLandAlignement::Open: fn_005576F0, SetVisualTimeScale(1), SetVisualTimeCycle(defaults), ForceVisualTime(12)
	_scale = 1.0f;
	_moveSeconds = 0.0f;
	SetCycle(k_DefaultDuration, k_DefaultNight, k_DefaultChange);
	ForceScriptTime(12.0f);
}

void DayNightClock::SetCycle(float duration, float night, float change)
{
	// 0x557625..0x557640: trunc(duration * 0.41666666f) with [0x8DF8F0] = 0x3ED55555 (10 / 24 in tenths of a second).
	// The product stays in the x87 register before __ftol; with the FPU at 24 bits (fn_007DEE00, and cw 0xFCFF at 0x7DEE0D) that is
	// the float product taken here, with 53 bits (the CRT's __setdefaultprecision 0x7CC96C) durations that are
	// multiples of 2.4 would truncate one lower.
	const auto tenths = static_cast<int>(duration * 0.41666666f);
	// 0x557645..0x557685: [0xBF338C] = [0xBF3390] = 10 / n (0 for n = 0), the day and night rates are the same
	_dayRate = tenths != 0 ? 10.0f / static_cast<float>(tenths) : 0.0f;
	_nightRate = _dayRate;

	// 0x55768F..0x5576E4: N = night 12, E = 12 change + N (kept as a float at [esp+8]), c = (E - N) * 0.25
	// ([0x8AB3D4]) limited to N, SetDayNightTimes(N - c, c + N, E - c, E + c)
	const float halfNight = night * 12.0f;
	const float end = change * 12.0f + halfNight;
	const float ramp = std::min((end - halfNight) * 0.25f, halfNight);
	_times = {halfNight - ramp, ramp + halfNight, end - ramp, end + ramp};
	sky_type::SetThresholds(_times[0], _times[1], _times[2], _times[3]); // 0x5576E4 -> 0x869FA0
}

void DayNightClock::SetCycleFromMapEditor(float duration, float night, float change)
{
	night = std::min(night, 1.0f);
	change = std::min(change, 1.0f - night);
	SetCycle(duration, night, change);
}

void DayNightClock::ForceScriptTime(float hour)
{
	_moveSeconds = 0.0f;
	SetTarget(ScriptToVisual(hour), 0.0f);
	_visualTime = _target;
	// fn_005E22A0 0x5E22CB: fn_0086A270, the sky type is sampled at once and the dome rebuilt whole. Its following
	// call fn_005E1DE0 (0x5E22D3, reads [0xBF3378]) is not sky type and is not done here.
	sky_type::Jump(_visualTime);
}

void DayNightClock::MoveScriptTime(float hour, float seconds)
{
	SetTarget(ScriptToVisual(hour), seconds);
}

void DayNightClock::SetTarget(float hour, float seconds)
{
	if (hour < -1000.0f || hour > 1000.0f)
	{
		hour = 0.0f;
	}
	_target = WrapHours(hour);
	if (seconds != 0.0f)
	{
		_moveSeconds = seconds;
		float diff = std::abs(_visualTime - _target);
		if (diff > 12.0f)
		{
			diff -= 24.0f;
		}
		_step = std::abs(diff) / seconds;
	}
}

void DayNightClock::ProcessTurn()
{
	const float gameSeconds = _scale * k_TurnSeconds;
	if (_moveSeconds != 0.0f)
	{
		if (_visualTime == _target)
		{
			_moveSeconds = 0.0f;
		}
	}
	else
	{
		_step = 2.5f;
		const float rate = (_nightRate - _dayRate) * GetSkyType() * 0.5f + _dayRate;
		SetTarget(_visualTime + rate * gameSeconds, 0.0f);
	}

	// Slide towards the target the short way round, at most _step hours per second of turn
	float target = _target;
	if (_visualTime - target > 12.0f)
	{
		target += 24.0f;
	}
	if (_visualTime - target < -12.0f)
	{
		target -= 24.0f;
	}
	const float step = _step * k_TurnSeconds;
	if (target < _visualTime)
	{
		_visualTime = std::max(_visualTime - step, target);
	}
	else if (target > _visualTime)
	{
		_visualTime = std::min(_visualTime + step, target);
	}
	_visualTime = WrapHours(_visualTime);
}

float DayNightClock::ScriptToVisual(float hour) const
{
	return MapHours(hour, k_StandardTimes, _times);
}

float DayNightClock::VisualToScript(float hour) const
{
	return MapHours(hour, _times, k_StandardTimes);
}

float DayNightClock::Time2SkyType(float hour) const
{
	// LH3DSky::Time2SkyType 0x86A1B0, on this clock's copy of the thresholds (the same as LH3DSky's after SetCycle)
	return sky_type::At(hour, _times);
}

bool DayNightClock::IsVisualNight() const
{
	return sky_type::IsVisualNight(GetSkyType());
}
