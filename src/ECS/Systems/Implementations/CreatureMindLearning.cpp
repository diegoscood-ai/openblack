/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRules.h"
#include "Creature/LocalPlayer.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreaturePose.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

float openblack::ecs::systems::mind_detail::BodyYaw(const ecs::Registry& registry, entt::entity creature, const Transform& at)
{
	const auto* moving = registry.TryGet<const CreatureLocomotion>(creature);
	const float heading = moving != nullptr && moving->started ? moving->heading : ecs::creature_pose::ReadHeading(at.rotation);
	return -heading;
}

namespace
{
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// The action a creature given something from the hand carries out to take it
constexpr std::string_view k_TakeFromHandAction = "TakeObjectFromHand";
/// What a creature does with what it was given, by the game's action table
constexpr std::string_view k_ExamineGivenAction = "ExamineObjectInHand";
constexpr std::string_view k_EatGivenAction = "EatObjectInHand";
constexpr std::string_view k_ThrowGivenAction = "ThrowObjectInHand";

/// Feedback teaches lessons only once the creature has grown up this far; before, it only stirs its feelings
constexpr uint32_t k_MinLessonPhase = 2;
/// A stroke holds back sadness this long
constexpr float k_StrokeSadnessSeconds = 90.0f;
/// The source of the desire to copy the player, and how much a stroke while copying adds to it
constexpr uint32_t k_FollowPlayerSource = 54;
constexpr float k_MimicStrokeBoost = 0.3f;
/// Forgiven for running away, the threshold of its wish to run raises this much
constexpr float k_RunAwayThresholdStep = 0.1f;
/// A thing to use is learnt about at most this strongly for poo
constexpr float k_PooUseFeedback = 0.5f;
/// A creature watches skills, miracles and the player's deeds this far away at most
constexpr float k_ViewDistance = 150.0f;
/// Copying the player moves on a step once a second
constexpr uint32_t k_MimicStepTurns = 10;
/// The land's script that curses the player's creature, and the values it keeps of the creature as it found it: the
/// size as a height, the strength and the alignment
constexpr std::string_view k_CurseScriptName = "CreatureCurse";
constexpr std::string_view k_CurseSizeVariable = "OriginalSizeOfMyCreature";
constexpr std::string_view k_CurseStrengthVariable = "OriginalStrengthOfMyCreature";
constexpr std::string_view k_CurseAlignmentVariable = "OriginalAlignmentOfMyCreature";
/// Things nobody owns, and things of no player
constexpr uint32_t k_Neutral = 1;
constexpr uint32_t k_NoPlayer = 7;

/// The curse's values, read from the script machine: its running tasks and its globals
std::optional<creature_spells::SavedBody> MindCurseValuesToSave(const lhvm::LHVM& vm)
{
	const auto& tasks = vm.GetTasks();
	const auto& variables = vm.GetVariables();
	return mind_detail::CurseValuesToSave(
	    [&tasks](std::string_view script) {
		    return std::ranges::any_of(tasks, [script](const auto& task) { return task.second.name == script; });
	    },
	    [&variables](std::string_view name) -> std::optional<float> {
		    const auto found = std::ranges::find(variables, name, &lhvm::VMVar::name);
		    return found != variables.end() ? std::optional<float>(found->value.floatVal) : std::nullopt;
	    });
}

/// A villager hurt to this share of its life or less can be healed by a creature
constexpr float k_HurtEnoughToHeal = 0.7f;

/// Whether a creature is on fire, which the action that casts water on itself asks; not without the fires
bool Burning(entt::entity creature)
{
	return Locator::fireSystem::has_value() && ecs::fire::IsOnFire(creature);
}

/// The actions that cast a power-up of a miracle, which only a creature grown up enough tries
bool IsPowerUpCast(std::string_view action)
{
	return action.ends_with("PU1") || action.ends_with("PU2");
}

/// While doing these, which it chose with nothing better to do, the creature changes to any plan good enough
bool Interruptible(creature_mind::Activity activity)
{
	using enum creature_mind::Activity;
	return activity == None || activity == BeIdle || activity == Sit || activity == HangAround;
}

/// The action of the game's table an idle activity counts as, for feedback to be credited to
std::string_view ActionForActivity(creature_mind::Activity activity)
{
	using enum creature_mind::Activity;
	switch (activity)
	{
	case Eat:
		return "EatAfterExamining";
	case Examine:
		return "ExamineByPickingUp";
	case PlayWithObject:
		return "PracticeThrow";
	case Hurl:
		return "Hurl";
	case Sleep:
		return "SleepOnTheSpot";
	case Poo:
		return "Poo";
	case Puke:
		return "Puke";
	case Drink:
		return "DrinkFromTheSea";
	case Sit:
		return "SitDown";
	case BeIdle:
		return "BeIdle";
	case HangAround:
		return "HangAroundAtHome";
	case ShowDesire:
		return "CommunicateState";
	default:
		return {};
	}
}

std::optional<glm::vec2> PointOf(const ecs::Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return glm::vec2(transform->position.x, transform->position.z);
}

uint32_t PlayerNumberOf(PlayerNames owner)
{
	return static_cast<uint32_t>(owner);
}

/// What the agenda knows of the camera: where it is, if there is one, and whether the creature has a player
void SeeCamera(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Situation& situation)
{
	if (Locator::camera::has_value())
	{
		situation.eye = Locator::camera::value().GetOrigin();
		situation.camera = glm::vec2(situation.eye.x, situation.eye.z);
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL;
}

float Usefulness(const creature_mind_model::Learnt& learnt, Desire desire, const std::optional<creature_tree::Belief>& belief)
{
	if (!belief.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto& tree = learnt.trees.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(desire));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

/// The creatures with a mind and a place on the land, found without making any storage
std::vector<entt::entity> MindsOf(const ecs::Registry& registry)
{
	std::vector<entt::entity> minds;
	std::as_const(registry).Each<const Creature, const CreatureMindState, const Transform>(
	    [&minds](entt::entity entity, const auto&...) { minds.push_back(entity); });
	return minds;
}

std::string ObjectName(const std::optional<creature_tree::Belief>& belief)
{
	return belief.has_value() ? fmt::format("{}s", creature_tree::BeliefName(belief->type)) : "that";
}

/// The mind of the player's own creature, if the player has one and it can see the point
CreatureMindState* MindSeeing(ecs::Registry& registry, PlayerNames player, const glm::vec3& point)
{
	if (!Locator::leashSystem::has_value())
	{
		return nullptr;
	}
	const auto creature = Locator::leashSystem::value().PlayersCreature(player);
	const auto& lookup = std::as_const(registry);
	if (!creature.has_value() || !lookup.Valid(*creature) || !lookup.AllOf<CreatureMindState, Transform>(*creature))
	{
		return nullptr;
	}
	const auto& at = lookup.Get<Transform>(*creature);
	const auto* animation = lookup.TryGet<const CreatureAnimation>(*creature);
	const auto look = creature_perceived_desires::LookYaw(animation != nullptr ? animation->lookAt : std::nullopt, at.position,
	                                                      BodyYaw(lookup, *creature, at));
	const auto from = map_coords::FromMetres(glm::vec2(at.position.x, at.position.z));
	const auto to = map_coords::FromMetres(glm::vec2(point.x, point.z));
	if (!creature_perceived_desires::CanSeePos(look, from, to))
	{
		return nullptr;
	}
	return &registry.Get<CreatureMindState>(*creature);
}
} // namespace

std::optional<uint32_t> mind_detail::CurrentAction(const CreatureMindState& mind, const creature_mind_tables::Tables* tables)
{
	if (mind.planActive && mind.planner.current.has_value())
	{
		return mind.planner.current->action;
	}
	const auto name = ActionForActivity(mind.idle.activity);
	if (tables == nullptr || name.empty() || mind.idle.step >= mind.idle.agenda.size())
	{
		return std::nullopt;
	}
	return creature_mind_tables::FindAction(*tables, name);
}

std::optional<creature_spells::SavedBody>
mind_detail::CurseValuesToSave(const std::function<bool(std::string_view)>& scriptRuns,
                               const std::function<std::optional<float>(std::string_view)>& global)
{
	if (!scriptRuns(k_CurseScriptName))
	{
		return std::nullopt;
	}
	const auto value = [&global](std::string_view name) { return global(name).value_or(0.0f); };
	return creature_spells::SavedBody {
	    .size = value(k_CurseSizeVariable) / creature_morph::k_HeightAtSizeOne,
	    .strength = value(k_CurseStrengthVariable),
	    .alignment = value(k_CurseAlignmentVariable),
	};
}

namespace
{
namespace town_compassion = creature_town_compassion;

/// The town a creature's compassion is about, and its worth for the distance
struct CompassionGoal
{
	entt::entity town {entt::null};
	float worth {0.0f};
};

/// The town a creature's compassion is about: of the towns and other creatures it knows, the one most worth it for the
/// distance, the first found winning a tie; none when that is a creature, for which compassion has nothing to do
std::optional<CompassionGoal> CompassionTown(const ecs::Registry& registry, entt::entity self, glm::vec2 position,
                                             const creature_mind_model::Learnt& learnt, float distanceWeight)
{
	std::optional<entt::entity> best;
	bool bestIsTown = false;
	float bestScore = 0.0f;
	const auto consider = [&](entt::entity entity, const Transform& at, bool town) {
		if (entity == self)
		{
			return;
		}
		const auto distance = gutils::GetDistanceInMetres(glm::vec2(at.position.x, at.position.z), position);
		const auto score = Usefulness(learnt, Desire::Compassion, BeliefOf(registry, entity, self)) *
		                   creature_planner::DistancePriority(false, distance, distanceWeight);
		if (score > bestScore)
		{
			best = entity;
			bestIsTown = town;
			bestScore = score;
		}
	};
	registry.Each<const Town, const Transform>([&](entt::entity entity, const Town&, const Transform& at) {
		// A town with nothing in it is of no use to compassion
		if (TownHasMembers(registry, entity))
		{
			consider(entity, at, true);
		}
	});
	registry.Each<const Creature, const Transform>(
	    [&](entt::entity entity, const Creature&, const Transform& at) { consider(entity, at, false); });
	if (!bestIsTown || !best.has_value())
	{
		return std::nullopt;
	}
	return CompassionGoal {.town = *best, .worth = bestScore};
}

/// How much a town feels each of its desires now, and its desires in its own order, most felt first
struct TownFeelings
{
	std::array<float, town_compassion::k_TownDesireCount> felt {};
	std::vector<uint32_t> order;
};

TownFeelings TownFeelingsOf(const ecs::Registry& registry, entt::entity town)
{
	TownFeelings feelings;
	const auto* data = registry.TryGet<const Town>(town);
	if (data == nullptr)
	{
		return feelings;
	}
	for (size_t d = 0; d < feelings.felt.size(); ++d)
	{
		feelings.felt.at(d) = ecs::town_desire::GetRawDesire(data->desire, d);
	}
	for (const auto& sorted : data->desire.sortedRaw)
	{
		feelings.order.push_back(sorted.index);
	}
	return feelings;
}

/// Whether a thing is part of a town, as the creature knows it: the town's buildings and its people
bool PartOfTown(const ecs::Registry& registry, entt::entity entity, entt::entity town)
{
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return villager->town == town;
	}
	const auto* abode = registry.TryGet<const Abode>(entity);
	const auto* data = registry.TryGet<const Town>(town);
	return abode != nullptr && data != nullptr && abode->townId == data->id;
}

/// Whether compassion is what a creature wants most: no desire it feels is stronger
bool CompassionMostWanted(const creature_desires::Desires& desires)
{
	const auto compassion = desires[Desire::Compassion].value;
	return std::ranges::none_of(desires.desires,
	                            [compassion](const auto& state) { return state.activated && state.value > compassion; });
}

/// The action that heals, which hurt people in a town may make a creature think of first
constexpr std::string_view k_HealAction = "CastHealSpell";

/// The actions a creature's compassion for a town may take now: those for the town desire it helps with, which it may
/// move on from first, or healing first for a town with hurt people
std::vector<uint32_t> TownCompassionActions(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind,
                                            entt::entity town, const creature_mind_tables::Tables& tables,
                                            const std::function<uint32_t(uint32_t)>& random)
{
	const auto feelings = TownFeelingsOf(registry, town);
	const auto desires = town_compassion::DesiresToHelp(feelings.order, feelings.felt, tables.townActions);
	auto& state = mind.townCompassion;
	const auto before = state.desire;
	const bool beingCompassionate =
	    mind.planActive && mind.planner.current.has_value() && mind.planner.current->desire == Desire::Compassion;
	if (mind.desires.has_value() &&
	    town_compassion::MovesOnWhilePlanning(state, CompassionMostWanted(*mind.desires), beingCompassionate, mind.turn))
	{
		town_compassion::MoveOnWhilePlanning(state, desires, feelings.felt, mind.turn);
	}
	std::optional<uint32_t> heal;
	const auto* data = registry.TryGet<const Town>(town);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (const auto healAction = creature_mind_tables::FindAction(tables, k_HealAction);
	    data != nullptr && body != nullptr && healAction.has_value())
	{
		// a town desire shown on the leash would hold off the healing; the leash shows none yet
		const auto sightings = MiracleSightings(mind, tables, body->species, MagicType::Heal);
		if (sightings.has_value() &&
		    town_compassion::HealFirst(data->injured, false, sightings->first, sightings->second, random))
		{
			heal = healAction;
		}
	}
	town_compassion::Settle(state, desires, feelings.felt);
	if (state.desire != before)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} helps town {} with its desire {} ({} it wants)",
		                    entt::to_integral(creature), entt::to_integral(town),
		                    state.desire.has_value() ? static_cast<int>(*state.desire) : -1, desires.size());
	}
	return town_compassion::Actions(state, tables.townActions, heal);
}

