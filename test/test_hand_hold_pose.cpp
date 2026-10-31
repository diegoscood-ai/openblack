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
#include <numbers>
#include <string>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "Magic/HandHoldPose.h"
#include "oracle/OracleData.h"

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
	// Each still frame is in whole milliseconds, the fraction cut off towards zero, as the original. The animation each
	// hold takes stays with the hand system (HandSystem.cpp), which picks it by the clips it has.
	// At the standard size: food (scale 0.8, radius 1), wood (0.65) and the physical shield (1.0) at the side
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 1.0f), 66);
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.65f, 1.0f), 54);
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 1.0f, 1.0f), 83);
	// The forest (1.0) and the flocks (2.6 and 1.6 times a radius of 0.4) above
	EXPECT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 1.0f, 1.0f), 197);
	EXPECT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 2.6f * 0.4f, 1.0f), 194);
	EXPECT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 1.6f * 0.4f, 1.0f), 222);
	// Every miracle not yet ready, and every one without a model, takes the middle of the rest pose
	EXPECT_EQ(HoldTimeMs(HoldType::Magic, k_WiggleMs, 0.8f, 1.0f), 133);
	// A smaller hand, closer to the camera, opens wider round the same seed
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 0.5f), 133);
	// Never past the middle of the animation
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 100.0f, 1.0f), 266);
	EXPECT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 100.0f, 1.0f), 0);
	// Cut, not rounded: 74.8125 ms at the side is 74, not 75
	EXPECT_EQ(HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.9f, 1.0f), 74);
	// The middle of an odd clip is its whole half
	EXPECT_EQ(HoldTimeMs(HoldType::Magic, 267, 0.8f, 1.0f), 133);
}

TEST(HandHoldPose, PullingAThingHeldAboveOpensTheHandByTheHandsHeightAlone)
{
	// A reach of the hand's own height closes the pull's pose fully, while holding it is not yet closed
	EXPECT_EQ(TugTimeMs(HoldType::Above, k_HoldAboveMs, 3.2f, 1.0f), 0);
	EXPECT_EQ(HoldTimeMs(HoldType::Above, k_HoldAboveMs, 3.2f, 1.0f), 44); // 44.41667, cut
	// A reach of 1: half the pose times (1 - 1 / 3.2)
	EXPECT_EQ(TugTimeMs(HoldType::Above, k_HoldAboveMs, 1.0f, 1.0f), 183); // 183.21875, cut to whole milliseconds
	// Every other hold, a tree's among them, pulls as it holds
	EXPECT_EQ(TugTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 1.0f), HoldTimeMs(HoldType::Side, k_HoldSideMs, 0.8f, 1.0f));
	EXPECT_EQ(TugTimeMs(HoldType::Tree, k_HoldSideMs, 0.3f, 0.7f), HoldTimeMs(HoldType::Tree, k_HoldSideMs, 0.3f, 0.7f));
	EXPECT_EQ(TugTimeMs(HoldType::Magic, k_WiggleMs, 0.8f, 1.0f), 133);
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

TEST(HandHoldPose, TheSwaysUpIsTheOriginalsToTheBit)
{
	// The original's two axis-angle turns, their product and the up through it, run on the same inputs (tools/oracle)
	const auto bits = [](glm::vec3 v) {
		return std::array<uint32_t, 3> {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y),
		                                std::bit_cast<uint32_t>(v.z)};
	};
	EXPECT_EQ(bits(HeldUp({120.5f, 80.25f, -40.75f}, {3.5f, 10.0f, 2.25f}, 0.1875f, -0.15625f)),
	          (std::array<uint32_t, 3> {0xBE45D2FCu, 0x3F79D0F3u, 0xBDD0EA5Eu}));
	EXPECT_EQ(bits(HeldUp({-60.0f, 45.5f, 150.25f}, {-2.0f, 4.5f, 7.75f}, -0.3f, 0.3f)),
	          (std::array<uint32_t, 3> {0xBEC020B8u, 0x3F6986C9u, 0x3E286EBCu}));
	// the camera on the hand: no axis, so the up is straight up times both cosines, its zeros positive
	EXPECT_EQ(bits(HeldUp({10.0f, 0.0f, -10.0f}, {10.0f, 0.0f, -10.0f}, 0.25f, 0.125f)),
	          (std::array<uint32_t, 3> {0x00000000u, 0x3F761B35u, 0x00000000u}));
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

TEST(HandHoldPose, AHeldThingFacesBackAlongTheHandsHeadingWithItsSideAcrossTheUp)
{
	// Upright, at a heading H: the side (-cos H, 0, -sin H), the up, and the forward (sin H, 0, -cos H)
	const float heading = 0.7f;
	const glm::vec3 straight {0.0f, 1.0f, 0.0f};
	const auto axes = HeldAxes(HeldFrame(heading, straight, HeldFrameSize(1.0f), false));
	ExpectNear(axes[0], {-std::cos(heading), 0.0f, -std::sin(heading)});
	ExpectNear(axes[1], straight);
	ExpectNear(axes[2], {std::sin(heading), 0.0f, -std::cos(heading)});
	// Tipped, its up is the up given and its axes stay square and unit, whatever the hand's size
	const auto up = glm::normalize(glm::vec3(0.3f, 1.0f, 0.1f));
	const auto tipped = HeldAxes(HeldFrame(heading, up, HeldFrameSize(7.5f), false));
	ExpectNear(tipped[1], up);
	EXPECT_NEAR(glm::dot(tipped[0], tipped[1]), 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::dot(tipped[0], tipped[2]), 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::length(tipped[0]), 1.0f, k_Epsilon);
	EXPECT_NEAR(glm::length(tipped[2]), 1.0f, k_Epsilon);
	// Its side stays level, across the way back along the heading
	EXPECT_NEAR(tipped[0].x * -std::sin(heading) + tipped[0].z * std::cos(heading), 0.0f, k_Epsilon);
}

