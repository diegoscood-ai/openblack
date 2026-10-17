/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "Animals/AnimalRules.h"
#include "Animals/BirdRules.h"
#include "ECS/WalkingList.h"

using namespace openblack;

TEST(BirdRules, ADeadBodysSmokeRisesHalfItsHeightOutTheWayItFaces)
{
	const glm::vec3 lying {10.0f, 2.0f, 20.0f};
	const auto east = animals::DeadBodySmokePlace(lying, 0.0f, 0.4f);
	EXPECT_FLOAT_EQ(east.x, 10.2f);
	EXPECT_FLOAT_EQ(east.y, 2.0f);
	EXPECT_FLOAT_EQ(east.z, 20.0f);
	const auto south = animals::DeadBodySmokePlace(lying, std::numbers::pi_v<float> * 0.5f, 0.4f);
	EXPECT_NEAR(south.x, 10.0f, 1e-5f);
	EXPECT_NEAR(south.z, 20.2f, 1e-5f);
}

using namespace openblack::animals::birds;

TEST(BirdRules, AGoodTempleHasDovesAndAnEvilOneBats)
{
	EXPECT_EQ(TempleBirdKind(0.3f), AnimalInfo::Dove);
	EXPECT_EQ(TempleBirdKind(0.0f), AnimalInfo::Dove);
	EXPECT_EQ(TempleBirdKind(-0.01f), AnimalInfo::Bat);
}

TEST(BirdRules, ATemplesBirdsGrowWithHowFarItsPlayerIsFromNeutral)
{
	EXPECT_EQ(TempleBirdCount(1.0f, 20, true), 20u);
	EXPECT_EQ(TempleBirdCount(-1.0f, 20, true), 20u);
	EXPECT_EQ(TempleBirdCount(0.5f, 20, true), 10u);
	// Cut down to a whole bird: none until a twentieth of the way
	EXPECT_EQ(TempleBirdCount(0.049f, 20, true), 0u);
	EXPECT_EQ(TempleBirdCount(-0.07f, 20, true), 1u);
	EXPECT_EQ(TempleBirdCount(1.0f, 20, false), 0u);
}

TEST(BirdRules, TheTemplesFlockMovesOneBirdAtATime)
{
	EXPECT_EQ(StepTowards(3, 5), TempleFlockStep::AddOne);
	EXPECT_EQ(StepTowards(5, 5), TempleFlockStep::None);
	EXPECT_EQ(StepTowards(6, 0), TempleFlockStep::RemoveOne);
}

TEST(BirdRules, TheTemplesBirdsFlyTenMetresOverItsModel)
{
	EXPECT_FLOAT_EQ(TempleFlockHeight(1.0f, 30.0f), 40.0f);
	EXPECT_FLOAT_EQ(TempleFlockHeight(1.5f, 20.0f), 40.0f);
}

TEST(BirdRules, ACreaturePicksUpOnlyABirdUnderAMetreAboveTheLand)
{
	EXPECT_TRUE(CreatureCanPickUp(0.0f));
	EXPECT_TRUE(CreatureCanPickUp(0.99f));
	EXPECT_FALSE(CreatureCanPickUp(1.0f));
	EXPECT_FALSE(CreatureCanPickUp(20.0f));
}

TEST(BirdRules, ACreatureStampsOnlyOnABirdLowerThanAQuarterOfItsOwnHeight)
{
	// A creature 15 m tall reaches birds under 3.75 m
	EXPECT_TRUE(CreatureCanStompOn(0.0f, 15.0f));
	EXPECT_TRUE(CreatureCanStompOn(3.7f, 15.0f));
	EXPECT_FALSE(CreatureCanStompOn(3.75f, 15.0f));
	// A small one only those on the ground
	EXPECT_TRUE(CreatureCanStompOn(0.5f, 3.0f));
	EXPECT_FALSE(CreatureCanStompOn(1.0f, 3.0f));
}

