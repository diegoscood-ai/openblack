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
#include <numbers>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::creature_locomotion;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;

float Degrees(float degrees)
{
	return degrees * k_Pi / 180.0f;
}

// A made-up species: a stand of 2 s, a walk of 2.4 s striding 70 mesh units, a run of 1 s striding 130
constexpr Cycle k_Stand {.durationMs = 2000.0f, .stride = 0.0f};
constexpr Cycle k_Walk {.durationMs = 2400.0f, .stride = 70.0f};
constexpr Cycle k_Run {.durationMs = 1000.0f, .stride = 130.0f};
} // namespace

TEST(CreatureLocomotion, SpeedsGrowWithSize)
{
	const auto one = SpeedsFor(1.0f);
	EXPECT_NEAR(one.walk, 8.0f * 1.125f, k_Tolerance);
	EXPECT_NEAR(one.run, 20.0f * 1.125f, k_Tolerance);
	const auto two = SpeedsFor(2.0f);
	EXPECT_NEAR(two.walk, 8.0f * 2.0f, k_Tolerance);
	// The size is taken as it is given, not kept within 0.05 and 4
	EXPECT_NEAR(SpeedsFor(10.0f).run, 20.0f * 9.0f, k_Tolerance);
	EXPECT_NEAR(SpeedsFor(0.0f).walk, 8.0f * 0.25f, k_Tolerance);
}

TEST(CreatureLocomotion, InItsPenItGoesAtThePenSizesSpeeds)
{
	// drawn at 0.22 in its temple's pen: 0.875 * 0.22 + 0.25 = 0.4425 a unit, whatever its own size
	const auto pen = SpeedsFor(0.22f);
	EXPECT_FLOAT_EQ(pen.walk, 8.0f * ((0.875f * 0.22f) + 0.25f));
	EXPECT_FLOAT_EQ(pen.run, 20.0f * ((0.875f * 0.22f) + 0.25f));
	EXPECT_NEAR(pen.walk, 3.54f, k_Tolerance);
	EXPECT_NEAR(pen.run, 8.85f, k_Tolerance);
}

TEST(CreatureLocomotion, InItsPenTheWalkStridesAtThePenScale)
{
	// the same walk at the same speed fraction, by a body drawn at its own scale and at its pen's: the stride shrinks
	// with the drawn scale, so a smaller body's feet cycle faster over the same distance
	const auto own = SpeedsFor(2.0f);
	const auto pen = SpeedsFor(0.22f);
	constexpr float k_OwnScale = 0.2f;
	constexpr float k_PenScale = k_OwnScale * 0.22f / 2.0f;
	constexpr float k_Turn = 0.1f;
	const auto big = BlendGait(own.walk * 0.5f, own, own.walk * 0.5f * k_Turn, k_OwnScale, k_Stand, k_Walk, k_Run, 0.0f, 0.0f);
	const auto small =
	    BlendGait(pen.walk * 0.5f, pen, pen.walk * 0.5f * k_Turn, k_PenScale, k_Stand, k_Walk, k_Run, 0.0f, 0.0f);
	// advance = walk duration x distance / (weight x walk stride x drawn scale), in our order (the original's: Pending)
	EXPECT_FLOAT_EQ(small.walkAdvanceMs, k_Walk.durationMs * (pen.walk * 0.5f * k_Turn) / (0.5f * k_Walk.stride * k_PenScale));
	// the feet cycle in proportion to (0.875 s + 0.25) / s: 2.011 in the pen against 1 at size 2
	EXPECT_NEAR(small.walkAdvanceMs / big.walkAdvanceMs, (0.4425f / 0.22f) / (2.0f / 2.0f), 1e-3f);
}

