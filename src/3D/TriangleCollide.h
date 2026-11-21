/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

/// The hand's triangle test of a drawn mesh under the mouse, as the game makes it while it draws the object. Wiki:
/// docs/wiki/hand/picking.md#object-under-the-cursor.
namespace openblack::triangle_collide
{

/// A primitive's triangles in the index list: its first index, its index count, and whether its material is
/// two-sided
struct Range
{
	uint32_t first {0};
	uint32_t count {0};
	bool twoSided {false};
};

/// Whether the pick tests a sub-mesh, as the game's draw collision of an object takes its sub-meshes: one of the
/// level of detail the object is drawn at (openblack draws the first, bit 0 of the loaded level mask; a sub-mesh
/// with no level bit is never tested), whose status the object's status shows (no more than it), and (openblack)
/// never a physics one
[[nodiscard]] constexpr bool Tested(bool physics, uint32_t lodMask, uint32_t subMeshStatus, uint32_t objectStatus) noexcept
{
	return !physics && (lodMask & 1u) != 0 && subMeshStatus <= objectStatus;
}

/// Where a ray meets a triangle and the triangle's unit normal ((V1 - V0) x (V2 - V0) normalised, left as it is when
/// zero)
struct Hit
{
	float t {0.0f};
	glm::vec3 normal {0.0f};
};

/// How far along the ray origin + t * direction (t > 0) it meets the triangle a, b, c, by either face; none when it
/// misses
[[nodiscard]] inline std::optional<float> Along(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& a,
                                                const glm::vec3& b, const glm::vec3& c) noexcept
{
	const auto e1 = b - a;
	const auto e2 = c - a;
	const auto p = glm::cross(direction, e2);
	const float det = glm::dot(e1, p);
	if (std::abs(det) < 1e-12f)
	{
		return std::nullopt;
	}
	const float inv = 1.0f / det;
	const auto s0 = origin - a;
	const float u = glm::dot(s0, p) * inv;
	if (u < 0.0f || u > 1.0f)
	{
		return std::nullopt;
	}
	const auto q = glm::cross(s0, e1);
	const float v = glm::dot(direction, q) * inv;
	if (v < 0.0f || u + v > 1.0f)
	{
		return std::nullopt;
	}
	const float t = glm::dot(e2, q) * inv;
	if (t <= 0.0f)
	{
		return std::nullopt;
	}
	return t;
}

/// The triangle the pick takes along a ray from the eye (`origin`): the first one in the order of the primitives and
/// their triangles that the ray meets, not the nearest, as the game takes the first triangle covering the mouse's
/// pixel. A one-sided primitive's triangles that face away from the eye ((V1 - V0) x (V2 - V0) not towards it) are
/// left out first, as the game drops the back faces before the test; a two-sided one's count by either face
[[nodiscard]] inline std::optional<Hit> First(std::span<const glm::vec3> positions, std::span<const uint16_t> indices,
                                              std::span<const Range> ranges, const glm::vec3& origin,
                                              const glm::vec3& direction) noexcept
{
	for (const auto& range : ranges)
	{
		for (uint32_t i = range.first; i + 2 < range.first + range.count && i + 2 < indices.size(); i += 3)
		{
			if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size())
			{
				continue;
			}
			const auto& a = positions[indices[i]];
			const auto& b = positions[indices[i + 1]];
			const auto& c = positions[indices[i + 2]];
			const auto n = glm::cross(b - a, c - a);
			if (!range.twoSided && glm::dot(n, origin - a) <= 0.0f)
			{
				continue;
			}
			if (const auto t = Along(origin, direction, a, b, c); t.has_value())
			{
				return Hit {.t = *t, .normal = n.x != 0.0f || n.y != 0.0f || n.z != 0.0f ? glm::normalize(n) : n};
			}
		}
	}
	return std::nullopt;
}

} // namespace openblack::triangle_collide
