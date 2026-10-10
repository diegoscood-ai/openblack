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

#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// At night the hand's light glows warm on the water beneath it: a square of 120 units on the sea level under the hand,
/// drawn with an additive material before the sea, so it shows through it like the rest of what is under the water.
namespace openblack::graphics::hand_water_glow
{

inline constexpr float k_HalfSize = 60.0f;
/// How far around the hand the land is looked at for water, in each direction: the half size and one cell
inline constexpr float k_Reach = 70.0f;
/// Land under this altitude is low enough for the water to show
inline constexpr uint8_t k_LowAltitude = 5;
inline constexpr uint32_t k_MaxAlpha = 190;
/// Where the glow is in the atmosphere texture: a 12 by 12 texel spot
inline constexpr glm::vec2 k_UvMinimum {0.75f, 0.375f};
inline constexpr glm::vec2 k_UvMaximum {0.796875f, 0.421875f};
/// The hand's light must be stronger than this for the glow to show
inline constexpr float k_MinimumStrength = 0.01f;

/// The glow's colour, 0xAARRGGBB: the palette's warm colour (0xRRGGBB) a quarter of the way to orange, rounding down,
/// and as opaque as the hand's light is strong, up to 190
[[nodiscard]] uint32_t Colour(uint32_t warmColour, float strength);

/// Whether the glow shows: never inside the temple, which has no sea for it to lie on, and outside only while the hand's
/// light, 0 to 1, is strong enough
[[nodiscard]] bool Shows(bool inTemple, float strength);

/// The altitude of the land's cell at a cell's x and z, none where there is no land
using AltitudeAt = std::function<std::optional<uint16_t>(int x, int z)>;

/// The map's last cell along each side
inline constexpr int k_LastCell = 511;

/// Whether there is water near the hand: any cell within reach is low or not land at all. The cells start no lower
/// than the map's first and no higher than its last, but may run past its last.
[[nodiscard]] bool NearLowLand(glm::vec2 xz, const AltitudeAt& altitudeAt, int lastCell = k_LastCell);
/// The same on an island, whose last cell is its own
[[nodiscard]] bool NearLowLand(const LandIslandInterface& island, glm::vec2 xz);

struct Quad
{
	glm::vec2 centre; ///< world x, z; the quad lies on y = 0
	uint32_t argb;    ///< the colour of its four vertices
};

/// The glow of this frame, if any: the hand's light strong enough (HandLight::GetStrength of the land's colour, 0 to 1)
/// and low land near the hand
[[nodiscard]] std::optional<Quad> Compute(const LandIslandInterface& island, const glm::vec3& landColour, uint32_t warmColour,
                                          const glm::vec3& handPosition);

} // namespace openblack::graphics::hand_water_glow
