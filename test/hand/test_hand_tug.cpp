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

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/ModelInverseSquareRoot.h"
#include "Hand/HandGrabRules.h"

using namespace openblack;
using namespace openblack::hand_grab;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

// The eye, the hand before the press and a tree's base, the numbers the original's answers below were taken with
constexpr glm::vec3 k_Eye {10.3f, 40.7f, -5.2f};
constexpr glm::vec3 k_Hand {14.1f, 33.9f, 3.3f};
constexpr glm::vec3 k_Base {25.6f, 12.0f, 30.9f};
} // namespace

TEST(HandTug, GroundRangeIsTheOriginals)
{
	// The original's distance across the ground, run on the same points
	EXPECT_EQ(Bits(GroundRange(k_Eye, k_Hand)), 0x4114F8D1u);
	EXPECT_EQ(Bits(GroundRange(k_Eye, k_Base)), 0x421CD56Bu);
	EXPECT_EQ(Bits(GroundRange(k_Hand, k_Eye)), 0x4114F8D1u);
}

TEST(HandTug, ThePlaneIsAtTheHandsHeightSeenAtTheBase)
{
	const auto plane = TugPlaneAt(k_Eye, k_Hand, k_Base, {0.1f, 0.99f, -0.05f});
	// Through the base across the ground, at the height of the eye's line through the hand carried on to the base:
	// 40.7 - (40.7 - 33.9) x 39.208416 / 9.310746, each step rounded to a float as the original's
	EXPECT_EQ(plane.point.x, k_Base.x);
	EXPECT_EQ(plane.point.z, k_Base.z);
	EXPECT_EQ(Bits(plane.point.y), 0x4141087Cu); // 12.064571
	EXPECT_EQ(plane.normal, glm::vec3(0.1f, 0.99f, -0.05f));
}

TEST(HandTug, AHandOnTheLineThroughTheBaseMakesAPlaneThroughTheBase)
{
	// The hand on the eye's line to the base: the plane is at the base's own height
	const glm::vec3 eye {0.0f, 20.0f, 0.0f};
	const glm::vec3 base {8.0f, 4.0f, 6.0f};
	const glm::vec3 hand {4.0f, 12.0f, 3.0f};
	EXPECT_FLOAT_EQ(TugPlaneAt(eye, hand, base, {0.0f, 1.0f, 0.0f}).point.y, 4.0f);
	// A hand pointed higher up the tree, at the base's distance, makes a higher plane
	EXPECT_FLOAT_EQ(TugPlaneAt(eye, {4.0f, 14.0f, 3.0f}, base, {0.0f, 1.0f, 0.0f}).point.y, 8.0f);
}

TEST(HandTug, TheHandIsWhereThePointersLineMeetsThePlane)
{
	const auto plane = TugPlaneAt(k_Eye, k_Hand, k_Base, {0.1f, 0.99f, -0.05f});
	const auto hand = TugHandOnPlane(plane, k_Eye, {0.3f, -0.6f, 0.74f});
	// The dot products summed z, y, x, then the eye plus the direction times the distance, as the original
	EXPECT_EQ(Bits(hand.x), 0x41C4B4AEu); // 24.588223
	EXPECT_EQ(Bits(hand.y), 0x4141FA16u); // 12.123556
	EXPECT_EQ(Bits(hand.z), 0x41F05AB0u); // 30.044281
	// On a level plane the hand is at the plane's height
	const TugPlane level {.point = {0.0f, 5.0f, 0.0f}, .normal = {0.0f, 1.0f, 0.0f}};
	EXPECT_FLOAT_EQ(TugHandOnPlane(level, {1.0f, 25.0f, 2.0f}, {0.0f, -1.0f, 0.5f}).y, 5.0f);
	EXPECT_FLOAT_EQ(TugHandOnPlane(level, {1.0f, 25.0f, 2.0f}, {0.0f, -1.0f, 0.5f}).z, 12.0f);
}

TEST(HandTug, TheConstantsAreTheOriginals)
{
	// The exe's own constants and the answers of its functions
	EXPECT_EQ(Bits(k_PullBlendSeconds), 0x3E051EB8u);
	EXPECT_EQ(Bits(k_TugStiffness), 0x447A0000u);
	EXPECT_EQ(Bits(k_TugTurnInertia), 0x447A0000u);
	EXPECT_EQ(Bits(k_TugTurnDamping), 0x457A0000u);
	EXPECT_EQ(Bits(k_TugMaxStretch), 0x3FA66666u);
	EXPECT_EQ(Bits(k_TugStretchSeconds), 0x3E99999Au);
	EXPECT_EQ(Bits(k_StandardHandHeight), 0x404CCCCDu);
	EXPECT_EQ(Bits(k_LeastLoweringShare), 0x3E99999Au);
	EXPECT_EQ(Bits(MaxForce(0.0f)), 0x49127C00u);
	EXPECT_EQ(Bits(MaxForce(0.5f)), 0x495BBA00u);
}

