/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAge.h"

#include <bit>

#include <fmt/format.h>

#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// A villager's age, scale, old age and pregnancy (VillagerAge.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

float ScaleOf(entt::entity villager)
{
	const auto* t = Entities().TryGet<const Transform>(villager);
	return t != nullptr ? t->scale.x : 1.0f;
}

void SetScale(entt::entity villager, float scale)
{
	// The uniform scale
	if (auto* t = Entities().TryGet<Transform>(villager))
	{
		t->scale = glm::vec3(scale);
	}
}
} // namespace

// ---- the villager info's tables ----------------------------------------------------------------------------------

float InitialScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	// At age 0 the field before the table (dancingSpeed), read as a float, as the original does
	return villager_age::StartScale(age, info.grownUpAge, info.ageToScale.values, std::bit_cast<float>(info.dancingSpeed));
}

float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current)
{
	return villager_age::GrownScale(age, info.grownUpAge, info.ageToScale.values, current,
	                                [](float limit) { return GameFloatRand(limit); });
}

villager_age::NewbornKind NewbornKindOf(const GVillagerInfo& info)
{
	return {
	    .grownUp = info.grownUpAge,
	    .hungryForFood = info.hungryForFood,
	    .processChecksEvery = info.processChecksEvery,
	    .ageToScale = info.ageToScale.values,
	    .beforeTable = std::bit_cast<float>(info.dancingSpeed),
	};
}

// ---- the villager ------------------------------------------------------------------------------------------------

void InitialiseScale(entt::entity villager, uint32_t age)
{
	SetScale(villager, InitialScaleForAge(InfoOf(villager), age));
}

void SetScaleForAge(entt::entity villager, uint32_t age)
{
	SetScale(villager, ScaleForAge(InfoOf(villager), age, ScaleOf(villager)));
}

uint32_t SetAgeAndScale(entt::entity villager, uint32_t age)
{
	const auto& info = InfoOf(villager);
	const uint32_t turn = CurrentTurn();
	return SetAge(villager, info, age, turn, [villager, &info](uint32_t set) {
		// The meshes only when the age crosses grownUpAge (GetAge with the old birth turn): a new child the child
		// meshes, a new adult the adult detail meshes. (pending) the skeleton branch is not here
		const auto setting = villager_age::SetAge(set, GetAge(villager), info.grownUpAge);
		if (setting.childModel.has_value())
		{
			SetVillagerMeshes(villager, info, *setting.childModel, false);
		}
		InitialiseScale(villager, set);
		SetScaleForAge(villager, set);
	});
}

uint32_t CheckChildGrownUp(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	const uint32_t age = GetAge(villager);
	if (!villager_age::IsChildAge(age, info.grownUpAge))
	{
		// No longer a child
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagChild);
		// SetAge(max(grownUpAge, 18)): the age a villager of grownUpAge is given
		const uint32_t to = villager_age::GivenAge(info.grownUpAge, info.grownUpAge);
		SetAgeAndScale(villager, to);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: grown {} -> {} (abode {})", age, to,
			                            static_cast<uint32_t>(Entities().Get<const Villager>(villager).abode)));
		}
		// The abode's ChildToAdult, else the town's
		const auto* again = Entities().TryGet<const Villager>(villager);
		// (guard) the original reads the abode link unchecked: this stands for the abode's unlinking on deletion
		if (again != nullptr && again->abode != entt::null && ecs::IsAvailable(again->abode))
		{
			abode_villagers::ChildToAdult(again->abode, villager);
		}
		// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
		else if (again != nullptr && again->town != entt::null && ecs::IsAvailable(again->town))
		{
			town_villagers::ChildToAdult(again->town, villager);
		}
		return ChildBecomesAdult(villager);
	}
	// Not grown up: rescale every 375 turns
	if (villager_age::IsGrowthTurn(CurrentTurn()))
	{
		SetScaleForAge(villager, age);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: rescale {} -> {:.4f}", age, ScaleOf(villager)));
		}
	}
	return 0;
}

uint32_t ChildBecomesAdult(entt::entity villager)
{
	if (auto* v = Entities().TryGet<Villager>(villager))
	{
		v->mother = entt::null;
	}
	// CheckNeedNewAbode, then 234 GO_HOME_AND_CHANGE; 1
	CheckNeedNewAbode(villager);
	SetTopState(villager, VillagerStates::GoHomeAndChange);
	return 1;
}

uint32_t ChildBecomesAdultState(LivingAction& action)
{
	return ChildBecomesAdult(Entities().ToEntity(action));
}

bool CheckDeathFromOldAge(entt::entity villager)
{
	const auto& info = InfoOf(villager);
	// Not past oldAge -> 0, no draw
	const uint32_t age = GetAge(villager);
	if (!(age > info.oldAge))
	{
		return false;
	}
	// r = GameFloatRand(1), d = GameRand(n), past retirementAge dies (villager_age::DiesOfOldAge)
	float r = 0.0f;
	uint32_t n = 0;
	uint32_t d = 0;
	const bool dies = villager_age::DiesOfOldAge(
	    age, {.grownUp = info.grownUpAge, .old = info.oldAge, .oldest = info.retirementAge},
	    [&r](float limit) {
		    r = GameFloatRand(limit);
		    return r;
	    },
	    [&n, &d](uint32_t limit) {
		    n = limit;
		    d = GameRand(limit);
		    return d;
	    });
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("age: old age {} r={:.6f} n={} d={} -> {}", age, r, n, d, dies ? "die" : "live"));
	}
	if (!dies)
	{
		return false;
	}
	// Dies of old age, by no player, with the info's life
	VillagerDead(villager, DeathReason::OldAge, std::nullopt, info.life, 1);
	return true;
}

bool IsPregnant(entt::entity villager)
{
	// A woman with a pregnancy count
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && InfoOf(villager).sex == SexType::Female && v->pregnancy != 0;
}

uint32_t UpdatePregnancy(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	// Pregnant and not controlled by a script
	if (v == nullptr || !IsPregnant(villager) || script_held::IsControlledByScript(villager))
	{
		return 0;
	}
	// Count the pregnancy down by the turns since the last check (16 bits); still > 0 -> 0
	const auto turns = GetGameTurnsSinceLastChecked(villager, CurrentTurn());
	v->pregnancy = static_cast<int16_t>(static_cast<uint16_t>(v->pregnancy) - static_cast<uint16_t>(turns));
	if (v->pregnancy > 0)
	{
		return 0;
	}
	// Due: start giving birth (VillagerBirth.cpp)
	return HousewifeStartsGivingBirth(villager);
}
} // namespace openblack::ecs::villager
