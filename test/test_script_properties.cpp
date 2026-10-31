/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::script::property_rules;

namespace
{
constexpr auto k_Thing = static_cast<entt::entity>(7);
constexpr auto k_Other = static_cast<entt::entity>(9);
} // namespace

TEST(ScriptProperties, ATownIsDestroyedWhenNoneOfItsBuildingsStands)
{
	EXPECT_TRUE(TownCompletelyDestroyed({}));
	const std::array standing {TownBuilding {.life = 0.5f, .field = false, .built = 1.0f}};
	EXPECT_FALSE(TownCompletelyDestroyed(standing));
	// Dead, a field, or no more than a tenth built
	const std::array fallen {TownBuilding {.life = 0.0f, .field = false, .built = 1.0f},
	                         TownBuilding {.life = 1.0f, .field = true, .built = 1.0f},
	                         TownBuilding {.life = 1.0f, .field = false, .built = 0.1f}};
	EXPECT_TRUE(TownCompletelyDestroyed(fallen));
	const std::array begun {TownBuilding {.life = 1.0f, .field = false, .built = 0.11f}};
	EXPECT_FALSE(TownCompletelyDestroyed(begun));
}

TEST(ScriptProperties, AThingIsValidWhileReachableAndATownWhileNotWhollyDestroyed)
{
	EXPECT_TRUE(ThingValid(true, std::nullopt));
	EXPECT_FALSE(ThingValid(false, std::nullopt));
	// a town: only while one of its buildings stands
	EXPECT_TRUE(ThingValid(true, false));
	EXPECT_FALSE(ThingValid(true, true));
	EXPECT_FALSE(ThingValid(false, false));
}

TEST(ScriptProperties, ANativeReachesAThingThatIsThereNotBeingDeletedAndNoDyingVillager)
{
	// The one look-up every native given a thing makes: what is gone, what is being deleted and a villager on its way to
	// dying are as no thing (docs/wiki/scripts/script-commands.md#the-thing-a-native-is-given)
	using ecs::script_held::Reachable;
	EXPECT_TRUE(Reachable(true, false, false));
	EXPECT_FALSE(Reachable(false, false, false));
	EXPECT_FALSE(Reachable(true, true, false));
	EXPECT_FALSE(Reachable(true, false, true));
}

TEST(ScriptProperties, OnlyAVillagerGoingToItsDyingStateIsOutOfTheScriptsReach)
{
	// the state table answers no state as final, so a villager's last state is the one it is going to
	const auto info = std::make_unique<InfoConstants>();
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto plain = registry.Create();
	const auto going = registry.Create();
	registry.Assign<ecs::components::Unavailable>(going);
	const auto gone = registry.Create();
	registry.Destroy(gone);
	const auto villager = [&registry](VillagerStates goingTo) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Villager>(entity);
		auto& action = registry.Assign<ecs::components::LivingAction>(entity, VillagerStates::MoveToPos, uint16_t {0});
		action.states[static_cast<size_t>(ecs::components::LivingAction::Index::Final)] = static_cast<uint8_t>(goingTo);
		return entity;
	};
	const auto dying = villager(VillagerStates::Dying);
	const auto settingDying = villager(VillagerStates::SetDying);
	EXPECT_TRUE(ecs::script_held::IsReachable(plain));
	EXPECT_FALSE(ecs::script_held::IsReachable(going));
	EXPECT_FALSE(ecs::script_held::IsReachable(gone));
	EXPECT_FALSE(ecs::script_held::IsReachable(entt::null));
	EXPECT_FALSE(ecs::script_held::IsReachable(dying));
	// only the dying state itself: one about to start dying is still reached
	EXPECT_TRUE(ecs::script_held::IsReachable(settingDying));
	Locator::entitiesRegistry::reset();
	Locator::infoConstants::reset();
}

TEST(ScriptProperties, PlayersAreNumberedFromOne)
{
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_ONE, false), 1.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_THREE, false), 3.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::NEUTRAL, false), 0.0f);
	EXPECT_EQ(PlayerProperty(std::nullopt, false), 0.0f);
	// A wholly destroyed town answers 1 whoever it belonged to
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_TWO, true), 1.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::NEUTRAL, true), 1.0f);
}