TEST(CreatureLocomotion, TargetSpeedIsAFractionOfALittleOverRunning)
{
	EXPECT_NEAR(TargetSpeed(1.0f, 20.0f), 22.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(0.5f, 20.0f), 11.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(2.0f, 20.0f), 22.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(-1.0f, 20.0f), 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, ExhaustedCreaturesGoSlowly)
{
	EXPECT_NEAR(RequiredFraction(0.9f, 0.5f, 0.79f), 0.9f, k_Tolerance);
	EXPECT_NEAR(RequiredFraction(0.9f, 0.5f, 0.8f), 0.5f, k_Tolerance);
}

TEST(CreatureLocomotion, AcceleratesAtTwelveUnitsASecondEachSecond)
{
	EXPECT_NEAR(Accelerate(0.0f, 10.0f, 0.1f), 1.2f, k_Tolerance);
	EXPECT_NEAR(Accelerate(9.5f, 10.0f, 0.1f), 10.0f, k_Tolerance);
	// It never slows down towards a lower target by itself
	EXPECT_NEAR(Accelerate(15.0f, 10.0f, 0.1f), 15.0f, k_Tolerance);
	// From standing it takes nine turns to reach 10
	float speed = 0.0f;
	int turns = 0;
	while (speed < 10.0f)
	{
		speed = Accelerate(speed, 10.0f, 0.1f);
		++turns;
	}
	EXPECT_EQ(turns, 9);
}

TEST(CreatureLocomotion, BrakesToStopInTheDistanceLeft)
{
	EXPECT_NEAR(StopCap(6.0f), 12.0f, k_Tolerance);
	EXPECT_NEAR(StopCap(0.0f), k_MinSpeed, k_Tolerance);
	EXPECT_NEAR(CornerCap(1.0f, 2.0f), std::sqrt(24.0f + 24.0f), k_Tolerance);
	// Braking each turn to the cap stops it right at the end
	float remaining = 30.0f;
	float speed = 20.0f;
	while (remaining > 0.0f && speed > k_MinSpeed)
	{
		speed = std::min(speed, StopCap(remaining));
		remaining -= speed * 0.1f;
	}
	EXPECT_GT(remaining, -1.5f);
}

TEST(CreatureLocomotion, UphillIsSlowerDownhillALittleFaster)
{
	EXPECT_NEAR(SlopeFactor(10.0f, 10.0f, 5.0f), 1.0f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(12.5f, 10.0f, 5.0f), 0.7f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(100.0f, 10.0f, 5.0f), k_MinSlopeFactor, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(0.0f, 10.0f, 5.0f), k_MaxSlopeFactor, k_Tolerance);
}

TEST(CreatureLocomotion, HeadingsFaceTheWayTheyPoint)
{
	for (const float heading : {0.0f, 1.0f, -2.0f, 3.0f})
	{
		EXPECT_NEAR(HeadingOf(DirectionOf(heading)), heading, k_Tolerance);
	}
	// Heading 0 looks along -z, as the meshes do
	EXPECT_NEAR(DirectionOf(0.0f).y, -1.0f, k_Tolerance);
	EXPECT_NEAR(WrapAngle(Degrees(270.0f)), Degrees(-90.0f), k_Tolerance);
	EXPECT_NEAR(WrapAngle(Degrees(-190.0f)), Degrees(170.0f), k_Tolerance);
}

// A step follows the route's turns with its heading, one turn's change at a time, and ends when the route turns more
// than a corner in one turn: never against where the step began
TEST(CreatureLocomotion, AStepsHeadingFollowsTheRouteTurnByTurn)
{
	// 20 degrees a turn, three turns: 60 degrees in all, never more than a corner in one turn
	float heading = Degrees(-90.0f);
	float route = 0.0f;
	for (int turn = 1; turn <= 3; ++turn)
	{
		const float next = Degrees(20.0f * static_cast<float>(turn));
		const auto followed = StepHeadingFollowingRoute(heading, route, next);
		ASSERT_TRUE(followed.has_value()) << turn;
		EXPECT_NEAR(*followed, heading + Degrees(20.0f), k_Tolerance) << turn;
		heading = *followed;
		route = next;
	}
	// across the turn from pi to -pi the change is the short way
	const auto across = StepHeadingFollowingRoute(0.0f, Degrees(175.0f), Degrees(-175.0f));
	ASSERT_TRUE(across.has_value());
	EXPECT_NEAR(*across, Degrees(10.0f), k_Tolerance);
	// a corner is 30 degrees: one turn of it still steps, more ends the step
	EXPECT_TRUE(StepHeadingFollowingRoute(0.0f, 0.0f, Degrees(29.9f)).has_value());
	EXPECT_FALSE(StepHeadingFollowingRoute(0.0f, 0.0f, Degrees(30.1f)).has_value());
	EXPECT_FALSE(StepHeadingFollowingRoute(0.0f, Degrees(10.0f), Degrees(-25.0f)).has_value());
}

TEST(CreatureLocomotion, SlowerThanWalkingBlendsStandAndWalk)
{
	const Speeds speeds {.walk = 9.0f, .run = 22.5f};
	const auto gait = BlendGait(4.5f, speeds, 0.45f, 0.2f, k_Stand, k_Walk, k_Run, 0.0f, 0.25f);
	EXPECT_EQ(gait.slots[0].animation, animations::k_Stand);
	EXPECT_EQ(gait.slots[1].animation, animations::k_Walk);
	EXPECT_NEAR(gait.slots[0].weight, 0.5f, k_Tolerance);
	EXPECT_NEAR(gait.slots[1].weight, 0.5f, k_Tolerance);
	// The stand follows the breath
	EXPECT_NEAR(gait.slots[0].timeMs, 500.0f, k_Tolerance);
	// The blend's stride is half the walk's: 0.5 * 70 * 0.2 = 7 units, so 0.45 units is 0.45 / 7 of the walk
	EXPECT_NEAR(gait.walkAdvanceMs, 2400.0f * 0.45f / 7.0f, 1e-2f);
	EXPECT_NEAR(gait.slots[1].timeMs, gait.walkAdvanceMs, 1e-2f);
}

TEST(CreatureLocomotion, FasterThanWalkingBlendsWalkAndRunInStep)
{
	const Speeds speeds {.walk = 10.0f, .run = 20.0f};
	const auto gait = BlendGait(15.0f, speeds, 1.5f, 0.1f, k_Stand, k_Walk, k_Run, 2000.0f, 0.0f);
	EXPECT_EQ(gait.slots[0].animation, animations::k_Run);
	EXPECT_EQ(gait.slots[1].animation, animations::k_Walk);
	EXPECT_NEAR(gait.slots[0].weight, 0.5f, k_Tolerance);
	EXPECT_NEAR(gait.slots[1].weight, 0.5f, k_Tolerance);
	// Stride (0.5 * 70 + 0.5 * 130) * 0.1 = 10 units, so 1.5 units is 15% of the walk, which loops
	EXPECT_NEAR(gait.walkAdvanceMs, 360.0f, 1e-2f);
	EXPECT_NEAR(gait.walkTimeMs, 2360.0f, 1e-2f);
	// The run keeps the walk's phase
	EXPECT_NEAR(gait.slots[0].timeMs / k_Run.durationMs, gait.slots[1].timeMs / k_Walk.durationMs, k_Tolerance);
	// Past running speed the weights stay within 0 and 1
	const auto flatOut = BlendGait(22.0f, speeds, 2.2f, 0.1f, k_Stand, k_Walk, k_Run, 0.0f, 0.0f);
	EXPECT_NEAR(flatOut.slots[0].weight, 1.0f, k_Tolerance);
	EXPECT_NEAR(flatOut.slots[1].weight, 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, StandingStillMovesNoFeet)
{
	const auto gait = BlendGait(0.0f, {.walk = 9.0f, .run = 22.0f}, 0.0f, 0.2f, k_Stand, k_Walk, k_Run, 300.0f, 0.5f);
	EXPECT_NEAR(gait.slots[0].weight, 1.0f, k_Tolerance);
	EXPECT_NEAR(gait.walkTimeMs, 300.0f, k_Tolerance);
	EXPECT_NEAR(gait.walkAdvanceMs, 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, AnglesPickTheirPairOfSteps)
{
	const auto ahead = PairFor(animations::k_RightStep, 0.0f);
	EXPECT_EQ(ahead.from, animations::k_RightStep);
	EXPECT_NEAR(ahead.weight, 0.0f, k_Tolerance);
	const auto side = PairFor(animations::k_RightStep, Degrees(45.0f));
	EXPECT_EQ(side.from, animations::k_RightStep);
	EXPECT_EQ(side.to, animations::k_RightStep + 1);
	EXPECT_NEAR(side.weight, 0.5f, k_Tolerance);
	const auto behind = PairFor(animations::k_RightStep, Degrees(-135.0f));
	EXPECT_EQ(behind.from, animations::k_RightStep + 1);
	EXPECT_EQ(behind.to, animations::k_RightStep + 2);
	EXPECT_NEAR(behind.weight, 0.5f, k_Tolerance);
}

TEST(CreatureLocomotion, StartsByStepWalkOrTurn)
{
	const StartOptions steps {.stepStrides = std::array {46.0f, 42.0f, 40.0f}, .moving = true};
	// Far enough for every step: it steps off, right for positive angles
	const auto step = ChooseStart(Degrees(100.0f), 50.0f, steps);
	EXPECT_EQ(step.kind, Start::Kind::Step);
	EXPECT_EQ(step.side, Side::Right);
	ASSERT_TRUE(step.animations.has_value());
	EXPECT_EQ(step.animations->from, animations::k_RightStep + 1);
	const auto left = ChooseStart(Degrees(-30.0f), 50.0f, steps);
	EXPECT_EQ(left.side, Side::Left);
	EXPECT_EQ(left.animations->from, animations::k_LeftStep);
	// Too close for a step: nearly facing it walks straight on, otherwise turns on the spot
	EXPECT_EQ(ChooseStart(Degrees(10.0f), 45.0f, steps).kind, Start::Kind::Walk);
	const auto turn = ChooseStart(Degrees(-60.0f), 45.0f, steps);
	EXPECT_EQ(turn.kind, Start::Kind::Turn);
	// A turn's spins are SpinsFor's, not the start's
	EXPECT_FALSE(turn.animations.has_value());
	EXPECT_FALSE(ChooseStart(Degrees(60.0f), 10.0f, {.stepStrides = std::nullopt, .moving = true}).animations.has_value());
	// Without its side's steps it turns even when nearly facing the way
	EXPECT_EQ(ChooseStart(Degrees(10.0f), 45.0f, {.stepStrides = std::nullopt, .moving = true}).kind, Start::Kind::Turn);
	EXPECT_EQ(ChooseStart(Degrees(1.0f), 0.0f, {.stepStrides = std::nullopt, .moving = false}).kind, Start::Kind::Turn);
	// Only turning to face, it never steps
	EXPECT_EQ(ChooseStart(Degrees(100.0f), 500.0f, {.stepStrides = steps.stepStrides, .moving = false}).kind,
	          Start::Kind::Turn);
}

namespace
{
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

/// The side's 0, 90 and 180 degree spins lasting these lengths, from its first spin; the other side's are missing
auto SpinLengths(size_t first, std::array<int32_t, 3> lengths)
{
	return [first, lengths](size_t index) -> std::optional<int32_t> {
		if (index < first || index >= first + lengths.size())
		{
			return std::nullopt;
		}
		return lengths.at(index - first);
	};
}

/// A turn from one yaw to another, the spins it plays and their blend, as the original gives them
struct SpinCase
{
	uint32_t yaw;
	uint32_t target;
	size_t first;
	std::array<int32_t, 3> lengths;
	size_t from;
	uint32_t weight;
	int32_t durationMs;
};
// Each case's weight is the original's for the yaws, its length the original's for the weight and the lengths
constexpr std::array k_SpinCases {
    // a positive angle plays the left spins: the weight is the angle times the float nearest 2 / pi, and the blend's
    // length 1200 + 501 x 0.50000006 = 1450.50003 is rounded to a float, 1450.5, then to the even 1450
    SpinCase {0x00000000, 0x3F490FDC, animations::k_LeftSpin, {1200, 1701, 1}, animations::k_LeftSpin, 0x3F000001, 1450},
    SpinCase {0x00000000, 0x3F490FDB, animations::k_LeftSpin, {1200, 1703, 1}, animations::k_LeftSpin, 0x3F000000, 1452},
    // a negative angle the right spins, by the angle negated
    SpinCase {0x00000000, 0xBF490FDB, animations::k_RightSpin, {1200, 1701, 1}, animations::k_RightSpin, 0x3F000000, 1450},
    SpinCase {0x00000000, 0xBF000000, animations::k_RightSpin, {1200, 1701, 1}, animations::k_RightSpin, 0x3EA2F983, 1359},
    // beyond a quarter turn the 90 and 180 degree spins, by what is beyond it; the angle within half a turn
    SpinCase {0x3F800000, 0xC0200000, animations::k_LeftSpin, {1, 1200, 1701}, animations::k_LeftSpin + 1, 0x3F4596B5, 1587},
    SpinCase {0x00000000, 0xC0200000, animations::k_RightSpin, {1, 1200, 1701}, animations::k_RightSpin + 1, 0x3F176FC8, 1496},
    SpinCase {0xC0400000, 0x40400000, animations::k_RightSpin, {1200, 1701, 1}, animations::k_RightSpin, 0x3E389BB5, 1290},
    // no angle at all turns by the right spins
    SpinCase {0x00000000, 0x00000000, animations::k_RightSpin, {1200, 1701, 1}, animations::k_RightSpin, 0x80000000, 1200},
    // just below a quarter turn, at it, and half a turn
    SpinCase {0x00000000, 0x3FC90FDA, animations::k_LeftSpin, {1000, 1000, 1}, animations::k_LeftSpin, 0x3F7FFFFF, 1000},
    SpinCase {0x00000000, 0x3FC90FDB, animations::k_LeftSpin, {1, 1200, 1701}, animations::k_LeftSpin + 1, 0x00000000, 1200},
    SpinCase {0x00000000, 0x40490FDB, animations::k_LeftSpin, {1, 1200, 1701}, animations::k_LeftSpin + 1, 0x3F800000, 1701},
    // a multiplication, not a division by a quarter turn (which gives 0x3908509C)
    SpinCase {0x00000000, 0x39561F77, animations::k_LeftSpin, {1000, 1000, 1}, animations::k_LeftSpin, 0x3908509B, 1000},
};
} // namespace

TEST(CreatureLocomotion, ATurnOnTheSpotBlendsTheSidesSpinsByTheAngleAsTheOriginal)
{
	for (const auto& c : k_SpinCases)
	{
		const auto spins = SpinsFor(Bits(c.yaw), Bits(c.target), SpinLengths(c.first, c.lengths));
		ASSERT_TRUE(spins.has_value()) << std::hex << c.target;
		EXPECT_EQ(spins->from, c.from) << std::hex << c.target;
		EXPECT_EQ(spins->to, c.from + 1) << std::hex << c.target;
		EXPECT_EQ(std::bit_cast<uint32_t>(spins->weight), c.weight) << std::hex << c.target;
		EXPECT_EQ(spins->durationMs, c.durationMs) << std::hex << c.target;
	}
}

TEST(CreatureLocomotion, ATurnThatIsNotANumberTakesTheFirstPairAndTheIndefiniteLength)
{
	const auto spins =
	    SpinsFor(0.0f, std::numeric_limits<float>::quiet_NaN(), SpinLengths(animations::k_RightSpin, {1200, 1701, 1}));
	ASSERT_TRUE(spins.has_value());
	EXPECT_EQ(spins->from, animations::k_RightSpin);
	EXPECT_TRUE(std::isnan(spins->weight));
	EXPECT_EQ(spins->durationMs, std::numeric_limits<int32_t>::min());
}

TEST(CreatureLocomotion, WithoutAllThreeOfTheSidesSpinsATurnHasNone)
{
	// the left spins are asked for, and only the right ones are there
	EXPECT_FALSE(SpinsFor(0.0f, 1.0f, SpinLengths(animations::k_RightSpin, {1200, 1701, 1})).has_value());
	const auto two = [](size_t index) -> std::optional<int32_t> {
		return index == animations::k_LeftSpin + 2 ? std::nullopt : std::optional<int32_t>(1000);
	};
	EXPECT_FALSE(SpinsFor(0.0f, 1.0f, two).has_value());
}

TEST(CreatureLocomotion, ATurnToFaceIsOverWithinATenThousandthOfItsTarget)
{
	EXPECT_TRUE(TurnIsOver(0.0f, 0.0f));
	// the float nearest 1e-4 is not within, the one below it is
	EXPECT_FALSE(TurnIsOver(0.0f, Bits(0x38D1B717)));
	EXPECT_TRUE(TurnIsOver(0.0f, Bits(0x38D1B716)));
	EXPECT_FALSE(TurnIsOver(1.0f, 1.0001f));
	EXPECT_TRUE(TurnIsOver(1.0f, 1.00009f));
	EXPECT_TRUE(TurnIsOver(0.5f, 0.4999f));
	// the difference is not wrapped round
	EXPECT_FALSE(TurnIsOver(3.1f, -3.1f));
	EXPECT_TRUE(TurnIsOver(0.0f, std::numeric_limits<float>::quiet_NaN()));
}

TEST(CreatureLocomotion, WithoutSpinsTheYawComesRoundByTheShareOfTheStandGoneOfWhatIsLeft)
{
	// at the start nothing has gone yet
	EXPECT_EQ(SpinlessYaw(0.0f, 1.0f, 0.0f, 1000.0f), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(SpinlessYaw(0.0f, 1.0f, 33.0f, 1000.0f)), 0x3D072B02u);
	EXPECT_EQ(std::bit_cast<uint32_t>(SpinlessYaw(0.1f, 2.0f, 500.0f, 1000.0f)), 0x3F866666u);
	// the way round within half a turn
	EXPECT_EQ(std::bit_cast<uint32_t>(SpinlessYaw(0.3f, -2.9f, 400.0f, 1200.0f)), 0x3FA9F302u);
	EXPECT_EQ(std::bit_cast<uint32_t>(SpinlessYaw(3.0f, -3.0f, 100.0f, 900.0f)), 0x40420386u);
}

// The step's heading sum and the walk's corner wrap each difference once, as the original does: an angle within -pi
// to pi is kept bit for bit (the numbers are the original's, from its own angle wrap)
TEST(CreatureLocomotion, TheStepsHeadingAndTheWalksCornerWrapAsTheOriginal)
{
	// 0.1 + (0.25 - 0.2), all in floats and never rounded again by the wrap
	const auto followed = StepHeadingFollowingRoute(0.1f, 0.2f, 0.25f);
	ASSERT_TRUE(followed.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(*followed), 0x3E19999Au);
	// a turn of exactly a corner in one turn still steps, by exactly that much
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CornerAngle), 0x3F060A92u);
	const auto corner = StepHeadingFollowingRoute(0.0f, 0.0f, k_CornerAngle);
	ASSERT_TRUE(corner.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(*corner), 0x3F060A92u);
	// across pi: -3.1 - 3 wraps to 0x3E3B9500, and 3.1 plus that to -3
	const auto across = StepHeadingFollowingRoute(3.1f, 3.0f, -3.1f);
	ASSERT_TRUE(across.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(*across), 0xC0400000u);

	// the walk stops on a corner or more
	EXPECT_TRUE(MeetsCorner(0.0f, k_CornerAngle));
	EXPECT_TRUE(MeetsCorner(0.0f, -k_CornerAngle));
	EXPECT_FALSE(MeetsCorner(0.0f, Bits(0x3F060A91)));
	// across pi: -3 from 3 is 0.28 round the short way, -2.6 from 3 is 0x3F2EE540
	EXPECT_FALSE(MeetsCorner(3.0f, -3.0f));
	EXPECT_TRUE(MeetsCorner(3.0f, -2.6f));
}

// The heading of a direction as the original works it out, minus its yaw (the original's own numbers), and 0 for a
// direction no longer than about 1e-3
TEST(CreatureLocomotion, AHeadingFromADirectionIsTheOriginals)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingFromDirection({3.0f, 4.0f})), 0xC01FE0BCu);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingFromDirection({10.0f, 0.0f})), 0xBFC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingFromDirection({-7.0f, -24.0f})), 0x3E914D76u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingFromDirection({0.0008f, 0.0008f})), 0xC016CBE4u);
	// x x + z z = 5e-7: no way to tell
	EXPECT_EQ(HeadingFromDirection({0.0005f, 0.0005f}), 0.0f);
	EXPECT_EQ(HeadingFromDirection({0.0f, 0.0f}), 0.0f);
}

