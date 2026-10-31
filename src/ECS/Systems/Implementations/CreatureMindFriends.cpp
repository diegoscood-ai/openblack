/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The joint actions of two creatures: the asker asks the other creature to be its partner, gives it orders, and makes
// its mind stand still or go free; the joint action stops for both when either creature stops what it does.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureSize.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureReactions.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreaturePose.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownQueries.h"
#include "Locator.h"
#include "ObjectMeasures.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
namespace land = openblack::ecs::creature_exploration_land;

namespace
{
/// The action of the plan that a partner carries out the orders of the asker in
constexpr std::string_view k_ObeyCreatureAction = "ObeyCreature";
/// The actions of dying and of lying dead for good: a creature that does either refuses. Dying for good as a script
/// orders is not one of them: such a creature lies as a dead one does, but it is still asked
constexpr std::string_view k_DieAction = "Die";
constexpr std::string_view k_DeadForeverAction = "DeadForever";
/// The priority of the plan of a partner that obeys the asker, in the planner's units
constexpr float k_ObeyPriority = 1.0f * creature_planner::k_PriorityScale;

/// The agenda of a creature, and whether it has steps left
bool AgendaRunning(const CreatureMindState& mind)
{
	return mind.idle.step < mind.idle.agenda.size();
}

void Note(CreaturePartner& link, const CreatureMindState& mind)
{
	link.agenda = mind.idle.serial;
	link.agendaRunning = AgendaRunning(mind);
}

/// The size a creature is shown at, which its height goes by
float ShownSize(const ecs::Registry& registry, entt::entity creature)
{
	return creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature));
}
} // namespace

bool CreatureMindSystem::RequestPartner(entt::entity asker, entt::entity asked, creature_friendship::RequestKind kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto* tables = GetTables();
	auto* askedMind = registry.Valid(asked) ? registry.TryGet<CreatureMindState>(asked) : nullptr;
	auto* askerMind = registry.Valid(asker) ? registry.TryGet<CreatureMindState>(asker) : nullptr;
	const bool bodies =
	    askedMind != nullptr && askerMind != nullptr && lookup.AllOf<Creature>(asked) && lookup.AllOf<Creature>(asker);
	if (tables == nullptr || !bodies || !askedMind->desires.has_value())
	{
		return false;
	}
	// The current plan's action: a creature that dies or lies dead for good refuses
	const auto action =
	    askedMind->planner.current.has_value() ? std::optional(askedMind->planner.current->action) : std::nullopt;
	const bool dead = action.has_value() && *action < tables->actions.size() &&
	                  (tables->actions[*action].name == k_DieAction || tables->actions[*action].name == k_DeadForeverAction);
	const auto* askedLink = lookup.TryGet<const CreaturePartner>(asked);
	const auto availability = creature_friendship::AvailabilityOf({
	    .dead = dead,
	    .scriptControlled = ecs::script_held::IsControlledByScript(asked),
	    .askerScriptControlled = ecs::script_held::IsControlledByScript(asker),
	    .partner = askedLink != nullptr ? std::optional(entt::to_integral(askedLink->partner)) : std::nullopt,
	    .asker = entt::to_integral(asker),
	});
	// The asked creature weighs its desires about the asker, where the asker is now
	const auto* at = lookup.TryGet<const Transform>(asked);
	const auto* askerAt = lookup.TryGet<const Transform>(asker);
	const auto worthAbout = [&](Desire desire) {
		if (at == nullptr || askerAt == nullptr || !askedMind->learnt.has_value())
		{
			return 0.0f;
		}
		const auto d = static_cast<size_t>(desire);
		const auto usefulness =
		    mind_detail::Usefulness(*askedMind->learnt, TreeKind::Goal, desire, mind_detail::BeliefOf(lookup, asker, asked));
		const auto nearness = creature_planner::DistancePriority(
		    false, gutils::GetDistanceInMetres(askerAt->position, at->position), tables->rules.at(d).distanceWeight);
		return creature_reaction_rules::WorthAbout((*askedMind->desires)[desire].value, usefulness, nearness);
	};
	const auto answer = creature_friendship::AnswerOf({
	    .kind = kind,
	    .availability = availability,
	    .alwaysAgree = lookup.Context().partnersAlwaysAgree,
	    .heightRatio = ShownSize(lookup, asked) / std::max(ShownSize(lookup, asker), std::numeric_limits<float>::min()),
	    .worthAbout = worthAbout,
	});
	if (!answer.agrees)
	{
		// The explanation the asked creature gives its player waits for the creature's help messages
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops because creature {} refused its request {}",
		                    entt::to_integral(asker), entt::to_integral(asked), static_cast<int>(kind));
		return false;
	}
	auto& askerLink = registry.AssignOrReplaceState<CreaturePartner>(asker, CreaturePartner {.partner = asked});
	Note(askerLink, *askerMind);
	auto& otherLink = registry.AssignOrReplaceState<CreaturePartner>(asked, CreaturePartner {.partner = asker});
	Note(otherLink, *askedMind);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is the partner of creature {}", entt::to_integral(asked),
	                    entt::to_integral(asker));
	return true;
}

