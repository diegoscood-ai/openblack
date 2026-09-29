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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs
{

/// fn_0074F2D0 & co.: something hit the water here this frame (the hand gripping the sea, an object falling in).
/// Every shoal within 300 units of the camera reacts in the next UpdateFishShoals.
void SplashWater(const glm::vec3& point);

/// FishFarm::Process 0x52D130, once per game turn: every 16th turn each farm gets 1 food back, up to 1400
void ProcessFishFarmsTurn(uint32_t turn);

/// fn_00824B10: the fish farm with a shown fish within 2 units (x, z) of the point
[[nodiscard]] std::optional<entt::entity> FindFishFarmAt(const glm::vec3& point);

/// FishFarm::RemoveFood 0x52CED0: all of `amount` if the farm has it, else what is left; the stock drops accordingly
uint32_t RemoveFishFarmFood(entt::entity farm, uint32_t amount);

/// Moves the fish of every fish farm shoal (fn_00824DA0 per shoal, fn_008248E0 per fish) by `seconds` of game time
/// and works out each shoal's visibility and alpha from the camera distance
void UpdateFishShoals(float seconds, const glm::vec3& camera);

} // namespace openblack::ecs
