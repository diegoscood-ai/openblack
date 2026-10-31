/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSkinRay.h"

#include <cmath>

using namespace openblack;

namespace
{
// The game's own vector arithmetic, in its order, every step a float

glm::vec3 Sub(const glm::vec3& a, const glm::vec3& b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

glm::vec3 Cross(const glm::vec3& a, const glm::vec3& b)
{
	return {(a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x)};
}

float Dot(const glm::vec3& a, const glm::vec3& b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

/// A unit length, unless it has none
glm::vec3 Normalised(const glm::vec3& a)
{
	if (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f)
	{
		return a;
	}
	const float inverse = 1.0f / std::sqrt((a.x * a.x) + (a.y * a.y) + (a.z * a.z));
	return {a.x * inverse, a.y * inverse, a.z * inverse};
}
} // namespace

std::optional<creature_skin_ray::Hit> creature_skin_ray::Nearest(std::span<const cave_copy_pick::Part> parts,
                                                                 const glm::vec3& start, const glm::vec3& direction)
{
	std::optional<Hit> nearest;
	for (uint32_t p = 0; p < parts.size(); ++p)
	{
		const auto& part = parts[p];
		const auto& corners = part.corners;
		for (const auto& primitive : part.primitives)
		{
			if (size_t {primitive.first} + primitive.count > part.indices.size())
			{
				continue;
			}
			for (uint32_t i = primitive.first; i + 2 < primitive.first + primitive.count; i += 3)
			{
				const std::array<uint16_t, 3> ids {part.indices[i], part.indices[i + 1], part.indices[i + 2]};
				if (ids[0] >= corners.size() || ids[1] >= corners.size() || ids[2] >= corners.size())
				{
					continue;
				}
				const auto& a = corners[ids[0]];
				const auto& b = corners[ids[1]];
				const auto& c = corners[ids[2]];
				const auto ab = Sub(b, a);
				const auto ac = Sub(c, a);
				const auto normal = Normalised(Cross(ab, ac));
				const float plane = Dot(normal, a);
				const float facing = Dot(normal, direction);
				if (facing >= -k_Epsilon && facing <= k_Epsilon)
				{
					continue;
				}
				const float distance = (plane - Dot(normal, start)) / facing;
				if (distance <= 0.0f)
				{
					continue;
				}
				const glm::vec3 hit {start.x + (direction.x * distance), start.y + (direction.y * distance),
				                     start.z + (direction.z * distance)};
				const auto ah = Sub(hit, a);
				const int sides = static_cast<int>(Dot(Cross(Sub(b, a), Sub(hit, a)), normal) > 0.0f) +
				                  static_cast<int>(Dot(Cross(Sub(c, b), Sub(hit, b)), normal) > 0.0f) +
				                  static_cast<int>(Dot(Cross(Sub(a, c), Sub(hit, c)), normal) > 0.0f);
				if (sides != 0 && sides != 3)
				{
					continue;
				}
				if (nearest.has_value() && distance >= nearest->distance)
				{
					continue;
				}
				// Where on the triangle, by its two edges from the first corner, worked through the edges' x and y
				if (!(std::abs(ac.x) > k_Epsilon))
				{
					continue;
				}
				const float det = ab.y - ((ab.x * ac.y) / ac.x);
				if (!(std::abs(det) > k_Epsilon))
				{
					continue;
				}
				const float u = (ah.y - ((ah.x * ac.y) / ac.x)) / det;
				const float v = (ah.x - (ab.x * u)) / ac.x;
				if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
				{
					nearest = Hit {.spot = {.part = p, .corners = ids, .u = u, .v = v}, .distance = distance, .point = hit};
				}
			}
		}
	}
	return nearest;
}

std::optional<glm::vec3> creature_skin_ray::PointOf(std::span<const cave_copy_pick::Part> parts, const Spot& spot)
{
	if (spot.part >= parts.size())
	{
		return std::nullopt;
	}
	const auto& corners = parts[spot.part].corners;
	if (spot.corners[0] >= corners.size() || spot.corners[1] >= corners.size() || spot.corners[2] >= corners.size())
	{
		return std::nullopt;
	}
	const auto& a = corners[spot.corners[0]];
	const auto ab = Sub(corners[spot.corners[1]], a);
	const auto ac = Sub(corners[spot.corners[2]], a);
	return glm::vec3 {a.x + (ab.x * spot.u) + (ac.x * spot.v), a.y + (ab.y * spot.u) + (ac.y * spot.v),
	                  a.z + (ab.z * spot.u) + (ac.z * spot.v)};
}
