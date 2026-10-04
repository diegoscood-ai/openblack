/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFood.h"

#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// VillagerFood.cpp of runblack.exe W120 (VillagerFood.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
using state_info::StateInfo;

namespace
{
/// 0.3 (0x8AB23C): GetAmountOfFoodToEat's share of the town's Food desire
constexpr float k_TownFoodShare = 0.3f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

entt::entity TownEntityOf(const Villager& v)
{
	return v.town != entt::null && Entities().Valid(v.town) && Entities().AllOf<Town>(v.town) ? v.town : entt::null;
}

entt::entity AbodeEntityOf(const Villager& v)
{
	return v.abode != entt::null && Entities().Valid(v.abode) ? v.abode : entt::null;
}

bool Inside(const Villager& v)
{
	return (v.flags & Villager::k_FlagAtHome) != 0;
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float HungerBatch(float food, uint32_t turns, float reducesFoodBy, std::optional<float> tribalPower, float speed, bool moving)
{
	// 0x75BCE7..0x75BCF7: (fild qword: unsigned) turns x info +0x2BC, stored
	float drop = static_cast<float>(static_cast<double>(turns)) * reducesFoodBy;
	// 0x75BD0B..0x75BD20: with a player, drop / player +0x74 (fdivr)
	if (tribalPower.has_value())
	{
		drop = drop / *tribalPower;
	}
	// 0x75BD24..0x75BD4B: speed > 1.0 (double; test ah, 0x41) and IsMoving -> drop = speed x drop
	if (static_cast<double>(speed) > 1.0 && moving)
	{
		drop = speed * drop;
	}
	// 0x75BD4F..0x75BD77: food - drop, below 0 -> 0
	const float left = food - drop;
	return left < 0.0f ? 0.0f : left;
}

uint32_t FoodToEat(float food, uint32_t dinner, std::optional<float> townFoodDesire)
{
	// 0x75BC2A..0x75BC49: POWER(food) x (fimul, u32 read as a qword's low half) foodReqiredForDinner, stored
	const float t = Power(food) * static_cast<float>(static_cast<double>(dinner));
	if (!townFoodDesire.has_value())
	{
		return static_cast<uint32_t>(map_coords::FtoL(t)); // 0x75BCA8
	}
	// 0x75BC5B..0x75BC87: below 0 -> 0, above 1 (test ah, 0x41) -> 1
	float c = *townFoodDesire;
	if (c < 0.0f)
	{
		c = 0.0f;
	}
	else if (c > 1.0f)
	{
		c = 1.0f;
	}
	// 0x75BC8D..0x75BC9D: ftol((1 - c x 0.3) x t)
	const float share = c * k_TownFoodShare;
	const float keep = 1.0f - share;
	return static_cast<uint32_t>(map_coords::FtoL(keep * t));
}

uint32_t FoodRequiredForMeal(uint32_t eat, int16_t held)
{
	// 0x75BC08..0x75BC1A: eat - (movsx) held; <= 0 -> 0
	const int32_t need = static_cast<int32_t>(eat) - static_cast<int32_t>(held);
	return need <= 0 ? 0u : static_cast<uint32_t>(need);
}

EatResult EatHeld(float food, int16_t held, uint32_t eat, float nourish)
{
	// 0x75BF26..0x75BF67: eat (fild qword: unsigned) and held (fild: movsx) as floats; the smaller (fcomp; test ah, 1)
	const float eatF = static_cast<float>(static_cast<double>(eat));
	const float heldF = static_cast<float>(held);
	const float eaten = eatF < heldF ? eatF : heldF;
	// 0x75BF7C..0x75BFC1: eaten / eat x info +0x2B8 + food; below 0 or unordered (test ah, 1) -> 0; above 1 -> 1
	const float share = eaten / eatF;
	const float gain = share * nourish;
	float result = gain + food;
	if (!(result >= 0.0f))
	{
		result = 0.0f;
	}
	else if (result > 1.0f)
	{
		result = 1.0f;
	}
	return {eaten, result};
}

// ---- the hunger --------------------------------------------------------------------------------------------------

bool CheckHungry(entt::entity villager, uint32_t turn)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return false;
	}
	// 0x75BCC9..0x75BCD0: no turn since the last check -> 0, without SetGameTurnLastChecked
	const uint32_t turns = GetGameTurnsSinceLastChecked(villager, turn);
	if (turns == 0)
	{
		return false;
	}
	const auto& info = InfoOf(villager);
	// 0x75BCDF..0x75BD07: speed = (float)(u16 +0x5A) / (int) info +0x104 (speedDefault, fidiv). (aproximado) the u16 is
	// WallHug::speed back to MapCoords (map_coords::ToFixed); (openblack, guard) a speed group of 0 gives 0
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	const auto raw = wallHug != nullptr ? static_cast<uint16_t>(map_coords::ToFixed(wallHug->speed)) : uint16_t {0};
	const auto group = static_cast<int32_t>(static_cast<uint32_t>(info.speedGroup.speedDefault));
	const float speed = group != 0 ? static_cast<float>(raw) / static_cast<float>(group) : 0.0f;
	// 0x75BD0B..0x75BD19: GetPlayer (Villager::GetPlayer 0x7502F0: the town's +0x2C) -> player +0x74 = TribalPower[3]
	// (PlayerMagic::tribalPower, 1.0 unless written)
	std::optional<float> tribal;
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		tribal = magic::players::MagicOf(Entities().Get<const Town>(town).owner).tribalPower.at(3);
	}
	// 0x75BD39 IsMoving (Object 0x402710: +0x14 != +0x2C, it moved since the last turn). (aproximado) the WallHug's
	// last step was not zero
	const bool moving = wallHug != nullptr && wallHug->step != glm::vec2(0.0f);
	const float before = v->food;
	v->food = HungerBatch(v->food, turns, info.gameTurnReducesFoodInBellyBy, tribal, speed, moving);
	// 0x75BD7D..0x75BD89: hungry = food < hungryForFood (test ah, 1: strict; IsHungry is <=)
	const bool hungry = v->food < info.hungryForFood;
	uint32_t result = 0;
	// 0x75BD92..0x75BDE6: hungry or poisoned (vt +0x4A4) -> ReduceLife(max(1 - food / hungry, 1) x +0x2D0, no player)
	if (hungry || life::IsPoisoned(villager))
	{
		life::ReduceLife(villager, life::HungerLifeLoss(v->food, info.hungryForFood, info.hungerToLifeMultiplier));
	}
	if (hungry)
	{
		// 0x75BDF0..0x75BE0F: GetFinalState's row, read once for both flags
		const auto final = GetFinalState(villager);
		const auto& row = StateInfo(final);
		auto* again = VillagerOf(villager);
		const bool ignores = again != nullptr && (again->flags & Villager::k_FlagDisciple) != 0 &&
		                     DiscipleIgnoresNeeds(again->discipleType);
		// 0x75BE0F..0x75BE4B: InterruptWhenHungry (memory +0xE0 = file 0xD0) and not a disciple that ignores needs
		if (state_info::InterruptWhenHungry(row) && !ignores)
		{
			result = ChangeStateToFindFoodToEat(villager);
		}
		// 0x75BE4D..0x75BE78: food < starvingForFood (+0x2C4) and InterruptWhenStarving (file 0xD4) and still 0 (no
		// disciple test)
		again = VillagerOf(villager);
		if (again != nullptr && again->food < info.starvingForFood && state_info::InterruptWhenStarving(row) && result == 0)
		{
			result = ChangeStateToFindFoodToEat(villager);
		}
		// 0x75BE7A..0x75BEDA: life <= 0 (test ah, 0x41) -> VillagerDead(final (read again) 248 / 249 / 250 or flags & 2
		// ? 4 CHANT : 1 STARVING, GetPlayer, GetLife, 1); 1
		if (life::LifeOf(villager) <= 0.0f)
		{
			const auto now = GetFinalState(villager);
			const auto* last = VillagerOf(villager);
			const bool chant = now == VillagerStates::GoHomeFromWorship || now == VillagerStates::ArrivesHomeFromWorship ||
			                   now == VillagerStates::SleepInTentFromWorship ||
			                   (last != nullptr && (last->flags & Villager::k_FlagAtWorshipSite) != 0);
			// GetPlayer (vt +0x1C, Villager::GetPlayer 0x7502F0: the town's owner, none without a town)
			VillagerDead(villager, chant ? DeathReason::Chant : DeathReason::Starving, GetPlayerOf(villager),
			             life::LifeOf(villager), 1);
			result = 1;
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("food: drop {:.6f} food {:.6f} life {:.6f}{} -> {}", before - v->food, v->food,
		                            life::LifeOf(villager), hungry ? " hungry" : "", result));
	}
	// 0x75BEDF SetGameTurnLastChecked
	SetGameTurnLastChecked(villager, turn);
	return result != 0;
}

