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
#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"
#include "3D/ScreenPick.h"
#include "Creature/LeashPull.h"

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

/// The camera an invisible draw collision is tested on the screen with, as the game keeps it for the frame
struct CollisionScreen
{
	/// The world-to-clipping matrix as the game computes it (billboard::CameraFrame::clipMatrices)
	affine::AffineMatrix worldToClipping;
	/// The camera's up, in the world: the second row of the inverse of the world-to-camera matrix
	glm::vec3 cameraUp {0.0f, 1.0f, 0.0f};
	/// The near clip the screen's points are found with
	float nearClip {1.0f};
	glm::ivec2 size {640, 480};
	/// The mouse's pixel
	glm::ivec2 mouse {0};
};

/// An invisible draw collision as the game tests it on the screen: the centre projected to its pixel (none when it lies
/// in front of the near clip; off the screen still counts), the circle's radius the distance in pixels down the screen
/// to the point `radius` along the camera's up from the centre (no circle when that point is not projected), and the
/// mouse's pixel strictly inside the circle. The hit's distance is the centre's depth, the camera depth the projection
/// gives
[[nodiscard]] inline std::optional<float> InvisibleDrawCollision(const CollisionScreen& screen, const glm::vec3& centre,
                                                                 float radius)
{
	const auto middle = creature_leash::ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, centre);
	if (!middle.projected)
	{
		return std::nullopt;
	}
	const glm::vec3 top(centre.x + screen.cameraUp.x * radius, centre.y + screen.cameraUp.y * radius,
	                    centre.z + screen.cameraUp.z * radius);
	const auto above = creature_leash::ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, top);
	const float screenRadius = above.projected ? std::fabs(static_cast<float>(middle.pixel.y - above.pixel.y)) : 0.0f;
	const auto dx = static_cast<float>(screen.mouse.x - middle.pixel.x);
	const auto dy = static_cast<float>(screen.mouse.y - middle.pixel.y);
	if (!(dy * dy + dx * dx < screenRadius * screenRadius))
	{
		return std::nullopt;
	}
	return middle.depth;
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
/// What an object is, as far as whether its model is drawn with a collision the cursor can find
struct DrawnModel
{
	bool lampModel {false};     ///< a lamp drawn as a model with a light
	bool streetLantern {false}; ///< a street lantern, a map object of its own
	bool shield {false};        ///< a physical or a spiritual shield
};

/// Why the cursor never finds an object's model, whatever the ray does
enum class NoCollision
{
	None,   ///< the model is drawn with its collision
	Lamp,   ///< a temple's or a Norse gate's lamp: only a model with a light, drawn with no collision (a street
	        ///< lantern is a map object and is found)
	Shield, ///< a shield's dome: the physical one's model is added for drawing with no object behind it, and the
	        ///< spiritual one is only particles, so the hand reaches what stands inside it
};

[[nodiscard]] constexpr NoCollision NoCollisionOf(const DrawnModel& model)
{
	if (model.lampModel && !model.streetLantern)
	{
		return NoCollision::Lamp;
	}
	if (model.shield)
	{
		return NoCollision::Shield;
	}
	return NoCollision::None;
}

} // namespace openblack::ecs::hand_pick
