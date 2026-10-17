/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The land under the hand's cursor and how far from the view the hand is drawn. The game looks for the land along the
/// line through the cursor's pixel moved forward along itself, so land closer to the eye than that is not found, and
/// takes the hand's distance from the view from the point it finds. Every step is a float operation in the game's
/// order (its FPU runs at 24 bits), so the results are bit-identical to the game's. Pure maths, no Locator.
namespace openblack::ecs::hand_land_point
{

/// The hand's size along the view, per unit of its scale, kept off the distance when the hand stands on the land
constexpr float k_HandDepthPerScale = 3.2f;
/// Land lower than this, the sea and a lake's floor included, keeps the hand's full distance
constexpr float k_LowestStandingHeight = 0.1f;
/// The hand is never drawn nearer the view than this
constexpr float k_NearestViewDistance = 2.0f;
/// Where the game makes the hand, which it stands for until the hand is first placed
constexpr glm::vec3 k_HandStartPosition {10.0f, 10.0f, 10.0f};

/// The length of a vector, summed as z squared plus y squared, then x squared, before the square root
[[nodiscard]] float LengthZyx(glm::vec3 v);

/// The distance between two points, the differences squared and summed as x plus z, then y, before the square root
[[nodiscard]] float DistanceXzy(glm::vec3 from, glm::vec3 to);

/// A line between two points
struct Line
{
	glm::vec3 from;
	glm::vec3 to;
};

/// The line from the eye through a pixel's near point, moved forward along itself by `depth` (by the near clip when the
/// depth is not above 0, a NaN depth included): both ends go forward by the unit eye-to-point direction times the depth.
/// A zero direction is not normalised, and leaves both ends where they are
[[nodiscard]] Line ShiftedAlongView(glm::vec3 eye, glm::vec3 nearPoint, float depth, float nearClip);

/// The point `length` from the eye towards a pixel's near point: the eye plus the direction times length over its
/// length. A zero direction gives the eye
[[nodiscard]] glm::vec3 AlongMouseRay(glm::vec3 eye, glm::vec3 nearPoint, float length);

/// What the hand's land point is worked out from
struct Inputs
{
	glm::vec3 eye;
	/// The cursor pixel's point on the near plane (screen_point::PixelPoint at depth 0)
	glm::vec3 nearPoint;
	/// Where the land or the near sea meets the cursor's line moved forward by the near clip plus 1
	/// (land_line::LandOrSeaAlong over ShiftedAlongView), its x and z
	std::optional<glm::vec2> hitXZ;
	/// Where the hand was placed last
	glm::vec3 handPosition;
	/// The hand is holding something, its spring on
	bool holdingWithSpring;
	/// Keep the hand's own size off the distance when it stands on the land
	bool subtractHand;
	float handScale;
	/// (inferred) 1 for the hand
	float handSize;
	/// The farthest the hand goes from the view
	float reach;
};

/// The hand's land point, its distance from the view, and whether a point was found (the land, the near sea, or the
/// reach along the cursor's ray while holding with the spring on)
struct Result
{
	glm::vec3 point;
	float viewDistance;
	bool found;
};

/// The land point under the cursor and the hand's distance from the view. With no land under the cursor the point is
/// the hand's position, or while holding with the spring on the point `reach` along the cursor's ray. When a point is
/// found and stands at or above the lowest standing height, and `subtractHand`, the hand's depth (its scale times
/// 3.2, times its size) comes off the distance. The distance is then kept between 2 (a NaN gives 2) and the reach.
/// `heightAt` gives the land's height at an x, z
[[nodiscard]] Result HandLandPoint(const Inputs& in, const std::function<float(glm::vec2)>& heightAt);

/// The hand's distance from the view as the hand keeps it with its scale: not above 2 (a NaN too) gives 2, not below
/// the reach gives the reach
[[nodiscard]] float ClampedViewDistance(float distance, float reach);

/// The hand's scale for its distance from the view, the distance first kept between 2 and the reach: (d / 10)^0.8 nearer
/// than 10, else 1, and beyond 150 times (d / 150)(1 - 0.3 (d - 150) / 1650)
[[nodiscard]] float ScaleForViewDistance(float distance, float reach);

} // namespace openblack::ecs::hand_land_point
