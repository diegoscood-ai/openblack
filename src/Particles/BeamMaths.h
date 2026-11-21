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
#include <cstdint>

#include <functional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The simple beam's maths: a few key points on the straight way from the beam's start to its end, the ones between the
// ends pushed aside by smooth noise that drifts along the beam over time and bulges most at its middle, kept above the
// land; then a smooth curve through them, with the ribbon's joints spread evenly along it, thickest at the middle. Free
// of state, so they are tested on their own.

namespace openblack::particles::maths
{

/// How a beam's key points wiggle
struct BeamWiggle
{
	/// How many times the noise changes along the beam
	float frequency {4.0f};
	/// How fast the noise drifts along the beam, a second
	float speed {1.0f};
	/// How far the key points are pushed aside
	float amount {0.1f};
	/// The key points between the ends keep at least this high above the land
	float minHeight {0.0f};
};

/// The noise across the beam drifts at the wiggle's speed, the noise across it the other way at this share of it, and
/// the noise upwards at this share
inline constexpr float k_BeamDepthDriftShare = 0.7f;
inline constexpr float k_BeamHeightDriftShare = 1.3f;
/// The upward push is half the sideways one, and only ever upwards
inline constexpr float k_BeamHeightShare = 0.5f;

/// How much of the push a point a share t of the way along the beam takes: none at the ends, all of it at the middle
[[nodiscard]] float BeamBulge(float t);

/// The beam's key points from its start to its end, count of them evenly along the way (none with fewer than two). The
/// ends are the start and the end themselves. Each point between is pushed sideways and upwards by the noise at the
/// point's share of the way times the frequency, plus the beam's age times the speed (times the shares above), plus
/// the beam's number so that no two beams wiggle alike; then raised to the minimum height above the land if under it.
[[nodiscard]] std::vector<glm::vec3> BeamKeyPoints(glm::vec3 start, glm::vec3 end, int count, const BeamWiggle& wiggle,
                                                   float age, int beam, const std::function<float(float)>& noise,
                                                   const std::function<float(glm::vec2)>& landHeight);

/// One of the ribbon's joints
struct BeamJoint
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
};

/// The ribbon's joints along a smooth curve through the key points, count of them evenly in the curve's own parameter
/// from the beam's start to its end. The curve (the key-point spline of Rules/KeyPoints.h, one for each axis, read in
/// the game's three-axis order) passes through each key point at its share of the way and leaves and arrives flat at
/// both ends; where it gives nothing a joint stays where the one before it is, the first at the last key point. Each
/// joint's scale goes from the smallest at the ends to the largest at the middle. A lone joint is not a number, as in
/// the game.
[[nodiscard]] std::vector<BeamJoint> BeamJoints(std::span<const glm::vec3> keys, size_t count, float minScale, float maxScale);

// The plasma beam's maths (Plasma): an arc from its start to its end, leaving along its start tangent and arriving
// along its end tangent, both pushed by a little randomness and lengthened with the arc, wiggled by the same drifting
// noise as the simple beam's (but not kept above the land), and faded in and out over its life. Its joints are laid
// with BeamJoints between the rule's two fork scales.

/// One plasma arc as it is asked for
struct PlasmaArc
{
	glm::vec3 start {0.0f};
	glm::vec3 end {0.0f};
	glm::vec3 startTangent {0.0f};
	glm::vec3 endTangent {0.0f};
};

/// The rule's shaping of an arc
struct PlasmaShape
{
	/// How much longer than the arc's length each tangent is drawn (ScaleTangents)
	float scaleTangents {1.0f};
	/// How far each tangent is pushed by a random point of the unit ball (RandomTangents)
	float randomTangents {0.2f};
	/// The wiggle: WiggleFreq, the drift's share of the arc's own speed (WiggleSpeed) and RandomFrac
	float frequency {4.0f};
	float speed {1.0f};
	float amount {0.1f};
};

/// The noise of a plasma's middle key points drifts by its age times the arc's speed times this, across the arc
/// the other way at 0.7 of it and upwards at 1.3 of it (the simple beam's shares)
[[nodiscard]] float PlasmaDrift(float age, float arcSpeed, float wiggleSpeed);

/// The arc's alpha at its age: none at its start and its end, the arc's alpha times MaxAlpha over 255 at the middle of
/// its life, truncated to a byte. An age before the start (or not a number) counts as its start, one past its life as
/// its end
[[nodiscard]] uint8_t PlasmaAlpha(float age, float life, uint8_t alpha, int32_t maxAlpha);

/// The arc's key points, count of them evenly in the curve's own parameter (none with fewer than two): the cubic from
/// the start to the end whose slopes are the two tangents, each first pushed by its random point times the shape's
/// randomTangents and then made the arc's length times scaleTangents. The points between the ends are pushed by the
/// noise as the simple beam's, from the drift (PlasmaDrift) and the beam's number, and are not kept above the land
[[nodiscard]] std::vector<glm::vec3> PlasmaKeyPoints(const PlasmaArc& arc, glm::vec3 startPush, glm::vec3 endPush,
                                                     const PlasmaShape& shape, int count, float drift, int beam,
                                                     const std::function<float(float)>& noise);

} // namespace openblack::particles::maths
