/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "Magic/HandHoldPose.h"

using namespace openblack;
using namespace openblack::magic::hand_hold;

namespace
{
constexpr float k_Epsilon = 1e-4f;
constexpr uint32_t k_HoldSideMs = 533;
constexpr uint32_t k_HoldAboveMs = 533;
constexpr uint32_t k_WiggleMs = 266;

void ExpectNear(glm::vec3 a, glm::vec3 b)
{
	EXPECT_NEAR(a.x, b.x, k_Epsilon);
	EXPECT_NEAR(a.y, b.y, k_Epsilon);
	EXPECT_NEAR(a.z, b.z, k_Epsilon);
}
} // namespace

TEST(HandHoldPose, ASeedIsHeldAsAMiracleNotYetReadyUntilItIsReady)
{
	EXPECT_EQ(HoldTypeOf(false, HoldType::Side), HoldType::Magic);
	EXPECT_EQ(HoldTypeOf(true, HoldType::Side), HoldType::Side);
	EXPECT_EQ(HoldTypeOf(true, HoldType::Above), HoldType::Above);
}

TEST(HandHoldPose, EachHoldTakesAStillFrameOfItsAnimation)
{
	// Changed from his whole milliseconds (66, 54, 83, 197, 194, 222): ours sets the hand's animation to the time
	// unrounded, so the fraction of a millisecond is kept. The animation each hold takes stays with the hand system
	// (HandSystem.cpp), which picks it by the clips it has.
	constexpr float k_Ms = 1e-3f;
	// At the standard size: food (scale 0.8, radius 1), wood (0.65) and the physical shield (1.0) at the side
	EXPECT_NEAR(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 1.0f), 66.5f, k_Ms);
	EXPECT_NEAR(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.65f, 1.0f), 54.03125f, k_Ms);
	EXPECT_NEAR(HoldTimeMs(HoldType::Side, k_HoldSideMs, 1.0f, 1.0f), 83.125f, k_Ms);
	// The forest (1.0) and the flocks (2.6 and 1.6 times a radius of 0.4) above
	EXPECT_NEAR(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 1.0f, 1.0f), 197.0990f, k_Ms);
	EXPECT_NEAR(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 2.6f * 0.4f, 1.0f), 194.3229f, k_Ms);
	EXPECT_NEAR(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 1.6f * 0.4f, 1.0f), 222.0833f, k_Ms);
	// Every miracle not yet ready, and every one without a model, takes the middle of the rest pose
	EXPECT_FLOAT_EQ(HoldTimeMs(HoldType::Magic, k_WiggleMs, 0.8f, 1.0f), 133.0f);
	// A smaller hand, closer to the camera, opens wider round the same seed
	EXPECT_FLOAT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 0.5f), 133.0f);
	// Never past the middle of the animation
	EXPECT_FLOAT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 100.0f, 1.0f), 266.0f);
	EXPECT_FLOAT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 100.0f, 1.0f), 0.0f);
}

TEST(HandHoldPose, TheHandRisesByHowItHolds)
{
	// Food's horn hangs 0.7 of its height below the hand point
	EXPECT_FLOAT_EQ(SeedHang(HoldType::Side, 0.7f, 4.0f), 2.8f);
	// Changed from his 0: ours hangs every hold by its lowering, a miracle not yet ready too
	EXPECT_FLOAT_EQ(SeedHang(HoldType::Magic, 0.5f, 4.0f), 2.0f);
	// The forest sits a tenth of its height above
	EXPECT_FLOAT_EQ(SeedHang(HoldType::Above, -0.1f, 5.0f), -0.5f);

	EXPECT_FLOAT_EQ(HoldLift(HoldType::Above, -0.5f, 1.0f), 0.2f);
	EXPECT_FLOAT_EQ(HoldLift(HoldType::Magic, 0.0f, 0.5f), 1.6f);
	EXPECT_FLOAT_EQ(HoldLift(HoldType::Side, 2.8f, 1.0f), 2.8f);
	EXPECT_FLOAT_EQ(HoldLift(HoldType::Side, 0.4f, 1.0f), 1.9f);
}

TEST(HandHoldPose, TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian)
{
	const auto sway = CursorSway({40.0f, -80.0f});
	EXPECT_NEAR(sway.x, 0.15f, k_Epsilon);
	EXPECT_NEAR(sway.y, -0.3f, k_Epsilon);
	EXPECT_NEAR(CursorSway({500.0f, 0.0f}).x, 0.3f, k_Epsilon);
}

