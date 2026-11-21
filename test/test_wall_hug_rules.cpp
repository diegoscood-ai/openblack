/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <functional>
#include <map>
#include <numbers>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/MapCollide.h"
#include "ECS/ThingCircles.h"
#include "ECS/WallHugBookkeeping.h"
#include "ECS/WallHugRules.h"

using namespace openblack::ecs::wall_hug;

namespace
{
/// A villager's usual walk: 4600 whole map units a turn
constexpr int32_t k_WholeSpeed = 4600;

/// Made-up circles are placed well inside the map, a hundred metres from its corner
constexpr glm::vec2 k_Middle {100.0f, 100.0f};

CircleSweepInput Around(glm::vec2 position, glm::vec2 goal, bool clockwise, std::span<const BlockingCircle> blockers = {})
{
	return {
	    .position = ToWhole(k_Middle + position),
	    .goal = ToWhole(k_Middle + goal),
	    .centre = k_Middle,
	    .radius = 5.0f,
	    .clockwise = clockwise,
	    .wholeSpeed = k_WholeSpeed,
	    .blockers = blockers,
	};
}

glm::vec2 OnCircle(float degrees, float radius = 5.0f)
{
	const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
	return glm::vec2(std::cos(radians), std::sin(radians)) * radius;
}
} // namespace

TEST(WallHugRules, TheWalkIsInWholeMapUnitsAtGameAngles)
{
	// At 4600 whole units a turn the step is a sixteenth of the speed (287) times the sine table, over 4096: 4592 along
	// x, and 2384 by 3924 at game angle 334 (about 59 degrees)
	EXPECT_EQ(StepAlong(0, 4600), glm::ivec2(4592, 0));
	EXPECT_EQ(StepAlong(334, 4600), glm::ivec2(2384, 3924));
	EXPECT_EQ(StepAlong(0x400, 4600), glm::ivec2(-4592, 0));
	// Going round a 5 m circle it turns 0.14 radians, 45 game angles, and one more
	EXPECT_EQ(OrbitTurn(4600, 5.0f), 46);
	EXPECT_EQ(OrbitTurn(4600, 50.0f), 5);
	// Within its step of a point: strictly nearer than its speed in whole units
	EXPECT_TRUE(WithinStep({0, 0}, {4599, 0}, 4600));
	EXPECT_FALSE(WithinStep({0, 0}, {4600, 0}, 4600));
	// Map positions and metres: a metre is 6553.6 whole units, truncated going in
	EXPECT_EQ(ToWhole({1.0f, 10.0f}), glm::ivec2(6553, 65536));
	EXPECT_FLOAT_EQ(ToPoint({65536, 32768}).x, 10.0f);
	EXPECT_FLOAT_EQ(ToPoint({65536, 32768}).y, 5.0f);
	EXPECT_FLOAT_EQ(MetresDistanceSq({0, 0}, {65536, 65536}), 200.0f);
}

TEST(WallHugRules, TheTurnRoundACircleIsOneMultiplyByTheGamesConstant)
{
	// The speed over the radius times the game's single constant, each a float, then truncated: worked out step by step
	// in single precision. Converting the radians to game angles in three steps (times 2048, over a full turn, over
	// 6553.6) gives 113 and 1039 here
	EXPECT_EQ(OrbitTurn(33891, 15.05f), 112);
	EXPECT_EQ(OrbitTurn(62671, 3.0f), 1040);
}

TEST(WallHugRules, AWalkerGoesRoundTheWayTheCircleLiesFromItsStep)
{
	// Heading along +x, with x to the right and z up: the circle's middle off to its right (-z) takes it round
	// clockwise, off to its left anticlockwise
	EXPECT_TRUE(GoesRoundClockwise({0, 0}, {65536, -65536}, {4592, 0}));
	EXPECT_FALSE(GoesRoundClockwise({0, 0}, {65536, 65536}, {4592, 0}));
}

