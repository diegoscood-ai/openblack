/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::hand_pick
{
/// The hand's object pick, its cheapest reject: seen from above, the line of the ray (origin, direction, both given by
/// their x and z) passes farther from a sphere's centre than its radius, by a margin far above the rounding of the
/// sphere and screen-circle tests that follow (1.1 r + 1 m). The distance seen from above is never more than the
/// distance in space, so a sphere this rejects is one those tests reject too. A ray straight down measures from its
/// origin
[[nodiscard]] inline bool MissesFromAbove(glm::vec2 originXZ, glm::vec2 directionXZ, glm::vec2 centreXZ, float radius)
{
	const glm::vec2 toCentre = centreXZ - originXZ;
	const float reach = radius * 1.1f + 1.0f;
	const float length2 = glm::dot(directionXZ, directionXZ);
	if (length2 > 1e-12f)
	{
		const float cross = toCentre.x * directionXZ.y - toCentre.y * directionXZ.x;
		return cross * cross > reach * reach * length2;
	}
	return glm::dot(toCentre, toCentre) > reach * reach;
}

/// An invisible draw collision, a sphere with no mesh, against the mouse ray (`origin`, `direction`): missed when the
/// centre's view depth along the camera's `forward` is short of the near clip or the ray does not go forward; else hit
/// when the ray passes within `radius` of the centre on the plane at the centre's view depth (the mouse inside the
/// screen circle of a point `radius` off the centre), at the ray's length to that plane
[[nodiscard]] inline std::optional<float> InvisibleSphereAlong(const glm::vec3& origin, const glm::vec3& direction,
                                                               const glm::vec3& forward, float nearClip,
                                                               const glm::vec3& centre, float radius)
{
	const float depth = glm::dot(centre - origin, forward);
	const float along = glm::dot(direction, forward);
	if (depth < nearClip || along <= 0.0f)
	{
		return std::nullopt;
	}
	const float t = depth / along;
	if (glm::distance(origin + direction * t, centre) >= radius)
	{
		return std::nullopt;
	}
	return t;
}

/// An invisible draw collision's sphere
struct Sphere
{
	glm::vec3 centre;
	float radius;
};

/// How much larger than the seed graphic's size the sphere it sends for its spell icon is
constexpr float k_SpellGraphicCollideScale = 2.0f;

/// The invisible draw collision a spell icon's seed graphic sends for its icon: centred on the graphic's mesh point,
/// raised by its mesh's bounding box centre (as the mesh file has it, not scaled) when the graphic draws a mesh, of
/// radius twice the graphic's size
[[nodiscard]] inline Sphere SpellGraphicSphere(const glm::vec3& meshPoint, const std::optional<glm::vec3>& meshBoxCentre,
                                               float size)
{
	return {.centre = meshPoint + meshBoxCentre.value_or(glm::vec3(0.0f)), .radius = size * k_SpellGraphicCollideScale};
}

/// The invisible draw collision an opened reward chest sends for itself through the town's new miracle spinning above
/// it: its seed graphic's own sphere (SpellGraphicSphere), drawn selectable with the chest as its object; none while
/// the chest is not open, when the graphic is not drawn
[[nodiscard]] inline std::optional<Sphere> OpenedChestSeedSphere(bool opened, const glm::vec3& meshPoint,
                                                                 const std::optional<glm::vec3>& meshBoxCentre, float size)
{
	if (!opened)
	{
		return std::nullopt;
	}
	return SpellGraphicSphere(meshPoint, meshBoxCentre, size);
}
} // namespace openblack::ecs::hand_pick
