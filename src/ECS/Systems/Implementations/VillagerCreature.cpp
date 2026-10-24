/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCreature.h"

#include <optional>

#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerInspection.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Effects/CreatureReactionFacts.h"
#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Effects/VillagerCreatureRules.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/LivingTurn.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Town/TownCreatureAttitude.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerSpeed.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerReactions.h"
#include "VillagerWorship.h"

namespace openblack::ecs::villager_creature
{
using components::LivingAction;
using components::VillagerInspection;
namespace rules = effects::creature_reaction;
namespace tq = town_queries;
namespace attitude = town_creature_attitude;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const ReactionInfo& InfoOf(openblack::Reaction type)
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
}

bool IsCreature(entt::entity thing)
{
	return creature_reaction_facts::IsCreature(thing);
}

/// The plan the creature holds as others see it, none once a plan has ended and before the next
std::optional<creature_planner::Plan> HeldPlan(entt::entity creature)
{
	return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().HeldPlanOf(creature) : std::nullopt;
}

/// The creature holds a plan done to a thing still there: itself when the plan names nothing
bool ActsOnSomething(entt::entity creature)
{
	const auto plan = HeldPlan(creature);
	return plan.has_value() && (!plan->object.has_value() || Entities().Valid(static_cast<entt::entity>(*plan->object)));
}

/// The reaction's creature, after the test every state starts with: none when it is gone or not available, and then
/// the villager stops reacting
std::optional<entt::entity> ObjectThere(entt::entity villager)
{
	const auto object = villager_reactions::SlotObject(villager);
	if (object == entt::null || !fire::traits::IsAvailable(object))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return std::nullopt;
	}
	return object;
}

VillagerInspection& InspectionOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* inspection = registry.TryGet<VillagerInspection>(villager); inspection != nullptr)
	{
		return *inspection;
	}
	return registry.AssignOrReplaceState<VillagerInspection>(villager);
}

uint32_t TurnsForSeconds(float seconds)
{
	return static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
}

/// What the two turn counts read: the reaction info's distances and turns for normal things, the creature's actions
/// and the villager's town's attitude to it
rules::TurnsFacts TurnsFactsOf(entt::entity villager, entt::entity object, uint8_t type, float distance, bool again)
{
	auto facts = creature_reaction_facts::TurnsFactsOf(object, type, distance, again);
	if (!facts.isCreature)
	{
		return facts;
	}
	if (const auto town = villager::GetTown(villager); town != entt::null)
	{
		facts.townAttitude = attitude::AttitudeOf(town, object);
	}
	return facts;
}
} // namespace

void SetupReactToCreature(entt::entity villager, entt::entity creature, uint32_t reaction)
{
	// (openblack, guard) the original takes the initiator for a creature without asking, and would fail on anything else
	if (!IsCreature(creature) || !fire::traits::IsAvailable(creature))
	{
		return;
	}
	const auto town = villager::GetTown(villager);
	if (town == entt::null)
	{
		villager_reactions::AddReaction(villager, reaction, VillagerStates::InspectCreatureReaction, creature);
		return;
	}
	// its town keeps a record of the creature (a new one: not yet judged), then its attitude chooses
	attitude::CreateRecord(town, creature);
	const auto townAttitude = attitude::AttitudeOf(town, creature);
	const auto state = rules::SetupState(townAttitude);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("react to creature {}: attitude {} -> {}", static_cast<uint32_t>(creature),
		                                      townAttitude, state.has_value() ? static_cast<int>(*state) : -1));
	}
	if (!state.has_value())
	{
		// nothing taken up; the creature becomes the object of the reaction it follows, if any
		villager_reactions::SetSlotObject(villager, creature);
		return;
	}
	villager_reactions::AddReaction(villager, reaction, *state, creature);
}

