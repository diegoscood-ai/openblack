/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashRope.h"

#include <cmath>

#include <algorithm>
#include <tuple>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::leash_rope;

namespace
{
/// The rope is divided into one more segment than it has masses
constexpr float k_Segments = static_cast<float>(k_NodeCount + 1);
/// Lengths are shared out among the segments by multiplying by this, as the original does, not by dividing
constexpr float k_OneOverSegments = 1.0f / k_Segments;
/// The rope is laid this far over the ground, the default look's half width, whatever look it then takes
constexpr float k_LayHeight = Look {}.halfWidth;
/// Shorter than this, a direction is no direction
constexpr float k_Degenerate = 1e-6f;
/// How fast the texture repeats along the rope, per unit of length and the look's scale
constexpr float k_UScale = 0.05f;

/// A vector's squared length summed as the original does, z first, then y, then x: one rounding per operation, each
/// its own statement so that no multiply and add are fused into one
float SquaredLengthZYX(const glm::vec3& v)
{
	const float zz = v.z * v.z;
	const float yy = v.y * v.y;
	const float xx = v.x * v.x;
	const float zy = zz + yy;
	return zy + xx;
}

float LengthZYX(const glm::vec3& v)
{
	return std::sqrt(SquaredLengthZYX(v));
}

/// The pull of a spring from p towards a neighbour: none while the segment is no longer than its rest, so that the
/// rope can go slack but not be squashed
glm::vec3 SpringPull(const glm::vec3& p, const glm::vec3& neighbour, float rest, float& stretch)
{
	const auto d = neighbour - p;
	const auto length = LengthZYX(d);
	if (length <= rest || length <= 0.0f)
	{
		stretch = 0.0f;
		return glm::vec3(0.0f);
	}
	stretch = (length / rest) - 1.0f;
	// The point at the rest length from p towards the neighbour, and the pull from it on to the neighbour
	const auto restPoint = (d * (rest / length)) + p;
	return k_Stiffness * (neighbour - restPoint);
}

/// The force on each mass, from the air, gravity and the springs to its neighbours
std::array<glm::vec3, k_NodeCount> Forces(Rope& rope, float rest)
{
	std::array<glm::vec3, k_NodeCount> forces {};
	// The air's drag is the speed times the velocity, scaled down, and divided by the segment's length by multiplying
	// by its reciprocal
	const auto overDragLength = 1.0f / std::max(rest, k_MinDragLength);
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		auto& node = rope.nodes.at(i);
		const auto& previous = i == 0 ? rope.start : rope.nodes.at(i - 1).position;
		const auto& next = i + 1 == k_NodeCount ? rope.end : rope.nodes.at(i + 1).position;
		const auto speed = LengthZYX(node.velocity);
		auto force = (-k_Drag * (speed * node.velocity)) * overDragLength;
		force.y += k_GravityForce;
		// Both springs write the mass's stretch, so the one to the next point is kept
		force += SpringPull(node.position, previous, rest, node.stretch);
		force += SpringPull(node.position, next, rest, node.stretch);
		forces.at(i) = force;
	}
	return forces;
}

void Integrate(Rope& rope, const std::array<glm::vec3, k_NodeCount>& forces, const GroundHeight& ground)
{
	constexpr float k_MaxSpeedSquared = k_MaxSpeed * k_MaxSpeed;
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		auto& node = rope.nodes.at(i);
		node.velocity += (k_StepSeconds / k_Mass) * forces.at(i);
		const auto speedSquared = SquaredLengthZYX(node.velocity);
		if (speedSquared > k_MaxSpeedSquared)
		{
			node.velocity *= k_MaxSpeed / std::sqrt(speedSquared);
		}
		node.position += k_StepSeconds * node.velocity;
		const auto floor = ground(glm::vec2(node.position.x, node.position.z)) + rope.look.halfWidth + k_GroundClearance;
		node.position.y = std::max(node.position.y, floor);
	}
}
} // namespace

float leash_rope::RestLength(float slackLength)
{
	return slackLength * k_OneOverSegments;
}

Rope leash_rope::Create(const glm::vec3& start, const glm::vec3& end, float slackLength, float maxLength, const Look& look,
                        const GroundHeight& ground)
{
	Rope rope {
	    .start = start,
	    .end = end,
	    .nodes = {},
	    .slackLength = slackLength,
	    .maxLength = maxLength,
	    .look = look,
	    .tension = 0.0f,
	};
	// Each mass is one step on from the one before. A mass is laid where the line is, and only then is the line lifted
	// over the ground, so a mass under it stays there and the next ones are laid higher.
	const auto step = (end - start) * k_OneOverSegments;
	auto point = step + start;
	for (auto& node : rope.nodes)
	{
		const auto floor = ground(glm::vec2(point.x, point.z)) + k_LayHeight;
		node.position = point;
		if (point.y < floor)
		{
			point.y = floor;
		}
		point += step;
	}
	rope.tension = Tension(rope);
	return rope;
}