/// Having finished helping a town, the creature may move on to another of its desires
void FinishedHelpingTown(const ecs::Registry& registry, CreatureMindState& mind, entt::entity town,
                         const creature_mind_tables::Tables& tables, const std::function<uint32_t(uint32_t)>& random)
{
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto feelings = TownFeelingsOf(registry, town);
	const auto desires = town_compassion::DesiresToHelp(feelings.order, feelings.felt, tables.townActions);
	town_compassion::FinishedHelping(mind.townCompassion, desires, feelings.felt, mind.turn, random);
}

/// The species' floor, which no desire is set below
float DesireFloorOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& species = Locator::infoConstants::value().creature;
	const auto row = static_cast<size_t>(creature::InfoRow(body->species));
	return row < species.size() ? species.at(row).desireFloor : 0.0f;
}

/// A plan carried out to its end: the body pays for its action, unless a step's effect charged it already, then its
/// desires take what finishing it does to them
void Complete(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind, const creature_planner::Plan& plan,
              const creature_mind_tables::Tables& tables)
{
	const auto& action = tables.actions[plan.action];
	if (!mind.satisfiedByEffect && Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().FinishAction(creature, action.name);
	}
	const auto actionDesire =
	    action.desire < creature_desires::k_DesireCount ? std::optional(static_cast<Desire>(action.desire)) : std::nullopt;
	const auto result = creature_learning::CompletePlan(*mind.desires,
	                                                    {
	                                                        .planDesire = plan.desire,
	                                                        .actionDesire = actionDesire,
	                                                        .multiplier = action.desireMultiplier,
	                                                        .alwaysApplies = action.alwaysApplies,
	                                                        .forced = plan.forced,
	                                                        .floor = DesireFloorOf(registry, creature),
	                                                    },
	                                                    k_TurnsPerSecond);
	if (result.restoreLife)
	{
		ecs::life::SetLife(creature, creature_learning::LifeAfterRestoringHealth(ecs::life::LifeOf(creature)));
	}
}
} // namespace

std::vector<entt::entity> mind_detail::TownMembers(const ecs::Registry& registry, entt::entity town)
{
	std::vector<entt::entity> members;
	const auto* data = registry.TryGet<const Town>(town);
	if (data == nullptr)
	{
		return members;
	}
	const auto id = data->id;
	registry.Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (villager.town == town && ecs::IsAvailable(entity))
		{
			members.push_back(entity);
		}
	});
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == id && ecs::IsAvailable(entity))
		{
			members.push_back(entity);
		}
	});
	registry.Each<const Field>([&](entt::entity entity, const Field& field) {
		if (field.town >= 0 && static_cast<uint32_t>(field.town) == id)
		{
			members.push_back(entity);
		}
	});
	registry.Each<const VillageTotem>([&](entt::entity entity, const VillageTotem& totem) {
		const auto* centre = totem.townCentre != entt::null ? registry.TryGet<const Abode>(totem.townCentre) : nullptr;
		if (centre != nullptr && centre->townId == id)
		{
			members.push_back(entity);
		}
	});
	return members;
}

bool mind_detail::TownHasMembers(const ecs::Registry& registry, entt::entity town)
{
	return !TownMembers(registry, town).empty();
}

/// What a creature knows of a thing: its kind and attributes, as far as the game's world here tells them
std::optional<creature_tree::Belief> mind_detail::BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                           entt::entity self)
{
	using creature_tree::Attribute;
	namespace types = creature_tree::belief_types;
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* me = registry.TryGet<const Creature>(self);
	creature_tree::Belief belief;
	const auto common = [&belief](uint32_t type, uint32_t allegiance, uint32_t origin, uint32_t animate, uint32_t player) {
		belief.type = type;
		belief.Set(Attribute::Allegiance, allegiance);
		belief.Set(Attribute::Origin, origin);
		belief.Set(Attribute::Animate, animate);
		belief.Set(Attribute::PlayerNumber, player);
		belief.Set(Attribute::HarderThanMe, 0);
		belief.Set(Attribute::CreatureType, 0);
		belief.Set(Attribute::Type, type);
	};
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		common(types::k_Villager, k_Neutral, 0, 1, k_NoPlayer);
		belief.Set(Attribute::Sex, static_cast<uint32_t>(villager->sex));
		belief.Set(Attribute::VillagerJob, static_cast<uint32_t>(villager->number));
		belief.Set(Attribute::Life, villager->life > 0.0f ? 1 : 0);
		belief.Set(Attribute::OnFire, OnFireValue(ecs::fire::IsOnFire(entity)));
		belief.Set(Attribute::Tribe, static_cast<uint32_t>(villager->tribe));
		return belief;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		// An animal some player owns is theirs, as their creature is; the rest belong to nobody
		const bool owned = animal->player >= 0;
		const auto player = owned ? static_cast<uint32_t>(animal->player) : k_NoPlayer;
		const bool mine = owned && me != nullptr && player == PlayerNumberOf(me->owner);
		common(types::k_Animal, owned ? (mine ? 0 : 2) : k_Neutral, 0, 1, player);
		belief.Set(Attribute::Life, ecs::life::LifeOf(entity) > 0.0f ? 1 : 0);
		return belief;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const bool mine = me != nullptr && me->owner == creature->owner;
		common(types::k_Creature, mine ? 0 : 2, 0, 1, PlayerNumberOf(creature->owner));
		belief.Set(Attribute::HarderThanMe,
		           me != nullptr && creature->strength * creature->size > me->strength * me->size ? 1 : 0);
		belief.Set(Attribute::CreatureType, static_cast<uint32_t>(creature::InfoRow(creature->species)));
		belief.Set(Attribute::Height, static_cast<uint32_t>(std::lround(creature->size * 3.0f)));
		belief.Set(Attribute::SpellKnowledge, 0);
		belief.Set(Attribute::Carrying, registry.AllOf<CreatureHeldObject>(entity) ? 1 : 0);
		uint32_t dominant = 0;
		if (const auto* mind = registry.TryGet<const CreatureMindState>(entity); mind != nullptr && mind->desires.has_value())
		{
			float strongest = 0.0f;
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& state = mind->desires->desires.at(d);
				if (state.activated && state.value > strongest)
				{
					strongest = state.value;
					dominant = static_cast<uint32_t>(d);
				}
			}
		}
		belief.Set(Attribute::DominantDesire, dominant);
		return belief;
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		common(types::k_Abode, k_Neutral, 1, 0, k_NoPlayer);
		belief.Set(Attribute::AbodeType, static_cast<uint32_t>(abode->type));
		belief.Set(Attribute::Life, 1);
		belief.Set(Attribute::OnFire, OnFireValue(ecs::fire::IsOnFire(entity)));
		belief.Set(Attribute::AbodeBeingBuilt, 0);
		return belief;
	}
	if (registry.AllOf<Tree>(entity))
	{
		common(types::k_Tree, k_Neutral, 0, 0, k_NoPlayer);
		return belief;
	}
	if (const auto* town = registry.TryGet<const Town>(entity))
	{
		// Its own player's town, or another's; made, not living
		const bool mine = me != nullptr && me->owner == town->owner;
		common(types::k_Town, mine ? 0 : 1, 1, 0, std::min(PlayerNumberOf(town->owner), k_NoPlayer));
		const float faith = me != nullptr ? town->belief.belief.at(static_cast<size_t>(me->owner)) : 0.0f;
		belief.Set(Attribute::TownReligiousBelief, creature_town_compassion::ReligiousBelief(faith));
		belief.Set(Attribute::TownNeedsMost,
		           creature_town_compassion::NeedsMost(ecs::town_desire::GetMostDesired(town->desire).value_or(-1)));
		belief.Set(Attribute::TownSize, creature_town_compassion::TownSize(town->stats.adults + town->stats.children));
		const auto* tribe = registry.TryGet<const Tribe>(entity);
		belief.Set(Attribute::Tribe,
		           tribe != nullptr ? std::min(static_cast<uint32_t>(*tribe), static_cast<uint32_t>(Tribe::TIBETAN)) : 0);
		return belief;
	}
	if (registry.AllOf<Temple>(entity))
	{
		common(types::k_Citadel, 0, 1, 0, me != nullptr ? PlayerNumberOf(me->owner) : 0);
		return belief;
	}
	if (registry.AllOf<Feature>(entity))
	{
		common(types::k_Feature, k_Neutral, 0, 0, k_NoPlayer);
		return belief;
	}
	if (registry.AnyOf<MobileObject, Pot>(entity))
	{
		common(types::k_Other, k_Neutral, 1, 0, k_NoPlayer);
		return belief;
	}
	return std::nullopt;
}

