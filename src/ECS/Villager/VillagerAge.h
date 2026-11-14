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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/VillagerAge.h"

namespace openblack
{
struct GVillagerInfo;
}

// A villager's age kept as its birth turn (docs/wiki/beings/villager-life.md#home-food-sleep-homeless-and-age), and what the
// age does: the child that grows up, the scale for the age, the old age's death and the pregnancy's count down
// (UpdatePregnancy). The rules are the pure villager_age (ECS/VillagerAge.h); this is the villager's side of them. The births
// are in VillagerBirth.h.
namespace openblack::ecs::villager
{

// ---- the villager info's tables ----------------------------------------------------------------------------------

/// villager_age::StartScale with the info's table, and its field before the table (dancingSpeed) read as a float for
/// age 0
[[nodiscard]] float InitialScaleForAge(const GVillagerInfo& info, uint32_t age);
/// villager_age::GrownScale with the info's table from the scale `current`, drawing the game's synced GameFloatRand
[[nodiscard]] float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current);
/// What villager_age::MakeNewborn needs from the info: grownUpAge, hungryForFood, processChecksEvery, the table and its
/// field before it (dancingSpeed) read as a float
[[nodiscard]] villager_age::NewbornKind NewbornKindOf(const GVillagerInfo& info);

// ---- the villager ------------------------------------------------------------------------------------------------

/// The periodic check, children only: grown up -> no longer a child, SetAge(max(13, 18)) (no mesh change, the age
/// is already >= 13), the abode's ChildToAdult (or the town's), and ChildBecomesAdult (its result); else on a
/// rescale turn SetScaleForAge(GetAge()). The result
uint32_t CheckChildGrownUp(entt::entity villager);
/// mother = none; CheckNeedNewAbode; SetTopState(234 GO_HOME_AND_CHANGE); 1
uint32_t ChildBecomesAdult(entt::entity villager);
/// State 115 CHILD_BECOMES_ADULT: ChildBecomesAdult (only a script sets 115 (inferred): no other setter was searched)
uint32_t ChildBecomesAdultState(components::LivingAction& action);
/// SetAge after the creation: VillagerCore's SetAge with the original's meshes (only when the age crosses
/// grownUpAge) and InitialiseScale + SetScaleForAge
uint32_t SetAgeAndScale(entt::entity villager, uint32_t age);
/// InitialScaleForAge / ScaleForAge on the villager's Transform scale (the uniform scale)
void InitialiseScale(entt::entity villager, uint32_t age);
void SetScaleForAge(entt::entity villager, uint32_t age);
/// villager_age::DiesOfOldAge with the info's ages and the game's draws; dies -> VillagerDead(9 OLD_AGE, no player,
/// info.life, 1); 1. Else 0
bool CheckDeathFromOldAge(entt::entity villager);
/// A woman (info sex == 1) with a pregnancy count != 0
[[nodiscard]] bool IsPregnant(entt::entity villager);
/// Pregnant and not controlled by a script: the pregnancy count -= GetGameTurnsSinceLastChecked (16 bits); <= 0 ->
/// HousewifeStartsGivingBirth (its result). Else 0
uint32_t UpdatePregnancy(entt::entity villager);
/// HousewifeStartsGivingBirth, UpdatePregnancy's tail, is in VillagerBirth.h
} // namespace openblack::ecs::villager
