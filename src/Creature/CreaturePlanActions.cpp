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

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureExplorationAgendas.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePlayerAgendas.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureTempleDance.h"
#include "Creature/CreatureTownHelp.h"
#include "Creature/CreatureTownTheft.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;
using creature_mind::Activity;

namespace
{
/// Actions of the creature's animation set that have no name elsewhere
constexpr size_t k_Sneeze = 62;
constexpr size_t k_PickMe = 74;
/// Looking something over from where it stands, following it about, and gazing at the view, last this long
constexpr float k_LookSeconds = 4.0f;
constexpr float k_FollowSeconds = 8.0f;
constexpr float k_GazeSeconds = 6.0f;
/// The looks at the sun and at the moon, which the sky allows only by day and only at night
constexpr std::string_view k_LookAtSun = "LookAtSun";
constexpr std::string_view k_LookAtMoon = "LookAtMoon";
/// Random fractions of a second are drawn in this many steps, as the idle mind draws them
constexpr uint32_t k_FractionSteps = 1000;

float RandomFraction(const creature_mind::Random& random)
{
	return static_cast<float>(random(k_FractionSteps)) / static_cast<float>(k_FractionSteps);
}

/// A chance from 0 to 1 in fine steps, from the whole-number random
float FineRandomFraction(const creature_mind::Random& random)
{
	constexpr uint32_t k_Steps = 1U << 24U;
	return static_cast<float>(random(k_Steps)) / static_cast<float>(k_Steps);
}

constexpr std::array k_Executors {
    Executor {.action = "EatAfterExamining", .target = Target::Food, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatAlive", .target = Target::LiveFood, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatFromFoodPile", .target = Target::FoodPile, .build = Build::EatPortion, .activity = Activity::Eat},
    Executor {.action = "EatFromStoragePit",
              .target = Target::StoragePitWithFood,
              .build = Build::EatFromStoragePit,
              .activity = Activity::Eat},
    // The game's table names eating from a field "EatFromContainer"
    Executor {
        .action = "EatFromContainer", .target = Target::FieldWithFood, .build = Build::EatFromField, .activity = Activity::Eat},
    Executor {.action = "EatFromTree", .target = Target::TreeBigEnough, .build = Build::EatFromTree, .activity = Activity::Eat},
    Executor {.action = "SleepOnTheSpot", .build = Build::Sleep, .activity = Activity::Sleep},
    Executor {.action = "Poo", .build = Build::Poo, .activity = Activity::Poo},
    // The poo desire's other action. Its agenda is the poo's own: only what comes out as it squats tells them apart
    Executor {.action = "Fart", .build = Build::Poo, .activity = Activity::Poo},
    Executor {.action = "Puke", .build = Build::Puke, .activity = Activity::Puke},
    Executor {.action = "DrinkFromTheSea", .build = Build::Drink, .activity = Activity::Drink},
    // Getting high is only ever done to a mushroom, as the desire to get high accepts nothing else
    Executor {.action = "GetHigh", .target = Target::Mushroom, .build = Build::GetHigh, .activity = Activity::Eat},
    // Trees: given to a town's storage pit as wood, only one that is fully built, or planted by something
    Executor {.action = "GiveWoodFromTreeToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::PickableTree,
              .build = Build::GiveWoodToStore},
    // And to a building going up or wanting repair, and mending a house with a tree
    Executor {.action = "GiveWoodFromTreeToBuildingSite",
              .target = Target::BuildingSite,
              .use = Target::PickableTree,
              .build = Build::GiveWoodToStore},
    Executor {.action = "HelpRepairHouse",
              .target = Target::NeedsRepair,
              .use = Target::PickableTree,
              .build = Build::HelpRepairHouse},
    Executor {.action = "PlantTree",
              .target = Target::Anything,
              .use = Target::PlantableTree,
              .notWhatItHolds = true,
              .build = Build::PlantTree},
    Executor {.action = "ExamineByPickingUp",
              .target = Target::Pickable,
              .build = Build::ExamineByPickingUp,
              .activity = Activity::Examine},
    // Looked at: not a creature, a field, a fish farm, a vortex or a teleport stone
    Executor {.action = "ExamineByLooking", .target = Target::Examinable, .build = Build::ExamineByLooking},
    Executor {.action = "ExamineByFollowing", .target = Target::Living, .build = Build::ExamineByFollowing},
    Executor {.action = "ThrowBallAtObject",
              .target = Target::Anything,
              .use = Target::Toy,
              .build = Build::ThrowToy,
              .activity = Activity::PlayWithObject},
    // Practising a throw on what it may pick up, and throwing living things about
    Executor {.action = "PracticeThrow",
              .target = Target::Pickable,
              .build = Build::PracticeThrow,
              .activity = Activity::PlayWithObject},
    Executor {
        .action = "BreakRock", .target = Target::PickableRock, .build = Build::BreakRock, .activity = Activity::PlayWithObject},
    // Stamping on a rock, which breaks in two
    Executor {.action = "SmashStoneInHalf", .target = Target::Rock, .build = Build::Destroy},
    // Toys: stroking a cuddly one, taking one home, throwing a die, and throwing one to the player's hand. The game's
    // data names stroking a toy "StrokToy"
    Executor {.action = "StrokToy", .target = Target::CuddlyToy, .build = Build::StrokeToy},
    Executor {.action = "TakeToyHome", .target = Target::ToyAwayFromHome, .build = Build::TakeToyHome},
    Executor {.action = "ThrowDie", .target = Target::Die, .build = Build::ThrowDie, .activity = Activity::PlayWithObject},
    Executor {.action = "PlayThrowingGameWithPlayer",
              .use = Target::Toy,
              .build = Build::ThrowToPlayer,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowAround",
              .target = Target::AnyLiving,
              .build = Build::PracticeThrow,
              .activity = Activity::PlayWithObject},
    Executor {.action = "KickBallAround",
              .target = Target::ToyBall,
              .build = Build::KickBallAround,
              .activity = Activity::PlayWithObject},
    // It walks to the nearest coast and throws the thing into the nearest sea, by its map of the land
    Executor {.action = "ThrowInTheSea",
              .target = Target::SeaThrowable,
              .build = Build::ThrowInTheSea,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowToImpress",
              .target = Target::Anything,
              .use = Target::ThrowableToImpress,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    // Kicking a tree or another thing sends it flying, as a kicked ball; a stomp only knocks at it
    Executor {.action = "KickTree", .target = Target::Tree, .build = Build::Kick},
    Executor {.action = "PullSillyFaces", .build = Build::Emote, .animation = animations::k_FeelPlayful},
    Executor {.action = "Hurl",
              .target = Target::Stonable,
              .use = Target::ThrowingDamage,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    Executor {.action = "Stomp", .target = Target::Destroyable, .build = Build::Destroy},
    Executor {.action = "Kick", .target = Target::Destroyable, .build = Build::Kick},
    Executor {.action = "SitDown", .build = Build::SitDown, .activity = Activity::Sit},
    Executor {.action = "BeIdle", .build = Build::BeIdle, .activity = Activity::BeIdle},
    Executor {.action = "HangAroundAtHome", .build = Build::HangAround, .activity = Activity::HangAround},
    Executor {.action = "CommunicateState", .build = Build::ShowDesire, .activity = Activity::ShowDesire},
    Executor {.action = "WaveAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtCamera", .build = Build::LookAtCamera},
    Executor {.action = "LookAtHand", .build = Build::LookAtHand},
    Executor {.action = "PointAtCamera", .build = Build::PointAtCamera},
    Executor {.action = "BePatheticToPlayer", .build = Build::FaceCameraEmote, .animation = k_PickMe},
    Executor {.action = "HowlAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_Summon},
    // What it runs from is what the Fear desire accepts: something that frightens it. Only Fear lists this action
    Executor {.action = "RunAwayFromObject", .target = Target::Frightening, .build = Build::RunFromObject},
    // Frightened on the spot about something that frightens it, as the Fear desire accepts it: its agenda never uses
    // the thing, which only decides whether the planner can choose it
    Executor {
        .action = "BeFrightenedOnTheSpot", .target = Target::Frightening, .build = Build::BeFrightened, .thingUnused = true},
    Executor {.action = "BeSad", .build = Build::Emote, .animation = animations::k_Sad},
    Executor {.action = "Scratch", .build = Build::Emote, .animation = animations::k_Scratch},
    Executor {.action = "Shiver", .build = Build::Emote, .animation = animations::k_Cold},
    Executor {.action = "Sneeze", .build = Build::Emote, .animation = k_Sneeze},
    Executor {.action = "ShowHotness", .build = Build::Emote, .animation = animations::k_Hot},
    Executor {.action = "RunAwayFromPlayer", .build = Build::RunFromPlayer},
    Executor {.action = "ShowImpressiveAnimation", .build = Build::Emote, .animation = animations::k_Impress},
    Executor {.action = "DanceWithVillagers",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_ImpressDanceType},
    // The other dances with villagers. The playful dance with them and the story need a villager near the creature,
    // as the dance to impress does. The table keeps only the first 32 letters of a name.
    Executor {.action = "DancePlayfullyWithVillagersParti",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_PlayfulDanceType},
    Executor {.action = "TellVillagersAStory",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_StoryDanceType},
    Executor {.action = "PlayGameWithVillagers",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_GameDanceType},
    Executor {.action = "DancePlayfullyWithVillagersWatch",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_WatchedDanceType},
    Executor {.action = "DanceAmorouslyWithVillagers",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_AmorousDanceType},
    Executor {.action = "PracticeDance", .build = Build::PracticeDance},
    // The dance outside a worship site goes to the heart of the creature's own temple; the dance around a thing goes to
    // any thing
    Executor {.action = "DanceOutsideWorshipSite", .target = Target::WorshipSite, .build = Build::DanceOutsideTemple},
    Executor {.action = "DanceAroundArtefact", .target = Target::Playable, .build = Build::DanceAroundThing},
    Executor {
        .action = "Stroke", .target = Target::Villager, .build = Build::ApproachEmote, .animation = animations::k_FeelingNice},
    Executor {.action = "SmileAtFriend", .target = Target::CreatureFreeForJoint, .build = Build::SmileAtFriend},
    Executor {.action = "InspectCreature", .target = Target::Creature, .build = Build::InspectCreature},
    Executor {.action = "LookAtFlyingObject", .target = Target::Anything, .build = Build::LookAtFlyingObject},
    Executor {.action = "HowlAtFriend", .target = Target::Creature, .build = Build::HowlAtFriend},
    Executor {.action = "PineForFriend", .target = Target::Creature, .build = Build::PineForFriend},
    Executor {.action = "LookForFriend", .target = Target::Creature, .build = Build::LookForFriend},
    Executor {.action = "WaveAtFriend", .target = Target::CreatureFreeForJoint, .build = Build::WaveAtFriend},
    Executor {.action = "BeHappyWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::BeHappyWithFriend},
    Executor {.action = "PooWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::PooWithFriend},
    Executor {.action = "SitWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::SitWithFriend},
    Executor {.action = "SleepWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::SleepWithFriend},
    Executor {.action = "DrinkWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::DrinkWithFriend},
    Executor {.action = "HelpTownWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::HelpTownWithFriend},
    Executor {.action = "AttackWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::AttackTownWithFriend},
    Executor {.action = "LookAtMountains", .build = Build::LookAtMountains},
    Executor {.action = "LookOutToSea", .build = Build::LookOutToSea, .leavesHome = true},
    Executor {.action = k_LookAtSun, .build = Build::LookAbout},
    Executor {.action = k_LookAtMoon, .build = Build::LookAbout},
    // Sleeping at home, waving at a thing, putting down what it holds and lying dead for good: the scripts of the first
    // land's lessons force them, and the desires that list them weigh them too
    Executor {.action = "SleepAtHome", .build = Build::SleepAtHome, .activity = Activity::Sleep},
    // The thing waved at is a creature: the original's row takes no other
    Executor {.action = "WaveAtObject", .target = Target::Creature, .build = Build::WaveAt},
    Executor {.action = "PutDown", .build = Build::PutDown, .activity = Activity::PutDown},
    Executor {.action = "DeadForever", .build = Build::DeadForever},
    // Holding up to its player something it can pick up, as the row's test takes
    Executor {.action = "HoldObject", .target = Target::Pickable, .build = Build::HoldObject},
    // Sleeping by any thing: the row's test of the thing measures from the creature, not from the thing
    Executor {
        .action = "SleepByObject", .target = Target::Anything, .build = Build::SleepByObject, .activity = Activity::Sleep},
    // The odd actions. The desire to behave strangely chooses the last one. Nothing in the game chooses the other two,
    // but a script can.
    Executor {.action = "ShowLearntLesson", .build = Build::ShowLearntLesson},
    Executor {.action = "LookConfused", .build = Build::LookConfused},
    Executor {.action = "BehaveStrangely", .build = Build::BehaveStrangely},
    // Forced on creatures by scripts to stage their scenes; no desire of the game's tables lists them
    Executor {.action = "LookForever", .target = Target::Anything, .build = Build::LookForever},
    Executor {.action = "LookButDontApproach", .target = Target::Anything, .build = Build::LookButDontApproach},
    Executor {.action = "PointAtObject", .target = Target::Anything, .build = Build::PointAtThing},
    // What a script's cut scene makes every creature free to think for itself do
    Executor {.action = "LookAtCameraInWideScreen", .build = Build::LookAtCameraInCutScene},
    // What it does with its player to get attention, or to tell the player what it thinks. The thing it throws at the
    // camera, or shows the player, is one it may pick up, which it takes up first
    Executor {.action = "GoToHand", .build = Build::GoToHand},
    Executor {.action = "PointAtHand", .build = Build::PointAtHand},
    Executor {.action = "ShowPlayerHowNiceYouThinkHeIs", .build = Build::ShowHowNice},
    Executor {.action = "PrayToPlayer", .build = Build::PrayToPlayer},
    Executor {.action = "ThrowAtCamera", .target = Target::Pickable, .use = Target::Pickable, .build = Build::ThrowAtCamera},
    Executor {.action = "WatchPlayer", .build = Build::WatchPlayer},
    Executor {.action = "GoToMiddleOfScreen", .build = Build::GoToMiddleOfScreen},
    Executor {.action = "FollowPlayer", .build = Build::FollowPlayer},
    Executor {.action = "BeCrossWithPlayer", .build = Build::BeCrossWithPlayer},
    Executor {.action = "ShowPlayerAnObject", .use = Target::Pickable, .build = Build::ShowPlayerAnObject},
    Executor {.action = "BeSillyWithPlayer", .build = Build::BeSillyWithPlayer},
    // The miracles it casts at things: lightning at anything, a heal at a villager hurt enough, a spell at another
    // creature. The power-ups of the heal are cast as the heal is
    Executor {.action = "CastLightningBolt", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastHealSpell", .target = Target::HurtVillager, .build = Build::CastHelpful},
    Executor {.action = "CastHealSpellPU1", .target = Target::HurtVillager, .build = Build::CastHelpful},
    // The shields at anything: the physical shield and the shield's power-up as helpful miracles, the shield itself
    // from where it stands
    Executor {.action = "CastPhysicalShield", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastShieldPU1", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastShield", .target = Target::Anything, .build = Build::CastShield},
    // The town's shield, at a built storage pit, cast as the shield itself is
    Executor {.action = "CastShieldAroundTown", .target = Target::BuiltStoragePit, .build = Build::CastShield},
    // A miracle that impresses, cast at a villager to impress its people
    Executor {.action = "CastImpressiveSpell", .target = Target::Villager, .build = Build::CastToImpress},
    // A forest, cast at the place where a villager is
    Executor {.action = "CastMagicForest", .target = Target::Villager, .build = Build::CastForest},
    // Food and wood: food into a built storage pit or at a worship site, scattered from above; the power-up's food at
    // anything, and wood into a storage pit, by a building going up or by a workshop, as helpful miracles. The game's
    // table gives the workshop's row no miracle, so the creature never chooses it, but a forced plan casts wood
    Executor {.action = "CastMagicFood", .target = Target::FoodStore, .build = Build::CastFood},
    Executor {.action = "CastMagicFoodByWorshipSite", .target = Target::WorshipSite, .build = Build::CastFood},
    Executor {.action = "CastMagicFoodPU1", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWood", .target = Target::WoodStore, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWoodByBuildingSite", .target = Target::BuildingSite, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWoodByWorkshop",
              .target = Target::Workshop,
              .build = Build::CastHelpful,
              .castMagic = static_cast<uint32_t>(MagicType::Wood)},
    // Water, scattered from above: on crops or a tree, a tree, a field that needs it, and for a town on the tree it is
    // done with. The power-ups' rows cast plain water too
    Executor {.action = "SprinkleMagicWaterOnCrops", .target = Target::Sprinklable, .build = Build::CastWater},
    Executor {.action = "WaterTree", .target = Target::AnyTree, .build = Build::CastWater},
    Executor {.action = "WaterField", .target = Target::DryField, .build = Build::CastWater},
    Executor {.action = "SprinkleMagicWaterPU1OnCrops", .target = Target::Sprinklable, .build = Build::CastWater},
    Executor {.action = "WaterTreeForTown", .target = Target::Anything, .build = Build::CastWaterOnInstrument},
    Executor {.action = "WaterTreePU1ForTown", .target = Target::Anything, .build = Build::CastWaterOnInstrument},
    // Water on a thing that burns, to put its fire out
    Executor {.action = "PutOutFireWithMagicWater", .target = Target::Burning, .build = Build::CastWater},
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
    // The miracles that attack: the power-ups of lightning, the fireball and its power-ups, the explosion and its
    // power-ups, the tornado and the lightning storm
    Executor {.action = "CastLightningBoltPU1", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastLightningBoltPU2", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastFireball", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastFireballPU1", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastFireballPU2", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastExplosion", .target = Target::Anything, .build = Build::CastExplosion},
    Executor {.action = "CastExplosionPU1", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastExplosionPU2", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastTornado", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastLightningStorm", .target = Target::Anything, .build = Build::CastAggressive},
    // To amuse a creature that seems friendly, it casts a spell on creatures that it picks at random
    Executor {.action = "CastAmusingSpellOnCreature", .target = Target::FriendlyCreature, .build = Build::CastAmusing},
    // The places of its map of the land: hills, coasts, towns and the beach
    Executor {.action = "GoToHillAndLook", .build = Build::GoToHillAndLook},
    Executor {.action = "GoToHillAndSit", .build = Build::GoToHillAndSit},
    Executor {.action = "GoToHillAndWalkAlongRidge", .build = Build::WalkAlongRidge},
    Executor {.action = "ExploreCoastline", .build = Build::ExploreCoast},
    Executor {.action = "ExploreTowns", .build = Build::ExploreTowns},
    Executor {.action = "SitDownOnBeach", .build = Build::SitDownOnBeach, .leavesHome = true},
    // Getting better: resting, healing itself, and resting on the spot. The one-shot miracles that restore health wait
    // for the creature's use of globes
    Executor {.action = "RestToGetBetter", .build = Build::RestToGetBetter},
    Executor {.action = "GoHomeToRecover", .build = Build::GoHomeToRecover},
    Executor {.action = "HealHimself", .build = Build::HealItself},
    Executor {.action = "RestOnTheSpot", .build = Build::RestOnTheSpot},
    // Home and the temple
    Executor {.action = "GoHome", .build = Build::GoHome},
    Executor {.action = "RunHome", .build = Build::RunHome},
    Executor {.action = "EnterCitadel", .build = Build::EnterTemple},
    Executor {.action = "PrayAtCitadel", .build = Build::PrayAtTemple},
    Executor {.action = "LookAtCitadel", .target = Target::TemplePart, .build = Build::LookAtTemple},
    // Helping a town: food and wood to its store, food by a worship site, a cow in its store, a breeder, a villager
    // taken home or to a worship site, its totem raised or lowered
    Executor {.action = "GiveFoodFromFieldToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::FieldWithFood,
              .build = Build::GiveFieldFoodToStore},
    Executor {.action = "GiveFruitFromTreeToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::TreeBigEnough,
              .build = Build::GiveFruitToStore},
    // The magic food it uses is a pile of magic food that it may eat from: not one by its town's storage pit
    Executor {.action = "GiveMagicFoodToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::FoodPile,
              .build = Build::GiveMagicFoodToStore},
    Executor {.action = "GiveMagicWoodToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::MagicWoodPile,
              .build = Build::GiveMagicWoodToStore},
    Executor {.action = "PutFoodFromFieldByWorshipSite",
              .target = Target::WorshipSite,
              .use = Target::FieldWithFood,
              .build = Build::PutFieldFoodBySite},
    Executor {.action = "DropCowInStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::Cow,
              .build = Build::DropCowInStore},
    Executor {.action = "MakeBreederDisciple", .target = Target::BreederVillager, .build = Build::MakeBreeder},
    Executor {.action = "TakeVillagerHomeToSleep", .target = Target::VillagerFarFromHome, .build = Build::TakeVillagerHome},
    Executor {.action = "BringVillagersToWorshipSite",
              .target = Target::WorshipSite,
              .use = Target::VillagerNotWorshipping,
              .build = Build::BringVillagerToSite},
    Executor {.action = "RaiseTotemPole", .target = Target::PlayerTotem, .build = Build::RaiseTotem},
    Executor {.action = "LowerTotemPole", .target = Target::PlayerTotem, .build = Build::LowerTotem},
    // Playing with villagers
    Executor {.action = "PlayfullyFrightenVillagers", .target = Target::Villager, .build = Build::FrightenVillagers},
    Executor {.action = "PlayfullyInteractWithVillager", .target = Target::Villager, .build = Build::PlayWithVillager},
    Executor {.action = "PlayfullyKissVillager", .target = Target::Villager, .build = Build::KissVillager},
    // The sacrifice: no desire chooses it, but a script or a copy of the player can make the creature do it
    Executor {.action = "Sacrifice", .target = Target::AnyLiving, .build = Build::Sacrifice},
    // The desire to steal: the creature takes a thing that it can steal to its temple
    Executor {.action = "StealObjectAndTakeToCitadel", .target = Target::Stealable, .build = Build::StealAndPutByTemple},
    // The thefts that only a copy of the player starts: a stolen thing to a town of its player, and food or wood from a
    // farm or a store of another player into a store of its player. The theft from a farm makes the agenda of the gift
    // of food from a field.
    Executor {.action = "StealObjectAndTakeToTown", .target = Target::Stealable, .build = Build::StealAndPutInTown},
    Executor {.action = "StealFoodFromFarm",
              .target = Target::OwnStoragePit,
              .use = Target::OthersField,
              .build = Build::GiveFieldFoodToStore},
    Executor {.action = "StealFoodFromStoragePit",
              .target = Target::OwnStoragePit,
              .use = Target::OthersStoragePit,
              .build = Build::StealFoodFromStore},
    Executor {.action = "StealWoodFromStoragePit",
              .target = Target::OwnStoragePit,
              .use = Target::OthersStoragePit,
              .build = Build::StealWoodFromStore},
    // Bringing things home: a thing far from home, food, and wheat from a field
    Executor {.action = "BringHome", .target = Target::BringableHome, .build = Build::BringHome},
    Executor {.action = "BringFoodHome", .target = Target::Food, .build = Build::BringFoodHome},
    Executor {.action = "TakeFoodFromFieldToHome", .target = Target::FieldWithFood, .build = Build::TakeFieldFoodHome},
};

/// The actions only ever forced on a creature: catching what flies at it, when the thing starts to fly, and fighting
/// another creature. What it catches is a thing it could pick up, as the game's row asks, though forcing the catch does
/// not ask it
constexpr std::array k_ForcedOnly {
    Executor {.action = "Catch", .target = Target::Pickable, .build = Build::Catch},
    // A fight a script forces: with another creature, as the game's row asks
    Executor {.action = "Fight", .target = Target::Creature, .build = Build::Fight},
    // Watching the telly is built as the poo is, and no desire lists it: forced on it, it has a poo
    Executor {.action = "WatchTelly", .build = Build::Poo, .activity = Activity::Poo},
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

/// The agenda of a town help action (see creature_town_help)
std::optional<std::vector<creature_mind::Step>> TownHelpAgenda(const Executor& executor, std::optional<uint32_t> object,
                                                               const Situation& situation, const creature_mind::Random& random)
{
	namespace help = creature_town_help;
	const uint32_t thing = object.value_or(0);
	switch (executor.build)
	{
	case Build::GiveFieldFoodToStore:
	case Build::GiveFruitToStore:
	case Build::GiveMagicFoodToStore:
	case Build::GiveMagicWoodToStore:
	{
		// It takes a handful unless its hand holds what it gives: food it can eat, or a tree for wood
		const bool wood = executor.build == Build::GiveMagicWoodToStore;
		std::optional<help::HandfulSource> take;
		if (!(wood ? situation.holdingTree : situation.holdingEatable))
		{
			if (!situation.instrument.has_value())
			{
				return std::nullopt;
			}
			take = help::HandfulSource {.thing = *situation.instrument, .point = situation.instrumentPoint};
			take->within = help::ThingReach(situation.radius, situation.instrumentRadius);
			if (executor.build == Build::GiveFieldFoodToStore)
			{
				take->within = help::FieldReach(situation.height);
			}
			else if (executor.build == Build::GiveFruitToStore)
			{
				take->look = PotInfo::TreeFruitInHand;
			}
			else if (wood)
			{
				take->look = help::MagicWoodLook(random);
			}
		}
		return help::GiveHandfulToStore(take, thing, situation.height);
	}
	case Build::PutFieldFoodBySite:
	{
		// It needs a field to take food from, and the place where the site keeps its food
		if (!situation.instrument.has_value() || !situation.siteFood.has_value())
		{
			return std::nullopt;
		}
		std::optional<help::HandfulSource> take;
		if (!situation.holdingEatable)
		{
			take = help::HandfulSource {.thing = *situation.instrument, .point = situation.instrumentPoint};
		}
		return help::PutFieldFoodBySite(take, *situation.siteFood, situation.height);
	}
	case Build::DropCowInStore:
		// It needs a cow to use, also when it holds one
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return help::DropCowInStore(situation.holdingCow ? std::nullopt : situation.instrument, thing, situation.height);
	case Build::MakeBreeder:
		return help::MakeBreeder(thing);
	case Build::TakeVillagerHome:
		if (!situation.villagerHome.has_value())
		{
			return std::nullopt;
		}
		return help::TakeVillagerHome(situation.holdingVillager ? std::nullopt : object, *situation.villagerHome);
	case Build::BringVillagerToSite:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return help::BringVillagerToSite(situation.holdingVillager ? std::nullopt : situation.instrument, thing);
	case Build::RaiseTotem:
	case Build::LowerTotem:
		return help::MoveTotem(thing, executor.build == Build::RaiseTotem);
	case Build::FrightenVillagers:
		return help::FrightenVillagers(thing, situation.height);
	case Build::PlayWithVillager:
		return help::PlayWithVillager(thing, situation.height, random);
	case Build::KissVillager:
		return help::KissVillager(thing, situation.height);
	default:
		break;
	}
	return std::nullopt;
}

/// The joint actions with a friend. Without what the action needs (the friend, a clear place, a coast and a sea, or a
/// town with a storage pit), the creature cannot do it.
std::optional<std::vector<creature_mind::Step>> FriendsAgenda(const Executor& executor, uint32_t friendly,
                                                              const Situation& situation, const creature_mind::Random& random)
{
	if (!situation.friendly.has_value())
	{
		return std::nullopt;
	}
	const creature_mind::Friends friends {
	    .self = situation.self,
	    .friendly = friendly,
	    .selfPoint = glm::vec2(situation.standing.x, situation.standing.z),
	    .friendPoint = situation.friendly->point,
	    .height = situation.height,
	    .friendHeight = situation.friendly->height,
	};
	switch (executor.build)
	{
	case Build::BeHappyWithFriend:
		return creature_mind::BeHappyWithFriend(friends);
	case Build::PooWithFriend:
		return creature_mind::PooWithFriend(friends, random);
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	{
		if (!situation.clearArea.has_value())
		{
			return std::nullopt;
		}
		const bool sit = executor.build == Build::SitWithFriend;
		const float radius = sit ? creature_mind::SitTogetherRadius(friends.height, friends.friendHeight)
		                         : creature_mind::SleepTogetherRadius(friends.height, friends.friendHeight);
		return sit ? creature_mind::SitWithFriend(friends, *situation.clearArea, radius, random)
		           : creature_mind::SleepWithFriend(friends, *situation.clearArea, radius, random);
	}
	case Build::DrinkWithFriend:
		if (!situation.explorePlace.has_value() || !situation.exploreSea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::DrinkWithFriend(friends, *situation.explorePlace, *situation.exploreSea, random);
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
		if (!situation.friendsTown.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::TownWithFriend(friends, *situation.friendsTown,
		                                     executor.build == Build::HelpTownWithFriend ? creature_desires::Desire::Compassion
		                                                                                 : creature_desires::Desire::Anger);
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
	switch (executor.build)
	{
	case Build::CastLightning:
	case Build::CastHelpful:
	case Build::CastPlayful:
	case Build::CastFood:
	case Build::CastWater:
	case Build::CastWaterOnInstrument:
	case Build::CastWaterOnItself:
	case Build::HealItself:
	case Build::CastAggressive:
	case Build::CastExplosion:
	case Build::CastFireball:
	case Build::CastShield:
	case Build::CastToImpress:
	case Build::CastAmusing:
	case Build::CastForest:
		return true;
	default:
		return false;
	}
}

bool creature_plan_actions::MayTry(const Executor& executor, bool mayCast, bool onFire)
{
	if (!IsCast(executor))
	{
		return true;
	}
	return mayCast && (executor.build != Build::CastWaterOnItself || onFire);
}

bool creature_plan_actions::WalksByItsHeight(Build build)
{
	return build == Build::GoHomeToRecover || build == Build::GoHome || build == Build::RunHome ||
	       build == Build::EnterTemple || build == Build::PrayAtTemple || build == Build::Sacrifice ||
	       build == Build::StealAndPutByTemple || build == Build::EatFromStoragePit || build == Build::EatFromField ||
	       build == Build::Drink || build == Build::SmileAtFriend || build == Build::SleepAtHome || build == Build::WaveAt ||
	       build == Build::InspectCreature || build == Build::DanceOutsideTemple || build == Build::DanceAroundThing ||
	       build == Build::BringHome || build == Build::BringFoodHome || build == Build::TakeFieldFoodHome ||
	       build == Build::StealAndPutInTown || build == Build::StealFoodFromStore || build == Build::StealWoodFromStore;
}

bool creature_plan_actions::ApproachesByRadii(Build build)
{
	return build == Build::EatPortion || build == Build::EatFromTree;
}

bool creature_plan_actions::Possible(const Executor& executor, const Situation& situation)
{
	// What the action needs to be made is asked of a forced plan too; the tests that only weigh whether to choose it are
	// not, as the original asks its rows' tests of no forced plan but a stroke's do-it-again
	// Too young to leave home is one of those: a forced look out to sea or sit on the beach is carried out
	if (executor.leavesHome && !situation.forced && situation.stage < k_LeaveHomeStage)
	{
		return false;
	}
	switch (executor.build)
	{
	case Build::Drink:
		// The creature can always try to drink. When it finds no place to drink, the action fails as it starts.
		return true;
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
	case Build::PrayAtTemple:
	case Build::SleepAtHome:
		// Only with a temple, and not too far from home; a plan forced on it is not asked, but still needs a home
		return situation.home.has_value() &&
		       (situation.forced || (situation.homeDistance.has_value() &&
		                             creature_mind::NotTooFarFromHome(situation.hasTemple, *situation.homeDistance)));
	case Build::GoHome:
		return situation.home.has_value() && (situation.forced || (situation.homeDistance.has_value() &&
		                                                           creature_mind::FarFromHome(*situation.homeDistance)));
	case Build::GoHomeToRecover:
	case Build::RunHome:
	case Build::BringHome:
	case Build::BringFoodHome:
	case Build::TakeFieldFoodHome:
		return situation.home.has_value();
	case Build::Sleep:
		// On the spot only on a leash, with no temple, or away from home; a plan forced on it is not asked
		return situation.forced ||
		       creature_mind::MaySleepOnTheSpot(situation.leashed, situation.hasTemple, situation.homeDistance);
	case Build::SleepByObject:
		// The same, and with nothing frightening near the creature itself; a plan forced on it is not asked
		return situation.forced ||
		       (creature_mind::MaySleepOnTheSpot(situation.leashed, situation.hasTemple, situation.homeDistance) &&
		        situation.nothingFrighteningNear);
	case Build::RestToGetBetter:
	case Build::RestOnTheSpot:
		return situation.forced || situation.nothingFrighteningNear;
	case Build::LookAtHand:
		// Only while the player's hand moves
		return situation.forced || situation.handMoving;
	case Build::GoToHand:
		return situation.forced || situation.awayFromHand;
	case Build::GoToMiddleOfScreen:
		return situation.forced || creature_player_attention::AwayFromMiddleOfScreen(situation.hasPlayer, situation.lookedAt);
	case Build::FollowPlayer:
	case Build::ShowPlayerAnObject:
		return situation.forced || creature_player_attention::FarFromCamera(situation.lookedAt);
	case Build::BeCrossWithPlayer:
		return situation.forced || creature_player_attention::IsCross(situation.attitudeToPlayer);
	case Build::Sacrifice:
	case Build::StealAndPutByTemple:
		// Only a creature whose player has a temple: what the action is made from, so a forced plan needs it too
		return situation.hasTemple;
	// The joint actions with a friend that need a desire of the creature's own to be a little strong
	case Build::DrinkWithFriend:
		return situation.waterDesire > k_SlightDesire;
	case Build::PooWithFriend:
		return situation.pooDesire > k_SlightDesire;
	case Build::SleepWithFriend:
		return situation.tiredness > k_SlightDesire;
	case Build::BeHappyWithFriend:
		return situation.sadness < k_SlightDesire;
	// Helping the nearest town with a friend when it does not want to attack the town more than to help it
	case Build::HelpTownWithFriend:
		return situation.attackNearestTown.has_value() && !*situation.attackNearestTown;
	case Build::AttackTownWithFriend:
		return situation.attackNearestTown.has_value() && *situation.attackNearestTown;
	case Build::LookAbout:
		// The sun only when it is not night and the moon only at night; a plan forced on it is not asked
		if (executor.action == k_LookAtSun)
		{
			return situation.forced || !situation.visualNight;
		}
		if (executor.action == k_LookAtMoon)
		{
			return situation.forced || situation.visualNight;
		}
		return true;
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
	if (!Possible(executor, situation))
	{
		return std::nullopt;
	}
	// A shield for a plan about nothing idles instead and casts nothing
	if (executor.build == Build::CastShield && !situation.activity.has_value())
	{
		return creature_mind::BeIdle(random);
	}
	if ((executor.target != Target::None && !executor.thingUnused && !object.has_value()) ||
	    (IsCast(executor) && !cast.has_value()))
	{
		return std::nullopt;
	}
	switch (executor.build)
	{
	case Build::Eat:
		return creature_mind::Eat(*object);
	case Build::EatPortion:
		// With something it can eat in its hand the game's plan ends at once, as eating from a field or a tree does; ours
		// makes no agenda. Holding anything else, it goes all the same
		if (situation.holdingEatable)
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
		return creature_mind::SleepOnTheSpot(situation.clearArea, situation.radius, situation.home, situation.height, random);
	case Build::Poo:
		return creature_mind::Poo(random);
	case Build::Puke:
		return creature_mind::Puke();
	case Build::Drink:
		if (!situation.water.has_value())
		{
			return std::nullopt;
		}
		return creature_drink::Drink(*situation.water, situation.height);
	case Build::ExamineByPickingUp:
		return creature_mind::ExamineByPickingUp(*object, situation.handFull, random);
	case Build::ExamineByLooking:
		return creature_mind::LookAt(objectPoint, k_LookSeconds);
	case Build::ExamineByFollowing:
		return creature_mind::FollowFor(*object, k_FollowSeconds);
	case Build::ThrowAbout:
		return creature_mind::ThrowAbout(*object, random);
	case Build::Hurl:
		// With nothing to throw, it can't; holding anything, it throws what it holds
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::HurlAt(*situation.instrument, *object, objectPoint, situation.height,
		                             situation.handFull || situation.held.has_value(), random);
	case Build::GiveWoodToStore:
	{
		// Holding a tree, it gives that; with none to take either, it idles
		std::optional<creature_mind::TreeToTake> take;
		if (!situation.holdingTree)
		{
			if (!situation.instrument.has_value())
			{
				return creature_mind::BeIdle(random);
			}
			take = creature_mind::TreeToTake {
			    .tree = *situation.instrument, .point = situation.instrumentPoint, .radius = situation.instrumentRadius};
		}
		return creature_mind::GiveWoodToStore(take, *object, situation.radius, situation.height);
	}
	case Build::ThrowToy:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ThrowToyAt(*situation.instrument, *object, objectPoint, situation.height, random);
	case Build::PracticeThrow:
		// Holding something, it throws that
		return creature_mind::PracticeThrow(situation.handFull || situation.held.has_value() ? std::nullopt : object,
		                                    situation.position, situation.height);
	case Build::GetHigh:
		return creature_mind::GetHigh(situation.holdingMushroom ? std::nullopt : object, *object);
	case Build::HelpRepairHouse:
		// With no tree to mend it with, it can't; holding something, it puts that down by the house
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::HelpRepairHouse(situation.handFull || situation.held.has_value() ? std::nullopt
		                                                                                       : situation.instrument,
		                                      *object, *situation.instrument);
	case Build::ThrowToPlayer:
		// Without a hand of its player's to throw to, or a toy, it can't
		if (!situation.playerHand.has_value() || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ThrowToPlayer(*situation.instrument, *situation.playerHand,
		                                    situation.handFull || situation.held.has_value());
	case Build::StrokeToy:
		return creature_mind::StrokeToy(*object);
	case Build::TakeToyHome:
		// Without a home it can't
		if (!situation.home.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::TakeToyHome(*object, *situation.home, situation.height, random,
		                                  [&random] { return FineRandomFraction(random); });
	case Build::ThrowDie:
		return creature_mind::ThrowDie(situation.handFull || situation.held.has_value() ? std::nullopt : object, *object,
		                               situation.position, situation.height, random);
	case Build::BreakRock:
		return creature_mind::BreakRock(situation.handFull || situation.held.has_value() ? std::nullopt : object, *object,
		                                situation.position, situation.height);
	case Build::KickBallAround:
		return creature_mind::KickBallAround(*object, random);
	case Build::PlantTree:
		if (!situation.holdingTree && !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::PlantTreeBy(situation.holdingTree ? std::nullopt : situation.instrument, *object);
	case Build::Destroy:
		return creature_mind::DestroyThing(*object);
	case Build::Kick:
		return creature_mind::KickThing(*object);
	case Build::SitDown:
		// Under a script's control, told to sit at a thing, it goes and sits there. A plan done to the creature itself
		// sits on the spot
		if (situation.controlledByScript && object.has_value() && *object != situation.self &&
		    situation.drawnSeconds.has_value())
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
	case Build::LookAtHand:
		return creature_mind::LookAtHand();
	case Build::GoToHand:
		return creature_player_attention::GoToHand(random);
	case Build::PointAtHand:
		// Without a hand of its player, or a camera, the action fails
		if (!situation.playerHand.has_value() || !situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::PointAtHand(*situation.camera, *situation.playerHand);
	case Build::ShowHowNice:
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::ShowHowNice(*situation.camera, situation.attitudeToPlayer);
	case Build::PrayToPlayer:
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::PrayToPlayer(*situation.camera);
	case Build::ThrowAtCamera:
		// Without a camera of its player, or a thing to throw, the action fails
		if (!situation.camera.has_value() || !situation.hasPlayer || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		// Holding anything, it throws what it holds
		return creature_player_attention::ThrowAtCamera(
		    situation.handFull || situation.held.has_value() ? std::nullopt : situation.instrument, situation.eye);
	case Build::WatchPlayer:
		return creature_player_attention::WatchPlayer();
	case Build::GoToMiddleOfScreen:
	case Build::FollowPlayer:
	case Build::BeCrossWithPlayer:
	case Build::BeSillyWithPlayer:
		// The creature needs the camera to turn to it
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		switch (executor.build)
		{
		case Build::GoToMiddleOfScreen:
			return creature_player_attention::GoToMiddleOfScreen(*situation.camera, situation.height);
		case Build::FollowPlayer:
			return creature_player_attention::FollowPlayer(*situation.camera, situation.height);
		case Build::BeCrossWithPlayer:
			return creature_player_attention::BeCrossWithPlayer(*situation.camera, situation.height, random);
		default:
			return creature_player_attention::BeSillyWithPlayer(*situation.camera, random);
		}
	case Build::ShowPlayerAnObject:
		// Without a camera, or a thing to show, the action fails
		if (!situation.camera.has_value() || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		// Holding anything, it shows what it holds
		return creature_player_attention::ShowPlayerAnObject(
		    situation.handFull || situation.held.has_value() ? std::nullopt : situation.instrument, *situation.camera,
		    situation.height);
	case Build::InspectCreature:
		return creature_mind::InspectCreature(*object, situation.height);
	case Build::LookAtFlyingObject:
		return creature_mind::LookAtFlyingObject(*object, random);
	case Build::ApproachEmote:
		return creature_mind::ApproachAndEmote(*object, executor.animation);
	case Build::DanceWithVillagers:
		// With no clear place about the villager the action fails
		if (!situation.danceArea.has_value() || !object.has_value())
		{
			return std::nullopt;
		}
		return creature_dance::Agenda({.villager = *object,
		                               .place = *situation.danceArea,
		                               .type = executor.danceType,
		                               .fewestDancers = situation.fewestDancers,
		                               .beats = situation.danceBeats,
		                               .height = situation.height});
	case Build::PracticeDance:
		return creature_dance::PracticeDance();
	case Build::SmileAtFriend:
		return creature_mind::SmileAtFriend(*object, situation.height, random);
	case Build::WaveAtFriend:
		return creature_mind::WaveAtFriend(situation.self, *object);
	case Build::BeHappyWithFriend:
	case Build::PooWithFriend:
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	case Build::DrinkWithFriend:
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
		return FriendsAgenda(executor, *object, situation, random);
	case Build::HowlAtFriend:
		return creature_mind::HowlAtFriend(*object);
	case Build::PineForFriend:
		return creature_mind::PineForFriend(*object);
	case Build::LookForFriend:
		return creature_mind::LookForFriend(*object, situation.height);
	case Build::DanceOutsideTemple:
		// Without the heart of its player's temple, the action fails
		if (!situation.templeHeart.has_value())
		{
			return std::nullopt;
		}
		return creature_temple_dance::DanceOutsideTemple(situation.templeHeart->temple, situation.height);
	case Build::DanceAroundThing:
		return creature_temple_dance::DanceAroundThing(*object, situation.height);
	case Build::SleepAtHome:
		return creature_mind::SleepAtHome(*situation.home, situation.height, random);
	case Build::WaveAt:
		return creature_mind::WaveAt(*object, situation.height);
	case Build::BeFrightened:
		return creature_mind::BeFrightenedOnTheSpot();
	case Build::PutDown:
		// Setting down what it holds with the put-down, as when told to
		return creature_mind::PutDownHeld();
	case Build::DeadForever:
		return creature_mind::DeadForever();
	case Build::SleepByObject:
		return creature_mind::SleepByObject(situation.clearArea, situation.radius, situation.home, situation.height, random);
	case Build::HoldObject:
		return creature_mind::HoldObject({.thing = *object,
		                                  .handFull = situation.held.has_value(),
		                                  .camera = situation.camera,
		                                  .playsWithIt = situation.playsWithHeld,
		                                  .turnsPerSecond = situation.turnsPerSecond},
		                                 random);
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
	case Build::PointAtThing:
		return creature_mind::PointAtThing(*object);
	case Build::CastLightning:
		return creature_mind::CastAt(creature_mind::CastStyle::Lightning, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastHelpful:
		return creature_mind::CastAt(creature_mind::CastStyle::Helpful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastPlayful:
		return creature_mind::CastAt(creature_mind::CastStyle::Playful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastAmusing:
		return creature_mind::CastAmusingAt(cast->amusingSpells, *object, cast->height, random);
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
	case Build::CastAggressive:
		return creature_mind::CastAt(creature_mind::CastStyle::Aggressive, cast->magicType, cast->gesture, *object,
		                             cast->height, random);
	case Build::CastExplosion:
		return creature_mind::CastAt(creature_mind::CastStyle::Explosion, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastFireball:
		// With no clear area near it, the creature can't cast a fireball
		if (!cast->clearArea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastFireballAt(cast->magicType, cast->gesture, *object, cast->targetLiving, *cast->clearArea,
		                                     cast->height, random);
	case Build::CastShield:
		return creature_mind::CastShieldAt(cast->magicType, *object);
	case Build::CastToImpress:
		// With no miracle that impresses known, it goes through the motions and casts nothing
		return creature_mind::CastToImpress(cast->magicType != 0 ? std::optional(cast->magicType) : std::nullopt, *object,
		                                    cast->height, random);
	case Build::ShowLearntLesson:
		return creature_odd::ShowLearntLesson();
	case Build::LookConfused:
		return creature_odd::LookConfused(situation.camera);
	case Build::BehaveStrangely:
		return creature_odd::BehaveStrangely(situation.turnsPerSecond, random);
	case Build::CastForest:
		// It casts the forest at the place where the thing was as it made the agenda, and can't without that place
		if (!cast->place.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastForestAt(cast->magicType, *object, *cast->place, cast->height);
	case Build::Catch:
		return creature_mind::CatchThing(*object);
	case Build::Fight:
		return creature_mind::FightCreature(*object);
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
	case Build::RestToGetBetter:
		return creature_mind::RestToGetBetter();
	case Build::GoHomeToRecover:
		return creature_mind::GoHomeToRecover(*situation.home, situation.height);
	case Build::HealItself:
		// Cast at itself, where it stands
		return creature_mind::HealItself(static_cast<uint32_t>(MagicType::Heal), situation.self);
	case Build::RestOnTheSpot:
		return creature_mind::RestOnTheSpot(situation.self,
		                                    creature_mind::k_RestOnTheSpotSeconds +
		                                        (creature_mind::k_RestOnTheSpotExtraSeconds * RandomFraction(random)));
	case Build::GoHome:
		return creature_mind::GoHome(*situation.home, situation.height);
	case Build::RunHome:
		return creature_mind::RunHome(*situation.home, situation.height,
		                              random(creature_mind::k_FrightenedBeforeRunningHomeLots) == 0);
	case Build::EnterTemple:
	{
		// Without a home and the heart of its player's temple, the action fails
		if (!situation.home.has_value() || !situation.templeHeart.has_value())
		{
			return std::nullopt;
		}
		const auto& heart = *situation.templeHeart;
		return creature_mind::EnterTemple(*situation.home, heart.point,
		                                  creature_mind::TempleStop(situation.standing, heart.point, heart.radius),
		                                  situation.height);
	}
	case Build::PrayAtTemple:
		return creature_mind::PrayAtTemple(*situation.home, situation.height);
	case Build::LookAtTemple:
		return creature_mind::LookAtTemple(*object, objectPoint, random(creature_mind::k_PointAtTempleLots) == 0);
	case Build::GiveFieldFoodToStore:
	case Build::GiveFruitToStore:
	case Build::GiveMagicFoodToStore:
	case Build::GiveMagicWoodToStore:
	case Build::PutFieldFoodBySite:
	case Build::DropCowInStore:
	case Build::MakeBreeder:
	case Build::TakeVillagerHome:
	case Build::BringVillagerToSite:
	case Build::RaiseTotem:
	case Build::LowerTotem:
	case Build::FrightenVillagers:
	case Build::PlayWithVillager:
	case Build::KissVillager:
		return TownHelpAgenda(executor, object, situation, random);
	case Build::Sacrifice:
		// Without a worship site at its temple, the creature has nowhere to take the thing
		if (!situation.sacrificeAltar.has_value())
		{
			return std::nullopt;
		}
		return creature_sacrifice::Sacrifice(situation.holdingLiving ? std::nullopt : object, *situation.sacrificeAltar,
		                                     situation.height);
	case Build::StealAndPutByTemple:
		if (!situation.home.has_value())
		{
			return std::nullopt;
		}
		// It picks up the thing only when its hand is empty
		return creature_steal::StealAndPutByTemple(situation.handFull ? std::nullopt : object, *situation.home,
		                                           situation.height);
	case Build::StealAndPutInTown:
		// Without a storage pit in a town of its player, the theft fails
		if (!situation.ownTownPit.has_value())
		{
			return std::nullopt;
		}
		// It picks up the thing only when its hand is empty
		return creature_town_theft::StealAndPutInTown(situation.handFull ? std::nullopt : object, *situation.ownTownPit,
		                                              situation.height);
	case Build::StealFoodFromStore:
	case Build::StealWoodFromStore:
		// The store it steals from is the thing it uses; the thing it acts on is its own store
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_town_theft::StealFromStore(*situation.instrument, situation.instrumentPoint, *object,
		                                           executor.build == Build::StealWoodFromStore, situation.height);
	case Build::BringHome:
		// It picks up the thing only when its hand is empty; with a full hand it brings home what it holds
		return creature_bring_home::BringHome(situation.handFull ? std::nullopt : std::optional(*object), *situation.home,
		                                      situation.height);
	case Build::BringFoodHome:
		// It picks up the food unless it holds something it can eat already
		return creature_bring_home::BringFoodHome(situation.holdingEatable ? std::nullopt : std::optional(*object),
		                                          *situation.home, situation.height);
	case Build::TakeFieldFoodHome:
	{
		// It takes wheat from the field unless it holds something it can eat already
		std::optional<creature_bring_home::Field> take;
		if (!situation.holdingEatable)
		{
			take = creature_bring_home::Field {.field = *object, .point = objectPoint};
		}
		return creature_bring_home::TakeFoodFromFieldHome(take, *situation.home, situation.height);
	}
	}
	return std::nullopt;
}