float mind_detail::FoodUsefulness(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity self,
                                  entt::entity food)
{
	if (!mind.learnt.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto belief = BeliefOf(registry, food, self);
	if (!belief.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto& tree = mind.learnt->trees.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(Desire::Hunger));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

bool mind_detail::CanBeEaten(const ecs::Registry& registry, entt::entity self, entt::entity thing)
{
	// A mobile static unless it is a toy (a toy ball, bowling ball, cuddly toy, dice or skittle, by its mesh)
	if (const auto* still = registry.TryGet<const MobileStatic>(thing); still != nullptr)
	{
		const auto kind = static_cast<size_t>(still->type);
		const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
		const auto mesh =
		    info != nullptr && kind < info->mobileStatic.size() ? info->mobileStatic.at(kind).meshId : MeshId::Dummy;
		const bool toy = mesh >= MeshId::ObjectToyBall && mesh <= MeshId::ObjectToySkittle;
		return creature_eating::CanBeEaten({.as = creature_eating::Eaten::AMobileStatic, .isToy = toy});
	}
	// A creature never; any other object (a villager, an animal, a mobile object, a pot) when it can be picked up
	if (registry.AllOf<Creature>(thing))
	{
		return creature_eating::CanBeEaten({.as = creature_eating::Eaten::Never});
	}
	const auto* hands =
	    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
	return creature_eating::CanBeEaten({
	    .as = creature_eating::Eaten::AnObject,
	    .mayPickUp = hands != nullptr && hands->CanPickUp(self, thing),
	});
}

bool mind_detail::Accepts(const ecs::Registry& registry, Target target, entt::entity entity, entt::entity self)
{
	if (!ecs::IsAvailable(entity) || entity == self)
	{
		return false;
	}
	const auto* hands =
	    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
	const bool heldByOther = [&] {
		const auto* held = registry.TryGet<const HeldByCreature>(entity);
		return held != nullptr && held->creature != self;
	}();
	switch (target)
	{
	case Target::None:
		return false;
	case Target::Food:
		// whatever it can eat, food or not: it finds out what is not food by eating it
		return !heldByOther && CanBeEaten(registry, self, entity);
	case Target::LiveFood:
		// the living: villagers and animals
		return !heldByOther && registry.AnyOf<Villager, Animal>(entity) && CanBeEaten(registry, self, entity);
	case Target::FoodPile:
	{
		// a pile of magic food, unless its nearest town has a storage pit and it lies within 1.1 times the pit's radius
		const auto* pot = registry.TryGet<const Pot>(entity);
		if (heldByOther || pot == nullptr || pot->type != PotInfo::MagicFood)
		{
			return false;
		}
		const auto coords = ecs::object::MapCoordsOf(entity);
		const auto town = ecs::map_cells::GetNearestTown(coords, std::numeric_limits<float>::max());
		const auto pit = town != entt::null ? ecs::town_queries::GetStoragePit(town) : entt::null;
		if (pit == entt::null)
		{
			return creature_eating::IsFoodPileOutsideStoragePit(true, std::nullopt, 0.0f);
		}
		return creature_eating::IsFoodPileOutsideStoragePit(true, ecs::object::Get2DRadius(pit),
		                                                    gutils::GetDistanceInMetres(ecs::object::MapCoordsOf(pit), coords));
	}
	case Target::Pickable:
		return !heldByOther && registry.AllOf<MobileObject>(entity) && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::Destroyable:
		return hands != nullptr && hands->CanDestroy(entity);
	case Target::Tree:
		return registry.AllOf<Tree>(entity) && hands != nullptr && hands->CanDestroy(entity);
	case Target::Villager:
		return registry.AllOf<Villager>(entity);
	case Target::HurtVillager:
		// Hurt to seven tenths of its life or less, or with a life that is no number
		return registry.AllOf<Villager>(entity) && !(ecs::life::LifeOf(entity) > k_HurtEnoughToHeal);
	case Target::Creature:
		return registry.AllOf<Creature>(entity);
	case Target::Living:
		return registry.AnyOf<Villager, Animal, Creature>(entity);
	case Target::Frightening:
		return registry.AnyOf<Creature, ecs::components::Spell>(entity) ||
		       (registry.AllOf<Animal>(entity) && Locator::animalSystem::has_value() &&
		        Locator::animalSystem::value().IsFrighteningToCreature(entity));
	case Target::Anything:
		return true;
	case Target::FoodStore:
		return registry.AnyOf<StoragePit, WorshipSite>(entity) && ecs::abodes::IsBuilt(entity);
	case Target::WoodStore:
		return registry.AllOf<StoragePit>(entity);
	case Target::WorshipSite:
		return registry.AllOf<WorshipSite>(entity);
	case Target::BuildingSite:
		return ecs::abodes::IsBeingBuilt(entity);
	case Target::Workshop:
		return registry.AllOf<Workshop>(entity);
	case Target::Sprinklable:
		return registry.AllOf<Tree>(entity) || (registry.AllOf<Field>(entity) && ecs::fields::GetFoodValue(entity) == 0.0f);
	case Target::AnyTree:
		return registry.AllOf<Tree>(entity);
	case Target::DryField:
		return registry.AllOf<Field>(entity) && ecs::fields::GetFoodValue(entity) == 0.0f;
	}
	return false;
}

std::vector<mind_detail::Found> mind_detail::Gather(const ecs::Registry& registry, Target target, entt::entity self,
                                                    glm::vec2 from)
{
	std::vector<Found> found;
	if (target == Target::None)
	{
		return found;
	}
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance <= creature_planner::k_MaxGoalDistance && Accepts(registry, target, entity, self))
		{
			found.push_back({entity, point, distance});
		}
	};
	std::as_const(registry).Each<const Villager, const Transform>(
	    [&](entt::entity entity, const Villager&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const Animal, const Transform>(
	    [&](entt::entity entity, const Animal&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const Creature, const Transform>(
	    [&](entt::entity entity, const Creature&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const MobileObject, const Transform>(
	    [&](entt::entity entity, const MobileObject&, const Transform& at) { consider(entity, at); });
	// Pots and piles of food are only ever eaten
	if (target == Target::Food || target == Target::FoodPile)
	{
		std::as_const(registry).Each<const Pot, const Transform>(
		    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || target == Target::Tree || target == Target::Anything ||
	    target == Target::Sprinklable || target == Target::AnyTree)
	{
		std::as_const(registry).Each<const Tree, const Transform>(
		    [&](entt::entity entity, const Tree&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || target == Target::Anything)
	{
		std::as_const(registry).Each<const Abode, const Transform>(
		    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	}
	// Storage pits, workshops and buildings going up are abodes; worship sites take food and may be going up too
	if (target == Target::FoodStore || target == Target::WoodStore || target == Target::BuildingSite ||
	    target == Target::Workshop)
	{
		std::as_const(registry).Each<const Abode, const Transform>(
		    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::FoodStore || target == Target::WorshipSite || target == Target::BuildingSite)
	{
		std::as_const(registry).Each<const WorshipSite, const Transform>(
		    [&](entt::entity entity, const WorshipSite&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Sprinklable || target == Target::DryField)
	{
		std::as_const(registry).Each<const Field, const Transform>(
		    [&](entt::entity entity, const Field&, const Transform& at) { consider(entity, at); });
	}
	// Nearest first; as near, the lower entity first
	std::ranges::sort(found, [](const Found& a, const Found& b) {
		return a.distance != b.distance ? a.distance < b.distance : entt::to_integral(a.entity) < entt::to_integral(b.entity);
	});
	return found;
}

uint32_t CreatureMindSystem::Random(uint32_t range)
{
	return game_random::GameRand(range);
}

float CreatureMindSystem::Chance()
{
	return game_random::GameFloatRand(1.0f);
}

const creature_mind_tables::Tables* CreatureMindSystem::GetTables()
{
	if (!_tables.has_value() && Locator::infoConstants::has_value())
	{
		_tables = creature_mind_tables::Build(Locator::infoConstants::value());
	}
	return _tables.has_value() ? &*_tables : nullptr;
}

std::optional<uint32_t> CreatureMindSystem::CurrentActionOf(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (creature == entt::null || !registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr ? mind_detail::CurrentAction(*mind, GetTables()) : std::nullopt;
}

void CreatureMindSystem::SetUpLearning(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	const auto actionCount = tables != nullptr ? tables->actions.size() : 0;
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	mind.learnt = creature_mind_model::Fresh(*mind.desires, actionCount, std::move(knowledge));
	// A new creature starts with one example of what is good to eat
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(*mind.learnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	if (mind.resourceChecked)
	{
		return;
	}
	mind.resourceChecked = true;
	// A creature made from a mind file takes up what the file holds
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->mind == 0 || !Locator::resources::has_value())
	{
		return;
	}
	const auto& minds = Locator::resources::value().GetCreatureMinds();
	if (minds.Contains(body->mind))
	{
		const auto handle = minds.Handle(body->mind);
		if (handle && handle->Loaded())
		{
			mind.pendingFile = std::make_shared<const creaturemind::MindFileData>(handle->data);
		}
	}
}

void CreatureMindSystem::TakeUpFile(entt::entity creature, CreatureMindState& mind)
{
	const auto file = std::move(mind.pendingFile);
	mind.pendingFile.reset();
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	// No draw: the file's desires replace those drawn as the creature was made, which stand in for what the file does
	// not hold. The decays are put back below
	const auto fresh = *mind.desires;
	const auto* tables = GetTables();
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	auto freshLearnt = creature_mind_model::Fresh(fresh, mind.learnt->opinions.size(), std::move(knowledge));
	// what the file does not hold is as the creature was made, with its starting example
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(freshLearnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	auto loaded = creature_mind_model::FromFile(*file, fresh, freshLearnt);
	// A mind file holds no decay: each desire keeps the one drawn when the creature's desires were made
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		loaded.desires.desires.at(d).decay = mind.desires->desires.at(d).decay;
	}
	if (tables != nullptr)
	{
		creature_mind_model::RebuildTrees(loaded.learnt, tables->attributes);
	}
	Abandon(mind);
	// Its pass over its desires starts empty, and it plans nothing the turn it takes the file up
	mind.planner = {};
	// (the mind's turn is counted on after the file is taken up, this same game turn)
	mind.plannedTurn = mind.turn + 1;
	mind.desires = std::move(loaded.desires);
	mind.learnt = std::move(loaded.learnt);
	mind.developmentPhase = std::min(loaded.developmentPhase, CreatureMindState::k_FullyGrownUp);
	// The file's desires are active as it says, for the stage it gives
	mind.desiresPhase = mind.developmentPhase;
	mind.attitudeToPlayer = loaded.attitudeToPlayer;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} took up a mind file: version {}, stage {}",
	                   entt::to_integral(creature), file->version, mind.developmentPhase);
}

bool CreatureMindSystem::PlayerLooksAt(entt::entity creature)
{
	const auto* at = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (at == nullptr || !Locator::camera::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	return creature_learning::LooksAt(camera.GetOrigin(), camera.GetFocus(), at->position);
}

void CreatureMindSystem::Abandon(CreatureMindState& mind)
{
	// What it learns from stays unfinished: only a plan carried out to its end finishes
	mind.planActive = false;
	mind.planner.current.reset();
	// Its agenda cleared, the pass over its desires starts again, and nothing it does has achieved its aim yet
	if (mind.desires.has_value())
	{
		mind.desires->achieved = false;
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(*mind.desires));
	}
}

void CreatureMindSystem::FollowAgenda(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.learnt.has_value() || !mind.desires.has_value())
	{
		return;
	}
	const auto* tables = GetTables();
	auto& idle = mind.idle;
	if (mind.planActive && (idle.serial != mind.planSerial || idle.step >= idle.agenda.size()))
	{
		// The plan is over. Carried out to its end, it counts towards satisfying the desire it served; cut short by
		// something else (fainting, a fight, a more pressing plan) or given up, it does nothing for it.
		const auto plan = *mind.planner.current;
		const bool carriedOut = idle.serial == mind.planSerial && !idle.gaveUp;
		// A step of its agenda may have completed it already
		if (carriedOut && !mind.completedByStep && tables != nullptr && plan.action < tables->actions.size())
		{
			Complete(Locator::entitiesRegistry::value(), creature, mind, plan, *tables);
		}
		// Having helped a town, its compassion may move on to another of the town's desires
		if (carriedOut && plan.desire == Desire::Compassion && plan.activityObject.has_value() && tables != nullptr)
		{
			FinishedHelpingTown(Locator::entitiesRegistry::value(), mind, static_cast<entt::entity>(*plan.activityObject),
			                    *tables, [](uint32_t range) { return Random(range); });
		}
		if (carriedOut)
		{
			creature_learning::Finished(mind.learnt->contexts, PlayerLooksAt(creature));
		}
		Abandon(mind);
		mind.planner.best.at(static_cast<size_t>(plan.desire)).reset();
	}
	// What the idle mind started is over once its agenda runs out
	if (!mind.planActive && idle.serial == mind.agendaSeen && idle.step >= idle.agenda.size() &&
	    !mind.learnt->contexts.empty() && mind.learnt->contexts.back().running)
	{
		creature_learning::Finished(mind.learnt->contexts, PlayerLooksAt(creature));
	}
	// The trainer judges what it did as soon as it is over
	if (mind.trainer.has_value() && !mind.learnt->contexts.empty())
	{
		const auto& last = mind.learnt->contexts.back();
		if (!last.running && !last.credited.has_value() && last.belief.has_value())
		{
			const bool reward = last.belief->type == *mind.trainer;
			mind.learnt->contexts.back().credited = reward ? 1.0f : -1.0f;
			ReceiveFeedback(creature, reward ? 1.0f : -1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} {} for a {}", entt::to_integral(creature),
			                   reward ? "stroked" : "slapped", creature_tree::BeliefName(last.belief->type));
		}
	}
	if (idle.serial == mind.agendaSeen)
	{
		return;
	}
	mind.agendaSeen = idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	if (mind.planActive || tables == nullptr)
	{
		return;
	}
	// Something the idle mind started counts as its action, for feedback
	const auto action = creature_mind_tables::FindAction(*tables, ActionForActivity(idle.activity));
	if (!action.has_value() || tables->actions[*action].desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	std::optional<uint32_t> object;
	for (const auto& step : idle.agenda)
	{
		if (step.order.object.has_value())
		{
			object = step.order.object;
			break;
		}
		if (step.object.has_value())
		{
			object = step.object;
			break;
		}
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = tables->actions[*action];
	creature_learning::Remember(
	    mind.learnt->contexts,
	    {
	        .action = *action,
	        .desire = static_cast<Desire>(info.desire),
	        .object = object,
	        .belief = object.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*object), creature) : std::nullopt,
	        .learnable = info.learnable,
	        .windowSeconds = info.learningWindowSeconds,
	    });
	mind.learnt->turnsSinceDone.at(*action) = 0;
}

std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
CreatureMindSystem::PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
                               const creature_plan_actions::Situation& situation)
{
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size())
	{
		return std::nullopt;
	}
	const auto* executor = creature_plan_actions::ForcedFor(tables->actions[action].name);
	if (executor == nullptr)
	{
		return std::nullopt;
	}
	glm::vec2 point {0.0f};
	if (object.has_value())
	{
		const auto at = PointOf(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object));
		if (!at.has_value())
		{
			return std::nullopt;
		}
		point = *at;
	}
	auto cast = creature_plan_actions::IsCast(*executor) ? CastInfoFor(creature, action) : std::nullopt;
	if (cast.has_value() && object.has_value())
	{
		cast->atNeedsSign = Locator::entitiesRegistry::value().AllOf<ShowNeedsVisuals>(static_cast<entt::entity>(*object));
	}
	auto local = situation;
	SituateExploration(creature, *executor, local);
	if (executor->build == creature_plan_actions::Build::EatPortion && object.has_value())
	{
		const auto pile = static_cast<entt::entity>(*object);
		local.portionApproach =
		    creature_eating::PileApproach(ecs::object::Get2DRadius(pile), ecs::object::Get2DRadius(creature));
	}
	auto agenda = creature_plan_actions::Agenda(
	    *executor, object, point, local, [](uint32_t range) { return Random(range); }, cast);
	if (!agenda.has_value())
	{
		return std::nullopt;
	}
	return std::pair {executor, std::move(*agenda)};
}

void CreatureMindSystem::CompleteByStep(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (!mind.planActive || mind.completedByStep || !mind.planner.current.has_value() || !mind.desires.has_value() ||
	    tables == nullptr || mind.planner.current->action >= tables->actions.size())
	{
		return;
	}
	Complete(Locator::entitiesRegistry::value(), creature, mind, *mind.planner.current, *tables);
	mind.completedByStep = true;
}

bool CreatureMindSystem::Adopt(entt::entity creature, CreatureMindState& mind, const creature_planner::Plan& plan,
                               const creature_plan_actions::Situation& situation)
{
	const auto* tables = GetTables();
	if (tables == nullptr || plan.action >= tables->actions.size() || !mind.learnt.has_value() || !mind.desires.has_value())
	{
		return false;
	}
	const auto& info = tables->actions[plan.action];
	auto built = PlanAgenda(creature, plan.action, plan.object, situation);
	if (!built.has_value())
	{
		return false;
	}
	auto& [executor, agenda] = *built;
	auto& registry = Locator::entitiesRegistry::value();
	// Whatever its hands were doing is given up for the plan
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	auto& learnt = *mind.learnt;
	mind.planner.current = plan;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	creature_learning::ResetDrives((*mind.desires)[plan.desire]);
	creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	learnt.turnsSinceDone.at(plan.action) = 0;
	// Taking up compassion for a town starts its half minute afresh
	if (plan.desire == Desire::Compassion && plan.activityObject.has_value() &&
	    registry.AllOf<Town>(static_cast<entt::entity>(*plan.activityObject)))
	{
		town_compassion::StartHalfMinute(mind.townCompassion, mind.turn);
	}
	creature_learning::Remember(learnt.contexts,
	                            {
	                                .action = plan.action,
	                                .desire = plan.desire,
	                                .object = plan.object,
	                                .belief = plan.object.has_value()
	                                              ? BeliefOf(registry, static_cast<entt::entity>(*plan.object), creature)
	                                              : std::nullopt,
	                                .used = plan.instrument,
	                                .about = plan.activityObject,
	                                .learnable = info.learnable,
	                                .windowSeconds = info.learningWindowSeconds,
	                            });
	if (executor->build == creature_plan_actions::Build::ShowDesire)
	{
		mind.idle.showDesireSeconds = creature_mind::k_ShowDesireSeconds;
	}
	return true;
}

bool CreatureMindSystem::ScriptDoAction(entt::entity creature, uint32_t action, entt::entity target,
                                        std::optional<entt::entity> with)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || tables == nullptr || action >= tables->actions.size())
	{
		return false;
	}
	// The start every scripted action shares, then the action forced as a plan of obeying its player
	SetUpMind(creature, body->species, *mind);
	PrepareForScript(creature, *mind, true);
	return ForceActivity(
	    creature,
	    {.desire = Desire::ObeyPlayer, .action = tables->actions[action].name, .actionObject = target, .instrument = with});
}

bool CreatureMindSystem::ForceActivity(entt::entity creature, const ForcedActivity& forced)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// A creature told to do something in the turn it was made has its mind set up for it first
	if (const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	    mind != nullptr && body != nullptr)
	{
		SetUpMind(creature, body->species, *mind);
	}
	const auto* tables = GetTables();
	if (mind == nullptr || !registry.AllOf<CreatureAnimation>(creature) || tables == nullptr || !mind->learnt.has_value() ||
	    !mind->desires.has_value())
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, forced.action);
	if (!action.has_value())
	{
		return false;
	}
	// What it was doing stops as a failure, whatever comes of the new plan: its hands give up, its plan is dropped and it
	// lets go of a miracle it holds
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(*mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	const auto toNumber = [](std::optional<entt::entity> entity) {
		return entity.has_value() && *entity != entt::null ? std::optional(entt::to_integral(*entity)) : std::nullopt;
	};
	const auto actionObject = toNumber(forced.actionObject);
	creature_plan_actions::Situation situation {
	    .instrument = toNumber(forced.instrument),
	    .handFull = registry.AllOf<CreatureHeldObject>(creature),
	};
	SeeCamera(registry, creature, situation);
	const auto* forcedExecutor = creature_plan_actions::For(tables->actions[*action].name);
	if (forcedExecutor != nullptr && forcedExecutor->build == creature_plan_actions::Build::CastWaterOnItself)
	{
		situation.clearSpot = ClearSpotToCastOnItself(registry, creature);
	}
	// A look that doesn't go up to the thing, and a sit at a thing under a script's control, draw how long they last as
	// the agenda is made, in whole turns
	situation.controlledByScript = ecs::script_held::IsControlledByScript(creature);
	const auto drawTurns = [](float least, float most) {
		const auto turns = game_clock::TicksForSeconds(least + game_random::GameFloatRand(most));
		return static_cast<float>(turns) * k_TurnSeconds;
	};
	if (forcedExecutor != nullptr && forcedExecutor->build == creature_plan_actions::Build::LookButDontApproach)
	{
		situation.drawnSeconds =
		    drawTurns(creature_mind::k_LookButDontApproachSeconds, creature_mind::k_LookButDontApproachExtraSeconds);
	}
	if (forcedExecutor != nullptr && forcedExecutor->build == creature_plan_actions::Build::SitDown &&
	    situation.controlledByScript && actionObject.has_value())
	{
		situation.radius = ecs::object::Get2DRadius(creature);
		situation.drawnSeconds = drawTurns(creature_mind::k_SitAtSeconds, creature_mind::k_SitAtExtraSeconds);
	}
	// An agenda given is carried out as it is, in place of the one the action would make
	if (forced.agenda.has_value())
	{
		if (!Replan(creature, creature_mind::Activity::Planned, *forced.agenda))
		{
			return false;
		}
	}
	else
	{
		auto built = PlanAgenda(creature, *action, actionObject, situation);
		if (!built.has_value())
		{
			// The plan could not be made: it stands, doing nothing, to decide afresh
			if (Locator::creatureLocomotionSystem::has_value())
			{
				Locator::creatureLocomotionSystem::value().Stop(creature);
			}
			registry.Get<CreatureAnimation>(creature).body = {};
			creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
			return false;
		}
		auto& [executor, agenda] = *built;
		if (!Replan(creature, executor->activity, std::move(agenda)))
		{
			return false;
		}
	}
	mind->planner.current = creature_planner::Plan {
	    .desire = forced.desire,
	    .action = *action,
	    .object = actionObject,
	    .goalUsefulness = forced.score,
	    .actionPriority = forced.score,
	    .priority = forced.score,
	    .activityObject = toNumber(forced.activityObject),
	    .instrument = situation.instrument,
	    .forced = true,
	};
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->completedByStep = false;
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves; and having
	// decided, the desires the plan's desire opposes are held back
	if (creature_learning::RemembersPlan((*mind->desires)[forced.desire]))
	{
		const auto& info = tables->actions[*action];
		creature_learning::Remember(mind->learnt->contexts,
		                            {
		                                .action = *action,
		                                .desire = forced.desire,
		                                .object = actionObject,
		                                .belief = actionObject.has_value()
		                                              ? BeliefOf(registry, static_cast<entt::entity>(*actionObject), creature)
		                                              : std::nullopt,
		                                .used = situation.instrument,
		                                .about = toNumber(forced.activityObject),
		                                .learnable = info.learnable,
		                                .windowSeconds = info.learningWindowSeconds,
		                            });
	}
	creature_learning::SuppressOpposed(*mind->desires, forced.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

bool CreatureMindSystem::TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || !mind->desires.has_value() || !registry.Valid(object))
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_TakeFromHandAction);
	if (!action.has_value())
	{
		return false;
	}
	// It drops what it was doing at once, its body's action ended there, as a failure
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(*mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	if (!Replan(creature, creature_mind::Activity::Planned,
	            creature_mind::TakeFromHand(entt::to_integral(object), camera, leftHand)))
	{
		return false;
	}
	// Curious about what it is given, the plan is on the creature itself
	mind->planner.current = creature_planner::Plan {
	    .desire = Desire::Curiosity,
	    .action = *action,
	    .object = entt::to_integral(creature),
	    .goalUsefulness = creature_planner::k_ForcedScore,
	    .actionPriority = creature_planner::k_ForcedScore,
	    .priority = creature_planner::k_ForcedScore,
	    .forced = true,
	};
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	creature_learning::SuppressOpposed(*mind->desires, Desire::Curiosity, tables->dependencies, k_TurnsPerSecond);
	return true;
}

bool CreatureMindSystem::IsTakingFromHand(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || mind->idle.step >= mind->idle.agenda.size())
	{
		return false;
	}
	// The take is still ahead of it, or under way
	return std::ranges::any_of(mind->idle.agenda.begin() + static_cast<std::ptrdiff_t>(mind->idle.step),
	                           mind->idle.agenda.end(), [](const creature_mind::Step& step) {
		                           return step.kind == creature_mind::Step::Kind::Object &&
		                                  step.order.kind == creature_mind::ObjectOrder::Kind::TakeFromHand;
	                           });
}

void CreatureMindSystem::UseGift(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !mind->learnt.has_value() || !registry.Valid(object) ||
	    !Locator::creatureObjectActionSystem::has_value() ||
	    Locator::creatureObjectActionSystem::value().GetHeld(creature) != object)
	{
		return;
	}
	// TODO(creature-giving): the game first carries on the plan the creature had before it was given the thing, when that
	// plan's action takes a thing of this sort; not done yet
	const auto& desires = *mind->desires;
	const auto belief = BeliefOf(registry, object, creature);
	const auto weigh = [&](Desire desire) {
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = Usefulness(*mind->learnt, desire, belief)};
	};
	const auto kind = belief.has_value() ? belief->type : creature_tree::belief_types::k_Other;
	// TODO(creature-giving): the game also keeps which kinds it has learnt it can't eat; openblack has no such memory yet
	const auto choice = creature_giving::ChooseUse({
	    .curiosity = weigh(Desire::Curiosity),
	    .hunger = weigh(Desire::Hunger),
	    .playWithPlayer = weigh(Desire::PlayWithPlayer),
	    .cantEatKind = false,
	    .givenKindBefore = mind->givenKinds.contains(kind),
	});
	if (choice.remembersKind)
	{
		mind->givenKinds.insert(kind);
	}
	const auto number = entt::to_integral(object);
	switch (choice.use)
	{
	case creature_giving::Use::Examine:
		std::ignore = ForceActivity(creature, {.desire = Desire::Curiosity,
		                                       .action = k_ExamineGivenAction,
		                                       .actionObject = object,
		                                       .agenda = creature_mind::ExamineGiven(number)});
		break;
	case creature_giving::Use::Eat:
		std::ignore = ForceActivity(creature, {.desire = Desire::Hunger,
		                                       .action = k_EatGivenAction,
		                                       .actionObject = object,
		                                       .agenda = creature_mind::EatGiven()});
		break;
	case creature_giving::Use::Throw:
	{
		const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
		std::ignore = ForceActivity(creature, {.desire = Desire::PlayWithPlayer,
		                                       .action = k_ThrowGivenAction,
		                                       .actionObject = object,
		                                       .agenda = creature_mind::ThrowGiven(eye)});
		break;
	}
	case creature_giving::Use::AsLed:
		// Acted on as a thing shown on the leash is: its desires plan only for it, the next time it plans
		mind->leash.actOn.push_back(number);
		break;
	}
}

void CreatureMindSystem::PlanCreature(entt::entity creature, CreatureMindState& mind, bool everyDesire)
{
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	// Under a script's control it plans nothing
	if (tables == nullptr || transform == nullptr || !mind.desires.has_value() || !mind.learnt.has_value() || mind.paused ||
	    mind.leash.obeying || creature_mind::IsUnconscious(mind.idle) || ecs::script_held::IsControlledByScript(creature))
	{
		return;
	}
	if (mind.plannedTurn == mind.turn)
	{
		return;
	}
	mind.plannedTurn = mind.turn;
	// The plans kept from before are lost when their desire has been suppressed for longer, or what they are about,
	// done to or done with is gone
	const auto gone = [&registry](std::optional<uint32_t> id) {
		if (!id.has_value())
		{
			return false;
		}
		const auto entity = static_cast<entt::entity>(*id);
		return !registry.Valid(entity) || !ecs::IsAvailable(entity);
	};
	// A plan lost with what it rests on starts the pass again
	bool passAgain = false;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		auto& state = mind.desires->desires.at(d);
		auto& kept = mind.planner.best.at(d);
		if (kept.has_value() && (gone(kept->object) || gone(kept->activityObject) || gone(kept->instrument)))
		{
			kept.reset();
			passAgain = true;
		}
		else if (state.planVoided)
		{
			kept.reset();
		}
		state.planVoided = false;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto& desires = *mind.desires;
	const auto& learnt = *mind.learnt;

	creature_plan_actions::Situation situation;
	SeeCamera(registry, creature, situation);
	if (mind.idle.showDesireSeconds <= 0.0f)
	{
		if (const auto strongest = creature_desires::StrongestShowable(desires, creature_mind::k_MinDesireShown))
		{
			situation.showDesireAnimation = creature_desires::EmoteFor(*strongest);
		}
	}
	bool waterLooked = false;
	bool hurlLooked = false;
	bool clearLooked = false;
	const auto prepare = [&](const creature_plan_actions::Executor& executor) {
		if (executor.build == creature_plan_actions::Build::Drink && !waterLooked)
		{
			waterLooked = true;
			situation.water = NearestWater(position);
		}
		if (executor.build == creature_plan_actions::Build::Hurl && !hurlLooked)
		{
			hurlLooked = true;
			situation.hurlTarget = NearestHurlTarget(registry, position);
		}
		// Only a creature on fire casts water on itself, so only then is a clear spot looked for
		if (executor.build == creature_plan_actions::Build::CastWaterOnItself && !clearLooked && Burning(creature))
		{
			clearLooked = true;
			situation.clearSpot = ClearSpotToCastOnItself(registry, creature);
		}
	};

	std::array<float, creature_desires::k_DesireCount> strengths {};
	std::array<bool, creature_desires::k_DesireCount> eligible {};
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto& state = desires.desires.at(d);
		const auto strength = state.activated && state.suppressedTurns == 0 ? state.value : 0.0f;
		strengths.at(d) = strength;
		// Compassion takes its turn whatever its own list holds: what it does comes from the town it helps
		eligible.at(d) = strength > 0.0f && (d == static_cast<size_t>(Desire::Compassion) ||
		                                     std::ranges::any_of(tables->desireActions.at(d), [tables](uint32_t action) {
			                                     return creature_plan_actions::For(tables->actions.at(action).name) != nullptr;
		                                     }));
	}

	// Something shown on the leash is the only goal there is this turn, for every desire
	std::optional<entt::entity> leashTarget;
	if (!mind.leash.actOn.empty())
	{
		leashTarget = static_cast<entt::entity>(mind.leash.actOn.back());
		mind.leash.actOn.clear();
	}
	if (passAgain)
	{
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(desires));
	}
	// The desires are planned two a turn in passes, and what to do is decided as a pass ends
	const bool inPasses = !leashTarget.has_value() && !everyDesire;
	auto goals = inPasses ? creature_planner::NextGoals(mind.planner) : std::vector<Desire> {};
	if (leashTarget.has_value() || everyDesire)
	{
		for (size_t d = 0; d < eligible.size(); ++d)
		{
			if (eligible.at(d))
			{
				goals.push_back(static_cast<Desire>(d));
			}
		}
	}

	creature_planner::PlanFacts facts {.developmentPhase = mind.developmentPhase, .attitudeToPlayer = mind.attitudeToPlayer};
	if (const auto* body = registry.TryGet<const CreatureNeeds>(creature))
	{
		facts.exhaustion = body->needs.exhaustion;
		facts.warmth = body->needs.warmth;
	}
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature); leash != nullptr && leash->home.has_value())
	{
		facts.metresFromHome = gutils::GetDistanceInMetres(transform->position, *leash->home);
	}
	for (const auto desire : goals)
	{
		const auto d = static_cast<size_t>(desire);
		// A desire that may not be planned now has its plan made worth nothing, the rest of it left as it was
		if (!creature_planner::MayPlan(desire, facts))
		{
			if (auto& old = mind.planner.best.at(d); old.has_value())
			{
				old->priority = 0.0f;
			}
			continue;
		}
		std::optional<creature_planner::Plan> best;
		// Actions are weighed in groups by the kind of thing they are done to, so each has a goal it can be done to; a
		// desire that looks for no goal weighs them all in the order of its list
		std::array<std::vector<creature_planner::ActionCandidate>, creature_plan_actions::k_TargetCount> groups {};
		std::vector<std::pair<size_t, creature_planner::ActionCandidate>> inListOrder;
		// Compassion is about a town, and what it may do is what the town desire it helps with asks for, done to the
		// town's own buildings and people
		std::span<const uint32_t> actions = tables->desireActions.at(d);
		std::vector<uint32_t> townActions;
		std::optional<entt::entity> town;
		// Anger does what it does to its goal itself
		creature_planner::GoalSearch search {.searches = tables->rules.at(d).searchesForGoal,
		                                     .actsOnGoal = desire == Desire::Anger};
		// Kept within an area, the radius it is kept within: what lies farther from it is out of its reach
		std::optional<float> confinedWithin;
		if (desire == Desire::Compassion)
		{
			if (const auto* kept = registry.TryGet<const CreatureLeash>(creature);
			    kept != nullptr && Locator::leashSystem::has_value())
			{
				const auto& leashes = Locator::leashSystem::value();
				const bool leashed = leashes.IsLeashed(creature);
				if (creature_leash::IsConfined(kept->confinementRadius, leashed, !leashed || leashes.Works(creature)))
				{
					confinedWithin = kept->confinementRadius;
				}
			}
			const auto goal = CompassionTown(registry, creature, position, learnt, tables->rules.at(d).distanceWeight);
			if (goal.has_value())
			{
				town = goal->town;
				search.foundWorth = goal->worth;
			}
			if (town.has_value())
			{
				townActions = TownCompassionActions(registry, creature, mind, *town, *tables,
				                                    [](uint32_t range) { return Random(range); });
			}
			actions = townActions;
		}
		for (const auto action : actions)
		{
			const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
			if (executor == nullptr)
			{
				continue;
			}
			prepare(*executor);
			const bool mayCast = creature_plan_actions::IsCast(*executor) &&
			                     MayCast(creature, mind, action, IsPowerUpCast(tables->actions.at(action).name));
			const bool onFire = executor->build == creature_plan_actions::Build::CastWaterOnItself && Burning(creature);
			const creature_planner::ActionCandidate candidate {
			    .action = action,
			    .needsObject = executor->target != Target::None,
			    .opinion = learnt.opinions.at(action),
			    .turnsSinceDone = learnt.turnsSinceDone.at(action),
			    .servesFirstDesire = tables->actions.at(action).desire == 0,
			    .disabled = !creature_plan_actions::Possible(*executor, situation) ||
			                !creature_plan_actions::MayTry(*executor, mayCast, onFire),
			};
			groups.at(static_cast<size_t>(executor->target)).push_back(candidate);
			inListOrder.push_back({static_cast<size_t>(executor->target), candidate});
		}
		// What each group's actions could be done to
		std::array<std::vector<creature_planner::ObjectCandidate>, creature_plan_actions::k_TargetCount> objectsOf {};
		for (size_t g = 0; g < groups.size(); ++g)
		{
			const auto target = static_cast<Target>(g);
			auto& objects = objectsOf.at(g);
			if (!groups.at(g).empty() && target != Target::None)
			{
				if (leashTarget.has_value())
				{
					if (Accepts(registry, target, *leashTarget, creature) &&
					    (!town.has_value() || PartOfTown(registry, *leashTarget, *town)))
					{
						const auto at = PointOf(registry, *leashTarget).value_or(position);
						objects.push_back(
						    {.id = entt::to_integral(*leashTarget),
						     .distance = gutils::GetDistanceInMetres(at, position),
						     .usefulness = Usefulness(learnt, desire, BeliefOf(registry, *leashTarget, creature))});
					}
				}
				else if (town.has_value())
				{
					// A plan for a town is done to one of its members, the best for the distance, however far
					std::vector<creature_planner::ObjectCandidate> members;
					for (const auto member : TownMembers(registry, *town))
					{
						if (!Accepts(registry, target, member, creature))
						{
							continue;
						}
						const auto at = PointOf(registry, member).value_or(position);
						const auto distance = gutils::GetDistanceInMetres(at, position);
						const auto* held = registry.TryGet<const HeldByCreature>(member);
						members.push_back({
						    .id = entt::to_integral(member),
						    .distance = distance,
						    .held = held != nullptr && held->creature == creature,
						    .usefulness = Usefulness(learnt, desire, BeliefOf(registry, member, creature)),
						    .outOfReach = confinedWithin.has_value() && distance > *confinedWithin,
						});
					}
					if (const auto chosen = creature_planner::BestActOn(members, tables->rules.at(d).distanceWeight))
					{
						objects.push_back(members.at(*chosen));
					}
				}
				else
				{
					for (const auto& found : Gather(registry, target, creature, position))
					{
						const auto* held = registry.TryGet<const HeldByCreature>(found.entity);
						objects.push_back({
						    .id = entt::to_integral(found.entity),
						    .distance =
						        gutils::GetDistanceInMetres(PointOf(registry, found.entity).value_or(position), position),
						    .held = held != nullptr && held->creature == creature,
						    .usefulness = Usefulness(learnt, desire, BeliefOf(registry, found.entity, creature)),
						});
					}
				}
			}
		}
		// A desire that looks for no goal takes its action first, down its whole list, then what to do it to
		if (!search.searches)
		{
			std::vector<creature_planner::ActionChoice> choices;
			choices.reserve(inListOrder.size());
			for (const auto& [g, candidate] : inListOrder)
			{
				choices.push_back({.action = candidate, .objects = objectsOf.at(g)});
			}
			mind.planner.best.at(d) = creature_planner::PlanWithoutGoal(desire, strengths.at(d), choices);
			continue;
		}
		for (size_t g = 0; g < groups.size(); ++g)
		{
			if (groups.at(g).empty())
			{
				continue;
			}
			const auto plan = creature_planner::PlanDesire(desire, strengths.at(d), objectsOf.at(g),
			                                               tables->rules.at(d).distanceWeight, groups.at(g), search);
			if (plan.has_value() && (!best.has_value() || creature_planner::Replaces(*plan, *best, actions)))
			{
				best = plan;
				if (town.has_value())
				{
					best->activityObject = entt::to_integral(*town);
				}
			}
		}
		mind.planner.best.at(d) = best;
	}

	// Doing something it chose for itself that isn't idling, it carries on
	if (!mind.planActive && !Interruptible(mind.idle.activity) && mind.idle.step < mind.idle.agenda.size())
	{
		return;
	}
	if (inPasses && !creature_planner::PassOver(mind.planner))
	{
		return;
	}
	// A plan it can't carry out is left as it is, as the game leaves a plan that is not valid
	if (const auto choice = creature_planner::Choose(mind.planner))
	{
		std::ignore = Adopt(creature, mind, *choice, situation);
	}
	if (inPasses)
	{
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(desires));
	}
}