void CreatureMindSystem::TakeOrder(entt::entity partner, entt::entity asker, const creature_mind::Step& ordered)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(partner) ? registry.TryGet<CreatureMindState>(partner) : nullptr;
	auto* link = registry.TryGet<CreaturePartner>(partner);
	const auto* tables = GetTables();
	if (mind == nullptr || link == nullptr || link->partner != asker || tables == nullptr || !mind->desires.has_value())
	{
		return;
	}
	// It stops what it does, without the stop of the joint action: it does the order, then waits for ever, watching the
	// asker
	creature_reactions::StopReacting(partner);
	const auto self = entt::to_integral(asker);
	std::vector agenda {
	    ordered,
	    creature_mind::Step {.kind = creature_mind::Step::Kind::Wait,
	                         .seconds = std::numeric_limits<float>::infinity(),
	                         .gaze = creature_mind::Gaze {.object = self}},
	};
	if (!Replan(partner, creature_mind::Activity::Told, std::move(agenda)))
	{
		return;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_ObeyCreatureAction);
	if (action.has_value())
	{
		mind->planner.current = creature_planner::Plan {
		    .desire = Desire::ObeyCreature,
		    .action = *action,
		    .object = self,
		    .priority = k_ObeyPriority,
		    .activityObject = self,
		};
		mind->planActive = true;
		mind->planSerial = mind->idle.serial;
		mind->agendaSeen = mind->idle.serial;
		mind->satisfiedByEffect = false;
		mind->completedByStep = false;
		// It is remembered for the player's feedback as what it did
		if (mind->learnt.has_value())
		{
			const auto& info = tables->actions[*action];
			creature_learning::Remember(mind->learnt->contexts,
			                            {
			                                .action = *action,
			                                .desire = Desire::ObeyCreature,
			                                .object = self,
			                                .belief = mind_detail::BeliefOf(std::as_const(registry), asker, partner),
			                                .about = self,
			                                .aboutBelief = mind_detail::BeliefOf(std::as_const(registry), asker, partner),
			                                .learnable = info.learnable,
			                                .windowSeconds = info.learningWindowSeconds,
			                            });
		}
	}
	// The new agenda is the joint action's own, so the joint action goes on
	Note(*link, *mind);
}

void CreatureMindSystem::SetDemobilised(entt::entity partner, bool demobilised)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(partner))
	{
		return;
	}
	if (demobilised)
	{
		registry.AssignOrReplaceState<CreatureDemobilised>(partner);
	}
	else
	{
		registry.RemoveState<CreatureDemobilised>(partner);
	}
}

void CreatureMindSystem::EndJointAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* link = std::as_const(registry).TryGet<const CreaturePartner>(creature);
	if (link == nullptr)
	{
		return;
	}
	const auto partner = link->partner;
	registry.RemoveState<CreaturePartner>(creature);
	registry.RemoveState<CreatureDemobilised>(creature);
	const auto* other = registry.Valid(partner) ? std::as_const(registry).TryGet<const CreaturePartner>(partner) : nullptr;
	if (other == nullptr || other->partner != creature)
	{
		return;
	}
	const auto partnerAgenda = other->agenda;
	registry.RemoveState<CreaturePartner>(partner);
	registry.RemoveState<CreatureDemobilised>(partner);
	// The partner stops what it does for the joint action as a failure, and decides afresh. An agenda that it took up
	// of its own accord goes on.
	auto* mind = registry.TryGet<CreatureMindState>(partner);
	if (mind == nullptr || mind->idle.serial != partnerAgenda)
	{
		return;
	}
	AbandonAction(partner);
	if (AgendaRunning(*mind))
	{
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops because its partner {} told it to", entt::to_integral(partner),
	                    entt::to_integral(creature));
}

void CreatureMindSystem::KeepJointActions()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> ended;
	lookup.Each<const CreaturePartner>([&](entt::entity creature, const CreaturePartner& link) {
		const auto* mind = lookup.TryGet<const CreatureMindState>(creature);
		// The joint action stops when the creature took up another agenda, or its agenda came to an end
		if (mind == nullptr || !lookup.Valid(link.partner) || mind->idle.serial != link.agenda ||
		    (link.agendaRunning && !AgendaRunning(*mind)))
		{
			ended.push_back(creature);
		}
	});
	for (const auto creature : ended)
	{
		EndJointAction(creature);
	}
}