uint32_t GetAmountOfFoodToEat(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x75BC4D..0x75BC5B: with a town, its Town +0x14C (TownDesire +0x118[0], the desire without the boosts)
	std::optional<float> desire;
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		desire = town_desire::GetField(town, TownDesireInfo::ForFood, town_desire::Field::Desire);
	}
	return FoodToEat(v->food, InfoOf(villager).foodReqiredForDinner, desire);
}

uint32_t GetAmountOfFoodRequiredForMeal(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? FoodRequiredForMeal(GetAmountOfFoodToEat(villager), v->resourceHeld.at(0)) : 0;
}

uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager)
{
	// 0x75BF03: IsHungry (food <= hungryForFood) -> ChangeStateToFindFoodToEat
	return IsHungry(villager) ? ChangeStateToFindFoodToEat(villager) : 0;
}

uint32_t ChangeStateToFindFoodToEat(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x75B998: need = GetAmountOfFoodRequiredForMeal; 0x75B99F..0x75B9B2: eat = inside ? 118 : 117
	const uint32_t need = GetAmountOfFoodRequiredForMeal(villager);
	const auto eat = Inside(*v) ? VillagerStates::EatFoodAtHome : VillagerStates::EatFood;
	const int16_t held = v->resourceHeld.at(0);
	// 0x75B9B4: need 0 -> SetTopState(eat); 1
	if (need == 0)
	{
		TraceIf(villager, fmt::format("food: need 0 held {} -> {}", held, static_cast<uint32_t>(eat)));
		SetTopState(villager, eat);
		return 1;
	}
	// 0x75B9BA..0x75B9FD: a functional abode whose GetResource(FOOD) + (movsx) held >= need (unsigned, jb) -> 36, or
	// 118 inside (0x24 + 0x52 x inside)
	if (const auto abode = AbodeEntityOf(*v); abode != entt::null && abode_queries::IsFunctional(abode))
	{
		const uint32_t have = object_resources::GetResource(abode, ResourceType::Food) +
		                      static_cast<uint32_t>(static_cast<int32_t>(held));
		if (!(have < need))
		{
			const auto next = Inside(*v) ? VillagerStates::EatFoodAtHome : VillagerStates::GoHome;
			TraceIf(villager, fmt::format("food: need {} home {} held {} -> {}", need, have - static_cast<uint32_t>(held),
			                              held, static_cast<uint32_t>(next)));
			SetTopState(villager, next);
			return 1;
		}
	}
	// 0x75BA26..0x75BA74: GetStoragePit (the town's, else its abode) functional: enough -> 33; else on to what it carries
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		const uint32_t inPit = object_resources::GetResource(pit, ResourceType::Food);
		if (!(inPit < need))
		{
			TraceIf(villager, fmt::format("food: need {} pit {} has {} -> 33", need, static_cast<uint32_t>(pit), inPit));
			SetTopState(villager, VillagerStates::GotoStoragePitForFood);
			return 1;
		}
	}
	else
	{
		// 0x75BA75..0x75BAAF: pos = GetResourceDropoffPos(FOOD); not IsCloseToEqual(pos, me, 0) -> walk, FINAL 34; 1
		const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
		if (!(tq::GetDistanceInMetres(pos, tq::PosOf(villager)) <= 0.0f))
		{
			TraceIf(villager, fmt::format("food: need {} no pit -> 34 at the drop-off point", need));
			SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForFood);
			return 1;
		}
	}
	// 0x75BAB0..0x75BAD3: it carries some (+0xF4 != 0) -> SetTopState(eat); 1; else 0
	if (held != 0)
	{
		TraceIf(villager, fmt::format("food: need {} -> eat-held {} ({})", need, held, static_cast<uint32_t>(eat)));
		SetTopState(villager, eat);
		return 1;
	}
	TraceIf(villager, fmt::format("food: need {} -> none", need));
	return 0;
}