void CreatureMindSystem::PlanTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> creatures;
	std::as_const(registry).Each<const Creature, const CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureMindState&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			PlanCreature(entity, *mind);
		}
	}
}

void CreatureMindSystem::LearnFromFeedback(entt::entity creature, CreatureMindState& mind, float feedback)
{
	namespace sources = creature_desires::sources;
	if (!mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto& desires = *mind.desires;
	auto& learnt = *mind.learnt;
	// The species' floor, which a desire made least dominant goes no lower than
	float floor = 0.0f;
	if (const auto* body = registry.TryGet<const Creature>(creature); body != nullptr && Locator::infoConstants::has_value())
	{
		const auto& species = Locator::infoConstants::value().creature;
		const auto row = static_cast<size_t>(creature::InfoRow(body->species));
		floor = row < species.size() ? species.at(row).desireFloor : 0.0f;
	}
	const auto contextIndex = creature_learning::BestContext(learnt.contexts);
	auto* context = contextIndex.has_value() ? &learnt.contexts.at(*contextIndex) : nullptr;
	const auto* info = context != nullptr && tables != nullptr && context->action < tables->actions.size()
	                       ? &tables->actions[context->action]
	                       : nullptr;

	// The eating and the running away are judged on the newest thing it did
	const auto* newest = creature_learning::Newest(learnt.contexts);
	const auto* newestInfo = newest != nullptr && tables != nullptr && newest->action < tables->actions.size()
	                             ? &tables->actions[newest->action]
	                             : nullptr;
	// Slapped, it gives up the newest thing it did, if that is a plan it could carry out, and wants its desire least of
	// all, every desire let go of again
	const auto giveUp = [&]() {
		if (!(feedback < 0.0f) || newest == nullptr || tables == nullptr ||
		    !creature_learning::PlanIsValid(*newest, tables->rules.at(static_cast<size_t>(newest->desire)).searchesForGoal,
		                                    creature_mind_tables::NeedsInstrument(newest->action)))
		{
			return;
		}
		creature_learning::MakeLeastDominant(desires, newest->desire, creature_learning::k_LeastDominantFactor, floor);
		for (auto& state : desires.desires)
		{
			state.suppressedTurns = 0;
		}
		Abandon(mind);
	};
	// Stroked for holding something while doing an action that allows it, it learns to eat from the hand
	if (creature_learning::StrokeTeachesEating(feedback, mind.developmentPhase,
	                                           newestInfo != nullptr && newestInfo->eatWhenStroked) &&
	    Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature); held.has_value())
		{
			creature_learning::MakeFullyDominant(desires, Desire::Hunger);
			if (const auto belief = BeliefOf(registry, *held, creature); belief.has_value() && tables != nullptr)
			{
				creature_mind_model::Learn(learnt, TreeKind::Use, Desire::Hunger, {.belief = *belief, .feedback = 1.0f},
				                           tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
			}
			creature_mind_model::Think(learnt, "I've learnt to eat what I'm given");
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} learns to eat what it holds", entt::to_integral(creature));
			Replan(creature, creature_mind::Activity::Eat, creature_mind::EatHeld());
			return;
		}
	}
	// Stroked for running away from the player, it is forgiven and wants to run away less
	if (creature_learning::StrokeForgivesRunningAway(feedback, newest))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is forgiven for running away", entt::to_integral(creature));
		auto& runAway = desires[Desire::RunAwayFromPlayer];
		creature_learning::MakeLeastDominant(desires, Desire::RunAwayFromPlayer, creature_learning::k_LeastDominantFactor,
		                                     floor);
		for (auto& source : runAway.sources)
		{
			if (source.type == sources::k_RunAwayFromPlayer)
			{
				source.value = 0.0f;
				source.threshold = std::min(source.threshold + k_RunAwayThresholdStep, creature_learning::k_MaxThreshold);
			}
		}
		Abandon(mind);
		return;
	}

	const auto strength = creature_learning::Strengthened(feedback);
	// With nothing it did worth crediting, the feedback does nothing more
	const auto path = creature_learning::PathOf(context != nullptr, info != nullptr, mind.developmentPhase, k_MinLessonPhase);
	if (path == creature_learning::FeedbackPath::Nothing)
	{
		return;
	}
	if (feedback > 0.0f)
	{
		if (learnt.mimicry.has_value() && tables != nullptr)
		{
			creature_watching::StrokedWhileMimicking(learnt.mimicry, tables->mimics);
			creature_desires::ChangeSource(desires, k_FollowPlayerSource, k_MimicStrokeBoost);
		}
		creature_desires::Suppress(desires, Desire::Sadness, k_StrokeSadnessSeconds, k_TurnsPerSecond);
	}

	if (path == creature_learning::FeedbackPath::Plain)
	{
		// Too young to learn from it: stroking makes it playful, showy and kind; slapping angry and fearful
		const auto plain = creature_learning::PlainOf(feedback);
		for (const auto type : plain.sources)
		{
			creature_desires::ChangeSource(desires, type, plain.amount);
		}
		// One time in two it has something to do about it next: frightened by a slap, happy for a stroke
		if (Random(2) == 0)
		{
			mind.afterFeedback =
			    feedback < 0.0f ? CreatureMindState::AfterFeedback::Frightened : CreatureMindState::AfterFeedback::Happy;
			if (auto* casting = registry.TryGet<CreatureCasting>(creature))
			{
				casting->fizzle.reset();
			}
		}
		giveUp();
		return;
	}

	const auto desire = context->desire;
	const auto d = static_cast<size_t>(desire);
	const auto& texts = tables->texts.at(d);
	context->credited = strength;
	if (tables->rules.at(d).learnable)
	{
		creature_learning::LearnDesireLesson(desires, desire, creature_learning::DominantSource(desires[desire]), strength,
		                                     tables->rules, tables->dependencies);
		creature_mind_model::Think(learnt, creature_learning::DesireLessonText(strength, texts.desire));
		if (context->belief.has_value())
		{
			for (const auto& [other, share] : creature_learning::Spread(desire, strength, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, TreeKind::ActOn, other,
				                           {.belief = *context->belief, .feedback = std::clamp(share, -1.0f, 1.0f)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
			creature_mind_model::Think(
			    learnt, creature_learning::ObjectLessonText(strength, ObjectName(context->belief), texts.trying));
		}
		if (context->usedBelief.has_value() && context->used != context->object)
		{
			const auto used = desire == Desire::Poo ? std::clamp(strength, -k_PooUseFeedback, k_PooUseFeedback) : strength;
			for (const auto& [other, share] : creature_learning::Spread(desire, used, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, TreeKind::Use, other,
				                           {.belief = *context->usedBelief, .feedback = std::clamp(share, -1.0f, 1.0f)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
		}
		auto& opinion = learnt.opinions.at(context->action);
		opinion = creature_learning::OpinionAfter(opinion, strength);
		creature_mind_model::Think(
		    learnt, creature_learning::ActionLessonText(strength, info->name, texts.trying, ObjectName(context->belief)));
	}

	if (feedback <= 0.0f)
	{
		giveUp();
		return;
	}
	// Stroked for something it did to a thing other than itself, and not while it copies the player, it does it again to
	// the thing of that kind most useful to do it to (the same one, it may be), forced on it at a plan's plain worth
	if (!context->object.has_value() || !context->belief.has_value() || *context->object == entt::to_integral(creature) ||
	    learnt.mimicry.has_value())
	{
		return;
	}
	// The game's row 276 is carried out as row 11
	constexpr uint32_t k_CarriedOutOtherwise = 276;
	constexpr uint32_t k_CarriedOutAs = 11;
	const auto action = context->action == k_CarriedOutOtherwise ? k_CarriedOutAs : context->action;
	const auto* executor = action < tables->actions.size() ? creature_plan_actions::For(tables->actions[action].name) : nullptr;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (executor == nullptr || transform == nullptr)
	{
		return;
	}
	// An action done to nothing is done to itself
	entt::entity again = entt::null;
	if (executor->target != Target::None)
	{
		std::optional<entt::entity> best;
		float bestUsefulness = 0.0f;
		const glm::vec2 position {transform->position.x, transform->position.z};
		for (const auto& found : Gather(registry, executor->target, creature, position))
		{
			const auto belief = BeliefOf(registry, found.entity, creature);
			if (found.entity == creature || !belief.has_value() || belief->type != context->belief->type)
			{
				continue;
			}
			// The most useful, the first of equals, worth more than nothing
			const auto usefulness = Usefulness(learnt, desire, belief);
			if (usefulness > bestUsefulness)
			{
				best = found.entity;
				bestUsefulness = usefulness;
			}
		}
		if (!best.has_value())
		{
			return;
		}
		again = *best;
	}
	std::ignore = ForceActivity(
	    creature, {.desire = desire, .action = tables->actions[action].name, .actionObject = again, .score = 1.0f});
}

void CreatureMindSystem::LearnTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr)
	{
		return;
	}
	std::vector<entt::entity> creatures;
	std::as_const(registry).Each<const Creature, const CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureMindState&) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		auto* mind = registry.TryGet<CreatureMindState>(creature);
		if (mind == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
		{
			continue;
		}
		auto& learnt = *mind->learnt;
		// What the player showed it on a leash: anger or compassion towards such things
		for (const auto& shown : mind->leash.shown)
		{
			const auto belief = BeliefOf(registry, static_cast<entt::entity>(shown.object), creature);
			if (!belief.has_value())
			{
				continue;
			}
			for (const auto& lesson : shown.lessons)
			{
				creature_mind_model::Learn(learnt, TreeKind::ActOn, lesson.desire,
				                           {.belief = *belief, .feedback = lesson.change},
				                           tables->attributes.at(static_cast<size_t>(lesson.desire)));
				creature_mind_model::Think(
				    learnt, creature_learning::ObjectLessonText(lesson.change, ObjectName(belief),
				                                                tables->texts.at(static_cast<size_t>(lesson.desire)).trying));
			}
		}
		mind->leash.shown.clear();
		// Leashed to another creature, it comes to find it nicer or nastier, and learns about creatures like it
		for (const auto& change : mind->leash.attitudes)
		{
			auto& attitude = creature_learning::AttitudeTo(learnt.creatures, change.creature);
			const auto lessons = creature_learning::ChangeHowNice(attitude, change.change);
			if (const auto belief = BeliefOf(registry, static_cast<entt::entity>(change.creature), creature))
			{
				for (const auto& [desire, amount] : lessons)
				{
					creature_mind_model::Learn(learnt, TreeKind::ActOn, desire, {.belief = *belief, .feedback = amount},
					                           tables->attributes.at(static_cast<size_t>(desire)));
				}
			}
		}
		mind->leash.attitudes.clear();

		// Copying the player moves on a stage at a time, not while a script controls it
		if (!learnt.mimicry.has_value() || mind->turn % k_MimicStepTurns != 0 ||
		    ecs::script_held::IsControlledByScript(creature))
		{
			continue;
		}
		const auto before = learnt.mimicry->stage;
		creature_watching::StepMimicry(learnt.mimicry, tables->mimics, [](uint32_t range) { return Random(range); });
		if (!learnt.mimicry.has_value() || learnt.mimicry->stage == before)
		{
			continue;
		}
		const auto& rule = tables->mimics.at(learnt.mimicry->rule);
		if (learnt.mimicry->stage == creature_watching::MimicStage::CopyAction)
		{
			// It does what the player did, to the same thing or one like it
			const auto* transform = registry.TryGet<const Transform>(creature);
			for (const auto action : rule.copyActions)
			{
				const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
				// A miracle it copies passes the same test as one it chooses
				if (executor == nullptr || transform == nullptr ||
				    !creature_plan_actions::MayTry(
				        *executor,
				        creature_plan_actions::IsCast(*executor) &&
				            MayCast(creature, *mind, action, IsPowerUpCast(tables->actions.at(action).name)),
				        executor->build == creature_plan_actions::Build::CastWaterOnItself && Burning(creature)))
				{
					continue;
				}
				std::optional<uint32_t> object;
				if (executor->target != Target::None)
				{
					if (learnt.mimicry->object.has_value() &&
					    Accepts(registry, executor->target, static_cast<entt::entity>(*learnt.mimicry->object), creature))
					{
						object = learnt.mimicry->object;
					}
					else if (const auto found = Gather(registry, executor->target, creature,
					                                   glm::vec2(transform->position.x, transform->position.z));
					         !found.empty())
					{
						object = entt::to_integral(found.front().entity);
					}
					if (!object.has_value())
					{
						continue;
					}
				}
				const auto desire = static_cast<Desire>(
				    std::min<uint32_t>(tables->actions.at(action).desire, creature_desires::k_DesireCount - 1));
				creature_plan_actions::Situation situation;
				situation.hurlTarget = NearestHurlTarget(registry, glm::vec2(transform->position.x, transform->position.z));
				if (executor->build == creature_plan_actions::Build::CastWaterOnItself)
				{
					situation.clearSpot = ClearSpotToCastOnItself(registry, creature);
				}
				if (Adopt(creature, *mind,
				          {.desire = desire,
				           .action = action,
				           .object = object,
				           // Copying counts as pressing as a desire at its strongest, with nothing learnt
				           .priority = creature_planner::Priority(1.0f, std::nullopt,
				                                                  creature_planner::ActionPriority(learnt.opinions.at(action)),
				                                                  creature_planner::k_DefaultUsefulness, std::nullopt)},
				          situation))
				{
					creature_mind_model::Think(learnt, fmt::format("I'm copying you: {}", tables->actions.at(action).name));
					break;
				}
			}
		}
		else if (learnt.mimicry->stage == creature_watching::MimicStage::CopyDesire &&
		         rule.desire < creature_desires::k_DesireCount)
		{
			// It comes to want what the player seemed to want
			creature_learning::MakeFullyDominant(*mind->desires, static_cast<Desire>(rule.desire));
			creature_mind_model::Think(learnt, fmt::format("I want what you wanted: {}", tables->texts.at(rule.desire).desire));
		}
	}
}

void CreatureMindSystem::LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind)
{
	if (auto* state = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		state->pendingFile = std::move(mind);
	}
}

std::optional<creaturemind::MindFileData> CreatureMindSystem::SaveMind(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value() || !mind->learnt.has_value())
	{
		return std::nullopt;
	}
	// What a script taught it that waits for the mind to take up its file is already what the creature knows
	auto learnt = *mind->learnt;
	TakeUpScriptKnows(mind->scriptKnows, _tables.has_value() ? &*_tables : nullptr,
	                  creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)), learnt);
	auto file = creature_mind_model::ToFile(*mind->desires, learnt, mind->developmentPhase, mind->attitudeToPlayer,
	                                        static_cast<uint32_t>(creature::InfoRow(body->species)));
	// A name the script gave it that waits for the mind to take up its file is already the creature's name
	if (mind->scriptName.has_value())
	{
		file.name = *mind->scriptName;
	}
	// It is saved as itself: as it was before any spell on it changed it
	const creature_spells::SavedBody now {.size = body->size, .strength = body->strength, .alignment = body->alignment};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, now) : now;
	// While the land's curse on the creature of the player at this machine runs, it is saved as the curse found it
	if (body->owner == creature::LocalPlayer() && Locator::vm::has_value())
	{
		if (const auto cursed = MindCurseValuesToSave(Locator::vm::value()); cursed.has_value())
		{
			saved = *cursed;
		}
	}
	file.physique.size = saved.size;
	file.physique.strength = saved.strength;
	file.alignment = saved.alignment;
	return file;
}

