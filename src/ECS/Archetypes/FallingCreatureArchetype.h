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

namespace openblack::ecs::archetypes
{
class FallingCreatureArchetype
{
public:
	/// The falling spell film's copy of a creature (components::FallingCreature), made as the film starts: a fresh body
	/// of the creature's species, with its shape, eyes and unstarted hair, at the creature's size held within its limits and
	/// its alignment held to -1 .. 1, at the world's origin facing heading 0, at the creature's scale. It stands and plays
	/// nothing yet (CreatureAnimationSystemInterface::StartFallingCopy starts it) and draws no random numbers. None
	/// when the creature has no body
	static std::optional<entt::entity> Create(entt::entity source);
	FallingCreatureArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