TEST(HandHoldPose, TheHandRollsBackAboutTheLineToTheCamera)
{
	// The camera due south of the hand, level with it: rolling turns up about the north-south line
	const glm::vec3 camera {0.0f, 0.0f, -10.0f};
	const glm::vec3 hand {0.0f, 0.0f, 0.0f};
	ExpectNear(HeldUp(camera, hand, 0.0f, 0.0f), {0.0f, 1.0f, 0.0f});
	const float angle = 0.5f;
	// Up turned by minus the roll about the line from the hand to the camera (towards -z): towards -x
	ExpectNear(HeldUp(camera, hand, angle, 0.0f), {-std::sin(angle), std::cos(angle), 0.0f});
	// The pitch turns it back about the level line across that, (1, 0, 0) here
	ExpectNear(HeldUp(camera, hand, 0.0f, angle), {0.0f, std::cos(angle), -std::sin(angle)});
	// The camera on the hand: only shrinks
	ExpectNear(HeldUp(hand, hand, angle, 0.0f), {0.0f, std::cos(angle), 0.0f});
}

TEST(HandHoldPose, UprightTheSeedTakesTheHandsTurn)
{
	const auto turn = glm::mat3(glm::eulerAngleY(0.7f));
	// Ours takes the level way the hand faces itself (the hand system keeps it), not the hand's level turn
	const auto facing = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), turn[0]);
	const auto basis = HeldBasis(facing, {0.0f, 1.0f, 0.0f});
	for (glm::length_t c = 0; c < 3; ++c)
	{
		ExpectNear(basis[c], turn[c]);
	}
	// Tipped, its up is the up given and its axes stay square
	const auto up = glm::normalize(glm::vec3(0.3f, 1.0f, 0.1f));
	const auto tipped = HeldBasis(facing, up);
	ExpectNear(tipped[1], up);
	EXPECT_NEAR(glm::dot(tipped[0], tipped[1]), 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::dot(tipped[0], tipped[2]), 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::length(tipped[0]), 1.0f, k_Epsilon);
	// Its side lies across the level way the hand faces
	EXPECT_NEAR(glm::dot(tipped[0], glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), turn[0])), 0.0f, k_Epsilon);
}

TEST(HandHoldPose, TheSeedTurnsHalfRoundForARightHandAndByItsOwnTurn)
{
	const glm::mat3 basis {1.0f};
	const auto left = SeedTurn(basis, 0.0f, false);
	ExpectNear(left[0], {1.0f, 0.0f, 0.0f});
	const auto right = SeedTurn(basis, 0.0f, true);
	ExpectNear(right[0], {-1.0f, 0.0f, 0.0f});
	ExpectNear(right[2], {0.0f, 0.0f, -1.0f});
	ExpectNear(right[1], {0.0f, 1.0f, 0.0f});
	// The ground flock turned a quarter round
	const auto quarter = SeedTurn(basis, std::numbers::pi_v<float> * 0.5f, false);
	ExpectNear(quarter[0], {0.0f, 0.0f, 1.0f});
	ExpectNear(quarter[2], {-1.0f, 0.0f, 0.0f});
}

TEST(HandHoldPose, TheHeldModelHangsBelowTheHandAlongItsUp)
{
	ExpectNear(HangBelow({1.0f, 5.0f, 2.0f}, {0.0f, 1.0f, 0.0f}, 2.0f), {1.0f, 3.0f, 2.0f});
	ExpectNear(HangBelow({1.0f, 5.0f, 2.0f}, {1.0f, 0.0f, 0.0f}, 0.5f), {0.5f, 5.0f, 2.0f});
}

TEST(HandHoldPose, TheSpringStepsAtLeastOnceAFrameUntilItCatchesUpWithItsClock)
{
	HoldingSpring spring;
	StepHoldingSpring(spring, {1.0f, 0.0f, 0.0f}, 35);
	EXPECT_EQ(spring.stepMs, 40u);
	// Ahead of the clock it still takes one step
	StepHoldingSpring(spring, {1.0f, 0.0f, 0.0f}, 0);
	EXPECT_EQ(spring.stepMs, 50u);
	// The first step: a = 260 d, so v = 2.6 and the hand moves 0.026
	HoldingSpring first;
	StepHoldingSpring(first, {1.0f, 0.0f, 0.0f}, 0);
	EXPECT_NEAR(first.velocity.x, 2.6f, k_Epsilon);
	EXPECT_NEAR(first.position.x, 0.026f, k_Epsilon);
}

