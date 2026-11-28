/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Physics/CollideBoxes.h"

// A creature among the physics objects. It never flies itself, but what is thrown hits it: it stands as a heavy body
// that does not move. What a creature lets go of flies as anything thrown does. Wiki:
// docs/wiki/physics/physics.md#who-owns-what-class-hooks-and-the-hands-api and
// docs/wiki/creature/hits-and-catch.md#struck-by-a-thrown-thing.

namespace openblack::physics
{
class Body;
} // namespace openblack::physics

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

namespace openblack::ecs::creature_physics
{
/// How heavy a creature's body is in the physics, whatever the creature weighs
inline constexpr float k_Mass = 1000.0f;

/// How much a creature weighs: its mass by the size its body is drawn at and the shape it is to become, thin or fat
/// and weak or strong, rather than the shape drawn yet. An entity that is not a creature throws std::invalid_argument
[[nodiscard]] float Weight(const Registry& registry, entt::entity creature);

/// The body a creature stands as: a ball as tall as the creature, as six points about its middle and the eight faces
/// between them, for what is thrown to hit
struct BodyShape
{
	/// The points about the centre of mass
	std::vector<glm::vec3> points;
	std::vector<std::array<uint32_t, 3>> faces;
	/// The centre of mass above the creature's feet
	glm::vec3 centre {0.0f};
	float radius {0.0f};
	float mass {k_Mass};
	/// Whether it moves when hit: a creature's does not
	bool dynamic {false};
};
/// The body of a creature this tall
[[nodiscard]] BodyShape ShapeOf(float height);
/// The same ball of this radius about this centre of mass (above the creature's feet, in its own turn)
[[nodiscard]] BodyShape ShapeOf(float radius, glm::vec3 centre);

/// The sphere about a creature's posed body: the mean of its bones' places and the distance to the farthest of them
struct BoundingSphere
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};
/// The sphere about these bone places (in the world), as the original works it out: the places summed a coordinate at
/// a time, times one over their count; then each one's distance, the squares of z, y and x in that order, kept when it
/// is farther than the farthest so far (from 0). None for no bones
[[nodiscard]] std::optional<BoundingSphere> BoundingSphereOf(std::span<const glm::vec3> bones);

/// The boxes of the creature's body as it is posed: its mesh's box about each bone (L3DMesh::GetBoneBounds), each
/// placed by its bone as the body is placed in the world. Empty for a body with no mesh or not posed yet
[[nodiscard]] std::vector<openblack::physics::collide_boxes::Box> CollideBoxesOf(const Registry& registry,
                                                                                 entt::entity creature);

/// The sphere about the creature's body as it is posed, its bones placed in the world as the body is drawn; none for a
/// body not posed yet
[[nodiscard]] std::optional<BoundingSphere> PosedSphereOf(const Registry& registry, entt::entity creature);

/// The creature's body follows its pose: its centre and radius the posed sphere's, its boxes placed by the posed bones
/// (as the original's pose update does for a creature in the physics). Nothing changes for a body not posed
void FollowPose(entt::entity creature, openblack::physics::Body& body);

/// The creature's body where it stands; false for anything but a creature
bool SetUpBody(entt::entity creature, openblack::physics::Body& body);

/// How what a creature lets go of goes into the physics
enum class Release : uint8_t
{
	/// Thrown at a target: it flies with the creature as its thrower
	AtTarget,
	/// Put down, dropped or tossed aside, as the hand lets go of things, at a creature's lower speed for a throw
	LetGo,
};
/// A throw at a target needs a creature to throw it; without one it is let go of
[[nodiscard]] Release ReleaseOf(bool atTarget, entt::entity creature);
/// The spin what a creature lets go of leaves its hand with: one about the vertical (in this port's sign, the opposite
/// of the original's), its put-down and its toss aside alike (docs/wiki/physics/physics.md#a-creatures-let-go); none
/// when the debug switch turns it off
[[nodiscard]] std::optional<glm::vec3> LetGoSpin(bool spins) noexcept;
} // namespace openblack::ecs::creature_physics
