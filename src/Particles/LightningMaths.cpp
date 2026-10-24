/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightningMaths.h"

#include <cmath>
#include <cstdint>

#include "3D/MapCoords.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// factor x side x side cells, side the radius in cells rounded up, in the original's 32-bit arithmetic; a count that
/// is not above 0 searches none
size_t SearchCells(float radius, uint32_t factor)
{
	const auto side = static_cast<uint32_t>(map_coords::FtoL(std::ceil(radius / maths::k_StrikeCellSize)));
	const auto cells = static_cast<int32_t>(factor * side * side);
	return cells > 0 ? static_cast<size_t>(cells) : 0;
}

/// The vector made unit length in place, its length returned; a zero vector stays zero with length 0. The squares
/// are summed z, y, then x
float Normalise(glm::vec3& v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return 0.0f;
	}
	const float length = std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	const float inverse = 1.0f / length;
	v = glm::vec3(v.x * inverse, v.y * inverse, v.z * inverse);
	return length;
}
} // namespace

size_t maths::StrikeSearchCells(float radius)
{
	return SearchCells(radius, 4);
}

size_t maths::CloudSearchCells(float radius)
{
	return SearchCells(radius, 1);
}

size_t maths::ForkCount(int /*atOnce*/, size_t targets)
{
	return 2 * targets;
}

std::vector<size_t> maths::CreatureRetargets(const std::vector<bool>& creatureInSight)
{
	std::vector<size_t> creatures;
	for (size_t i = 0; i < creatureInSight.size(); ++i)
	{
		if (creatureInSight[i])
		{
			creatures.push_back(i);
		}
	}
	if (creatures.empty())
	{
		return {};
	}
	std::vector<size_t> retargets(creatureInSight.size());
	for (size_t i = 0; i < retargets.size(); ++i)
	{
		retargets[i] = creatures[i % creatures.size()];
	}
	return retargets;
}

std::optional<glm::vec3> maths::ClashPoint(const BoltPose& newer, const BoltPose& older, float reach)
{
	auto d1 = older.centroid - older.origin;
	auto d2 = older.centroid - newer.origin;
	const float length1 = Normalise(d1);
	const float length2 = Normalise(d2);
	const float c = std::cos(newer.heading);
	const float s = std::sin(newer.heading);
	// They must point the same way across the land
	if (!(std::cos(older.heading) * c + std::sin(older.heading) * s > 0.0f))
	{
		return std::nullopt;
	}
	// (d1 + (c, 0, s)) x 0.5 x the share x the shorter length (length1 when it is the smaller, else length2)
	const glm::vec3 sum(d1.x + c, d1.y, d1.z + s);
	const glm::vec3 half = sum * 0.5f;
	const glm::vec3 scaled = half * k_ClashShare;
	const float shorter = length1 < length2 ? length1 : length2;
	const glm::vec3 meeting = older.centroid - scaled * shorter;
	auto toMeeting = meeting - newer.origin;
	const float distance = Normalise(toMeeting);
	if (!(reach > distance))
	{
		return std::nullopt;
	}
	// In front of the newer bolt: the cosine of the widest angle is below the dot of the unit way with its heading
	if (!(static_cast<float>(std::cos(k_ClashWidestAngle)) < toMeeting.z * s + toMeeting.x * c))
	{
		return std::nullopt;
	}
	return meeting;
}

float maths::ArcLength(const ArcEnds& ends)
{
	const auto d = ends.from - ends.to;
	return std::sqrt((d.z * d.z + d.y * d.y) + d.x * d.x);
}

glm::vec3 maths::ArcTangent(glm::vec3 normal, glm::vec3 nudge, float randomTangents, float length, float scale)
{
	return (normal + nudge * randomTangents) * (length * scale);
}

std::vector<glm::vec3> maths::ArcJoints(const ArcEnds& ends, glm::vec3 fromTangent, glm::vec3 toTangent,
                                        std::span<const glm::vec3> jitters, float randomFrac)
{
	const auto& p0 = ends.from;
	const auto& p1 = ends.to;
	const auto& m0 = fromTangent;
	const auto& m1 = toTangent;
	// The cubic's coefficients, each sum in the original's order
	const glm::vec3 a = ((-2.0f * p1 + 2.0f * p0) + m0) + m1;
	const glm::vec3 b = ((3.0f * p1 + -3.0f * p0) + -2.0f * m0) + -1.0f * m1;
	const glm::vec3 c = ((p0 * 0.0f + p1 * 0.0f) + m0 * 1.0f) + glm::vec3(0.0f);
	const glm::vec3 d = ((p0 * 1.0f + p1 * 0.0f) + m0 * 0.0f) + m1 * 0.0f;
	const float length = ArcLength(ends);
	const float step = 1.0f / (static_cast<float>(jitters.size()) - 1.0f);
	std::vector<glm::vec3> joints;
	joints.reserve(jitters.size());
	for (size_t k = 0; k < jitters.size(); ++k)
	{
		const float t = static_cast<float>(k) * step;
		const float t2 = t * t;
		const float t3 = t2 * t;
		const glm::vec3 curve = ((a * t3 + b * t2) + c * t) + d;
		joints.push_back(curve + (jitters[k] * length) * randomFrac);
	}
	return joints;
}

glm::vec3 maths::ArcSurfacePoint(const glm::mat3& axes, glm::vec3 position, glm::vec3 local)
{
	return {((local.z * axes[2].x + local.y * axes[1].x) + local.x * axes[0].x) + position.x,
	        ((local.z * axes[2].y + local.y * axes[1].y) + local.x * axes[0].y) + position.y,
	        ((local.z * axes[2].z + local.y * axes[1].z) + local.x * axes[0].z) + position.z};
}

glm::vec3 maths::ArcSurfaceNormal(const glm::mat3& axes, glm::vec3 position, glm::vec3 normal)
{
	glm::vec3 turned {((normal.z * axes[2].x + normal.y * axes[1].x) + normal.x * axes[0].x) + position.x,
	                  ((normal.x * axes[0].y + normal.z * axes[2].y) + normal.y * axes[1].y) + position.y,
	                  ((normal.x * axes[0].z + normal.z * axes[2].z) + normal.y * axes[1].z) + position.z};
	Normalise(turned);
	return turned;
}
