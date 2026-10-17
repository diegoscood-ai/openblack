/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpellCreature.h"

using namespace openblack::magic;
using namespace openblack::magic::falling_spell;

namespace
{
// 255 x the film ms in 32 bits, wrapping, as a signed number
int32_t Times255(int32_t ms) noexcept
{
	return static_cast<int32_t>(static_cast<uint32_t>(ms) * 0xFFu);
}

// One channel of the diffuse colour: 255 less k x level / 256 rounded up
uint32_t Darkened(uint32_t k, uint32_t level) noexcept
{
	return 0xFFu - (k * level + 0xFFu) / 0x100u;
}

// A ramp of a window: the ms into it while rising over 1000 ms, then 1000 less half the ms past that (truncated toward
// zero) over 2000 ms; 0 outside
int32_t RiseAndFall(int32_t ms, int32_t start) noexcept
{
	if (ms >= start && ms < start + 1000)
	{
		return ms - start;
	}
	if (ms >= start + 1000 && ms < start + 3000)
	{
		return 1000 - (ms - (start + 1000)) / 2;
	}
	return 0;
}

// The ms into a rise-only window of 1000 ms; 0 outside
int32_t Rise(int32_t ms, int32_t start) noexcept
{
	return ms >= start && ms < start + 1000 ? ms - start : 0;
}
} // namespace

int32_t falling_spell::CreatureTintLevel(int32_t filmMs) noexcept
{
	int32_t level = 0;
	if (filmMs < k_TintRiseMs)
	{
		level = Times255(filmMs) / k_TintRiseMs;
	}
	else if (filmMs < k_CreatureHiddenToMs)
	{
		level = Times255(filmMs - k_TintRiseMs) / k_TintSecondRiseMs;
	}
	// held to 0..255
	if (level <= 0)
	{
		return 0;
	}
	return level < 0xFF ? level : 0xFF;
}

CreatureTint falling_spell::TintOf(int32_t level, const Flicker& flicker) noexcept
{
	const auto v = static_cast<uint32_t>(level);
	// the flicker colour (0x20 + red, 8 + green, blue), each channel x level / 256
	const uint32_t red = ((0x20u + flicker.red) * v) >> 8;
	const uint32_t green = ((8u + flicker.green) * v) >> 8;
	const uint32_t blue = (flicker.blue * v) >> 8;
	// white darkened by (127, 191, 255) x level / 256, rounded up: (0x80, 0x40, 0) at 255
	return {
	    .colour = 0xFF000000u | (Darkened(0x7Fu, v) << 16) | (Darkened(0xBFu, v) << 8) | Darkened(0xFFu, v),
	    .specular = 0xFF000000u | ((red & 0xFFu) << 16) | ((green & 0xFFu) << 8) | (blue & 0xFFu),
	};
}

std::optional<CreatureTint> falling_spell::CreatureTintAt(int32_t filmMs, const std::function<uint32_t(int32_t)>& localRand)
{
	if (CreatureHidden(filmMs))
	{
		return std::nullopt;
	}
	const int32_t level = CreatureTintLevel(filmMs);
	// the three draws in their order: red, green, blue
	Flicker flicker;
	flicker.red = localRand(k_FlickerRedRange);
	flicker.green = localRand(k_FlickerGreenRange);
	flicker.blue = localRand(k_FlickerBlueRange);
	return TintOf(level, flicker);
}

std::optional<float> falling_spell::GlowPower(FallGlow glow, int32_t filmMs) noexcept
{
	// the ms into the glow's windows (they do not overlap)
	int32_t into = 0;
	if (glow == FallGlow::First)
	{
		into = RiseAndFall(filmMs, 16350) + Rise(filmMs, 31650);
	}
	else
	{
		into = RiseAndFall(filmMs, 17200) + Rise(filmMs, 36500);
	}
	// no change at 0
	if (into == 0)
	{
		return std::nullopt;
	}
	// x = into x 0.001 held to 0..1, the power 2x + 1
	float x = static_cast<float>(into) * 0.001f;
	if (x <= 0.0f)
	{
		x = 0.0f;
	}
	else if (!(x < 1.0f))
	{
		x = 1.0f;
	}
	return (x + x) + 1.0f;
}

int32_t falling_spell::CreatureAnimationMs(int32_t milliseconds, float size, bool filmTimed) noexcept
{
	if (filmTimed)
	{
		return milliseconds;
	}
	// in doubles, as the original's floating point unit works at a double's precision: half the size times 0.85, from
	// 1.6, times the milliseconds, truncated toward zero
	constexpr float k_Half = 0.5f;
	constexpr float k_SlowingPerHalfSize = 0.85f;
	constexpr float k_RateAtSizeZero = 1.6f;
	const double rate = static_cast<double>(k_RateAtSizeZero) -
	                    ((static_cast<double>(size) * static_cast<double>(k_Half)) * static_cast<double>(k_SlowingPerHalfSize));
	return static_cast<int32_t>(static_cast<double>(milliseconds) * rate);
}
