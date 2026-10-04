/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSpeed.h"

#include <algorithm>
#include <array>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "Game.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// VillagerDisciple 9 TRADER (SetStateSpeed 0x7539FE `cmp cl, 9`)
constexpr uint8_t k_DiscipleTrader = 9;

/// g_game +0x205A40, the game turn
uint32_t CurrentGameTurn()
{
	return game_clock::Turn();
}

uint32_t Raw(SpeedState state)
{
	return static_cast<uint32_t>(state);
}

uint32_t SpeedGroupEntry(const SpeedGroup& group, uint32_t index)
{
	const std::array<SpeedState, 6> entries = {group.speedDefault, group.speedFleeing, group.speed2,
	                                           group.speed3,       group.speed4,       group.speed5};
	return Raw(entries.at(std::min<uint32_t>(index, 5)));
}
} // namespace

const GVillagerInfo* VillagerInfoOf(entt::entity entity)
{
	const auto* villager = Locator::entitiesRegistry::value().TryGet<const Villager>(entity);
	if (villager == nullptr)
	{
		return nullptr;
	}
	for (const auto& info : Locator::infoConstants::value().villager)
	{
		if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
		{
			return &info;
		}
	}
	return nullptr;
}

void SetVillagerStateSpeed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager = registry.TryGet<const Villager>(entity);
	const auto* action = registry.TryGet<const LivingAction>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (villager == nullptr || action == nullptr || wallHug == nullptr || info == nullptr)
	{
		return;
	}
	const auto& states = Locator::infoConstants::value().villagerStateTable;
	// GetFinalState: the current state if it is a final one, else the destination
	const auto top = action->states[static_cast<size_t>(LivingAction::Index::Top)];
	// 0x753766: a villager controlled by a script keeps its speed (GameThingWithPos +0x25 & 4)
	if (script_held::IsControlledByScript(entity))
	{
		return;
	}
	// 0x753772: so does a dancing one (Living::IsDancing 0x5ECC10: the DanceGroup at Living +0xD8 is not null).
	// (aproximado: openblack has no Living +0xD8 yet; WorshipVillager::dancing, set where the original's
	// GroupBehaviour::FindDanceGroup puts it in a dance group and cleared at RemoveFromDance, stands for it, and TOP ==
	// IN_DANCE for the other dances, as openblack had it)
	const auto* worship = registry.TryGet<const WorshipVillager>(entity);
	if (top == static_cast<uint8_t>(VillagerStates::InDance) || (worship != nullptr && worship->dancing))
	{
		return;
	}
	const auto destination = action->states[static_cast<size_t>(LivingAction::Index::Final)];
	auto final = top < states.size() && states[top].isFinalState != 0 ? top : destination;
	if (final >= states.size())
	{
		final = top;
	}
	if (final >= states.size())
	{
		return;
	}
	// m: the land balance speed scale (GLandBalance::Values[4]: 1.5 in Land2, 1.25 in Land3) * the player's wonder bonus
	// (1) * the town's belief term (openblack has no belief in the player yet: 1)
	const float m = land_balance::Get(4);
	const float life = villager->life;
	const auto& group = info->speedGroup;
	float speed = 0.0f;
	// the emergency and normal branches go through the food speed-up (0x753976 / 0x753B13 -> 0x753B15); the two wounded
	// ones jump past it (0x7538EB / 0x753934 -> 0x753B36)
	bool foodSpeedUpApplies = false;
	if (life <= info->lifeWhenCrawlsWounded)
	{
		// 0x7538D3: GameFloatRand(0.2) + 0.4
		speed = (game_random::GameFloatRand(0.2f) + 0.4f) * static_cast<float>(Raw(group.speed4)) * m;
	}
	else if (life <= info->lifeWhenWalksWounded)
	{
		// 0x75391C: GameFloatRand(0.25) + 0.5
		speed = (game_random::GameFloatRand(0.25f) + 0.5f) * static_cast<float>(Raw(group.speedDefault)) * m;
	}
	else if (const auto* town = villager->town != entt::null ? registry.TryGet<const Town>(villager->town) : nullptr;
	         town != nullptr && town_queries::IsInStateOfEmergency(*town))
	{
		// 0x753939..0x753972: in a town in a state of emergency (Town::IsInStateOfEmergency 0x747970),
		// (GameFloatRand(0.5) + 0.75) x the fleeing speed (GVillagerInfo +0x108) x m (0x75395A)
		speed = (game_random::GameFloatRand(0.5f) + 0.75f) * static_cast<float>(Raw(group.speedFleeing)) * m;
		foodSpeedUpApplies = true;
	}
	else
	{
		// 0x75397F..0x7539F4: T = 1 without a town; with one, base (+0x36C) + clamp(TownNeedsSum fn_00747150 / divisor
		// (+0x370), 0, 0.5) (on the x87 stack: float precision, the x87 control word is 24 bits, 0x7DEE0D)
		const float townNeeds = villager->town != entt::null
		                            ? villager::TownNeedsFactor(town_desire::TownNeedsSum(villager->town), *info)
		                            : 1.0f;
		// 0x7539F8..0x753AF7: the loads of wood and food (+0xF6 / +0xF4), with the trader's capacities for disciple 9
		const auto load = villager::LoadFactors(villager->resourceHeld.at(1), villager->resourceHeld.at(0), *info,
		                                        villager->discipleType == k_DiscipleTrader);
		// 0x753AFD..0x753B0D: fild spd; x foodF; x woodF; x T; x m; fstp float (each fmul rounded to float, 0x7DEE0D)
		speed = static_cast<float>(static_cast<int32_t>(SpeedGroupEntry(group, states[final].field0x24))) * load.food *
		        load.wood * townNeeds * m;
		foodSpeedUpApplies = true;
		if (villager::TraceOn(entity))
		{
			villager::Trace(entity, fmt::format("speed: spd {} foodF {:.9f} woodF {:.9f} T {:.9f} -> {:.6f}",
			                                    SpeedGroupEntry(group, states[final].field0x24), load.food, load.wood,
			                                    townNeeds, speed));
		}
	}
	// 0x753B15..0x753B38: IsFoodSpeedUp (vt +0x87C, 0x55C980: +0xF0 != 0) -> speed x info +0x39C (foodPowerupIncrease,
	// (inferred) the field at that offset of openblack's GVillagerInfo), kept on the x87 stack (float precision,
	// 0x7DEE0D); then __ftol
	int32_t whole = static_cast<int32_t>(speed);
	if (foodSpeedUpApplies && villager->foodSpeedUp != 0)
	{
		whole = static_cast<int32_t>(speed * info->foodPowerupIncrease);
	}
	// Villager::SetSpeed: the factor of the villager (its creation index), age, and for adults food, life and sex
	// ObjectCreationIndex (+0x3C), a signed int in the original's multiplication
	const auto index = static_cast<int32_t>(std::max<int64_t>(object_index::Of(entity), 0));
	float f = static_cast<float>((index * 47) % 31 - 16) * 0.01f + 1.0f;
	// Living::GetAge 0x5ECAF0 (vt +0x8D0, an unsigned div), compared unsigned with GLivingInfo +0x138 grownUpAge
	// (0x750F26, jae) and +0x13C oldAge (0x750F87, jbe); the differences are loaded as unsigned qwords (fild, high
	// dword 0: 0x750F47 / 0x750FA6), times 0.2 and 0.1 (0x8AA3AC, 0x8AC404), at most 0.4 (0x8C7A44)
	const uint32_t age = villager::AgeFromBirthTurn(villager->birthTurn, CurrentGameTurn());
	const uint32_t grownUp = info->grownUpAge;
	const uint32_t old = info->oldAge;
	if (age < grownUp)
	{
		f -= std::min(static_cast<float>(grownUp - age) * 0.2f * 0.1f, 0.4f);
	}
	else if (age > old)
	{
		f -= std::min(static_cast<float>(age - old) * 0.2f * 0.1f, 0.4f);
	}
	else
	{
		// 0x750FDB..0x750FEE: GetDesireForFood 0x75BB50 (POWER(food) = 1 - min(food, 1)^3) * 0.1
		f -= villager::GetDesireForFood(entity) * 0.1f;
		// 0x750FF2..0x751008: life (vt +0x11C) * 0.1, and 0.2 more for a woman (GVillagerInfo +0x1F8 == 1, which
		// Villager::sex mirrors)
		f -= life * 0.1f;
		if (villager->sex == Villager::Sex::FEMALE)
		{
			f -= 0.2f;
		}
	}
	const auto raw = std::clamp(static_cast<int32_t>(static_cast<float>(whole) * f), 0, 0xFFFF);
	// MobileWallHug::SetSpeed 0x60FC50 clamps to 0..0xFFFF and stores the u16 at +0x5A as it is: the distance per turn in
	// MapCoords units. openblack keeps the speed in metres, so it converts here; (inferido) the original never converts
	// this value, it adds it to a MapCoords, and ToMetres ([0x8AA3A4], the conversion it uses everywhere else) is the
	// nearest thing to it (it differs from / 6553.6f by at most one bit)
	wallHug->speed = map_coords::ToMetres(raw);
}

float VillagerScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	// InitialiseScale 0x74FB80, then SetScaleForAge 0x752A90 from it: GameFloatRand (Villager.cpp 0x92F / 0x933), the
	// game's synced draws in the constructor's order (V4; before, openblack's own generator)
	return villager::ScaleForAge(info, age, villager::InitialScaleForAge(info, age));
}

} // namespace openblack::ecs
