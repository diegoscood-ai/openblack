/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCover.h"

#include <numbers>
#include <vector>

#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerHidingRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerReactions.h"
#include "VillagerShield.h"

namespace openblack::ecs::villager_cover
{
using components::Abode;
using components::LivingAction;
using components::Spell;
using components::Transform;
using components::VillagerHiding;
using components::VillagerPanic;
namespace hiding = villager_hiding;
namespace tq = town_queries;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

auto IntRandom()
{
	return [](uint32_t limit) { return villager::GameRand(limit); };
}

/// Where a thing that frightened a villager is: a shield spell where it was cast, anything else where it stands
glm::ivec2 ThreatPos(entt::entity threat)
{
	const auto& registry = Entities();
	if (const auto* spell = registry.TryGet<const Spell>(threat))
	{
		return tq::ToMapCoords(glm::vec2(spell->position.x, spell->position.z));
	}
	return tq::PosOf(threat);
}

/// Whether a building is still there and can hide a villager: any of its town that works, but the open places
bool CanHideIn(entt::entity building)
{
	auto& registry = Entities();
	if (building == entt::null || !abode_queries::IsAvailable(building) || !registry.AllOf<Abode, Transform>(building))
	{
		return false;
	}
	const auto* info = abode_villagers::InfoOf(building);
	return info != nullptr && hiding::CanBeHiddenIn(info->abodeType, abode_queries::IsFunctional(building));
}

/// Whether what frightened a villager is still about: a shield only while its miracle stands
bool ThreatAvailable(entt::entity threat)
{
	auto& registry = Entities();
	if (threat == entt::null || !registry.Valid(threat))
	{
		return false;
	}
	if (registry.AllOf<Spell>(threat))
	{
		return villager_shield::IsShieldSpellAvailable(threat);
	}
	return registry.AllOf<Transform>(threat);
}

/// The villager stops being frightened and goes back to what it was doing
uint32_t StopHiding(entt::entity villager)
{
	Entities().RemoveState<VillagerHiding>(villager);
	villager_reactions::StopReactingAndSetState(villager);
	return 1;
}

void GoBackInside(entt::entity villager, entt::entity building)
{
	villager::SetupMoveToOnFootpath(villager, building, abode_queries::GetArrivePos(building),
	                                VillagerStates::GoAndHideInNearbyBuilding);
}
} // namespace

bool SetupGoAndHideInNearbyBuilding(entt::entity villager, entt::entity threat, Reaction threatKind)
{
	auto& registry = Entities();
	const auto town = villager::GetTown(villager);
	if (town == entt::null || !registry.Valid(town))
	{
		return false;
	}
	const auto buildings = town_stats::AbodesOf(town);
	const auto here = tq::PosOf(villager);
	std::vector<hiding::Shelter> shelters;
	shelters.reserve(buildings.size());
	for (const auto building : buildings)
	{
		const bool hides = CanHideIn(building);
		shelters.push_back({.distance = hides ? tq::GetDistanceInMetres(here, abode_queries::GetArrivePos(building)) : 0.0f,
		                    .canBeHiddenIn = hides});
	}
	const auto nearest = hiding::NearestShelter(shelters);
	if (!nearest.has_value())
	{
		return false;
	}
	const auto building = buildings.at(*nearest);
	registry.AssignOrReplaceState<VillagerHiding>(
	    villager, VillagerHiding {.building = building, .turns = 0, .threat = threat, .threatKind = threatKind});
	GoBackInside(villager, building);
	return true;
}

void SetPanicPoint(entt::entity villager, glm::vec2 point)
{
	Entities().AssignOrReplaceState<VillagerPanic>(villager, VillagerPanic {.point = point});
}

void SetupReactToMagicShieldDestroyed(entt::entity villager, entt::entity spell, uint32_t reaction)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction>(villager) || !registry.AllOf<Spell>(spell))
	{
		return;
	}
	// The shield it was amazed by, if any: what it hides from, or goes on watching
	const auto watched = villager_shield::ReactionObject(villager);
	const auto watchedKind = watched != entt::null ? Reaction::ReactToMagicShield : Reaction::None;
	// First it runs to hide in a building of its town nearby, not held by this reaction
	if (SetupGoAndHideInNearbyBuilding(villager, watched, watchedKind))
	{
		return;
	}
	// With nowhere to hide it runs from where the shield stood, a little to one side, then looks back there in panic
	const auto& row = Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToMagicShieldDestroyed));
	const auto& component = registry.Get<const Spell>(spell);
	const auto centre = tq::ToMapCoords(glm::vec2(component.position.x, component.position.z));
	const auto me = tq::PosOf(villager);
	constexpr float k_TurnRange = std::numbers::pi_v<float> / 4.0f;
	const float angle = tq::Get3DAngleFromXZ(centre, me) + villager::GameFloatRand(k_TurnRange) - (k_TurnRange * 0.5f);
	const float distance = row.minDistanceToRunAwayFromObject +
	                       villager::GameFloatRand(row.maxDistanceToRunAwayFromObject - row.minDistanceToRunAwayFromObject);
	villager_reactions::AddReaction(villager, reaction, VillagerStates::PanicReaction);
	// It goes on watching the shield it was amazed by; with none it watches nothing, and so stops reacting on its next
	// turn (vanilla bug: the panic set-up gives it nothing to watch)
	if (watched != entt::null)
	{
		villager_reactions::SetSlotObject(villager, watched);
	}
	villager::SetupMoveToWithHug(villager, tq::ToMetres(me + tq::GetPosFromAngle(angle, distance)),
	                             VillagerStates::PanicReaction);
	SetPanicPoint(villager, tq::ToMetres(centre));
}