uint8_t ReactToCreaturePriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	auto& registry = Entities();
	const auto* found = effects::reactions::Find(reaction);
	rules::PriorityFacts facts {.dancing = dances::DanceOf(registry, villager) != entt::null};
	facts.isCreature = found != nullptr && IsCreature(found->initiator);
	if (facts.dancing || !facts.isCreature)
	{
		return rules::VillagerPriority(facts);
	}
	const auto creature = found->initiator;
	const auto& infos = Locator::infoConstants::value().reaction;
	// villagers never ignore a creature in the game: nothing sets it
	facts.ignoredByVillagers = false;
	const auto* spells = registry.TryGet<const components::CreatureSpells>(creature);
	facts.invisible = spells != nullptr && spells->invisible;
	facts.distance = tq::GetDistanceInMetres(tq::PosOf(villager), tq::PosOf(creature));
	facts.maxDistanceToRunAway = InfoOf(found->type).maxDistanceToRunAwayFromObject;
	facts.priority = InfoOf(found->type).priority;
	facts.fleePriority = infos.at(static_cast<size_t>(openblack::Reaction::FleeFromObject)).priority;
	facts.followPriority = infos.at(static_cast<size_t>(openblack::Reaction::FollowObject)).priority;
	const auto town = villager::GetTown(villager);
	facts.hasTown = town != entt::null && registry.Valid(town) && registry.AllOf<components::Town>(town);
	if (facts.hasTown)
	{
		facts.attitude = attitude::AttitudeOf(town, creature);
		facts.sameOwner =
		    registry.Get<const components::Town>(town).owner == registry.Get<const components::Creature>(creature).owner;
		const auto* component = registry.TryGet<const components::Villager>(villager);
		facts.disciple = component != nullptr && (component->flags & components::Villager::k_FlagDisciple) != 0;
		const auto& row = villager::state_info::StateInfo(villager::GetFinalState(villager));
		facts.servedDesire = villager::state_info::ServedDesire(row);
		facts.servedDesireAmount = villager::state_info::ServedDesireAmount(row);
		if (facts.servedDesire >= 0 && facts.servedDesire <= static_cast<int32_t>(TownDesireInfo::ForSleep))
		{
			facts.townRawDesire = town_desire::GetRawDesire(town, static_cast<TownDesireInfo>(facts.servedDesire));
		}
	}
	return rules::VillagerPriority(facts);
}

uint32_t TurnsToReactToCreature(entt::entity villager, entt::entity object, uint8_t type, float distance)
{
	return rules::TurnsToReact(TurnsFactsOf(villager, object, type, distance, false));
}

uint32_t TurnsBeforeReactingAgainToCreature(entt::entity villager, entt::entity object, uint8_t type, float distance)
{
	// (openblack, guard) the original takes the initiator for a creature without asking: anything else reacts again at
	// once
	if (!IsCreature(object))
	{
		return 0;
	}
	return rules::TurnsBeforeReactingAgain(TurnsFactsOf(villager, object, type, distance, true));
}