float EatFoodHeld(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0.0f;
	}
	const auto& info = InfoOf(villager);
	const uint32_t eat = GetAmountOfFoodToEat(villager);
	const auto before = v->food;
	const auto r = EatHeld(v->food, v->resourceHeld.at(0), eat, info.foodNurishmentMultiplier);
	// 0x75BF6B..0x75BF77: DropFood(ftol(eaten)) (DropFood(0) drops all of it: 0x7511EB)
	const auto eatenInt = static_cast<uint16_t>(map_coords::FtoL(r.eaten));
	DropFood(villager, eatenInt);
	// 0x75BF7C..0x75BFC1: the belly
	if (auto* again = VillagerOf(villager))
	{
		again->food = r.food;
	}
	// 0x75BFCB..0x75BFE9: with a town, Town::UseFood(ftol(eaten))
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		town_villagers::UseFood(town, static_cast<uint32_t>(map_coords::FtoL(r.eaten)));
	}
	TraceIf(villager, fmt::format("eat: held {} eat {} food {:.6f} -> {:.6f}", eatenInt, eat, before, r.food));
	return r.food;
}

void GetFoodFromHome(entt::entity villager, uint32_t amount)
{
	const auto* v = VillagerOf(villager);
	const auto abode = v != nullptr ? AbodeEntityOf(*v) : entt::null;
	if (abode == entt::null)
	{
		return;
	}
	// 0x75C04F..0x75C06F: m = min(n, GetResource(FOOD)) (cmp; jb keeps n)
	const uint32_t have = object_resources::GetResource(abode, ResourceType::Food);
	const uint32_t m = amount < have ? amount : have;
	// 0x75C071..0x75C077: GetResourceFrom(abode, FOOD, m): RemoveResource + PickupResource
	const auto took = GetResourceFrom(villager, abode, ResourceType::Food, static_cast<int16_t>(m));
	// 0x75C07C..0x75C07F: PickupFood(what it took) again (literal: the villager gains twice what the abode lost)
	PickupFood(villager, static_cast<int16_t>(took));
	if (TraceOn(villager))
	{
		const auto* after = VillagerOf(villager);
		Trace(villager, fmt::format("home-food: took {} held {}", took, after != nullptr ? after->resourceHeld.at(0) : 0));
	}
}

