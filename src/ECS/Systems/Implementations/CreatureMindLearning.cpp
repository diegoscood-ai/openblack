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
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLightbulb.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureTeleportAgendas.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRules.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "CreatureReactions.h"
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
#include "ECS/Components/CreatureLightbulbs.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/FootballPitch.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GodCamera.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MimicType.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Football.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateFunctions.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/Weather/Weather.h"
#include "GameClock.h"
#include "Graphics/Moon.h"
#include "InfoConstants.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
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
/// A fish farm this near makes the fishing plan one it can choose (the original's IsThereFishFarmNearby)
constexpr float k_FishFarmNearby = 600.0f;

/// The fish farm closest to the creature on the map within a distance, as the game walks its list of fish farms: a farm
/// takes the place only when it is nearer than the nearest so far
std::optional<entt::entity> ClosestFishFarm(const ecs::Registry& registry, entt::entity creature, float within)
{
	const auto from = ecs::object::MapCoordsOf(creature);
	std::optional<entt::entity> closest;
	float nearest = within;
	registry.Each<const FishFarm>(
	    [&](entt::entity farm, const FishFarm& /*data*/) {
		    const float distance = game_math::GetDistanceInMetres(from, ecs::object::MapCoordsOf(farm));
		    if (distance < nearest)
		    {
			    nearest = distance;
			    closest = farm;
		    }
	    },
	    entt::exclude<Unavailable>);
	return closest;
}

/// Where the creature fishes for itself: the closest fish farm within 600 m, its shoal's middle (the farm's own place
/// without a shoal); where it can't stand there, the nearest place it can, if within 30 m of it in space; as near as it is
/// tall, 15 m at least. None when it holds something it can eat, or has no farm or nowhere to stand
std::optional<creature_plan_actions::Situation::Fishing> FishingOf(const ecs::Registry& registry, entt::entity creature,
                                                                   float height)
{
	constexpr float k_StandSearch = 1000.0f;
	constexpr float k_StandWithin = 30.0f;
	constexpr float k_LeastArrival = 15.0f;
	// Holding something it can eat, by the same test as what it may eat (not by a food value), it does not fish
	if (Locator::creatureObjectActionSystem::has_value())
	{
		const auto& hands = Locator::creatureObjectActionSystem::value();
		if (const auto held = hands.GetHeld(creature); held.has_value() && mind_detail::CanBeEaten(registry, creature, *held))
		{
			return std::nullopt;
		}
	}
	const auto farm = ClosestFishFarm(registry, creature, k_FishFarmNearby);
	if (!farm.has_value())
	{
		return std::nullopt;
	}
	const auto& data = registry.Get<const FishFarm>(*farm);
	const auto shoal = data.shoal.has_value() ? data.shoal->centre : registry.Get<const Transform>(*farm).position;
	glm::vec2 point {shoal.x, shoal.z};
	// the walk keeps the shoal's height, or none for the place found to stand on, as the game's search leaves it
	float pointHeight = shoal.y;
	if (!land_avoid::IsPosValid(glm::vec3(point.x, 0.0f, point.y), land_avoid::k_CreatureRadius))
	{
		pointHeight = 0.0f;
		const auto valid = land_avoid::NearestValid(point, land_avoid::k_CreatureRadius, k_StandSearch);
		if (!valid.has_value() ||
		    !creature_plan_actions::StandPlaceNearEnough(glm::vec3(point.x, shoal.y, point.y), *valid, k_StandWithin))
		{
			return std::nullopt;
		}
		point = *valid;
	}
	return creature_plan_actions::Situation::Fishing {
	    .point = point, .arriveWithin = std::max(k_LeastArrival, height), .pointHeight = pointHeight};
}

/// Where the creature fishes to give food to a store, put it by a worship site or take it home: the closest fish farm within
/// 600 m, its shoal's middle (the farm's own place without a shoal), as near as it is tall, 15 m at least; none with no farm
std::optional<creature_plan_actions::Situation::Fishing> ShoalTripOf(const ecs::Registry& registry, entt::entity creature,
                                                                     float height)
{
	constexpr float k_LeastArrival = 15.0f;
	const auto farm = ClosestFishFarm(registry, creature, k_FishFarmNearby);
	if (!farm.has_value())
	{
		return std::nullopt;
	}
	const auto& data = registry.Get<const FishFarm>(*farm);
	const auto shoal = data.shoal.has_value() ? data.shoal->centre : registry.Get<const Transform>(*farm).position;
	return creature_plan_actions::Situation::Fishing {.point = {shoal.x, shoal.z},
	                                                  .arriveWithin = std::max(k_LeastArrival, height)};
}

bool OutsideConfinement(const ecs::Registry& registry, entt::entity creature, entt::entity object);

/// The town a thing belongs to, as each kind answers it: a town itself, a town's desire flag its town's, any other its
/// own (ecs::town_queries::GetTown); none for a thing of no town
entt::entity TownOfThing(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Town>(thing))
	{
		return thing;
	}
	if (const auto* flag = registry.TryGet<const TownDesireFlag>(thing))
	{
		return flag->town;
	}
	return ecs::town_queries::GetTown(thing);
}

/// What the creature counts of its life, from its tallies and its fight record (none: nothing counted yet)
creature_mind_model::Counts CountsOf(const ecs::Registry& registry, entt::entity creature)
{
	creature_mind_model::Counts counts;
	if (const auto* tallies = registry.TryGet<const CreatureTallies>(creature))
	{
		counts.peopleKilled = tallies->meals.peopleKilled;
		counts.animalsKilled = tallies->meals.animalsKilled;
		counts.creaturesKilled = tallies->meals.creaturesKilled;
		counts.poos = tallies->poos;
		counts.mushroomsEaten = tallies->mushroomsEaten;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(creature))
	{
		counts.battlesFought = record->fights;
		counts.battlesWon = record->wins;
	}
	return counts;
}

/// The counts given to the creature: its tallies and its fight record
void SetCounts(ecs::Registry& registry, entt::entity creature, const creature_mind_model::Counts& counts)
{
	auto& tallies = registry.AllOf<CreatureTallies>(creature) ? registry.Get<CreatureTallies>(creature)
	                                                          : registry.AssignState<CreatureTallies>(creature);
	tallies.meals = {
	    .peopleKilled = counts.peopleKilled, .animalsKilled = counts.animalsKilled, .creaturesKilled = counts.creaturesKilled};
	tallies.poos = counts.poos;
	tallies.mushroomsEaten = counts.mushroomsEaten;
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.AssignState<CreatureFightRecord>(creature);
	record.fights = counts.battlesFought;
	record.wins = counts.battlesWon;
}

/// The advisor may tell the player of what the creature learnt
void TellHelp(entt::entity creature, const creature_help::Message& message)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(creature, message);
	}
}

/// Its player's order to act on a thing taken, with a plan it can carry out, it tells what it wants now
void TellLeashOrder(const ecs::Registry& registry, entt::entity creature, bool planMade, creature_desires::Desire desire,
                    entt::entity thing)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (const auto told = creature_help::LeashOrderMessage(planMade, desire, thing,
	                                                       leashes != nullptr ? leashes->lastOrderPoint : glm::vec3(0.0f)))
	{
		TellHelp(creature, *told);
	}
}

constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// The action a creature given something from the hand carries out to take it
constexpr std::string_view k_TakeFromHandAction = "TakeObjectFromHand";
/// A sit it chooses by itself looks for its clear place this many of its 2D radii wide
constexpr float k_SitClearAreaRadii = 4.0f;
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
/// A creature watches skills, miracles and the player's deeds this far away at most
constexpr float k_ViewDistance = 150.0f;
//// The land's script that curses the player's creature, and the values it keeps of the creature as it found it: the
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
	// The camera of its own god
	if (const auto camera = ecs::god_camera::Of(creature); camera.has_value())
	{
		situation.eye = camera->origin;
		situation.camera = glm::vec2(situation.eye.x, situation.eye.z);
	}
	// the moon where the sky stands it from the game's camera at its hour, whether or not it shows
	if (Locator::camera::has_value() && Locator::skySystem::has_value())
	{
		situation.moon = Locator::camera::value().GetOrigin() + graphics::moon::Offset(Locator::skySystem::value().GetTime());
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL;
	situation.evil = body != nullptr && body->alignment < 0.0f;
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
		// Whether it showed off an impressive pose lately, by the turn it last carried one out
		if (const auto* tables =
		        Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr)
		{
			if (const auto pose = creature_mind_tables::FindAction(*tables, "ShowImpressiveAnimation"))
			{
				const auto last = mind->actionTurns.find(*pose);
				situation.impressivePoseRested = creature_plan_actions::HasntShownImpressivePoseRecently(
				    game_clock::Turn(), last != mind->actionTurns.end() ? last->second : 0);
			}
		}
	}
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		situation.home = glm::vec2(home->x, home->z);
		if (Locator::terrainSystem::has_value())
		{
			situation.homeAltitude = Locator::terrainSystem::value().GetHeightAt(*situation.home);
		}
		situation.homeDistance = game_math::GetDistanceInMetres(glm::vec2(at->position.x, at->position.z), *situation.home);
	}
	situation.fishFarmNearby = ClosestFishFarm(registry, creature, k_FishFarmNearby).has_value();
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
	// Every other creature counts, of any player, dead or alive, but not one waiting on the dead list: its deletion took
	// it off the list of creatures
	std::vector<float> others;
	registry.Each<const Creature, const Transform>(
	    [&](entt::entity other, const Creature&, const Transform& place) {
		    if (other != creature)
		    {
			    others.push_back(game_math::GetDistanceInMetres(place.position, at->position));
		    }
	    },
	    entt::exclude<Unavailable>);
	situation.nothingFrighteningNear =
	    creature_mind::NothingFrighteningNear(object_measures::Height(registry, creature), others);
	situation.visualNight = Locator::dayNightClock::has_value() && Locator::dayNightClock::value().Clock().IsVisualNight();
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature); needs != nullptr)
	{
		situation.healthyEnoughToFight = creature_fight::HealthyEnoughToFight(needs->needs.life);
	}
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
			metres.push_back(game_math::GetDistanceInMetres(at.position, from));
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
/// its player. Only the local player's hands and camera are here, but the distance to where its player's nearest hand
/// last had a place is there for every player
void SituatePlayer(const ecs::Registry& registry, entt::entity self, creature_plan_actions::Situation& situation)
{
	const auto* body = registry.TryGet<const Creature>(self);
	const auto* at = registry.TryGet<const Transform>(self);
	const auto* mind = registry.TryGet<const CreatureMindState>(self);
	if (mind != nullptr)
	{
		situation.attitudeToPlayer = mind->attitudeToPlayer;
	}
	// How far it stands from where its player's nearest hand last had a place, whoever its player is
	if (mind != nullptr && body != nullptr && at != nullptr && body->owner != PlayerNames::NEUTRAL)
	{
		situation.handDistance = game_math::GetDistance(at->position, mind->lastHandPoint);
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
	return build == Build::GoToMiddleOfScreen || build == Build::FollowPlayer || build == Build::BeCrossWithPlayer;
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
/// How a thing weighs by how near it is, as the desire weighs distance: from where its player's leash is tied, when it
/// is, else from the creature; a thing it holds counts as near as can be
float DistancePriorityOf(const ecs::Registry& registry, entt::entity creature, entt::entity object, float distanceWeight)
{
	const auto* thing = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (thing == nullptr || !registry.AllOf<Transform>(creature))
	{
		return 0.0f;
	}
	glm::vec3 from = registry.Get<const Transform>(creature).position;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
	    registry.Valid(*leashes->worn->tiedTo))
	{
		if (const auto* tied = registry.TryGet<const Transform>(*leashes->worn->tiedTo))
		{
			from = tied->position;
		}
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	return creature_planner::DistancePriority(held != nullptr && held->object == object,
	                                          game_math::GetDistanceInMetres(thing->position, from), distanceWeight);
}

} // namespace

bool CreatureMindSystem::SuitableForDesire(entt::entity creature, CreatureMindState& mind, entt::entity thing, Desire desire)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const uint32_t msPerTurn = game_clock::MsPerTurn();
	if (auto* belief = mind.beliefs.Find(thing);
	    belief != nullptr && msPerTurn != 0 &&
	    creature_object_beliefs::BlockedRecently(*belief, game_clock::Turn(), 1000 / msPerTurn,
	                                             ecs::map_cells::IsMultiCellStaticClass(thing)))
	{
		return false;
	}
	if (OutsideConfinement(registry, creature, thing) || !ecs::IsAvailable(thing))
	{
		return false;
	}
	// A hungry creature first comes across the town the thing belongs to, and leaves the thing alone when compassion for
	// the town is worth enough to it
	if (desire == Desire::Hunger)
	{
		if (const auto town = TownOfThing(registry, thing); town != entt::null && registry.Valid(town))
		{
			ComeAcross(creature, mind, town);
			if (creature_planner::HungerSparesTownThing(ActivityUsefulness(creature, mind, Desire::Compassion, town)))
			{
				return false;
			}
		}
	}
	return mind_detail::LetsCreatureAct(registry, thing) && mind_detail::DesireMayActOn(registry, desire, thing, creature);
}

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
	    .size = value(k_CurseSizeVariable) * creature_morph::k_SizePerHeight,
	    .strength = value(k_CurseStrengthVariable),
	    .alignment = value(k_CurseAlignmentVariable),
	};
}