uint32_t InspectCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (!ObjectThere(villager).has_value())
	{
		return 1;
	}
	// a look from 20 metres: the least turns first, then the draw
	auto& inspection = InspectionOf(villager);
	inspection.distance = rules::k_InspectStartDistance;
	const auto least = game_clock::TicksForSeconds(rules::k_InspectMinSeconds);
	const auto draw = villager::GameRand(TurnsForSeconds(rules::k_InspectFirstRandomSeconds));
	inspection.turnsUntilApproach = rules::InspectTurns(least, draw);
	villager_reactions::SetTopState(villager, VillagerStates::PerformInspectCreatureReaction);
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t PerformInspectCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	auto& inspection = InspectionOf(villager);
	inspection.turnsUntilApproach -= 1;
	switch (rules::PerformInspectStep(inspection.turnsUntilApproach, inspection.distance))
	{
	case rules::InspectStep::Stop:
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	case rules::InspectStep::Wait:
		break;
	case rules::InspectStep::Approach:
		villager_reactions::SetTopState(villager, VillagerStates::ApproachCreatureReaction);
		break;
	case rules::InspectStep::Look:
		villager::LookAtPos(villager, tq::PosOf(*object), rules::k_InspectLookMode);
		break;
	}
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t ApproachCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	auto& inspection = InspectionOf(villager);
	const auto here = tq::PosOf(villager);
	const auto there = tq::PosOf(*object);
	const float distance = tq::GetDistanceInMetres(here, there);
	if (rules::ApproachFurther(distance, inspection.distance))
	{
		// to `distance` metres from it, along the angle to it
		const auto step = rules::ApproachStep(gutils::GetAngleFromXZ(here, there), distance, inspection.distance);
		const map_coords::MapCoords goal {.x = here.x + step.x, .z = here.y + step.y};
		SetupMoveToCreatureReaction(villager, goal, VillagerStates::ApproachCreatureReaction);
		UpdateReactiveStateFromAttitudeToCreature(villager);
		return 1;
	}
	// near enough: another look, 8 metres closer; the least turns first, then the draw
	const auto least = game_clock::TicksForSeconds(rules::k_InspectMinSeconds);
	const auto draw = villager::GameRand(TurnsForSeconds(rules::k_InspectAgainRandomSeconds));
	inspection.distance = rules::NextInspectDistance(inspection.distance);
	inspection.turnsUntilApproach = rules::InspectTurns(least, draw);
	villager_reactions::SetTopState(villager, VillagerStates::PerformInspectCreatureReaction);
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t InitialiseRespectCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	// to the creature's edge nearest to it
	const auto goal = rules::RespectWalkGoal(object::GetNearestPosOfObject(villager, *object));
	SetupMoveToCreatureReaction(villager, goal, VillagerStates::TurnToFaceCreatureReaction);
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t PerformRespectCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	if (rules::FacingAway(gutils::GetAngleFromXZ(tq::PosOf(villager), tq::PosOf(*object)), villager::GetGameAngle(villager)))
	{
		villager_reactions::SetTopState(villager, VillagerStates::TurnToFaceCreatureReaction);
		return 1;
	}
	// four seconds of facing it
	action.turnsUntilStateChange = static_cast<uint16_t>(action.turnsUntilStateChange + 1);
	if (static_cast<int16_t>(action.turnsUntilStateChange) > game_clock::TicksForSeconds(rules::k_RespectSeconds))
	{
		villager_reactions::SetTopState(villager, VillagerStates::FinishRespectCreatureReaction);
	}
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t FinishRespectCreatureReaction(LivingAction& action)
{
	villager_reactions::StopReactingAndSetState(Entities().ToEntity(action));
	return 1;
}

uint32_t FleeingFromCreatureReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	const auto& info = InfoOf(registry.Get<const components::LivingReaction>(villager).type);
	const float flee = rules::FleeDistance(info.maxReactionDistance);
	const auto here = tq::PosOf(villager);
	const auto there = tq::PosOf(*object);
	if (rules::FleeGivesUp(flee, tq::GetDistanceInMetres(here, there)))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// the run's length straight away from it; a point off the map: it stays
	const auto goal = effects::flee::FleeingPositionFromStill(tq::ToMetres(here), tq::ToMetres(there), flee);
	if (!map_coords::InBounds(map_coords::FromMetres(goal)))
	{
		UpdateReactiveStateFromAttitudeToCreature(villager);
		return 1;
	}
	const auto fleeing = static_cast<int32_t>(villager::InfoOf(villager).speedGroup.speedFleeing);
	if (IsCreature(*object) && ActsOnSomething(*object))
	{
		// one draw in ten runs, at half its fleeing speed
		if (villager::GameRand(rules::k_FleeChanceRange) != 0)
		{
			return 1;
		}
		SetVillagerSpeed(villager, rules::HalfSpeed(fleeing), true);
		SetupMoveToCreatureReaction(villager, goal, VillagerStates::FleeingFromCreatureReaction);
		return 1;
	}
	SetVillagerSpeed(villager, fleeing, true);
	return 1;
}