TEST(WallHugRules, AGoalInsideTheCircleEndsTheWalkRoundAtItsBearing)
{
	// The goal is a quarter turn ahead going anticlockwise, three quarters going clockwise; a turn walks 0.7 m of the
	// arc and the look ahead counts it at two thirds of that
	const auto anticlockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false));
	EXPECT_EQ(anticlockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(anticlockwise.hugged.has_value());
	EXPECT_EQ(anticlockwise.turnsToObstacle, 7);
	ASSERT_TRUE(anticlockwise.heading.has_value());

	const auto clockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, true));
	EXPECT_EQ(clockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(clockwise.turnsToObstacle, 22);
}

TEST(WallHugRules, AtTheGoalsBearingTheWalkerStepsStraightIn)
{
	const auto sweep = SweepCircle(Around(OnCircle(89.5f), {0.0f, 2.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::StepThrough);
	EXPECT_FALSE(sweep.heading.has_value());
}

TEST(WallHugRules, WithTheGoalOutsideAndNothingInTheWayTheWalkerKeepsHugging)
{
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {20.0f, 20.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(sweep.turnsToObstacle, k_NoObstacleInReach);
	ASSERT_TRUE(sweep.heading.has_value());
	// On the circle, a tenth of a radius out from 0.9 of it: it faces a quarter turn (512 game angles) round from the
	// bearing to the middle (1024, back along -x), anticlockwise here, less 64 game angles for each radius beyond 0.9
	// of it, truncated: 1024 - 512 + 6
	EXPECT_EQ(*sweep.heading, 518);
}

TEST(WallHugRules, AnOverlappingCircleAheadEndsTheWalkRound)
{
	// The neighbour cuts the hugged circle at a bearing of about 37 degrees; going clockwise from 90 the walker has 53
	// degrees to go
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = k_Middle + glm::vec2(8.0f, 0.0f), .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(90.0f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 4);
}

TEST(WallHugRules, AtTheOverlapTheWalkerIsHandedOverToTheNeighbour)
{
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = k_Middle + glm::vec2(8.0f, 0.0f), .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(37.5f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	ASSERT_TRUE(sweep.hugged.has_value());
	EXPECT_EQ(*sweep.hugged, 0U);
	EXPECT_FALSE(sweep.needsLookahead);
}

TEST(WallHugRules, WaterNeverCutsShortTheWalkRoundToAGoalInside)
{
	const std::array<BlockingCircle, 1> water {{{.centre = k_Middle + glm::vec2(6.0f, 6.0f),
	                                             .radius = k_LandscapeBlockerRadius,
	                                             .landscape = true,
	                                             .landscapeOrFence = true}}};
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false, water));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 7);
}

TEST(WallHugRules, AWalkerLeavesTheCircleOnlyNearerItsGoalAndWithTheGoalAhead)
{
	const glm::ivec2 goal = ToWhole(k_Middle);
	const auto at = [](float x) { return ToWhole(k_Middle + glm::vec2(x, 0.0f)); };
	// 10 m from the goal on starting round: 1279 hundred-and-twenty-eighths
	const auto entry = EntryDistance(at(-10.0f), goal);
	EXPECT_EQ(entry, 1279U);
	EXPECT_EQ(EntryDistance(goal, goal), 0U);
	// Nearer, with the goal ahead and off to the side away from an anticlockwise turn: going anticlockwise it leaves
	EXPECT_TRUE(LeavesCircle(at(-5.0f), goal, {3277, 3277}, entry, false));
	EXPECT_FALSE(LeavesCircle(at(-5.0f), goal, {3277, 3277}, entry, true));
	// The goal behind it
	EXPECT_FALSE(LeavesCircle(at(-5.0f), goal, {-3277, 3277}, entry, false));
	// No nearer than on starting round
	EXPECT_FALSE(LeavesCircle(at(-11.0f), goal, {3277, 3277}, entry, false));
}

TEST(WallHugRules, TheEntryDistanceIsWorkedOutInSinglePrecision)
{
	// About 37.8 m: the square root, the times 128 and the less one each a float give 4841. In double precision it
	// would be 4840
	EXPECT_EQ(EntryDistance({1000000, 1000000}, {1167349, 817096}), 4841U);
}

TEST(WallHugRules, FarOutTheLeaveTestTakesEachDifferenceExactlyBeforeMakingItAFloat)
{
	// Beyond 2560 m a map position is no longer exact as a float. The difference from the goal is taken in whole
	// units first and made a float once (16777216 along x, and the same along z), so the goal is straight along the
	// step and not to one side: the walker going round clockwise does not leave. Making each position a float first
	// would give 16777215 along x and send it off the circle
	const glm::ivec2 position {16777217, 16777216};
	const glm::ivec2 goal {1, 0};
	const glm::ivec2 step {-1, -1};
	constexpr uint32_t k_FarEntry = 600000;
	EXPECT_FALSE(LeavesCircle(position, goal, step, k_FarEntry, true));
	EXPECT_FALSE(LeavesCircle(position, goal, step, k_FarEntry, false));
}

namespace
{
/// A walker at 1000 m, 1000 m stepping 5 m a turn along x
const glm::ivec2 k_Walker = ToWhole({1000.0f, 1000.0f});
constexpr glm::ivec2 k_FiveMetresAlongX {32768, 0};

BlockingCircle Thing(glm::vec2 offset, float radius)
{
	return {.centre = glm::vec2(1000.0f, 1000.0f) + offset, .radius = radius, .landscape = false, .landscapeOrFence = false};
}
} // namespace

TEST(WallHugRules, HeadingStraightOnTheWalkerHeadsForTheNearestCircleOnItsLine)
{
	// The far one comes first in the cell, the near one is reached first: its edge 38 m on, after 7 whole turns
	const std::array<BlockingCircle, 3> circles {Thing({60.0f, 0.0f}, 2.0f), Thing({0.0f, 30.0f}, 2.0f),
	                                             Thing({40.0f, 1.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	ASSERT_TRUE(scan.circle.has_value());
	EXPECT_EQ(*scan.circle, 2U);
	EXPECT_EQ(scan.turnsToObstacle, 7);
}

TEST(WallHugRules, HeadingStraightOnNothingOnTheLineIsNothingInReach)
{
	const std::array<BlockingCircle, 2> circles {Thing({0.0f, 30.0f}, 2.0f), Thing({-20.0f, 0.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	EXPECT_FALSE(scan.circle.has_value());
	EXPECT_EQ(scan.turnsToObstacle, k_NoObstacleInReach);
	// Nor is a circle more than 255 turns away
	const std::array<BlockingCircle, 1> far {Thing({1300.0f, 0.0f}, 2.0f)};
	EXPECT_FALSE(ScanLine(k_Walker, k_FiveMetresAlongX, far).circle.has_value());
}

TEST(WallHugRules, HeadingStraightOnACircleTheWalkerIsWellInsideDoesNotCount)
{
	// Five metres inside the first: it is passed over for the one ahead
	const std::array<BlockingCircle, 2> circles {Thing({0.0f, 0.0f}, 5.0f), Thing({20.0f, 0.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	ASSERT_TRUE(scan.circle.has_value());
	EXPECT_EQ(*scan.circle, 1U);
	EXPECT_EQ(scan.turnsToObstacle, 3);
	// Just inside the edge of one, a tenth of a metre, it meets it at once
	const std::array<BlockingCircle, 1> edge {Thing({2.1f, 0.0f}, 2.2f)};
	const auto atOnce = ScanLine(k_Walker, k_FiveMetresAlongX, edge);
	ASSERT_TRUE(atOnce.circle.has_value());
	EXPECT_EQ(atOnce.turnsToObstacle, 0);
}

TEST(WallHugRules, HeadingStraightOnFromMetresIsTheSameLook)
{
	// A step of whole map units made metres gives the same answer from metres as from whole units
	const std::array<BlockingCircle, 3> circles {Thing({60.0f, 0.0f}, 2.0f), Thing({0.0f, 30.0f}, 2.0f),
	                                             Thing({40.0f, 1.0f}, 2.0f)};
	const auto whole = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	const auto metres = ScanLineFrom(ToPoint(k_Walker), ToPoint(k_FiveMetresAlongX), circles);
	EXPECT_EQ(metres.circle, whole.circle);
	EXPECT_EQ(metres.turnsToObstacle, whole.turnsToObstacle);
}

TEST(WallHugRules, OnlyTheFencesModelsAreFences)
{
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingAmericanFence));
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingCelticFenceShort));
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingCelticFenceTall));
	EXPECT_FALSE(IsFenceModel(openblack::MeshId::BuildingCeltic4));
}

// A trial walk has a turn, then up to 1,500 more: the turns left when it gets there are its turns to spare
TEST(WallHugRules, ATrialWalkCountsTheTurnsItHasToSpare)
{
	const auto arrivingOnTurn = [](int turn) {
		int walked = 0;
		return TurnsToSpare([&walked, turn]() { return ++walked == turn; });
	};
	EXPECT_EQ(arrivingOnTurn(1), 1500);
	EXPECT_EQ(arrivingOnTurn(2), 1499);
	EXPECT_EQ(arrivingOnTurn(1501), 0);
	EXPECT_EQ(arrivingOnTurn(1502), -1);
	// Never getting there, it walks 1,501 turns
	int walked = 0;
	EXPECT_EQ(TurnsToSpare([&walked]() {
		          ++walked;
		          return false;
	          }),
	          -1);
	EXPECT_EQ(walked, 1501);
}

// Turned round wins with more turns to spare, or with as many and ending nearer; a tie keeps the way it goes
TEST(WallHugRules, TurningRoundWinsOnlyWhenItGetsThereSoonerOrEndsNearer)
{
	EXPECT_TRUE(TurnedRoundIsBetter({.turnsToSpare = 900, .distanceSq = 9.0f}, {.turnsToSpare = 800, .distanceSq = 0.0f}));
	EXPECT_FALSE(TurnedRoundIsBetter({.turnsToSpare = 800, .distanceSq = 0.0f}, {.turnsToSpare = 900, .distanceSq = 9.0f}));
	EXPECT_TRUE(TurnedRoundIsBetter({.turnsToSpare = -1, .distanceSq = 4.0f}, {.turnsToSpare = -1, .distanceSq = 25.0f}));
	EXPECT_FALSE(TurnedRoundIsBetter({.turnsToSpare = -1, .distanceSq = 25.0f}, {.turnsToSpare = -1, .distanceSq = 4.0f}));
	EXPECT_FALSE(TurnedRoundIsBetter({.turnsToSpare = 5, .distanceSq = 1.0f}, {.turnsToSpare = 5, .distanceSq = 1.0f}));
}

// A walker turned round jumps three steps on and faces back. The numbers are the original game's, run on these
// walkers: its step's along and height parts change places as they turn round
TEST(WallHugRules, AWalkerTurnedRoundJumpsThreeStepsOnAndFacesBack)
{
	const auto turned = TurnRound({1000000, 2000000}, {1200, -500}, 0.0f, 100);
	EXPECT_EQ(turned.position, glm::ivec2(1003600, 1998500));
	EXPECT_EQ(turned.step, glm::ivec2(-1200, 0));
	EXPECT_EQ(turned.stepAltitude, 500.0f);
	EXPECT_EQ(turned.gameAngle, 1124);
	EXPECT_EQ(TurnRound({0, 0}, {0, 0}, 0.0f, 1500).gameAngle, 476);

	const auto backwards = TurnRound({3000000, 400000}, {-4093, 77}, 0.0f, 0x7ff);
	EXPECT_EQ(backwards.position, glm::ivec2(2987721, 400231));
	EXPECT_EQ(backwards.step, glm::ivec2(4093, 0));
	EXPECT_EQ(backwards.stepAltitude, -77.0f);
	EXPECT_EQ(backwards.gameAngle, 1023);

	// Turned round twice before its step is aimed again, its along part comes back
	const auto twice = TurnRound(turned.position, turned.step, turned.stepAltitude, turned.gameAngle);
	EXPECT_EQ(twice.step, glm::ivec2(1200, -500));
	EXPECT_EQ(twice.stepAltitude, 0.0f);
	EXPECT_EQ(twice.gameAngle, 100);

	// The height part is truncated into the along part, and the along part rounded into the height part
	const auto truncated = TurnRound({500000, 500000}, {7, 9}, -2.75f, 300);
	EXPECT_EQ(truncated.position, glm::ivec2(500021, 500027));
	EXPECT_EQ(truncated.step, glm::ivec2(-7, 2));
	EXPECT_EQ(truncated.stepAltitude, -9.0f);
	const auto rounded = TurnRound({500000, 500000}, {7, 16777217}, 16777217.0f, 300);
	EXPECT_EQ(rounded.position, glm::ivec2(500021, 50831651));
	EXPECT_EQ(rounded.step, glm::ivec2(-7, -16777216));
	EXPECT_EQ(rounded.stepAltitude, -16777216.0f);
}

// A walker going round a circle that keeps its way faces a quarter turn round from the circle's middle; the angles
// towards the middle are the original game's
TEST(WallHugRules, AWalkerKeepingItsWayRoundFacesAQuarterTurnFromTheMiddle)
{
	const glm::ivec2 position {655360, 655360};
	const glm::ivec2 east = ToWhole({110.0f, 100.0f});
	EXPECT_EQ(HeadingRound(position, east, true), 0x200);
	EXPECT_EQ(HeadingRound(position, east, false), 2048 - 0x200);
	// Towards (103, 104) metres is 304, and towards (94.875, 102) metres 904
	EXPECT_EQ(HeadingRound(position, {675020, 681574}, true), 816);
	EXPECT_EQ(HeadingRound(position, {675020, 681574}, false), 1840);
	EXPECT_EQ(HeadingRound(position, {621776, 668467}, true), 1416);
	EXPECT_EQ(HeadingRound(position, {621776, 668467}, false), 392);
}

// ---- The circles of the things on the map (ecs::thing_circles) -----------------------------------------------------

namespace
{
namespace thing_circles = openblack::ecs::thing_circles;
using openblack::ecs::map_collide::Shape;

/// A made-up cell: its fixed list in order, each thing's circles, and the cells of water or off the land
struct FakeCell
{
	std::vector<entt::entity> fixed;
	std::map<entt::entity, std::vector<BlockingCircle>> circles;
	std::function<bool(glm::ivec2)> blocked;

	[[nodiscard]] thing_circles::CellSources Sources() const
	{
		return {
		    .forEachFixed =
		        [this](glm::ivec2 /*cell*/, const std::function<bool(entt::entity)>& fn) {
			        for (const auto thing : fixed)
			        {
				        if (!fn(thing))
				        {
					        return;
				        }
			        }
		        },
		    .circlesOf =
		        [this](entt::entity thing) {
			        const auto found = circles.find(thing);
			        return found != circles.end() ? found->second : std::vector<BlockingCircle> {};
		        },
		    .blocked = blocked,
		};
	}
};
} // namespace

TEST(ThingCircles, AShapeWithoutARowIsItsOneCircle)
{
	const Shape shape {.centre = {10.0f, 20.0f}, .radius = 3.5f, .children = {}, .childRadius = 0.0f, .what = "test"};
	const auto circles = thing_circles::CirclesOf(shape, false);
	ASSERT_EQ(circles.size(), 1U);
	EXPECT_EQ(circles[0].centre, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(circles[0].radius, 3.5f);
	EXPECT_FALSE(circles[0].landscape);
	EXPECT_FALSE(circles[0].landscapeOrFence);
}

TEST(ThingCircles, ALongShapeIsItsRowOfCirclesNotTheOneRoundIt)
{
	// The circle round the whole row is not one the walkers come across
	const Shape shape {.centre = {10.0f, 20.0f},
	                   .radius = 6.0f,
	                   .children = {{6.0f, 20.0f}, {10.0f, 20.0f}, {14.0f, 20.0f}},
	                   .childRadius = 2.0f,
	                   .what = "test"};
	const auto circles = thing_circles::CirclesOf(shape, true);
	ASSERT_EQ(circles.size(), 3U);
	for (size_t i = 0; i < circles.size(); ++i)
	{
		EXPECT_EQ(circles[i].centre, shape.children[i]);
		EXPECT_EQ(circles[i].radius, 2.0f);
		// a fence's circles count as the land's edge does, but are a thing's
		EXPECT_FALSE(circles[i].landscape);
		EXPECT_TRUE(circles[i].landscapeOrFence);
	}
}

TEST(ThingCircles, ACellsCirclesFollowItsFixedListThingByThing)
{
	// The head first; a thing's row of circles before the next thing; a thing without circles (no collide data, or a
	// field) gives none
	const auto house = entt::entity {3};
	const auto pot = entt::entity {7};
	const auto rock = entt::entity {1};
	FakeCell cell;
	cell.fixed = {house, pot, rock};
	cell.circles[house] = {Thing({0.0f, 0.0f}, 2.0f), Thing({4.0f, 0.0f}, 2.0f)};
	cell.circles[rock] = {Thing({20.0f, 0.0f}, 1.0f)};
	const auto circles = thing_circles::CirclesInCell({100, 100}, cell.Sources());
	ASSERT_EQ(circles.size(), 3U);
	EXPECT_EQ(circles[0].owner, house);
	EXPECT_EQ(circles[0].index, 0);
	EXPECT_EQ(circles[1].owner, house);
	EXPECT_EQ(circles[1].index, 1);
	EXPECT_EQ(circles[1].circle.centre, Thing({4.0f, 0.0f}, 2.0f).centre);
	EXPECT_EQ(circles[2].owner, rock);
	EXPECT_EQ(circles[2].index, 0);
	EXPECT_EQ(circles[2].circle.radius, 1.0f);
}

TEST(ThingCircles, AfterItsThingsACellHasItsWaterAndItsNeighboursInTheGamesOrder)
{
	// Every cell round (100, 100) is water: after the thing, the cell itself, then +x, -x, +z, -z, then the corners
	// -x -z, -x +z, +x -z, +x +z, each a 7.2 m circle in the cell's middle
	const auto rock = entt::entity {5};
	FakeCell cell;
	cell.fixed = {rock};
	cell.circles[rock] = {Thing({0.0f, 0.0f}, 1.0f)};
	cell.blocked = [](glm::ivec2) { return true; };
	const auto circles = thing_circles::CirclesInCell({100, 100}, cell.Sources());
	ASSERT_EQ(circles.size(), 10U);
	EXPECT_EQ(circles[0].owner, rock);
	EXPECT_FALSE(circles[0].circle.landscape);
	const std::array<glm::ivec2, 9> order {
	    {{100, 100}, {101, 100}, {99, 100}, {100, 101}, {100, 99}, {99, 99}, {99, 101}, {101, 99}, {101, 101}}};
	for (size_t i = 0; i < order.size(); ++i)
	{
		const auto& water = circles[i + 1];
		EXPECT_EQ(water.cell, order[i]) << i;
		EXPECT_EQ(water.owner, entt::entity {entt::null}) << i;
		EXPECT_EQ(water.circle.centre, glm::vec2(order[i]) * 10.0f + 5.0f) << i;
		EXPECT_EQ(water.circle.radius, 7.2f) << i;
		EXPECT_TRUE(water.circle.landscape) << i;
		EXPECT_TRUE(water.circle.landscapeOrFence) << i;
	}
}

TEST(ThingCircles, OnlyTheCellsOfWaterOrOffTheLandHaveACircle)
{
	// At the map's corner the cells below 0 are counted in 16 bits (65535) and are off the land; their circles' middles
	// are below 0 too, 5 m out
	FakeCell cell;
	cell.blocked = [](glm::ivec2 c) { return c.x >= 512 || c.y >= 512; };
	const auto circles = thing_circles::CirclesInCell({0, 0}, cell.Sources());
	ASSERT_EQ(circles.size(), 5U);
	EXPECT_EQ(circles[0].cell, glm::ivec2(65535, 0));
	EXPECT_EQ(circles[0].circle.centre, glm::vec2(-5.0f, 5.0f));
	EXPECT_EQ(circles[1].cell, glm::ivec2(0, 65535));
	EXPECT_EQ(circles[1].circle.centre, glm::vec2(5.0f, -5.0f));
	EXPECT_EQ(circles[2].cell, glm::ivec2(65535, 65535));
	EXPECT_EQ(circles[2].circle.centre, glm::vec2(-5.0f, -5.0f));
	EXPECT_EQ(circles[3].cell, glm::ivec2(65535, 1));
	EXPECT_EQ(circles[4].cell, glm::ivec2(1, 65535));
}
