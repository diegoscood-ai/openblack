/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A town's records of the creatures its people met (ecs::town_creature_attitude): one record each, the newest first,
// made with the attitude of a new one; and the creature's plans as the town judges them (creature_planner::IsValid).
// Pure rules, no game data.

#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Town.h"
#include "ECS/Town/TownCreatureAttitude.h"

using namespace openblack;
namespace attitude = openblack::ecs::town_creature_attitude;
using openblack::ecs::components::TownCreatureRecord;

TEST(TownCreatureAttitude, ACreatureMetHasOneRecordTheNewestFirst)
{
	const auto first = static_cast<entt::entity>(7);
	const auto second = static_cast<entt::entity>(9);
	std::vector<TownCreatureRecord> records;
	// no record: the attitude of a new one
	EXPECT_EQ(attitude::AttitudeOf(records, first), attitude::k_New);
	attitude::CreateRecord(records, first, 40);
	ASSERT_EQ(records.size(), 1u);
	EXPECT_EQ(records[0].creature, first);
	EXPECT_EQ(records[0].attitude, attitude::k_New);
	EXPECT_EQ(records[0].setTurn, 40u);
	EXPECT_EQ(records[0].resetTurn, 40u);
	// a second creature goes in front
	attitude::CreateRecord(records, second, 41);
	ASSERT_EQ(records.size(), 2u);
	EXPECT_EQ(records[0].creature, second);
	// meeting one again keeps its record as it is
	records[1].attitude = attitude::k_Fear;
	attitude::CreateRecord(records, first, 50);
	ASSERT_EQ(records.size(), 2u);
	EXPECT_EQ(records[1].setTurn, 40u);
	EXPECT_EQ(attitude::AttitudeOf(records, first), attitude::k_Fear);
	EXPECT_EQ(attitude::AttitudeOf(records, second), attitude::k_New);
	// no town: nothing to read
	EXPECT_EQ(attitude::AttitudeOf(entt::entity {entt::null}, first), attitude::k_New);
}

TEST(TownCreatureAttitude, APlanIsValidWithAnActionAndWhatItNeeds)
{
	using creature_desires::Desire;
	creature_planner::Plan plan {.desire = Desire::Anger, .action = 15};
	EXPECT_TRUE(creature_planner::IsValid(plan, true, false));
	// no action
	plan.action = 0;
	EXPECT_FALSE(creature_planner::IsValid(plan, false, false));
	plan.action = 15;
	// compassion looks for a town to help
	plan.desire = Desire::Compassion;
	EXPECT_FALSE(creature_planner::IsValid(plan, true, false));
	EXPECT_TRUE(creature_planner::IsValid(plan, false, false));
	plan.activityObject = 3;
	EXPECT_TRUE(creature_planner::IsValid(plan, true, false));
	// an action done with something needs it
	EXPECT_FALSE(creature_planner::IsValid(plan, true, true));
	plan.instrument = 4;
	EXPECT_TRUE(creature_planner::IsValid(plan, true, true));
}

// The game's action rows, by how a town takes them: one test per family, and the attitude each gives
namespace
{
using creature_mind_tables::TownAttitudeFamily;
using creature_mind_tables::TownAttitudeFamilyOf;
} // namespace

TEST(TownCreatureAttitude, MostActionsAreNothingToATown)
{
	// Undefined, MoveToPos, ExamineByPickingUp, RunAwayFromObject, ThrowDie
	for (const uint32_t action : {0u, 1u, 10u, 16u, 324u})
	{
		EXPECT_EQ(TownAttitudeFamilyOf(action), TownAttitudeFamily::None) << action;
	}
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::None, .actsOnVillager = true}), attitude::k_None);
}

TEST(TownCreatureAttitude, ATownFearsTheHurlsTheStompsTheKicksAndTheAggressiveMiracles)
{
	// Hurl, Stomp, CastLightningBolt, CastFireball, CastTornado, Kick, ThrowToImpress, SetFireToObject,
	// PlayfullyFrightenVillagers
	for (const uint32_t action : {15u, 18u, 45u, 46u, 103u, 236u, 252u, 283u, 296u})
	{
		EXPECT_EQ(TownAttitudeFamilyOf(action), TownAttitudeFamily::Fear) << action;
	}
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::Fear}), attitude::k_Fear);
}