TEST(ScriptProperties, ACreatureHasInItsHandWhatItCarriesOrEats)
{
	EXPECT_TRUE(InCreatureHand(k_Thing, k_Thing, std::nullopt));
	EXPECT_TRUE(InCreatureHand(k_Thing, entt::null, k_Thing));
	EXPECT_FALSE(InCreatureHand(k_Thing, k_Other, k_Other));
	EXPECT_FALSE(InCreatureHand(k_Thing, entt::null, std::nullopt));
	EXPECT_FALSE(InCreatureHand(entt::null, entt::null, std::nullopt));
}

TEST(ScriptProperties, AThingIsInHandOnlyWhileThePlayersHandHoldsIt)
{
	// a number, 1 or 0, not a yes or no
	EXPECT_EQ(InHandProperty(k_Thing, k_Thing), 1.0f);
	EXPECT_EQ(InHandProperty(k_Thing, k_Other), 0.0f);
	EXPECT_EQ(InHandProperty(k_Thing, std::nullopt), 0.0f);
	// nothing is never held
	EXPECT_EQ(InHandProperty(entt::null, std::nullopt), 0.0f);
}

TEST(ScriptProperties, TheInHandGrabTypeIsOneWhileTheThingIsInTheHandsLockedSelect)
{
	// a number, 1 or 0, as the in-hand property
	EXPECT_EQ(InHandGrabTypeProperty(true), 1.0f);
	EXPECT_EQ(InHandGrabTypeProperty(false), 0.0f);
}

TEST(ScriptProperties, InCreatureHandGivesFalseWithALineWhenItCannotAsk)
{
	// Either thing missing
	const auto missing = InCreatureHandNative(false, false, k_Thing, k_Thing, std::nullopt);
	EXPECT_FALSE(missing.inHand);
	EXPECT_EQ(missing.message, "Invalid creature or thing");
	// Not a creature: the false the original gives before it would crash, whatever is held
	const auto notCreature = InCreatureHandNative(true, false, k_Thing, k_Thing, k_Thing);
	EXPECT_FALSE(notCreature.inHand);
	EXPECT_EQ(notCreature.message, "Not a creature");
}

TEST(ScriptProperties, InCreatureHandAsksACreatureWhatItHoldsOrEats)
{
	const auto held = InCreatureHandNative(true, true, k_Thing, k_Thing, std::nullopt);
	EXPECT_TRUE(held.inHand);
	EXPECT_TRUE(held.message.empty());
	EXPECT_TRUE(InCreatureHandNative(true, true, k_Thing, entt::null, k_Thing).inHand);
	const auto other = InCreatureHandNative(true, true, k_Thing, k_Other, std::nullopt);
	EXPECT_FALSE(other.inHand);
	EXPECT_TRUE(other.message.empty());
}

TEST(ScriptProperties, WarmthAndEnergyAreKeptInTheirRanges)
{
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, -3.0f), -1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, 2.0f), 1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, -0.5f), -0.5f);
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, -0.5f), 0.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, 1.5f), 1.0f);
	// Not a number becomes the top of the range
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, std::numeric_limits<float>::quiet_NaN()), 1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Exhaustion, 3.0f), 3.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Poo, -2.0f), -2.0f);
}

TEST(ScriptProperties, PropertyNumbersMatchTheScripts)
{
	using script::ObjectPropertyType;
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::InHand), 9);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::Speed), 11);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::Player), 21);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::BuiltPercentage), 22);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::YPos), 24);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureWarmth), 26);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureExhaustion), 31);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureDehydration), 32);
}