// A walk sets off for where the route's heading at the follower leads over the whole node it is on, not for the
// node's end: the sine and minus the cosine of that heading times the node's length, at least 0.01, each product
// rounded to a float and added to the position
TEST(CreatureLocomotion, AWalkSetsOffAlongTheRoutesHeadingForTheWholeNode)
{
	const auto along = RouteStartTarget({100.0f, 100.0f}, 0.7f, 12.5f);
	EXPECT_EQ(std::bit_cast<uint32_t>(along.x), 0x42D81AFEu);
	EXPECT_EQ(std::bit_cast<uint32_t>(along.y), 0x42B4E102u);
	// a node shorter than 0.01, or of no length that is a number, counts as 0.01
	const auto shortest = RouteStartTarget({-35.25f, 410.5f}, -2.9f, 0.004f);
	EXPECT_EQ(std::bit_cast<uint32_t>(shortest.x), 0xC20D0273u);
	EXPECT_EQ(std::bit_cast<uint32_t>(shortest.y), 0x43CD413Eu);
	const auto unknown = RouteStartTarget({-35.25f, 410.5f}, -2.9f, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(unknown, shortest);
	// the route's heading is the follower's own: pi leads along +z
	const auto back = RouteStartTarget({250.0f, 250.0f}, 3.1415927f, 7.3f);
	EXPECT_EQ(back.x, 250.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(back.y), 0x4380A666u);
	// the heading it sets off with is that way's, as the original works it out
	EXPECT_NEAR(HeadingFromDirection(along - glm::vec2(100.0f, 100.0f)), -0.7f, 1e-5f);
}

// A step off over, the walk is taken up as far through as the step had got, half a walk back after a left step, in
// floats and cut to whole milliseconds towards zero
TEST(CreatureLocomotion, AfterAStepTheWalkIsTakenUpByHowFarThroughTheStepItWas)
{
	EXPECT_EQ(WalkTimeAfterStep(900.0f, 1000.0f, 2400.0f, Side::Right), 2160.0f);
	// 0.4 as a float, times 2400, is 959.99994
	EXPECT_EQ(WalkTimeAfterStep(900.0f, 1000.0f, 2400.0f, Side::Left), 959.0f);
	// before half the step a left step's walk time is below zero, cut towards zero
	EXPECT_EQ(WalkTimeAfterStep(300.0f, 1000.0f, 2400.0f, Side::Left), -479.0f);
	EXPECT_EQ(WalkTimeAfterStep(700.0f, 1100.0f, 1300.0f, Side::Right), 827.0f);
	EXPECT_EQ(WalkTimeAfterStep(700.0f, 1100.0f, 1300.0f, Side::Left), 177.0f);
	EXPECT_EQ(WalkTimeAfterStep(700.0f, 0.0f, 1300.0f, Side::Right), 0.0f);
}

TEST(CreatureLocomotion, ArrivalRings)
{
	const auto ring = MoveRing(50.0f, 0.0f, 1.0f);
	EXPECT_NEAR(ring.min, 0.0f, k_Tolerance);
	EXPECT_NEAR(ring.max, 1.0f, k_Tolerance);
	// Never asked to stop beyond where it already is, and never thinner than a hair
	const auto close = MoveRing(3.0f, 10.0f, 2.0f);
	EXPECT_NEAR(close.min, 2.95f, k_Tolerance);
	EXPECT_NEAR(close.max, 2.951f, k_Tolerance);
	const auto object = ObjectRing(5.0f, 2.0f, 3.0f);
	EXPECT_NEAR(object.min, 7.0f, k_Tolerance);
	EXPECT_NEAR(object.max, 10.0f, k_Tolerance);
	EXPECT_TRUE(Arrived(9.0f, object));
	EXPECT_FALSE(Arrived(10.5f, object));
	EXPECT_NEAR(MaxRouteLength(100.0f), 5200.0f, k_Tolerance);
}

TEST(CreatureLocomotion, TheSearchingCreaturesShareThirtyTwoSearchTurnsATurn)
{
	// with none searching as the turn starts, a creature that starts a search later gets them all
	EXPECT_EQ(SearchTurnsShare(0), 32);
	EXPECT_EQ(SearchTurnsShare(1), 32);
	EXPECT_EQ(SearchTurnsShare(2), 16);
	// the share is rounded down, and is at least one turn
	EXPECT_EQ(SearchTurnsShare(3), 10);
	EXPECT_EQ(SearchTurnsShare(32), 1);
	EXPECT_EQ(SearchTurnsShare(33), 1);
	EXPECT_EQ(SearchTurnsShare(100), 1);
}

TEST(CreatureLocomotion, ThePlanningEndsReadyWithARouteFailedWithNoWayThereAndWaitsOtherwise)
{
	// a route, whatever the last end
	EXPECT_EQ(PlanningOutcome(true, false, -1), Planning::Ready);
	EXPECT_EQ(PlanningOutcome(true, false, 1), Planning::Ready);
	// no end yet
	EXPECT_EQ(PlanningOutcome(false, false, -1), Planning::Searching);
	// a failed plan with a newer destination still to plan, or a re-plan under way: it waits for that plan
	EXPECT_EQ(PlanningOutcome(false, false, 1), Planning::Searching);
	EXPECT_EQ(PlanningOutcome(false, false, 2), Planning::Searching);
	// the destination inside an obstacle or a failed plan, with nothing left to plan, or given up
	EXPECT_EQ(PlanningOutcome(false, false, 0), Planning::Failed);
	EXPECT_EQ(PlanningOutcome(false, false, 3), Planning::Failed);
	EXPECT_EQ(PlanningOutcome(false, true, -1), Planning::Failed);
}

TEST(CreatureLocomotion, RunsAwayFromTheThreat)
{
	const auto point = RunAwayPoint({10.0f, 0.0f}, {0.0f, 0.0f}, 50.0f);
	EXPECT_NEAR(point.x, 60.0f, k_Tolerance);
	EXPECT_NEAR(point.y, 0.0f, k_Tolerance);
	EXPECT_NEAR(glm::length(RunAwayPoint({3.0f, 3.0f}, {3.0f, 3.0f}, 20.0f) - glm::vec2(3.0f, 3.0f)), 20.0f, k_Tolerance);
}

TEST(CreatureLocomotion, ALegTrackLoopsRoundOrHoldsWithinItsLength)
{
	// a walk loops round its length; a step holds short of its end
	EXPECT_FLOAT_EQ(TrackTime(900.0f, 300.0f, 1000.0f, true, 1.0f), 200.0f);
	EXPECT_FLOAT_EQ(TrackTime(900.0f, 300.0f, 1000.0f, true, 0.5f), 50.0f);
	EXPECT_FLOAT_EQ(TrackTime(900.0f, 300.0f, 1000.0f, false, 1.0f), 999.0f);
	EXPECT_FLOAT_EQ(TrackTime(100.0f, 300.0f, 0.0f, true, 1.0f), 0.0f);
}

namespace
{
uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// Two steps blended, the blend's move, and its stopping distance at two scales and places along the node, as the
/// original works them out
struct StepCase
{
	StepMove from;
	StepMove to;
	float weight;
	std::array<uint32_t, 3> move;
	uint32_t stopAtOnePointThreeAfterFourTenths;
	uint32_t stopAtThreeQuartersAfterSevenAndAHalf;
};

constexpr std::array k_StepCases {
    StepCase {.from = {.move = {0.5f, 0.05f, -1.9f}, .stride = 2.0f},
              .to = {.move = {1.7f, 0.0f, -2.6f}, .stride = 3.1f},
              .weight = 0.37f,
              .move = {0x3F76D5FBu, 0x3D03C906u, 0xC00D2213u},
              .stopAtOnePointThreeAfterFourTenths = 0x402EA993u,
              .stopAtThreeQuartersAfterSevenAndAHalf = 0xC0B63B64u},
    StepCase {.from = {.move = {0.0f, 0.0f, -5.0f}, .stride = 5.0f},
              .to = {.move = {3.0f, 0.0f, -4.0f}, .stride = 5.0f},
              .weight = 0.5f,
              .move = {0x3FCA62C3u, 0x00000000u, 0xC097CA12u},
              .stopAtOnePointThreeAfterFourTenths = 0x40C33333u,
              .stopAtThreeQuartersAfterSevenAndAHalf = 0xC0700000u},
    StepCase {.from = {.move = {0.0f, 0.0f, 0.0f}, .stride = 0.0f},
              .to = {.move = {0.0f, 0.0f, 0.0f}, .stride = 0.0f},
              .weight = 0.5f,
              .move = {0x00000000u, 0x00000000u, 0x00000000u},
              .stopAtOnePointThreeAfterFourTenths = 0xBECCCCCDu,
              .stopAtThreeQuartersAfterSevenAndAHalf = 0xC0F00000u},
    StepCase {.from = {.move = {0.3f, 0.2f, -4.1f}, .stride = 4.2f},
              .to = {.move = {-0.8f, 0.1f, -3.7f}, .stride = 3.9f},
              .weight = 0.8125f,
              .move = {0xBF1D499Au, 0x3DFBA8F6u, 0xC07A011Eu},
              .stopAtOnePointThreeAfterFourTenths = 0x4097C7ADu,
              .stopAtThreeQuartersAfterSevenAndAHalf = 0xC0910CCDu},
};
} // namespace

// The numbers are the original's, run on these blends
TEST(CreatureLocomotion, AStepsBlendedMoveTakesTheBlendedStridesLengthAsTheOriginal)
{
	for (const auto& c : k_StepCases)
	{
		const auto move = BlendedMove(c.from, c.to, c.weight);
		EXPECT_EQ(BitsOf(move.x), c.move.at(0)) << c.weight;
		EXPECT_EQ(BitsOf(move.y), c.move.at(1)) << c.weight;
		EXPECT_EQ(BitsOf(move.z), c.move.at(2)) << c.weight;
	}
	// a move not a number counts as none, and is left as blended
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto left =
	    BlendedMove({.move = {nan, 0.0f, 0.0f}, .stride = 1.0f}, {.move = {0.0f, 0.0f, 0.0f}, .stride = 1.0f}, 0.5f);
	EXPECT_TRUE(std::isnan(left.x));
	EXPECT_EQ(left.z, 0.0f);
}

TEST(CreatureLocomotion, WhileItStepsOffItIsBoundToGoTheRestOfItsStepAsTheOriginal)
{
	EXPECT_EQ(BitsOf(k_TurningStopDistance), 0x3D4CCCCDu);
	for (const auto& c : k_StepCases)
	{
		const auto move = BlendedMove(c.from, c.to, c.weight);
		EXPECT_EQ(BitsOf(SteppingStopDistance(move, 1.3f, 0.4f)), c.stopAtOnePointThreeAfterFourTenths) << c.weight;
		EXPECT_EQ(BitsOf(SteppingStopDistance(move, 0.75f, 7.5f)), c.stopAtThreeQuartersAfterSevenAndAHalf) << c.weight;
	}
}

// The numbers are the original's, run on these blends
TEST(CreatureLocomotion, AStepsBlendLastsItsTwoLengthsBlendedToTheNearestMillisecond)
{
	EXPECT_EQ(BlendedLength(1000, 1200, 0.37f), 1074);
	EXPECT_EQ(BlendedLength(1000, 1100, 0.8125f), 1081);
	EXPECT_EQ(BlendedLength(900, 900, 0.5f), 900);
	// a half goes to the even one
	EXPECT_EQ(BlendedLength(1000, 1001, 0.5f), 1000);
	EXPECT_EQ(BlendedLength(1001, 1002, 0.5f), 1002);
}

namespace
{
/// A turn of a step off: its whole move, its scale, the turn's and the step's lengths and the original's yaw, and the
/// speed the original gives its route follower
struct StepTurnCase
{
	std::array<uint32_t, 3> move;
	float scale;
	int32_t turnMs;
	int32_t durationMs;
	float yaw;
	uint32_t speed;
};

constexpr std::array k_StepTurnCases {
    StepTurnCase {.move = {0x3F76D5FBu, 0x3D03C906u, 0xC00D2213u},
                  .scale = 1.3f,
                  .turnMs = 100,
                  .durationMs = 1074,
                  .yaw = 0.7f,
                  .speed = 0x403A728Bu},
    StepTurnCase {.move = {0xBF1D499Au, 0x3DFBA8F6u, 0xC07A011Eu},
                  .scale = 0.75f,
                  .turnMs = 100,
                  .durationMs = 1081,
                  .yaw = -2.9f,
                  .speed = 0x402F95FEu},
    StepTurnCase {.move = {0x3FCA62C3u, 0x00000000u, 0xC097CA12u},
                  .scale = 1.0f,
                  .turnMs = 33,
                  .durationMs = 900,
                  .yaw = 3.1f,
                  .speed = 0x3FEAAAABu},
    StepTurnCase {.move = {0x3F76D5FBu, 0x3D03C906u, 0xC00D2213u},
                  .scale = 2.6f,
                  .turnMs = 100,
                  .durationMs = 1074,
                  .yaw = 0.0f,
                  .speed = 0x40BA728Au},
};
} // namespace

// The numbers are the original's, run on these steps (the first two moves are the step blends above)
TEST(CreatureLocomotion, EachTurnAStepOffGivesItsFollowerTheSpeedOfThatTurnsShareOfItsMoveAsTheOriginal)
{
	for (const auto& c : k_StepTurnCases)
	{
		const glm::vec3 move {Bits(c.move.at(0)), Bits(c.move.at(1)), Bits(c.move.at(2))};
		EXPECT_EQ(BitsOf(StepFollowerSpeed(move, c.scale, c.turnMs, c.durationMs, c.yaw)), c.speed) << c.yaw;
	}
	// its rise or fall is left out
	EXPECT_EQ(StepFollowerSpeed({3.0f, 7.0f, -4.0f}, 1.0f, 100, 1000, 0.0f),
	          StepFollowerSpeed({3.0f, 0.0f, -4.0f}, 1.0f, 100, 1000, 0.0f));
}