TEST(TownCreatureAttitude, ATownRespectsTheGiftsTheBuildingTheKindMiraclesAndTheGames)
{
	// GiveFoodFromFieldToStoragePit, HelpBuildHouse, PutOutFire, Stroke, CastHealSpell, GiveWoodFromTreeToWorkshop,
	// TellVillagersAStory, the last row (watering a tree for the town)
	for (const uint32_t action : {28u, 34u, 37u, 38u, 101u, 262u, 295u, 327u})
	{
		EXPECT_EQ(TownAttitudeFamilyOf(action), TownAttitudeFamily::Respect) << action;
	}
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::Respect}), attitude::k_Respect);
}

TEST(TownCreatureAttitude, ATownFearsACreatureThatEatsItsPeopleAndNothingElseItEats)
{
	// EatAlive, EatAfterExamining, StompAndEat, StoneAndEat, and the four actions on a thing in its hand
	for (const uint32_t action : {11u, 12u, 13u, 14u, 275u, 276u, 277u, 278u})
	{
		EXPECT_EQ(TownAttitudeFamilyOf(action), TownAttitudeFamily::Eating) << action;
	}
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::Eating, .actsOnVillager = true}), attitude::k_Fear);
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::Eating, .actsOnVillager = false}), attitude::k_None);
}

TEST(TownCreatureAttitude, AHungryCreatureHoldingTheVillagerItActsOnIsFearedWhateverItDid)
{
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::Respect, .actsOnVillager = true, .hungryHoldingIt = true}),
	          attitude::k_Fear);
	EXPECT_EQ(attitude::NewAttitude({.family = TownAttitudeFamily::None, .hungryHoldingIt = true}), attitude::k_Fear);
}

TEST(TownCreatureAttitude, FearIsStampedEachTurnItIsSeenRespectOnlyWhenItStarts)
{
	TownCreatureRecord record {.creature = static_cast<entt::entity>(7), .attitude = attitude::k_New};
	// a change is stored, stamped and told
	EXPECT_TRUE(attitude::Store(record, attitude::k_Respect, 100));
	EXPECT_EQ(record.attitude, attitude::k_Respect);
	EXPECT_EQ(record.setTurn, 100u);
	// respect seen again: kept from when it started, not told again
	EXPECT_FALSE(attitude::Store(record, attitude::k_Respect, 105));
	EXPECT_EQ(record.setTurn, 100u);
	// fear: told once, stamped every turn it is seen
	EXPECT_TRUE(attitude::Store(record, attitude::k_Fear, 110));
	EXPECT_FALSE(attitude::Store(record, attitude::k_Fear, 120));
	EXPECT_EQ(record.attitude, attitude::k_Fear);
	EXPECT_EQ(record.setTurn, 120u);
}

TEST(TownCreatureAttitude, EachAttitudeFallsBackToNoneAfterItsWholeSeconds)
{
	// at a tenth of a second a turn: a new record after 160 turns, fear after 310, respect after 110
	for (const auto [kind, turns] :
	     {std::pair {attitude::k_New, 160u}, std::pair {attitude::k_Fear, 310u}, std::pair {attitude::k_Respect, 110u}})
	{
		TownCreatureRecord record {.creature = static_cast<entt::entity>(7), .attitude = kind, .setTurn = 1000};
		attitude::TimeOut(record, 1000 + turns - 1, 100);
		EXPECT_EQ(record.attitude, kind) << kind;
		attitude::TimeOut(record, 1000 + turns, 100);
		EXPECT_EQ(record.attitude, attitude::k_None) << kind;
		EXPECT_EQ(record.resetTurn, 1000 + turns) << kind;
		EXPECT_EQ(record.setTurn, 1000u) << kind;
	}
	// none stays none
	TownCreatureRecord none {.attitude = attitude::k_None, .setTurn = 0};
	attitude::TimeOut(none, 100000, 100);
	EXPECT_EQ(none.resetTurn, 0u);
}