TEST(ScriptProperties, NativesTakeAndGiveWhatTheGameDoes)
{
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	struct Expected
	{
		size_t index;
		const char* name;
		int32_t in;
		uint32_t out;
	};
	// Each native's place in the table, and how many values it takes from the stack and gives back
	for (const auto& [index, name, in, out] :
	     {Expected {11, "GAME_THING_FIELD_OF_VIEW", 1, 1}, Expected {12, "POS_FIELD_OF_VIEW", 3, 1},
	      Expected {21, "GET_PROPERTY", 2, 1}, Expected {22, "SET_PROPERTY", 3, 0}, Expected {26, "CALL", 6, 1},
	      Expected {51, "CALL_NEAR", 7, 1}, Expected {76, "IN_CREATURE_HAND", 2, 1}})
	{
		ASSERT_LT(index, table.size());
		EXPECT_EQ(table[index].name, std::string(name));
		EXPECT_EQ(table[index].stackIn, in) << name;
		EXPECT_EQ(table[index].stackOut, out) << name;
	}
}

TEST(ScriptProperties, AnglesAreGivenAndTakenInDegrees)
{
	EXPECT_NEAR(AngleToScript(glm::pi<float>()), 180.0f, 1e-3f);
	EXPECT_NEAR(AngleFromScript(90.0f), glm::half_pi<float>(), 1e-6f);
	EXPECT_NEAR(AngleToScript(AngleFromScript(-45.0f)), -45.0f, 1e-3f);
}

TEST(ScriptProperties, AnglesAreTurnedAsTheOriginalsFloats)
{
	// the original's own numbers: two float products to degrees, one from them, and no wrapping either way
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(1.0f)), 0x42652EE0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(7.0f)), 0x43C88905u);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(0.017453292f)), 0x3F7FFFFFu);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(glm::pi<float>())), 0x43340000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleFromScript(160.0f)), 0x4032B8C2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleFromScript(-180.0f)), 0xC0490FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleFromScript(45.0f)), 0x3F490FDBu);
}

TEST(ScriptProperties, AnUprightThingKeepsItsWholeTurn)
{
	for (const float y : {0.3f, 2.5f, -2.9f})
	{
		const auto angles = PlacedAngles(PlacedRotation({.x = 0.0f, .y = y, .z = 0.0f}));
		EXPECT_NEAR(angles.y, y, 1e-5f);
		EXPECT_FLOAT_EQ(angles.x, 0.0f);
		EXPECT_FLOAT_EQ(angles.z, 0.0f);
	}
}

TEST(ScriptProperties, ALeaningThingKeepsItsLeans)
{
	const Angles placed {.x = 0.2f, .y = 0.7f, .z = -0.4f};
	const auto angles = PlacedAngles(PlacedRotation(placed));
	EXPECT_NEAR(angles.x, placed.x, 1e-5f);
	EXPECT_NEAR(angles.y, placed.y, 1e-5f);
	EXPECT_NEAR(angles.z, placed.z, 1e-5f);
}

TEST(ScriptProperties, NearlyNoLifeIsKeptFromProtectedThings)
{
	EXPECT_TRUE(CanSetLife(0.0f, false, false));
	EXPECT_FALSE(CanSetLife(0.0f, true, false));
	EXPECT_FALSE(CanSetLife(0.01f, false, true));
	// More than a hundredth may always be set
	EXPECT_TRUE(CanSetLife(0.02f, true, true));
}

TEST(ScriptProperties, ACreatureStandsFifteenTimesItsSize)
{
	EXPECT_FLOAT_EQ(CreatureHeight(1.0f), 15.0f);
	EXPECT_FLOAT_EQ(CreatureHeight(0.5f), 7.5f);
	EXPECT_NEAR(CreatureSizeForHeight(30.0f), 2.0f, 1e-6f);
}

