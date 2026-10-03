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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// The villager's food (VillagerFood.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V4_spec.md §5, disassembly
// dev\tmp_dis\aldeanos\v4\food.txt, misc.txt): the hunger of the periodic check, the amounts, where it goes to eat, and
// the eating states 117 EAT_FOOD, 118 EAT_FOOD_AT_HOME, 212 SHOW_POISONED, 33 / 34 (the storage pit, P-1) and 35 (home
// with food, the housewife's: V14 uses it). The pure layer has the float arithmetic for the tests.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// CheckHungry's batch 0x75BCD6..0x75BD77: drop = (float)(u64) turns x 9e-5 (+0x2BC); / tribalPower (player +0x74,
/// when the villager has a player); x speed when speed > 1 (double compare) and it is moving; food = max(food - drop, 0)
[[nodiscard]] float HungerBatch(float food, uint32_t turns, float reducesFoodBy, std::optional<float> tribalPower,
                                float speed, bool moving);
/// GetAmountOfFoodToEat 0x75BC20: t = (float)(POWER(food) x dinner) (fimul, stored); with a town's Food desire
/// (Town +0x14C, no boosts) clamped to [0, 1]: ftol((1 - 0.3 c) x t); without one ftol(t)
[[nodiscard]] uint32_t FoodToEat(float food, uint32_t dinner, std::optional<float> townFoodDesire);
/// GetAmountOfFoodRequiredForMeal 0x75BC00: max(eat - held, 0), signed (setle)
[[nodiscard]] uint32_t FoodRequiredForMeal(uint32_t eat, int16_t held);
/// EatFoodHeld 0x75BF20's arithmetic: eaten = min((float) eat, (float) held); food = (eaten / eat) x nourish + food,
/// below 0 (or NaN: 0 / 0) -> 0, above 1 -> 1. Returns {eaten, food}
struct EatResult
{
	float eaten {0.0f};
	float food {0.0f};
};
[[nodiscard]] EatResult EatHeld(float food, int16_t held, uint32_t eat, float nourish);

// ---- the hunger --------------------------------------------------------------------------------------------------

/// Villager::CheckHungry 0x75BCC0 (the periodic check's last one, CheckEveryTime 0x7505F3, and 129's)
bool CheckHungry(entt::entity villager, uint32_t turn);
/// Villager::GetAmountOfFoodToEat 0x75BC20
[[nodiscard]] uint32_t GetAmountOfFoodToEat(entt::entity villager);
/// Villager::GetAmountOfFoodRequiredForMeal 0x75BC00
[[nodiscard]] uint32_t GetAmountOfFoodRequiredForMeal(entt::entity villager);
/// Villager::CheckSatisfyOwnFoodDesire 0x75BF00: IsHungry ? ChangeStateToFindFoodToEat : 0
uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager);
/// Villager::ChangeStateToFindFoodToEat 0x75B990 (0 / 1): need 0 -> eat (117, or 118 inside); its functional abode has
/// enough with what it carries -> 36 (118 inside); the storage pit (the town's, or its abode) functional with enough
/// -> 33; no functional one -> walk to GetResourceDropoffPos with FINAL 34 unless it is there; it carries some -> eat;
/// else 0
uint32_t ChangeStateToFindFoodToEat(entt::entity villager);
/// Villager::EatFoodHeld 0x75BF20: DropFood, the belly, Town::UseFood. Returns the food
float EatFoodHeld(entt::entity villager);
/// Villager::GetFoodFromHome 0x75C040 (n): m = min(n, abode food); GetResourceFrom(abode, FOOD, m) and PickupFood(what
/// it took) again: the abode loses m and the villager gains 2m (literal)
void GetFoodFromHome(entt::entity villager, uint32_t amount);

// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// 117 EAT_FOOD: Villager::EatFood 0x75C000: EatFoodHeld; PlayAnimThenSetState(poisoned ? 212 : 163). 1
uint32_t EatFood(components::LivingAction& action);
/// 118 EAT_FOOD_AT_HOME: Villager::EatFoodAtHome 0x75C090: eat - held > 0 -> GetFoodFromHome; EatFoodHeld;
/// SetTopState(poisoned ? 212 : 38). 1
uint32_t EatFoodAtHome(components::LivingAction& action);
/// 212 SHOW_POISONED: Villager::ShowPoisoned 0x75B940: inside -> SetupMoveToWithHug(FindPosOutsideAbode(0), 212); then
/// PlayAnimThenSetState(163). 1
uint32_t ShowPoisoned(components::LivingAction& action);
/// 33 GOTO_STORAGE_PIT_FOR_FOOD: Villager::GotoStoragePitForFood 0x769830. 1
uint32_t GotoStoragePitForFood(components::LivingAction& action);
/// 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD: Villager::ArrivesAtStoragePitForFood 0x7698B0 =
/// ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal, 163, 163)
uint32_t ArrivesAtStoragePitForFood(components::LivingAction& action);
/// 35 ARRIVES_AT_HOME_WITH_FOOD: Villager::ArrivesAtHomeWithFood 0x769B30: with an abode, abode.AddResource(FOOD,
/// DropFood(0)) (all it carries); then ArrivesHome
uint32_t ArrivesAtHomeWithFood(components::LivingAction& action);
} // namespace openblack::ecs::villager
