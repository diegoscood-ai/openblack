/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCreatureDance.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureDance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Impressiveness.h"
#include "VillagerDance.h"

namespace openblack::ecs::villager_creature_dance
{
using namespace components;

namespace
{
/// The move of a dance that turns its dancers, which a villager back from a reaction dances whatever its group does
constexpr uint32_t k_TurnMove = 9;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The dance a villager dances in, when it dances in a group of it
const Dance* PlacedDanceOf(entt::entity villager)
{
	auto& registry = Entities();
	if (!dances::PlaceOf(registry, villager).has_value())
	{
		return nullptr;
	}
	return &registry.Get<const Dance>(dances::DanceOf(registry, villager));
}

/// The villager's place in its dance, as a walk reaches it
std::optional<map_coords::MapCoords> PlaceInDance(entt::entity villager)
{
	auto& registry = Entities();
	const auto at = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(villager).position));
	const auto turn = dances::DancerTurnOf(registry, villager, at, villager::GetGameAngle(villager));
	return turn.has_value() ? std::optional(turn->place) : std::nullopt;
}
} // namespace

bool AvailableToBeSummoned(entt::entity villager)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value() || dances::DanceOf(registry, villager) != entt::null ||
	    person->lifeStage == Villager::LifeStage::Child || person->discipleType != 0)
	{
		return false;
	}
	return villager::InfoOf(villager).damageThresholdToGoHome < life::LifeOf(villager);
}

std::vector<entt::entity> SummonOrder(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> order;
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return order;
	}
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* building = registry.TryGet<const Abode>(abode);
		if (building == nullptr)
		{
			continue;
		}
		for (const auto villager : building->inhabitants)
		{
			if (registry.Valid(villager) && registry.AllOf<Villager, LivingAction, Transform>(villager) &&
			    AvailableToBeSummoned(villager))
			{
				order.push_back(villager);
			}
		}
	}
	// The creature calls the last one found first
	std::ranges::reverse(order);
	return order;
}

bool StartWorshippingCreature(entt::entity villager, entt::entity dance)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr || !dances::AddDancer(registry, dance, villager, dances::DanceSex(registry, villager)) ||
	    !dances::PlaceOf(registry, villager).has_value())
	{
		return false;
	}
	// A villager whose state leaves a dance on its way out has its top state written straight, so that it keeps its
	// new place in this one
	if (villager::IsStateExitFunctionSameAs(villager, VillagerStates::WorshippingCreature))
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) =
		    static_cast<uint8_t>(VillagerStates::ControlledByCreature);
	}
	static_cast<void>(villager::SetTopState(villager, VillagerStates::ControlledByCreature));
	SetVillagerSpeed(villager, static_cast<int32_t>(villager::InfoOf(villager).speedGroup.speed3), true);
	return true;
}

bool SendToPlace(entt::entity villager, entt::entity dance)
{
	const auto place = PlaceInDance(villager);
	if (!place.has_value() || !Entities().AllOf<LivingAction>(villager))
	{
		return false;
	}
	const auto run = static_cast<int32_t>(villager::InfoOf(villager).speedGroup.speedFleeing) * 2;
	SetVillagerSpeed(villager, run, true);
	// The villager walks the dance's footpaths where it has any, else straight to its place
	villager::SetupMoveToOnFootpath(villager, dance, glm::ivec2(place->x, place->z), VillagerStates::ControlledByCreature);
	// Its new state set its speed: the run is set again
	SetVillagerSpeed(villager, run, true);
	return true;
}

void TellToDance(entt::entity villager)
{
	if (PlacedDanceOf(villager) != nullptr)
	{
		static_cast<void>(villager::SetTopState(villager, VillagerStates::WorshippingCreature));
	}
}

void WorshipCreature(entt::entity villager, entt::entity owner, DanceInfo type)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || owner == entt::null || !registry.Valid(owner) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto town = person->town;
	const auto* creature = registry.TryGet<const Creature>(owner);
	if (creature == nullptr || town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto& constants = Locator::infoConstants::value();
	const auto row = static_cast<size_t>(type);
	const auto species = creature::InfoRow(creature->species);
	if (type == DanceInfo::None || row >= constants.dance.size() || species >= constants.creature.size())
	{
		return;
	}
	const auto& stats = registry.Get<const Town>(town).stats;
	const auto& info = constants.creature.at(species);
	const auto size = magic::CreatureImpressiveSize(creature->size, info.impressiveSizeAddition, info.impressiveSizeMultiplier);
	const auto belief =
	    creature_dance::WorshipBelief(constants.town.populationForUnmodifiedBelief, stats.adults + stats.children, size,
	                                  constants.dance.at(row).impressiveness);
	town_stores::AddToBelief(town, creature->owner, belief, villager, true, static_cast<int>(GuidanceAlignment::Good));
}

uint32_t ControlledByCreature(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto* dance = PlacedDanceOf(villager);
	const auto place = dance != nullptr ? PlaceInDance(villager) : std::nullopt;
	if (!place.has_value())
	{
		return 1;
	}
	// Only a villager that stands right at the middle of the dance walks to its place
	const auto here = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(villager).position));
	const bool there = here.x == place->x && here.z == place->z;
	if (!there && here.x == dance->place.x && here.z == dance->place.z)
	{
		static_cast<void>(
		    villager::SetupMoveToPos(villager, map_coords::ToMetres(*place), VillagerStates::ControlledByCreature));
	}
	return 1;
}

uint32_t ExitControlledByCreature(LivingAction& action, VillagerStates next)
{
	if (next == VillagerStates::InHand || next == VillagerStates::Flying || villager_animation::IsDeathState(next))
	{
		auto& registry = Entities();
		const auto villager = registry.ToEntity(action);
		// It leaves the list of the first creature that called it
		bool left = false;
		registry.Each<CreatureDance>([villager, &left](entt::entity, CreatureDance& dance) {
			const auto found = std::ranges::find(dance.dancers, villager);
			if (left || found == dance.dancers.end())
			{
				return;
			}
			dance.dancers.erase(found);
			left = true;
		});
		dances::RemoveDancer(registry, villager);
		return 1;
	}
	return next == VillagerStates::ControlledByCreature || next == VillagerStates::WorshippingCreature ||
	               next == VillagerStates::DecideWhatToDo
	           ? 1
	           : 0;
}

uint32_t WorshippingCreature(LivingAction& action, const villager_dance::WalkStep& walk)
{
	const auto villager = Entities().ToEntity(action);
	const auto* dance = PlacedDanceOf(villager);
	if (dance == nullptr)
	{
		return 1;
	}
	if (dance->state == Dance::State::Dancing)
	{
		return villager_dance::PerformDance(action, walk, VillagerStates::WorshippingCreature);
	}
	static_cast<void>(villager::GoHome(villager));
	return 1;
}

uint32_t RestartWorshippingCreature(LivingAction& action, const villager_dance::WalkStep& walk)
{
	const auto villager = Entities().ToEntity(action);
	if (PlacedDanceOf(villager) != nullptr)
	{
		return villager_dance::PerformDance(action, walk, VillagerStates::WorshippingCreature, k_TurnMove);
	}
	static_cast<void>(villager::GoHome(villager));
	return 1;
}

} // namespace openblack::ecs::villager_creature_dance
