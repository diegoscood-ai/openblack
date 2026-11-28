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
#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "ECS/AnimalWallHug.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/WallHugBookkeeping.h"

using namespace openblack::ecs::wall_hug;

TEST(WallHugBookkeeping, TheMoveStatesKeepTheGamesNumbers)
{
	// The game's numbers for a walker's move states, which the animals' brains hold as they are
	struct Case
	{
		WalkState state;
		uint8_t number;
	};
	constexpr std::array<Case, 11> k_Cases {{
	    {WalkState::Arrived, 0x1},
	    {WalkState::FinalStep, 0x4},
	    {WalkState::Wander, 0x5},
	    {WalkState::StepThrough, 0xB},
	    {WalkState::Linear, 0xC},
	    {WalkState::LinearClockwise, 0xD},
	    {WalkState::LinearAnticlockwise, 0xE},
	    {WalkState::OrbitClockwise, 0xF},
	    {WalkState::OrbitAnticlockwise, 0x10},
	    {WalkState::ExitAnticlockwise, 0x11},
	    {WalkState::ExitClockwise, 0x12},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(static_cast<uint8_t>(c.state), c.number);
	}
}

TEST(WallHugBookkeeping, AWalkerHeadingStraightOnLooksAgainWhenItsCircleGoes)
{
	for (const auto state : {WalkState::Linear, WalkState::LinearClockwise, WalkState::LinearAnticlockwise})
	{
		const auto gone = OnCircleGone(state);
		EXPECT_EQ(gone.state, state);
		EXPECT_FALSE(gone.reAim);
		EXPECT_TRUE(gone.lookAgain);
		EXPECT_FALSE(gone.letGo);
	}
}

TEST(WallHugBookkeeping, AWalkerGoingRoundHeadsStraightOnTheSameWayRound)
{
	const auto clockwise = OnCircleGone(WalkState::OrbitClockwise);
	EXPECT_EQ(clockwise.state, WalkState::LinearClockwise);
	EXPECT_TRUE(clockwise.reAim);
	EXPECT_TRUE(clockwise.lookAgain);
	EXPECT_FALSE(clockwise.letGo);
	const auto anticlockwise = OnCircleGone(WalkState::OrbitAnticlockwise);
	EXPECT_EQ(anticlockwise.state, WalkState::LinearAnticlockwise);
	EXPECT_TRUE(anticlockwise.reAim);
	EXPECT_TRUE(anticlockwise.lookAgain);
}

TEST(WallHugBookkeeping, AWalkerLeavingACircleAlsoLetsGoOfWhatItFinds)
{
	const auto clockwise = OnCircleGone(WalkState::ExitClockwise);
	EXPECT_EQ(clockwise.state, WalkState::LinearClockwise);
	EXPECT_TRUE(clockwise.reAim && clockwise.lookAgain && clockwise.letGo);
	const auto anticlockwise = OnCircleGone(WalkState::ExitAnticlockwise);
	EXPECT_EQ(anticlockwise.state, WalkState::LinearAnticlockwise);
	EXPECT_TRUE(anticlockwise.reAim && anticlockwise.lookAgain && anticlockwise.letGo);
}

TEST(WallHugBookkeeping, AnyOtherWalkerOnlyLetsGo)
{
	for (const auto state : {WalkState::Arrived, WalkState::FinalStep, WalkState::Wander, WalkState::StepThrough})
	{
		const auto gone = OnCircleGone(state);
		EXPECT_EQ(gone.state, state);
		EXPECT_FALSE(gone.reAim);
		EXPECT_FALSE(gone.lookAgain);
		EXPECT_TRUE(gone.letGo);
	}
}

TEST(WallHugBookkeeping, TheLandscapeCellsComeInTheGamesOrder)
{
	const std::array<glm::ivec2, 9> expected {{
	    {0, 0},
	    {1, 0},
	    {-1, 0},
	    {0, 1},
	    {0, -1},
	    {-1, -1},
	    {-1, 1},
	    {1, -1},
	    {1, 1},
	}};
	EXPECT_EQ(k_LandscapeCellOrder, expected);
}

TEST(WallHugBookkeeping, TheLandscapeCircleStandsInTheMiddleOfItsCell)
{
	// The original's metres for these cells, as bits
	struct Case
	{
		int32_t cell;
		uint32_t metres;
	};
	constexpr std::array<Case, 7> k_Cases {{
	    {0, 0x40A00000},
	    {1, 0x41700000},
	    {255, 0x451FB000},
	    {511, 0x459FD800},
	    {512, 0x45A02800},
	    {-1, 0xC0A00000},
	    {-2, 0xC1700000},
	}};
	for (const auto& c : k_Cases)
	{
		const auto centre = LandscapeCircleCentre({c.cell, 3});
		EXPECT_EQ(std::bit_cast<uint32_t>(centre.x), c.metres) << c.cell;
		EXPECT_EQ(std::bit_cast<uint32_t>(centre.y), 0x420C0000u) << c.cell;
	}
}

TEST(WallHugBookkeeping, ATrialThatArrivesAtOnceKeepsAllItsTurns)
{
	int calls = 0;
	EXPECT_EQ(RunTrial([&] { return ++calls == 1; }), k_TrialTurns);
	EXPECT_EQ(calls, 1);
}

TEST(WallHugBookkeeping, ATrialLosesATurnForEveryTurnItDidNotArrive)
{
	int calls = 0;
	EXPECT_EQ(RunTrial([&] { return ++calls == 4; }), k_TrialTurns - 3);
	EXPECT_EQ(calls, 4);
}

TEST(WallHugBookkeeping, ATrialThatNeverArrivesWalksOneTurnMoreThanItsTurns)
{
	int calls = 0;
	const auto neverArrives = [&calls] {
		++calls;
		return false;
	};
	EXPECT_EQ(RunTrial(neverArrives), -1);
	EXPECT_EQ(calls, k_TrialTurns + 1);
}

TEST(WallHugBookkeeping, MoreTurnsLeftDoesBetterWhateverTheDistance)
{
	EXPECT_TRUE(DidBetter({10, 100.0f}, {9, 1.0f}));
	EXPECT_FALSE(DidBetter({9, 1.0f}, {10, 100.0f}));
	EXPECT_TRUE(DidBetter({0, 0.0f}, {-1, 0.0f}));
}

TEST(WallHugBookkeeping, AsManyTurnsLeftThenNearerDoesBetter)
{
	EXPECT_TRUE(DidBetter({5, 1.0f}, {5, 2.0f}));
	EXPECT_FALSE(DidBetter({5, 2.0f}, {5, 1.0f}));
	EXPECT_FALSE(DidBetter({5, 2.0f}, {5, 2.0f}));
	EXPECT_TRUE(DidBetter({5, std::numeric_limits<float>::quiet_NaN()}, {5, 2.0f}));
	EXPECT_FALSE(DidBetter(k_TrialGivenUp, {0, 1.0e30f}));
}

TEST(WallHugBookkeeping, TurningRoundMatchesTheOriginal)
{
	// The original's answers for these walkers: state, angle, the place it jumped to and its new step
	struct Case
	{
		WalkPose in;
		WalkPose out;
	};
	const std::array<Case, 4> k_Cases {{
	    {{WalkState::LinearAnticlockwise, 100, {1000000, 2000000}, {1234, -567}, 0.0f},
	     {WalkState::LinearClockwise, 1124, {1003702, 1998299}, {-1234, 0}, 567.0f}},
	    {{WalkState::LinearClockwise, 2000, {-5, 7}, {-300, 4000}, 2.75f},
	     {WalkState::LinearAnticlockwise, 976, {-905, 12007}, {300, -2}, -4000.0f}},
	    {{WalkState::ExitAnticlockwise, 1023, {65536, 65536}, {0, -1}, -0.5f},
	     {WalkState::ExitClockwise, 2047, {65536, 65533}, {0, 0}, 1.0f}},
	    {{WalkState::ExitClockwise, 1024, {0, 0}, {7, 8}, -3.5f}, {WalkState::ExitAnticlockwise, 0, {21, 24}, {-7, 3}, -8.0f}},
	}};
	for (const auto& c : k_Cases)
	{
		const auto turned = TurnRound(c.in);
		EXPECT_TRUE(turned.turned);
		EXPECT_FALSE(turned.lookAlongCircle);
		EXPECT_EQ(turned.pose.state, c.out.state);
		EXPECT_EQ(turned.pose.angle, c.out.angle);
		EXPECT_EQ(turned.pose.position, c.out.position);
		EXPECT_EQ(turned.pose.step, c.out.step);
		EXPECT_EQ(std::bit_cast<uint32_t>(turned.pose.stepAltitude), std::bit_cast<uint32_t>(c.out.stepAltitude));
	}
}

TEST(WallHugBookkeeping, AWalkerGoingRoundTurnsRoundAndLooksAlongItsCircleAgain)
{
	const WalkPose pose {WalkState::OrbitClockwise, 10, {0, 0}, {1, 2}, 0.0f};
	const auto turned = TurnRound(pose);
	EXPECT_TRUE(turned.turned);
	EXPECT_TRUE(turned.lookAlongCircle);
	EXPECT_EQ(turned.pose.state, WalkState::OrbitAnticlockwise);
	EXPECT_EQ(TurnRound({WalkState::OrbitAnticlockwise, 10, {0, 0}, {1, 2}, 0.0f}).pose.state, WalkState::OrbitClockwise);
}

TEST(WallHugBookkeeping, AWalkerThatIsNotHuggingDoesNotTurnRound)
{
	for (const auto state : {WalkState::Linear, WalkState::StepThrough, WalkState::Arrived})
	{
		const WalkPose pose {state, 5, {1, 1}, {2, 3}, 1.0f};
		const auto turned = TurnRound(pose);
		EXPECT_FALSE(turned.turned);
		EXPECT_EQ(turned.pose.state, state);
		EXPECT_EQ(turned.pose.angle, 5);
		EXPECT_EQ(turned.pose.position, glm::ivec2(1, 1));
		EXPECT_EQ(turned.pose.step, glm::ivec2(2, 3));
	}
}

TEST(WallHugBookkeeping, AKeptWalkerGoingRoundFacesAlongItsCircle)
{
	EXPECT_EQ(HeadingWhenKept(WalkState::OrbitClockwise, 100), uint16_t {612});
	EXPECT_EQ(HeadingWhenKept(WalkState::OrbitClockwise, 1800), uint16_t {264});
	EXPECT_EQ(HeadingWhenKept(WalkState::OrbitAnticlockwise, 100), uint16_t {1636});
	EXPECT_EQ(HeadingWhenKept(WalkState::LinearClockwise, 100), std::nullopt);
}

TEST(WallHugBookkeeping, AnAnimalGoingRoundAnOverlappedThingIsToldToLeaveAtOnce)
{
	using openblack::ecs::components::AnimalBrain;
	const auto building = static_cast<entt::entity>(7);
	const auto neighbour = static_cast<entt::entity>(8);
	const auto other = static_cast<entt::entity>(9);
	const std::array overlapping {building, neighbour};

	AnimalBrain round;
	round.hugCircle = {.centre = {10.0f, 10.0f}, .radius = 4.0f, .owner = neighbour, .set = true};
	round.hugGoalDistance = 1200;
	EXPECT_TRUE(openblack::ecs::animal_ai::detail::MarkLeaveAtOnce(round, overlapping));
	EXPECT_EQ(round.hugGoalDistance, k_LeaveAtOnce);

	// another thing's circle, water (no owner) and no circle at all are left alone
	AnimalBrain elsewhere;
	elsewhere.hugCircle = {.centre = {0.0f, 0.0f}, .radius = 2.0f, .owner = other, .set = true};
	elsewhere.hugGoalDistance = 300;
	EXPECT_FALSE(openblack::ecs::animal_ai::detail::MarkLeaveAtOnce(elsewhere, overlapping));
	EXPECT_EQ(elsewhere.hugGoalDistance, 300u);
	AnimalBrain water;
	water.hugCircle = {.centre = {5.0f, 5.0f}, .radius = 7.2f, .owner = entt::null, .set = true};
	EXPECT_FALSE(openblack::ecs::animal_ai::detail::MarkLeaveAtOnce(water, overlapping));
	AnimalBrain none;
	none.hugCircle.owner = building;
	EXPECT_FALSE(openblack::ecs::animal_ai::detail::MarkLeaveAtOnce(none, overlapping));
	// so far that the orbit's leave test always passes on its distance
	EXPECT_GT(static_cast<double>(k_LeaveAtOnce) * 6.103515625e-05 * static_cast<double>(k_LeaveAtOnce), 1.0e14);
}
