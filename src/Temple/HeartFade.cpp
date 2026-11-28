/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeartFade.h"

#include <limits>

using namespace openblack;

namespace
{
/// The game's float to integer, toward zero: a value it cannot hold (or not a number) gives the lowest integer
int32_t TruncateToInteger(float value)
{
	constexpr float k_Limit = 2147483648.0f;
	if (!(value > -k_Limit && value < k_Limit))
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(value);
}

/// One of a colour's channels times a level, over 256
uint32_t Scaled(uint32_t colour, uint32_t shift, uint32_t level)
{
	return ((((colour >> shift) & 0xFFu) * level) >> 8u) << shift;
}
} // namespace

std::optional<uint32_t> heart_fade::Specular(float clock, float pulse, uint32_t god)
{
	if (!(clock > k_SpecularFrom))
	{
		return std::nullopt;
	}
	float share = (clock - k_SpecularFrom) / k_SpecularOver;
	if (!(share > 0.0f))
	{
		share = 0.0f;
	}
	else if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	const float beat = share * pulse;
	const float level = beat * (k_SpecularHigh - k_SpecularLow) + k_SpecularLow;
	const auto byte = static_cast<uint32_t>(TruncateToInteger(level)) & 0xFFu;
	return (god & 0xFF000000u) | Scaled(god, 16u, byte) | Scaled(god, 8u, byte) | Scaled(god, 0u, byte);
}
