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
#include <vector>

#include <L3DFile.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The hand's own ray test of the mesh of the object under the cursor, the one that places the hand on it. It is not
/// the cursor's pick: it tests every sub-mesh of the first level of detail (whatever else its flags say), works in
/// world space with the object's matrix, takes the nearest triangle from either side, and turns the face's normal away
/// from the ray's start. Wiki: docs/bw1-notes/hand-and-interface.md, "The hand's mesh test".
///
/// The products and sums between the values the original stores are worked in double, as its float unit keeps more
/// precision than a float; every stored value is rounded to a float where it is stored.
namespace openblack::ecs::hand_mesh_ray
{

/// A plane is not hit when the ray is this close to parallel to it: |unit normal . direction| must be larger
inline constexpr float k_Parallel = 0.005f;

/// A sub-mesh as the test reads it
struct SubMesh
{
	l3d::L3DSubmeshHeader::Flags flags;
	std::span<const glm::vec3> positions; ///< in the mesh's space
	std::span<const uint16_t> indices;    ///< three per triangle, into positions
};

struct Hit
{
	float distance;   ///< t along the ray: the point is origin + t x direction
	glm::vec3 point;  ///< in world space
	glm::vec3 normal; ///< the face's unit normal in world space, turned so that normal . direction > 0
};

/// Whether a sub-mesh is tested: it carries the first level of detail. A sub-mesh with no level and without the
/// second bit of unknown2 is loaded as of every level
[[nodiscard]] constexpr bool Tested(l3d::L3DSubmeshHeader::Flags flags) noexcept
{
	const bool everyLevel = flags.lodMask == 0 && (flags.unknown2 & 0b10) == 0;
	return everyLevel || (flags.lodMask & 0b001) != 0;
}

/// A mesh-space point to world space: x' = z m[2] + y m[1] + x m[0] + m[3], each axis summed in that order
[[nodiscard]] inline glm::vec3 ToWorld(const glm::mat4& model, const glm::vec3& p) noexcept
{
	const double x = p.x;
	const double y = p.y;
	const double z = p.z;
	glm::vec3 out;
	for (int axis = 0; axis < 3; ++axis)
	{
		out[axis] = static_cast<float>(z * model[2][axis] + y * model[1][axis] + x * model[0][axis] + model[3][axis]);
	}
	return out;
}

/// Whether the point lies on the inner side of the edge from `start` to `end` of a triangle of normal n: the
/// edge x (point - start) points along n
[[nodiscard]] inline bool InsideEdge(const glm::vec3& start, const glm::vec3& end, const glm::vec3& point,
                                     const glm::vec3& n) noexcept
{
	const double ex = static_cast<double>(end.x) - start.x;
	const double ey = static_cast<double>(end.y) - start.y;
	const double ez = static_cast<double>(end.z) - start.z;
	const double wx = static_cast<double>(point.x) - start.x;
	const double wy = static_cast<double>(point.y) - start.y;
	const double wz = static_cast<double>(point.z) - start.z;
	const auto cx = static_cast<float>(ey * wz - ez * wy);
	const auto cy = static_cast<float>(ez * wx - ex * wz);
	const double cz = ex * wy - ey * wx;
	return cz * n.z + static_cast<double>(cy) * n.y + static_cast<double>(cx) * n.x > 0.0;
}

/// The ray origin + t x direction against one world-space triangle, from either side. A hit needs t > 0 and the point
/// strictly inside the three edges (a point on an edge misses)
[[nodiscard]] inline std::optional<Hit> Triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                                 const glm::vec3& origin, const glm::vec3& direction) noexcept
{
	// the normal (V1 - V0) x (V2 - V0), made unit unless it is zero
	const float e1x = v1.x - v0.x;
	const float e1y = v1.y - v0.y;
	const double e1z = static_cast<double>(v1.z) - v0.z;
	const float e2x = v2.x - v0.x;
	const float e2y = v2.y - v0.y;
	const double e2z = static_cast<double>(v2.z) - v0.z;
	const float e2yByE1x = e2y * e1x;
	glm::vec3 n;
	n.x = static_cast<float>(e2z * e1y - e1z * e2y);
	n.y = static_cast<float>(e1z * e2x - e2z * e1x);
	n.z = static_cast<float>(static_cast<double>(e2yByE1x) - static_cast<double>(e1y) * e2x);
	if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f)
	{
		const double nx = n.x;
		const double ny = n.y;
		const double nz = n.z;
		const double inverse = 1.0 / std::sqrt(nz * nz + ny * ny + nx * nx);
		n = glm::vec3(static_cast<float>(inverse * nx), static_cast<float>(inverse * ny), static_cast<float>(inverse * nz));
	}
	const auto dot = [&n](const glm::vec3& v) {
		return static_cast<double>(v.z) * n.z + static_cast<double>(v.y) * n.y + static_cast<double>(v.x) * n.x;
	};
	const double plane = dot(v0);
	const auto across = static_cast<float>(dot(direction));
	if (!(across < -k_Parallel || across > k_Parallel))
	{
		return std::nullopt;
	}
	const auto t = static_cast<float>((plane - dot(origin)) / across);
	if (!(t > 0.0f))
	{
		return std::nullopt;
	}
	const double along = t;
	const auto alongY = static_cast<float>(along * direction.y);
	const auto alongZ = static_cast<float>(along * direction.z);
	const glm::vec3 point(static_cast<float>(along * direction.x + origin.x), alongY + origin.y, alongZ + origin.z);
	int inside = 0;
	inside += InsideEdge(v0, v1, point, n) ? 1 : 0;
	inside += InsideEdge(v1, v2, point, n) ? 1 : 0;
	inside += InsideEdge(v2, v0, point, n) ? 1 : 0;
	if (inside != 0 && inside != 3)
	{
		return std::nullopt;
	}
	return Hit {.distance = t, .point = point, .normal = across > 0.0f ? n : -n};
}

/// The nearest hit of the ray (world space) with the triangles of the tested sub-meshes, the mesh drawn at `model`.
/// Sub-meshes and triangles are walked in order and a later hit replaces an earlier one only when strictly nearer
[[nodiscard]] inline std::optional<Hit> Nearest(std::span<const SubMesh> subMeshes, const glm::mat4& model,
                                                const glm::vec3& origin, const glm::vec3& direction)
{
	std::optional<Hit> best;
	std::vector<glm::vec3> world;
	for (const auto& subMesh : subMeshes)
	{
		if (!Tested(subMesh.flags))
		{
			continue;
		}
		world.resize(subMesh.positions.size());
		for (size_t i = 0; i < world.size(); ++i)
		{
			world[i] = ToWorld(model, subMesh.positions[i]);
		}
		const auto& indices = subMesh.indices;
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			if (indices[i] >= world.size() || indices[i + 1] >= world.size() || indices[i + 2] >= world.size())
			{
				continue; // (port guard)
			}
			const auto hit = Triangle(world[indices[i]], world[indices[i + 1]], world[indices[i + 2]], origin, direction);
			if (hit.has_value() && (!best.has_value() || hit->distance < best->distance))
			{
				best = hit;
			}
		}
	}
	return best;
}

/// How far along the unit direction the hand stands for a hit point: -(origin - point) . direction
[[nodiscard]] inline float AlongRay(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& point) noexcept
{
	const glm::vec3 toOrigin = origin - point;
	const double x = toOrigin.x;
	const double y = toOrigin.y;
	const double z = toOrigin.z;
	return static_cast<float>(-(z * direction.z + x * direction.x + y * direction.y));
}

} // namespace openblack::ecs::hand_mesh_ray
