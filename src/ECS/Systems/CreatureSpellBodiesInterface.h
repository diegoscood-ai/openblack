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

#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{
/// A creature's body as the particles of a creature spell read it
struct CreatureSpellBody
{
	/// Where the body stands
	glm::vec3 origin {0.0f};
	/// The size it is drawn at
	float size {1.0f};
	/// Each bone's place in the world, as posed for the game turn; none before it is first posed
	std::vector<glm::vec3> bones;
	/// The bone of its right eye and that bone's mirror, the left eye; none when its species names no eye or no mirror
	std::optional<uint32_t> rightEye;
	std::optional<uint32_t> leftEye;
	/// The bone of its right foot and that bone's mirror, the left foot; none when its species names no foot or no
	/// mirror
	std::optional<uint32_t> rightFoot;
	std::optional<uint32_t> leftFoot;
	/// How far the invisible spell is over it, 0 to 1
	float invisible {0.0f};
};

/// What the particles of a creature spell read of the creature the spell was cast on
class CreatureSpellBodiesInterface
{
public:
	virtual ~CreatureSpellBodiesInterface() = default;

	/// Whether it is still in the game: false once it is gone or being deleted
	[[nodiscard]] virtual bool IsAvailable(entt::entity creature) const = 0;
	/// Its body; none for anything that is not a creature
	[[nodiscard]] virtual std::optional<CreatureSpellBody> CreatureBody(entt::entity creature) const = 0;
};
} // namespace openblack::ecs::systems
