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
namespace components
{
struct FishShoal;
}

/// fn_00824740 for each of the 15 fish: size 0.8..1.2, within +-5 (x, z) and -1..0 (y) of the centre, a random heading,
/// speed 0.5..1.5 and turn rate speed x [0x900C90] (bit_cast 0x3F20D97C, about pi / 5) x (1 +- 0.1); the target is
/// the centre
void InitFishShoal(components::FishShoal& shoal, const glm::vec3& centre);

/// fn_0074F2D0 & co.: something hit the water here this frame (the hand gripping the sea, an object falling in).
/// Every shoal within 300 units of the camera reacts in the next UpdateFishShoals.
void SplashWater(const glm::vec3& point);

/// FishFarm::Process 0x52D130, once per game turn: every 16th turn each farm gets 1 food back, up to 1400
void ProcessFishFarmsTurn(uint32_t turn);

/// fn_00824B10: the fish farm with a shown fish within 2 units (x, z) of the point
[[nodiscard]] std::optional<entt::entity> FindFishFarmAt(const glm::vec3& point);

/// GFishFarmInfo::IsOkToCreateAtPos (0x52D100): MapCoords::IsCoastal (a land cell on the coast line, 0x6036A0) and no
/// fish farm already in that map cell (MapCoords::FindType(OBJECT_TYPE 0x21, nullptr) 0x6045C0 == 0). Nothing else
/// (town, depth, distance). Only scripts create fish farms today; this is the rule for placing one by hand.
[[nodiscard]] bool IsOkToCreateFishFarmAt(const glm::vec3& point);

/// FishFarm::RemoveFood 0x52CED0: all of `amount` if the farm has it, else what is left; the stock drops accordingly
uint32_t RemoveFishFarmFood(entt::entity farm, uint32_t amount);

/// Moves the fish of every fish farm shoal (fn_00824DA0 per shoal, fn_008248E0 per fish) by `seconds` of game time
/// and works out each shoal's visibility and alpha from the camera distance. For the shoals with a bait (the fish
/// puzzle) it also runs fn_00824B90's rule: the fish inside, the net, and when it is done a ring per fish.
void UpdateFishShoals(float seconds, const glm::vec3& camera);

} // namespace openblack::ecs
