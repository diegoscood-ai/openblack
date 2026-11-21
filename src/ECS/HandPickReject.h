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

#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/ScreenPick.h"

namespace openblack::ecs::hand_pick
{
/// An object's draw collision under the cursor: its drawn primitives in the order they are drawn, each projected and
/// tested on the screen (screen_pick::FirstHit: clipped to the view, facing away dropped unless two-sided, the first
/// triangle covering the cursor's pixel), the first primitive that covers it giving the object's view depth. As the
/// original's, the test stops at that first hit: a nearer triangle of a later primitive is not looked for
[[nodiscard]] inline std::optional<float> FirstHitAmong(const screen_pick::View& view,
                                                        std::span<const screen_pick::Primitive> primitives,
                                                        screen_pick::PickScratch& scratch)
{
	for (const auto& primitive : primitives)
	{
		if (const auto depth = screen_pick::FirstHit(view, primitive, scratch); depth.has_value())
		{
			return depth;
		}
	}
	return std::nullopt;
}

/// A sub-mesh's corners on the screen as it is drawn: with a drawn pose (`pose`, one model matrix per bone, empty
/// without one), each skinned vertex is its position in its bone's space (`locals`) under that bone's matrix, so an
/// animated object is tested as its bones are posed this frame, as the original's animated draw collision poses them
/// before the test; without a pose, or for a vertex of a bone the pose has no matrix for, its rest position (`rest`)
[[nodiscard]] inline std::vector<screen_pick::ClipCorner>
PosedCorners(const screen_pick::View& view, std::span<const glm::vec3> rest, std::span<const uint16_t> bones,
             std::span<const glm::vec3> locals, std::span<const glm::mat4> pose)
{
	std::vector<screen_pick::ClipCorner> corners;
	corners.reserve(rest.size());
	for (size_t v = 0; v < rest.size(); ++v)
	{
		const bool posed = !pose.empty() && v < bones.size() && v < locals.size() && bones[v] < pose.size();
		const auto position = posed ? glm::vec3(pose[bones[v]] * glm::vec4(locals[v], 1.0f)) : rest[v];
		corners.push_back(screen_pick::ToClip(view, position, glm::vec2(0.0f)));
	}
	return corners;
}

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

/// A miracle dispenser's sphere, round the bubble it floats: its height times this above its place
constexpr float k_DispenserCollideHeight = 0.8f;
/// and its radius across the land times this
constexpr float k_DispenserCollideRadius = 1.5f;

/// Where the hand goes over an object an invisible sphere picked: one with a model of its own (not a spell icon, which
/// the game places by the land under it) is placed as any object with a model, by the hand's own test of that model
/// along the cursor's line, the land point kept when the line meets none of its triangles; one without, at the
/// sphere's point
[[nodiscard]] constexpr bool SpherePickFeelsOwnModel(bool hasModel, bool spellIcon)
{
	return hasModel && !spellIcon;
}

/// The invisible draw collision a miracle dispenser sends for itself before its building is drawn: at its place on the
/// land raised by eight tenths of its height, of radius half again its radius across the land
[[nodiscard]] inline Sphere DispenserSphere(const glm::vec3& place, float height, float radius2d)
{
	return {.centre = glm::vec3(place.x, height * k_DispenserCollideHeight + place.y, place.z),
	        .radius = radius2d * k_DispenserCollideRadius};
}

/// A vortex's sphere: its radius across the land times this
constexpr float k_VortexCollideRadius = 0.7f;

/// The invisible draw collision a vortex between the lands sends for itself: at its point, of seven tenths of its
/// radius across the land
[[nodiscard]] inline Sphere VortexSphere(const glm::vec3& point, float radius2d)
{
	return {.centre = point, .radius = k_VortexCollideRadius * radius2d};
}
} // namespace openblack::ecs::hand_pick