TEST(BirdRules, ABirdGoesOnlyWhereLandBlocksLieAllTheWayBack)
{
	// Pinned to the original run by the oracle (Dove::IsPosValidForMapCellExistance) on a map of every block but one,
	// the block (10, 10), which covers 1600..1760 m each way. The way is walked from the target's 16 m square back to the
	// bird's: the target's own square is not looked at, and the same square needs nothing
	const auto hasBlock = [](int32_t cellX, int32_t cellZ) { return !((cellX >> 4) == 10 && (cellZ >> 4) == 10); };
	const auto at = [](float x, float z) {
		return map_coords::MapCoords {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z)};
	};
	using animals::birds::LandAllTheWay;
	EXPECT_FALSE(LandAllTheWay(at(1900.0f, 1680.0f), at(1500.0f, 1680.0f), hasBlock));
	EXPECT_TRUE(LandAllTheWay(at(1900.0f, 1500.0f), at(1500.0f, 1500.0f), hasBlock));
	EXPECT_TRUE(LandAllTheWay(at(1655.0f, 1652.0f), at(1650.0f, 1650.0f), hasBlock));
	EXPECT_FALSE(LandAllTheWay(at(1700.0f, 1650.0f), at(1650.0f, 1650.0f), hasBlock));
	EXPECT_FALSE(LandAllTheWay(at(1900.0f, 1900.0f), at(1500.0f, 1500.0f), hasBlock));
	EXPECT_TRUE(LandAllTheWay(at(1500.0f, 1900.0f), at(1500.0f, 1500.0f), hasBlock));
	EXPECT_FALSE(LandAllTheWay(at(5200.0f, 100.0f), at(100.0f, 100.0f), hasBlock));
	EXPECT_TRUE(LandAllTheWay(at(-30.0f, 10.0f), at(10.0f, 10.0f), hasBlock));
	EXPECT_TRUE(LandAllTheWay(at(40.0f, 40.0f), at(-5.0f, -5.0f), hasBlock));
	EXPECT_FALSE(LandAllTheWay(at(1770.0f, 1900.0f), at(1590.0f, 1500.0f), hasBlock));
}

TEST(BirdRules, TheWaysSquaresAreTheMiddlesOfSixteenMetres)
{
	// whole metres (cut towards zero), then a sixteenth with 15 added below zero: -15..15 m all fall in the square 8
	using animals::birds::WaySquareCentre;
	EXPECT_EQ(WaySquareCentre(0), 8);
	EXPECT_EQ(WaySquareCentre(104857), 8);    // 15.9999 m
	EXPECT_EQ(WaySquareCentre(104858), 24);   // 16.00002 m
	EXPECT_EQ(WaySquareCentre(-98304), 8);    // -15 m
	EXPECT_EQ(WaySquareCentre(-104858), -8);  // -16.00002 m
	EXPECT_EQ(WaySquareCentre(-209716), -24); // -32.00006 m
}

TEST(AnimalRules, AGrazersFoodSearchGoesRoundATenthOfItsDomainSquaredInFloats)
{
	// (radius / 10)^2 as floats, then truncated: not the whole tenths squared
	EXPECT_EQ(animals::GrazeSearchCells(0), 0);
	EXPECT_EQ(animals::GrazeSearchCells(9), 0);
	EXPECT_EQ(animals::GrazeSearchCells(10), 1);
	EXPECT_EQ(animals::GrazeSearchCells(15), 2);
	EXPECT_EQ(animals::GrazeSearchCells(19), 3);
	EXPECT_EQ(animals::GrazeSearchCells(25), 6);
	EXPECT_EQ(animals::GrazeSearchCells(100), 100);
}

TEST(AnimalRules, AnAnimalIsFarEnoughFromAPredatorByItsTrueDistance)
{
	EXPECT_TRUE(animals::FarEnoughFromPredator(30.0f, 30.0f));
	EXPECT_TRUE(animals::FarEnoughFromPredator(31.0f, 30.0f));
	EXPECT_FALSE(animals::FarEnoughFromPredator(29.99f, 30.0f));
	EXPECT_FALSE(animals::FarEnoughFromPredator(std::numeric_limits<float>::quiet_NaN(), 30.0f));
}

TEST(WalkingList, ANewWalkerGoesFirstAndOneWalkingAgainKeepsItsPlace)
{
	std::vector<int> list;
	ecs::walking_list::Enlist(list, 1);
	ecs::walking_list::Enlist(list, 2);
	ecs::walking_list::Enlist(list, 3);
	EXPECT_EQ(list, (std::vector<int> {3, 2, 1}));
	ecs::walking_list::Enlist(list, 1);
	EXPECT_EQ(list, (std::vector<int> {3, 2, 1}));
}
