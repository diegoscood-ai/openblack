/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
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

TEST(BirdRules, ABirdsNextLegVariesAboutItsLastGoalsHeightWithinItsKindsLimits)
{
	// a spell dove's rows of the info file: a variance of 10 m, and from 10 m under to 20 m over its flock's height of 45
	constexpr float k_Variance = 10.0f;
	constexpr float k_Least = -10.0f;
	constexpr float k_Most = 20.0f;
	constexpr float k_Flock = 45.0f;
	// legs one after the other, each from the last one's goal height: (goal + 10) less the draw
	constexpr std::array k_Draws {3.0f, 17.0f, 0.5f, 19.75f, 1.25f};
	constexpr std::array k_Heights {52.0f, 45.0f, 54.5f, 44.75f, 53.5f};
	float goal = k_Flock;
	for (size_t leg = 0; leg < k_Draws.size(); ++leg)
	{
		goal = NextLegAltitude(goal, k_Variance, k_Draws.at(leg), k_Flock, k_Least, k_Most);
		EXPECT_FLOAT_EQ(goal, k_Heights.at(leg)) << leg;
	}
	// outside 35 to 65 m, both ends out: the flock's height
	EXPECT_FLOAT_EQ(NextLegAltitude(64.0f, k_Variance, 2.0f, k_Flock, k_Least, k_Most), k_Flock);
	EXPECT_FLOAT_EQ(NextLegAltitude(26.0f, k_Variance, 11.0f, k_Flock, k_Least, k_Most), k_Flock);
	EXPECT_FLOAT_EQ(NextLegAltitude(55.0f, k_Variance, 0.0f, k_Flock, k_Least, k_Most), k_Flock);
	EXPECT_FLOAT_EQ(NextLegAltitude(45.0f, k_Variance, 20.0f, k_Flock, k_Least, k_Most), k_Flock);
	EXPECT_FLOAT_EQ(NextLegAltitude(55.0f, k_Variance, 0.5f, k_Flock, k_Least, k_Most), 64.5f);
}

