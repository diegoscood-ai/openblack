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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The player's creature: the one creature the game keeps for each player, which every part of the game that asks for
/// "the player's creature" is about. Kept on the player beside PlayerCreatures, the list of every creature they had;
/// the player service keeps it for a player without an entity (PlayerSystemInterface::SetPrimaryCreature).
struct PlayerCreatureSlot
{
	std::optional<entt::entity> creature;
};

} // namespace openblack::ecs::components