namespace
{
namespace town_compassion = creature_town_compassion;

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
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		if (desires.IsActivated(d) && desires.desires.at(d).value > compassion)
		{
			return false;
		}
	}
	return true;
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
		// a leash on and tied to a town's desire flag holds off the healing
		const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
		const bool leashedToTownDesire = leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
		                                 registry.Valid(*leashes->worn->tiedTo) &&
		                                 registry.AllOf<TownDesireFlag>(*leashes->worn->tiedTo);
		const auto sightings = MiracleSightings(mind, tables, body->species, MagicType::Heal);
		if (sightings.has_value() &&
		    town_compassion::HealFirst(data->injured, leashedToTownDesire, sightings->first, sightings->second, random))
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
	                                                        // as the original's: commanded, unless by its player's action
	                                                        .commanded = mind.commanded && !mind.forcedByPlayerAction,
	                                                        .floor = DesireFloorOf(registry, creature),
	                                                        .fullRecovery = action.fullRecovery,
	                                                    },
	                                                    k_TurnsPerSecond);
	// A readout at run time (the game channel at debug, not compiled out of release builds): what a plan carried out
	// to its end did for its desire, so that a played run shows a forced plan counted and a commanded one not
	if (const auto logger = spdlog::get("game"))
	{
		const bool commanded = mind.commanded && !mind.forcedByPlayerAction;
		logger->debug("Creature {} carried out {} for {}: {}, turn {}", entt::to_integral(creature), action.name,
		              creature_desires::Name(plan.desire),
		              commanded ? "commanded, it counts for no desire"
		                        : (result.satisfied ? "the desire satisfied" : "counted"),
		              game_clock::Turn());
	}
	if (result.restoreLife)
	{
		ecs::life::SetLife(creature, creature_learning::LifeAfterRestoringHealth(ecs::life::LifeOf(creature)));
	}
	// After a full recovery, its full life and the temperature of the climate where it stands, with no source
	if (result.fullRecovery)
	{
		ecs::life::SetLife(creature, creature_learning::k_FullRecoveryLife);
		const auto* transform = registry.TryGet<const Transform>(creature);
		if (transform != nullptr && Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().SetTemperature(creature, weather::GetTemperatureAt(transform->position), entt::null);
		}
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

namespace
{
/// How tall a creature is drawn: the size it is drawn at, in the game's height per unit of size
float DrawnHeight(const ecs::Registry& registry, entt::entity creature)
{
	return ecs::creature_pose::DrawnSize(registry, creature) * ecs::object::k_CreatureHeightPerScale;
}
} // namespace

/// What a creature finds of a thing now: its kind and attributes, as far as the game's world here tells them
std::optional<creature_tree::Belief> mind_detail::BeliefNowOf(const ecs::Registry& registry, entt::entity entity,
                                                              entt::entity self)
{
	using creature_tree::Attribute;
	namespace types = creature_tree::belief_types;
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* me = registry.TryGet<const Creature>(self);
	// The believer's player, which the things of the kinds with only the common attributes are compared with; none for
	// a believer that is no creature
	const auto believer = me != nullptr ? std::optional(PlayerNumberOf(me->owner)) : std::nullopt;
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
		// A villager is its town's player's, and of no player without a town; natural and alive
		const auto town = registry.Valid(villager->town) ? TownPlayerOf(registry, villager->town) : std::nullopt;
		belief = creature_tree::CommonBelief(types::k_Villager, town, believer, creature_tree::k_OriginNatural, true);
		belief.Set(Attribute::Sex, static_cast<uint32_t>(villager->sex));
		// Its job is the town desire that the state it is in now serves, not the kind of villager it is
		int32_t served = -1;
		if (const auto* action = registry.TryGet<const LivingAction>(entity);
		    action != nullptr && Locator::infoConstants::has_value())
		{
			const auto top = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Top)));
			served = ecs::villager::state_info::ServedDesire(ecs::villager::state_info::StateInfo(top));
		}
		belief.Set(Attribute::VillagerJob, creature_tree::VillagerJobValue(served));
		belief.Set(Attribute::Life, creature_tree::LifeValue(villager->life));
		belief.Set(Attribute::OnFire, OnFireValue(ecs::fire::IsOnFire(entity)));
		return belief;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		// Every animal is the neutral player's but the wolf miracle's wolves, which are their miracle's player's, and of
		// no player when it has none; natural and alive. An animal has only the attributes every thing has: not how much
		// of its life is left
		std::optional<uint32_t> player = PlayerNumberOf(PlayerNames::NEUTRAL);
		if (animal->type == AnimalInfo::SpellWolf)
		{
			player = animal->player >= 0 ? std::optional(static_cast<uint32_t>(animal->player)) : std::nullopt;
		}
		return creature_tree::CommonBelief(types::k_Animal, player, believer, creature_tree::k_OriginNatural, true);
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const bool mine = me != nullptr && me->owner == creature->owner;
		// Another player's creature is another's, as a thing of no player is
		common(types::k_Creature, mine ? 0 : 1, 0, 1, PlayerNumberOf(creature->owner));
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
		// What it knows of another creature: how many miracles it knows, the object type of the thing it carries, and
		// the desire that the action of its plan, or of its last plan, shows
		const auto* mind = registry.TryGet<const CreatureMindState>(entity);
		const size_t miracles = mind != nullptr && mind->learnt.has_value()
		                            ? static_cast<size_t>(std::ranges::count(mind->learnt->knowledge.miraclesKnown, true))
		                            : 0;
		belief.Set(Attribute::SpellKnowledge, creature_tree::SpellKnowledgeValue(miracles));
		std::optional<uint32_t> carried;
		if (const auto* held = registry.TryGet<const CreatureHeldObject>(entity);
		    held != nullptr && registry.Valid(held->object))
		{
			// The type of the thing's own info, not the kind of belief it gives; a thing openblack has no type for counts
			// as nothing carried
			if (const auto type = ecs::map_cells::TypeOf(held->object); static_cast<int>(type) >= 0)
			{
				carried = static_cast<uint32_t>(type);
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
	if (const auto* field = registry.TryGet<const Field>(entity))
	{
		// A field is its town's player's, and of no player without a town; made by people, not alive. Its abode part
		// gives it no more
		const auto town = field->town >= 0 ? ecs::town_queries::TownByKey(static_cast<uint32_t>(field->town)) : entt::null;
		return creature_tree::CommonBelief(types::k_Field, TownPlayerOf(registry, town), believer,
		                                   creature_tree::k_OriginMadeByPeople, false);
	}
	if (const auto* farm = registry.TryGet<const FishFarm>(entity))
	{
		// A fish farm is its town's player's too, but natural
		return creature_tree::CommonBelief(types::k_Field, TownPlayerOf(registry, farm->town), believer,
		                                   creature_tree::k_OriginNatural, false);
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		// An abode is its town's player's, and the neutral player's without a town; made by people, not alive
		const auto town = abode->townId != Abode::k_NoTown ? ecs::town_queries::TownByKey(abode->townId) : entt::null;
		const auto player = TownPlayerOf(registry, town).value_or(PlayerNumberOf(PlayerNames::NEUTRAL));
		// A football pitch is a feature, with only the attributes every thing has
		if (abode->type == AbodeNumber::FootballPitch)
		{
			return creature_tree::CommonBelief(types::k_Feature, player, believer, creature_tree::k_OriginMadeByPeople, false);
		}
		belief = creature_tree::CommonBelief(types::k_Abode, player, believer, creature_tree::k_OriginMadeByPeople, false);
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
	if (const auto* temple = registry.TryGet<const Temple>(entity))
	{
		// The temple is its owner's, the believer's own only when that is the believer's player; magic, not made by
		// people, and not alive
		return creature_tree::CommonBelief(types::k_Citadel, PlayerNumberOf(temple->owner), believer,
		                                   creature_tree::k_OriginMagic, false);
	}
	if (registry.AnyOf<WorshipSite, WorshipTotem>(entity))
	{
		// A worship site and the altar at its head are parts of their temple, its owner's, the believer's own only when
		// that is the believer's player; natural, unlike the temple, and not alive
		std::optional<uint32_t> player;
		if (const auto temple = TempleOfPart(registry, entity))
		{
			player = PlayerNumberOf(registry.Get<const Temple>(*temple).owner);
		}
		return creature_tree::CommonBelief(types::k_CitadelPart, player, believer, creature_tree::k_OriginNatural, false);
	}
	if (const auto* flock = registry.TryGet<const Flock>(entity))
	{
		// A flock is its player's, and of no player without one; natural, and not alive itself
		const auto player = flock->player.has_value() ? std::optional(PlayerNumberOf(*flock->player)) : std::nullopt;
		return creature_tree::CommonBelief(types::k_Flock, player, believer, creature_tree::k_OriginNatural, false);
	}
	if (const auto* spell = registry.TryGet<const Spell>(entity))
	{
		// A miracle is its caster's player's, and of no player when it has none; magic, not alive
		const auto caster = ReactionPlayerOf(*spell);
		const auto player = caster.has_value() ? std::optional(PlayerNumberOf(*caster)) : std::nullopt;
		return creature_tree::CommonBelief(types::k_Spell, player, believer, creature_tree::k_OriginMagic, false);
	}
	if (registry.AllOf<SpellSeed>(entity))
	{
		// A miracle's seed is the neutral player's, whoever holds it; magic, not alive
		return creature_tree::CommonBelief(types::k_SpellSeed, PlayerNumberOf(PlayerNames::NEUTRAL), believer,
		                                   creature_tree::k_OriginMagic, false);
	}
	if (registry.AllOf<Mist>(entity))
	{
		// A puff of mist is of the miracle seed's kind, but of no player and natural
		return creature_tree::CommonBelief(types::k_SpellSeed, std::nullopt, believer, creature_tree::k_OriginNatural, false);
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

std::optional<creature_tree::Belief> mind_detail::BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                           entt::entity self)
{
	auto now = BeliefNowOf(registry, entity, self);
	// A thing it has a belief about keeps what does not change as the creature first saw it
	const auto* mind = now.has_value() ? registry.TryGet<const CreatureMindState>(self) : nullptr;
	if (const auto* known = mind != nullptr ? mind->beliefs.Find(entity) : nullptr; known != nullptr)
	{
		return creature_object_beliefs::KnownOf(*known, *now);
	}
	return now;
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

/// A town counts as believing in the creature's player when its belief in that player is more than this share of its
/// largest belief in another player
constexpr float k_HalfOfMostOtherBelief = 0.5f;

/// What a creature knows about a whole thing it believes in as it weighs a desire about it: whose it is, whether a town
/// believes in the creature's player, whether a creature is one it may be angry with, and whether its player looks at it
creature_activity::WholeFacts WholeFactsOf(const ecs::Registry& registry, entt::entity creature, entt::entity whole,
                                           creature_activity::Whole kind, bool lookedAt)
{
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto mine = self != nullptr ? self->owner : PlayerNames::NEUTRAL;
	creature_activity::WholeFacts facts {.kind = kind, .lookedAt = lookedAt};
	// A forest and a flock have no player
	auto player = PlayerNames::NEUTRAL;
	switch (kind)
	{
	case creature_activity::Whole::Town:
		if (const auto* town = registry.TryGet<const Town>(whole))
		{
			player = town->owner;
			// Always, for a creature of no player: such a creature impresses any town of a player
			facts.believesInPlayer = mine == PlayerNames::NEUTRAL ||
			                         ecs::town_belief::GetMaxBeliefMeNotIncluded(town->belief, mine) * k_HalfOfMostOtherBelief <
			                             ecs::town_belief::GetBeliefInPlayer(town->belief, mine);
		}
		break;
	case creature_activity::Whole::Citadel:
		if (const auto* temple = registry.TryGet<const Temple>(whole))
		{
			player = temple->owner;
		}
		break;
	case creature_activity::Whole::Creature:
		if (const auto* other = registry.TryGet<const Creature>(whole))
		{
			player = other->owner;
		}
		facts.attackable = MayBeAngryWith(registry, whole, creature);
		break;
	case creature_activity::Whole::Forest:
	case creature_activity::Whole::Flock:
		break;
	}
	facts.samePlayer = player == mine;
	facts.hasPlayer = player != PlayerNames::NEUTRAL;
	return facts;
}

/// The whole thing a desire that looks for a goal is about, its kind, and its worth as a goal for the distance
struct BelievedGoal
{
	entt::entity whole {entt::null};
	creature_activity::Whole kind {creature_activity::Whole::Town};
	float worth {0.0f};
};

/// The whole thing a desire that looks for a goal is about: of the whole things the creature believes in, the newest
/// first, not itself, those the desire can be about (creature_activity::CanBeAbout), the one most worth it as a goal
/// (GoalUsefulnessOf: nothing for one none of whose parts it has come across) for the distance from where it stands,
/// the first of the most, and only one worth more than nothing
std::optional<BelievedGoal> GoalAmongBeliefs(const ecs::Registry& registry, entt::entity creature, glm::vec2 position,
                                             const CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                                             Desire desire, bool lookedAt)
{
	const auto distanceWeight = tables.rules.at(static_cast<size_t>(desire)).distanceWeight;
	std::vector<BelievedGoal> found;
	std::vector<creature_activity::WholeCandidate> wholes;
	for (const auto* belief : mind.beliefs.WholeThingsNewestFirst())
	{
		const auto kind = creature_activity::WholeOf(belief->kind);
		// Where the thing is now: the creature keeps no memory of where it saw it last
		const auto at = PointOf(registry, belief->thing);
		if (!kind.has_value() || !at.has_value())
		{
			continue;
		}
		creature_activity::WholeCandidate whole {
		    .facts = WholeFactsOf(registry, creature, belief->thing, *kind, lookedAt),
		    .itself = belief->thing == creature,
		};
		// Only what the desire can be about is weighed
		if (!whole.itself && creature_activity::CanBeAbout(desire, whole.facts, !mind.angerSparesOwnPlayer))
		{
			const auto distance = game_math::GetDistanceInMetres(*at, position);
			whole.worth = GoalUsefulnessOf(registry, creature, mind, tables, desire, belief->thing) *
			              creature_planner::DistancePriority(false, distance, distanceWeight);
		}
		found.push_back({.whole = belief->thing, .kind = *kind, .worth = whole.worth});
		wholes.push_back(whole);
	}
	const auto best = creature_activity::ChooseWhole(desire, wholes, !mind.angerSparesOwnPlayer);
	return best.has_value() ? std::optional(found.at(*best)) : std::nullopt;
}
} // namespace

std::optional<Desire> mind_detail::DesireAboutCreature(const ecs::Registry& registry, entt::entity creature,
                                                       const CreatureMindState& mind, entt::entity other,
                                                       const creature_mind_tables::Tables& tables,
                                                       std::span<const Desire> leftOut)
{
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* otherAt = registry.TryGet<const Transform>(other);
	if (!mind.desires.has_value() || !mind.learnt.has_value() || at == nullptr || otherAt == nullptr ||
	    !registry.AllOf<Creature>(other))
	{
		return std::nullopt;
	}
	// From where it believes the other creature to be (where it is now, as a belief taken now gives) to itself, or to
	// the thing its leash is tied to; a creature it carries is at the carried distance's priority
	const auto believed = BelievedPlaceOf(registry, creature, other);
	if (!believed.has_value())
	{
		return std::nullopt;
	}
	std::optional<map_coords::MapCoords> tiedTo;
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	    leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo.has_value() &&
	    registry.Valid(*leash->worn->tiedTo) && registry.AllOf<Transform>(*leash->worn->tiedTo))
	{
		tiedTo = ecs::object::MapCoordsOf(*leash->worn->tiedTo);
	}
	const auto distance = creature_planner::DistanceFromBelief(*believed, ecs::object::MapCoordsOf(creature), tiedTo);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	const bool carried = held != nullptr && held->object == other;
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool ownPlayers = body != nullptr && registry.Get<const Creature>(other).owner == body->owner;
	std::vector<creature_planner::DesireOnThing> weighed;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		const auto& state = mind.desires->desires.at(d);
		if (!mind.desires->IsActivated(d) || state.suppressedTurns != 0 || !tables.rules.at(d).searchesForGoal ||
		    !AppliesToCreature(registry, desire, other, creature) || std::ranges::find(leftOut, desire) != leftOut.end())
		{
			continue;
		}
		const auto nearness = creature_planner::DistancePriority(carried, distance, tables.rules.at(d).distanceWeight);
		weighed.push_back({.desire = desire,
		                   .strength = state.value,
		                   .usefulness = GoalUsefulnessOf(registry, creature, mind, tables, desire, other) * nearness,
		                   .ownPlayers = ownPlayers});
	}
	return creature_planner::BestDesireOnThing(weighed, mind.angerSparesOwnPlayer);
}

float mind_detail::WorthAboutCreature(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind,
                                      entt::entity other, const creature_mind_tables::Tables& tables, Desire desire)
{
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* otherAt = registry.TryGet<const Transform>(other);
	if (!mind.desires.has_value() || !mind.learnt.has_value() || at == nullptr || otherAt == nullptr)
	{
		return 0.0f;
	}
	const auto d = static_cast<size_t>(desire);
	const auto distance = game_math::GetDistanceInMetres(otherAt->position, at->position);
	const auto nearness = creature_planner::DistancePriority(false, distance, tables.rules.at(d).distanceWeight);
	return creature_reaction_rules::WorthAbout(mind.desires->desires.at(d).value,
	                                           GoalUsefulnessOf(registry, creature, mind, tables, desire, other), nearness);
}

bool CreatureMindSystem::WeighWatchedCreature(entt::entity creature, CreatureMindState& mind, entt::entity other)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* theirMind = registry.Valid(other) ? registry.TryGet<const CreatureMindState>(other) : nullptr;
	const auto* tables = GetTables();
	if (theirMind == nullptr || tables == nullptr || !mind.desires.has_value())
	{
		return false;
	}
	std::vector<Desire> leftOut;
	for (;;)
	{
		// What it would do about the other, and what the other would do about it
		const auto mine = mind_detail::DesireAboutCreature(registry, creature, mind, other, *tables, leftOut);
		const auto theirs = mind_detail::DesireAboutCreature(registry, other, *theirMind, creature, *tables);
		const auto worths = [&]() {
			return std::pair {mind_detail::WorthAboutCreature(registry, creature, mind, other, *tables, Desire::Anger),
			                  mind_detail::WorthAboutCreature(registry, creature, mind, other, *tables, Desire::Fear)};
		};
		const auto dominant = [&]() {
			const auto* own = registry.TryGet<const CreatureNeeds>(creature);
			const auto* their = registry.TryGet<const CreatureNeeds>(other);
			// Near in age, the one later in the game's list of things: openblack's order of making stands for that list
			return creature_reaction_rules::IsDominant(own != nullptr ? own->needs.age : 0,
			                                           their != nullptr ? their->needs.age : 0,
			                                           entt::to_entity(other) < entt::to_entity(creature));
		};
		const auto verdict = creature_reaction_rules::WeighWatch(mine, theirs, worths, dominant);
		if (verdict.weighAgainWithoutFear)
		{
			leftOut.push_back(Desire::Fear);
			continue;
		}
		if (!verdict.act.has_value())
		{
			return false;
		}
		// It drops the look to act on that desire about the other, whether or not a plan comes of it
		std::ignore = ForceDesireOn(creature, *verdict.act, other);
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Creature {} stops looking {} over to act on desire {}", entt::to_integral(creature),
			              entt::to_integral(other), static_cast<int>(*verdict.act));
		}
		return true;
	}
}

std::optional<creature_planner::Plan> mind_detail::LeashFightPlan(bool fightsOnLeash, std::optional<uint32_t> fightAction,
                                                                  uint32_t target)
{
	if (!fightsOnLeash || !fightAction.has_value())
	{
		return std::nullopt;
	}
	return creature_planner::Plan {
	    .desire = Desire::Anger, .action = *fightAction, .object = target, .priority = 1.0f, .activityObject = target};
}

std::vector<Desire> mind_detail::GoalsForThing(std::optional<Desire> forced,
                                               const std::array<bool, creature_desires::k_DesireCount>& eligible)
{
	// Forcing a desire on a thing plans that desire alone, as the game finds the best action for one plan of it
	if (forced.has_value())
	{
		return {*forced};
	}
	std::vector<Desire> goals;
	for (size_t d = 0; d < eligible.size(); ++d)
	{
		if (eligible.at(d))
		{
			goals.push_back(static_cast<Desire>(d));
		}
	}
	return goals;
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

/// A villager plays football while the state it ends in (its top state when that is final, else its final one) is one
/// of the match's, which all share the football exit. Needs the info constants
bool PlaysFootball(const LivingAction& action)
{
	namespace state_info = ecs::villager::state_info;
	const auto stateAt = [&action](LivingAction::Index index) {
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	};
	const auto top = stateAt(LivingAction::Index::Top);
	const auto end = state_info::IsFinal(state_info::StateInfo(top)) ? top : stateAt(LivingAction::Index::Final);
	const auto row = static_cast<size_t>(static_cast<uint8_t>(end));
	const auto& rows = ecs::villager::k_StateFunctionIds;
	return row < rows.size() && rows.at(row).exit == ecs::villager::k_ExitFootball;
}

/// What play and curiosity look at in a thing to act on: a toy (the football, or a mobile static with a toy's mesh), a
/// villager whose state is one of a football match, and a villager whose state shows something the creature could learn
/// by watching and has not learnt. No kind of object is kept as looked over, so none counts as one
creature_planner::ActOnFacts ActOnFactsOf(const ecs::Registry& registry, entt::entity thing,
                                          const creature_mind_model::Learnt& learnt,
                                          const std::optional<creature_tree::Belief>& belief)
{
	creature_planner::ActOnFacts facts;
	// An object of a kind it has had a good look at
	facts.kindLookedOver = belief.has_value() &&
	                       creature_planner::IsLookedOverAsAKind(belief->type, registry.AllOf<Mist>(thing)) &&
	                       learnt.lookedOverKinds.contains(belief->type);
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
	// The football match is told by the state it ends in
	facts.playingFootball = PlaysFootball(*action);
	const auto top = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Top)));
	// What it does now that could be learnt by watching
	const auto skill = state_info::Interesting(state_info::StateInfo(top));
	const auto& known = learnt.knowledge.skillsKnown;
	facts.doingSomethingNew = skill != k_NothingToLearn && !(skill < known.size() && known.at(skill));
	return facts;
}

/// Whether a thing is part of a temple: its heart, a worship site or a site's altar
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

/// The area a creature is kept within, when it is: the point it is kept at, across the ground, and the radius
struct KeptArea
{
	glm::vec2 centre {0.0f};
	float radius {0.0f};
};
std::optional<KeptArea> KeptAreaOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto radius = KeptWithin(registry, creature);
	if (!radius.has_value())
	{
		return std::nullopt;
	}
	const auto& centre = registry.Get<const CreatureLeash>(creature).confinementCentre;
	return KeptArea {.centre = {centre.x, centre.z}, .radius = *radius};
}

/// Whether the creature, kept within an area, does not take an action whose plan was found to walk it out of the area
bool UnavailableWhileKept(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind, uint32_t action)
{
	return action < mind.unavailableWhenConfined.size() && mind.unavailableWhenConfined[action] &&
	       KeptWithin(registry, creature).has_value();
}

/// The points a plan's walks go to: a walk to a point, and a walk to a place about where the creature stands now
std::vector<glm::vec2> WalksTo(std::span<const creature_mind::Step> agenda, glm::vec2 from)
{
	std::vector<glm::vec2> points;
	for (const auto& step : agenda)
	{
		if (step.kind != creature_mind::Step::Kind::Move)
		{
			continue;
		}
		if (step.movement.kind == creature_mind::Movement::Kind::ToPoint)
		{
			points.push_back(step.movement.point);
		}
		else if (step.movement.kind == creature_mind::Movement::Kind::Nearby)
		{
			points.push_back(from + step.movement.point);
		}
	}
	return points;
}

/// A thing to act on for a desire, as the planner weighs it: what the creature has learnt of it, the distance, whether it
/// is held, whether it is out of reach of a creature kept within an area, and the desire's factor for it
creature_planner::ObjectCandidate ActOnCandidate(const ecs::Registry& registry, entt::entity creature, glm::vec2 position,
                                                 const creature_mind_model::Learnt& learnt, Desire desire, entt::entity thing,
                                                 std::optional<float> keptWithin)
{
	const auto distance = game_math::GetDistanceInMetres(PointOf(registry, thing).value_or(position), position);
	const auto* held = registry.TryGet<const HeldByCreature>(thing);
	const auto belief = BeliefOf(registry, thing, creature);
	return {
	    .id = entt::to_integral(thing),
	    .distance = distance,
	    .held = held != nullptr && held->creature == creature,
	    .usefulness = Usefulness(learnt, TreeKind::ActOn, desire, belief),
	    .goalUsefulness = Usefulness(learnt, TreeKind::Goal, desire, belief),
	    .outOfReach = creature_planner::BeyondKeptArea(desire, keptWithin, distance, IsTemplePart(registry, thing)),
	    .actOnFactor = creature_planner::ActOnFactor(desire, ActOnFactsOf(registry, thing, learnt, belief)),
	};
}
} // namespace

float mind_detail::GoalUsefulnessOf(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind,
                                    const creature_mind_tables::Tables& tables, Desire desire, entt::entity thing)
{
	if (!mind.learnt.has_value())
	{
		return 0.0f;
	}
	const bool isCreature = registry.AllOf<Creature>(thing);
	creature_planner::GoalFacts facts {
	    .searchesForGoal = tables.rules.at(static_cast<size_t>(desire)).searchesForGoal,
	    .isCreature = isCreature,
	    .partsBelieved = mind.beliefs.CountBelongingTo(thing),
	};
	// Its leash tied to a thing it has a belief about: only what that thing belongs to counts
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	    leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo.has_value() &&
	    registry.Valid(*leash->worn->tiedTo) && mind.beliefs.Find(*leash->worn->tiedTo) != nullptr)
	{
		const auto whole = CreatureMindSystem::BelongsTo(*leash->worn->tiedTo);
		facts.leashTiedElsewhere = whole.has_value() && *whole != thing;
	}
	// Where the thing is now: the creature keeps no memory of where it saw it last
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* thingAt = registry.TryGet<const Transform>(thing);
	if (at != nullptr && thingAt != nullptr)
	{
		facts.beyondKeptArea = creature_planner::BeyondKeptArea(desire, KeptWithin(registry, creature),
		                                                        game_math::GetDistanceInMetres(at->position, thingAt->position),
		                                                        IsTemplePart(registry, thing));
	}
	facts.aFriend = isCreature && AreFriends(registry, creature, thing);
	return creature_planner::GoalUsefulness(
	    desire, facts, Usefulness(*mind.learnt, TreeKind::Goal, desire, BeliefOf(registry, thing, creature)));
}

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

