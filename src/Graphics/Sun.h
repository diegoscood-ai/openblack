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

#include <array>
#include <functional>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// Black & White's sun: a square far out to the north west, facing the island, and its glare over the view
namespace openblack::graphics::sun
{

/// Where the sun stands at an hour of script time and how strongly it shows, 0 to 255: it rises from 6 to noon to 7500
/// units high and sets as it rose, coming up from 3 and going down after 18 by a third of the way each hour. None
/// while it is down.
struct Placement
{
	glm::vec3 position;
	float alpha;
};
[[nodiscard]] std::optional<Placement> Place(float scriptHour);

/// The glare is the sun again, larger, over the finished view
inline constexpr float k_GlareScale = 1.8f;

/// Where the glare looks for what is in the way: the sun's middle and its four corners 500 units out from it, across
/// the world's x and up its y
inline constexpr std::array<glm::vec2, 5> k_GlareSamples = {{
    {0.0f, 0.0f},
    {-500.0f, -500.0f},
    {-500.0f, 500.0f},
    {500.0f, -500.0f},
    {500.0f, 500.0f},
}};
/// The samples are never lower than this
inline constexpr float k_GlareLowestSample = 10.0f;

/// The glare's strength, 0 to 255, eased towards a fifth less for each hidden sample by a hundredth of the way for
/// each millisecond of game time; it stays put while the game is paused
[[nodiscard]] float EaseGlare(float glare, int hiddenSamples, uint32_t frameMilliseconds);

/// The points the glare looks at: the sun and each sample's offset, carried on past the sun by the near clip distance
/// along the line from the camera through it, and never lower than k_GlareLowestSample
[[nodiscard]] std::array<glm::vec3, k_GlareSamples.size()> GlareSamplePoints(glm::vec3 sunPosition, glm::vec3 camera,
                                                                             float nearClip);

/// How many of the points are hidden, by the given test of each point and its place among them
[[nodiscard]] int HiddenSamples(std::span<const glm::vec3> points, const std::function<bool(size_t, const glm::vec3&)>& hidden);

/// The pixel of a screen of a resolution a point lands on, from its clip coordinates, as the glare's depth test finds
/// it: none when it is nearer the camera than the near clip, or off the screen; the screen's coordinates cut towards 0
[[nodiscard]] std::optional<glm::ivec2> SamplePixel(const glm::vec4& clip, float nearClip, glm::vec2 resolution);

/// A point the land doesn't hide is hidden by what is drawn at its pixel nearer the camera than this. The original
/// hides it where its 16-bit depth buffer, holding 1 - near / depth, is below 65000; taken as rounded to 16 bits
[[nodiscard]] float GlareHidingDepth(float nearClip);

/// The glare's drawn alpha, 0 to 255, as a whole number like the game's colour byte: the glare times the sun's alpha
/// over 255, then weakened through an overcast as the sun is (sky_dome::ThroughOvercast)
[[nodiscard]] int GlareAlpha(float glare, float sunAlpha, float overcast, bool fog);

} // namespace openblack::graphics::sun
