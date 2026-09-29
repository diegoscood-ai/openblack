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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class DayNightClock;

namespace night_lights
{

/// Abode::Draw 0x515F70: the grey (0..1) of a house's window submeshes (L3D isWindow) at night, or a negative value
/// when they are lit normally (day, or nobody at home). The windows are drawn unlit in that flat colour, which
/// flickers a little (table 0x8D86D0) and has a per-house time offset from the position.
[[nodiscard]] float WindowGrey(const DayNightClock& clock, const glm::vec3& position, bool someoneHome);

/// fn_0086C220: strength 0..255 of the village lights by script time: off from 7 to 16.5 h, fading in over
/// 16.5..17.5 h and out over 6..7 h
[[nodiscard]] float VillageLightIntensity(float scriptHour);

/// The land cells the lights are stamped into: luminosity caps with the layout of the island's cell map (the
/// shaders take min(cell luminosity, cap)), the cells' own luminosity and the green of light table[255]
struct LightCells
{
	glm::ivec2 firstCell {0}; ///< global cell of cap index 0
	glm::ivec2 size {0};
	const std::vector<uint8_t>* luminosity {nullptr};
	std::vector<uint8_t>* cap {nullptr};
	uint8_t fullLightGreen {255}; ///< [0xEDDD09]
};

/// An 8-bit N x N light image, remapped at load like GLandscape::Open does: min(47, trunc(v * 48 / 255 + 0.5))
struct LightImage
{
	int side {0};
	std::vector<uint8_t> texels;
};
[[nodiscard]] LightImage LoadLightImage(const std::vector<uint8_t>& raw);

/// fn_008229B0: stamps the image into the cells from world (x, z) (its first texel, one texel per cell) with
/// strength 0..1. A lit cell uses light-table entries 0..47 (the warm ramp); dim texels leave bright land as it is.
void StampLight(const LightImage& image, float x, float z, float intensity, LightCells& cells);

/// fn_005E5830 (hand light, village lights, lanterns on/off) and fn_00823460 / fn_00823570 (lantern flames and
/// glow), every frame after the cloud shadows: stamps into `cells` and updates the lantern sprites.
/// @param milliseconds game time of this frame (0 while paused)
/// @param baseColour light-table base colour of this frame, 0..1
void Update(float milliseconds, float scriptHour, const glm::vec3& baseColour, LightCells& cells);

/// Forgets the lights and their sprites (new map)
void Clear();

} // namespace night_lights
} // namespace openblack
