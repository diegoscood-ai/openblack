/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CollideBoxes.h"

#include <cmath>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{
/// A box this thin on any side is met by nothing
constexpr float k_LeastHalfSide = 0.0001f;
/// A direction this short in the box's space meets nothing either
constexpr float k_LeastRaySquared = 0.0001f;
/// The nearest distance before any box is met
constexpr float k_NoImpact = 1e20f;

/// The point into the box's space through its inverse matrix, as the original sums each row: the first row's terms z,
/// y, x, the other two's z, x, y, then the place
glm::vec3 IntoBox(const affine::AffineMatrix& m, const glm::vec3& p, bool withPlace)
{
	const auto& a = m.m;
	const float x = ((a[6] * p.z) + (a[3] * p.y)) + (p.x * a[0]);
	const float y = ((a[7] * p.z) + (a[1] * p.x)) + (a[4] * p.y);
	const float z = ((a[8] * p.z) + (a[2] * p.x)) + (a[5] * p.y);
	return withPlace ? glm::vec3(x + a[9], y + a[10], z + a[11]) : glm::vec3(x, y, z);
}

/// Out of the box's space through its matrix, each row's terms summed z, y, x, then the place
glm::vec3 OutOfBox(const affine::AffineMatrix& m, const glm::vec3& p, bool withPlace)
{
	const auto& a = m.m;
	const float x = ((p.z * a[6]) + (p.y * a[3])) + (p.x * a[0]);
	const float y = ((p.z * a[7]) + (p.y * a[4])) + (p.x * a[1]);
	const float z = ((p.z * a[8]) + (p.y * a[5])) + (p.x * a[2]);
	return withPlace ? glm::vec3(x + a[9], y + a[10], z + a[11]) : glm::vec3(x, y, z);
}

/// The dot product summed z, y, x
float DotZyx(const glm::vec3& a, const glm::vec3& b)
{
	return ((a.z * b.z) + (a.y * b.y)) + (a.x * b.x);
}

/// Made unit by the 3D engine's inverse square root of x^2 + y^2 + z^2, with no test of a zero length
glm::vec3 FastNormalised(const glm::vec3& v)
{
	const float scale = game_math::ModelInverseSquareRoot(((v.x * v.x) + (v.y * v.y)) + (v.z * v.z));
	return {scale * v.x, scale * v.y, scale * v.z};
}
} // namespace

std::vector<collide_boxes::Box> collide_boxes::Make(uint32_t count)
{
	return std::vector<Box>(count);
}

void collide_boxes::AddPrimitive(std::span<Box> boxes, std::span<const glm::vec3> vertices, std::span<const VertexGroup> groups,
                                 bool hasBones)
{
	const auto grow = [](Box& box, const glm::vec3& p) {
		box.min = glm::min(box.min, p);
		box.max = glm::max(box.max, p);
	};
	if (boxes.empty())
	{
		return;
	}
	if (!hasBones)
	{
		for (const auto& p : vertices)
		{
			grow(boxes.front(), p);
		}
		return;
	}
	size_t next = 0;
	for (const auto& group : groups)
	{
		for (uint32_t v = 0; v < group.vertexCount && next < vertices.size(); ++v, ++next)
		{
			if (group.bone < boxes.size())
			{
				grow(boxes[group.bone], vertices[next]);
			}
		}
	}
}

void collide_boxes::InitForCollision(std::span<Box> boxes, std::span<const affine::AffineMatrix> bones)
{
	for (size_t i = 0; i < boxes.size() && i < bones.size(); ++i)
	{
		boxes[i].matrix = bones[i];
		boxes[i].inverse = affine::Inverse(bones[i]);
	}
}

std::optional<collide_boxes::Impact> collide_boxes::NearestImpact(const Box& box, const glm::vec3& start,
                                                                  const glm::vec3& direction)
{
	const glm::vec3 half {(box.max.x - box.min.x) * 0.5f, (box.max.y - box.min.y) * 0.5f, (box.max.z - box.min.z) * 0.5f};
	if (!(half.x > k_LeastHalfSide && half.y > k_LeastHalfSide && half.z > k_LeastHalfSide))
	{
		return std::nullopt;
	}
	// the line in the box's space, scaled so that its ellipsoid is the unit sphere about the box's middle
	const auto local = IntoBox(box.inverse, start, true);
	const glm::vec3 offset {(local.x - box.min.x) / half.x - 1.0f, (local.y - box.min.y) / half.y - 1.0f,
	                        (local.z - box.min.z) / half.z - 1.0f};
	const auto turned = IntoBox(box.inverse, direction, false);
	const glm::vec3 ray {turned.x / half.x, turned.y / half.y, turned.z / half.z};
	const float a = DotZyx(ray, ray);
	if (!(a > k_LeastRaySquared))
	{
		return std::nullopt;
	}
	const float dot = DotZyx(ray, offset);
	const float b = dot + dot;
	const float discriminant = b * b - ((DotZyx(offset, offset) - 1.0f) * a) * 4.0f;
	if (!(discriminant >= 0.0f))
	{
		return std::nullopt;
	}
	const float t = (-b - std::sqrt(discriminant)) / (a + a);
	const glm::vec3 hit {offset.x + ray.x * t, offset.y + ray.y * t, offset.z + ray.z * t};
	const glm::vec3 point {(hit.x + 1.0f) * half.x + box.min.x, (hit.y + 1.0f) * half.y + box.min.y,
	                       (hit.z + 1.0f) * half.z + box.min.z};
	const glm::vec3 normal {hit.x * half.x, hit.y * half.y, hit.z * half.z};
	return Impact {.point = OutOfBox(box.matrix, point, true),
	               .normal = FastNormalised(OutOfBox(box.matrix, normal, false)),
	               .distance = t};
}

std::optional<collide_boxes::Impact> collide_boxes::NearestImpact(std::span<const Box> boxes, const glm::vec3& start,
                                                                  const glm::vec3& direction)
{
	std::optional<Impact> nearest;
	float nearestDistance = k_NoImpact;
	for (const auto& box : boxes)
	{
		if (const auto impact = NearestImpact(box, start, direction); impact.has_value() && impact->distance < nearestDistance)
		{
			nearestDistance = impact->distance;
			nearest = impact;
		}
	}
	return nearestDistance != k_NoImpact ? nearest : std::nullopt;
}
