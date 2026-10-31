/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// The lightning bolt's maths: how many of the land's cells it searches, how many forks it keeps ready, where two
// players' bolts meet when they clash, and the electric arcs a strike leaves crawling over what it hit. Free of state,
// so they are tested on their own. Wiki: docs/wiki/magic/fireball-and-lightning.md#lightning.

namespace openblack::particles::maths
{

/// A bolt searches the land's cells in a spiral out from its origin, cells this many metres across
inline constexpr float k_StrikeCellSize = 10.0f;
/// A bolt from a hand searches 4 x side x side cells, a square twice its radius across, side being radius / 10 rounded
/// up and truncated to an integer. The count is worked out as the original's 32-bit integer: a radius of 0 (or just
/// below) searches none, one of -10 or less searches the square of its negative side, and a count past the integer's
/// range searches none
[[nodiscard]] size_t StrikeSearchCells(float radius);
/// A bolt from a cloud or a parent searches a quarter as many, side x side: a square its radius across
[[nodiscard]] size_t CloudSearchCells(float radius);
/// The forks a bolt keeps ready: two for each of its targets. How many it may strike at once does not change it
[[nodiscard]] size_t ForkCount(int atOnce, size_t targets);
/// A bolt from a hand that found a creature in sight among its targets strikes only creatures: given which of its
/// targets are such creatures, each target in turn takes the next of them, going round them again when there are more
/// targets than creatures. Empty when there are none: the targets stay as they are
[[nodiscard]] std::vector<size_t> CreatureRetargets(const std::vector<bool>& creatureInSight);

/// Where a bolt from a hand points across the land: the camera's forward as the arctangent of its z over its x, with
/// no guard. The camera looking straight down gives 0 or minus 0 (x not negative) or plus or minus pi (x negative)
/// from the signs of the zeros, so the cone turns to the x axis
[[nodiscard]] float HandBoltHeading(glm::vec3 cameraForward);
/// Where a bolt points across the land: from a hand as HandBoltHeading, else along the x axis (0), whatever the camera
[[nodiscard]] float BoltHeading(bool castingFromHand, glm::vec3 cameraForward);

/// What a bolt shows another to see whether they clash
struct BoltPose
{
	glm::vec3 origin {0.0f};
	/// The middle of its targets' tips
	glm::vec3 centroid {0.0f};
	/// Where it points across the land, in radians from east towards north
	float heading {0.0f};
};
/// How far a bolt's trunk reaches towards another's middle before they meet
inline constexpr float k_ClashShare = 0.7f;
/// The widest angle off the newer bolt's heading the meeting may be at, in radians. A double: its cosine is taken in
/// double precision and rounded to a float
inline constexpr double k_ClashWidestAngle = 1.036725640296936;
/// Where a newer bolt meets an older one, if they clash: they point the same way across the land, and the meeting
/// point, on the way from the older one's middle back towards its origin, pulled towards the newer one's heading, is
/// within the newer one's reach and in front of it (the way to it made unit length first)
[[nodiscard]] std::optional<glm::vec3> ClashPoint(const BoltPose& newer, const BoltPose& older, float reach);
/// The older bolt carries on past where they meet this much thicker, fully opaque
inline constexpr float k_ClashForkScale = 3.0f;
/// Each strike of a clash hits twice as hard, for both miracles
inline constexpr float k_ClashStrength = 2.0f;

/// An electric arc crawling over what a bolt struck: from one point of its model to another, leaving and arriving along
/// the directions picked with them
struct ArcEnds
{
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
	glm::vec3 fromNormal {0.0f};
	glm::vec3 toNormal {0.0f};
};
/// The arc's length: from - to, its squares summed z, y, then x
[[nodiscard]] float ArcLength(const ArcEnds& ends);
/// An end's tangent: its normal, nudged by a point of the unit ball times the random tangent share, times the arc's
/// length times the tangent scale (that product first)
[[nodiscard]] glm::vec3 ArcTangent(glm::vec3 normal, glm::vec3 nudge, float randomTangents, float length, float scale);
/// The arc's joints, one per jitter: a cubic from one end to the other with the given tangents, joint k at t = k x (1 /
/// (count - 1)) and summed as A t^3 + B t^2, then + C t, then + D, each moved by its jitter (a point of the unit ball)
/// times the arc's length times the jitter share. The coefficients are worked out as the original does, with its
/// products by 0 and 1, so a signed zero or a NaN comes out as its own
[[nodiscard]] std::vector<glm::vec3> ArcJoints(const ArcEnds& ends, glm::vec3 fromTangent, glm::vec3 toTangent,
                                               std::span<const glm::vec3> jitters, float randomFrac);
/// A point of a model in the world: `axes` holds the model's axes (scaled) as its columns. Each coordinate is the z
/// term plus the y term, then the x term, then the position's
[[nodiscard]] glm::vec3 ArcSurfacePoint(const glm::mat3& axes, glm::vec3 position, glm::vec3 local);
/// A normal of a model as an arc takes it: through the whole matrix, the position added too, then made unit length
/// unless it is zero (squares summed z, y, then x). Its y and z sum the x term and the z term first, then the y term
[[nodiscard]] glm::vec3 ArcSurfaceNormal(const glm::mat3& axes, glm::vec3 position, glm::vec3 normal);

} // namespace openblack::particles::maths