uint32_t GoAndHideInNearbyBuilding(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* hidingIn = registry.TryGet<VillagerHiding>(villager);
	if (hidingIn == nullptr)
	{
		return StopHiding(villager);
	}
	// What frightened it is forgotten once it is gone
	if (hidingIn->threat != entt::null && !ThreatAvailable(hidingIn->threat))
	{
		hidingIn->threat = entt::null;
	}
	const auto building = hidingIn->building;
	const bool stands = CanHideIn(building);
	const bool atDoor = stands && villager::AreWeThere(villager, tq::ToMetres(abode_queries::GetArrivePos(building)), 0.0f);
	bool threatNear = false;
	if (stands && atDoor && hidingIn->threat != entt::null)
	{
		const auto& row = Locator::infoConstants::value().reaction.at(static_cast<size_t>(hidingIn->threatKind));
		threatNear =
		    tq::GetDistanceInMetres(tq::PosOf(villager), ThreatPos(hidingIn->threat)) <= row.minDistanceToRunAwayFromObject;
	}
	const auto step = hiding::HideStep(
	    {.shelterStands = stands, .atDoor = atDoor, .threatNear = threatNear, .turnsInside = hidingIn->turns}, IntRandom());
	hidingIn->turns = step.turnsInside;
	switch (step.what)
	{
	case hiding::Hiding::GiveUp:
		return StopHiding(villager);
	case hiding::Hiding::WalkToDoor:
		GoBackInside(villager, building);
		return 1;
	case hiding::Hiding::ComeOut:
		// It steps out of the door and looks about
		villager::SetupMoveToWithHug(villager, tq::ToMetres(villager::FindPosOutsideAbode(villager, building)),
		                             VillagerStates::LookToSeeIfItIsSafe);
		return 1;
	case hiding::Hiding::StayWhileThreatNear:
	case hiding::Hiding::Stay:
		return 1;
	}
	return 1;
}

uint32_t LookToSeeIfItIsSafe(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* hidingIn = registry.TryGet<VillagerHiding>(villager);
	if (hidingIn == nullptr)
	{
		return StopHiding(villager);
	}
	// TODO(villagers): a predator still awake and about within the reaction's reach sends it back inside, or makes it
	// run if its building is gone; the villagers' reactions to predators are not here yet
	const auto step = hiding::LookAbout(hidingIn->turns, IntRandom());
	hidingIn->turns = step.turns;
	if (step.safe)
	{
		return StopHiding(villager);
	}
	return 1;
}

uint32_t PanicReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto* panic = registry.TryGet<const VillagerPanic>(villager);
	const auto point = panic != nullptr ? tq::ToMapCoords(panic->point) : tq::PosOf(villager);
	// It turns quickly to look back at what it ran from, then plays its clip and decides afresh
	constexpr uint32_t k_QuickestTurn = 2;
	if (villager::LookAtPos(villager, point, k_QuickestTurn) == 1)
	{
		registry.RemoveState<VillagerPanic>(villager);
		villager::PlayAnimThenSetState(villager, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

void Clear()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	std::vector<entt::entity> hiding;
	registry.Each<const VillagerHiding>(
	    [&hiding](entt::entity villager, const VillagerHiding&) { hiding.push_back(villager); });
	for (const auto villager : hiding)
	{
		registry.RemoveState<VillagerHiding>(villager);
	}
	std::vector<entt::entity> panicking;
	registry.Each<const VillagerPanic>(
	    [&panicking](entt::entity villager, const VillagerPanic&) { panicking.push_back(villager); });
	for (const auto villager : panicking)
	{
		registry.RemoveState<VillagerPanic>(villager);
	}
}

} // namespace openblack::ecs::villager_cover
