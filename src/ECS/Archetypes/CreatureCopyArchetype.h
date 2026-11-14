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

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::archetypes
{
class CreatureCopyArchetype
{
public:
	/// The places of the Creature Cave the copy is put by: it stands between the first and the third, facing the fire
	struct CavePlaces
	{
		glm::vec3 first;
		glm::vec3 third;
		glm::vec3 fire;
	};

	/// The Creature Cave's copy of a creature, made as the room starts: its body's shape, pose and eyes as the
	/// creature's, at the creature's size held within its limits, where the coin tossed now puts it, facing the fire.
	/// It tosses the coin and then the rest's flag on the game's local stream. None, and no toss, when the creature has
	/// no body
	static std::optional<entt::entity> Create(entt::entity source, const CavePlaces& places);
	CreatureCopyArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