uint32_t leash_rope::SubSteps(float seconds)
{
	// at least one step, also with no time: a paused game's rope still follows its ends
	return 1U + static_cast<uint32_t>(std::max(seconds, 0.0f) * k_StepsPerSecond);
}

glm::vec3 leash_rope::ClampEnd(const glm::vec3& point)
{
	return {std::clamp(point.x, k_MinAcross, k_MaxAcross), std::clamp(point.y, k_MinHeight, k_MaxHeight),
	        std::clamp(point.z, k_MinAcross, k_MaxAcross)};
}

void leash_rope::Step(Rope& rope, const glm::vec3& start, const glm::vec3& end, float seconds, const GroundHeight& ground)
{
	const auto targetStart = ClampEnd(start);
	const auto targetEnd = ClampEnd(end);
	const auto steps = SubSteps(seconds);
	const auto rest = RestLength(rope.slackLength);
	// Each step moves the ends by the same share of the way, added to where they are, so they come close to where they
	// go and are put there after the last step
	const auto share = 1.0f / static_cast<float>(steps);
	const auto startMove = (targetStart - rope.start) * share;
	const auto endMove = (targetEnd - rope.end) * share;
	for (uint32_t step = 0; step < steps; ++step)
	{
		rope.start = startMove + rope.start;
		rope.end = endMove + rope.end;
		Integrate(rope, Forces(rope, rest), ground);
	}
	rope.start = targetStart;
	rope.end = targetEnd;
	rope.tension = Tension(rope);
}

float leash_rope::Tension(const Rope& rope)
{
	const auto rest = RestLength(rope.slackLength);
	const auto taut = rope.maxLength * k_OneOverSegments;
	if (taut <= rest)
	{
		return 0.0f;
	}
	const auto first = LengthZYX(rope.nodes.front().position - rope.start);
	return std::clamp((first - rest) / (taut - rest), 0.0f, 1.0f);
}

glm::vec3 leash_rope::Point(const Rope& rope, size_t i)
{
	if (i == 0)
	{
		return rope.start;
	}
	if (i >= k_PointCount - 1)
	{
		return rope.end;
	}
	return rope.nodes.at(i - 1).position;
}

Ribbon leash_rope::BuildRibbon(const Rope& rope, const glm::vec3& eye, const GroundHeight& ground)
{
	Ribbon ribbon {};
	float travelled = 0.0f;
	for (size_t i = 0; i < k_PointCount; ++i)
	{
		const auto point = Point(rope, i);
		if (i > 0)
		{
			travelled += glm::distance(point, Point(rope, i - 1));
		}
		// The rope's direction at a point is from the point before it to the point after it
		auto tangent = Point(rope, std::min(i + 1, k_PointCount - 1)) - Point(rope, i == 0 ? 0 : i - 1);
		if (glm::length(tangent) < k_Degenerate)
		{
			tangent = glm::vec3(0.0f, 0.0f, 1.0f);
		}
		auto side = glm::cross(eye - point, tangent);
		side = glm::length(side) < k_Degenerate ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::normalize(side);
		side *= rope.look.halfWidth;
		const auto u = travelled * rope.look.uScale * k_UScale;
		ribbon.rope.at(i * 2) = {.position = point + side, .uv = {u, rope.look.v1}, .alpha = 1.0f};
		ribbon.rope.at((i * 2) + 1) = {.position = point - side, .uv = {u, rope.look.v0}, .alpha = 1.0f};

		// The shadow lies flat, across the rope's direction over the land
		auto flat = glm::vec3(-tangent.z, 0.0f, tangent.x);
		flat = glm::length(flat) < k_Degenerate ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::normalize(flat);
		flat *= rope.look.halfWidth;
		const bool atEnd = i == 0 || i == k_PointCount - 1;
		const auto alpha = atEnd ? 0.0f : static_cast<float>(k_ShadowAlpha) / 255.0f;
		for (const auto& [corner, offset, v] : {std::tuple {0, flat, rope.look.v1}, std::tuple {1, -flat, rope.look.v0}})
		{
			auto position = point + offset;
			position.y = ground(glm::vec2(position.x, position.z)) + k_ShadowLift;
			ribbon.shadow.at((i * 2) + static_cast<size_t>(corner)) = {.position = position, .uv = {u, v}, .alpha = alpha};
		}
	}
	return ribbon;
}

std::array<uint16_t, k_RibbonIndexCount> leash_rope::RibbonIndices()
{
	std::array<uint16_t, k_RibbonIndexCount> indices {};
	size_t index = 0;
	for (uint16_t i = 0; i + 1 < static_cast<uint16_t>(k_PointCount); ++i)
	{
		const auto a = static_cast<uint16_t>(i * 2);
		for (const auto offset : {0, 1, 2, 2, 1, 3})
		{
			indices.at(index++) = static_cast<uint16_t>(a + offset);
		}
	}
	return indices;
}
