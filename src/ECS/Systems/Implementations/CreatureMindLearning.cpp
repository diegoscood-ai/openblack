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
#include <array>
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
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRules.h"
#include "Creature/LocalPlayer.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
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
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerStateFunctions.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
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
	return ecs::creature_pose::BodyYaw(registry, creature, at.rotation);
}

namespace
{
/// The advisor may tell the player of what the creature learnt
void TellHelp(entt::entity creature, const creature_help::Message& message)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(creature, message);
	}
}

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
/// What noticing a deed adds to the first source of the deed's desire, and to its wish to follow the player
constexpr float k_NoticedDeedSource = 0.1f;
constexpr float k_NoticedDeedFollow = 0.3f;
/// Forgiven for running away, the threshold of its wish to run raises this much
constexpr float k_RunAwayThresholdStep = 0.1f;
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

/// The player of a town, as a thing of the town has it; none without a town
std::optional<uint32_t> TownPlayerOf(const ecs::Registry& registry, entt::entity town)
{
	const auto* data = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	return data != nullptr ? std::optional(PlayerNumberOf(data->owner)) : std::nullopt;
}

/// What the agenda knows of the camera: where it is, if there is one, and whether the creature has a player. And the
/// game's turns in a second, which the agendas count their drawn times in
void SeeCamera(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Situation& situation)
{
	situation.turnsPerSecond = k_TurnsPerSecond;
	if (Locator::camera::has_value())
	{
		situation.eye = Locator::camera::value().GetOrigin();
		situation.camera = glm::vec2(situation.eye.x, situation.eye.z);
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL;
}

/// What the agenda knows of the creature's home: the creature itself, where it stands, where its home is and how far,
/// the heart of its player's temple, and whether another creature is near enough to frighten it
void SituateHome(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body == nullptr || at == nullptr)
	{
		return;
	}
	situation.self = entt::to_integral(creature);
	situation.standing = at->position;
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		situation.stage = mind->developmentPhase;
	}
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		situation.home = glm::vec2(home->x, home->z);
		situation.homeDistance = gutils::GetDistanceInMetres(glm::vec2(at->position.x, at->position.z), *situation.home);
	}
	// The heart of its player's temple is the temple itself, on the ground
	registry.Each<const Temple, const Transform>([&](entt::entity temple, const Temple& building, const Transform& place) {
		if (building.owner != body->owner || situation.templeHeart.has_value())
		{
			return;
		}
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt({place.position.x, place.position.z})
		                        : place.position.y;
		situation.templeHeart = creature_plan_actions::Situation::TempleHeart {
		    .temple = entt::to_integral(temple),
		    .point = {place.position.x, ground, place.position.z},
		    .radius = object_measures::TwoDRadius(registry, temple),
		};
	});
	situation.hasTemple = situation.templeHeart.has_value();
	situation.leashed = Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature);
	// Every other creature counts, of any player, dead or alive
	std::vector<float> others;
	registry.Each<const Creature, const Transform>([&](entt::entity other, const Creature&, const Transform& place) {
		if (other != creature)
		{
			others.push_back(gutils::GetDistanceInMetres(place.position, at->position));
		}
	});
	situation.nothingFrighteningNear =
	    creature_mind::NothingFrighteningNear(object_measures::Height(registry, creature), others);
	situation.visualNight = Locator::dayNightClock::has_value() && Locator::dayNightClock::value().Clock().IsVisualNight();
}

/// Where the altar stands of the worship site that a creature of the player takes a sacrifice to: the site in the first
/// slot around the player's temple that has a site. None when the player has no temple, or the temple has no site.
std::optional<glm::vec3> SacrificeAltarOf(const ecs::Registry& registry, PlayerNames player)
{
	std::optional<glm::vec3> altar;
	registry.Each<const Temple, const CitadelWorship>([&](const Temple& temple, const CitadelWorship& worship) {
		if (temple.owner != player || altar.has_value())
		{
			return;
		}
		std::array<bool, CitadelWorship::k_Sites> slotHasSite {};
		std::ranges::transform(worship.sites, slotHasSite.begin(),
		                       [&registry](entt::entity site) { return site != entt::null && registry.Valid(site); });
		const auto slot = creature_sacrifice::SitePlace(slotHasSite);
		if (!slot.has_value())
		{
			return;
		}
		// The altar is the site's totem
		const auto* site = registry.TryGet<const WorshipSite>(worship.sites.at(*slot));
		const auto* at =
		    site != nullptr && registry.Valid(site->totem) ? registry.TryGet<const Transform>(site->totem) : nullptr;
		if (at != nullptr)
		{
			altar = at->position;
		}
	});
	return altar;
}

/// What the agenda knows for the sacrifice: whether the creature's hand holds a living thing, and where the altar of
/// its temple's first worship site stands. Whether its player has a temple is SituateHome's.
void SituateSacrifice(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature);
		situation.holdingLiving =
		    held.has_value() && registry.Valid(*held) && registry.AnyOf<Villager, Animal, Creature>(*held);
	}
	situation.sacrificeAltar = SacrificeAltarOf(registry, body->owner);
}

/// The storage pit of the nearest town of a player to a place, by the distance across the land. None when the player
/// has no town, or when the nearest town has no storage pit.
std::optional<uint32_t> OwnTownPitOf(const ecs::Registry& registry, PlayerNames player, glm::vec3 from)
{
	std::vector<entt::entity> towns;
	std::vector<float> metres;
	registry.Each<const Town, const Transform>([&](entt::entity town, const Town& data, const Transform& at) {
		if (data.owner == player)
		{
			towns.push_back(town);
			metres.push_back(gutils::GetDistanceInMetres(at.position, from));
		}
	});
	const auto nearest = creature_town_theft::NearestTown(metres);
	if (!nearest.has_value())
	{
		return std::nullopt;
	}
	const auto pit = ecs::town_queries::GetStoragePit(towns.at(*nearest));
	return pit != entt::null ? std::optional(entt::to_integral(pit)) : std::nullopt;
}

/// What the agenda knows for the theft that takes a thing to a town: the storage pit of its player's nearest town
void SituateTownTheft(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body != nullptr && at != nullptr)
	{
		situation.ownTownPit = OwnTownPitOf(registry, body->owner, at->position);
	}
}