/// The one-shot miracle that a globe holds, as a creature sees it: the first miracle of the globe's seed, whatever its
/// power-up, with that miracle's one-shot marks; none for a thing that is not a globe
std::optional<creature_plan_actions::OneShot> OneShotOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* globe = registry.Valid(entity) ? registry.TryGet<const OneOffSpellSeed>(entity) : nullptr;
	if (globe == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto type = magic::GetSpellSeedInfo(info, globe->seedType).magicTypes[0];
	const auto& miracle = magic::GetMagicInfo(info, type);
	return creature_plan_actions::OneShot {.magicType = static_cast<uint32_t>(type),
	                                       .aggressive = miracle.oneOffSpellIsAggressive != 0,
	                                       .compassionate = miracle.oneOffSpellIsCompassionate != 0,
	                                       .playful = miracle.oneOffSpellIsPlayful != 0,
	                                       .restoreHealth = miracle.oneOffSpellIsToRestoreHealth != 0};
}
} // namespace

bool mind_detail::IsDoingSomethingInteresting(const ecs::Registry& registry, entt::entity thing, entt::entity creature)
{
	// What the villager does now shows a skill the creature does not know: the one thing a creature follows to look
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return registry.AllOf<Villager>(thing) && mind != nullptr && mind->learnt.has_value() &&
	       ActOnFactsOf(registry, thing, *mind->learnt, std::nullopt).doingSomethingNew;
}