// A bird's height on the way, as the original's 3D move: towards a goal under 2 m it steps by its kind's change above
// the land and stays put within a change of it; towards a higher goal its absolute height steps the same way, so it
// does not follow the hills, and it is never under 2 m above the land where it now is
TEST(BirdRules, ABirdClimbsOrSinksByItsKindsChangeAndNeverFliesUnderTwoMetres)
{
	namespace birds = animals::birds;
	EXPECT_FLOAT_EQ(birds::LowGoalStep(1.0f, 0.0f, 0.25f), 0.75f);
	EXPECT_FLOAT_EQ(birds::LowGoalStep(0.0f, 1.5f, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(birds::LowGoalStep(0.25f, 0.0f, 0.25f), 0.25f); // within a change: stays
	// 30 m up over land at 5 m: towards 40 m it climbs half a metre, the land under it now taken off
	EXPECT_FLOAT_EQ(birds::HighGoalStep(30.0f, 40.0f, 0.5f, 5.0f), 25.5f);
	EXPECT_FLOAT_EQ(birds::HighGoalStep(30.0f, 20.0f, 0.5f, 5.0f), 24.5f);
	EXPECT_FLOAT_EQ(birds::HighGoalStep(30.0f, 30.25f, 0.5f, 5.0f), 25.0f); // within a change: its height kept
	// the land rising under it: its absolute height kept, but never under 2 m above the land
	EXPECT_FLOAT_EQ(birds::HighGoalStep(30.0f, 30.0f, 0.5f, 26.0f), 4.0f);
	EXPECT_FLOAT_EQ(birds::HighGoalStep(30.0f, 30.0f, 0.5f, 29.0f), birds::k_LowestFlight);
	EXPECT_FLOAT_EQ(birds::k_LowestFlight, 2.0f);
}

// A follower's slot, pinned to the original run by the oracle on a fake flock (the row and column it turns into an
// angle): the flock's size less the follower's place in the list (0 the newest) picks the square that holds it, the odd
// ones on one side and the even ones on the other, halved toward zero
TEST(BirdRules, AFollowersSlotGrowsOutwardsSquareBySquareFromItsPlaceInTheList)
{
	struct Case
	{
		int32_t members;
		int32_t index;
		int32_t row;
		int32_t column;
	};
	constexpr std::array k_Cases {
	    Case {2, 0, -3, -1},
	    Case {3, 0, -3, 1},
	    Case {4, 0, -2, -2},
	    Case {5, 0, -2, 2},
	    Case {6, 0, -2, -1},
	    Case {7, 0, -2, 1},
	    Case {8, 0, -2, 0},
	    Case {9, 0, -1, 4},
	    Case {10, 0, -1, -3},
	    Case {17, 0, 0, 4},
	    Case {17, 1, 0, -4},
	    Case {26, 0, 1, -5},
	    // a bird the list does not hold counts one place before the newest
	    Case {4, -1, -2, 2},
	};
	for (const auto& c : k_Cases)
	{
		const auto slot = FormationSlotOf(c.members, c.index);
		EXPECT_EQ(slot.row, c.row) << c.members << " " << c.index;
		EXPECT_EQ(slot.column, c.column) << c.members << " " << c.index;
	}
}

// The point a follower flies to for its slot, pinned to the original run by the oracle on a fake flock (the map
// coordinates it hands to its move): the slot turned by the way to the leader, laid out from the leader in 10 m steps
TEST(BirdRules, AFollowerFliesToItsSlotTurnedByTheWayToItsLeader)
{
	struct Case
	{
		int32_t members;
		int32_t index;
		map_coords::MapCoords follower;
		map_coords::MapCoords leader;
		int32_t x;
		int32_t z;
	};
	constexpr map_coords::MapCoords k_A {.x = 1000000, .z = 2000000};
	constexpr map_coords::MapCoords k_LeaderA {.x = 1100000, .z = 2050000};
	constexpr map_coords::MapCoords k_B {.x = 16777216, .z = 16777216};
	constexpr map_coords::MapCoords k_LeaderB {.x = 16700000, .z = 16900000};
	constexpr map_coords::MapCoords k_C {.x = 1966200, .z = 1638500};
	constexpr map_coords::MapCoords k_LeaderC {.x = 2031740, .z = 1605730};
	constexpr std::array k_Cases {
	    Case {2, 0, k_A, k_LeaderA, 960551, 2003801},    Case {2, 0, k_B, k_LeaderB, 16851212, 16858114},
	    Case {2, 0, k_C, k_LeaderC, 1837172, 1615147},   Case {3, 0, k_A, k_LeaderA, 905432, 2059417},
	    Case {3, 0, k_B, k_LeaderB, 16746600, 16963668}, Case {4, 0, k_A, k_LeaderA, 1058503, 1925670},
	    Case {4, 0, k_C, k_LeaderC, 1907410, 1564233},   Case {5, 0, k_A, k_LeaderA, 975670, 2008503},
	    Case {5, 0, k_B, k_LeaderB, 16670498, 17027708}, Case {5, 0, k_C, k_LeaderC, 1990243, 1481400},
	    Case {9, 0, k_A, k_LeaderA, 1057499, 1850455},   Case {9, 1, k_A, k_LeaderA, 982743, 2050000},
	    Case {9, 2, k_A, k_LeaderA, 968928, 2050000},    Case {9, 3, k_A, k_LeaderA, 1021276, 1997601},
	    Case {10, 0, k_A, k_LeaderA, 1109417, 1855432},  Case {17, 0, k_A, k_LeaderA, 1100000, 1815486},
	    Case {17, 2, k_A, k_LeaderA, 1037835, 2029251},  Case {17, 3, k_A, k_LeaderA, 1079251, 1987835},
	    Case {17, 4, k_A, k_LeaderA, 1047601, 1971276},  Case {17, 5, k_A, k_LeaderA, 1100000, 1918928},
	    Case {17, 6, k_A, k_LeaderA, 1053801, 1910551},
	};
	for (const auto& c : k_Cases)
	{
		const auto goal = FormationGoal(FormationSlotOf(c.members, c.index), c.follower, c.leader);
		EXPECT_EQ(goal.x, c.x) << c.members << " " << c.index;
		EXPECT_EQ(goal.z, c.z) << c.members << " " << c.index;
	}
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

TEST(AnimalRules, AnAnimalWeighsAReactionOfItsOwnTypeOnlyWhereTheTableAllowsIt)
{
	// Another type (a creature 6 while it runs from a thrown thing 9): always weighed
	EXPECT_TRUE(animals::MayWeighAnotherReaction(9, 4, 6, 5, false));
	// The same type without the table's flag (9 -> 9, 7 -> 7): never, whichever reaction it is
	EXPECT_FALSE(animals::MayWeighAnotherReaction(9, 4, 9, 5, false));
	EXPECT_FALSE(animals::MayWeighAnotherReaction(7, 4, 7, 5, false));
	// The same type with the flag (28, another predator): weighed, but never the reaction it already follows
	EXPECT_TRUE(animals::MayWeighAnotherReaction(28, 4, 28, 5, true));
	EXPECT_FALSE(animals::MayWeighAnotherReaction(28, 4, 28, 4, true));
}

TEST(AnimalRules, AnAnimalLookingAtAThingTurnsAStepATurnTheShortWay)
{
	// The original's answers for its step of looking at a thing (current, target -> new heading)
	constexpr int32_t k_Step = animals::k_LookAtObjectTurn;
	EXPECT_EQ(k_Step, 0x80);
	// less than a step away: straight to it
	EXPECT_EQ(animals::LookTowards(100, 150, k_Step), 150);
	EXPECT_EQ(animals::LookTowards(100, 227, k_Step), 227);
	// a step or more: a step, the short way, round the circle
	EXPECT_EQ(animals::LookTowards(100, 228, k_Step), 228);
	EXPECT_EQ(animals::LookTowards(100, 300, k_Step), 228);
	EXPECT_EQ(animals::LookTowards(100, 1900, k_Step), 2020);
	EXPECT_EQ(animals::LookTowards(1900, 100, k_Step), 2028);
	EXPECT_EQ(animals::LookTowards(1900, 1000, k_Step), 1772);
	EXPECT_EQ(animals::LookTowards(2000, 100, k_Step), 80);
	// half a circle exactly: down from below the target, up from above it
	EXPECT_EQ(animals::LookTowards(0, 1024, k_Step), 1920);
	EXPECT_EQ(animals::LookTowards(1024, 0, k_Step), 1152);
	// a heading beyond the circle is taken as it is
	EXPECT_EQ(animals::LookTowards(3000, 100, k_Step), 1080);
}

TEST(AnimalRules, AnAnimalLetsGoOfItsFoodWhenItGoesOrTheHandHoldsIt)
{
	// the food no longer available, or held by the player's hand (the original tests both, either drops it)
	EXPECT_FALSE(animals::DropsFoodTarget(true, false));
	EXPECT_TRUE(animals::DropsFoodTarget(false, false));
	EXPECT_TRUE(animals::DropsFoodTarget(true, true));
	EXPECT_TRUE(animals::DropsFoodTarget(false, true));
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

TEST(AnimalRules, AnAnimalRunsFromAThingFlyingNearerThanTwiceItsSpeed)
{
	EXPECT_TRUE(animals::FleesFlyingObject(5.0f, 9.9f));
	EXPECT_FALSE(animals::FleesFlyingObject(5.0f, 10.0f));
	EXPECT_FALSE(animals::FleesFlyingObject(5.0f, 30.0f));
	// a thing at rest frightens nothing
	EXPECT_FALSE(animals::FleesFlyingObject(0.0f, 0.0f));
}
