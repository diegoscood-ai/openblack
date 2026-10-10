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

#include <algorithm>

#include "ECS/Components/Weather.h"

namespace openblack::weather
{
/// The weather at a point (ECS/Components/Weather.h): a grid cell of the atmosphere, the tail of a storm and the result
/// of climate::ComputeWeather
using ecs::components::WeatherInfo;
static_assert(sizeof(WeatherInfo) == 8);

/// The byte arithmetic of the original: an 8-bit add that wraps (the temperature) ...
[[nodiscard]] constexpr int8_t WrapAdd(int a, int b)
{
	return static_cast<int8_t>(static_cast<uint8_t>(a + b));
}
/// ... and the clamped one (-128..127, the other fields).
[[nodiscard]] constexpr int8_t ClampAdd(int a, int b)
{
	return static_cast<int8_t>(std::clamp(a + b, -128, 127));
}
/// `a + ((b - a) * w >> 8)` with an arithmetic shift and a wrapping byte add (the atmosphere's temperature and the
/// smooth sampling)
[[nodiscard]] constexpr int8_t LerpByte(int8_t a, int8_t b, int w)
{
	return WrapAdd(a, ((static_cast<int>(b) - static_cast<int>(a)) * w) >> 8);
}
} // namespace openblack::weather