bool mind_detail::LetsCreatureAct(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return false;
	}
	if (registry.AnyOf<Villager, Animal, Creature, Spell>(thing))
	{
		return true;
	}
	// (a big forest, no forest of its own since it is believed in as the plain base, is still let not act as before)
	if (registry.AnyOf<WorshipSite, Town, Flock, Forest, BigForest>(thing))
	{
		return false;
	}
	if (registry.AllOf<Temple>(thing))
	{
		return Locator::infoConstants::has_value() &&
		       Locator::infoConstants::value().citadelHeart.canCreatureInteractWithMe != 0;
	}
	// An object whose info row openblack does not find (a class whose row is not looked up yet) is let as before
	const auto* info = ecs::fire::traits::InfoOf(thing);
	return info == nullptr || info->canCreatureInteractWithMe != 0;
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
		return creature_eating::IsFoodPileOutsideStoragePit(
		    true, ecs::object::Get2DRadius(pit), game_math::GetDistanceInMetres(ecs::object::MapCoordsOf(pit), coords));
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
		       game_math::GetDistanceInMetres(glm::vec2(home->x, home->z), *at) > k_ToyAwayFromHome;
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
		    !(game_math::GetDistanceInMetres(*at, mind->copiedAt) > k_PlantFarFromCopied))
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
		const auto metres = game_math::GetDistanceInMetres(*at, glm::vec2(home->x, home->z));
		return creature_bring_home::CanBeBroughtHome(metres, registry.AnyOf<Villager, Animal, Creature>(entity),
		                                             hands->CanPickUp(self, entity));
	}
	case Target::Destroyable:
		return hands != nullptr && hands->CanDestroy(entity);
	case Target::Kickable:
		return hands != nullptr && hands->CanKick(self, entity);
	case Target::Tree:
		return registry.AllOf<Tree>(entity) && hands != nullptr && hands->CanDestroy(entity);
	case Target::Stompable:
	{
		// By the test of the thing's class: a villager or an animal while it lives, a bird low enough under the
		// creature's height, another creature by the two creatures' heights and what it is doing, a building low enough
		// under the creature's height, a tree and what is set into the land by its height for the creature's size,
		// anything lying about always, and never a field, a fish farm, a vortex or a teleport stone
		if (hands == nullptr)
		{
			return false;
		}
		using creature_object_actions::StompRule;
		const auto kind = hands->KindOf(entity);
		creature_object_actions::StompTarget stomped {
		    .rule = creature_object_actions::StompRuleOf(kind.thing, kind.stompGround),
		};
		switch (stomped.rule)
		{
		case StompRule::Never:
		case StompRule::Always:
			break;
		case StompRule::WhileAlive:
			stomped.dead = kind.thing == creature_object_actions::Thing::Villager ? ecs::villager::IsDead(entity)
			                                                                      : ecs::animal_ai::IsDead(entity);
			break;
		case StompRule::LowBird:
			stomped.lowBird = Locator::animalSystem::has_value() &&
			                  Locator::animalSystem::value().CanBeStompedOnByCreature(entity, ecs::object::GetHeight(self));
			break;
		case StompRule::Creature:
		{
			// Whether the other creature's plan, as others see it, is of an action of recovery
			const auto* tables =
			    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
			const auto* mind = registry.TryGet<const CreatureMindState>(entity);
			const auto plan = mind != nullptr ? mind_detail::HeldPlan(*mind, tables) : std::nullopt;
			stomped.recovering = plan.has_value() && tables != nullptr && plan->action < tables->actions.size() &&
			                     tables->actions.at(plan->action).fullRecovery;
			[[fallthrough]];
		}
		case StompRule::OnTheLand:
		case StompRule::LowBuilding:
			stomped.inMap = ecs::map_cells::IsObjectInMap(entity);
			stomped.height = ecs::object::GetHeight(entity);
			break;
		}
		return creature_object_actions::CanBeStompedOn(stomped, ecs::creature_pose::DrawnSize(registry, self),
		                                               ecs::object::GetHeight(self));
	}
	case Target::Villager:
		return registry.AllOf<Villager>(entity);
	case Target::Strokeable:
		return mind_detail::Strokeable(registry, entity, self);
	case Target::DanceVillager:
	{
		// Measured across the land between map positions, as the game measures it
		const auto* at = registry.TryGet<const Transform>(entity);
		const auto* from = registry.TryGet<const Transform>(self);
		return registry.AllOf<Villager>(entity) && at != nullptr && from != nullptr &&
		       game_math::GetDistanceInMetres(from->position, at->position) < creature_dance::k_FurthestVillager;
	}
	case Target::HurtVillager:
		// Hurt to seven tenths of its life or less, or with a life that is no number
		return registry.AllOf<Villager>(entity) && !(ecs::life::LifeOf(entity) > k_HurtEnoughToHeal);
	case Target::Creature:
		return registry.AllOf<Creature>(entity);
	case Target::FightableCreature:
	{
		// A creature, unless its current plan is to die or to lie dead for good, as the fight row's object test
		if (!registry.AllOf<Creature>(entity))
		{
			return false;
		}
		const auto* mind = registry.TryGet<const CreatureMindState>(entity);
		const auto* tables =
		    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
		if (mind == nullptr || tables == nullptr || !mind->planner.current.has_value() ||
		    mind->planner.current->action >= tables->actions.size())
		{
			return true;
		}
		const std::string_view action = tables->actions[mind->planner.current->action].name;
		return action != creature_friendship::k_DieAction && action != creature_friendship::k_DeadForeverAction;
	}
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
	case Target::InterestingVillager:
		return IsDoingSomethingInteresting(registry, entity, self);
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
	case Target::PooDesireThing:
	case Target::Pooable:
	{
		// the poo desire's own test: no living thing, and none of the things only placed on the map, and taller than a
		// metre and a half; a poo itself is not done on a field or a fish farm
		constexpr float k_PooableHeight = 1.5f;
		// (a big forest is still left out as it was when it carried the forest's component)
		if (registry.AnyOf<Villager, Animal, Creature, Town, Forest, BigForest, ecs::components::Spell>(entity) ||
		    !(object_measures::Height(registry, entity) > k_PooableHeight))
		{
			return false;
		}
		return target == Target::PooDesireThing || !registry.AnyOf<Field, FishFarm>(entity);
	}
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
	case Target::OneShotAggressive:
	case Target::OneShotCompassionate:
	case Target::OneShotPlayful:
	case Target::OneShotRestoreHealth:
	{
		// A globe whose miracle is of the kind; the game asks nothing else of it
		const auto style = target == Target::OneShotAggressive      ? creature_mind::OneShotStyle::Aggressive
		                   : target == Target::OneShotCompassionate ? creature_mind::OneShotStyle::Compassionate
		                   : target == Target::OneShotPlayful       ? creature_mind::OneShotStyle::Playful
		                                                            : creature_mind::OneShotStyle::RestoreHealth;
		const auto oneShot = OneShotOf(registry, entity);
		return !heldByOther && oneShot.has_value() && oneShot->Is(style);
	}
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
	case Target::Footballer:
	{
		// A villager playing or watching a match, as the match's actions ask
		const auto* action = registry.TryGet<const LivingAction>(entity);
		return action != nullptr && registry.AllOf<Villager>(entity) && Locator::infoConstants::has_value() &&
		       PlaysFootball(*action);
	}
	case Target::FootballerMySideScored:
	case Target::FootballerOtherSideScored:
	{
		// A footballer of a match whose goal was just scored, by the side the creature's player plays for or not
		const auto* action = registry.TryGet<const LivingAction>(entity);
		const auto* player = registry.TryGet<const Villager>(entity);
		const auto* body = registry.TryGet<const Creature>(self);
		if (action == nullptr || player == nullptr || body == nullptr || !Locator::infoConstants::has_value() ||
		    !PlaysFootball(*action))
		{
			return false;
		}
		const auto* match = registry.TryGet<const FootballPitch>(player->footballPitch);
		if (match == nullptr || match->playState != FootballPitch::PlayState::GoalScored)
		{
			return false;
		}
		return creature_mind::IsItsSidesGoal(match->scoredSide, static_cast<uint32_t>(body->owner)) ==
		       (target == Target::FootballerMySideScored);
	}
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
	// The thing's kind first, then the creature's pick-up test, then its player's influence, each asked only when the
	// one before passed, as the game asks them
	const bool toy = ToyOf(registry, thing).has_value();
	const bool scaffold = registry.AllOf<Scaffold>(thing);
	const bool oneShot = registry.AllOf<OneOffSpellSeed>(thing);
	if (!creature_steal::IsStealableKind(toy, scaffold, oneShot) ||
	    !Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing))
	{
		return false;
	}
	return creature_steal::IsStealable({
	    .toy = toy,
	    .scaffold = scaffold,
	    .oneShot = oneShot,
	    .canPickUp = true,
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
	const bool anything = target == Target::Anything || target == Target::Examinable || target == Target::Playable ||
	                      target == Target::Burning || target == Target::PooDesireThing || target == Target::Pooable;
	const bool sea = target == Target::SeaThrowable;
	// Of pots and piles, food is eaten, and any may be picked up or thrown into the sea
	const bool pickable = target == Target::Pickable || target == Target::ThrowableToImpress;
	// What it may kick: anything that can fly or is a home, as long as the kick's own test allows it
	const bool kickable = target == Target::Kickable;
	// What it may stomp on: whatever the test of its class may take
	const bool stompable = target == Target::Stompable;
	if (target == Target::Food || target == Target::FoodPile || pickable || sea || anything ||
	    target == Target::BringableHome || kickable || stompable)
	{
		std::as_const(registry).Each<const Pot, const Transform>(
		    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	}
	// Whatever else it may pick up: the football, statics and rocks, dead trees, trees and mushrooms. What it may
	// steal: the toys among them, the scaffolds (mobile objects) and the one-shot miracles' globes
	const bool stealable = target == Target::Stealable;
	if (pickable || target == Target::Toy || target == Target::ToyBall || sea || anything || stealable || kickable || stompable)
	{
		std::as_const(registry).Each<const Ball, const Transform>(
		    [&](entt::entity entity, const Ball&, const Transform& at) { consider(entity, at); });
	}
	const bool statics = pickable || target == Target::Toy || target == Target::ToyBall || sea || target == Target::Stonable ||
	                     target == Target::ThrowingDamage || target == Target::PickableRock || target == Target::Rock ||
	                     target == Target::CuddlyToy || target == Target::Die || target == Target::ToyAwayFromHome ||
	                     anything || stealable || kickable || stompable;
	if (statics)
	{
		std::as_const(registry).Each<const MobileStatic, const Transform>(
		    [&](entt::entity entity, const MobileStatic&, const Transform& at) { consider(entity, at); });
		std::as_const(registry).Each<const DeadTree, const Transform>(
		    [&](entt::entity entity, const DeadTree&, const Transform& at) { consider(entity, at); });
	}
	if (stealable || target == Target::OneShotAggressive || target == Target::OneShotCompassionate ||
	    target == Target::OneShotPlayful || target == Target::OneShotRestoreHealth)
	{
		std::as_const(registry).Each<const OneOffSpellSeed, const Transform>(
		    [&](entt::entity entity, const OneOffSpellSeed&, const Transform& at) { consider(entity, at); });
	}
	if (pickable || target == Target::Stonable || anything || target == Target::Mushroom || stompable)
	{
		std::as_const(registry).Each<const Feature, const Transform>(
		    [&](entt::entity entity, const Feature&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || target == Target::Stompable || target == Target::Tree || anything ||
	    target == Target::Sprinklable || target == Target::AnyTree || target == Target::Examinable ||
	    target == Target::Pooable || target == Target::PooDesireThing || pickable || sea || target == Target::PickableTree ||
	    target == Target::PlantableTree || target == Target::TreeBigEnough || kickable)
	{
		std::as_const(registry).Each<const Tree, const Transform>(
		    [&](entt::entity entity, const Tree&, const Transform& at) { consider(entity, at); });
	}
	if (target == Target::Destroyable || target == Target::Stompable || anything || target == Target::Stonable ||
	    target == Target::Examinable || target == Target::Pooable || target == Target::PooDesireThing || kickable)
	{
		std::as_const(registry).Each<const Abode, const Transform>(
		    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	}
	// What else is built or set into the land, which thrown things can knock about and a stomp may take
	if (target == Target::Stonable || anything || stompable)
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
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	// Each action's opinion starts as the action table has it
	mind.learnt = tables != nullptr ? creature_mind_model::Fresh(*mind.desires, creature_mind_tables::StartingOpinions(*tables),
	                                                             std::move(knowledge))
	                                : creature_mind_model::Fresh(*mind.desires, 0, std::move(knowledge));
	// How long since it did each action starts at the draws of 120 its making took, one an action
	for (size_t action = 0; action < mind.learnt->turnsSinceDone.size() && action < mind.previousActions.size(); ++action)
	{
		mind.learnt->turnsSinceDone[action] = mind.previousActions[action];
	}
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
	auto freshLearnt =
	    tables != nullptr
	        ? creature_mind_model::Fresh(fresh, creature_mind_tables::StartingOpinions(*tables), std::move(knowledge))
	        : creature_mind_model::Fresh(fresh, mind.learnt->opinions.size(), std::move(knowledge));
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
	// The kinds of thing it has looked over are the creature's own and in no mind file: a mind taken up by a body that
	// already has one (a swap) leaves them as they were
	loaded.learnt.lookedOverKinds = std::move(mind.learnt->lookedOverKinds);
	mind.learnt = std::move(loaded.learnt);
	// Whatever the file says, its anger spares its own player's things again
	mind.angerSparesOwnPlayer = true;
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
	// What it has counted of its life is the file's: its kills, battles, poos and mushrooms
	{
		auto& registry = Locator::entitiesRegistry::value();
		SetCounts(registry, creature, creature_mind_model::CountsFromFile(*file, CountsOf(registry, creature)));
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} took up a mind file: version {}, stage {}",
	                   entt::to_integral(creature), file->version, mind.developmentPhase);
	// A file the creature-file load gave it tells the player the stage it holds, before any stage a script set after it
	if (std::exchange(mind.fileTellsStage, false))
	{
		if (const auto kind = creature_help::StageMessageOf(mind.developmentPhase); kind.has_value())
		{
			TellHelp(creature, {.kind = *kind});
		}
	}
}

bool CreatureMindSystem::PlayerLooksAt(entt::entity creature)
{
	// The camera of its own god, the game's for the player at this machine
	const auto* at = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	const auto camera = ecs::god_camera::Of(creature);
	if (at == nullptr || !camera.has_value())
	{
		return false;
	}
	return creature_learning::LooksAt(camera->origin, camera->focus, at->position);
}

void CreatureMindSystem::AgeOnStop(CreatureMindState& mind)
{
	const auto* tables =
	    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	if (mind.planner.current.has_value() && mind.learnt.has_value() && tables != nullptr)
	{
		const auto& plan = *mind.planner.current;
		creature_planner::AgeOnStop(mind.learnt->turnsSinceDone, tables->desireActions.at(static_cast<size_t>(plan.desire)),
		                            plan.action, mind.turnsPerformingAction);
	}
	mind.turnsPerformingAction = 0;
	mind.turnsWithoutAction = 0;
	// As the action ends, a fight it set out on is over for its orders
	mind.fightStarted = false;
}

void CreatureMindSystem::Abandon(CreatureMindState& mind)
{
	// What it learns from stays unfinished: only a plan carried out to its end finishes
	mind.planActive = false;
	AgeOnStop(mind);
	creature_planner::StopPlan(mind.planner);
	// An emergency it was in is over, and so is an order of its player's it carried out
	mind.emergencyRunning = false;
	mind.startActionAgain = false;
	mind.setOffSinceStop = false;
	mind.commanded = false;
	mind.fightEnded = false;
	mind.forcedByPlayerAction = false;
	mind.controlledByTeleport = false;
	// and it is no longer shut in
	mind.trapped = false;
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
	// A step's thing that is there but no longer available: being deleted, or a villager whose final state is dying
	const auto unavailable = [&registry](uint32_t thing) {
		const auto entity = static_cast<entt::entity>(thing);
		return registry.Valid(entity) &&
		       (!ecs::IsAvailable(entity) || (registry.AllOf<Villager>(entity) && !ecs::villager::IsAvailable(entity)));
	};
	if (!creature_lost_thing::Lost(inUse, gone, unavailable))
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
	// A plan whose steps a script's question cut off is not over: it stays a turn as it was, then goes with no end of its
	// own, as one with no steps whose priority is nothing
	const bool planOfThisAgenda = mind.planActive && idle.serial == mind.planSerial && idle.step >= idle.agenda.size();
	const auto stopped =
	    creature_script_play::FollowStoppedPlan(idle, planOfThisAgenda, ecs::script_held::IsControlledByScript(creature));
	if (stopped == creature_script_play::StoppedPlan::Goes)
	{
		mind.planActive = false;
		AgeOnStop(mind);
		creature_planner::StopPlan(mind.planner);
	}
	if (stopped == creature_script_play::StoppedPlan::None && mind.planActive &&
	    (idle.serial != mind.planSerial || idle.step >= idle.agenda.size()))
	{
		// The plan is over. Carried out to its end, it counts towards satisfying the desire it served; cut short by
		// something else (fainting, a fight, a more pressing plan) or given up, it does nothing for it.
		const auto plan = *mind.planner.current;
		const bool carriedOut = idle.serial == mind.planSerial && !idle.gaveUp;
		// The scripts count it as its action carried out, once, even when a step of its agenda completed it already
		if (carriedOut)
		{
			// While it copies its player, an action carried out to its end counts towards the stage of copying
			creature_watching::CountFinishedAction(mind.learnt->mimicry);
			// The fight counts as the fight system finds it won on life, not here
			if (creature_training::CountedAtItsAgendaEnd(plan.action))
			{
				creature_training::CountCarriedOut(mind.actionCounts, plan.action,
				                                   ecs::script_held::IsControlledByScript(creature));
				if (!ecs::script_held::IsControlledByScript(creature))
				{
					mind.actionTurns[plan.action] = game_clock::Turn();
				}
			}
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
		// Finished or failed, its action has just been done; cut short by something else, it has not
		if (idle.serial == mind.planSerial && plan.action < mind.learnt->turnsSinceDone.size())
		{
			mind.learnt->turnsSinceDone[plan.action] = 0;
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
		// finished or failed, its action has just been done
		if (mind.underway->action < mind.learnt->turnsSinceDone.size())
		{
			mind.learnt->turnsSinceDone[mind.underway->action] = 0;
		}
		if (!idle.gaveUp)
		{
			creature_watching::CountFinishedAction(mind.learnt->mimicry);
			if (creature_training::CountedAtItsAgendaEnd(mind.underway->action))
			{
				creature_training::CountCarriedOut(mind.actionCounts, mind.underway->action,
				                                   ecs::script_held::IsControlledByScript(creature));
				if (!ecs::script_held::IsControlledByScript(creature))
				{
					mind.actionTurns[mind.underway->action] = game_clock::Turn();
				}
			}
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
	mind.underway = CreatureMindState::ActionUnderway {.serial = idle.serial, .action = *action};
}

namespace
{
/// Whether a creature kept within an area by its leash has the thing farther from it than the area's radius
bool OutsideConfinement(const ecs::Registry& registry, entt::entity creature, entt::entity object);

/// Whether a creature may use a thing for an action, as the game asks it of what it knows: not a thing it found no way
/// to lately, nor one outside the area its leash keeps it within, and one a creature may act on at all
bool SuitableToUse(const ecs::Registry& registry, CreatureMindState& mind, entt::entity creature, entt::entity thing)
{
	const uint32_t msPerTurn = game_clock::MsPerTurn();
	if (auto* belief = mind.beliefs.Find(thing);
	    belief != nullptr && msPerTurn != 0 &&
	    creature_object_beliefs::BlockedRecently(*belief, game_clock::Turn(), 1000 / msPerTurn,
	                                             ecs::map_cells::IsMultiCellStaticClass(thing)))
	{
		return false;
	}
	return !OutsideConfinement(registry, creature, thing) && mind_detail::LetsCreatureAct(registry, thing);
}

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
	return from.has_value() && at.has_value() && game_math::GetDistanceInMetres(*at, *from) > kept->confinementRadius;
}

/// The thing a creature at a point uses for an action, of a kind, other than the goal: nothing farther than the area its
/// leash keeps it within, nor another creature
std::optional<uint32_t> UsedFor(const ecs::Registry& registry, entt::entity creature, Target use, glm::vec2 position,
                                float distanceWeight, std::optional<uint32_t> goal)
{
	std::vector<creature_planner::ObjectCandidate> candidates;
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	for (const auto& found : mind_detail::Gather(registry, use, creature, position))
	{
		// Not another creature, nor a thing it may not use for an action as the game asks it
		if (registry.AllOf<Creature>(found.entity) || OutsideConfinement(registry, creature, found.entity) ||
		    (mind != nullptr && !SuitableToUse(registry, *mind, creature, found.entity)))
		{
			continue;
		}
		const auto* held = registry.TryGet<const HeldByCreature>(found.entity);
		candidates.push_back({
		    .id = entt::to_integral(found.entity),
		    .distance = game_math::GetDistanceInMetres(found.point, position),
		    .held = held != nullptr && held->creature == creature,
		});
	}
	return creature_planner::ChooseUsed(candidates, distanceWeight, goal);
}

/// What the agenda knows of the creature's hands and where it stands: what it holds (a tree or not), where it is, and
/// where the thing it uses is and its radius on the ground; for the actions done with things, its height and radius
/// too. Whether its hand is full stays as the caller says
/// For the match's actions: the footballer's match, its ball, the goal the creature's side attacks (an even player
/// number plays for the home side, which attacks the second goal mouth) and the other side's first player
void SituateFootball(const ecs::Registry& registry, entt::entity creature, const creature_plan_actions::Executor& executor,
                     std::optional<uint32_t> object, creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	if ((executor.build != Build::ThrowBallAtGoal && executor.build != Build::FoulPlayer) || !object.has_value())
	{
		return;
	}
	const auto* player = registry.TryGet<const Villager>(static_cast<entt::entity>(*object));
	const auto* body = registry.TryGet<const Creature>(creature);
	if (player == nullptr || body == nullptr || !ecs::football::IsPitch(player->footballPitch))
	{
		return;
	}
	const auto pitch = player->footballPitch;
	const auto ball = ecs::football::GetBall(pitch);
	const auto* match = registry.TryGet<const FootballPitch>(pitch);
	if (ball == entt::null || match == nullptr)
	{
		return;
	}
	const bool home = creature_mind::IsOnHomeSide(static_cast<uint32_t>(body->owner));
	const auto& others = home ? match->awayTeam : match->homeTeam;
	situation.football = creature_plan_actions::Situation::FootballMatch {
	    .ball = entt::to_integral(ball),
	    .goal = map_coords::ToWorld(ecs::football::GoalPosition(pitch, home ? 1u : 0u)),
	    .opponent = others.empty() ? std::nullopt : std::optional(entt::to_integral(others.front())),
	};
}

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
	situation.holdingStrokeable = held.has_value() && mind_detail::Strokeable(registry, *held, creature);
	if (held.has_value())
	{
		const auto thing = Locator::creatureObjectActionSystem::value().KindOf(*held).thing;
		situation.holdingTree = thing == creature_object_actions::Thing::Tree;
		situation.holdingMushroom = IsMushroom(registry, *held);
		situation.heldOneShot = OneShotOf(registry, *held);
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

void CreatureMindSystem::SeeCameraFor(entt::entity creature, creature_plan_actions::Situation& situation)
{
	SeeCamera(std::as_const(Locator::entitiesRegistry::value()), creature, situation);
}

std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
CreatureMindSystem::PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
                               const creature_plan_actions::Situation& situation, const creature_mind::Random& random)
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
	// the height of the thing acted on, as a walk to it keeps it
	if (object.has_value())
	{
		if (const auto* at = Locator::entitiesRegistry::value().TryGet<const Transform>(static_cast<entt::entity>(*object));
		    at != nullptr)
		{
			local.objectAltitude = at->position.y;
		}
	}
	SituateExploration(creature, *executor, local);
	// Running from or going to look at where a miracle stood: the place its mind keeps
	if (executor->build == creature_plan_actions::Build::RunFromPlace ||
	    executor->build == creature_plan_actions::Build::ExaminePlace)
	{
		if (const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature))
		{
			local.miraclePlace = mind->miraclePlace;
		}
		// running from it, the point it runs to is picked as the plan is made
		if (executor->build == creature_plan_actions::Build::RunFromPlace && Locator::creatureLocomotionSystem::has_value())
		{
			local.pickRunAwayPoint = [creature](glm::vec2 place, uint32_t extra) {
				return Locator::creatureLocomotionSystem::value().RunAwayPointFrom(creature, place, extra);
			};
		}
	}
	// Hanging around at home: the area that holds it sets how far out it goes
	if (executor->build == creature_plan_actions::Build::HangAround)
	{
		if (const auto* leash = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature))
		{
			local.confinementRadius = leash->confinementRadius;
		}
	}
	// The rest and home actions and the actions with its player that first follow the camera go near by the creature's own
	// height
	if (creature_plan_actions::WalksByItsHeight(executor->build) || FollowsTheCamera(executor->build))
	{
		local.height = object_measures::Height(Locator::entitiesRegistry::value(), creature);
	}
	// Looking for a friend goes near it by the size the creature is drawn at
	if (executor->build == creature_plan_actions::Build::LookForFriend)
	{
		local.drawnSize = ecs::object::GetScale(creature);
	}
	if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
	{
		SituateDance(creature, *executor, point, local);
	}
	// Looking a thing over goes near it by the thing's height when that is the farther
	if (executor->build == creature_plan_actions::Build::ExamineByLooking && object.has_value())
	{
		local.objectHeight = object_measures::Height(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object));
	}
	// Showing another creature it hates it goes near by the taller one's height
	if (executor->build == creature_plan_actions::Build::ShowHate && object.has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		local.height = object_measures::Height(registry, creature);
		local.objectHeight = object_measures::Height(registry, static_cast<entt::entity>(*object));
	}
	// Telling another creature to go away points past it by where the two stand, and sends it on by its height
	if (executor->build == creature_plan_actions::Build::TellToGoAway && object.has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto other = static_cast<entt::entity>(*object);
		if (const auto* at = registry.TryGet<const Transform>(other))
		{
			local.objectStanding = at->position;
		}
		local.objectHeight = object_measures::Height(registry, other);
	}
	// Showing another creature how nice it reckons it is reads what it thinks of it, an attitude made for it if it has
	// none yet
	if (executor->build == creature_plan_actions::Build::ShowHowNiceToCreature && object.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* mind = registry.TryGet<CreatureMindState>(creature);
		const auto other = static_cast<entt::entity>(*object);
		if (registry.Valid(other) && registry.AllOf<Creature>(other) && mind != nullptr && mind->learnt.has_value())
		{
			local.howNice = creature_learning::AttitudeTo(mind->learnt->creatures, *object).howNice;
		}
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
	// Choosing to sit by itself: a clear place about the thing four times as wide as its 2D radius, things living in
	// it or not; it walks to within its 2D radius of it
	if (executor->build == creature_plan_actions::Build::SitDown)
	{
		local.controlledByScript = local.controlledByScript || ecs::script_held::IsControlledByScript(creature);
		if (!local.controlledByScript)
		{
			const auto& registry = Locator::entitiesRegistry::value();
			local.radius = ecs::object::Get2DRadius(creature);
			// A sit with no thing of its own is about the creature itself
			const auto about = object.has_value() ? std::optional(point) : PointOf(registry, creature);
			if (about.has_value())
			{
				local.clearArea = FindClearArea(registry, creature, *about, k_SitClearAreaRadii * local.radius, false);
			}
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
	SituateFootball(Locator::entitiesRegistry::value(), creature, *executor, object, local);
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
	// A one-shot miracle: the globe that it uses, picked up only with an empty hand, cast from its own height
	if (executor->build == creature_plan_actions::Build::CastOneShotInHand ||
	    executor->build == creature_plan_actions::Build::PickUpAndCastOneShot ||
	    executor->build == creature_plan_actions::Build::CastRestoreHealthInHand ||
	    executor->build == creature_plan_actions::Build::PickUpAndCastRestoreHealth)
	{
		const auto& registry = Locator::entitiesRegistry::value();
		local.handFull = registry.AllOf<CreatureHeldObject>(creature);
		local.height = object_measures::Height(registry, creature);
		local.usedOneShot =
		    local.instrument.has_value() ? OneShotOf(registry, static_cast<entt::entity>(*local.instrument)) : std::nullopt;
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
	if (executor->build == creature_plan_actions::Build::FishAndEat)
	{
		local.fishing = FishingOf(std::as_const(Locator::entitiesRegistry::value()), creature, local.height);
	}
	if (executor->build == creature_plan_actions::Build::GiveFishToStore ||
	    executor->build == creature_plan_actions::Build::PutFishBySite ||
	    executor->build == creature_plan_actions::Build::TakeFishHome)
	{
		local.fishing = ShoalTripOf(std::as_const(Locator::entitiesRegistry::value()), creature, local.height);
	}
	SituateEatingFrom(Locator::entitiesRegistry::value(), creature, executor->build, object, local);
	// Whether what it acts on lives, for the agendas that stop a living thing for good
	if (object.has_value())
	{
		const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
		const auto thing = static_cast<entt::entity>(*object);
		local.objectLiving = lookup.Valid(thing) && lookup.AnyOf<Villager, Animal, Creature>(thing);
	}
	// the game's draws, unless the caller gives its own (a saved agenda made again draws nothing)
	const creature_mind::Random draw = random ? random : creature_mind::Random([](uint32_t range) { return Random(range); });
	auto agenda = creature_plan_actions::Agenda(*executor, object, point, local, draw, cast);
	// The fight's agenda draws the two creatures' taunts as it is made, the other creature's first. A saved agenda made
	// again draws none: the saved taunts are not read, and its fight draws its own as it starts
	if (agenda.has_value() && executor->build == creature_plan_actions::Build::Fight && !random)
	{
		creature_mind::DrawFightTaunts(*agenda, draw);
	}
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
	// Setting off into its player's temple, or to dance outside it, it comes to believe in the temple's heart
	if ((executor->build == creature_plan_actions::Build::EnterTemple ||
	     executor->build == creature_plan_actions::Build::DanceOutsideTemple) &&
	    local.templeHeart.has_value())
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature))
		{
			ComeAcross(creature, *mind, static_cast<entt::entity>(local.templeHeart->temple));
		}
	}
	// Kept within an area (the leash's full length about the hand or what it is tied to, or its home while it is young),
	// a plan that walks it out of the area can't be made, unless a script controls it or it goes to a teleport stone:
	// what it was doing ends, and it does not take the action again while it is kept within an area
	if (const auto kept = KeptAreaOf(registry, creature);
	    kept.has_value() && !ecs::script_held::IsControlledByScript(creature) &&
	    tables->actions[action].name != creature_leash::k_MayLeaveKeptAreaAction &&
	    creature_leash::LeavesKeptArea(WalksTo(*agenda, PointOf(registry, creature).value_or(glm::vec2(0.0f))), kept->centre,
	                                   kept->radius))
	{
		// What it was doing ends as an action ends unsuccessfully: a walk brakes to a stop and it does nothing, to decide
		// afresh
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
		}
		if (auto* body = registry.TryGet<CreatureAnimation>(creature))
		{
			body->body = {};
		}
		if (auto* mind = registry.TryGet<CreatureMindState>(creature))
		{
			Abandon(*mind);
			creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
			if (mind->unavailableWhenConfined.size() < tables->actions.size())
			{
				mind->unavailableWhenConfined.resize(tables->actions.size(), false);
			}
			mind->unavailableWhenConfined[action] = true;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {}'s plan {} would take it out of the area it is kept within",
		                    entt::to_integral(creature), tables->actions[action].name);
		return std::nullopt;
	}
	// The action whose agenda is made, a trace at run time (the game channel at trace level), not compiled out of release
	// builds, so that a moved run can name what the creature set out to do
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Creature {} plans {} on {} at turn {}", entt::to_integral(creature), tables->actions[action].name,
		              object.has_value() ? static_cast<int64_t>(*object) : -1, game_clock::Turn());
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

void CreatureMindSystem::OtherComesAcrossAsker(entt::entity creature, const CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.planActive || !mind.planner.current.has_value() ||
	    !mind.planner.current->object.has_value() || mind.planner.current->action >= tables->actions.size())
	{
		return;
	}
	// The fight is only ever forced, so it is looked up among the forced actions too
	const auto* executor = creature_plan_actions::ForcedFor(tables->actions[mind.planner.current->action].name);
	if (executor == nullptr || (executor->build != creature_plan_actions::Build::Fight &&
	                            executor->build != creature_plan_actions::Build::TellToGoAway))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = static_cast<entt::entity>(*mind.planner.current->object);
	if (other == creature || !registry.Valid(other) || !registry.AllOf<Creature>(other))
	{
		return;
	}
	if (auto* otherMind = registry.TryGet<CreatureMindState>(other))
	{
		ComeAcross(other, *otherMind, creature);
	}
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
	// Choosing to follow what its player seems to want makes no steps: what it was doing stops, it sets about copying
	// the deed it saw last again, which forces what it does at its next decision, and the choice is taken up for its
	// feedback as any plan is. A creature that never set about copying a deed has nothing to copy
	if (const auto* executor = creature_plan_actions::For(info.name);
	    executor != nullptr && executor->build == creature_plan_actions::Build::MimicPlayer)
	{
		if (!mind.observedDeed.has_value())
		{
			return false;
		}
		if (Locator::creatureObjectActionSystem::has_value() &&
		    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
		{
			Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
		}
		Abandon(mind);
		creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
		// The thing the player did it to may be gone since
		auto& observed = *mind.observedDeed;
		if (observed.object.has_value() && !registry.Valid(static_cast<entt::entity>(*observed.object)))
		{
			observed.object.reset();
		}
		creature_watching::CopyDeedAgain(mind.learnt->mimicry, observed.rule, tables->mimics, observed.object,
		                                 game_clock::Turn(), [](uint32_t range) { return Random(range); });
		// a thing like the one the deed was done to is of its kind
		if (observed.object.has_value())
		{
			mind.learnt->mimicType = ecs::MimicTypeOf(registry, static_cast<entt::entity>(*observed.object));
		}
		if (const auto copied =
		        observed.object.has_value() ? PointOf(registry, static_cast<entt::entity>(*observed.object)) : std::nullopt)
		{
			mind.copiedAt = *copied;
		}
		const auto source = creature_learning::PlanSource((*mind.desires)[plan.desire]);
		creature_learning::ResetDrives((*mind.desires)[plan.desire]);
		creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
		RememberAction(creature, mind,
		               {
		                   .action = plan.action,
		                   .desire = plan.desire,
		                   .learnable = info.learnable,
		                   .windowSeconds = info.learningWindowSeconds,
		                   .source = source.value_or(creature_learning::k_NoTellingSource),
		               });
		return false;
	}
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
		// A sit it chose by itself with no clear place to sit in is taken up and then fails at once, as an action whose
		// agenda can't be made: what it was doing stops as a failure
		if (const auto* chosenExecutor = creature_plan_actions::For(info.name);
		    chosenExecutor != nullptr && creature_plan_actions::TakenUpThenFailsWhenUnmade(
		                                     chosenExecutor->build, ecs::script_held::IsControlledByScript(creature)))
		{
			mind.planner.current = chosen;
			StopWhatItWasDoing(creature, mind);
		}
		return false;
	}
	auto& [executor, agenda] = *built;
	// What it was doing stops as a failure for the plan: its hands' animations on a thing play on to their end and a
	// point brings its arm down, as the body's object states do
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
	}
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	auto& learnt = *mind.learnt;
	mind.planner.current = chosen;
	mind.planActive = true;
	// A readout at run time (the game channel at debug, not compiled out of release builds): each plan the creature
	// takes up, the desire it serves with that desire's value and the plan's priority, so that a played run shows why it
	// chose what it did
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Creature {} takes up {} for {} (the desire at {:.3f}, the plan's priority {:.3f}), turn {}",
		              entt::to_integral(creature), info.name, creature_desires::Name(chosen.desire),
		              (*mind.desires)[chosen.desire].value, chosen.priority, game_clock::Turn());
	}
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	OtherComesAcrossAsker(creature, mind);
	// The source that says why it wants what the plan serves is asked before the drives are counted afresh
	const auto source = creature_learning::PlanSource((*mind.desires)[plan.desire]);
	creature_learning::ResetDrives((*mind.desires)[plan.desire]);
	creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
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
	return true;
}

void CreatureMindSystem::StartPlanAgain(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.planner.current.has_value() || mind.planner.current->action >= tables->actions.size())
	{
		return;
	}
	const auto plan = *mind.planner.current;
	// What it does ends as an action that failed: the stronger for a duel it is in, the action counted as just done
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().FailsWhileDueling(creature);
	}
	if (mind.learnt.has_value() && plan.action < mind.learnt->turnsSinceDone.size())
	{
		mind.learnt->turnsSinceDone[plan.action] = 0;
	}
	StopWhatItWasDoing(creature, mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	// The same plan's agenda is made again, as the planner's choice makes it, with what the plan uses and is about; the
	// plan is not taken up afresh: what it learns from and its desires stay as they are
	auto& registry = Locator::entitiesRegistry::value();
	creature_plan_actions::Situation situation;
	situation.self = entt::to_integral(creature);
	SeeCamera(registry, creature, situation);
	SituateHome(registry, creature, situation);
	SituatePlayer(registry, creature, situation);
	SituateSacrifice(registry, creature, situation);
	SituateTownTheft(registry, creature, situation);
	const auto* executor = creature_plan_actions::For(tables->actions[plan.action].name);
	if (executor != nullptr && executor->target == Target::CreatureFreeForJoint)
	{
		SituateFriends(creature, situation);
	}
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastWaterOnItself && Burning(creature))
	{
		situation.clearSpot = ClearSpotToCastOnItself(registry, creature);
	}
	situation.instrument = plan.instrument;
	situation.activity = creature_learning::PlanActivity(
	    plan.activityObject, tables->rules.at(static_cast<size_t>(plan.desire)).searchesForGoal, plan.object);
	auto built = PlanAgenda(creature, plan.action, plan.object, situation);
	if (!built.has_value())
	{
		return;
	}
	auto& [builtExecutor, agenda] = *built;
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
	}
	if (!Replan(creature, builtExecutor->activity, std::move(agenda)))
	{
		return;
	}
	mind.planner.current = plan;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Creature {} starts the plan of row {} again after the hand let go", entt::to_integral(creature),
		              plan.action);
	}
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

bool CreatureMindSystem::IsCommanded(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr && mind->commanded;
}

bool CreatureMindSystem::IsForcedByPlayerAction(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr && mind->forcedByPlayerAction;
}

void CreatureMindSystem::SetForcedByPlayerAction(entt::entity creature, bool forced)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		mind->forcedByPlayerAction = forced;
	}
}

void CreatureMindSystem::ComeAcrossThing(entt::entity creature, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind != nullptr && registry.Valid(thing))
	{
		ComeAcross(creature, *mind, thing);
	}
}