void CreatureMindSystem::GiveUpPlan(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		Abandon(*mind);
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	}
}

void CreatureMindSystem::SetAgendaPriority(entt::entity creature, float priority)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	    mind != nullptr && mind->planner.current.has_value())
	{
		mind->planner.current->priority = priority;
	}
}

void CreatureMindSystem::ClearLearning(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr)
	{
		return;
	}
	const auto name = mind->learnt.has_value() ? mind->learnt->name : std::u16string {};
	Abandon(*mind);
	mind->desires = creature_desires::Create(SetupFor(body->species),
	                                         [](float low, float high) { return game_random::GameFloatRange(low, high); });
	mind->desiresPhase.reset();
	mind->planner = {};
	mind->pendingFile.reset();
	mind->resourceChecked = true;
	SetUpLearning(creature, *mind);
	mind->learnt->name = name;
}

void CreatureMindSystem::SeeSkill(const glm::vec3& point, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr)
	{
		return;
	}
	for (const auto entity : MindsOf(registry))
	{
		auto& mind = registry.Get<CreatureMindState>(entity);
		const auto& at = std::as_const(registry).Get<Transform>(entity);
		// A mind stilled, as by the freeze spell, learns nothing
		if (!mind.learnt.has_value() || mind.paused || glm::distance(at.position, point) > k_ViewDistance)
		{
			continue;
		}
		const auto progress = creature_watching::SeeSkill(mind.learnt->knowledge, skill, tables->skills, mind.developmentPhase,
		                                                  mind.turn, k_TurnsPerSecond);
		if (progress.learnt && !progress.ignored)
		{
			creature_mind_model::Think(*mind.learnt, fmt::format("I've learnt to {}", tables->skills.at(skill).name));
		}
	}
}