TEST(HandTug, TheGripsLeastHangIsTheHandsHeightTimesItsSizeThenTheShare)
{
	// The least hang is 3.2 x the hand's size x 0.3, multiplied in that order: at a size of 0.7 the other order is a
	// float away
	EXPECT_EQ(Bits(HoldDistance(0.1f, 1.0f, 0.7f)), 0x3F2C0832u);
	EXPECT_EQ(Bits(HoldDistance(0.1f, 1.0f, 1.0f)), Bits(3.2f * 0.3f));
	// A tall tree hangs its own share of its height
	EXPECT_FLOAT_EQ(HoldDistance(0.1f, 18.0f, 1.0f), 1.8f);
}

TEST(HandTug, ThePullsLengthSumsAcrossThenUp)
{
	// Pulled from far away the pull is capped at the hand's greatest, scaled by the stretch's length with its squares
	// summed x, z, then y, as the original: the other order is a float away on each part
	Tug tug {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	const TugFrame frame {.hand = {385.4f, 758.0f, -189.1f},
	                      .holdDistance = 1.0f,
	                      .weight = 1000.0f,
	                      .height = 10.0f,
	                      .tree = true,
	                      .seconds = 0.01f};
	const auto result = PullAt(tug, frame);
	EXPECT_EQ(Bits(result.force.x), 0x4881BE76u);
	EXPECT_EQ(Bits(result.force.y), 0x48FED79Eu);
	EXPECT_EQ(Bits(result.force.z), 0xC7FEA3E9u);
	EXPECT_TRUE(result.comesFree);
}

TEST(HandTug, TheTurnIsThePullTimesTheInverseOfTheHeaviness)
{
	// The turning force is multiplied by 1 / 1000, as the original, not divided by 1000: a float apart here. Its sign
	// is the turn's, about the axis the lean rotates about
	Tug tug {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	const TugFrame frame {
	    .hand = {4.42f, 2.4f, 4.22f}, .holdDistance = 1.0f, .weight = 1.0e6f, .height = 10.0f, .tree = true, .seconds = 0.01f};
	static_cast<void>(PullAt(tug, frame));
	// The original keeps the spin the other way round (the pull across the grip's place) and turns about the opposite
	// axis: its bits are these with the sign turned
	EXPECT_EQ(Bits(tug.spin.x), 0x3D2CD9A9u);
	EXPECT_EQ(Bits(tug.spin.y), 0u);
	EXPECT_EQ(Bits(tug.spin.z), 0xBD350ACBu);
	// and the frame's lean is the original's, bit for bit
	constexpr std::array<std::array<uint32_t, 3>, 3> k_Axes {{
	    {0x3F7FFFFEu, 0xB9E7BBE5u, 0xB3C00000u},
	    {0x39E7BBE5u, 0x3F7FFFFDu, 0x39DD3F8Fu},
	    {0xB3C00000u, 0xB9DD3F8Fu, 0x3F7FFFFEu},
	}};
	for (int axis = 0; axis < 3; ++axis)
	{
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_EQ(Bits(tug.axes[axis][k]), k_Axes.at(static_cast<size_t>(axis)).at(static_cast<size_t>(k)))
			    << axis << " " << k;
		}
	}
}

TEST(HandTug, ATreeComesFreeOnlyWhenTheHandCanLiftItAndPullsHarderThanItWeighs)
{
	// The hand must be able to lift it (its greatest pull at least 9.81 x its weight), then pull harder than it weighs:
	// one unit above the grip (a hair below 1, up the axis made unit length with the game's inverse square root) the
	// pull is exactly its weight and it stays; a float higher it comes free, as in the original
	Tug tug {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	TugFrame frame {
	    .hand = {0.0f, std::bit_cast<float>(0x3FFFFFD0u), 0.0f}, .holdDistance = 1.0f, .weight = 1000.0f, .tree = true};
	EXPECT_EQ(Bits(PullAt(tug, frame).force.y), Bits(1000.0f));
	EXPECT_FALSE(PullAt(tug, frame).comesFree);
	frame.hand.y = std::bit_cast<float>(0x3FFFFFD1u);
	EXPECT_TRUE(PullAt(tug, frame).comesFree);
	// Heavier than the hand's greatest pull over gravity, never
	frame.weight = 600000.0f / 9.81f * 1.0001f;
	frame.hand.y = 1.0e5f;
	EXPECT_FALSE(PullAt(tug, frame).comesFree);
}

TEST(HandTug, AThingAboutToBePulledHasItsAxesScaled)
{
	// Its axes are its rotation's, each times its scale, and its base is where it stands
	const glm::mat3 rotation {glm::vec3(0.6f, 0.0f, -0.8f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.8f, 0.0f, 0.6f)};
	const auto tug = TugOf(rotation, glm::vec3(1.7f), {5.0f, 2.0f, -3.0f});
	EXPECT_EQ(tug.axes[0], rotation[0] * 1.7f);
	EXPECT_EQ(tug.axes[1], glm::vec3(0.0f, 1.7f, 0.0f));
	EXPECT_EQ(tug.axes[2], rotation[2] * 1.7f);
	EXPECT_EQ(tug.base, glm::vec3(5.0f, 2.0f, -3.0f));
	EXPECT_EQ(tug.spin, glm::vec3(0.0f));
}

TEST(HandTug, TheGripIsUpTheUnitAxisAsTheGameMakesItUnit)
{
	// A tree of scale 1.7: its up made unit length with the game's inverse square root, then times the grip's height
	const auto tug = TugOf(glm::mat3(1.0f), glm::vec3(1.7f), {5.0f, 2.0f, -3.0f});
	const auto up = tug.axes[1] * gutils::ModelInverseSquareRoot(glm::dot(tug.axes[1], tug.axes[1]));
	const auto grip = TugGrip(tug, 1.8f);
	EXPECT_EQ(Bits(grip.y), Bits(2.0f + up.y * 1.8f));
	EXPECT_EQ(grip.x, 5.0f);
	EXPECT_EQ(grip.z, -3.0f);
	// Drawn, the hand is up the unit axis times the stretch, and that times the grip's height
	const auto hand = TugHandPoint(tug, 1.8f, 1.17f);
	EXPECT_EQ(Bits(hand.y), Bits(2.0f + (up.y * 1.17f) * 1.8f));
	// The tree is drawn with its axes made unit length the same way
	EXPECT_EQ(TugRotation(tug)[1], up);
}

TEST(HandTug, TheLeanIsTheGamesAxisAngleTurnOfEachAxis)
{
	// One frame of a pull from rest, off to the side of the grip: the spin is the turning force times the frame, and each
	// axis is turned by the spin over the frame. The game turns about the opposite of the turn's axis and multiplies each
	// axis as a row, its sums from the last term, so it is written out here as it must round
	auto tug = TugOf(glm::mat3(1.0f), glm::vec3(1.3f), {0.0f, 0.0f, 0.0f});
	const TugFrame frame {
	    .hand = {0.7f, 2.9f, 0.4f}, .holdDistance = 2.34f, .weight = 1.0e6f, .height = 23.4f, .tree = true, .seconds = 0.016f};
	const auto before = tug.axes;
	const auto grip = TugGrip(tug, frame.holdDistance);
	const auto force = (frame.hand - grip) * k_TugStiffness;
	const auto spin = glm::cross(grip, force) * (1.0f / k_TugTurnInertia) * frame.seconds;
	const auto turn = spin * frame.seconds;
	const float angle = std::sqrt(turn.x * turn.x + turn.y * turn.y + turn.z * turn.z);
	const auto r = affine::AxisAngle(-(turn * (1.0f / angle)), angle);
	static_cast<void>(PullAt(tug, frame));
	EXPECT_EQ(tug.spin, spin);
	for (int axis = 0; axis < 3; ++axis)
	{
		const auto& m = before[axis];
		for (int k = 0; k < 3; ++k)
		{
			const float expected = (r[2][k] * m[2] + r[1][k] * m[1]) + r[0][k] * m[0];
			EXPECT_EQ(Bits(tug.axes[axis][k]), Bits(expected)) << axis << " " << k;
		}
	}
	// Its middle's speed: how far the turn moved it, times one over the frame's time
	EXPECT_EQ(Bits(tug.pullVelocity.x),
	          Bits((tug.axes[1].x * (frame.height * 0.5f) + 0.0f - (before[1].x * (frame.height * 0.5f) + 0.0f)) *
	               (1.0f / frame.seconds)));
	// It leans towards the hand: its top goes the way the hand is from the grip
	EXPECT_GT(tug.axes[1].x, 0.0f);
	EXPECT_GT(tug.axes[1].z, 0.0f);
}

TEST(HandTug, ThePullWaitsForTheHandsFade)
{
	// 16 ms frames: the fade is counted from the state's first frame; the frame that finds it over (0.144 s counted)
	// still waits, and the pull starts on the eleventh frame
	TugFade fade;
	int firstPull = 0;
	for (int frame = 1; frame <= 12 && firstPull == 0; ++frame)
	{
		if (StepTugFade(fade, 0.016f))
		{
			firstPull = frame;
		}
	}
	EXPECT_EQ(firstPull, 11);
	EXPECT_FALSE(fade.waiting);
	// It stops counting once it has run its length
	EXPECT_GE(fade.seconds, k_PullBlendSeconds);
	const float counted = fade.seconds;
	EXPECT_TRUE(StepTugFade(fade, 0.016f));
	EXPECT_EQ(fade.seconds, counted);
	// One long frame runs the whole fade: the first frame waits, the second finds it over and waits, the third pulls
	TugFade slow;
	EXPECT_FALSE(StepTugFade(slow, 0.2f));
	EXPECT_FALSE(StepTugFade(slow, 0.2f));
	EXPECT_TRUE(StepTugFade(slow, 0.2f));
}