TEST(ScriptProperties, ACreaturesSpeedIsAFractionOfItsTopSpeedFromNoneToAll)
{
	EXPECT_EQ(CreatureSpeedFraction(0.5f), 0.5f);
	// the guide's 40.5 and the storm scene's 8 are all of it
	EXPECT_EQ(CreatureSpeedFraction(40.5f), 1.0f);
	EXPECT_EQ(CreatureSpeedFraction(8.0f), 1.0f);
	EXPECT_EQ(CreatureSpeedFraction(1.0f), 1.0f);
	EXPECT_EQ(CreatureSpeedFraction(std::numeric_limits<float>::infinity()), 1.0f);
	// none: below 0, and a value that is not a number; -0 gives +0
	EXPECT_EQ(CreatureSpeedFraction(-1.0f), 0.0f);
	EXPECT_EQ(CreatureSpeedFraction(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(CreatureSpeedFraction(-0.0f)), 0u);
}

TEST(ScriptProperties, BeliefForAPlayer)
{
	// A town answers its belief, none when it was given none
	EXPECT_FLOAT_EQ(BeliefForPlayer(true, 0.5f, PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE), 0.5f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(true, std::nullopt, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE), 0.0f);
	// Anything else believes wholly in its own player
	EXPECT_FLOAT_EQ(BeliefForPlayer(false, std::nullopt, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE), 1.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(false, std::nullopt, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(false, std::nullopt, std::nullopt, PlayerNames::PLAYER_ONE), 0.0f);
}

TEST(ScriptProperties, AnyThingThatIsNotATownBelievesInThePlayerItBelongsTo)
{
	// The original answers every kind that is not a town through its own player: a rock or a dead tree the one it was
	// made for, anything of no player of its own the neutral player, a living tree none
	// (docs/wiki/scripts/script-commands.md#belief_for_player-247)
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto rock = registry.Create();
	registry.Assign<ecs::components::MobileStatic>(rock, MobileStaticInfo {}, PlayerNames::PLAYER_TWO);
	const auto dead = registry.Create();
	registry.Assign<ecs::components::DeadTree>(dead, ecs::components::DeadTree {.owner = PlayerNames::PLAYER_THREE});
	const auto tree = registry.Create();
	registry.Assign<ecs::components::Tree>(tree);
	const auto plain = registry.Create();
	EXPECT_EQ(chlapi::BeliefOwnerOf(rock), std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(chlapi::BeliefOwnerOf(dead), std::optional(PlayerNames::PLAYER_THREE));
	EXPECT_EQ(chlapi::BeliefOwnerOf(tree), std::nullopt);
	EXPECT_EQ(chlapi::BeliefOwnerOf(plain), std::optional(PlayerNames::NEUTRAL));
	EXPECT_FLOAT_EQ(BeliefForPlayer(false, std::nullopt, chlapi::BeliefOwnerOf(rock), PlayerNames::PLAYER_TWO), 1.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(false, std::nullopt, chlapi::BeliefOwnerOf(tree), PlayerNames::NEUTRAL), 0.0f);
	Locator::entitiesRegistry::reset();
}

TEST(ScriptProperties, ACreaturesHeadingIsTheWayItFacesAcrossTheLand)
{
	// A creature with a heading walks towards (-sin, -cos); a living thing at an angle faces (cos, sin)
	for (const float heading : {0.0f, 0.5f, 1.5707964f, 2.0f, 3.1415927f, -1.0f, 5.5f})
	{
		const float angle = CreatureHeadingToLivingAngle(heading);
		EXPECT_GE(angle, 0.0f);
		EXPECT_LT(angle, 2.0f * glm::pi<float>());
		EXPECT_NEAR(std::cos(angle), -std::sin(heading), 1e-5f);
		EXPECT_NEAR(std::sin(angle), -std::cos(heading), 1e-5f);
		const float back = LivingAngleToCreatureHeading(angle);
		EXPECT_NEAR(std::sin(back), std::sin(heading), 1e-5f);
		EXPECT_NEAR(std::cos(back), std::cos(heading), 1e-5f);
	}
	// Facing +x is no angle at all
	EXPECT_NEAR(CreatureHeadingToLivingAngle(-glm::half_pi<float>()), 0.0f, 1e-6f);
}

TEST(ScriptProperties, MovingIsAcrossTheLandInWholeMapUnits)
{
	EXPECT_FALSE(MovedAcross({10.0f, 0.0f, 20.0f}, {10.0f, 5.0f, 20.0f}));
	EXPECT_TRUE(MovedAcross({10.0f, 0.0f, 20.0f}, {10.01f, 0.0f, 20.0f}));
	EXPECT_TRUE(MovedAcross({10.0f, 0.0f, 20.0f}, {10.0f, 0.0f, 19.99f}));
	// Less than a map unit (a 65536th of ten metres) is no move
	EXPECT_FALSE(MovedAcross({10.0f, 0.0f, 20.0f}, {10.00001f, 0.0f, 20.0f}));
}

TEST(ScriptProperties, ScriptsNumberTheSpeciesFromTheGiantApe)
{
	EXPECT_EQ(CreatureTypeFromScript(0), CreatureType::GiantApe);
	EXPECT_EQ(CreatureTypeFromScript(1), CreatureType::Cow);
	EXPECT_EQ(CreatureTypeFromScript(2), CreatureType::Tiger);
	EXPECT_EQ(CreatureTypeFromScript(16), CreatureType::Gorilla);
	EXPECT_FALSE(CreatureTypeFromScript(17).has_value());
	EXPECT_EQ(ScriptCreatureType(CreatureType::GiantApe), 0u);
	EXPECT_EQ(ScriptCreatureType(CreatureType::Lion), 5u);
	for (uint32_t type = 0; type < 17; ++type)
	{
		EXPECT_EQ(ScriptCreatureType(*CreatureTypeFromScript(type)), type);
	}
}

TEST(ScriptProperties, TheOpposingCreatureIsTheTablesRowForThePlayersSpeciesAndColumnForTheGod)
{
	// A row for each species of the player's creature, as the scripts number them (the giant ape first), and a column
	// for each of the three gods
	std::array<GScriptOpposingCreature, 17> table {};
	table.at(0) = {.field0x0 = 7, .field0x4 = 8, .field0x8 = 9};
	table.at(5) = {.field0x0 = 1, .field0x4 = 2, .field0x8 = 16};
	EXPECT_EQ(OpposingCreatureType(table, CreatureType::GiantApe, 0), 7u);
	EXPECT_EQ(OpposingCreatureType(table, CreatureType::GiantApe, 2), 9u);
	EXPECT_EQ(OpposingCreatureType(table, CreatureType::Lion, 0), 1u);
	EXPECT_EQ(OpposingCreatureType(table, CreatureType::Lion, 1), 2u);
	EXPECT_EQ(OpposingCreatureType(table, CreatureType::Lion, 2), 16u);
	// No column for a god before the first or past the third, and no row past the table's
	EXPECT_FALSE(OpposingCreatureType(table, CreatureType::Lion, 3).has_value());
	EXPECT_FALSE(OpposingCreatureType(table, CreatureType::Lion, -1).has_value());
	EXPECT_FALSE(OpposingCreatureType(std::span(table).first(5), CreatureType::Lion, 0).has_value());
}

TEST(ScriptProperties, AThingFacesAPointByTheWayAcrossTheGroundFromTheXAxis)
{
	constexpr float k_Quarter = k_WholeTurnRadians / 4.0f;
	EXPECT_FLOAT_EQ(FacingAngle({10.0f, 10.0f}, {20.0f, 10.0f}), 0.0f);
	EXPECT_FLOAT_EQ(FacingAngle({10.0f, 10.0f}, {10.0f, 20.0f}), k_Quarter);
	EXPECT_FLOAT_EQ(FacingAngle({10.0f, 10.0f}, {0.0f, 10.0f}), 2.0f * k_Quarter);
	// The ways below the x axis are counted on round the turn, never below 0
	EXPECT_FLOAT_EQ(FacingAngle({10.0f, 10.0f}, {10.0f, 0.0f}), 3.0f * k_Quarter);
	EXPECT_GE(FacingAngle({0.0f, 0.0f}, {1.0f, -0.0001f}), 0.0f);
	EXPECT_LT(FacingAngle({0.0f, 0.0f}, {1.0f, -0.0001f}), k_WholeTurnRadians);
}

TEST(ScriptProperties, HealthIsAnObjectsOrAPilesLifeAndNoneForAThingThatIsNotAnObject)
{
	EXPECT_EQ(HealthProperty(HealthKind::Object, 0.37f), 0.37f);
	EXPECT_EQ(HealthProperty(HealthKind::Object, 0.0f), 0.0f);
	// A town is no object: the game reports it and the script reads 0, whatever life is passed
	EXPECT_EQ(HealthProperty(HealthKind::NotAnObject, 0.5f), 0.0f);
	EXPECT_EQ(HealthProperty(HealthKind::Pile, 0.25f), 0.25f);
}

TEST(ScriptProperties, HealthReadsContainersDancesMistsSpellsAndRingsAsNoObject)
{
	EXPECT_EQ(HealthKindOf({}), HealthKind::Object);
	EXPECT_EQ(HealthKindOf({.container = true}), HealthKind::NotAnObject);
	EXPECT_EQ(HealthKindOf({.dance = true}), HealthKind::NotAnObject);
	EXPECT_EQ(HealthKindOf({.mist = true}), HealthKind::NotAnObject);
	EXPECT_EQ(HealthKindOf({.spell = true}), HealthKind::NotAnObject);
	EXPECT_EQ(HealthKindOf({.influenceRing = true}), HealthKind::NotAnObject);
	// a forest is a container, a big forest an object
	EXPECT_EQ(HealthKindOf({.forest = true}), HealthKind::NotAnObject);
	EXPECT_EQ(HealthKindOf({.forest = true, .bigForest = true}), HealthKind::Object);
	// a pile of food or wood is a pile; a pile that is also no object is no object
	EXPECT_EQ(HealthKindOf({.pileOfFoodOrWood = true}), HealthKind::Pile);
	EXPECT_EQ(HealthKindOf({.container = true, .pileOfFoodOrWood = true}), HealthKind::NotAnObject);
}

TEST(ScriptProperties, APilesLifeIsItsAmountOverItsMostDividedOnce)
{
	EXPECT_EQ(PileLife(0, 500), 0.0f);
	EXPECT_EQ(PileLife(250, 500), 0.5f);
	EXPECT_EQ(PileLife(500, 500), 1.0f);
	// A pile over its most (the hand's piles are not capped) reads above 1
	EXPECT_EQ(PileLife(750, 500), 1.5f);
	// The quotient correctly rounded to a float: 1/3 is 0x3EAAAAAB, 2/3 is 0x3F2AAAAB
	EXPECT_EQ(std::bit_cast<uint32_t>(PileLife(1, 3)), 0x3EAAAAABu);
	EXPECT_EQ(std::bit_cast<uint32_t>(PileLife(2, 3)), 0x3F2AAAABu);
}

TEST(ScriptProperties, StrengthIsAskedOfACreatureThenAnOrbAHeldMiracleAnEffectAndACastMiracle)
{
	EXPECT_EQ(StrengthKindOf({}), StrengthKind::None);
	EXPECT_EQ(StrengthKindOf({.creature = true}), StrengthKind::Creature);
	EXPECT_EQ(StrengthKindOf({.oneShotSeed = true}), StrengthKind::OneShotSeed);
	EXPECT_EQ(StrengthKindOf({.spellSeed = true}), StrengthKind::SpellSeed);
	EXPECT_EQ(StrengthKindOf({.spotVisual = true}), StrengthKind::SpotVisual);
	EXPECT_EQ(StrengthKindOf({.spell = true}), StrengthKind::Spell);
	// the first that holds wins
	EXPECT_EQ(StrengthKindOf({.creature = true, .oneShotSeed = true, .spellSeed = true, .spotVisual = true, .spell = true}),
	          StrengthKind::Creature);
	EXPECT_EQ(StrengthKindOf({.oneShotSeed = true, .spellSeed = true, .spotVisual = true, .spell = true}),
	          StrengthKind::OneShotSeed);
	EXPECT_EQ(StrengthKindOf({.spellSeed = true, .spotVisual = true, .spell = true}), StrengthKind::SpellSeed);
	EXPECT_EQ(StrengthKindOf({.spotVisual = true, .spell = true}), StrengthKind::SpotVisual);
}

TEST(ScriptProperties, AThingWithNoStrengthReadsItsLifeAndOneWhenItIsNoObject)
{
	EXPECT_EQ(LifeInPlaceOfStrength(HealthKind::Object, 0.37f), 0.37f);
	EXPECT_EQ(LifeInPlaceOfStrength(HealthKind::Pile, 0.25f), 0.25f);
	// a forest, a mist or an influence ring has the life every thing that is no object has
	EXPECT_EQ(LifeInPlaceOfStrength(HealthKind::NotAnObject, 0.0f), 1.0f);
}
