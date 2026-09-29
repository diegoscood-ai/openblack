/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DayNightClock.h"

#include <algorithm>
#include <cmath>

using namespace openblack;

namespace
{
// Game turn length in seconds, as passed by GGame::ProcessTurn
constexpr float k_TurnSeconds = 0.1f;

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
	// 1 / (hours per second) in tenths of a second: trunc(duration * 10 / 24); the day and night rates are the same
	const auto tenths = static_cast<int>(duration * 0.416667f);
	_dayRate = tenths != 0 ? 10.0f / static_cast<float>(tenths) : 0.0f;
	_nightRate = _dayRate;

	const float halfNight = night * 12.0f;
	const float halfChange = change * 12.0f;
	const float end = halfNight + halfChange;
	const float ramp = std::min(halfChange * 0.25f, halfNight);
	_times = {halfNight - ramp, halfNight + ramp, end - ramp, end + ramp};
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
	const float t = hour > 12.0f ? 24.0f - hour : hour;
	if (t < _times[0])
	{
		return 2.0f;
	}
	if (t < _times[1])
	{
		return 2.0f - (t - _times[0]) / (_times[1] - _times[0]);
	}
	if (t < _times[2])
	{
		return 1.0f;
	}
	if (t < _times[3])
	{
		return 1.0f - (t - _times[2]) / (_times[3] - _times[2]);
	}
	return 0.0f;
}