uint32_t TurnToFaceCreatureReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = ObjectThere(villager);
	if (!object.has_value())
	{
		return 1;
	}
	if (villager::LookAtPos(villager, tq::PosOf(*object), rules::k_RespectLookMode) == 1)
	{
		action.turnsUntilStateChange = 0;
		villager_reactions::SetTopState(villager, VillagerStates::PerformRespectCreatureReaction);
	}
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

uint32_t MoveTowardsCreatureReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	if (!ObjectThere(villager).has_value())
	{
		return 1;
	}
	living_turn::MoveToStep(villager);
	if (living_turn::WallHugMoveToResult(villager) == 0xA)
	{
		const auto* wallHug = registry.TryGet<const components::WallHug>(villager);
		const auto goal = wallHug != nullptr ? tq::ToMapCoords(wallHug->goal) : tq::PosOf(villager);
		if (rules::Arrived(tq::GetDistanceInMetres(tq::PosOf(villager), goal)))
		{
			// the arrival is taken: the next walk or the state the walk was for starts afresh, as the walk's own state's
			registry.Remove<components::MoveStateFinalStepTag, components::MoveStateArrivedTag>(villager);
			villager::SetTopStateToFinal(villager);
		}
	}
	UpdateReactiveStateFromAttitudeToCreature(villager);
	return 1;
}

bool MoveTowardsCreatureValidate(LivingAction& action)
{
	// the wall hug's own validate does nothing
	return villager_reactions::ReactionValidate(action);
}

void SetupMoveToCreatureReaction(entt::entity villager, glm::vec2 goal, VillagerStates state)
{
	// the walk for `state` (its result is not looked at), then 167 written straight over the walk's state: the turns in
	// the state start again
	villager::SetupMoveToWithHug(villager, goal, state);
	if (auto* action = Entities().TryGet<LivingAction>(villager); action != nullptr)
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) =
		    static_cast<uint8_t>(VillagerStates::MoveTowardsCreatureReaction);
		action->turnsSinceStateChange = 0;
	}
}

void SetupMoveToCreatureReaction(entt::entity villager, const map_coords::MapCoords& goal, VillagerStates state)
{
	// (approximate) the walk's goal is kept in metres, as every MapCoords walk's is
	SetupMoveToCreatureReaction(villager, glm::vec2(map_coords::ToMetres(goal.x), map_coords::ToMetres(goal.z)), state);
}

void UpdateReactiveStateFromAttitudeToCreature(entt::entity villager)
{
	if (villager_worship::CheckNeededForWorship(villager))
	{
		return;
	}
	const auto creature = villager_reactions::SlotObject(villager);
	if (!IsCreature(creature))
	{
		return;
	}
	// a creature in a fight's arena: it stops reacting. (missing) the original then looks for the arena's own reaction
	// to watch the fight, which openblack does not make yet: there is none to take up
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().ArenaOf(creature).has_value())
	{
		villager_reactions::StopReactingAndSetState(villager);
		return;
	}
	const auto town = villager::GetTown(villager);
	if (town == entt::null)
	{
		return;
	}
	switch (rules::StepFromAttitude(attitude::AttitudeOf(town, creature), villager::GetFinalState(villager)))
	{
	case rules::AttitudeStep::Keep:
		break;
	case rules::AttitudeStep::Stop:
		villager_reactions::StopReactingAndSetState(villager);
		break;
	case rules::AttitudeStep::Flee:
		villager_reactions::SetTopState(villager, VillagerStates::FleeingFromCreatureReaction);
		break;
	case rules::AttitudeStep::Respect:
		villager_reactions::SetTopState(villager, VillagerStates::InitialiseRespectCreatureReaction);
		break;
	}
}

} // namespace openblack::ecs::villager_creature