void CreatureMindSystem::SeeMiracle(const glm::vec3& point, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (GetTables() == nullptr)
	{
		return;
	}
	for (const auto entity : MindsOf(registry))
	{
		const auto& at = std::as_const(registry).Get<Transform>(entity);
		if (glm::distance(at.position, point) <= k_ViewDistance)
		{
			WatchMiracle(entity, miracle);
		}
	}
}

void CreatureMindSystem::WatchMiracle(entt::entity creature, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || body == nullptr || mind == nullptr || !mind->learnt.has_value() || mind->paused)
	{
		return;
	}
	auto& knowledge = mind->learnt->knowledge;
	const auto before = miracle < knowledge.miraclesSeen.size() ? knowledge.miraclesSeen[miracle].count : 0u;
	const auto multiplier = creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species));
	const auto progress = creature_watching::SeeMiracle(knowledge, miracle, tables->miracles, mind->developmentPhase,
	                                                    mind->turn, mind->leash.miracleSightingWeight, multiplier);
	// It thinks so once, at the sighting that takes it to the times needed, though it is told at every sighting after
	if (progress.learnt && !progress.ignored &&
	    static_cast<float>(before) < creature_watching::TimesNeeded(tables->miracles.at(miracle).timesToSee, multiplier))
	{
		creature_mind_model::Think(*mind->learnt, fmt::format("I've learnt the miracle {}", tables->miracles.at(miracle).name));
	}
}