TEST(HandHoldPose, AMirroredHandHoldsTheThingWithItsSideAndForwardTurnedOver)
{
	const auto up = glm::normalize(glm::vec3(-0.2f, 1.0f, 0.4f));
	const auto left = HeldAxes(HeldFrame(-2.1f, up, HeldFrameSize(1.0f), false));
	const auto right = HeldAxes(HeldFrame(-2.1f, up, HeldFrameSize(1.0f), true));
	ExpectNear(right[0], -left[0]);
	ExpectNear(right[1], left[1]);
	ExpectNear(right[2], -left[2]);
}

TEST(HandHoldPose, TheFrameIsTheHandsDrawnSizeTimesItsAxes)
{
	// The hand's drawn size: its standard height over its model's height first, then times its scale
	EXPECT_EQ(HeldFrameSize(2.5f), (3.2f / 555.29437f) * 2.5f);
	// the model's height is the one the game works out from the hand's bones as it loads them, to the bit
	EXPECT_EQ(std::bit_cast<uint32_t>(k_HandModelHeight), 0x440AD2D7u);
	const float size = HeldFrameSize(2.5f);
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	const auto frame = HeldFrame(0.0f, up, size, false);
	ExpectNear(frame[0] / size, {-1.0f, 0.0f, 0.0f});
	ExpectNear(frame[1] / size, up);
	ExpectNear(frame[2] / size, {0.0f, 0.0f, -1.0f});
	// A mirrored hand's side is minus the size times the side
	const auto mirrored = HeldFrame(0.0f, up, size, true);
	ExpectNear(mirrored[0] / size, {1.0f, 0.0f, 0.0f});
	ExpectNear(mirrored[2] / size, {0.0f, 0.0f, 1.0f});
}

TEST(HandHoldPose, TheHoldsOwnTurnAddsAZeroThatCanChangeTheSignOfAZeroPart)
{
	// The side's zero x takes the forward's x times the sine 0 (a positive zero) added to it
	const glm::mat3 frame {glm::vec3(-0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(3.0f, 0.0f, -0.0f)};
	const auto axes = HeldAxes(frame);
	EXPECT_FALSE(std::signbit(axes[0].x));
	EXPECT_EQ(axes[0], glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(axes[1], glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(axes[2].x, 1.0f);
	// An axis whose parts are all zero is left as it is
	const auto none = HeldAxes(glm::mat3(0.0f));
	EXPECT_EQ(none[1], glm::vec3(0.0f));
}

// The frame of a held thing, bit for bit with the original's, for headings, ups, hand sizes and either hand (the
// oracle's held_frame_scaled.txt)
TEST(HandHoldPose, TheHeldFrameIsTheOriginalsBitForBit)
{
	const auto table = oracle::Table::Load("held_frame_scaled.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto column = [&table](const char* name) { return table->Column(name).value_or(0); };
	size_t sizesChecked = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&table, &column, row](const char* name) { return table->Float(row, column(name)); };
		const glm::vec3 up {at("ux"), at("uy"), at("uz")};
		const bool mirrored = table->Bits(row, column("mirror")) != 0;
		const auto frame = HeldFrame(at("heading"), up, at("drawn"), mirrored);
		for (int part = 0; part < 9; ++part)
		{
			const auto name = "r" + std::to_string(part);
			const float ours = frame[part / 3][part % 3];
			EXPECT_EQ(std::bit_cast<uint32_t>(ours), table->Bits(row, column(name.c_str())))
			    << "row " << row << " " << name << " ours " << oracle::Hex(ours);
		}
		// The drawn size, where the hand has our model's height and its own size 1
		if (at("divisor") == k_HandModelHeight && at("size") == 1.0f)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(HeldFrameSize(at("scale"))), table->Bits(row, column("drawn"))) << "row " << row;
			++sizesChecked;
		}
	}
	EXPECT_GT(sizesChecked, 0u);
}

// The hold's own turn of the frame, bit for bit with the original's (927 cases of its holding state's own turn run on
// the frozen exe by the oracle: angles about every quarter turn and at random, frames with zero parts; the oracle's
// held_turn.txt)
TEST(HandHoldPose, TheHoldsOwnTurnOfTheFrameIsTheOriginalsBitForBit)
{
	const auto table = oracle::Table::Load("held_turn.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto column = [&table](const std::string& name) { return table->Column(name.c_str()).value_or(0); };
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		glm::mat3 frame {0.0f};
		for (int part = 0; part < 9; ++part)
		{
			frame[part / 3][part % 3] = table->Float(row, column("m" + std::to_string(part)));
		}
		const auto turned = HeldTurn(frame, table->Float(row, column("angle")));
		for (int part = 0; part < 9; ++part)
		{
			const float ours = turned[part / 3][part % 3];
			EXPECT_EQ(std::bit_cast<uint32_t>(ours), table->Bits(row, column("o" + std::to_string(part))))
			    << "row " << row << " o" << part << " ours " << oracle::Hex(ours);
		}
	}
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
	const glm::mat3 drawnAxes = HeldAxes(HeldFrame(0.0f, k_Up, HeldFrameSize(1.0f), false));
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