bool CreatureMindSystem::ObeyGodOrder(entt::entity creature, uint32_t action, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || action >= tables->actions.size())
	{
		return false;
	}
	// It comes across the thing; the thing's whole is what the plan is for
	const bool known = registry.Valid(object);
	const auto whole = known ? BelongsTo(object) : std::nullopt;
	if (known)
	{
		ComeAcross(creature, *mind, object);
	}
	// A desire made dominant ends, every desire let go
	EndDominantDesire(creature);
	const bool planned = ForceActivity(creature, {.desire = Desire::ObeyPlayer,
	                                              .activityObject = whole,
	                                              .action = tables->actions[action].name,
	                                              .actionObject = known ? object : entt::null});
	// A plan made is commanded. (The original never marks a plan as forced by its player's action, so a god's order
	// counts for no desire either.)
	if (planned)
	{
		registry.Get<CreatureMindState>(creature).commanded = true;
	}
	return true;
}

void CreatureMindSystem::LearnAction(entt::entity creature, uint32_t kind, uint32_t action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->learnt.has_value())
	{
		return;
	}
	// Known at once, the lesson told of a thing it did not know; a miracle's sightings stay as they are
	std::ignore =
	    ChangeKnows(creature, {.kind = kind, .action = action, .knows = true, .tell = true, .countsSightings = false});
}

bool CreatureMindSystem::KnowsAction(entt::entity creature, uint32_t kind, uint32_t action) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr && mind->learnt.has_value() && creature_watching::Knows(mind->learnt->knowledge, kind, action);
}

bool CreatureMindSystem::ForceActivity(entt::entity creature, const ForcedActivity& forced)
{
	auto& registry = Locator::entitiesRegistry::value();
	// What it was doing ends as an action finished unsuccessfully, with what the leash had it do after its walk
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().OrderGivenUp(creature);
	}
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
	// Asked as the planner asks its choice, an action it does not take while kept within an area is not taken
	if (forced.checkedAsChosen && UnavailableWhileKept(registry, creature, *mind, *action))
	{
		return false;
	}
	// The reaction it follows stops first, its record refreshed; then what it was doing stops as a failure, whatever
	// comes of the new plan: its hands' animations on a thing play on to their end with no step waiting for them and a
	// point brings its arm down, as the body's object states do, its plan is dropped and it lets go of a miracle it holds
	creature_reactions::StopReacting(creature);
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
	}
	if (forced.endsWhatItDoes)
	{
		EndFightAsItStops(creature);
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
		if (!Replan(creature, creature_mind::Activity::Planned, *forced.agenda, forced.endsWhatItDoes, forced.endsWhatItDoes))
		{
			return false;
		}
	}
	else
	{
		auto built = PlanAgenda(creature, *action, actionObject, situation);
		if (!built.has_value())
		{
			// The plan could not be made: it stops, a walk braking as the forcing ends what it did (unless it goes on with
			// it), doing nothing, to decide afresh
			if (forced.endsWhatItDoes)
			{
				if (Locator::creatureLocomotionSystem::has_value())
				{
					Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
				}
				// and the body's own action plays its end, as a new plan's does
				auto& played = registry.Get<CreatureAnimation>(creature).body;
				played = played.timedByPlayer ? creature_layers::BodyAction {}
				                              : creature_layers::EndDance(creature_layers::EndLoop(played));
			}
			creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
			// A forced sit with no clear place to sit in is taken up and then fails at once, as the original's: what it
			// was doing stops as a failure
			if (forcedExecutor != nullptr &&
			    creature_plan_actions::TakenUpThenFailsWhenUnmade(forcedExecutor->build, situation.controlledByScript))
			{
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
				StopWhatItWasDoing(creature, *mind);
			}
			return false;
		}
		auto& [executor, agenda] = *built;
		if (!Replan(creature, executor->activity, std::move(agenda), forced.endsWhatItDoes, forced.endsWhatItDoes))
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
	// An agenda given is not made, so the other comes across nothing
	if (!forced.agenda.has_value())
	{
		OtherComesAcrossAsker(creature, *mind);
	}
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
		        .ordered = forced.byPlayer,
		        .source = *source,
		    });
	}
	creature_learning::SuppressOpposed(*mind->desires, forced.desire, tables->dependencies, k_TurnsPerSecond);
	// Dying, it faints, lies, is carried home and comes round as a knock-out does, its mind leaving it alone meanwhile;
	// the agenda's step waits for it to be on its feet again
	if (const auto* dying = creature_plan_actions::ForcedFor(tables->actions[*action].name);
	    !forced.agenda.has_value() && dying != nullptr && dying->build == creature_plan_actions::Build::Die &&
	    Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().ForceDie(creature, forced.actionObject);
	}
	return true;
}

void CreatureMindSystem::MoveByTeleport(entt::entity creature, const glm::vec3& arrival, const glm::vec3& destination)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<CreatureMindState>(creature))
	{
		return;
	}
	// Kept within an area and not under a script's control, the walk on is left out when it ends farther out than the
	// area: the plan is no longer the one going to the stone, which alone may leave the area
	const glm::vec2 to {destination.x, destination.z};
	std::optional<glm::vec2> walk = to;
	if (const auto kept = KeptAreaOf(registry, creature);
	    kept.has_value() && !ecs::script_held::IsControlledByScript(creature) &&
	    creature_leash::LeavesKeptArea(std::array {to}, kept->centre, kept->radius))
	{
		walk.reset();
	}
	// It obeys its player, about itself, with the steps of the jump in place of the action's
	const bool planned = ForceActivity(creature, {.desire = Desire::ObeyPlayer,
	                                              .action = creature_mind::k_AfterTeleportAction,
	                                              .actionObject = creature,
	                                              .agenda = creature_mind::AfterTeleport({arrival.x, arrival.z}, walk)});
	// Commanded, and the stone controls it until it lets it go
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.commanded = true;
	mind.controlledByTeleport = true;
	// A trace at run time, as the plans' trace, so that a run can show the jump
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Creature {} is carried by a teleport stone to ({}, {}), then walks to {}: {}",
		              entt::to_integral(creature), arrival.x, arrival.z,
		              walk.has_value() ? fmt::format("({}, {})", walk->x, walk->y) : std::string("nowhere"),
		              planned ? "planned" : "no plan");
	}
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
	// The plan it was carrying out is kept unless it follows a reaction (which kept the plan before it)
	if (creature_reactions::ReactionOf(creature) == 0)
	{
		mind->previousPlan = mind->planner.current;
	}
	// What its body plays hurries to its end. It drops what it was doing at once: the reaction it follows first (its
	// record refreshed), then its body's action, ended there as a failure, its hands' animations on a thing playing on
	Hurry(creature);
	creature_reactions::StopReacting(creature);
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
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
	// commanded, as the original's take from the hand: its mind chooses nothing for itself meanwhile, and the take
	// carried out counts for no desire
	mind->commanded = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	creature_learning::SuppressOpposed(*mind->desires, Desire::Curiosity, tables->dependencies, k_TurnsPerSecond);
	// Once it is free to choose again it decides what to do with what it holds then, in place of anything else it was
	// left to do next
	mind->useWhatItHolds = true;
	mind->afterFeedback.reset();
	mind->highPending = false;
	mind->leashCast.reset();
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
	// The current action stops as a failure, whether or not a new plan is found, its hands' animations on a thing
	// playing on. A catch is the body's own and goes on
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy &&
	    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
	{
		Locator::creatureObjectActionSystem::value().StopForNewAction(creature);
	}
	Abandon(*mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	creature_learning::MakeFullyDominant(*mind->desires, desire);
	// The thing is the only goal, of that desire alone, and it plans now, even when it has planned this turn already
	mind->leash.actOn = {entt::to_integral(thing)};
	mind->plannedTurn = mind->turn + 1;
	PlanCreature(creature, *mind, desire);
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
	// A one-shot miracle's globe given to a grown creature is cast, from the desire the miracle is cast from: a miracle
	// for creatures, of anger, at the nearest other creature it knows of (one of compassion at itself), as the game
	// casts it; seeing it cast is a lesson in the miracle
	// Any other miracle goes to the thing it knows of most useful to the desire, of those the desire and the action may
	// act on (itself when the action is done to nothing), what the thing belongs to the plan's activity
	const auto& lookup = std::as_const(registry);
	const auto* tables = GetTables();
	const auto oneShot = OneShotOf(lookup, object);
	const auto cast = oneShot.has_value() && tables != nullptr && oneShot->magicType < tables->miracles.size()
	                      ? creature_giving::CastGlobe(mind->developmentPhase, static_cast<MagicType>(oneShot->magicType),
	                                                   tables->miracles.at(oneShot->magicType).desire)
	                      : std::nullopt;
	if (cast.has_value())
	{
		const auto desire = static_cast<Desire>(cast->desire);
		const auto actionName = cast->action;
		std::optional<entt::entity> target;
		std::optional<entt::entity> activity;
		if (cast->target == creature_giving::GlobeTarget::MostUseful)
		{
			const auto action = creature_mind_tables::FindAction(*tables, actionName);
			const auto* info = action.has_value() ? &tables->actions.at(*action) : nullptr;
			if (info != nullptr && !info->needsObjectToActOn)
			{
				target = creature;
			}
			else if (info != nullptr)
			{
				const auto held = Locator::creatureObjectActionSystem::has_value()
				                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
				                      : std::nullopt;
				float best = 0.0f;
				mind->beliefs.ForEach([&](const creature_object_beliefs::Belief& belief) {
					const auto thing = belief.thing;
					if (thing == creature || !lookup.Valid(thing) || (info->excludesCarriedObject && held == thing) ||
					    !SuitableForDesire(creature, *mind, thing, desire))
					{
						return;
					}
					if (const float usefulness = ObjectToActOnUsefulness(creature, *mind, desire, thing); usefulness > best)
					{
						best = usefulness;
						target = thing;
					}
				});
			}
			activity = target.has_value() ? BelongsTo(*target) : std::nullopt;
		}
		else if (cast->target == creature_giving::GlobeTarget::Itself)
		{
			target = creature;
			activity = target;
		}
		else
		{
			// the nearest creature it knows of, by the distance from where it stands, the first of equals in the order of
			// its beliefs
			const auto from = ecs::object::MapCoordsOf(creature);
			float nearest = std::numeric_limits<float>::max();
			mind->beliefs.ForEach([&](const creature_object_beliefs::Belief& belief) {
				if (belief.thing == creature || !lookup.Valid(belief.thing) || !lookup.AllOf<Creature>(belief.thing))
				{
					return;
				}
				const float distance = game_math::GetDistanceInMetres(from, ecs::object::MapCoordsOf(belief.thing));
				if (distance < nearest)
				{
					nearest = distance;
					target = belief.thing;
				}
			});
			activity = target;
		}
		// Only a cast that is a plan it can carry out is cast, and only then does it count as a lesson
		const auto row = static_cast<size_t>(cast->desire);
		const bool looksForGoal = row < creature_desires::k_DesireCount && tables->rules.at(row).searchesForGoal;
		if (creature_giving::GlobeCastIsPlan(*cast, looksForGoal, target.has_value(), activity.has_value()))
		{
			WatchMiracle(creature, oneShot->magicType);
			const bool planned = ForceActivity(
			    creature, {.desire = desire, .activityObject = activity, .action = actionName, .actionObject = *target});
			// what it does with a gift is commanded, as the original's waiting action is once taken up
			mind->commanded = mind->commanded || planned;
			// A readout at run time (the game channel at debug): the gift's globe, the plan it is given and its target
			if (const auto logger = spdlog::get("game"))
			{
				logger->debug("Creature {} given a one-shot globe {} of miracle {}: {} at {}{}", entt::to_integral(creature),
				              entt::to_integral(object), oneShot->magicType, actionName, entt::to_integral(*target),
				              planned ? "" : ", not planned");
			}
			return;
		}
	}
	// The plan it carried out before it took the thing (or before the reaction it followed then), when its action is done
	// to a thing: the gift becomes what it is done to when the action takes it, else what it is done with when the action
	// uses a thing and may use this one, and that plan is forced again
	if (const auto& before = mind->previousPlan; before.has_value() && tables != nullptr &&
	                                             before->action < tables->actions.size() &&
	                                             tables->actions.at(before->action).needsObjectToActOn)
	{
		if (const auto* executor = creature_plan_actions::For(tables->actions.at(before->action).name); executor != nullptr)
		{
			const bool actsOn = !executor->notWhatItHolds && (executor->target == Target::None ||
			                                                  mind_detail::Accepts(lookup, executor->target, object, creature));
			const bool uses = !actsOn && executor->use != Target::None && SuitableToUse(lookup, *mind, creature, object) &&
			                  mind_detail::Accepts(lookup, executor->use, object, creature);
			if (actsOn || uses)
			{
				const auto thingOf = [](std::optional<uint32_t> id) {
					return id.has_value() ? std::optional(static_cast<entt::entity>(*id)) : std::nullopt;
				};
				std::ignore =
				    ForceActivity(creature, {.desire = before->desire,
				                             .activityObject = thingOf(before->activityObject),
				                             .action = tables->actions.at(before->action).name,
				                             .actionObject = actsOn ? object : thingOf(before->object).value_or(entt::null),
				                             .instrument = uses ? std::optional(object) : thingOf(before->instrument)});
				return;
			}
		}
	}
	const auto& desires = *mind->desires;
	const auto* at = registry.TryGet<const Transform>(creature);
	const glm::vec2 position = at != nullptr ? glm::vec2(at->position.x, at->position.z) : glm::vec2(0.0f);
	const auto keptWithin = KeptWithin(registry, creature);
	// Each desire weighs what it was given as a thing to act on, the thing it holds
	const auto weigh = [&](Desire desire) {
		auto thing = ActOnCandidate(registry, creature, position, *mind->learnt, desire, object, keptWithin);
		thing.held = true;
		// Held, the distance weight does not count
		const auto weight = tables != nullptr ? tables->rules.at(static_cast<size_t>(desire)).distanceWeight : 0.0f;
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = creature_planner::ActOnUsefulness(thing, weight)};
	};
	// What its learning counts of the thing's object type (from its info row): hunger does not count for a type it found
	// was not food, and a type it was given or played with held up before is not looked over again
	creature_mind_model::ObjectTypeCounts* counts = nullptr;
	if (const auto objectType = static_cast<int>(ecs::map_cells::TypeOf(object));
	    mind->learnt.has_value() && objectType >= 0 && static_cast<size_t>(objectType) < mind->learnt->objectTypes.size())
	{
		counts = &mind->learnt->objectTypes.at(static_cast<size_t>(objectType));
	}
	std::optional<creature_giving::KindCounts> typeCounts;
	if (counts != nullptr)
	{
		typeCounts = creature_giving::KindCounts {.notFood = counts->notFood, .heldPlays = counts->heldPlays};
	}
	const auto choice = creature_giving::ChooseUse({
	    .curiosity = weigh(Desire::Curiosity),
	    .hunger = weigh(Desire::Hunger),
	    .playWithPlayer = weigh(Desire::PlayWithPlayer),
	    .cantEatKind = creature_giving::CantEatKind(typeCounts),
	    .givenKindBefore = creature_giving::GivenKindBefore(typeCounts),
	});
	// The first of its type looked over counts as one held up to play with, as the two share the count
	if (choice.remembersKind && counts != nullptr)
	{
		counts->heldPlays = creature_giving::k_HeldPlaysOnceLookedOver;
	}
	const auto number = entt::to_integral(object);
	// What it does with a gift is commanded, as the original's waiting action is once taken up
	const auto command = [&mind](bool planned) { mind->commanded = mind->commanded || planned; };
	switch (choice.use)
	{
	case creature_giving::Use::Examine:
		command(ForceActivity(creature, {.desire = Desire::Curiosity,
		                                 .action = k_ExamineGivenAction,
		                                 .actionObject = object,
		                                 .agenda = creature_mind::ExamineGiven(number)}));
		break;
	case creature_giving::Use::Eat:
		command(ForceActivity(creature, {.desire = Desire::Hunger,
		                                 .action = k_EatGivenAction,
		                                 .actionObject = object,
		                                 .agenda = creature_mind::EatGiven()}));
		break;
	case creature_giving::Use::Throw:
	{
		const auto camera = ecs::god_camera::Of(creature);
		const auto eye = camera.has_value() ? camera->origin : glm::vec3(0.0f);
		command(ForceActivity(creature, {.desire = Desire::PlayWithPlayer,
		                                 .action = k_ThrowGivenAction,
		                                 .actionObject = object,
		                                 .agenda = creature_mind::ThrowGiven(eye)}));
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
	return mind.beliefs.Find(object) != nullptr ? ActOnUsefulnessOnceBelieved(creature, mind, desire, object) : 0.0f;
}

float CreatureMindSystem::ActOnUsefulnessOnceBelieved(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                                      entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* thing = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (!mind.learnt.has_value() || tables == nullptr || thing == nullptr || !registry.AllOf<Transform>(creature))
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
		if (creature_planner::BeyondKeptArea(desire, keptWithin, game_math::GetDistanceInMetres(at, thing->position),
		                                     IsTemplePart(registry, object)))
		{
			return 0.0f;
		}
	}
	// The distance counts from where its player's leash is tied, when it is, else from the creature
	const auto distance =
	    DistancePriorityOf(registry, creature, object, tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	// In the game's order, each step rounded to a float: the tree's usefulness for the distance, then the desire's
	// factor for what the thing is
	const auto belief = BeliefOf(registry, object, creature);
	const float forDistance = Usefulness(*mind.learnt, TreeKind::ActOn, desire, belief) * distance;
	return forDistance * creature_planner::ActOnFactor(desire, ActOnFactsOf(registry, object, *mind.learnt, belief));
}

float CreatureMindSystem::ActivityUsefulness(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                             entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !registry.Valid(object) || mind.beliefs.Find(object) == nullptr ||
	    !registry.AllOf<Transform>(creature))
	{
		return 0.0f;
	}
	return GoalUsefulnessOf(registry, creature, mind, *tables, desire, object);
}