void CreatureMindSystem::EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = MindSeeing(registry, player, point); mind != nullptr)
	{
		creature_perceived_desires::Increase(mind->perceivedDesires, static_cast<int32_t>(desire), weight);
	}
}

void CreatureMindSystem::EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight,
                                                 const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = MindSeeing(registry, player, point); mind != nullptr)
	{
		creature_perceived_desires::IncreaseTown(mind->perceivedDesires, static_cast<int32_t>(desire), weight);
	}
}

bool CreatureMindSystem::SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		return false;
	}
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return true;
	}
	const CreatureMindState::ScriptKnows edit {.kind = kind, .action = action, .knows = knows};
	// The original's mind holds its file from the creature's making, so what the script teaches goes over the file's
	if (mind->learnt.has_value() && mind->pendingFile == nullptr && mind->scriptKnows.empty())
	{
		TakeUpScriptKnows({&edit, 1}, GetTables(), creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)),
		                  *mind->learnt);
	}
	else
	{
		mind->scriptKnows.push_back(edit);
	}
	return true;
}

void CreatureMindSystem::TakeUpScriptKnows(std::span<const CreatureMindState::ScriptKnows> edits,
                                           const creature_mind_tables::Tables* tables, float speciesMultiplier,
                                           creature_mind_model::Learnt& learnt)
{
	if (tables == nullptr)
	{
		return;
	}
	for (const auto& edit : edits)
	{
		// A kind or a row the game's tables don't have is left alone
		creature_watching::SetKnows(learnt.knowledge, edit.kind, edit.action, edit.knows, tables->miracles, speciesMultiplier);
	}
}

