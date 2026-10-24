/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMissionary.h"

#include <glm/mat3x3.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MissionaryControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Locator.h"

// The missionary disciple (VillagerMissionary.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace reactions = effects::reactions;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

bool IsMissionary(entt::entity villager)
{
	const auto* v = Entities().Valid(villager) ? Entities().TryGet<const Villager>(villager) : nullptr;
	return v != nullptr && v->discipleType == static_cast<uint8_t>(VillagerDisciple::Missionary);
}

bool ReactionAvailable(uint32_t reaction)
{
	return reaction != 0 && reactions::IsAvailable(reactions::Find(reaction));
}
} // namespace

uint32_t MissionaryDisciple(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// The closest abode of any player's town or the neutral one's
	const auto abode = abode_queries::FindClosestAbode(object::MapCoordsOf(villager), std::nullopt, false, false);
	const auto town = abode != entt::null ? town_queries::GetTown(abode) : entt::null;
	// (guard) the original reads its own town's player unchecked: a missionary of no town decides what to do
	const auto player = GetPlayerOf(villager);
	if (town == entt::null || !player.has_value())
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// That town is preached to for its own player; it moves in and goes home
	AddMissionary(town, villager, *player);
	TraceFormatted(villager, "missionary: preaching in town {} for player {}", static_cast<uint32_t>(town),
	               static_cast<int>(*player));
	ForceMoveVillagerToAbode(villager, abode);
	SetTopState(villager, VillagerStates::GoHomeAndChange);
	return 1;
}

void AddMissionary(entt::entity town, entt::entity villager, PlayerNames player)
{
	auto& registry = Entities();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	// The control is made at the map's origin, and its reaction first spreads from there: it stands where the
	// missionary is only from its first turn on (ProcessMissionary)
	const auto control = registry.Create();
	registry.Assign<Transform>(control, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MissionaryControl>(control, MissionaryControl {.missionary = villager});
	const auto reaction = reactions::CreateReaction(control, Reaction::ReactToMissionary, player, false);
	registry.Get<MissionaryControl>(control).reaction = reaction;
	// at the head of the town's list
	registry.Get<Town>(town).missionaries.insert(registry.Get<Town>(town).missionaries.begin(), control);
}

void ProcessMissionary(entt::entity control)
{
	auto& registry = Entities();
	auto* mc = registry.TryGet<MissionaryControl>(control);
	if (mc == nullptr)
	{
		return;
	}
	if (mc->missionary == entt::null || mc->reaction == 0)
	{
		DeleteMissionary(control);
		return;
	}
	// The missionary gone or no longer one: the control and its reaction go
	if (!ecs::IsAvailable(mc->missionary) || !IsMissionary(mc->missionary))
	{
		// (its reaction is taken off first: the control may be destroyed at once)
		if (ReactionAvailable(mc->reaction))
		{
			reactions::RemoveAllReactionsInitiatedByObject(control);
		}
		DeleteMissionary(control);
		return;
	}
	// Its reaction gone: the control goes
	if (!ReactionAvailable(mc->reaction))
	{
		DeleteMissionary(control);
		return;
	}
	// It follows the missionary
	registry.Get<Transform>(control).position = registry.Get<const Transform>(mc->missionary).position;
}

void DeleteMissionary(entt::entity control)
{
	auto& registry = Entities();
	if (const auto* mc = registry.TryGet<const MissionaryControl>(control); mc != nullptr && IsMissionary(mc->missionary))
	{
		// a missionary still at it is one no more
		SetVillagerDisciple(mc->missionary, entt::null, VillagerDisciple::None, 0);
	}
	ecs::ToBeDeleted(control);
}

void ProcessMissionaries(entt::entity town)
{
	auto& registry = Entities();
	if (!registry.AllOf<Town>(town))
	{
		return;
	}
	// newest first; each control that is no longer available after its turn leaves the list
	const auto controls = registry.Get<const Town>(town).missionaries;
	for (const auto control : controls)
	{
		ProcessMissionary(control);
		if (!ecs::IsAvailable(control))
		{
			auto& list = registry.Get<Town>(town).missionaries;
			std::erase(list, control);
		}
	}
}

void DeleteAllMissionaries(entt::entity town)
{
	auto& registry = Entities();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	const auto controls = std::move(t->missionaries);
	t->missionaries.clear();
	for (const auto control : controls)
	{
		DeleteMissionary(control);
	}
}

float MissionaryImpressiveValue(float landBalance, float missionaryValue, float multiplier, float beliefShare, float boredom)
{
	// the missionary's own value, times the reaction's multiplier; then the boredom by the distance's share of it
	const float value = landBalance * missionaryValue * multiplier;
	return boredom * beliefShare * value;
}
} // namespace openblack::ecs::villager