void CreatureMindSystem::PlanCreature(entt::entity creature, CreatureMindState& mind, std::optional<Desire> forcedDesire)
{
	const bool forcedOnThing = forcedDesire.has_value();
	// Only asked what its desires would do with one thing (DesireFoundOn): nothing is chosen or changed, and nothing
	// stops the search
	const auto query = std::exchange(_desireQuery, std::nullopt);
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	// What it was ordered or forced to do is over once its agenda has no step left, and so is what a fight's knock-out
	// made it do
	if (!query.has_value() && mind.idle.step >= mind.idle.agenda.size())
	{
		mind.commanded = false;
		mind.fightEnded = false;
	}
	// Under a script's control it plans nothing of its own accord, nor does a partner of a joint action whose mind
	// stands still, nor one out cold, nor one carrying out its player's order. A desire forced on a thing is planned all
	// the same, and so is a thing its player sent it to act on with the leash: the game's forced planning does not ask
	// whether a script holds the creature. A creature asked to fight chooses nothing from the turn its mind is told to
	// stand still
	const bool toldToAct = forcedOnThing || mind.leash.ordered.has_value();
	const auto fight = mind_detail::FightOf(std::as_const(registry), creature);
	if (tables == nullptr || transform == nullptr || !mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	if (!query.has_value() &&
	    (mind.paused || mind.leash.obeying || mind.commanded || creature_mind::IsUnconscious(mind.idle) || mind.idle.outCold ||
	     (!toldToAct && ecs::script_held::IsControlledByScript(creature)) || registry.AllOf<CreatureDemobilised>(creature) ||
	     (fight.has_value() && creature_asked_to_fight::MindStandsStill(*fight))))
	{
		return;
	}
	if (!query.has_value() && mind.plannedTurn == mind.turn)
	{
		return;
	}
	if (!query.has_value())
	{
		mind.plannedTurn = mind.turn;
	}
	// Having eaten a magic mushroom, once it is done with what it was doing it becomes high before it chooses anything
	// else
	if (!query.has_value() && mind.highPending && mind.idle.step >= mind.idle.agenda.size())
	{
		mind.highPending = false;
		static_cast<void>(BecomeHigh(creature));
		// it gets high as an order is carried out, its own choices waiting
		mind.commanded = true;
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
	for (size_t d = 0; d < creature_desires::k_DesireCount && !query.has_value(); ++d)
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
		const auto strength = desires.IsActivated(d) && state.suppressedTurns == 0 ? state.value : 0.0f;
		strengths.at(d) = strength;
		// Compassion takes its turn whatever its own list holds: what it does comes from the town it helps
		eligible.at(d) = strength > 0.0f && (d == static_cast<size_t>(Desire::Compassion) ||
		                                     std::ranges::any_of(tables->desireActions.at(d), [tables](uint32_t action) {
			                                     return creature_plan_actions::For(tables->actions.at(action).name) != nullptr;
		                                     }));
	}

	// Something shown on the leash is the only goal there is this turn, for every desire
	std::optional<entt::entity> leashTarget = query;
	if (!query.has_value() && !mind.leash.actOn.empty())
	{
		leashTarget = static_cast<entt::entity>(mind.leash.actOn.back());
		mind.leash.actOn.clear();
	}
	// Whether that is the thing its player sent it to act on with the leash
	const auto ordered = query.has_value() ? std::nullopt : std::exchange(mind.leash.ordered, std::nullopt);
	const bool orderedByLeash = leashTarget.has_value() && ordered == entt::to_integral(*leashTarget);
	// Whether its player sent it to act on that thing (with the leash, or a gift used as led), rather than a desire being
	// forced on the thing
	const bool sentByPlayer = !query.has_value() && leashTarget.has_value() && !forcedOnThing;
	if (passAgain)
	{
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(desires));
	}
	// The desires are planned two a turn in passes, one a turn once there are three creatures or more in the world (as
	// counted when the creatures' turn started), and what to do is decided as a pass ends
	const bool inPasses = !leashTarget.has_value();
	auto goals = inPasses ? creature_planner::NextGoals(mind.planner, _goalsPerTurn) : std::vector<Desire> {};
	if (leashTarget.has_value())
	{
		goals = mind_detail::GoalsForThing(forcedDesire, eligible);
	}

	creature_planner::PlanFacts facts {.developmentPhase = mind.developmentPhase,
	                                   .attitudeToPlayer = mind.attitudeToPlayer,
	                                   .noticedDeed = mind.observedDeed.has_value()};
	facts.ignoredByPlayer = creature_attention::IgnoredByPlayer(mind.turnsNotLookedAt, game_clock::MsPerTurn());
	if (const auto* body = registry.TryGet<const CreatureNeeds>(creature))
	{
		facts.exhaustion = body->needs.exhaustion;
		facts.warmth = body->needs.warmth;
	}
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature); leash != nullptr && leash->home.has_value())
	{
		facts.metresFromHome = game_math::GetDistanceInMetres(transform->position, *leash->home);
	}
	// A thing it is sent to act on is searched afresh: the plans found for it are weighed on their own, and the plans its
	// desires keep from their passes are left as they were, neither weighed against them nor replaced
	std::array<std::optional<creature_planner::Plan>, creature_desires::k_DesireCount> onThing {};
	const auto planOf = [&](size_t d) -> std::optional<creature_planner::Plan>& {
		return leashTarget.has_value() ? onThing.at(d) : mind.planner.best.at(d);
	};
	// Sent by its player to act on another creature, with the aggression leash and close enough, it is made to fight it
	// for its anger and its desires look for nothing else; as with any forced action, every free creature agrees to joint
	// actions from then on
	if (sentByPlayer && *leashTarget != creature && registry.Valid(*leashTarget) &&
	    registry.AllOf<Creature, Transform>(*leashTarget))
	{
		const auto* leash = registry.TryGet<const CreatureLeash>(creature);
		const bool fightsOnLeash =
		    leash != nullptr && leash->worn.has_value() &&
		    creature_leash::LeashedFightsCreature(
		        leash->worn->type,
		        game_math::GetDistanceInMetres(transform->position, registry.Get<const Transform>(*leashTarget).position),
		        DrawnHeight(registry, creature));
		if (auto fight = mind_detail::LeashFightPlan(fightsOnLeash, creature_mind_tables::FindAction(*tables, "Fight"),
		                                             entt::to_integral(*leashTarget));
		    fight.has_value())
		{
			onThing.at(static_cast<size_t>(Desire::Anger)) = std::move(fight);
			goals.clear();
			registry.Context().partnersAlwaysAgree = true;
		}
	}
	for (const auto desire : goals)
	{
		const auto d = static_cast<size_t>(desire);
		// A desire that may not be planned now has its plan made worth nothing, the rest of it left as it was; a desire
		// forced on a thing is planned all the same
		if (!forcedOnThing && !creature_planner::MayPlan(desire, facts))
		{
			if (auto& old = planOf(d); old.has_value())
			{
				old->priority = 0.0f;
			}
			continue;
		}
		// Actions are weighed in groups by the kind of thing they are done to, and whether that may be what it holds, so
		// each has a goal it can be done to; a desire that looks for no goal weighs them all in the order of its list
		std::array<std::vector<creature_planner::ActionCandidate>, creature_plan_actions::k_TargetCount * 2> actionGroups {};
		std::vector<std::pair<size_t, creature_planner::ActionCandidate>> actionsInListOrder;
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
		// A desire that looks for a goal is about a whole thing it believes in, found beforehand; shown a thing on the
		// leash, only compassion still looks for one
		std::optional<entt::entity> whole;
		if (search.searches && (!leashTarget.has_value() || desire == Desire::Compassion))
		{
			const auto goal = GoalAmongBeliefs(registry, creature, position, mind, *tables, desire, situation.lookedAt);
			// Compassion for a creature has nothing to do
			if (goal.has_value() && (desire != Desire::Compassion || creature_activity::CompassionHasActions(goal->kind)))
			{
				whole = goal->whole;
				search.foundWorth = goal->worth;
			}
		}
		if (desire == Desire::Compassion)
		{
			town = whole;
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
			    .disabled =
			        !creature_plan_actions::Possible(*executor, situation) ||
			        !creature_plan_actions::MayTry(*executor, mayCast, onFire) ||
			        UnavailableWhileKept(registry, creature, mind, action) ||
			        (executor->notDoneWithinSeconds.has_value() &&
			         creature_plan_actions::DoneWithin(learnt.turnsSinceDone.at(action),
			                                           game_clock::MsPerTurn() != 0 ? 1000u / game_clock::MsPerTurn() : 0u,
			                                           *executor->notDoneWithinSeconds)) ||
			        (executor->build == creature_plan_actions::Build::DanceWithVillagers &&
			         !MayDanceAgain(creature, executor->danceType)) ||
			        // Nor one that needs a skill it does not know or a miracle it knows nothing of
			        !creature_mind_tables::KnowsWhatItNeeds(tables->actions.at(action), learnt.knowledge),
			};
			const auto group = static_cast<size_t>(executor->target) * 2 + (executor->notWhatItHolds ? 1 : 0);
			actionGroups.at(group).push_back(candidate);
			actionsInListOrder.push_back({group, candidate});
		}
		// What each group's actions could be done to, and the best plan they give: for a thing it is sent to act on,
		// done to that thing alone
		const auto planOn = [&](std::optional<entt::entity> sentTo, decltype(actionGroups) groups,
		                        decltype(actionsInListOrder) inListOrder) -> std::optional<creature_planner::Plan> {
			std::optional<creature_planner::Plan> best;
			// A desire that takes its whole thing's part before the action chooses that one part first, whatever the action:
			// of the parts that suit the desire, the first most useful to act on (a creature is its own); only that part is
			// then tried, and with none the desire has no plan
			const bool partFirst =
			    search.searches && !sentTo.has_value() && whole.has_value() && tables->rules.at(d).partBeforeAction;
			std::optional<entt::entity> partChosen;
			if (partFirst)
			{
				if (registry.AllOf<Creature>(*whole))
				{
					partChosen = *whole;
				}
				else
				{
					std::vector<creature_planner::PartCandidate> partCandidates;
					for (const auto* part : mind.beliefs.PartsOf(*whole))
					{
						const auto thing = part->thing;
						const bool suits =
						    registry.Valid(thing) && thing != creature && SuitableForDesire(creature, mind, thing, desire);
						partCandidates.push_back(
						    {.id = entt::to_integral(thing),
						     .suits = suits,
						     .usefulness = suits ? ObjectToActOnUsefulness(creature, mind, desire, thing) : 0.0f});
					}
					if (const auto chosen = creature_planner::PartBeforeAction(partCandidates))
					{
						partChosen = static_cast<entt::entity>(*chosen);
					}
				}
				if (!partChosen.has_value())
				{
					return std::nullopt;
				}
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
					if (sentTo.has_value())
					{
						if (Accepts(registry, target, *sentTo, creature) &&
						    (!town.has_value() || PartOfTown(registry, *sentTo, *town)))
						{
							objects.push_back(
							    ActOnCandidate(registry, creature, position, learnt, desire, *sentTo, confinedWithin));
						}
					}
					else if (search.searches)
					{
						// A plan for a whole thing is done to one of the things the creature took to belong to it (a creature
						// is its own), the most useful to act on for the distance, however far; with no whole thing, to
						// nothing
						std::vector<creature_planner::ObjectCandidate> parts;
						const auto consider = [&](entt::entity part) {
							if (!registry.Valid(part) || part == creature || !Accepts(registry, target, part, creature))
							{
								return;
							}
							const auto* held = registry.TryGet<const HeldByCreature>(part);
							if (notHeld && held != nullptr && held->creature == creature)
							{
								return;
							}
							// The whole thing is the goal, found beforehand: a part is judged only as what to act on
							auto candidate = ActOnCandidate(registry, creature, position, learnt, desire, part, confinedWithin);
							candidate.goalUsefulness = candidate.usefulness;
							parts.push_back(candidate);
						};
						if (partChosen.has_value())
						{
							consider(*partChosen);
						}
						else if (whole.has_value() && registry.AllOf<Creature>(*whole))
						{
							consider(*whole);
						}
						else if (whole.has_value())
						{
							for (const auto* part : mind.beliefs.PartsOf(*whole))
							{
								consider(part->thing);
							}
						}
						if (const auto chosen = creature_planner::BestActOn(parts, tables->rules.at(d).distanceWeight))
						{
							objects.push_back(parts.at(*chosen));
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
				// An action the game's own action table says uses a thing takes up the thing to use too; with nothing to
				// use, the next action is tried
				while (plan.has_value())
				{
					if (!tables->actions.at(plan->action).needsObjectToUse)
					{
						break;
					}
					plan->instrument = InstrumentFor(creature, mind, desire, plan->action, plan->object);
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
				return plan;
			}
			for (size_t g = 0; g < groups.size(); ++g)
			{
				if (groups.at(g).empty())
				{
					continue;
				}
				const auto distanceWeight = tables->rules.at(d).distanceWeight;
				// A plan about the thing found for the desire's goal takes up no thing to use, as the game's planner looks
				// for one only for an action it chose from the goal with nothing to be about (an action that needs one then
				// fails as its agenda is made)
				auto plan = creature_planner::PlanDesire(desire, strengths.at(d), objectsOf.at(g), distanceWeight, groups.at(g),
				                                         search);
				if (plan.has_value() && (!best.has_value() || creature_planner::Replaces(*plan, *best, actions)))
				{
					best = plan;
					// The plan is about the whole thing it found for its goal
					if (whole.has_value())
					{
						best->activityObject = entt::to_integral(*whole);
					}
				}
			}
			return best;
		};
		if (!sentByPlayer)
		{
			planOf(d) = planOn(leashTarget, actionGroups, actionsInListOrder);
			continue;
		}
		// Sent to act on a thing by its player (with the leash, or a gift used as led), a desire acts on the thing when
		// the thing suits the desire and is of some use to it. Else it acts on the things it took to belong to the same
		// whole thing, the newest belief first, each that suits and is of use, until one gives a plan; with none, the
		// desire has no plan
		const auto suits = [&](entt::entity thing) {
			return registry.Valid(thing) && SuitableForDesire(creature, mind, thing, desire) &&
			       ActOnUsefulnessOnceBelieved(creature, mind, desire, thing) > 0.0f;
		};
		std::vector<entt::entity> sentParts;
		if (const auto sentWhole = BelongsTo(*leashTarget); sentWhole.has_value())
		{
			for (const auto* part : mind.beliefs.PartsOf(*sentWhole))
			{
				sentParts.push_back(part->thing);
			}
		}
		std::optional<creature_planner::Plan> sentPlan;
		for (const auto thing : creature_activity::SentToActOn<entt::entity>(*leashTarget, sentParts, suits))
		{
			sentPlan = planOn(thing, actionGroups, actionsInListOrder);
			if (sentPlan.has_value())
			{
				break;
			}
		}
		planOf(d) = sentPlan;
	}

	// Doing something it chose for itself that isn't idling, it carries on
	if (!query.has_value() && !mind.planActive && !Interruptible(mind.idle.activity) &&
	    mind.idle.step < mind.idle.agenda.size())
	{
		return;
	}
	if (inPasses && !creature_planner::PassOver(mind.planner))
	{
		return;
	}
	// A plan it can't carry out is left as it is, as the game leaves a plan that is not valid. For a thing it is sent to
	// act on, only the plans found for the thing are chosen from
	creature_planner::PlannerState searched;
	if (leashTarget.has_value())
	{
		searched.best = onThing;
		searched.current = mind.planner.current;
	}
	if (query.has_value())
	{
		// the best plan found for the thing, whatever it is doing
		const auto best = creature_planner::Best(searched);
		_desireFound = best.has_value() ? std::optional(best->desire) : std::nullopt;
		return;
	}
	if (const auto choice = creature_planner::Choose(leashTarget.has_value() ? searched : mind.planner))
	{
		const bool adopted = Adopt(creature, mind, *choice, situation);
		// sent by the leash to act on a thing, a plan it takes marks the thing as its target, but a desire forced on the
		// thing marks nothing; when its player sent it, it carries the plan out as its player's order
		if (mind_detail::MarksThingPlannedFor(adopted, forcedOnThing) && leashTarget.has_value() &&
		    Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().MarkTargetAt(creature, *leashTarget);
		}
		if (adopted && orderedByLeash)
		{
			mind.commanded = true;
		}
		// its player's order taken with a plan, it tells what it wants now, about the thing its plan is done to
		if (orderedByLeash)
		{
			TellLeashOrder(registry, creature, adopted, choice->desire,
			               choice->object.has_value() ? static_cast<entt::entity>(*choice->object) : creature);
		}
	}
	else if (orderedByLeash && registry.Valid(*leashTarget))
	{
		// Sent by its player to act on a thing its desires find nothing to do with, it is made curious about the thing: it
		// holds it up when it can pick it up, else looks it over, and the thing is marked as its target. It carries that
		// out as its player's order, whether or not the plan could be made
		const bool canPickUp = Locator::creatureObjectActionSystem::has_value() &&
		                       Locator::creatureObjectActionSystem::value().CanPickUp(creature, *leashTarget);
		const bool forced = ForceActivity(creature, {.desire = Desire::Curiosity,
		                                             .action = creature_leash::OrderedThingAction(canPickUp),
		                                             .actionObject = *leashTarget});
		mind.commanded = true;
		if (forced && Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().MarkTargetAt(creature, *leashTarget);
		}
		TellLeashOrder(registry, creature, forced, Desire::Curiosity, *leashTarget);
	}
	if (inPasses)
	{
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(desires));
	}
}

std::optional<Desire> CreatureMindSystem::DesireFoundOn(entt::entity creature, CreatureMindState& mind, entt::entity thing)
{
	_desireQuery = thing;
	_desireFound.reset();
	PlanCreature(creature, mind);
	_desireQuery.reset();
	return std::exchange(_desireFound, std::nullopt);
}

creature_mind::ObjectOrder::Kind CreatureMindSystem::LetGoBeforeStep(entt::entity creature, CreatureMindState& mind,
                                                                     entt::entity held)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// a creature it holds is put down; anything else by what its desires would do with it
	const bool holdsCreature = registry.Valid(held) && registry.AllOf<Creature>(held);
	const auto desire = holdsCreature ? std::nullopt : DesireFoundOn(creature, mind, held);
	const bool putDown = creature_throw::LetGoBeforeFreeHands(holdsCreature, desire) == creature_throw::k_PutDown;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} lets go of {} before its step: {} (its plan for it: {})",
	                   entt::to_integral(creature), entt::to_integral(held), putDown ? "put down" : "tossed away",
	                   desire.has_value() ? static_cast<int>(*desire) : -1);
	return putDown ? creature_mind::ObjectOrder::Kind::PutDown : creature_mind::ObjectOrder::Kind::Discard;
}

void CreatureMindSystem::AteMagicMushroom(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Not while it has a gift to decide about or a cast on the leash waiting: they keep the place the high would take
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	    mind != nullptr && !mind->useWhatItHolds && !mind->leashCast.has_value())
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
	// Kept within an area and not under a script's control, it leaves out a walk to its seat that ends outside the area,
	// and sits where it stands: the rest of the steps go on
	if (const auto kept = KeptAreaOf(registry, creature); kept.has_value() && !ecs::script_held::IsControlledByScript(creature))
	{
		agenda = creature_agenda_area::WithoutWalksOut(std::move(agenda), *position, kept->centre, kept->radius);
	}
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
		PlanCreatureTurn(entity);
	}
}

void CreatureMindSystem::PlanCreatureTurn(entt::entity entity)
{
	// While the player's hand holds a creature, and until the hand's locked select on it ends, it decides nothing: it
	// keeps the plan that it had
	const auto held =
	    Locator::creatureHandSystem::has_value() ? Locator::creatureHandSystem::value().GetCreature() : std::nullopt;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || held == entity || ecs::object_flags::IsInLockedSelect(entity))
	{
		return;
	}
	if (auto* mind = registry.TryGet<CreatureMindState>(entity))
	{
		PlanCreature(entity, *mind);
	}
}

