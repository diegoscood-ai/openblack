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

#include <VillagerRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"

// The values a villager's state writes after the villager's own values, made from what openblack holds of it
// (docs/wiki/engine/saved-games.md#openblack, the villagers)
namespace openblack::ecs::saved_world_write
{

/// The values the villager's state `state` writes, in the original's order, from what openblack holds of the villager;
/// `living` is the living part already made for its record (the footpath states write its footpath and node again).
/// Nothing for a state whose values openblack does not hold, or one that writes none
[[nodiscard]] std::optional<std::vector<gamesave::StateValue>>
HeldVillagerStateValues(WriteContext& context, entt::entity entity, uint8_t state, const gamesave::LivingFields& living);

} // namespace openblack::ecs::saved_world_write