void CreatureMindSystem::PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || deed >= tables->mimics.size())
	{
		return;
	}
	std::vector<entt::entity> noticed;
	for (const auto entity : MindsOf(registry))
	{
		// only the player's own creatures watch what the player does
		if (std::as_const(registry).Get<const Creature>(entity).owner != player)
		{
			continue;
		}
		auto& mind = registry.Get<CreatureMindState>(entity);
		const auto& at = std::as_const(registry).Get<Transform>(entity);
		if (!mind.learnt.has_value())
		{
			continue;
		}
		const creature_watching::MimicConditions conditions {
		    .phase = mind.developmentPhase,
		    .learningLeashInHand = mind.leash.learningInHand,
		    .reactionPriority = 0.0f,
		    .canSee = glm::distance(at.position, point) <= k_ViewDistance,
		};
		const auto target = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt;
		if (creature_watching::StartMimicry(mind.learnt->mimicry, deed, tables->mimics, conditions, target,
		                                    [] { return Chance(); }))
		{
			noticed.push_back(entity);
		}
	}
	// It notices: it turns to where the player did it and looks at it
	for (const auto entity : noticed)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			creature_mind_model::Think(*mind->learnt, fmt::format("I saw you: {}", tables->mimics.at(deed).name));
			Replan(entity, creature_mind::Activity::Planned, creature_mind::LookAt(glm::vec2(point.x, point.z), 2.0f));
		}
	}
}
