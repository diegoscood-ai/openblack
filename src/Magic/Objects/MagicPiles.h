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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The piles the food and wood miracles (and the hand) leave on the land: MagicFood (MagicFood.cpp) and MagicWood
// (MagicWood.cpp). Wiki: docs/bw1-notes/magic.md, "Comida y madera".

namespace openblack::magic::objects
{
/// fn_005FA8B0 (pos, player, type, amount, town): FOOD -> MagicFood, WOOD -> MagicWood, then
/// CallVirtualFunctionsForCreation; any other type none. position: x, z on the map, y ignored (the pile stands on the
/// land). player nullopt = NULL.
entt::entity CreateMagicResourcePile(const glm::vec3& position, std::optional<PlayerNames> player, ResourceType type,
                                     uint32_t amount);

/// MagicFood::MagicFood 0x5FA9F0: a PileFood of GPotInfo 10 "Magic Food" (MSH_S_GRAIN_PILE), owner +0xBC (NULL -> the
/// local player), SetScale(0.3) (fn_005FAAE0)
entt::entity CreateMagicFood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount);

/// MagicWood::MagicWood 0x600E20: a PileWood of GPotInfo 9 "Magic Wood" (MSH_B_WOOD_01), owner +0xB4, SetScale(0.7)
/// (fn_00600EE0)
entt::entity CreateMagicWood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount);
} // namespace openblack::magic::objects