// ---- the state functions -----------------------------------------------------------------------------------------

uint32_t EatFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x75C003 EatFoodHeld; 0x75C00E..0x75C025: PlayAnimThenSetState(poisoned ? 0xD4 212 : 0xA3 163, 1)
	EatFoodHeld(villager);
	PlayAnimThenSetState(villager, life::IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t EatFoodAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x75C094..0x75C0AC: the held food read first; n = GetAmountOfFoodToEat - held; > 0 -> GetFoodFromHome(n)
	const int16_t held = v->resourceHeld.at(0);
	const int32_t n = static_cast<int32_t>(GetAmountOfFoodToEat(villager)) - static_cast<int32_t>(held);
	if (n > 0)
	{
		GetFoodFromHome(villager, static_cast<uint32_t>(n));
	}
	// 0x75C0B3 EatFoodHeld; 0x75C0BA..0x75C0D5: SetTopState(poisoned ? 0xD4 212 : 0x26 38)
	EatFoodHeld(villager);
	SetTopState(villager, life::IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::AtHome);
	return 1;
}

uint32_t ShowPoisoned(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// 0x75B946..0x75B967: inside -> SetupMoveToWithHug(FindPosOutsideAbode(0), 212)
	if (v != nullptr && Inside(*v))
	{
		const auto pos = FindPosOutsideAbode(villager, entt::null);
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ShowPoisoned);
	}
	// 0x75B96C..0x75B975: PlayAnimThenSetState(163, 1)
	PlayAnimThenSetState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t GotoStoragePitForFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x769836..0x769880: GetStoragePit functional -> SetupMoveToOnFootpath(pit, pit.GetArrivePos, 34)
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		TraceIf(villager, fmt::format("food 33: to the pit {}", static_cast<uint32_t>(pit)));
		SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), VillagerStates::ArrivesAtStoragePitForFood);
		return 1;
	}
	// 0x76988A..0x7698A1: else SetupMoveToWithHug(GetResourceDropoffPos(FOOD), 34)
	const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
	SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForFood);
	return 1;
}

uint32_t ArrivesAtStoragePitForFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x7698B1..0x7698C7: ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal(), 163, 163)
	return ArrivesAtStoragePitForResource(villager, ResourceType::Food, GetAmountOfFoodRequiredForMeal(villager),
	                                      VillagerStates::DecideWhatToDo, VillagerStates::DecideWhatToDo);
}

uint32_t ArrivesAtHomeWithFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// 0x769B33..0x769B62: with an abode, abode.AddResource(FOOD, (movsx) DropFood(0), 0, 0, 0, 0)
	if (v != nullptr)
	{
		if (const auto abode = AbodeEntityOf(*v); abode != entt::null)
		{
			const auto dropped = DropFood(villager, 0);
			object_resources::AddResource(abode, ResourceType::Food,
			                              static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(dropped))));
		}
	}
	// 0x769B6C ArrivesHome 0x760930 (its result)
	return ArrivesHome(villager);
}
} // namespace openblack::ecs::villager