void CreatureMindSystem::KeepAfterFeedback(entt::entity creature, CreatureMindState& mind,
                                           CreatureMindState::AfterFeedback after)
{
	mind.afterFeedback = after;
	mind.useWhatItHolds = false;
	mind.leashCast.reset();
	if (auto* casting = Locator::entitiesRegistry::value().TryGet<CreatureCasting>(creature))
	{
		casting->fizzle.reset();
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
					const auto* at = registry.TryGet<const Transform>(*held);
					const auto coords = at != nullptr
					                        ? std::optional(map_coords::FromMetres(glm::vec2(at->position.x, at->position.z)))
					                        : std::nullopt;
					creature_mind_model::Learn(
					    learnt, TreeKind::ActOn, other,
					    {.belief = *belief,
					     .feedback = share,
					     .saved = creature_mind_model::SavedPlace(
					         coords.has_value() ? std::optional(std::array<int32_t, 2> {coords->x, coords->z}) : std::nullopt)},
					    tables->attributes.at(static_cast<size_t>(other)));
				}
			}
			creature_mind_model::Think(learnt, "I've learnt to eat what I'm given");
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} learns to eat what it holds", entt::to_integral(creature));
			// It is made to eat it alive as a plan forced on it, though not one its player forced
			ForceActivity(creature, {.desire = Desire::Hunger, .action = "EatAlive", .actionObject = *held, .byPlayer = false});
			// Once free to choose again, it does again what it learns most from then
			KeepAfterFeedback(creature, mind, CreatureMindState::AfterFeedback::RepeatLastAction);
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
				creature_learning::ForgiveRunningAway(source);
			}
		}
		Abandon(mind);
		return;
	}

	// Every slap stops its copying of the player and leaves it to show how the slap made it feel, even when it has
	// nothing to learn from it
	if (feedback < 0.0f)
	{
		learnt.mimicry.reset();
		KeepAfterFeedback(creature, mind, CreatureMindState::AfterFeedback::ShowSlapFeeling);
	}
	const auto strength = creature_learning::Strengthened(feedback);
	// With nothing it did worth crediting, the feedback does nothing more
	const auto path = creature_learning::PathOf(context != nullptr, info != nullptr, mind.developmentPhase, k_MinLessonPhase);
	if (path == creature_learning::FeedbackPath::Nothing)
	{
		if (const auto logger = spdlog::get("game"))
		{
			logger->info("Creature {} learns nothing from it: nothing it did to credit", entt::to_integral(creature));
		}
		return;
	}
	if (feedback > 0.0f)
	{
		if (learnt.mimicry.has_value() && tables != nullptr)
		{
			// the stage's steps and a draw of GameRand(1), on the game's stream, as the game draws one
			creature_watching::StrokedWhileMimicking(learnt.mimicry, tables->mimics, game_random::GameRand(1));
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
			KeepAfterFeedback(creature, mind,
			                  feedback < 0.0f ? CreatureMindState::AfterFeedback::Frightened
			                                  : CreatureMindState::AfterFeedback::Happy);
		}
		TellHelp(creature, creature_help::TooYoungToLearn(feedback));
		if (const auto logger = spdlog::get("game"))
		{
			logger->info("Creature {} is too young to learn from it, at stage {}", entt::to_integral(creature),
			             mind.developmentPhase);
		}
		giveUp();
		// Stroked, it still does again what it did, as one old enough to learn does
		if (feedback > 0.0f)
		{
			RepeatLastAction(creature, mind, *context);
		}
		return;
	}

	const auto desire = context->desire;
	const auto d = static_cast<size_t>(desire);
	const auto& texts = tables->texts.at(d);
	context->credited = strength;
	// A lesson from its player is something to do with the player: its turns without are counted from none, whether or
	// not the desire can be taught
	mind.turnsSinceMadeToAct = 0;
	if (const auto logger = spdlog::get("game"))
	{
		logger->info("Creature {} learns from it about desire {} and action {} ({}) by {:.2f}{}", entt::to_integral(creature),
		             d, context->action, info->name, strength, tables->rules.at(d).learnable ? "" : ": a desire not taught");
	}
	if (tables->rules.at(d).learnable)
	{
		creature_learning::LearnDesireLesson(desires, desire, creature_learning::DominantSource(desires[desire]), strength,
		                                     tables->rules, tables->dependencies);
		creature_mind_model::Think(learnt, creature_learning::DesireLessonText(strength, texts.desire));
		// Then about the thing the plan was for, in the goal trees, and the thing it was done to, in the act-on trees, each
		// spread to the desires that depend on its desire
		const auto taught = creature_learning::ThingLessons(*context, entt::to_integral(creature), strength);
		context->taught = taught;
		const auto teach = [&](TreeKind tree, const std::optional<creature_tree::Belief>& belief, float feedback,
		                       const std::optional<std::array<int32_t, 2>>& place) {
			if (!belief.has_value())
			{
				return;
			}
			for (const auto& [other, share] : creature_learning::Spread(desire, feedback, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, tree, other,
				                           {.belief = *belief,
				                            .feedback = std::clamp(share, -1.0f, 1.0f),
				                            .saved = creature_mind_model::SavedPlace(place)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
			creature_mind_model::Think(learnt, creature_learning::ObjectLessonText(strength, ObjectName(belief), texts.trying));
		};
		if (taught.goal.has_value())
		{
			teach(TreeKind::Goal, context->aboutBelief, *taught.goal, context->aboutPlace);
		}
		// Where a thing it was taught about now sits in its desire's tree, once the lesson has grown it
		const auto leafOf = [&learnt, desire](TreeKind kind, bool taughtIt,
		                                      const std::optional<creature_tree::Belief>& belief) {
			if (!taughtIt || !belief.has_value())
			{
				return std::optional<creature_tree::Leaf> {};
			}
			return creature_tree::LeafOf(learnt.trees.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire)), *belief);
		};
		const auto goalLeaf = leafOf(TreeKind::Goal, taught.goal.has_value(), context->aboutBelief);
		if (taught.actOn.has_value())
		{
			teach(TreeKind::ActOn, context->belief, *taught.actOn, context->place);
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
	RepeatLastAction(creature, mind, *context);
}

std::optional<uint32_t> CreatureMindSystem::InstrumentFor(entt::entity creature, CreatureMindState& mind, Desire desire,
                                                          size_t action, std::optional<uint32_t> goal)
{
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size())
	{
		return std::nullopt;
	}
	// over the things it knows of, as the game's planner looks for one (the executor's thing to use is the row's own
	// test of what may be used, BestObjectToUse)
	const auto best =
	    BestObjectToUse(creature, mind, desire, action, goal.has_value() ? static_cast<entt::entity>(*goal) : entt::null);
	return best.has_value() ? std::optional(entt::to_integral(*best)) : std::nullopt;
}

std::optional<entt::entity> CreatureMindSystem::BestObjectToUse(entt::entity creature, CreatureMindState& mind, Desire desire,
                                                                size_t action, entt::entity actOn)
{
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size())
	{
		return std::nullopt;
	}
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	const auto* executor = creature_plan_actions::For(tables->actions[action].name);
	const uint32_t msPerTurn = game_clock::MsPerTurn();
	const float distanceWeight = tables->rules.at(static_cast<size_t>(desire)).distanceWeight;
	std::vector<creature_planner::UseCandidate> candidates;
	// Over the things it knows of, but the one it does the action to: one its way to was not blocked a while ago, inside
	// any area it is kept to, that lets a creature act on it, and that the action may use (the action's own test, else
	// whether the creature could pick it up). Each is worth the usefulness the game gives a thing used, by how near it is,
	// up to 1; the first of the best
	std::vector<entt::entity> known;
	mind.beliefs.ForEach([&known](const creature_object_beliefs::Belief& belief) { known.push_back(belief.thing); });
	for (const auto thing : known)
	{
		auto* belief = mind.beliefs.Find(thing);
		if (belief == nullptr || thing == actOn || !lookup.Valid(thing) ||
		    (msPerTurn != 0 && creature_object_beliefs::BlockedRecently(*belief, game_clock::Turn(), 1000 / msPerTurn,
		                                                                ecs::map_cells::IsMultiCellStaticClass(thing))) ||
		    OutsideConfinement(lookup, creature, thing) || !mind_detail::LetsCreatureAct(lookup, thing))
		{
			continue;
		}
		const bool usable = executor != nullptr && executor->use != Target::None
		                        ? mind_detail::Accepts(lookup, executor->use, thing, creature)
		                        : Locator::creatureObjectActionSystem::has_value() &&
		                              Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing);
		candidates.push_back({.id = entt::to_integral(thing),
		                      .passes = usable,
		                      .distancePriority = usable ? DistancePriorityOf(lookup, creature, thing, distanceWeight) : 0.0f});
	}
	const auto best = creature_planner::BestToUse(candidates, creature_mind_tables::k_UsingUsefulness);
	return best.has_value() ? std::optional(static_cast<entt::entity>(*best)) : std::nullopt;
}

void CreatureMindSystem::RepeatLastAction(entt::entity creature, CreatureMindState& mind,
                                          const creature_learning::Context& lesson)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value())
	{
		return;
	}
	const auto& learnt = *mind.learnt;
	const auto* context = &lesson;
	const auto desire = context->desire;
	// Stroked for something it did to a thing other than itself, and not while it copies the player, it does it again,
	// forced on it at a plan's plain worth: to itself, or to the thing of that kind most useful to act on (the same one,
	// it may be)
	// The thing it did it to must still be there, though another of its kind may be the one it does it to
	if (creature_learning::OnItself(*context, entt::to_integral(creature)) || !context->belief.has_value() ||
	    !registry.Valid(static_cast<entt::entity>(*context->object)) || learnt.mimicry.has_value())
	{
		return;
	}
	// The game's row 276 is carried out as row 11
	constexpr uint32_t k_CarriedOutOtherwise = 276;
	constexpr uint32_t k_CarriedOutAs = 11;
	const auto action = context->action == k_CarriedOutOtherwise ? k_CarriedOutAs : context->action;
	// Any action of the table may be done again, the ones only ever forced on it too
	if (action >= tables->actions.size())
	{
		return;
	}
	const auto& info = tables->actions[action];
	// An action its row does not do to a thing is done to itself, and is never refused for want of one
	entt::entity again = creature;
	if (info.needsObjectToActOn)
	{
		// Else the thing of that kind it knows of most useful to act on for the desire, the first of equals, worth more
		// than nothing: one the desire may act on, the action may be done to, and not what it holds when its row says so
		const auto& lookup = std::as_const(registry);
		const auto* executor = creature_plan_actions::ForcedFor(info.name);
		const auto held = Locator::creatureObjectActionSystem::has_value()
		                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
		                      : std::nullopt;
		std::optional<entt::entity> best;
		float bestUsefulness = 0.0f;
		mind.beliefs.ForEach([&](const creature_object_beliefs::Belief& known) {
			const auto thing = known.thing;
			if (thing == creature || !lookup.Valid(thing) || !SuitableForDesire(creature, mind, thing, desire) ||
			    (info.excludesCarriedObject && held == thing) ||
			    (executor != nullptr && executor->target != Target::None &&
			     !mind_detail::Accepts(lookup, executor->target, thing, creature)))
			{
				return;
			}
			if (const auto belief = BeliefOf(lookup, thing, creature);
			    !belief.has_value() || belief->type != context->belief->type)
			{
				return;
			}
			if (const float usefulness = ObjectToActOnUsefulness(creature, mind, desire, thing); usefulness > bestUsefulness)
			{
				best = thing;
				bestUsefulness = usefulness;
			}
		});
		if (!best.has_value())
		{
			return;
		}
		again = *best;
	}
	// An action that needs a skill it does not know or a miracle it knows nothing of is not done again, as the planner
	// does not choose it
	if (!creature_mind_tables::KnowsWhatItNeeds(tables->actions[action], learnt.knowledge))
	{
		return;
	}
	// What it does it about is what the thing belongs to: a villager's, a building's, a field's or a totem statue's
	// town, a temple part's temple, a tree's forest, an animal's flock, a creature itself. Belonging to nothing, a plan
	// of a desire whose row says so is about the town nearest the creature, however far, which it then knows of
	auto activity = BelongsTo(again);
	if (!activity.has_value() && tables->rules.at(static_cast<size_t>(desire)).social)
	{
		const auto town = ecs::map_cells::GetNearestTown(ecs::object::MapCoordsOf(creature), std::numeric_limits<float>::max());
		if (town != entt::null)
		{
			ComeAcross(creature, mind, town);
			activity = town;
		}
	}
	// An action that uses a thing takes the thing it knows of best to use; knowing of none, it is not done again
	std::optional<entt::entity> instrument;
	if (tables->actions[action].needsObjectToUse)
	{
		instrument = BestObjectToUse(creature, mind, desire, action, again);
		if (!instrument.has_value())
		{
			return;
		}
	}
	// The original asks the action's own test here, as its planner does. Done again, it is carried out as an order is
	if (ForceActivity(creature, {.desire = desire,
	                             .activityObject = activity,
	                             .action = tables->actions[action].name,
	                             .actionObject = again,
	                             .instrument = instrument,
	                             .score = 1.0f,
	                             .checkedAsChosen = true}))
	{
		mind.commanded = true;
	}
}

namespace
{
/// A lesson about what a plan is for, as the game teaches one: the desire's goal tree takes the feedback, and the goal
/// tree of every other desire that depends on it a share of it (creature_learning::Spread), each in the desires' order
/// Where a thing stands now in map coordinates (x and z), as a belief of it brought up to date now keeps it
std::optional<std::array<int32_t, 2>> MapPlaceOf(const ecs::Registry& registry, entt::entity thing)
{
	const auto* at = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (at == nullptr)
	{
		return std::nullopt;
	}
	const auto coords = map_coords::FromMetres(glm::vec2(at->position.x, at->position.z));
	return std::array<int32_t, 2> {coords.x, coords.z};
}

void LearnAboutGoal(creature_mind_model::Learnt& learnt, const creature_mind_tables::Tables& tables, Desire desire,
                    const creature_tree::Belief& belief, float feedback, const std::optional<std::array<int32_t, 2>>& place)
{
	for (const auto& [other, share] : creature_learning::Spread(desire, feedback, tables.dependencies))
	{
		creature_mind_model::Learn(learnt, TreeKind::Goal, other,
		                           {.belief = belief, .feedback = share, .saved = creature_mind_model::SavedPlace(place)},
		                           tables.attributes.at(static_cast<size_t>(other)));
	}
}
} // namespace

void CreatureMindSystem::ChangeHowNice(entt::entity creature, entt::entity other, float change)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (tables == nullptr || mind == nullptr || !mind->learnt.has_value())
	{
		return;
	}
	auto& learnt = *mind->learnt;
	auto& attitude = creature_learning::AttitudeTo(learnt.creatures, entt::to_integral(other));
	const auto lessons = creature_learning::ChangeHowNice(attitude, change);
	if (const auto belief = BeliefOf(registry, other, creature))
	{
		for (const auto& [desire, amount] : lessons)
		{
			LearnAboutGoal(learnt, *tables, desire, *belief, amount, MapPlaceOf(registry, other));
		}
	}
}

void CreatureMindSystem::LearnTurn()
{
	if (GetTables() == nullptr)
	{
		return;
	}
	std::vector<entt::entity> creatures;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const Creature, const CreatureMindState>(
	        [&creatures](entt::entity entity, const Creature&, const CreatureMindState&) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		LearnCreatureTurn(creature);
	}
}

void CreatureMindSystem::LearnCreatureTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !registry.Valid(creature))
	{
		return;
	}
	{
		auto* mind = registry.TryGet<CreatureMindState>(creature);
		if (mind == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
		{
			return;
		}
		auto& learnt = *mind->learnt;
		// What the player showed it on a leash: anger or compassion towards such things, as what a plan is for. The lesson
		// is about the whole thing the shown thing belongs to (a villager's town, a creature itself); a thing that belongs
		// to nothing teaches nothing
		for (const auto& shown : mind->leash.shown)
		{
			const auto thing = static_cast<entt::entity>(shown.object);
			const auto whole = registry.Valid(thing) ? BelongsTo(thing) : std::nullopt;
			const auto belief = whole.has_value() ? BeliefOf(registry, *whole, creature) : std::nullopt;
			if (!belief.has_value())
			{
				continue;
			}
			for (const auto& lesson : shown.lessons)
			{
				LearnAboutGoal(learnt, *tables, lesson.desire, *belief, lesson.change, MapPlaceOf(registry, *whole));
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
			ChangeHowNice(creature, static_cast<entt::entity>(change.creature), change.change);
		}
		mind->leash.attitudes.clear();
	}
}

void CreatureMindSystem::LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind)
{
	if (auto* state = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		// the new file replaces any stage that waited for an earlier one
		state->scriptStage.reset();
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
	std::ignore = TakeUpScriptKnows(mind->scriptKnows, _tables.has_value() ? &*_tables : nullptr,
	                                creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)), learnt);
	auto file = creature_mind_model::ToFile(*mind->desires, learnt, mind->developmentPhase, mind->attitudeToPlayer,
	                                        static_cast<uint32_t>(creature::InfoRow(body->species)));
	file.turnsLookedAt = mind->turnsLookedAt;
	// whether its anger spares its own player's things, as it is now: 0 once it copied its player's anger at one of them
	file.unknown1 = mind->angerSparesOwnPlayer ? 1U : 0U;
	// A body that no file made keeps the speeds that its species gave it; one that loaded a file keeps the file's
	if (learnt.file == nullptr && Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		const auto row = static_cast<size_t>(creature::InfoRow(body->species));
		if (row < creatures.size())
		{
			const auto& info = creatures.at(row);
			file = creature_mind_body::WithBornSpeeds(std::move(file),
			                                          {.slow = info.slowSpeed, .walk = info.walkSpeed, .run = info.runSpeed});
		}
	}
	// with what it has counted of its life
	creature_mind_model::PutCounts(file, CountsOf(registry, creature));
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
	mind->desires = creature_desires::Create(
	    SetupFor(body->species), [](float low, float high) { return game_random::GameFloatRange(low, high); },
	    influence::IsInfluenceEverywhere());
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
	if (GetTables() == nullptr)
	{
		return;
	}
	for (const auto entity : MindsOf(registry))
	{
		const auto& at = std::as_const(registry).Get<Transform>(entity);
		if (glm::distance(at.position, point) <= k_ViewDistance)
		{
			WatchSkill(entity, skill);
		}
	}
}

namespace
{
/// A light bulb over the creature's head at how far it has got, at most one in a hundred turns. Whether one was made
bool MakeLightbulb(ecs::Registry& registry, entt::entity creature, float progress)
{
	const auto head = mind_detail::HeadPosition(registry, creature);
	if (!head.has_value())
	{
		return false;
	}
	auto& bulbs = registry.AllOf<CreatureLightbulbs>(creature) ? registry.Get<CreatureLightbulbs>(creature)
	                                                           : registry.AssignState<CreatureLightbulbs>(creature);
	const auto turn = game_clock::Turn();
	if (!creature_lightbulb::MayMake(bulbs.lastTurn, turn))
	{
		return false;
	}
	bulbs.lastTurn = turn;
	bulbs.bulbs.push_back(creature_lightbulb::Make(*head, ecs::object::GetHeight(creature), progress));
	// a trace at run time (the game channel at trace level), not compiled out of release builds
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Creature {} shows a light bulb at {:.0f}% at turn {}", entt::to_integral(creature), progress * 100.0f,
		              turn);
	}
	return true;
}
} // namespace

bool CreatureMindSystem::ShowLightbulb(entt::entity creature, float progress)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && MakeLightbulb(registry, creature, progress);
}

void CreatureMindSystem::WatchSkill(entt::entity creature, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || mind == nullptr || !mind->learnt.has_value() || mind->paused)
	{
		return;
	}
	const auto& known = mind->learnt->knowledge.skillsKnown;
	const bool knownBefore = skill < known.size() && known[skill];
	const auto progress = creature_watching::SeeSkill(mind->learnt->knowledge, skill, tables->skills, mind->developmentPhase,
	                                                  mind->turn, k_TurnsPerSecond);
	// A readout for the log (the game channel at debug): the skill seen, the creature's stage against the skill's, how
	// often and how long it has watched it, and what came of the sighting
	if (const auto logger = spdlog::get("game"); logger != nullptr && skill < tables->skills.size())
	{
		const auto& rule = tables->skills.at(skill);
		const auto& seen = mind->learnt->knowledge.skillsSeen;
		const auto* sighting = skill < seen.size() ? &seen.at(skill) : nullptr;
		const auto watched = sighting != nullptr && sighting->turn.has_value()
		                         ? static_cast<float>(mind->turn - *sighting->turn) / k_TurnsPerSecond
		                         : 0.0f;
		const char* outcome = progress.ignored  ? (progress.learnt ? "known already" : "too young to learn it")
		                      : progress.learnt ? "learnt it"
		                                        : "still learning it";
		logger->debug("Creature {} sees skill {} ({}) at stage {} (it needs {}): seen {} times over {:.1f} s of {:.1f}: {}",
		              entt::to_integral(creature), skill, rule.name, mind->developmentPhase, rule.minPhase,
		              sighting != nullptr ? sighting->count : 0u, watched, rule.watchSeconds, outcome);
	}
	// It shows a light bulb: a full one as it learns the action, half of one while it is still learning it
	if (const auto bulb = creature_lightbulb::ForAction(progress.learnt, knownBefore, progress.ignored && !knownBefore))
	{
		MakeLightbulb(registry, creature, *bulb);
	}
	if (progress.learnt && !progress.ignored)
	{
		creature_mind_model::Think(*mind->learnt, fmt::format("I've learnt to {}", tables->skills.at(skill).name));
	}
	// The advisor tells of it: too young to learn it (a skill it knows too), learnt it now, or still learning it (a
	// skill it knows already says nothing once it is old enough)
	using creature_help::Kind;
	const auto parameter = static_cast<uint32_t>(skill);
	if (progress.ignored && !progress.learnt)
	{
		TellHelp(creature, {.kind = Kind::CantLearnNormalActionYet, .parameter = parameter});
	}
	else if (!progress.ignored)
	{
		TellHelp(creature,
		         {.kind = progress.learnt ? Kind::LessonNormalAction : Kind::NearlyLearntNormalAction, .parameter = parameter});
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
	// The sightings are timed by the game's turn
	const auto progress = creature_watching::SeeMiracle(knowledge, miracle, tables->miracles, mind->developmentPhase,
	                                                    game_clock::Turn(), mind->leash.miracleSightingWeight, multiplier);
	// It shows a light bulb, old enough and knowing what the miracle needs: a full one as it learns it, and for the next
	// sightings, its share of the sightings while it is still learning it
	if (!progress.ignored && (!progress.event.has_value() || *progress.event != creature_watching::LearningEvent::TooYoung))
	{
		const auto seen =
		    miracle < knowledge.miraclesSeen.size() ? static_cast<float>(knowledge.miraclesSeen[miracle].count) : 0.0f;
		const auto needed = creature_watching::TimesNeeded(tables->miracles.at(miracle).timesToSee, multiplier);
		if (const auto bulb = creature_lightbulb::ForMiracle(progress.learnt, seen, needed))
		{
			MakeLightbulb(registry, creature, *bulb);
		}
	}
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
	auto* mind = MindSeeing(registry, player, point);
	if (mind != nullptr)
	{
		creature_perceived_desires::Increase(mind->perceivedDesires, static_cast<int32_t>(desire), weight);
	}
	// A readout for the log (the game channel at debug): whose creature was shown what, and whether one saw it
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->debug("Empathy: player {} shows desire {} by {:.2f} at ({:.1f}, {:.1f}), {}", static_cast<int>(player),
		              static_cast<int>(desire), weight, point.x, point.z,
		              mind != nullptr ? "its creature sees it" : "no creature of its sees it");
	}
}

