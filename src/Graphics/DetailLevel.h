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
#include <array>
#include <cstdint>

namespace openblack::graphics
{

/// The original's graphics detail levels (fn_00823AD0, table 0x9A3704; registry "detailidx", default 4; level 5 is
/// "custom", where fn_008237B0 reads every key from the registry and the table's values are the custom defaults).
struct DetailLevel
{
	float waterTiling;     ///< sea texture period = 2000 - 1800 * WaterTiling (0: a nearly static quad, fn_0087A090)
	bool landReflection;   ///< "LandRef": the land mirrored under the sea
	bool fog;              ///< "Fog": software distance haze
	bool clouds;           ///< "Clouds" and "CloudShadows"
	bool useHighTexture;   ///< 256 px landscape textures (else 128)
	uint8_t rainSplash;    ///< "RainSplash"

	[[nodiscard]] float SeaPeriod() const { return 2000.0f - 1800.0f * waterTiling; }
};

inline constexpr std::array<DetailLevel, 7> k_DetailLevels = {{
    {0.0f, false, false, false, false, 0},
    {0.2f, false, false, false, false, 0},
    {0.4f, false, false, false, false, 3},
    {0.6f, true, true, true, false, 5},
    {0.8f, true, true, true, true, 8},
    {0.5f, true, true, true, true, 8},
    {1.0f, true, true, true, true, 8},
}};

inline constexpr uint8_t k_DefaultDetailLevel = 4;

inline const DetailLevel& GetDetailLevel(uint8_t level)
{
	return k_DetailLevels.at(std::min<size_t>(level, k_DetailLevels.size() - 1));
}

} // namespace openblack::graphics