void CreatureMindSystem::SituateFriends(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (mind == nullptr || transform == nullptr || !mind->desires.has_value())
	{
		return;
	}
	const auto& desires = *mind->desires;
	situation.waterDesire = desires.desires.at(static_cast<size_t>(Desire::Water)).value;
	situation.pooDesire = desires.desires.at(static_cast<size_t>(Desire::Poo)).value;
	situation.tiredness = desires.desires.at(static_cast<size_t>(Desire::Tiredness)).value;
	const auto sadDesire = creature_desires::DesireOfSource(desires, creature_desires::sources::k_Sadness);
	situation.sadness = sadDesire.has_value()
	                        ? creature_desires::SourceValue(desires.desires.at(static_cast<size_t>(*sadDesire)),
	                                                        creature_desires::sources::k_Sadness)
	                              .value_or(0.0f)
	                        : 0.0f;
	// The nearest town at any distance, when the creature believes something about it. It wants to attack it when the
	// town is more useful to its anger than to its compassion.
	const auto town = ecs::map_cells::GetNearestTown(ecs::object::MapCoordsOf(creature), std::numeric_limits<float>::max());
	if (town == entt::null || mind->beliefs.Find(town) == nullptr || !mind->learnt.has_value())
	{
		return;
	}
	const auto belief = mind_detail::BeliefOf(registry, town, creature);
	situation.attackNearestTown = mind_detail::Usefulness(*mind->learnt, TreeKind::Goal, Desire::Compassion, belief) <
	                              mind_detail::Usefulness(*mind->learnt, TreeKind::Goal, Desire::Anger, belief);
}

void CreatureMindSystem::SituateFriend(entt::entity creature, const creature_plan_actions::Executor& executor,
                                       entt::entity friendly, creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	switch (executor.build)
	{
	case Build::BeHappyWithFriend:
	case Build::PooWithFriend:
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	case Build::DrinkWithFriend:
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
		break;
	default:
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* body = registry.Valid(friendly) ? registry.TryGet<const Creature>(friendly) : nullptr;
	const auto* at = body != nullptr ? registry.TryGet<const Transform>(friendly) : nullptr;
	const auto* self = registry.TryGet<const Transform>(creature);
	if (at == nullptr || self == nullptr)
	{
		return;
	}
	// What lets it start, read again for an action that no pass over the desires came before, as a script's
	SituateFriends(creature, situation);
	// Where it stands and how tall it is, and the same of the friend
	situation.self = entt::to_integral(creature);
	situation.standing = self->position;
	situation.height = object_measures::Height(registry, creature);
	situation.friendly = creature_plan_actions::Situation::Friend {
	    .point = glm::vec2(at->position.x, at->position.z),
	    .height = creature_size::k_HeightOfSizeOne * ShownSize(registry, friendly),
	};
	const auto selfPoint = glm::vec2(self->position.x, self->position.z);
	switch (executor.build)
	{
	case Build::SitWithFriend:
		// A clear place about the point halfway between the two, with nothing standing in it
		situation.clearArea =
		    mind_detail::FindClearArea(registry, creature, selfPoint + (situation.friendly->point - selfPoint) * 0.5f,
		                               creature_mind::SitTogetherRadius(situation.height, situation.friendly->height));
		break;
	case Build::SleepWithFriend:
		// A clear place about the creature itself
		situation.clearArea = mind_detail::FindClearArea(
		    registry, creature, selfPoint, creature_mind::SleepTogetherRadius(situation.height, situation.friendly->height));
		break;
	case Build::DrinkWithFriend:
		// The coast nearest the creature, and the sea nearest that coast, by its map of the land
		situation.explorePlace = land::FindPlace(creature, creature_exploration::Kind::Coast, selfPoint, false);
		situation.exploreSea = situation.explorePlace.has_value()
		                           ? land::FindPlace(creature, creature_exploration::Kind::Sea, *situation.explorePlace, false)
		                           : std::nullopt;
		break;
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
	{
		// The nearest town at any distance, with a storage pit, when the creature believes something about the town
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		const auto town = ecs::map_cells::GetNearestTown(ecs::object::MapCoordsOf(creature), std::numeric_limits<float>::max());
		const auto pit = town != entt::null ? ecs::town_queries::GetStoragePit(town) : entt::null;
		const auto* townAt = town != entt::null ? registry.TryGet<const Transform>(town) : nullptr;
		if (mind == nullptr || pit == entt::null || townAt == nullptr || mind->beliefs.Find(town) == nullptr)
		{
			break;
		}
		situation.friendsTown = creature_mind::FriendsTown {
		    .town = entt::to_integral(town),
		    .pit = entt::to_integral(pit),
		    .townPoint = glm::vec2(townAt->position.x, townAt->position.z),
		    .pitRadius = object_measures::TwoDRadius(registry, pit),
		};
		break;
	}
	default:
		break;
	}
}
