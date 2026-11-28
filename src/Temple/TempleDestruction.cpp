/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleDestruction.h"

#include <algorithm>

namespace openblack::temple_destruction
{

Events Between(float before, float after)
{
	return {
	    .loopStarts = Passes(before, k_LoopFrom, after),
	    .shellShown = Passes(before, k_FadeFrom, after),
	    .glow = Passes(before, k_GlowAt, after),
	    .explosion = Passes(before, k_ExplodeAt, after),
	    .shellGoes = Passes(before, k_ShellGoesAt, after),
	    .smoke = Passes(before, k_EndAt, after),
	    .end = Passes(before, k_EndAt, after),
	};
}

float BeamShare(float clock)
{
	const float share = (clock - k_BeamsFrom) / k_BeamsOver;
	if (share <= 0.0f)
	{
		return 0.0f;
	}
	return share >= 1.0f ? 1.0f : share;
}

namespace
{
/// From first to last by the share, a step at a time: the difference, times the share, plus the first
float Along(float first, float last, float share)
{
	const float difference = last - first;
	const float part = difference * share;
	return part + first;
}
} // namespace

float NextBeam(float beamClock, float share)
{
	const float gap = Along(k_FirstBeamGap, k_LastBeamGap, share);
	return gap + beamClock;
}

BeamLook BeamLookAt(float share)
{
	return {
	    .life = Along(k_FirstBeamLife, k_LastBeamLife, share),
	    .speed = Along(k_FirstBeamSpeed, k_LastBeamSpeed, share),
	    .alpha = static_cast<uint8_t>(static_cast<int32_t>(Along(k_FirstBeamAlpha, k_LastBeamAlpha, share))),
	};
}

uint8_t HeartAlpha(float clock)
{
	constexpr float k_Whole = 255.0f;
	const float faded = std::clamp((clock - k_FadeFrom) / k_FadeOver, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int32_t>(k_Whole - (k_Whole * faded)));
}

float HeartbeatPace(float clock)
{
	// Not yet past the first point (or a clock that compares with nothing), and from the last point on, their own pace
	if (!(clock > k_HeartbeatAt.front()))
	{
		return k_HeartbeatPace.front();
	}
	if (!(clock < k_HeartbeatAt.back()))
	{
		return k_HeartbeatPace.back();
	}
	for (size_t next = 1; next < k_HeartbeatAt.size(); ++next)
	{
		if (clock < k_HeartbeatAt.at(next))
		{
			const float into = clock - k_HeartbeatAt.at(next - 1);
			const float width = k_HeartbeatAt.at(next) - k_HeartbeatAt.at(next - 1);
			const float share = into / width;
			return Along(k_HeartbeatPace.at(next - 1), k_HeartbeatPace.at(next), share);
		}
	}
	return k_HeartbeatPace.back();
}

float GlowPower(float pulse)
{
	return Along(k_GlowPowerLow, k_GlowPowerHigh, pulse);
}

int32_t TurnsFor(uint32_t millisecondsPerTurn, float seconds)
{
	return static_cast<int32_t>(static_cast<float>(1000u / millisecondsPerTurn) * seconds);
}

int32_t SmokeTurns(uint32_t millisecondsPerTurn, float share)
{
	// The share and its start, times the scale, then times the whole turns a second
	const float shares = share + k_SmokeShareFrom;
	const float seconds = shares * k_SmokeSecondsScale;
	return static_cast<int32_t>(static_cast<float>(1000u / millisecondsPerTurn) * seconds);
}

} // namespace openblack::temple_destruction