namespace
{
/// A hand dragged along x at a steady speed on the holding spring, frames of 20 ms and game turns of 100 ms, until
/// it lets go after some frames
struct DraggedHand
{
	HoldingSpring spring;
	/// Where the game turn last put the held object: the hand point as the previous turn knew it
	glm::vec3 turnPlace {0.0f};
	/// Where the hand drew the held object this frame
	glm::vec3 drawn {0.0f};
};

constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
constexpr float k_Hang = 1.3f;

DraggedHand Drag(float speed, int frames)
{
	DraggedHand hand;
	uint32_t clockMs = 0;
	glm::vec3 synced(0.0f);
	for (int frame = 1; frame <= frames; ++frame)
	{
		clockMs += 20;
		const glm::vec3 required(speed * static_cast<float>(clockMs) * 0.001f, 0.0f, 0.0f);
		StepHoldingSpring(hand.spring, required, clockMs);
		hand.drawn = HangBelow(hand.spring.position, k_Up, k_Hang);
		if (frame % 5 == 0)
		{
			// a turn: the object takes the hand the last turn sent, and this turn sends the hand where it is now
			hand.turnPlace = synced;
			synced = hand.spring.position;
		}
	}
	return hand;
}
} // namespace

TEST(HandHoldPose, TheHandTrailsADragOnItsSpringAndThrowsAtTheDragsSpeed)
{
	// At a steady 30 m/s the hand settles to the drag's speed: each 20 ms frame it moves the drag's 0.6 m, at about the
	// drag's speed, and stays behind where it should be by a few metres (at most 40 x 30 / 260, the spring's own lag
	// for a smooth drag; the frames' steps keep it a little closer)
	const auto steady = Drag(30.0f, 100);
	const auto next = Drag(30.0f, 101);
	EXPECT_NEAR(next.spring.position.x - steady.spring.position.x, 0.6f, 1e-3f);
	EXPECT_NEAR(steady.spring.velocity.x, 30.0f, 0.5f);
	const float lag = 60.0f - steady.spring.position.x;
	EXPECT_GT(lag, 4.0f);
	EXPECT_LT(lag, 40.0f * 30.0f / 260.0f);
	// However fast the drag, the throw is never faster than the cap
	const auto fast = Drag(500.0f, 50);
	EXPECT_LE(glm::length(fast.spring.velocity), k_MaxHandSpeed + 1e-3f);
	EXPECT_NEAR(glm::length(fast.spring.velocity), k_MaxHandSpeed, 1e-3f);
}

TEST(HandHoldPose, AThrowLeavesFromWhereTheHandDrewTheObjectHoweverFastItMoves)
{
	const glm::mat3 drawnAxes = HeldBasis({0.0f, 0.0f, 1.0f}, k_Up);
	for (const float speed : {10.0f, 40.0f, 100.0f})
	{
		const auto hand = Drag(speed, 75);
		const auto start = ReleasePlace({hand.turnPlace, glm::mat3(1.0f)}, HeldPlace {hand.drawn, drawnAxes});
		ExpectNear(start.position, hand.drawn);
		// hanging from the hand, wherever the spring has taken it
		ExpectNear(start.position, hand.spring.position - k_Up * k_Hang);
		EXPECT_NEAR(start.rotation[0].x, drawnAxes[0].x, k_Epsilon);
	}
	// The turn's place trails the hand further the faster it moves: a throw from it would leave from behind the hand
	const auto slow = Drag(10.0f, 75);
	const auto quick = Drag(40.0f, 75);
	const float slowGap = slow.spring.position.x - slow.turnPlace.x;
	const float quickGap = quick.spring.position.x - quick.turnPlace.x;
	EXPECT_GT(slowGap, 0.0f);
	EXPECT_NEAR(quickGap, 4.0f * slowGap, 1e-2f);
}

TEST(HandHoldPose, WithNothingDrawnAThrowLeavesFromTheTurnsPlace)
{
	const HeldPlace turn {{3.0f, 4.0f, 5.0f}, glm::mat3(2.0f)};
	const auto start = ReleasePlace(turn, std::nullopt);
	ExpectNear(start.position, turn.position);
	EXPECT_EQ(start.rotation, turn.rotation);
}
