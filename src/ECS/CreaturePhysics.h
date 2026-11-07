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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

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
} // namespace openblack::ecs::creature_physics