/// What eating from a storage pit, a field or a tree needs to know: the pit's food pile and where it is, whether its
/// hand holds something it can eat, and how near it goes to a tree
void SituateEatingFrom(const ecs::Registry& registry, entt::entity creature, creature_plan_actions::Build build,
                       std::optional<uint32_t> object, creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	if (!object.has_value())
	{
		return;
	}
	const auto thing = static_cast<entt::entity>(*object);
	if (build == Build::EatFromStoragePit)
	{
		const auto* pit = registry.TryGet<const StoragePit>(thing);
		const auto pile = pit != nullptr ? pit->foodPile : entt::null;
		const auto at = pile != entt::null && registry.Valid(pile) ? PointOf(registry, pile) : std::nullopt;
		if (at.has_value())
		{
			situation.pitPile = entt::to_integral(pile);
			situation.pitPilePoint = *at;
		}
		return;
	}
	if (build != Build::EatFromField && build != Build::EatFromTree)
	{
		return;
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	situation.holdingEatable = held != nullptr && held->object != entt::null && registry.Valid(held->object) &&
	                           mind_detail::CanBeEaten(registry, creature, held->object);
	if (build == Build::EatFromTree)
	{
		situation.portionApproach =
		    creature_eating::PileApproach(ecs::object::Get2DRadius(thing), ecs::object::Get2DRadius(creature));
	}
}

/// What the plan actions with its player need to know of the player: where the nearest of the player's hands is, and
/// whether that hand moves and is away from the creature; whether the player's camera looks at it; and its attitude to
/// its player. Only the local player's hands and camera are here
void SituatePlayer(const ecs::Registry& registry, entt::entity self, creature_plan_actions::Situation& situation)
{
	const auto* body = registry.TryGet<const Creature>(self);
	const auto* at = registry.TryGet<const Transform>(self);
	const auto* mind = registry.TryGet<const CreatureMindState>(self);
	if (mind != nullptr)
	{
		situation.attitudeToPlayer = mind->attitudeToPlayer;
	}
	if (body == nullptr || at == nullptr || body->owner != creature::LocalPlayer())
	{
		return;
	}
	if (Locator::handSystem::has_value())
	{
		for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
		{
			if (hand.has_value() && (!situation.playerHand.has_value() ||
			                         glm::distance(*hand, at->position) < glm::distance(*situation.playerHand, at->position)))
			{
				situation.playerHand = hand;
			}
		}
	}
	if (Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		situation.lookedAt = creature_attention::IsLookingAt(camera.GetOrigin(), camera.GetFocus(), at->position);
		if (situation.playerHand.has_value())
		{
			situation.awayFromHand = creature_player_attention::AwayFromHand(at->position, *situation.playerHand,
			                                                                 camera.GetOrigin(), camera.GetFocus());
		}
	}
	if (mind != nullptr)
	{
		situation.handMoving = situation.playerHand.has_value() && creature_player_attention::IsHandMoving(mind->handSpeed);
	}
}

/// The actions with its player that first follow the camera, which stop by the creature's height
bool FollowsTheCamera(creature_plan_actions::Build build)
{
	using creature_plan_actions::Build;
	return build == Build::GoToMiddleOfScreen || build == Build::FollowPlayer || build == Build::BeCrossWithPlayer ||
	       build == Build::ShowPlayerAnObject;
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

/// How pressing the reaction the creature is taken up with is: its row's priority in the game's table, 0 with none
float ReactionPriorityOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* reaction = registry.TryGet<const LivingReaction>(creature);
	if (reaction == nullptr || reaction->type == openblack::Reaction::None || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& table = Locator::infoConstants::value().reaction;
	const auto row = static_cast<size_t>(reaction->type);
	return row < table.size() ? static_cast<float>(table.at(row).priority) : 0.0f;
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

std::optional<creature_planner::Plan> mind_detail::HeldPlan(const CreatureMindState& mind,
                                                            const creature_mind_tables::Tables* tables)
{
	if (mind.planner.current.has_value())
	{
		return mind.planner.current;
	}
	if (mind.planActive)
	{
		return std::nullopt;
	}
	const auto row = CurrentAction(mind, tables);
	if (!row.has_value() || tables == nullptr || *row >= tables->actions.size())
	{
		return std::nullopt;
	}
	creature_planner::Plan plan {.action = *row};
	if (const auto desire = tables->actions.at(*row).desire; desire < creature_desires::k_DesireCount)
	{
		plan.desire = static_cast<creature_desires::Desire>(desire);
	}
	plan.object = mind.idle.agenda.at(mind.idle.step).object;
	return plan;
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
		const auto score = Usefulness(learnt, TreeKind::Goal, Desire::Compassion, BeliefOf(registry, entity, self)) *
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

/// A plan carried out to its end: the body pays for its action, unless a step's effect charged it already, then its
/// desires take what finishing it does to them
void Complete(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind, const creature_planner::Plan& plan,
              const creature_mind_tables::Tables& tables)
{
	const auto& action = tables.actions[plan.action];
	mind.lastCompletedAction = plan.action;
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
	// An action that ends well on a creature makes it find that creature nicer or nastier, by the game's table. An
	// action on no thing acts on the creature itself (its plan's thing is then its own), so it also changes how nice
	// it finds itself, a quirk of the game
	const auto other = plan.object.has_value() ? static_cast<entt::entity>(*plan.object) : creature;
	if (action.creatureNicenessChange != 0.0f && registry.Valid(other) && registry.AllOf<Creature>(other))
	{
		mind.leash.attitudes.push_back({.creature = entt::to_integral(other), .change = action.creatureNicenessChange});
	}
}
} // namespace

float mind_detail::DesireFloorOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	return creature_spell_mind::SpeciesDesireFloor(body->species, Locator::infoConstants::value().creature);
}

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

namespace
{
/// How tall a creature is drawn: the size it is drawn at, in the game's height per unit of size
float DrawnHeight(const ecs::Registry& registry, entt::entity creature)
{
	return ecs::creature_pose::DrawnSize(registry, creature) * ecs::object::k_CreatureHeightPerScale;
}
} // namespace

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
		belief.Set(Attribute::Life, creature_tree::LifeValue(villager->life));
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
		belief.Set(Attribute::Life, creature_tree::LifeValue(ecs::life::LifeOf(entity)));
		return belief;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const bool mine = me != nullptr && me->owner == creature->owner;
		common(types::k_Creature, mine ? 0 : 2, 0, 1, PlayerNumberOf(creature->owner));
		// It is harder than the believer when it is drawn taller than the believer's player's creature, which is
		// usually the believer itself; a believer of no player compares with none
		std::optional<float> playersCreatureHeight;
		if (me != nullptr && me->owner < PlayerNames::_COUNT && Locator::playerSystem::has_value())
		{
			if (const auto theirs = Locator::playerSystem::value().GetPrimaryCreature(me->owner);
			    theirs.has_value() && registry.Valid(*theirs) && registry.AllOf<Creature>(*theirs))
			{
				playersCreatureHeight = DrawnHeight(registry, *theirs);
			}
		}
		belief.Set(Attribute::HarderThanMe,
		           creature_tree::HarderThanMeValue(DrawnHeight(registry, entity), playersCreatureHeight));
		belief.Set(Attribute::CreatureType, static_cast<uint32_t>(creature::InfoRow(creature->species)));
		belief.Set(Attribute::Height, creature_tree::CreatureHeightValue(ecs::creature_pose::DrawnSize(registry, entity)));
		// What it knows of another creature: how many miracles it knows, the kind of thing it carries, and the desire
		// that the action of its plan, or of its last plan, shows
		const auto* mind = registry.TryGet<const CreatureMindState>(entity);
		const size_t miracles = mind != nullptr && mind->learnt.has_value()
		                            ? static_cast<size_t>(std::ranges::count(mind->learnt->knowledge.miraclesKnown, true))
		                            : 0;
		belief.Set(Attribute::SpellKnowledge, creature_tree::SpellKnowledgeValue(miracles));
		std::optional<uint32_t> carried;
		if (const auto* held = registry.TryGet<const CreatureHeldObject>(entity);
		    held != nullptr && registry.Valid(held->object))
		{
			if (const auto heldBelief = mind_detail::BeliefOf(registry, held->object, self))
			{
				carried = heldBelief->Value(Attribute::Type);
			}
		}
		belief.Set(Attribute::Carrying, creature_tree::CarryingValue(carried));
		uint32_t shown = 0;
		if (const auto* tables =
		        Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
		    mind != nullptr && tables != nullptr)
		{
			// The action it carries out now: its plan's, or what its idle mind does; none once a plan has ended
			const auto held = mind_detail::HeldPlan(*mind, tables);
			if (held.has_value() && held->action < tables->actions.size())
			{
				shown = tables->actions[held->action].shownDesire;
			}
		}
		belief.Set(Attribute::DominantDesire, creature_tree::DominantDesireValue(shown));
		return belief;
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		common(types::k_Abode, k_Neutral, 1, 0, k_NoPlayer);
		belief.Set(Attribute::AbodeType, static_cast<uint32_t>(abode->type));
		belief.Set(Attribute::Life, creature_tree::LifeValue(ecs::life::LifeOf(entity)));
		belief.Set(Attribute::OnFire, OnFireValue(ecs::fire::IsOnFire(entity)));
		belief.Set(Attribute::AbodeBeingBuilt, creature_tree::BeingBuiltValue(ecs::abodes::IsBuilt(entity)));
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
		const auto faith =
		    me != nullptr ? creature_town_compassion::ReligiousBeliefIn(town->belief.belief, static_cast<size_t>(me->owner))
		                  : creature_town_compassion::ReligiousBelief(0.0f);
		belief.Set(Attribute::TownReligiousBelief, faith);
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

float mind_detail::Usefulness(const creature_mind_model::Learnt& learnt, TreeKind kind, Desire desire,
                              const std::optional<creature_tree::Belief>& belief)
{
	if (!belief.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto& tree = learnt.trees.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

namespace
{
/// Whether one of two creatures is the friend of the other, as a script makes them
bool AreFriends(const ecs::Registry& registry, entt::entity first, entt::entity second)
{
	const auto lists = [&registry](entt::entity creature, entt::entity other) {
		const auto* friends = registry.TryGet<const CreatureFriends>(creature);
		return friends != nullptr && std::ranges::find(friends->friends, other) != friends->friends.end();
	};
	return lists(first, second) || lists(second, first);
}

/// Whether a creature may be angry with another creature: never with itself, a friend or one knocked out, and only
/// with a species the game's creature table lets creatures attack
bool MayBeAngryWith(const ecs::Registry& registry, entt::entity other, entt::entity creature)
{
	if (other == creature || AreFriends(registry, other, creature) || registry.AllOf<CreatureKnockedOut>(other) ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(registry.Get<const Creature>(other).species);
	return row < rows.size() && rows.at(row).canCreatureAttackMe != 0;
}

/// Whether a desire can be about another creature: the desire to impress is about the towns of other players, anger only
/// about a creature it may be angry with, and every other desire can
bool AppliesToCreature(const ecs::Registry& registry, Desire desire, entt::entity other, entt::entity creature)
{
	switch (desire)
	{
	case Desire::Impress:
		return false;
	case Desire::Anger:
		return MayBeAngryWith(registry, other, creature);
	default:
		return true;
	}
}
} // namespace

std::optional<Desire> mind_detail::DesireAboutCreature(const ecs::Registry& registry, entt::entity creature,
                                                       const CreatureMindState& mind, entt::entity other,
                                                       const creature_mind_tables::Tables& tables)
{
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* otherAt = registry.TryGet<const Transform>(other);
	if (!mind.desires.has_value() || !mind.learnt.has_value() || at == nullptr || otherAt == nullptr ||
	    !registry.AllOf<Creature>(other))
	{
		return std::nullopt;
	}
	// Where the other creature is now: the creature keeps no memory of where it saw it last
	const auto distance = gutils::GetDistanceInMetres(otherAt->position, at->position);
	const auto belief = BeliefOf(registry, other, creature);
	std::vector<creature_planner::DesireOnThing> weighed;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		const auto& state = mind.desires->desires.at(d);
		if (!state.activated || state.suppressedTurns != 0 || !tables.rules.at(d).searchesForGoal ||
		    !AppliesToCreature(registry, desire, other, creature))
		{
			continue;
		}
		const auto nearness = creature_planner::DistancePriority(false, distance, tables.rules.at(d).distanceWeight);
		weighed.push_back({.desire = desire,
		                   .strength = state.value,
		                   .usefulness = Usefulness(*mind.learnt, TreeKind::Goal, desire, belief) * nearness});
	}
	return creature_planner::BestDesireOnThing(weighed);
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
	// The food is what eating is done to
	const auto& tree = mind.learnt->trees.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(Desire::Hunger));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

namespace
{
/// Whether a mobile static is a toy, by its mesh: a toy ball, bowling ball, cuddly toy, dice or skittle
bool IsToyMesh(const MobileStatic& still)
{
	const auto kind = static_cast<size_t>(still.type);
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	const auto mesh = info != nullptr && kind < info->mobileStatic.size() ? info->mobileStatic.at(kind).meshId : MeshId::Dummy;
	return mesh >= MeshId::ObjectToyBall && mesh <= MeshId::ObjectToySkittle;
}

/// What a villager's state holds for a creature to learn by watching, when it holds nothing
constexpr uint32_t k_NothingToLearn = 6;

/// What play and curiosity look at in a thing to act on: a toy (the football, or a mobile static with a toy's mesh), a
/// villager whose state is one of a football match, and a villager whose state shows something the creature could learn
/// by watching and has not learnt. No kind of object is kept as looked over, so none counts as one
creature_planner::ActOnFacts ActOnFactsOf(const ecs::Registry& registry, entt::entity thing,
                                          const creature_mind_model::Learnt& learnt)
{
	creature_planner::ActOnFacts facts;
	if (registry.AllOf<Ball>(thing))
	{
		facts.toy = true;
	}
	else if (const auto* still = registry.TryGet<const MobileStatic>(thing); still != nullptr)
	{
		facts.toy = IsToyMesh(*still);
	}
	const auto* action = registry.TryGet<const LivingAction>(thing);
	if (action == nullptr || !registry.AllOf<Villager>(thing) || !Locator::infoConstants::has_value())
	{
		return facts;
	}
	namespace state_info = ecs::villager::state_info;
	const auto stateAt = [action](LivingAction::Index index) {
		return static_cast<VillagerStates>(action->states.at(static_cast<size_t>(index)));
	};
	// The football match is told by the state it ends in: its top state when that is final, else its final one
	const auto top = stateAt(LivingAction::Index::Top);
	const auto end = state_info::IsFinal(state_info::StateInfo(top)) ? top : stateAt(LivingAction::Index::Final);
	const auto row = static_cast<size_t>(static_cast<uint8_t>(end));
	const auto& rows = ecs::villager::k_StateFunctionIds;
	facts.playingFootball = row < rows.size() && rows.at(row).exit == ecs::villager::k_ExitFootball;
	// What it does now that could be learnt by watching
	const auto skill = state_info::Interesting(state_info::StateInfo(top));
	const auto& known = learnt.knowledge.skillsKnown;
	facts.doingSomethingNew = skill != k_NothingToLearn && !(skill < known.size() && known.at(skill));
	return facts;
}

/// Whether a thing is part of a temple
bool IsTemplePart(const ecs::Registry& registry, entt::entity thing)
{
	return registry.AllOf<CitadelPartBuild>(thing);
}

/// The radius a creature is kept within, when it is kept within an area
std::optional<float> KeptWithin(const ecs::Registry& registry, entt::entity creature)
{
	const auto* kept = registry.TryGet<const CreatureLeash>(creature);
	if (kept == nullptr || !Locator::leashSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& leashes = Locator::leashSystem::value();
	const bool leashed = leashes.IsLeashed(creature);
	if (!creature_leash::IsConfined(kept->confinementRadius, leashed, !leashed || leashes.Works(creature)))
	{
		return std::nullopt;
	}
	return kept->confinementRadius;
}

/// A thing to act on for a desire, as the planner weighs it: what the creature has learnt of it, the distance, whether it
/// is held, whether it is out of reach of a creature kept within an area, and the desire's factor for it
creature_planner::ObjectCandidate ActOnCandidate(const ecs::Registry& registry, entt::entity creature, glm::vec2 position,
                                                 const creature_mind_model::Learnt& learnt, Desire desire, entt::entity thing,
                                                 std::optional<float> keptWithin)
{
	const auto distance = gutils::GetDistanceInMetres(PointOf(registry, thing).value_or(position), position);
	const auto* held = registry.TryGet<const HeldByCreature>(thing);
	const auto belief = BeliefOf(registry, thing, creature);
	return {
	    .id = entt::to_integral(thing),
	    .distance = distance,
	    .held = held != nullptr && held->creature == creature,
	    .usefulness = Usefulness(learnt, TreeKind::ActOn, desire, belief),
	    .goalUsefulness = Usefulness(learnt, TreeKind::Goal, desire, belief),
	    .outOfReach = creature_planner::BeyondKeptArea(desire, keptWithin, distance, IsTemplePart(registry, thing)),
	    .actOnFactor = creature_planner::ActOnFactor(desire, ActOnFactsOf(registry, thing, learnt)),
	};
}
} // namespace

bool mind_detail::CanBeEaten(const ecs::Registry& registry, entt::entity self, entt::entity thing)
{
	// A mobile static unless it is a toy (a toy ball, bowling ball, cuddly toy, dice or skittle, by its mesh)
	if (const auto* still = registry.TryGet<const MobileStatic>(thing); still != nullptr)
	{
		return creature_eating::CanBeEaten({.as = creature_eating::Eaten::AMobileStatic, .isToy = IsToyMesh(*still)});
	}
	// A dead tree is one of the mobile statics too, and keeps its tree's mesh, never a toy's
	if (registry.AllOf<DeadTree>(thing))
	{
		return creature_eating::CanBeEaten({.as = creature_eating::Eaten::AMobileStatic, .isToy = false});
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

namespace
{
/// A creature takes no tree to plant from within this many metres of where the player did what it last set about
/// copying (docs/wiki/project/progress/vanilla_bugs/creature.md, row 67)
constexpr float k_PlantFarFromCopied = 20.0f;
/// A toy further than this from its creature's home is one it may take home
constexpr float k_ToyAwayFromHome = 50.0f;

/// Which toy the thing is: the football is a ball, and of the statics those with a toy's model
std::optional<creature_object_actions::Toy> ToyOf(const ecs::Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Ball>(entity))
	{
		return creature_object_actions::Toy::Ball;
	}
	const auto* still = registry.TryGet<const MobileStatic>(entity);
	if (still == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& statics = Locator::infoConstants::value().mobileStatic;
	const auto kind = static_cast<size_t>(still->type);
	return kind < statics.size() ? creature_object_actions::ToyOf(statics.at(kind).meshId) : std::nullopt;
}

/// Whether the thing is a mushroom: the magic mushroom that grows on the land, or a thing lying about of the magic
/// mushroom or toadstool class
bool IsMushroom(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* feature = registry.TryGet<const Feature>(entity))
	{
		return feature->type == FeatureInfo::MagicMushroom;
	}
	const auto* mobile = registry.TryGet<const MobileObject>(entity);
	if (mobile == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().mobileObject;
	const auto row = static_cast<size_t>(mobile->type);
	if (row >= rows.size())
	{
		return false;
	}
	const auto mobileClass = rows.at(row).mobileType;
	return mobileClass == MobileObjectInfo::MagicMushroom || mobileClass == MobileObjectInfo::Toadstool;
}
} // namespace

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
		// whatever its hands may pick up, of every kind
		return !heldByOther && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::Toy:
		return !heldByOther && ToyOf(registry, entity).has_value() && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::ToyBall:
		return !heldByOther && ToyOf(registry, entity) == creature_object_actions::Toy::Ball;
	case Target::AnyLiving:
		return hands != nullptr && creature_object_actions::IsLiving(hands->KindOf(entity).thing);
	case Target::Stonable:
	{
		const bool dead = (registry.AllOf<Villager>(entity) && ecs::villager::IsDead(entity)) ||
		                  (registry.AllOf<Animal>(entity) && ecs::animal_ai::IsDead(entity));
		return hands != nullptr && creature_object_actions::CanBeStoned(hands->KindOf(entity).thing, dead);
	}
	case Target::ThrowingDamage:
	{
		if (heldByOther || hands == nullptr)
		{
			return false;
		}
		const auto kind = hands->KindOf(entity);
		return creature_object_actions::DoesThrowingDamage(kind.thing, kind.toy) && hands->CanPickUp(self, entity);
	}
	case Target::SeaThrowable:
		return hands != nullptr && creature_object_actions::CanBeThrownInTheSea(hands->KindOf(entity).thing);
	case Target::ThrowableToImpress:
		return !heldByOther && hands != nullptr && hands->CanThrowToImpress(self, entity) && hands->CanPickUp(self, entity);
	case Target::PickableRock:
	case Target::Rock:
	{
		if (hands == nullptr || (target == Target::PickableRock && heldByOther))
		{
			return false;
		}
		const auto thing = hands->KindOf(entity).thing;
		const bool rock = thing == creature_object_actions::Thing::Rock || thing == creature_object_actions::Thing::Bonfire;
		return rock && (target == Target::Rock || hands->CanPickUp(self, entity));
	}
	case Target::CuddlyToy:
		return registry.AllOf<MobileStatic>(entity) && ToyOf(registry, entity) == creature_object_actions::Toy::Cuddly;
	case Target::Die:
		return registry.AllOf<MobileStatic>(entity) && ToyOf(registry, entity) == creature_object_actions::Toy::Die;
	case Target::ToyAwayFromHome:
	{
		// Only a static toy: the football is never taken home. Without a home, none is away from it
		if (!registry.AllOf<MobileStatic>(entity) || !ToyOf(registry, entity).has_value())
		{
			return false;
		}
		const auto home = ecs::creature_home::HomeOf(registry, self);
		const auto at = PointOf(registry, entity);
		return home.has_value() && at.has_value() &&
		       gutils::GetDistanceInMetres(glm::vec2(home->x, home->z), *at) > k_ToyAwayFromHome;
	}
	case Target::NeedsRepair:
		// A town's buildings and its totem say they want repair, while below their full life
		return creature_plan_actions::NeedsRepair(registry.AllOf<Abode>(entity), registry.AllOf<Temple>(entity),
		                                          registry.AllOf<VillageTotem>(entity), ecs::life::LifeOf(entity));
	case Target::PickableTree:
		return !heldByOther && registry.AllOf<Tree>(entity) && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::PlantableTree:
	{
		// Not one it planted another by lately, nor one near where the player did what it last set about copying
		const auto* mind = registry.TryGet<const CreatureMindState>(self);
		const auto at = PointOf(registry, entity);
		if (heldByOther || !registry.AllOf<Tree>(entity) || hands == nullptr || mind == nullptr || !at.has_value() ||
		    std::ranges::find(mind->plantedBy, entt::to_integral(entity)) != mind->plantedBy.end() ||
		    !(gutils::GetDistanceInMetres(*at, mind->copiedAt) > k_PlantFarFromCopied))
		{
			return false;
		}
		return hands->CanPickUp(self, entity);
	}
	case Target::BringableHome:
	{
		// More than 50 m from the creature's home across the land, not alive, and a thing it can pick up
		const auto home = ecs::creature_home::HomeOf(registry, self);
		const auto at = PointOf(registry, entity);
		if (heldByOther || hands == nullptr || !home.has_value() || !at.has_value())
		{
			return false;
		}
		const auto metres = gutils::GetDistanceInMetres(*at, glm::vec2(home->x, home->z));
		return creature_bring_home::CanBeBroughtHome(metres, registry.AnyOf<Villager, Animal, Creature>(entity),
		                                             hands->CanPickUp(self, entity));
	}
	case Target::Destroyable:
		return hands != nullptr && hands->CanDestroy(entity);
	case Target::Tree:
		return registry.AllOf<Tree>(entity) && hands != nullptr && hands->CanDestroy(entity);
	case Target::Villager:
		return registry.AllOf<Villager>(entity);
	case Target::DanceVillager:
	{
		// Measured across the land between map positions, as the game measures it
		const auto* at = registry.TryGet<const Transform>(entity);
		const auto* from = registry.TryGet<const Transform>(self);
		return registry.AllOf<Villager>(entity) && at != nullptr && from != nullptr &&
		       gutils::GetDistanceInMetres(from->position, at->position) < creature_dance::k_FurthestVillager;
	}
	case Target::HurtVillager:
		// Hurt to seven tenths of its life or less, or with a life that is no number
		return registry.AllOf<Villager>(entity) && !(ecs::life::LifeOf(entity) > k_HurtEnoughToHeal);
	case Target::Creature:
		return registry.AllOf<Creature>(entity);
	case Target::CreatureFreeForJoint:
		return registry.AllOf<Creature>(entity) && !registry.AllOf<CreaturePartner>(entity);
	case Target::CreatureNotFreeForJoint:
		return registry.AllOf<Creature, CreaturePartner>(entity);
	case Target::FriendlyCreature:
		// A creature that seems friendly, to amuse with a spell
		return registry.AllOf<Creature>(entity) && Locator::creatureMindSystem::has_value() &&
		       Locator::creatureMindSystem::value().SeemsFriendly(self, entity);
	case Target::Living:
		return registry.AnyOf<Villager, Animal, Creature>(entity);
	case Target::Frightening:
		return registry.AnyOf<Creature, ecs::components::Spell>(entity) ||
		       (registry.AllOf<Animal>(entity) && Locator::animalSystem::has_value() &&
		        Locator::animalSystem::value().IsFrighteningToCreature(entity));
	case Target::Anything:
		return true;
	case Target::Examinable:
		// a landscape vortex, a field, a fish farm or a miracle at work is no thing to look at, whatever its kind
		return hands != nullptr && creature_object_actions::CanBeExaminedByLooking(hands->KindOf(entity).thing) &&
		       !registry.AnyOf<Field, FishFarm, LandscapeVortex, ecs::components::Spell>(entity);
	case Target::Mushroom:
		return !heldByOther && IsMushroom(registry, entity);
	case Target::Playable:
		// A villager or an animal is played with only alive (the object types' own "may be played with" flag is
		// not read yet)
		if (registry.AllOf<Villager>(entity))
		{
			return !ecs::villager::IsDead(entity);
		}
		return !registry.AllOf<Animal>(entity) || !ecs::animal_ai::IsDead(entity);
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
	case Target::TemplePart:
		// The temple of any player, a worship site of any town, or the altar at the head of a worship site
		return registry.AnyOf<Temple, WorshipSite, WorshipTotem>(entity);
	case Target::FieldWithFood:
	{
		const auto* field = registry.TryGet<const Field>(entity);
		return field != nullptr && creature_eating::FieldHasFood(field->growth, field->food);
	}
	case Target::TreeBigEnough:
		// Tall enough for the creature's drawn size, as for eating from it
		return registry.AllOf<Tree>(entity) &&
		       creature_eating::TreeTallEnough(object_measures::Height(registry, entity),
		                                       creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, self)));
	case Target::BuiltStoragePit:
		return registry.AllOf<StoragePit>(entity) && ecs::abodes::IsBuilt(entity);
	case Target::OwnStoragePit:
	case Target::OthersStoragePit:
	{
		// A pit's player is its town's; a creature with no player has none
		const auto* me = registry.TryGet<const Creature>(self);
		if (!registry.AllOf<StoragePit>(entity) || me == nullptr)
		{
			return false;
		}
		const bool built = ecs::abodes::IsBuilt(entity);
		const auto pitPlayer = TownPlayerOf(registry, ecs::abode_villagers::TownOf(entity));
		const auto creaturePlayer = std::optional(PlayerNumberOf(me->owner));
		return target == Target::OwnStoragePit ? creature_town_theft::IsOwnStoragePit(built, pitPlayer, creaturePlayer)
		                                       : creature_town_theft::IsOthersStoragePit(built, pitPlayer, creaturePlayer);
	}
	case Target::OthersField:
	{
		const auto* me = registry.TryGet<const Creature>(self);
		return registry.AllOf<Field>(entity) && me != nullptr &&
		       creature_town_theft::IsOthersField(TownPlayerOf(registry, ecs::fields::TownOf(entity)),
		                                          PlayerNumberOf(me->owner));
	}
	case Target::MagicWoodPile:
	case Target::Cow:
	case Target::VillagerNotWorshipping:
	case Target::BreederVillager:
	case Target::VillagerFarFromHome:
	case Target::PlayerTotem:
		return mind_town_help::Accepts(registry, target, entity, self, heldByOther);
	case Target::Stealable:
		return !heldByOther && CanSteal(registry, self, entity);
	case Target::StoragePitWithFood:
	{
		// Not too wide for the creature's drawn size, with its food pile there, not on its way out, and holding food
		const auto* pit = registry.TryGet<const StoragePit>(entity);
		if (pit == nullptr)
		{
			return false;
		}
		const auto pile = pit->foodPile;
		const bool hasPile = pile != entt::null && registry.Valid(pile);
		const auto* food = hasPile ? registry.TryGet<const Pot>(pile) : nullptr;
		return creature_eating::StoragePitHasFood(
		    ecs::object::Get2DRadius(entity), creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, self)),
		    food != nullptr, food != nullptr && !ecs::IsAvailable(pile), food != nullptr ? food->amount : 0u);
	}
	case Target::Burning:
		// Anything on fire; nothing without the fires
		return Burning(entity);
	}
	return false;
}

bool mind_detail::CanSteal(const ecs::Registry& registry, entt::entity creature, entt::entity thing)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (body == nullptr || transform == nullptr || !Locator::creatureObjectActionSystem::has_value())
	{
		return false;
	}
	return creature_steal::IsStealable({
	    .toy = ToyOf(registry, thing).has_value(),
	    .scaffold = registry.AllOf<Scaffold>(thing),
	    .oneShot = registry.AllOf<OneOffSpellSeed>(thing),
	    .canPickUp = Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing),
	    .ownInfluence = influence::CalculatePlayerRawInfluence(body->owner, transform->position),
	});
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
	// Anything at all is whatever it may come to know of: the living, what lies about, statics, features, trees and
	// buildings; what it may look at is found among the same things, what may be played with too, judged by the
	// Playable test, and what burns, with the fields
	const bool anything =
	    target == Target::Anything || target == Target::Examinable || target == Target::Playable || target == Target::Burning;
	const bool sea = target == Target::SeaThrowable;
	// Of pots and piles, food is eaten, and any may be picked up or thrown into the sea
	const bool pickable = target == Target::Pickable || target == Target::ThrowableToImpress;
	if (target == Target::Food || target == Target::FoodPile || pickable || sea || anything || target == Target::BringableHome)
	{
		std::as_const(registry).Each<const Pot, const Transform>(
		    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	}
	// Whatever else it may pick up: the football, statics and rocks, dead trees, trees and mushrooms. What it may
	// steal: the toys among them, the scaffolds (mobile objects) and the one-shot miracles' globes
	const bool stealable = target == Target::Stealable;
	if (pickable || target == Target::Toy || target == Target::ToyBall || sea || anything || stealable)
	{
		std::as_const(registry).Each<const Ball, const Transform>(
		    [&](entt::entity entity, const Ball&, const Transform& at) { consider(entity, at); });
	}
	const bool statics = pickable || target == Target::Toy || target == Target::ToyBall || sea || target == Target::Stonable ||
	                     target == Target::ThrowingDamage || target == Target::PickableRock || target == Target::Rock ||
	                     target == Target::CuddlyToy || target == Target::Die || target == Target::ToyAwayFromHome ||
	                     anything || stealable;
	if (statics)
	{
		std::as_const(registry).Each<const MobileStatic, const Transform>(
		    [&](entt::entity entity, const MobileStatic&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const DeadTree, const Transform>(
		    [&](entt::entity entity, const DeadTree&, const Transform& at) { consider(entity, at); });
	}
	if (stealable)
	{
		std::as_const(registry).Each<const OneOffSpellSeed, const Transform>(
		    [&](entt::entity entity, const OneOffSpellSeed&, const Transform& at) { consider(entity, at); });
	}
	if (pickable || target == Target::Stonable || anything || target == Target::Mushroom)
	{
		std::as_const(registry).Each<const Feature, const Transform>(
		    [&](entt::entity entity, const Feature&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || target == Target::Tree || anything || target == Target::Sprinklable ||
	    target == Target::AnyTree || target == Target::Examinable || pickable || sea || target == Target::PickableTree ||
	    target == Target::PlantableTree || target == Target::TreeBigEnough)
	{
		std::as_const(registry).Each<const Tree, const Transform>(
		    [&](entt::entity entity, const Tree&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || anything || target == Target::Stonable || target == Target::Examinable)
	{
		std::as_const(registry).Each<const Abode, const Transform>(
		    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	}
	// What else is built or set into the land, which thrown things can knock about
	if (target == Target::Stonable || anything)
	{
		std::as_const(registry).Each<const Temple, const Transform>(
		    [&](entt::entity entity, const Temple&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const WorshipSite, const Transform>(
		    [&](entt::entity entity, const WorshipSite&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const VillageTotem, const Transform>(
		    [&](entt::entity entity, const VillageTotem&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const AnimatedStatic, const Transform>(
		    [&](entt::entity entity, const AnimatedStatic&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const Flowers, const Transform>(
		    [&](entt::entity entity, const Flowers&, const Transform& at) { consider(entity, at); });
	}
	// Storage pits, workshops and buildings going up are abodes; worship sites take food and may be going up too
	if (target == Target::FoodStore || target == Target::WoodStore || target == Target::BuildingSite ||
	    target == Target::Workshop || target == Target::BuiltStoragePit || target == Target::StoragePitWithFood ||
	    target == Target::OwnStoragePit || target == Target::OthersStoragePit || target == Target::NeedsRepair)
	{
		std::as_const(registry).Each<const Abode, const Transform>(
		    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	}
	// A town's totem may want repair as its buildings do
	if (target == Target::NeedsRepair)
	{
		std::as_const(registry).Each<const VillageTotem, const Transform>(
		    [&](entt::entity entity, const VillageTotem&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::FoodStore || target == Target::WorshipSite || target == Target::BuildingSite ||
	    target == Target::TemplePart)
	{
		std::as_const(registry).Each<const WorshipSite, const Transform>(
		    [&](entt::entity entity, const WorshipSite&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::TemplePart)
	{
		std::as_const(registry).Each<const Temple, const Transform>(
		    [&](entt::entity entity, const Temple&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const WorshipTotem, const Transform>(
		    [&](entt::entity entity, const WorshipTotem&, const Transform& at) { consider(entity, at); });
	}
	// Things lying about set into the land (rocks among them) are only ever brought home
	if (target == Target::BringableHome)
	{
		std::as_const(registry).Each<const MobileStatic, const Transform>(
		    [&](entt::entity entity, const MobileStatic&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Sprinklable || target == Target::DryField || target == Target::FieldWithFood ||
	    target == Target::OthersField || target == Target::Burning)
	{
		std::as_const(registry).Each<const Field, const Transform>(
		    [&](entt::entity entity, const Field&, const Transform& at) { consider(entity, at); });
	}
	mind_town_help::Gather(registry, target, consider);
	// A thing found in two of the lists counts once
	std::ranges::sort(found, {}, [](const Found& f) { return entt::to_integral(f.entity); });
	const auto [first, last] = std::ranges::unique(found, {}, &Found::entity);
	found.erase(first, last);
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

std::optional<creature_planner::Plan> CreatureMindSystem::HeldPlanOf(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (creature == entt::null || !registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr ? mind_detail::HeldPlan(*mind, GetTables()) : std::nullopt;
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
	// It takes the name the file keeps, unless a computer player owns it: that one keeps the name it was given
	if (!creature_names::TakesNameFromFile(creature_naming::OwnerOf(body->owner)))
	{
		mind.learnt->name.clear();
	}
	mind.developmentPhase = std::min(loaded.developmentPhase, CreatureMindState::k_FullyGrownUp);
	// The file's desires are active as it says, for the stage it gives
	mind.desiresPhase = mind.developmentPhase;
	mind.attitudeToPlayer = loaded.attitudeToPlayer;
	mind.turnsLookedAt = loaded.turnsLookedAt;
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
	creature_planner::StopPlan(mind.planner);
	// An emergency it was in is over
	mind.emergencyRunning = false;
	// Its agenda cleared, the pass over its desires starts again, and nothing it does has achieved its aim yet
	if (mind.desires.has_value())
	{
		mind.desires->achieved = false;
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(*mind.desires));
	}
}

void CreatureMindSystem::LoseGoneThings(entt::entity creature, CreatureMindState& mind)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& idle = mind.idle;
	// When a thing that the creature uses goes before the creature completes the work, the creature stops and decides
	// again
	creature_lost_thing::InUse inUse {.stepThings = creature_lost_thing::StepThings(idle.agenda, idle.step)};
	if (mind.planActive && mind.planner.current.has_value() && idle.step < idle.agenda.size())
	{
		inUse.planThings = creature_lost_thing::PlanThings(*mind.planner.current);
	}
	// What it eats goes as the meal ends: it does not lose that
	if (const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	    action != nullptr && action->kind == creature_object_actions::Kind::Eat && action->target.has_value())
	{
		inUse.usedByItself.push_back(entt::to_integral(*action->target));
	}
	const auto gone = [&registry](uint32_t thing) { return !registry.Valid(static_cast<entt::entity>(thing)); };
	if (!creature_lost_thing::Lost(inUse, gone))
	{
		return;
	}
	// A frightened creature that loses what it fled from wants to flee least of all
	if (mind.planActive && mind.planner.current.has_value() && mind.planner.current->desire == Desire::Fear &&
	    mind.desires.has_value())
	{
		creature_learning::MakeLeastDominant(*mind.desires, Desire::Fear, creature_learning::k_LeastDominantFactor,
		                                     DesireFloorOf(registry, creature));
	}
	// The advisor tells the player that it lost the thing
	TellHelp(creature, {.kind = creature_help::Kind::FailureExplanation, .parameter = creature_help::k_LostThing});
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} lost a thing that it used", entt::to_integral(creature));
	Abandon(mind);
	idle.step = idle.agenda.size();
	idle.stepStarted = false;
	idle.gaveUp = true;
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
		// The scripts count it as its action carried out, once, even when a step of its agenda completed it already
		if (carriedOut)
		{
			creature_training::CountCarriedOut(mind.actionCounts, plan.action,
			                                   ecs::script_held::IsControlledByScript(creature));
		}
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
	// What the idle mind started is over once its agenda runs out: carried out to its end, its action counts
	if (mind.underway.has_value() && (mind.planActive || mind.underway->serial != idle.serial))
	{
		mind.underway.reset();
	}
	if (mind.underway.has_value() && idle.step >= idle.agenda.size())
	{
		if (!idle.gaveUp)
		{
			creature_training::CountCarriedOut(mind.actionCounts, mind.underway->action,
			                                   ecs::script_held::IsControlledByScript(creature));
		}
		mind.underway.reset();
	}
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
	RememberAction(
	    creature, mind,
	    {
	        .action = *action,
	        .desire = static_cast<Desire>(info.desire),
	        .object = object,
	        .belief = object.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*object), creature) : std::nullopt,
	        .learnable = info.learnable,
	        .windowSeconds = info.learningWindowSeconds,
	        .source = mind.desires.has_value()
	                      ? creature_learning::PlanSource((*mind.desires)[static_cast<Desire>(info.desire)])
	                            .value_or(creature_learning::k_NoTellingSource)
	                      : creature_learning::k_NoTellingSource,
	    });
	mind.learnt->turnsSinceDone.at(*action) = 0;
	mind.underway = CreatureMindState::ActionUnderway {.serial = idle.serial, .action = *action};
}

namespace
{
/// Whether a creature kept within an area by its leash has the thing farther from it than the area's radius
bool OutsideConfinement(const ecs::Registry& registry, entt::entity creature, entt::entity object)
{
	const auto* kept = registry.TryGet<const CreatureLeash>(creature);
	if (kept == nullptr || !Locator::leashSystem::has_value())
	{
		return false;
	}
	const auto& leashes = Locator::leashSystem::value();
	const bool leashed = leashes.IsLeashed(creature);
	if (!creature_leash::IsConfined(kept->confinementRadius, leashed, !leashed || leashes.Works(creature)))
	{
		return false;
	}
	const auto from = PointOf(registry, creature);
	const auto at = PointOf(registry, object);
	return from.has_value() && at.has_value() && gutils::GetDistanceInMetres(*at, *from) > kept->confinementRadius;
}

/// The thing a creature at a point uses for an action, of a kind, other than the goal: nothing farther than the area its
/// leash keeps it within, nor another creature
std::optional<uint32_t> UsedFor(const ecs::Registry& registry, entt::entity creature, Target use, glm::vec2 position,
                                float distanceWeight, std::optional<uint32_t> goal)
{
	std::vector<creature_planner::ObjectCandidate> candidates;
	for (const auto& found : mind_detail::Gather(registry, use, creature, position))
	{
		if (registry.AllOf<Creature>(found.entity) || OutsideConfinement(registry, creature, found.entity))
		{
			continue;
		}
		const auto* held = registry.TryGet<const HeldByCreature>(found.entity);
		candidates.push_back({
		    .id = entt::to_integral(found.entity),
		    .distance = gutils::GetDistanceInMetres(found.point, position),
		    .held = held != nullptr && held->creature == creature,
		});
	}
	return creature_planner::ChooseUsed(candidates, distanceWeight, goal);
}

/// What the agenda knows of the creature's hands and where it stands: what it holds (a tree or not), where it is, and
/// where the thing it uses is and its radius on the ground; for the actions done with things, its height and radius
/// too. Whether its hand is full stays as the caller says
void SituateHands(const ecs::Registry& registry, entt::entity creature, const creature_plan_actions::Executor& executor,
                  creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	switch (executor.build)
	{
	case Build::Hurl:
	case Build::GiveWoodToStore:
	case Build::PlantTree:
	case Build::PracticeThrow:
	case Build::ThrowToy:
	case Build::BreakRock:
	case Build::ThrowDie:
	case Build::TakeToyHome:
		situation.height = object_measures::Height(registry, creature);
		situation.radius = ecs::object::Get2DRadius(creature);
		break;
	default:
		break;
	}
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
	                      : std::nullopt;
	situation.held = held.has_value() ? std::optional(entt::to_integral(*held)) : std::nullopt;
	if (held.has_value())
	{
		const auto thing = Locator::creatureObjectActionSystem::value().KindOf(*held).thing;
		situation.holdingTree = thing == creature_object_actions::Thing::Tree;
		situation.holdingMushroom = IsMushroom(registry, *held);
		// Whether it holds food goes by the same test as what it may eat, not by a food value: an object it may pick up
		// (the one in its hand passes that test), a mobile static that is not a toy, never a creature
		situation.holdingEatable = mind_detail::CanBeEaten(registry, creature, *held);
	}
	if (const auto at = PointOf(registry, creature))
	{
		situation.position = *at;
	}
	if (situation.instrument.has_value())
	{
		const auto used = static_cast<entt::entity>(*situation.instrument);
		situation.instrumentPoint = PointOf(registry, used).value_or(glm::vec2(0.0f));
		situation.instrumentRadius = registry.Valid(used) ? ecs::object::Get2DRadius(used) : 0.0f;
	}
}
} // namespace

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
		const auto& registry = Locator::entitiesRegistry::value();
		const auto target = static_cast<entt::entity>(*object);
		cast->atNeedsSign = registry.AllOf<ShowNeedsVisuals>(target);
		// Living as Target::Living has it: a villager, an animal or a creature
		cast->targetLiving = registry.AnyOf<Villager, Animal, Creature>(target);
		// A forest goes to the place where the thing is now
		if (executor->build == creature_plan_actions::Build::CastForest)
		{
			cast->place = object_measures::PositionOf(registry, target);
		}
	}
	auto local = situation;
	SituateExploration(creature, *executor, local);
	// The rest and home actions, the actions with its player that first follow the camera, and looking for a friend,
	// go near by the creature's own height
	if (creature_plan_actions::WalksByItsHeight(executor->build) || FollowsTheCamera(executor->build) ||
	    executor->build == creature_plan_actions::Build::LookForFriend)
	{
		local.height = object_measures::Height(Locator::entitiesRegistry::value(), creature);
	}
	if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
	{
		SituateDance(creature, *executor, point, local);
	}
	// Sleeping on the spot: a clear place about itself four times as wide as it is tall, with nothing living in it
	// unless a script controls it; it walks to within its 2D radius of it, else home to within its height
	if (executor->build == creature_plan_actions::Build::Sleep)
	{
		const auto& registry = Locator::entitiesRegistry::value();
		local.height = object_measures::Height(registry, creature);
		local.radius = ecs::object::Get2DRadius(creature);
		if (const auto* at = registry.TryGet<const Transform>(creature))
		{
			local.clearArea = FindClearArea(registry, creature, glm::vec2(at->position.x, at->position.z),
			                                creature_mind::k_SleepOnTheSpotHeights * local.height,
			                                !ecs::script_held::IsControlledByScript(creature));
		}
	}
	// Sleeping by a thing: the same clear place, about the thing
	if (executor->build == creature_plan_actions::Build::SleepByObject && object.has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		local.height = object_measures::Height(registry, creature);
		local.radius = ecs::object::Get2DRadius(creature);
		local.clearArea = FindClearArea(registry, creature, point, creature_mind::k_SleepOnTheSpotHeights * local.height,
		                                !ecs::script_held::IsControlledByScript(creature));
	}
	SituateHands(Locator::entitiesRegistry::value(), creature, *executor, local);
	mind_town_help::Situate(Locator::entitiesRegistry::value(), creature, *executor, object, local);
	// Holding a thing up to its player, it plays with it the first two times it holds a thing of the kind, as its
	// learning counts them by the object type (kept in its mind file); a thing that is not an object of the map has no
	// kind and is not played with
	std::optional<size_t> heldKind;
	if (executor->build == creature_plan_actions::Build::HoldObject && object.has_value())
	{
		const auto kind = static_cast<int>(ecs::map_cells::TypeOf(static_cast<entt::entity>(*object)));
		const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature);
		if (mind != nullptr && mind->learnt.has_value() && kind >= 0 &&
		    static_cast<size_t>(kind) < mind->learnt->objectTypes.size())
		{
			heldKind = static_cast<size_t>(kind);
			local.playsWithHeld = mind->learnt->objectTypes.at(*heldKind).heldPlays < creature_mind::k_HeldPlaysPerKind;
		}
	}
	// Stealing picks the thing up only with an empty hand
	if (executor->build == creature_plan_actions::Build::StealAndPutByTemple ||
	    executor->build == creature_plan_actions::Build::StealAndPutInTown)
	{
		local.handFull = Locator::entitiesRegistry::value().AllOf<CreatureHeldObject>(creature);
	}
	// A joint action with a friend: what lets it start, the friend, and the place or the town the two go to
	if (object.has_value())
	{
		SituateFriend(creature, *executor, static_cast<entt::entity>(*object), local);
	}
	// Eating from a food pile or a tree, it goes to within the two radii and a share more
	if (creature_plan_actions::ApproachesByRadii(executor->build) && object.has_value())
	{
		const auto source = static_cast<entt::entity>(*object);
		local.portionApproach =
		    creature_eating::PileApproach(ecs::object::Get2DRadius(source), ecs::object::Get2DRadius(creature));
	}
	const bool drinks = executor->build == creature_plan_actions::Build::Drink;
	if (drinks)
	{
		local.water = DrinkPlaceOf(creature);
	}
	SituateEatingFrom(Locator::entitiesRegistry::value(), creature, executor->build, object, local);
	auto agenda = creature_plan_actions::Agenda(
	    *executor, object, point, local, [](uint32_t range) { return Random(range); }, cast);
	if (!agenda.has_value())
	{
		// The creature that finds no place to drink does not want water or to get colder for a time
		if (drinks)
		{
			if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
			    mind != nullptr && mind->desires.has_value())
			{
				creature_drink::NoPlaceFound(*mind->desires, k_TurnsPerSecond);
			}
		}
		return std::nullopt;
	}
	// Planting by a tree, it remembers that tree, and won't take it to plant elsewhere for a while
	auto& registry = Locator::entitiesRegistry::value();
	// Having planned to play with a held thing, it counts the play for the thing's kind
	if (heldKind.has_value() && local.playsWithHeld)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr && mind->learnt.has_value())
		{
			++mind->learnt->objectTypes.at(*heldKind).heldPlays;
		}
	}
	if (executor->build == creature_plan_actions::Build::PlantTree && object.has_value() &&
	    registry.AllOf<Tree>(static_cast<entt::entity>(*object)))
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature))
		{
			mind->plantedBy.push_back(*object);
			if (mind->plantedBy.size() > creature_mind::k_PlantedRemembered)
			{
				mind->plantedBy.erase(mind->plantedBy.begin());
			}
		}
	}
	// Setting off to take a stolen thing to a town, it comes to believe in that town's storage pit
	if (executor->build == creature_plan_actions::Build::StealAndPutInTown && local.ownTownPit.has_value())
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature))
		{
			ComeAcross(creature, *mind, static_cast<entt::entity>(*local.ownTownPit));
		}
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
	auto& registry = Locator::entitiesRegistry::value();
	// An action done with something takes up the thing to use too, as the plan found it or, for a plan made otherwise
	// (copying the player, doing it again), as the planner would
	auto chosen = plan;
	auto situated = situation;
	if (const auto* used = creature_plan_actions::For(info.name); used != nullptr && used->use != Target::None)
	{
		if (const auto at = PointOf(registry, creature); !chosen.instrument.has_value() && at.has_value())
		{
			chosen.instrument = UsedFor(registry, creature, used->use, *at,
			                            tables->rules.at(static_cast<size_t>(plan.desire)).distanceWeight, plan.object);
		}
		situated.instrument = chosen.instrument;
	}
	// The agenda knows what the plan is about
	situated.activity = creature_learning::PlanActivity(
	    plan.activityObject, tables->rules.at(static_cast<size_t>(plan.desire)).searchesForGoal, plan.object);
	auto built = PlanAgenda(creature, plan.action, plan.object, situated);
	if (!built.has_value())
	{
		return false;
	}
	auto& [executor, agenda] = *built;
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
	mind.planner.current = chosen;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	// The source that says why it wants what the plan serves is asked before the drives are counted afresh
	const auto source = creature_learning::PlanSource((*mind.desires)[plan.desire]);
	creature_learning::ResetDrives((*mind.desires)[plan.desire]);
	creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	learnt.turnsSinceDone.at(plan.action) = 0;
	// Taking up compassion for a town starts its half minute afresh
	if (plan.desire == Desire::Compassion && plan.activityObject.has_value() &&
	    registry.AllOf<Town>(static_cast<entt::entity>(*plan.activityObject)))
	{
		town_compassion::StartHalfMinute(mind.townCompassion, mind.turn);
	}
	RememberAction(creature, mind,
	               {
	                   .action = plan.action,
	                   .desire = plan.desire,
	                   .object = plan.object,
	                   .belief = plan.object.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*plan.object), creature)
	                                                     : std::nullopt,
	                   .used = chosen.instrument,
	                   .about = plan.activityObject,
	                   .aboutBelief = plan.activityObject.has_value()
	                                      ? BeliefOf(registry, static_cast<entt::entity>(*plan.activityObject), creature)
	                                      : std::nullopt,
	                   .learnable = info.learnable,
	                   .windowSeconds = info.learningWindowSeconds,
	                   .source = source.value_or(creature_learning::k_NoTellingSource),
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
	// From now on, every free creature agrees when another creature asks it to play, to be friends or to be shown
	// something, until the land changes
	registry.Context().partnersAlwaysAgree = true;
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
	// What the forced plan is about, none when it is about nothing, as a script's is
	creature_plan_actions::Situation situation {
	    .instrument = toNumber(forced.instrument),
	    .handFull = registry.AllOf<CreatureHeldObject>(creature),
	    .activity = toNumber(forced.activityObject),
	};
	situation.self = entt::to_integral(creature);
	SeeCamera(registry, creature, situation);
	SituateHome(registry, creature, situation);
	SituatePlayer(registry, creature, situation);
	SituateSacrifice(registry, creature, situation);
	SituateTownTheft(registry, creature, situation);
	situation.forced = !forced.checkedAsChosen;
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
	if (const auto source = creature_learning::PlanSource((*mind->desires)[forced.desire]); source.has_value())
	{
		const auto& info = tables->actions[*action];
		RememberAction(
		    creature, *mind,
		    {
		        .action = *action,
		        .desire = forced.desire,
		        .object = actionObject,
		        .belief = actionObject.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*actionObject), creature)
		                                           : std::nullopt,
		        .used = situation.instrument,
		        .about = toNumber(forced.activityObject),
		        .aboutBelief =
		            forced.activityObject.has_value() ? BeliefOf(registry, *forced.activityObject, creature) : std::nullopt,
		        .learnable = info.learnable,
		        .windowSeconds = info.learningWindowSeconds,
		        .ordered = true,
		        .source = *source,
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
	// Once it is free to choose again it decides what to do with what it holds then, in place of anything else it was
	// left to do next
	mind->useWhatItHolds = true;
	mind->afterFeedback.reset();
	mind->highPending = false;
	if (auto* casting = registry.TryGet<CreatureCasting>(creature))
	{
		casting->fizzle.reset();
	}
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

bool CreatureMindSystem::ForceDesireOn(entt::entity creature, Desire desire, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !registry.Valid(thing))
	{
		return false;
	}
	// The current action stops as a failure, whether or not a new plan is found. A catch is the body's own and goes on
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy &&
	    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(*mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	creature_learning::MakeFullyDominant(*mind->desires, desire);
	// The thing is the only goal of its desires, and it plans now, even when it has planned this turn already
	mind->leash.actOn = {entt::to_integral(thing)};
	mind->plannedTurn = mind->turn + 1;
	PlanCreature(creature, *mind, true);
	mind->leash.actOn.clear();
	return mind->planActive && mind->planner.current.has_value() && mind->planner.current->desire == desire;
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
	const auto* at = registry.TryGet<const Transform>(creature);
	const glm::vec2 position = at != nullptr ? glm::vec2(at->position.x, at->position.z) : glm::vec2(0.0f);
	const auto keptWithin = KeptWithin(registry, creature);
	const auto* tables = GetTables();
	// Each desire weighs what it was given as a thing to act on, the thing it holds
	const auto weigh = [&](Desire desire) {
		auto thing = ActOnCandidate(registry, creature, position, *mind->learnt, desire, object, keptWithin);
		thing.held = true;
		// Held, the distance weight does not count
		const auto weight = tables != nullptr ? tables->rules.at(static_cast<size_t>(desire)).distanceWeight : 0.0f;
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = creature_planner::ActOnUsefulness(thing, weight)};
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

bool CreatureMindSystem::UseWhatItWasGiven(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.useWhatItHolds)
	{
		return false;
	}
	mind.useWhatItHolds = false;
	// What it holds now: the thing given when it took it, what it already held when it took nothing, or nothing
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
	                      : std::nullopt;
	if (held.has_value())
	{
		UseGift(creature, *held);
	}
	return true;
}

float CreatureMindSystem::ObjectToActOnUsefulness(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                                  entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* thing = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (!mind.learnt.has_value() || mind.beliefs.Find(object) == nullptr || tables == nullptr || thing == nullptr ||
	    !registry.AllOf<Transform>(creature))
	{
		return 0.0f;
	}
	const auto& at = registry.Get<const Transform>(creature).position;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	// Kept within an area, it doesn't act on what lies farther from it than the area's radius, but a part of a temple
	// when it wants to hang around at home
	if (leashes != nullptr)
	{
		const bool leashed = leashes->worn.has_value();
		const bool works = !leashed || leashes->worn->works;
		std::optional<float> keptWithin;
		if (creature_leash::IsConfined(leashes->confinementRadius, leashed, works))
		{
			keptWithin = leashes->confinementRadius;
		}
		if (creature_planner::BeyondKeptArea(desire, keptWithin, gutils::GetDistanceInMetres(at, thing->position),
		                                     IsTemplePart(registry, object)))
		{
			return 0.0f;
		}
	}
	// The distance counts from where its player's leash is tied, when it is, else from the creature; a thing it holds
	// counts as near as can be
	glm::vec3 from = at;
	if (leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
	    registry.Valid(*leashes->worn->tiedTo))
	{
		if (const auto* tied = registry.TryGet<const Transform>(*leashes->worn->tiedTo))
		{
			from = tied->position;
		}
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	const auto distance = creature_planner::DistancePriority(held != nullptr && held->object == object,
	                                                         gutils::GetDistanceInMetres(thing->position, from),
	                                                         tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	// In the game's order, each step rounded to a float: the tree's usefulness for the distance, then the desire's
	// factor for what the thing is
	const float forDistance =
	    Usefulness(*mind.learnt, TreeKind::ActOn, desire, BeliefOf(registry, object, creature)) * distance;
	return forDistance * creature_planner::ActOnFactor(desire, ActOnFactsOf(registry, object, *mind.learnt));
}

float CreatureMindSystem::ActivityUsefulness(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                             entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!mind.learnt.has_value() || !registry.Valid(object) || mind.beliefs.Find(object) == nullptr ||
	    !registry.AllOf<Transform>(creature))
	{
		return 0.0f;
	}
	// A thing with a place on the land is weighed only inside the area the creature is kept within, as the planner's
	// own test has it
	const auto* thing = registry.TryGet<const Transform>(object);
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (thing != nullptr && leashes != nullptr)
	{
		const bool leashed = leashes->worn.has_value();
		const bool works = !leashed || leashes->worn->works;
		std::optional<float> keptWithin;
		if (creature_leash::IsConfined(leashes->confinementRadius, leashed, works))
		{
			keptWithin = leashes->confinementRadius;
		}
		const auto& at = registry.Get<const Transform>(creature).position;
		if (creature_planner::BeyondKeptArea(desire, keptWithin, gutils::GetDistanceInMetres(at, thing->position),
		                                     IsTemplePart(registry, object)))
		{
			return 0.0f;
		}
	}
	return Usefulness(*mind.learnt, TreeKind::ActOn, desire, BeliefOf(registry, object, creature));
}

void CreatureMindSystem::PlanCreature(entt::entity creature, CreatureMindState& mind, bool forcedOnThing)
{
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	// Under a script's control it plans nothing of its own accord, nor does a partner of a joint action whose mind
	// stands still, nor one out cold. A desire forced on a thing is planned all the same: the game's forced planning does not
	// ask whether a script holds the creature
	if (tables == nullptr || transform == nullptr || !mind.desires.has_value() || !mind.learnt.has_value() || mind.paused ||
	    mind.leash.obeying || creature_mind::IsUnconscious(mind.idle) || mind.idle.outCold ||
	    (!forcedOnThing && ecs::script_held::IsControlledByScript(creature)) || registry.AllOf<CreatureDemobilised>(creature))
	{
		return;
	}
	if (mind.plannedTurn == mind.turn)
	{
		return;
	}
	mind.plannedTurn = mind.turn;
	// Having eaten a magic mushroom, once it is done with what it was doing it becomes high before it chooses anything
	// else
	if (mind.highPending && mind.idle.step >= mind.idle.agenda.size())
	{
		mind.highPending = false;
		static_cast<void>(BecomeHigh(creature));
		return;
	}
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
	situation.self = entt::to_integral(creature);
	SeeCamera(registry, creature, situation);
	SituateHome(registry, creature, situation);
	SituatePlayer(registry, creature, situation);
	SituateSacrifice(registry, creature, situation);
	SituateTownTheft(registry, creature, situation);
	if (mind.idle.showDesireSeconds <= 0.0f)
	{
		if (const auto strongest = creature_desires::StrongestShowable(desires, creature_mind::k_MinDesireShown))
		{
			situation.showDesireAnimation = creature_desires::EmoteFor(*strongest);
		}
	}
	bool clearLooked = false;
	bool friendsLooked = false;
	const auto prepare = [&](const creature_plan_actions::Executor& executor) {
		// The joint actions with a friend start by its own desires and the town it believes in
		if (executor.target == Target::CreatureFreeForJoint && !friendsLooked)
		{
			friendsLooked = true;
			SituateFriends(creature, situation);
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
	const bool inPasses = !leashTarget.has_value();
	auto goals = inPasses ? creature_planner::NextGoals(mind.planner) : std::vector<Desire> {};
	if (leashTarget.has_value())
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
	facts.ignoredByPlayer = creature_attention::IgnoredByPlayer(mind.turnsNotLookedAt, game_clock::MsPerTurn());
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
		// Actions are weighed in groups by the kind of thing they are done to, and whether that may be what it holds, so
		// each has a goal it can be done to; a desire that looks for no goal weighs them all in the order of its list
		std::array<std::vector<creature_planner::ActionCandidate>, creature_plan_actions::k_TargetCount * 2> groups {};
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
		const auto confinedWithin = KeptWithin(registry, creature);
		if (desire == Desire::Compassion)
		{
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
			                !creature_plan_actions::MayTry(*executor, mayCast, onFire) ||
			                (executor->build == creature_plan_actions::Build::DanceWithVillagers &&
			                 !MayDanceAgain(creature, executor->danceType)),
			};
			const auto group = static_cast<size_t>(executor->target) * 2 + (executor->notWhatItHolds ? 1 : 0);
			groups.at(group).push_back(candidate);
			inListOrder.push_back({group, candidate});
		}
		// What each group's actions could be done to
		std::array<std::vector<creature_planner::ObjectCandidate>, creature_plan_actions::k_TargetCount * 2> objectsOf {};
		for (size_t g = 0; g < groups.size(); ++g)
		{
			const auto target = static_cast<Target>(g / 2);
			const bool notHeld = g % 2 != 0;
			auto& objects = objectsOf.at(g);
			if (!groups.at(g).empty() && target != Target::None)
			{
				if (leashTarget.has_value())
				{
					if (Accepts(registry, target, *leashTarget, creature) &&
					    (!town.has_value() || PartOfTown(registry, *leashTarget, *town)))
					{
						objects.push_back(
						    ActOnCandidate(registry, creature, position, learnt, desire, *leashTarget, confinedWithin));
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
						if (notHeld && held != nullptr && held->creature == creature)
						{
							continue;
						}
						// The town is the goal, found beforehand: a member is judged only as what to act on
						const auto actOn = Usefulness(learnt, TreeKind::ActOn, desire, BeliefOf(registry, member, creature));
						members.push_back({
						    .id = entt::to_integral(member),
						    .distance = distance,
						    .held = held != nullptr && held->creature == creature,
						    .usefulness = actOn,
						    .goalUsefulness = actOn,
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
						// An action never done to what it holds passes over what it holds
						if (notHeld && held != nullptr && held->creature == creature)
						{
							continue;
						}
						// Weighed as a thing to act on too, which only a desire that looks for no goal does
						objects.push_back(
						    ActOnCandidate(registry, creature, position, learnt, desire, found.entity, confinedWithin));
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
			const auto distanceWeight = tables->rules.at(d).distanceWeight;
			auto plan = creature_planner::PlanWithoutGoal(desire, strengths.at(d), choices, distanceWeight,
			                                              entt::to_integral(creature));
			// An action done with something takes up the thing to use too; with nothing to use, the next action is tried
			while (plan.has_value())
			{
				const auto* executor = creature_plan_actions::For(tables->actions.at(plan->action).name);
				if (executor == nullptr || executor->use == Target::None)
				{
					break;
				}
				plan->instrument = UsedFor(registry, creature, executor->use, position, distanceWeight, plan->object);
				if (plan->instrument.has_value())
				{
					break;
				}
				const auto tried = std::ranges::find_if(choices, [&plan](const creature_planner::ActionChoice& choice) {
					return choice.action.action == plan->action;
				});
				if (tried == choices.end())
				{
					plan.reset();
					break;
				}
				tried->action.disabled = true;
				plan = creature_planner::PlanWithoutGoal(desire, strengths.at(d), choices, distanceWeight,
				                                         entt::to_integral(creature));
			}
			mind.planner.best.at(d) = plan;
			continue;
		}
		for (size_t g = 0; g < groups.size(); ++g)
		{
			if (groups.at(g).empty())
			{
				continue;
			}
			const auto distanceWeight = tables->rules.at(d).distanceWeight;
			auto plan =
			    creature_planner::PlanDesire(desire, strengths.at(d), objectsOf.at(g), distanceWeight, groups.at(g), search);
			// An action done with something takes up the thing to use too; with nothing to use, the next action is tried
			while (plan.has_value())
			{
				const auto* executor = creature_plan_actions::For(tables->actions.at(plan->action).name);
				if (executor == nullptr || executor->use == Target::None)
				{
					break;
				}
				plan->instrument = UsedFor(registry, creature, executor->use, position, distanceWeight, plan->object);
				if (plan->instrument.has_value())
				{
					break;
				}
				auto& tried = *std::ranges::find(groups.at(g), plan->action, &creature_planner::ActionCandidate::action);
				tried.disabled = true;
				plan = creature_planner::PlanDesire(desire, strengths.at(d), objectsOf.at(g), distanceWeight, groups.at(g),
				                                    search);
			}
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

void CreatureMindSystem::AteMagicMushroom(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Not while it has a gift to decide about: that keeps the place the high would take
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	    mind != nullptr && !mind->useWhatItHolds)
	{
		mind->highPending = true;
	}
}

bool CreatureMindSystem::BecomeHigh(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto position = PointOf(registry, creature);
	if (!position.has_value())
	{
		return false;
	}
	const auto radius = ecs::object::Get2DRadius(creature);
	const auto findSeat = [&]() -> std::optional<creature_mind::HighSeat> {
		const auto place = FindClearArea(registry, creature, *position, creature_mind::k_HighSeatRadii * radius);
		return place.has_value() ? std::optional(creature_mind::HighSeat {.place = *place, .radius = radius}) : std::nullopt;
	};
	// The sit's extra seconds are the game's own float draw over their range, not a share of one scaled up: the two
	// round differently
	auto agenda = creature_mind::BecomeHigh(
	    findSeat, [](uint32_t range) { return Random(range); }, [](float most) { return game_random::GameFloatRand(most); });
	// Carried out as a plan of getting high, which nothing it wants replaces until it is done
	return ForceActivity(
	    creature, {.desire = Desire::GetHigh, .action = "GetHigh", .actionObject = creature, .agenda = std::move(agenda)});
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
	const float floor = DesireFloorOf(std::as_const(registry), creature);
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
			// What it holds is good to eat, learnt about what it acts on, for hunger and what depends on hunger
			if (const auto belief = BeliefOf(registry, *held, creature); belief.has_value() && tables != nullptr)
			{
				for (const auto& [other, share] : creature_learning::EatWhatItHoldsLessons(tables->dependencies))
				{
					creature_mind_model::Learn(learnt, TreeKind::ActOn, other, {.belief = *belief, .feedback = share},
					                           tables->attributes.at(static_cast<size_t>(other)));
				}
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
			mind.useWhatItHolds = false;
			if (auto* casting = registry.TryGet<CreatureCasting>(creature))
			{
				casting->fizzle.reset();
			}
		}
		TellHelp(creature, creature_help::TooYoungToLearn(feedback));
		giveUp();
		return;
	}

	const auto desire = context->desire;
	const auto d = static_cast<size_t>(desire);
	const auto& texts = tables->texts.at(d);
	context->credited = strength;
	// A lesson from its player is something to do with the player: its turns without are counted from none, whether or
	// not the desire can be taught
	mind.turnsSinceMadeToAct = 0;
	if (tables->rules.at(d).learnable)
	{
		creature_learning::LearnDesireLesson(desires, desire, creature_learning::DominantSource(desires[desire]), strength,
		                                     tables->rules, tables->dependencies);
		creature_mind_model::Think(learnt, creature_learning::DesireLessonText(strength, texts.desire));
		// Then about the thing the plan was for, in the goal trees, and the thing it was done to, in the act-on trees, each
		// spread to the desires that depend on its desire
		const auto taught = creature_learning::ThingLessons(*context, entt::to_integral(creature), strength);
		context->taught = taught;
		const auto teach = [&](TreeKind tree, const std::optional<creature_tree::Belief>& belief, float feedback) {
			if (!belief.has_value())
			{
				return;
			}
			for (const auto& [other, share] : creature_learning::Spread(desire, feedback, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, tree, other, {.belief = *belief, .feedback = std::clamp(share, -1.0f, 1.0f)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
			creature_mind_model::Think(learnt, creature_learning::ObjectLessonText(strength, ObjectName(belief), texts.trying));
		};
		if (taught.goal.has_value())
		{
			teach(TreeKind::Goal, context->aboutBelief, *taught.goal);
		}
		// Where a thing it was taught about now sits in its desire's tree, once the lesson has grown it
		const auto leafOf = [&learnt, desire](TreeKind kind, bool taughtIt,
		                                      const std::optional<creature_tree::Belief>& belief) -> std::optional<size_t> {
			if (!taughtIt || !belief.has_value())
			{
				return std::nullopt;
			}
			return creature_tree::LeafOf(learnt.trees.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire)), *belief);
		};
		const auto goalLeaf = leafOf(TreeKind::Goal, taught.goal.has_value(), context->aboutBelief);
		if (taught.actOn.has_value())
		{
			teach(TreeKind::ActOn, context->belief, *taught.actOn);
		}
		const bool opinionChanged =
		    creature_help::OpinionChanged(goalLeaf, leafOf(TreeKind::ActOn, taught.actOn.has_value(), context->belief));
		auto& opinion = learnt.opinions.at(context->action);
		opinion = creature_learning::OpinionAfter(opinion, strength);
		creature_mind_model::Think(
		    learnt, creature_learning::ActionLessonText(strength, info->name, texts.trying, ObjectName(context->belief)));
		TellHelp(creature, creature_help::FeedbackLesson(strength, opinionChanged, desire, context->source));
	}

	if (feedback <= 0.0f)
	{
		giveUp();
		return;
	}
	// Stroked for something it did to a thing other than itself, and not while it copies the player, it does it again to
	// the thing of that kind most useful to act on, as the planner weighs it (the same one, it may be), forced on it at a
	// plan's plain worth
	if (creature_learning::OnItself(*context, entt::to_integral(creature)) || !context->belief.has_value() ||
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
		const auto keptWithin = KeptWithin(registry, creature);
		for (const auto& found : Gather(registry, executor->target, creature, position))
		{
			const auto belief = BeliefOf(registry, found.entity, creature);
			if (found.entity == creature || !belief.has_value() || belief->type != context->belief->type)
			{
				continue;
			}
			// The most useful to act on, the first of equals, worth more than nothing
			const auto usefulness = creature_planner::ActOnUsefulness(
			    ActOnCandidate(registry, creature, position, learnt, desire, found.entity, keptWithin),
			    tables->rules.at(d).distanceWeight);
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
	// The original asks the action's own test here, as its planner does
	std::ignore = ForceActivity(creature, {.desire = desire,
	                                       .action = tables->actions[action].name,
	                                       .actionObject = again,
	                                       .score = 1.0f,
	                                       .checkedAsChosen = true});
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
		// What the player showed it on a leash: anger or compassion towards such things, as what a plan is for
		for (const auto& shown : mind->leash.shown)
		{
			const auto belief = BeliefOf(registry, static_cast<entt::entity>(shown.object), creature);
			if (!belief.has_value())
			{
				continue;
			}
			for (const auto& lesson : shown.lessons)
			{
				creature_mind_model::Learn(learnt, TreeKind::Goal, lesson.desire,
				                           {.belief = *belief, .feedback = lesson.change},
				                           tables->attributes.at(static_cast<size_t>(lesson.desire)));
				creature_mind_model::Think(
				    learnt, creature_learning::ObjectLessonText(lesson.change, ObjectName(belief),
				                                                tables->texts.at(static_cast<size_t>(lesson.desire)).trying));
			}
		}
		mind->leash.shown.clear();
		// Leashed to another creature, it comes to find it nicer or nastier, and learns about creatures like it as what a
		// plan is for
		for (const auto& change : mind->leash.attitudes)
		{
			auto& attitude = creature_learning::AttitudeTo(learnt.creatures, change.creature);
			const auto lessons = creature_learning::ChangeHowNice(attitude, change.change);
			if (const auto belief = BeliefOf(registry, static_cast<entt::entity>(change.creature), creature))
			{
				for (const auto& [desire, amount] : lessons)
				{
					creature_mind_model::Learn(learnt, TreeKind::Goal, desire, {.belief = *belief, .feedback = amount},
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
				situation.turnsPerSecond = k_TurnsPerSecond;
				situation.self = entt::to_integral(creature);
				SituateHome(registry, creature, situation);
				SituateSacrifice(registry, creature, situation);
				SituateTownTheft(registry, creature, situation);
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
	file.turnsLookedAt = mind->turnsLookedAt;
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
		// The advisor tells of it: too young to learn it, learnt it now, or still learning it (a skill it knows already
		// says nothing)
		using creature_help::Kind;
		const auto parameter = static_cast<uint32_t>(skill);
		if (progress.ignored && !progress.learnt)
		{
			TellHelp(entity, {.kind = Kind::CantLearnNormalActionYet, .parameter = parameter});
		}
		else if (!progress.ignored)
		{
			TellHelp(entity, {.kind = progress.learnt ? Kind::LessonNormalAction : Kind::NearlyLearntNormalAction,
			                  .parameter = parameter});
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
	// The advisor tells of what the sighting showed, whether the miracle was shown to it or it saw one cast
	if (progress.event.has_value())
	{
		using creature_help::Kind;
		using creature_watching::LearningEvent;
		const auto kind = *progress.event == LearningEvent::TooYoung       ? Kind::CantLearnMagicActionYet
		                  : *progress.event == LearningEvent::NearlyLearnt ? Kind::NearlyLearntMagicAction
		                                                                   : Kind::LessonMagicAction;
		TellHelp(creature, {.kind = kind, .parameter = static_cast<uint32_t>(miracle)});
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
	NoticeDeed(player, deed, point, object, 0);
}

void CreatureMindSystem::PlayerCastDeed(PlayerNames player, size_t deed, const glm::vec3& point,
                                        std::optional<entt::entity> object, MagicType magic)
{
	NoticeDeed(player, deed, point, object, static_cast<uint32_t>(magic));
}

void CreatureMindSystem::NoticeDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                    uint32_t magic)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || deed >= tables->mimics.size())
	{
		return;
	}
	std::vector<entt::entity> mayCopy;
	for (const auto entity : MindsOf(registry))
	{
		// only the player's own creatures that may be leashed watch what the player does
		const auto& body = std::as_const(registry).Get<const Creature>(entity);
		if (body.owner != player || !body.leashable)
		{
			continue;
		}
		auto& mind = registry.Get<CreatureMindState>(entity);
		const auto& at = std::as_const(registry).Get<Transform>(entity);
		if (!mind.learnt.has_value())
		{
			continue;
		}
		// it must see where the thing is, and be nearer to it than the cells its eyes sweep
		const auto* animation = std::as_const(registry).TryGet<const CreatureAnimation>(entity);
		const auto look = creature_perceived_desires::LookYaw(animation != nullptr ? animation->lookAt : std::nullopt,
		                                                      at.position, BodyYaw(registry, entity, at));
		const auto from = map_coords::FromMetres(glm::vec2(at.position.x, at.position.z));
		const auto to = map_coords::FromMetres(glm::vec2(point.x, point.z));
		const bool nearEnough = creature_watching::WithinCopyReach(
		    gutils::GetDistanceInMetres(to, from), creature_look::SweepCells(ecs::creature_pose::DrawnSize(registry, entity)));
		const creature_watching::MimicConditions conditions {
		    .phase = mind.developmentPhase,
		    .learningLeashInHand = mind.leash.learningInHand,
		    .reactionPriority = ReactionPriorityOf(registry, entity),
		    .canSee = creature_perceived_desires::CanSeePos(look, from, to) && nearEnough,
		};
		if (creature_watching::MayStartMimicry(mind.learnt->mimicry, deed, tables->mimics, conditions))
		{
			mayCopy.push_back(entity);
		}
	}
	const auto target = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt;
	for (const auto entity : mayCopy)
	{
		auto* mind = registry.TryGet<CreatureMindState>(entity);
		const auto* body = std::as_const(registry).TryGet<const Creature>(entity);
		if (mind == nullptr || body == nullptr || !mind->learnt.has_value())
		{
			continue;
		}
		// a deed done with a miracle it has not seen often enough counts as a sighting of the miracle: with more than one
		// still needed it stops there, with one it goes on one time in two
		if (magic != 0 && magic < tables->miracles.size() && magic < mind->learnt->knowledge.miraclesSeen.size())
		{
			const auto step = creature_watching::CastDeedGate(
			    mind->learnt->knowledge.miraclesSeen.at(magic).count, tables->miracles.at(magic).timesToSee,
			    creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)));
			if (step != creature_watching::CastDeedStep::Copy)
			{
				WatchMiracle(entity, magic);
				if (step == creature_watching::CastDeedStep::CountAndStop || Random(2) == 0)
				{
					continue;
				}
			}
		}
		// Where the thing stood that it set about copying the player doing something to
		if (const auto copied = object.has_value() ? PointOf(registry, *object) : std::nullopt)
		{
			mind->copiedAt = *copied;
		}
		creature_watching::BeginMimicry(mind->learnt->mimicry, deed, tables->mimics, target);
		// seeing the deed stirs the first source of the desire behind it, and its wish to follow the player
		const auto desire = tables->mimics.at(deed).desire;
		if (mind->desires.has_value() && desire < creature_desires::k_DesireCount)
		{
			const auto& sources = (*mind->desires)[static_cast<Desire>(desire)].sources;
			if (!sources.empty())
			{
				creature_desires::ChangeSource(*mind->desires, sources.front().type, k_NoticedDeedSource);
			}
			creature_desires::ChangeSource(*mind->desires, k_FollowPlayerSource, k_NoticedDeedFollow);
		}
		// it notices: it turns to where the player did it and looks at it
		creature_mind_model::Think(*mind->learnt, fmt::format("I saw you: {}", tables->mimics.at(deed).name));
		Replan(entity, creature_mind::Activity::Planned, creature_mind::LookAt(glm::vec2(point.x, point.z), 2.0f));
	}
}

void CreatureMindSystem::RememberAction(entt::entity creature, CreatureMindState& mind, creature_learning::Context context)
{
	if (!mind.learnt.has_value())
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto forgotten = creature_learning::Remember(learnt.contexts, std::move(context));
	const auto* tables = GetTables();
	if (!forgotten.has_value() || tables == nullptr || forgotten->action >= tables->actions.size())
	{
		return;
	}
	// An action forced on it teaches approval as it is forgotten, as a little feedback would: about what it was for and
	// what it acted on, through the desires that go with its own. Nothing is learnt about the desire or the action
	// themselves.
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& species = Locator::infoConstants::value().creature;
	const auto row = static_cast<size_t>(creature::InfoRow(body->species));
	const auto desire = forgotten->desire;
	const auto approval = creature_learning::ForgottenOrderApproval(
	    *forgotten, tables->rules.at(static_cast<size_t>(desire)), row < species.size() ? species.at(row).orderApproval : 0.0f);
	if (!approval.has_value() ||
	    !creature_learning::PlanIsValid(*forgotten, tables->rules.at(static_cast<size_t>(desire)).searchesForGoal,
	                                    creature_mind_tables::NeedsInstrument(forgotten->action)))
	{
		return;
	}
	const auto teach = [&](TreeKind tree, const creature_tree::Belief& belief, float feedback) {
		for (const auto& [other, share] : creature_learning::Spread(desire, feedback, tables->dependencies))
		{
			creature_mind_model::Learn(learnt, tree, other, {.belief = belief, .feedback = std::clamp(share, -1.0f, 1.0f)},
			                           tables->attributes.at(static_cast<size_t>(other)));
		}
	};
	const auto taught = creature_learning::ThingLessons(*forgotten, entt::to_integral(creature), *approval);
	if (taught.goal.has_value())
	{
		teach(TreeKind::Goal, *forgotten->aboutBelief, *taught.goal);
	}
	if (taught.actOn.has_value())
	{
		teach(TreeKind::ActOn, *forgotten->belief, *taught.actOn);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} forgets action {} that was forced on it and approves it by {}",
	                    entt::to_integral(creature), forgotten->action, *approval);
}
