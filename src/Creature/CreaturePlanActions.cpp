/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlanActions.h"

#include <algorithm>
#include <array>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureExplorationAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;
using creature_mind::Activity;

namespace
{
/// Actions of the creature's animation set that have no name elsewhere
constexpr size_t k_Sneeze = 62;
constexpr size_t k_LookAtMe = 69;
constexpr size_t k_PickMe = 74;
/// Looking something over from where it stands, following it about, and gazing at the view, last this long
constexpr float k_LookSeconds = 4.0f;
constexpr float k_FollowSeconds = 8.0f;
constexpr float k_GazeSeconds = 6.0f;

constexpr std::array k_Executors {
    Executor {.action = "EatAfterExamining", .target = Target::Food, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatAlive", .target = Target::LiveFood, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatFromFoodPile", .target = Target::FoodPile, .build = Build::EatPortion, .activity = Activity::Eat},
    Executor {.action = "SleepOnTheSpot", .build = Build::Sleep, .activity = Activity::Sleep},
    Executor {.action = "Poo", .build = Build::Poo, .activity = Activity::Poo},
    Executor {.action = "Puke", .build = Build::Puke, .activity = Activity::Puke},
    Executor {.action = "DrinkFromTheSea", .build = Build::Drink, .activity = Activity::Drink},
    Executor {.action = "ExamineByPickingUp",
              .target = Target::Pickable,
              .build = Build::ExamineByPickingUp,
              .activity = Activity::Examine},
    Executor {.action = "ExamineByLooking", .target = Target::Anything, .build = Build::ExamineByLooking},
    Executor {.action = "ExamineByFollowing", .target = Target::Living, .build = Build::ExamineByFollowing},
    Executor {.action = "ThrowBallAtObject",
              .target = Target::Pickable,
              .build = Build::ThrowAbout,
              .activity = Activity::PlayWithObject},
    // Throwing things about, kicking a ball about and throwing to impress are all throwing what it picks up somewhere
    // nearby, as the throws aimed at an audience need systems that aren't here yet
    Executor {
        .action = "ThrowAround", .target = Target::Pickable, .build = Build::ThrowAbout, .activity = Activity::PlayWithObject},
    Executor {.action = "KickBallAround",
              .target = Target::Pickable,
              .build = Build::ThrowAbout,
              .activity = Activity::PlayWithObject},
    // It walks to the nearest coast and throws the thing into the nearest sea, by its map of the land
    Executor {.action = "ThrowInTheSea",
              .target = Target::Pickable,
              .build = Build::ThrowInTheSea,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowToImpress",
              .target = Target::Pickable,
              .build = Build::ThrowAbout,
              .activity = Activity::PlayWithObject},
    Executor {.action = "KickTree", .target = Target::Tree, .build = Build::Destroy},
    Executor {.action = "PullSillyFaces", .build = Build::Emote, .animation = animations::k_FeelPlayful},
    Executor {.action = "Hurl", .target = Target::Pickable, .build = Build::Hurl, .activity = Activity::Hurl},
    Executor {.action = "Stomp", .target = Target::Destroyable, .build = Build::Destroy},
    Executor {.action = "Kick", .target = Target::Destroyable, .build = Build::Destroy},
    Executor {.action = "SitDown", .build = Build::SitDown, .activity = Activity::Sit},
    Executor {.action = "BeIdle", .build = Build::BeIdle, .activity = Activity::BeIdle},
    Executor {.action = "HangAroundAtHome", .build = Build::HangAround, .activity = Activity::HangAround},
    Executor {.action = "CommunicateState", .build = Build::ShowDesire, .activity = Activity::ShowDesire},
    Executor {.action = "WaveAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtCamera", .build = Build::LookAtCamera},
    Executor {.action = "LookAtHand", .build = Build::FaceCameraEmote, .animation = k_LookAtMe},
    Executor {.action = "PointAtCamera", .build = Build::PointAtCamera},
    Executor {.action = "BePatheticToPlayer", .build = Build::FaceCameraEmote, .animation = k_PickMe},
    Executor {.action = "HowlAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_Summon},
    // What it runs from is what the Fear desire accepts: something that frightens it. Only Fear lists this action
    Executor {.action = "RunAwayFromObject", .target = Target::Frightening, .build = Build::RunFromObject},
    Executor {.action = "BeFrightenedOnTheSpot", .build = Build::Emote, .animation = animations::k_Frightened},
    Executor {.action = "BeSad", .build = Build::Emote, .animation = animations::k_Sad},
    Executor {.action = "Scratch", .build = Build::Emote, .animation = animations::k_Scratch},
    Executor {.action = "Shiver", .build = Build::Emote, .animation = animations::k_Cold},
    Executor {.action = "Sneeze", .build = Build::Emote, .animation = k_Sneeze},
    Executor {.action = "ShowHotness", .build = Build::Emote, .animation = animations::k_Hot},
    Executor {.action = "RunAwayFromPlayer", .build = Build::RunFromPlayer},
    Executor {.action = "ShowImpressiveAnimation", .build = Build::Emote, .animation = animations::k_Impress},
    Executor {
        .action = "Stroke", .target = Target::Villager, .build = Build::ApproachEmote, .animation = animations::k_FeelingNice},
    Executor {
        .action = "SmileAtFriend", .target = Target::Creature, .build = Build::ApproachEmote, .animation = animations::k_Happy},
    Executor {.action = "WaveAtFriend",
              .target = Target::Creature,
              .build = Build::ApproachEmote,
              .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtMountains", .build = Build::LookAtMountains},
    Executor {.action = "LookOutToSea", .build = Build::LookOutToSea},
    Executor {.action = "LookAtSun", .build = Build::LookAbout},
    Executor {.action = "LookAtMoon", .build = Build::LookAbout},
    // Forced on creatures by scripts to stage their scenes; no desire of the game's tables lists them
    Executor {.action = "LookForever", .target = Target::Anything, .build = Build::LookForever},
    Executor {.action = "LookButDontApproach", .target = Target::Anything, .build = Build::LookButDontApproach},
    // What a script's cut scene makes every creature free to think for itself do
    Executor {.action = "LookAtCameraInWideScreen", .build = Build::LookAtCameraInCutScene},
    // The miracles it casts at things: lightning at anything, a heal at a villager hurt enough, a spell at another
    // creature
    Executor {.action = "CastLightningBolt", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastHealSpell", .target = Target::HurtVillager, .build = Build::CastHelpful},
    // Food and wood: food into a built storage pit or at a worship site, scattered from above; the power-up's food at
    // anything, and wood into a storage pit, by a building going up or by a workshop, as helpful miracles. The
    // workshop's is never cast, as the game's table gives it no miracle
    Executor {.action = "CastMagicFood", .target = Target::FoodStore, .build = Build::CastFood},
    Executor {.action = "CastMagicFoodByWorshipSite", .target = Target::WorshipSite, .build = Build::CastFood},
    Executor {.action = "CastMagicFoodPU1", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWood", .target = Target::WoodStore, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWoodByBuildingSite", .target = Target::BuildingSite, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWoodByWorkshop", .target = Target::Workshop, .build = Build::CastHelpful},
    // Water, scattered from above: on crops or a tree, a tree, a field that needs it, and for a town on the tree it is
    // done with. The power-ups' rows cast plain water too
    Executor {.action = "SprinkleMagicWaterOnCrops", .target = Target::Sprinklable, .build = Build::CastWater},
    Executor {.action = "WaterTree", .target = Target::AnyTree, .build = Build::CastWater},
    Executor {.action = "WaterField", .target = Target::DryField, .build = Build::CastWater},
    Executor {.action = "SprinkleMagicWaterPU1OnCrops", .target = Target::Sprinklable, .build = Build::CastWater},
    Executor {.action = "WaterTreeForTown", .target = Target::Anything, .build = Build::CastWaterOnInstrument},
    Executor {.action = "WaterTreePU1ForTown", .target = Target::Anything, .build = Build::CastWaterOnInstrument},
    // And water on itself, when it is on fire
    Executor {.action = "CastMagicWater", .build = Build::CastWaterOnItself},
    Executor {.action = "CastMakeCreatureFreeze", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureSmall", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureBig", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureWeak", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureStrong", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureFat", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureThin", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureInvisible", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureNice", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureAngry", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureHungry", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureFrightened", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureTired", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureIll", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureThirsty", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureItchy", .target = Target::Creature, .build = Build::CastPlayful},
    // The places of its map of the land: hills, coasts, towns and the beach
    Executor {.action = "GoToHillAndLook", .build = Build::GoToHillAndLook},
    Executor {.action = "GoToHillAndSit", .build = Build::GoToHillAndSit},
    Executor {.action = "GoToHillAndWalkAlongRidge", .build = Build::WalkAlongRidge},
    Executor {.action = "ExploreCoastline", .build = Build::ExploreCoast},
    Executor {.action = "ExploreTowns", .build = Build::ExploreTowns},
    Executor {.action = "SitDownOnBeach", .build = Build::SitDownOnBeach},
};

/// The actions only ever forced on a creature: catching what flies at it, when the thing starts to fly
constexpr std::array k_ForcedOnly {
    Executor {.action = "Catch", .target = Target::Anything, .build = Build::Catch},
};

/// The agenda of an action that uses the creature's map of the land. Without the places that the agenda needs, the
/// action fails.
std::optional<std::vector<creature_mind::Step>>
ExplorationAgenda(Build build, std::optional<uint32_t> object, const Situation& situation, const creature_mind::Random& random)
{
	if (!situation.explorePlace.has_value())
	{
		return std::nullopt;
	}
	const auto place = *situation.explorePlace;
	switch (build)
	{
	case Build::GoToHillAndLook:
		return creature_exploration::GoToHillAndLook(place, situation.height);
	case Build::GoToHillAndSit:
		return creature_exploration::GoToHillAndSit(place, situation.height);
	case Build::WalkAlongRidge:
		return creature_exploration::GoToHillAndWalkAlongRidge(place, situation.height);
	case Build::ExploreCoast:
	case Build::ExploreTowns:
		return creature_exploration::Explore(place, situation.height);
	case Build::LookAtMountains:
		return creature_exploration::LookAtMountains(place, random);
	default:
		break;
	}
	if (!situation.exploreSea.has_value())
	{
		return std::nullopt;
	}
	const auto sea = *situation.exploreSea;
	switch (build)
	{
	case Build::SitDownOnBeach:
		return creature_exploration::SitDownOnBeach(place, sea, situation.height, random);
	case Build::LookOutToSea:
		return creature_exploration::LookOutToSea(place, sea, situation.height, random);
	case Build::ThrowInTheSea:
		// It picks the thing up first, unless its hand already holds something
		if (!object.has_value())
		{
			return std::nullopt;
		}
		return creature_exploration::ThrowInTheSea(situation.handFull ? std::nullopt : object, place, sea, situation.height);
	default:
		return std::nullopt;
	}
}
} // namespace

std::span<const Executor> creature_plan_actions::All()
{
	return k_Executors;
}

const Executor* creature_plan_actions::For(std::string_view action)
{
	const auto found = std::ranges::find(k_Executors, action, &Executor::action);
	return found != k_Executors.end() ? &*found : nullptr;
}

const Executor* creature_plan_actions::ForcedFor(std::string_view action)
{
	if (const auto* executor = For(action); executor != nullptr)
	{
		return executor;
	}
	const auto found = std::ranges::find(k_ForcedOnly, action, &Executor::action);
	return found != k_ForcedOnly.end() ? &*found : nullptr;
}

bool creature_plan_actions::IsCast(const Executor& executor)
{
	return executor.build == Build::CastLightning || executor.build == Build::CastHelpful ||
	       executor.build == Build::CastPlayful || executor.build == Build::CastFood || executor.build == Build::CastWater ||
	       executor.build == Build::CastWaterOnInstrument || executor.build == Build::CastWaterOnItself;
}

bool creature_plan_actions::MayTry(const Executor& executor, bool mayCast, bool onFire)
{
	if (!IsCast(executor))
	{
		return true;
	}
	return mayCast && (executor.build != Build::CastWaterOnItself || onFire);
}

bool creature_plan_actions::Possible(const Executor& executor, const Situation& situation)
{
	switch (executor.build)
	{
	case Build::Drink:
		return situation.water.has_value();
	case Build::Hurl:
		return situation.hurlTarget.has_value();
	case Build::FaceCameraEmote:
	case Build::RunFromPlayer:
		return situation.camera.has_value();
	case Build::ShowDesire:
		return situation.showDesireAnimation.has_value();
	case Build::LookButDontApproach:
		return situation.drawnSeconds.has_value();
	case Build::CastWaterOnInstrument:
		// Nothing to cast on: it has no object to use
		return situation.instrument.has_value();
	default:
		return true;
	}
}

std::optional<std::vector<creature_mind::Step>> creature_plan_actions::Agenda(const Executor& executor,
                                                                              std::optional<uint32_t> object,
                                                                              glm::vec2 objectPoint, const Situation& situation,
                                                                              const creature_mind::Random& random,
                                                                              const std::optional<CastInfo>& cast)
{
	if (!Possible(executor, situation) || (executor.target != Target::None && !object.has_value()) ||
	    (IsCast(executor) && !cast.has_value()))
	{
		return std::nullopt;
	}
	switch (executor.build)
	{
	case Build::Eat:
		return creature_mind::Eat(*object);
	case Build::EatPortion:
		// (pending) with something in its hand the game's plan either ends at once, when it can eat what it holds, or
		// deals with what it holds first; ours makes no agenda
		if (situation.handFull)
		{
			return std::nullopt;
		}
		return creature_mind::EatPortion(*object, objectPoint, situation.portionApproach);
	case Build::EatFromStoragePit:
		// With no food pile it can't; what it holds does not matter
		if (!situation.pitPile.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::EatFromStoragePit(*situation.pitPile, situation.pitPilePoint, situation.height);
	case Build::EatFromField:
	case Build::EatFromTree:
		// (pending) with something it can eat in its hand the game's plan ends at once, to what end is not known; ours
		// makes no agenda. Holding anything else, it goes all the same
		if (situation.holdingEatable)
		{
			return std::nullopt;
		}
		return executor.build == Build::EatFromField
		           ? creature_mind::EatFromField(*object, objectPoint, situation.height)
		           : creature_mind::EatFromTree(*object, objectPoint, situation.portionApproach);
	case Build::Sleep:
		return creature_mind::Sleep(random);
	case Build::Poo:
		return creature_mind::Poo(random);
	case Build::Puke:
		return creature_mind::Puke();
	case Build::Drink:
		return creature_mind::Drink(situation.water->shore, situation.water->water);
	case Build::ExamineByPickingUp:
		return creature_mind::ExamineByPickingUp(*object, random);
	case Build::ExamineByLooking:
		return creature_mind::LookAt(objectPoint, k_LookSeconds);
	case Build::ExamineByFollowing:
		return creature_mind::FollowFor(*object, k_FollowSeconds);
	case Build::ThrowAbout:
		return creature_mind::ThrowAbout(*object, random);
	case Build::Hurl:
		return creature_mind::Hurl(*object, *situation.hurlTarget, random);
	case Build::Destroy:
		return creature_mind::DestroyThing(*object);
	case Build::SitDown:
		// Under a script's control, told to sit at a thing, it goes and sits there
		if (situation.controlledByScript && object.has_value() && situation.drawnSeconds.has_value())
		{
			return creature_mind::SitDownAt(*object, objectPoint, situation.radius, *situation.drawnSeconds);
		}
		return std::vector {creature_mind::SitDown(random)};
	case Build::BeIdle:
		return creature_mind::BeIdle(random);
	case Build::HangAround:
		return creature_mind::HangAround(random);
	case Build::ShowDesire:
		return creature_mind::Emote(*situation.showDesireAnimation);
	case Build::Emote:
		return creature_mind::Emote(executor.animation);
	case Build::FaceCameraEmote:
		return creature_mind::FaceAndEmote(*situation.camera, executor.animation);
	case Build::LookAtCamera:
		return creature_mind::LookAtCamera();
	case Build::PointAtCamera:
		// It may be chosen with no camera to point at, and then the plan fails as its agenda is made. Looking at the
		// camera never fails: without a camera it is over at once
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::PointAtCamera(situation.hasPlayer, situation.eye);
	case Build::LookAtCameraInCutScene:
		// It never fails: without a camera it doesn't point at it
		return creature_mind::LookAtCameraInCutScene(situation.camera.has_value() ? std::optional(situation.eye) : std::nullopt,
		                                             random);
	case Build::ApproachEmote:
		return creature_mind::ApproachAndEmote(*object, executor.animation);
	case Build::RunFromObject:
		return creature_mind::RunFrom(objectPoint);
	case Build::RunFromPlayer:
		return creature_mind::RunFrom(*situation.camera);
	case Build::LookAbout:
		return creature_mind::LookAbout(k_GazeSeconds);
	case Build::LookForever:
		return creature_mind::LookForever(*object);
	case Build::LookButDontApproach:
		return creature_mind::LookButDontApproach(*object, *situation.drawnSeconds);
	case Build::CastLightning:
		return creature_mind::CastAt(creature_mind::CastStyle::Lightning, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastHelpful:
		return creature_mind::CastAt(creature_mind::CastStyle::Helpful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastPlayful:
		return creature_mind::CastAt(creature_mind::CastStyle::Playful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastFood:
		return creature_mind::CastFoodAt(cast->gesture, *object,
		                                 cast->atNeedsSign ? creature_mind::k_FoodGoNearSign : creature_mind::k_FoodGoNear);
	case Build::CastWater:
		return creature_mind::CastWaterAt(*object, cast->height);
	case Build::CastWaterOnInstrument:
		return creature_mind::CastWaterAt(*situation.instrument, cast->height);
	case Build::CastWaterOnItself:
		// With no clear spot near it, it can't
		if (!situation.clearSpot.has_value() || !cast->caster.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastWaterOnItself(*situation.clearSpot, *cast->caster, cast->height);
	case Build::Catch:
		return creature_mind::CatchThing(*object);
	case Build::GoToHillAndLook:
	case Build::GoToHillAndSit:
	case Build::WalkAlongRidge:
	case Build::ExploreCoast:
	case Build::ExploreTowns:
	case Build::SitDownOnBeach:
	case Build::LookAtMountains:
	case Build::LookOutToSea:
	case Build::ThrowInTheSea:
		return ExplorationAgenda(executor.build, object, situation, random);
	}
	return std::nullopt;
}