void CreatureMindSystem::EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight,
                                                 const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = MindSeeing(registry, player, point);
	if (mind != nullptr)
	{
		creature_perceived_desires::IncreaseTown(mind->perceivedDesires, static_cast<int32_t>(desire), weight);
	}
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->debug("Empathy: player {} shows town desire {} by {:.2f} at ({:.1f}, {:.1f}), {}", static_cast<int>(player),
		              static_cast<int>(desire), weight, point.x, point.z,
		              mind != nullptr ? "its creature sees it" : "no creature of its sees it");
	}
}

bool CreatureMindSystem::SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows)
{
	// What it comes to know by it, the advisor tells the player of, as a lesson learnt
	return ChangeKnows(creature, {.kind = kind, .action = action, .knows = knows, .tell = true});
}

bool CreatureMindSystem::LearnEverything(entt::entity creature)
{
	// the six ordinary skills, then every miracle row, each only known about: no miracle's sightings change, and the
	// player is told of none of it
	std::vector<CreatureMindState::ScriptKnows> edits;
	for (uint32_t skill = 0; skill < k_LearnEverythingSkills; ++skill)
	{
		edits.push_back({.kind = static_cast<uint32_t>(CreatureActionLearningType::Normal), .action = skill, .knows = true});
	}
	for (uint32_t miracle = 0; miracle < k_LearnEverythingMiracles; ++miracle)
	{
		edits.push_back({.kind = static_cast<uint32_t>(CreatureActionLearningType::Magic),
		                 .action = miracle,
		                 .knows = true,
		                 .countsSightings = false});
	}
	return ApplyScriptKnows(creature, edits);
}

bool CreatureMindSystem::ChangeKnows(entt::entity creature, const CreatureMindState::ScriptKnows& edit)
{
	return ApplyScriptKnows(creature, {&edit, 1});
}

bool CreatureMindSystem::ApplyScriptKnows(entt::entity creature, std::span<const CreatureMindState::ScriptKnows> edits)
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
	// The creature's mind is whole from its making and holds a mind file as soon as it is loaded, so a mind not set up
	// yet is set up now and takes up a file that waits: what it is taught goes over the file's at once, and the player is
	// told of it in the same turn
	SetUpMind(creature, body->species, *mind);
	if (mind->pendingFile != nullptr)
	{
		TakeUpFile(creature, *mind);
	}
	if (!mind->learnt.has_value())
	{
		mind->scriptKnows.insert(mind->scriptKnows.end(), edits.begin(), edits.end());
		return true;
	}
	// what was taught before, then this, in order
	std::vector<CreatureMindState::ScriptKnows> all = std::exchange(mind->scriptKnows, {});
	all.insert(all.end(), edits.begin(), edits.end());
	for (const auto& lesson : TakeUpScriptKnows(
	         all, GetTables(), creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)), *mind->learnt))
	{
		TellHelp(creature, lesson);
	}
	return true;
}

std::vector<creature_help::Message> CreatureMindSystem::TakeUpScriptKnows(std::span<const CreatureMindState::ScriptKnows> edits,
                                                                          const creature_mind_tables::Tables* tables,
                                                                          float speciesMultiplier,
                                                                          creature_mind_model::Learnt& learnt)
{
	std::vector<creature_help::Message> lessons;
	if (tables == nullptr)
	{
		return lessons;
	}
	for (const auto& edit : edits)
	{
		const bool knewIt = creature_watching::Knows(learnt.knowledge, edit.kind, edit.action);
		// A kind or a row the game's tables don't have is left alone
		const bool set = creature_watching::SetKnows(learnt.knowledge, edit.kind, edit.action, edit.knows, tables->miracles,
		                                             speciesMultiplier, edit.countsSightings);
		// Something it did not know before is a lesson learnt: an ordinary skill or a miracle, by its number
		if (set && edit.tell && edit.knows && !knewIt)
		{
			const auto kind = edit.kind == static_cast<uint32_t>(CreatureActionLearningType::Magic)
			                      ? creature_help::Kind::LessonMagicAction
			                      : creature_help::Kind::LessonNormalAction;
			lessons.push_back({.kind = kind, .parameter = edit.action});
		}
	}
	return lessons;
}

namespace
{
/// The player whose thing a deed was done to: a town's or a temple's owner, any other thing's the player it belongs to
/// (effects::HarmedPlayerOf), read of a thing taken out of play too; none once the thing is gone
std::optional<PlayerNames> PlayerOfDeedThing(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* town = registry.TryGet<const Town>(thing))
	{
		return town->owner;
	}
	if (const auto* temple = registry.TryGet<const Temple>(thing))
	{
		return temple->owner;
	}
	return ecs::effects::HarmedPlayerOf(thing);
}
} // namespace

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
		const bool nearEnough =
		    creature_watching::WithinCopyReach(game_math::GetDistanceInMetres(to, from),
		                                       creature_look::SweepCells(ecs::creature_pose::DrawnSize(registry, entity)));
		const auto* leashes = std::as_const(registry).TryGet<const CreatureLeash>(entity);
		const creature_watching::MimicConditions conditions {
		    .phase = mind.developmentPhase,
		    .learningLeashInHand = mind.leash.learningInHand,
		    .reactionPriority = ReactionPriorityOf(registry, entity),
		    .canSee = creature_perceived_desires::CanSeePos(look, from, to) && nearEnough,
		    .lockedSelect = Locator::handSystem::has_value() && Locator::handSystem::value().IsInLockedSelect(entity),
		    .leashWorks = leashes == nullptr || !leashes->worn.has_value() || leashes->worn->works,
		    .computerPlayer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(player),
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
		// Copying its player's anger at one of the player's own things, its anger no longer spares them
		if (creature_watching::CopiesAngerAtOwnPlayer(tables->mimics.at(deed).desire,
		                                              object.has_value() ? PlayerOfDeedThing(registry, *object) : std::nullopt,
		                                              body->owner))
		{
			mind->angerSparesOwnPlayer = false;
		}
		// Where the thing stood that it set about copying the player doing something to
		if (const auto copied = object.has_value() ? PointOf(registry, *object) : std::nullopt)
		{
			mind->copiedAt = *copied;
		}
		// noticing begins now; each stage ends after the table's count of finished actions and a draw of GameRand(1)
		creature_watching::BeginMimicry(mind->learnt->mimicry, deed, tables->mimics, target, game_clock::Turn(),
		                                [](uint32_t range) { return Random(range); });
		// a thing like the one the deed was done to is of its kind
		if (target.has_value())
		{
			mind->learnt->mimicType = ecs::MimicTypeOf(registry, static_cast<entt::entity>(*target));
		}
		// it remembers the deed as the last it started to copy
		mind->observedDeed = creature_watching::ObservedDeed {.rule = deed, .object = target, .magic = magic};
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
		// It notices: what it was doing stops as a failure, with what the leash had it do after its walk, and at its next
		// decision once it is free it plays the table's noticing action on the thing the deed was done to (see
		// PassMimicry). A deed done to no thing is noticed with no action
		creature_mind_model::Think(*mind->learnt, fmt::format("I saw you: {}", tables->mimics.at(deed).name));
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().OrderGivenUp(entity);
		}
		AbandonAction(entity);
	}
}

bool CreatureMindSystem::PassMimicry(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value())
	{
		return false;
	}
	auto& learnt = *mind.learnt;
	const auto before = learnt.mimicry->stage;
	// the stage's time in game turns, compared unsigned
	const auto stage = creature_watching::ProcessMimicry(
	    learnt.mimicry, tables->mimics, game_clock::Turn(),
	    static_cast<uint32_t>(game_clock::TicksForSeconds(creature_watching::k_MimicStageTimeoutSeconds)),
	    [](uint32_t range) { return Random(range); });
	if (!stage.has_value())
	{
		// On to copying the desire: it comes to want what the player seemed to want, from its next pass
		if (learnt.mimicry.has_value() && before != learnt.mimicry->stage &&
		    learnt.mimicry->stage == creature_watching::MimicStage::CopyDesire)
		{
			const auto desire = tables->mimics.at(learnt.mimicry->rule).desire;
			if (desire < tables->texts.size())
			{
				creature_mind_model::Think(learnt, fmt::format("I want what you wanted: {}", tables->texts.at(desire).desire));
			}
		}
		return false;
	}
	switch (*stage)
	{
	case creature_watching::MimicStage::Notice:
		return ForceNotice(creature, mind);
	case creature_watching::MimicStage::CopyAction:
		return ForceCopy(creature, mind);
	case creature_watching::MimicStage::CopyDesire:
		break;
	}
	// Copying its player's desire, the desire is forced on it again, above its other desires for a while, its wish to
	// follow the player's desire let go of; once it has gone more than 40 turns without an action it stops copying,
	// holding its wish to follow its player's desire down. It decides what to do as it would
	const auto& copied = tables->mimics.at(learnt.mimicry->rule);
	if (copied.desire < creature_desires::k_DesireCount && mind.desires.has_value())
	{
		const auto msPerTurn = game_clock::MsPerTurn();
		const auto pass = creature_spell_mind::PassCopyingDesire(
		    *mind.desires, static_cast<Desire>(copied.desire), msPerTurn > 0 ? static_cast<float>(1000 / msPerTurn) : 0.0f,
		    mind_detail::DesireFloorOf(std::as_const(Locator::entitiesRegistry::value()), creature), mind.turnsWithoutAction);
		mind.dominantDesire = pass.cheat;
		if (pass.stops)
		{
			learnt.mimicry.reset();
		}
	}
	return false;
}

bool CreatureMindSystem::ForceNotice(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	// A deed done to no thing, or to one gone, is noticed with no action
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() ||
	    !mind.learnt->mimicry->object.has_value() || mind.learnt->mimicry->rule >= tables->mimics.size())
	{
		return false;
	}
	const auto thing = static_cast<entt::entity>(*mind.learnt->mimicry->object);
	if (!registry.Valid(thing))
	{
		return false;
	}
	const auto notice = tables->mimics.at(mind.learnt->mimicry->rule).noticeAction;
	if (notice >= tables->actions.size())
	{
		return false;
	}
	// Made curious about the thing, it plays the noticing action on it
	mind.noticedWith = tables->actions.at(notice).name;
	return ForceActivity(creature,
	                     {.desire = Desire::Curiosity, .action = tables->actions.at(notice).name, .actionObject = thing});
}

bool CreatureMindSystem::ForceCopy(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() ||
	    mind.learnt->mimicry->rule >= tables->mimics.size())
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& learnt = *mind.learnt;
	const auto& rule = tables->mimics.at(learnt.mimicry->rule);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return false;
	}
	// The thing the player did it to: once a hand holds it, copying stops
	const auto deedThing =
	    learnt.mimicry->object.has_value() ? std::optional(static_cast<entt::entity>(*learnt.mimicry->object)) : std::nullopt;
	const bool onDeedThing = deedThing.has_value() && registry.Valid(*deedThing);
	if (onDeedThing && Locator::handSystem::has_value() && Locator::handSystem::value().IsInMagicHand(*deedThing))
	{
		learnt.mimicry.reset();
		return false;
	}
	// A miracle it copies passes the same test as one it chooses, and an action it does not take while kept within an
	// area it does not copy either; nor one that needs a skill it does not know or a miracle it knows nothing of
	const auto mayDo = [&](uint32_t action) -> const creature_plan_actions::Executor* {
		const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
		if (executor == nullptr || UnavailableWhileKept(registry, creature, mind, action) ||
		    !creature_mind_tables::KnowsWhatItNeeds(tables->actions.at(action), learnt.knowledge) ||
		    !creature_plan_actions::MayTry(*executor,
		                                   creature_plan_actions::IsCast(*executor) &&
		                                       MayCast(creature, mind, action, IsPowerUpCast(tables->actions.at(action).name)),
		                                   executor->build == creature_plan_actions::Build::CastWaterOnItself &&
		                                       Burning(creature)))
		{
			return nullptr;
		}
		return executor;
	};
	// Each copy is done for the desire the deed's row names, in place of the copy before it
	const auto desire = static_cast<Desire>(std::min<uint32_t>(rule.desire, creature_desires::k_DesireCount - 1));
	const auto force = [&](uint32_t action, std::optional<uint32_t> object, const creature_plan_actions::Executor& executor) {
		creature_plan_actions::Situation situation;
		situation.turnsPerSecond = k_TurnsPerSecond;
		situation.self = entt::to_integral(creature);
		SituateHome(registry, creature, situation);
		SituateSacrifice(registry, creature, situation);
		SituateTownTheft(registry, creature, situation);
		if (executor.build == creature_plan_actions::Build::CastWaterOnItself)
		{
			situation.clearSpot = ClearSpotToCastOnItself(registry, creature);
		}
		// The copy is about what the thing belongs to (its town, temple, forest, flock or itself); belonging to nothing,
		// a copy for a desire whose record says so is about the town nearest the creature, however far, which it then
		// knows of
		std::optional<entt::entity> activity =
		    object.has_value() ? BelongsTo(static_cast<entt::entity>(*object)) : std::nullopt;
		if (!activity.has_value() && tables->rules.at(static_cast<size_t>(desire)).social)
		{
			if (const auto town =
			        ecs::map_cells::GetNearestTown(ecs::object::MapCoordsOf(creature), std::numeric_limits<float>::max());
			    town != entt::null)
			{
				ComeAcross(creature, mind, town);
				activity = town;
			}
		}
		// A copy for such a desire about nothing at all, with no town in the world, is no plan: it is not copied
		if (!activity.has_value() && tables->rules.at(static_cast<size_t>(desire)).social)
		{
			return false;
		}
		// An action that uses a thing takes the thing it knows of best to use; knowing of none, it is not copied
		std::optional<uint32_t> instrument;
		if (tables->actions.at(action).needsObjectToUse)
		{
			instrument = InstrumentFor(creature, mind, desire, action, object);
			if (!instrument.has_value())
			{
				return false;
			}
		}
		const bool adopted =
		    Adopt(creature, mind,
		          {.desire = desire,
		           .action = action,
		           .object = object,
		           // Copying counts as pressing as a desire at its strongest, with nothing learnt
		           .priority = creature_planner::Priority(1.0f, std::nullopt,
		                                                  creature_planner::ActionPriority(learnt.opinions.at(action)),
		                                                  creature_planner::k_DefaultUsefulness, std::nullopt),
		           .activityObject = activity.has_value() ? std::optional(entt::to_integral(*activity)) : std::nullopt,
		           .instrument = instrument},
		          situation);
		if (adopted)
		{
			creature_mind_model::Think(learnt, fmt::format("I'm copying you: {}", tables->actions.at(action).name));
		}
		return adopted;
	};
	bool forced = false;
	// First every copy action it can do to the thing the player did it to; after each one forced, it stops there one
	// time in two
	if (onDeedThing)
	{
		for (const auto action : rule.copyActions)
		{
			const auto* executor = mayDo(action);
			if (executor == nullptr ||
			    (executor->target != Target::None && !Accepts(registry, executor->target, *deedThing, creature)))
			{
				continue;
			}
			const auto object = executor->target != Target::None ? std::optional(entt::to_integral(*deedThing)) : std::nullopt;
			if (force(action, object, *executor))
			{
				forced = true;
				if (Random(2) == 0)
				{
					return true;
				}
			}
		}
	}
	// Then each one again to a thing like it, twice over: first the thing it knows of the deed's thing's kind that
	// weighs most by how near it is, then the thing it knows of most useful to act on, of any kind. Each is a thing the
	// desire may act on and the action may be done to, not what it holds when the action's row says so, and not
	// itself; the first of equals
	const auto& lookup = std::as_const(registry);
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
	                      : std::nullopt;
	const auto bestLike = [&](uint32_t action, const creature_plan_actions::Executor& executor,
	                          bool sameKind) -> std::optional<entt::entity> {
		const auto& info = tables->actions.at(action);
		std::optional<entt::entity> best;
		float bestWeight = 0.0f;
		mind.beliefs.ForEach([&](const creature_object_beliefs::Belief& known) {
			const auto thing = known.thing;
			if (thing == creature || !lookup.Valid(thing) || !SuitableForDesire(creature, mind, thing, desire) ||
			    (info.excludesCarriedObject && held == thing) ||
			    !mind_detail::Accepts(lookup, executor.target, thing, creature))
			{
				return;
			}
			if (sameKind && ecs::MimicTypeOf(lookup, thing) != learnt.mimicType)
			{
				return;
			}
			const float weight = sameKind ? DistancePriorityOf(lookup, creature, thing,
			                                                   tables->rules.at(static_cast<size_t>(desire)).distanceWeight)
			                              : ObjectToActOnUsefulness(creature, mind, desire, thing);
			if (weight > bestWeight)
			{
				best = thing;
				bestWeight = weight;
			}
		});
		return best;
	};
	for (const bool sameKind : {true, false})
	{
		for (const auto action : rule.copyActions)
		{
			const auto* executor = mayDo(action);
			if (executor == nullptr)
			{
				continue;
			}
			// an action done to no thing is done to itself
			std::optional<uint32_t> object;
			if (executor->target != Target::None)
			{
				const auto found = bestLike(action, *executor, sameKind);
				if (!found.has_value())
				{
					continue;
				}
				object = entt::to_integral(*found);
			}
			if (force(action, object, *executor))
			{
				forced = true;
				if (Random(2) == 0)
				{
					return true;
				}
			}
		}
	}
	return forced;
}

void CreatureMindSystem::RememberAction(entt::entity creature, CreatureMindState& mind, creature_learning::Context context)
{
	if (!mind.learnt.has_value())
	{
		return;
	}
	// Where the things stand as it takes the plan up: its beliefs of them are brought up to date then, their places too
	{
		const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
		const auto placeOf = [&lookup](std::optional<uint32_t> thing) -> std::optional<std::array<int32_t, 2>> {
			const auto entity = thing.has_value() ? static_cast<entt::entity>(*thing) : entt::null;
			const auto* at = entity != entt::null && lookup.Valid(entity) ? lookup.TryGet<const Transform>(entity) : nullptr;
			if (at == nullptr)
			{
				return std::nullopt;
			}
			const auto coords = map_coords::FromMetres(glm::vec2(at->position.x, at->position.z));
			return std::array<int32_t, 2> {coords.x, coords.z};
		};
		if (!context.place.has_value())
		{
			context.place = placeOf(context.object);
		}
		if (!context.aboutPlace.has_value())
		{
			context.aboutPlace = placeOf(context.about);
		}
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
	const auto teach = [&](TreeKind tree, const creature_tree::Belief& belief, float feedback,
	                       const std::optional<std::array<int32_t, 2>>& place) {
		for (const auto& [other, share] : creature_learning::Spread(desire, feedback, tables->dependencies))
		{
			creature_mind_model::Learn(
			    learnt, tree, other,
			    {.belief = belief, .feedback = std::clamp(share, -1.0f, 1.0f), .saved = creature_mind_model::SavedPlace(place)},
			    tables->attributes.at(static_cast<size_t>(other)));
		}
	};
	const auto taught = creature_learning::ThingLessons(*forgotten, entt::to_integral(creature), *approval);
	if (taught.goal.has_value())
	{
		teach(TreeKind::Goal, *forgotten->aboutBelief, *taught.goal, forgotten->aboutPlace);
	}
	if (taught.actOn.has_value())
	{
		teach(TreeKind::ActOn, *forgotten->belief, *taught.actOn, forgotten->place);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} forgets action {} that was forced on it and approves it by {}",
	                    entt::to_integral(creature), forgotten->action, *approval);
}
