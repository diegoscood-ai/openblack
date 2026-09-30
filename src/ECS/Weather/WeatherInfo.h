/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <cstdint>

namespace openblack::weather
{
/// WeatherInfo (8 bytes, returned in edx:eax). The same layout is a grid cell of LH3DAtmos, the tail of an LH3DStorm
/// (+0x48) and the result of GClimate::ComputeWeather.
struct WeatherInfo
{
	int8_t temperature {0}; ///< +0 degrees
	int8_t rain {0};        ///< +1 0..100 (a storm: 100 x strength)
	int8_t snow {0};        ///< +2
	int8_t overcast {0};    ///< +3
	int8_t windX {0};       ///< +4 (x 1/8 = metres per second, fn_00771B10)
	int8_t windZ {0};       ///< +5
	int8_t snowCover {0};   ///< +6 the snow lying on the ground (SnowCover fn_0086CB80 x 0.5, fn_00834DD0)
	uint8_t stamp {0};      ///< +7 grid cells: the LH3DAtmos frame the cell was computed in (0xEDC340)
};
static_assert(sizeof(WeatherInfo) == 8);

/// The byte arithmetic of the original: an 8-bit add that wraps (the temperature, `add cl, al`) ...
[[nodiscard]] constexpr int8_t WrapAdd(int a, int b)
{
	return static_cast<int8_t>(static_cast<uint8_t>(a + b));
}
/// ... and the clamped one (-128..127, the other fields).
[[nodiscard]] constexpr int8_t ClampAdd(int a, int b)
{
	return static_cast<int8_t>(std::clamp(a + b, -128, 127));
}
/// `a + ((b - a) * w >> 8)` with an arithmetic shift and a wrapping byte add (CalcAtmos's temperature, the smooth
/// sampling and fn_00835620)
[[nodiscard]] constexpr int8_t LerpByte(int8_t a, int8_t b, int w)
{
	return WrapAdd(a, ((static_cast<int>(b) - static_cast<int>(a)) * w) >> 8);
}
} // namespace openblack::weather
