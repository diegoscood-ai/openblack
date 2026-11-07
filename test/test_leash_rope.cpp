/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/LeashRope.h"

using namespace openblack;
using namespace openblack::leash_rope;

namespace
{
constexpr float k_Tolerance = 1e-4f;
// Flat land at height 0, well below the ropes
float Flat(glm::vec2 /*point*/)
{
	return 0.0f;
}
// A rope 32.5 long at rest and 77 at full length, as held in the hand for a creature of size 1
constexpr float k_Slack = 32.5f;
constexpr float k_Max = 77.0f;
const glm::vec3 k_Start {100.0f, 50.0f, 100.0f};

// The tests pinning the original's floats: a rope from (0, 10, 0) to (20, 10, 0), over land far below it
const glm::vec3 k_PinStart {0.0f, 10.0f, 0.0f};
const glm::vec3 k_PinEnd {20.0f, 10.0f, 0.0f};
float FarBelow(glm::vec2 /*point*/)
{
	return -100.0f;
}

// A frame's seconds as the game works them out from its whole milliseconds
float FrameSeconds(int milliseconds)
{
	return static_cast<float>(milliseconds) * 0.001f;
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

void ExpectBits(const glm::vec3& value, uint32_t x, uint32_t y, uint32_t z)
{
	EXPECT_EQ(Bits(value.x), x);
	EXPECT_EQ(Bits(value.y), y);
	EXPECT_EQ(Bits(value.z), z);
}
} // namespace

TEST(LeashRope, StartsStraightAndEvenlySpread)
{
	const auto end = k_Start + glm::vec3(41.0f, 0.0f, 0.0f);
	const auto rope = Create(k_Start, end, k_Slack, k_Max, {}, Flat);
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		EXPECT_NEAR(rope.nodes.at(i).position.x, k_Start.x + static_cast<float>(i + 1), k_Tolerance);
		EXPECT_EQ(rope.nodes.at(i).velocity, glm::vec3(0.0f));
	}
	EXPECT_NEAR(RestLength(41.0f), 1.0f, k_Tolerance);
}

TEST(LeashRope, ASlackRopeHasNoTension)
{
	// The ends closer together than the rope's rest length
	const auto rope = Create(k_Start, k_Start + glm::vec3(20.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	EXPECT_FLOAT_EQ(rope.tension, 0.0f);
}

TEST(LeashRope, StretchedToItsFullLengthIsTaut)
{
	const auto atMax = Create(k_Start, k_Start + glm::vec3(k_Max, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	EXPECT_NEAR(atMax.tension, 1.0f, k_Tolerance);
	const auto beyond = Create(k_Start, k_Start + glm::vec3(2.0f * k_Max, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	EXPECT_FLOAT_EQ(beyond.tension, 1.0f);
	// Halfway between the rest length and the full length, half taut
	const auto half = Create(k_Start, k_Start + glm::vec3((k_Slack + k_Max) * 0.5f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	EXPECT_NEAR(half.tension, 0.5f, 1e-3f);
}

TEST(LeashRope, SagsUnderItsWeight)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(25.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	for (int frame = 0; frame < 120; ++frame)
	{
		Step(rope, rope.start, rope.end, 1.0f / 30.0f, Flat);
	}
	// The middle hangs below the ends, the ends exactly where they were put. Still swinging a little, its weight
	// stretches the first segment only slightly past its rest length: far from taut. The original's rope never
	// settles to a tension of exactly 0; it keeps swinging between 0 and about 0.05, and is at these bits after 4 s
	EXPECT_LT(rope.nodes.at(k_NodeCount / 2).position.y, k_Start.y - 1.0f);
	EXPECT_EQ(rope.start, k_Start);
	EXPECT_EQ(Bits(rope.tension), 0x3CA7285Fu);
}

TEST(LeashRope, PullingTheEndAwayTightensIt)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(25.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	auto end = rope.end;
	for (int frame = 0; frame < 60; ++frame)
	{
		end.x += 2.0f;
		Step(rope, rope.start, end, 1.0f / 30.0f, Flat);
	}
	// The ends are 145 apart, far beyond the rope's full length
	EXPECT_GT(rope.tension, 0.8f);
}

TEST(LeashRope, StaysOffTheGround)
{
	const auto high = [](glm::vec2 /*point*/) { return 45.0f; };
	auto rope = Create(k_Start, k_Start + glm::vec3(10.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	for (int frame = 0; frame < 120; ++frame)
	{
		Step(rope, rope.start, rope.end, 1.0f / 30.0f, high);
	}
	const auto floor = 45.0f + Look {}.halfWidth + k_GroundClearance;
	EXPECT_TRUE(std::ranges::all_of(rope.nodes, [floor](const Node& node) { return node.position.y >= floor - 1e-3f; }));
}

TEST(LeashRope, NoTimeStillTakesOneStep)
{
	// As when the game is paused: the rope still takes one step, as it does for any time under 1/200 s
	auto paused = Create(k_Start, k_Start + glm::vec3(10.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	auto brief = paused;
	const auto before = paused.nodes;
	const auto hand = k_Start + glm::vec3(0.0f, 5.0f, 0.0f);
	Step(paused, hand, paused.end, 0.0f, Flat);
	Step(brief, hand, brief.end, 0.001f, Flat);
	EXPECT_EQ(paused.start, hand);
	EXPECT_NE(paused.nodes.at(1).position, before.at(1).position);
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		EXPECT_EQ(paused.nodes.at(i).position, brief.nodes.at(i).position);
	}
}

TEST(LeashRope, EndsAreKeptWithinTheWorld)
{
	EXPECT_EQ(ClampEnd({-5000.0f, -3.0f, 9000.0f}), glm::vec3(k_MinAcross, k_MinHeight, k_MaxAcross));
}

TEST(LeashRope, RibbonFacesTheEyeAndShadowLiesOnTheLand)
{
	const auto rope = Create(k_Start, k_Start + glm::vec3(41.0f, 0.0f, 0.0f), k_Slack, k_Max, {}, Flat);
	// Looking straight down on a rope running along x: the ribbon spreads along z, half its width each side
	const auto ribbon = BuildRibbon(rope, k_Start + glm::vec3(20.0f, 100.0f, 0.0f), Flat);
	const auto& a = ribbon.rope.at(2);
	const auto& b = ribbon.rope.at(3);
	EXPECT_NEAR(glm::distance(a.position, b.position), 2.0f * Look {}.halfWidth, k_Tolerance);
	EXPECT_NEAR(a.position.x, b.position.x, k_Tolerance);
	EXPECT_FLOAT_EQ(a.uv.y, Look {}.v1);
	EXPECT_FLOAT_EQ(b.uv.y, Look {}.v0);
	// The texture runs along the rope's length: one unit is 2.5 x 0.05 of it
	EXPECT_NEAR(a.uv.x, 0.125f, k_Tolerance);
	EXPECT_NEAR(ribbon.rope.back().uv.x, 41.0f * 0.125f, 1e-3f);
	// The shadow sits just over the land, faint, and fades out at the ends
	for (const auto& corner : ribbon.shadow)
	{
		EXPECT_NEAR(corner.position.y, k_ShadowLift, k_Tolerance);
	}
	EXPECT_FLOAT_EQ(ribbon.shadow.front().alpha, 0.0f);
	EXPECT_FLOAT_EQ(ribbon.shadow.back().alpha, 0.0f);
	EXPECT_NEAR(ribbon.shadow.at(10).alpha, 65.0f / 255.0f, k_Tolerance);
}

TEST(LeashRope, RibbonTrianglesCoverEverySegment)
{
	const auto indices = RibbonIndices();
	EXPECT_EQ(indices.size(), (k_PointCount - 1) * 6);
	EXPECT_EQ(*std::ranges::max_element(indices), k_RibbonVertexCount - 1);
}

// ---- The original's floats, bit for bit -----------------------------------------------------------------------------

TEST(LeashRope, LaysByAccumulatedSteps)
{
	const auto rope = Create(k_PinStart, k_PinEnd, k_Slack, k_Max, {}, FarBelow);
	ExpectBits(rope.nodes.at(0).position, 0x3EF9C18Fu, 0x41200000u, 0u);
	ExpectBits(rope.nodes.at(19).position, 0x411C18F8u, 0x41200000u, 0u);
	ExpectBits(rope.nodes.at(39).position, 0x419C18F4u, 0x41200000u, 0u);
	EXPECT_TRUE(std::ranges::all_of(rope.nodes, [](const Node& node) { return node.velocity == glm::vec3(0.0f); }));
	// A segment's rest length is the rope's times the reciprocal of 41
	EXPECT_EQ(Bits(RestLength(k_Slack)), 0x3F4AED44u);
}

TEST(LeashRope, OneSixteenMsFrameTakesFourSteps)
{
	auto rope = Create(k_PinStart, k_PinEnd, k_Slack, k_Max, {}, FarBelow);
	const auto laid = rope.nodes;
	Step(rope, k_PinStart, k_PinEnd, FrameSeconds(16), FarBelow);
	// The rope is slack, so every mass just falls for four steps of 1/200 s
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		const auto& node = rope.nodes.at(i);
		EXPECT_EQ(Bits(node.position.x), Bits(laid.at(i).position.x));
		EXPECT_EQ(Bits(node.position.y), 0x411FF5F5u);
		EXPECT_EQ(Bits(node.velocity.y), 0xBE48E1B0u);
	}
	// Gravity's force is the mass times gravity, rounded once
	EXPECT_EQ(Bits(k_GravityForce), 0xC0FB22D2u);
}

TEST(LeashRope, PausedFramesStillStepAndFollowTheHand)
{
	auto rope = Create(k_PinStart, k_PinEnd, k_Slack, k_Max, {}, FarBelow);
	for (int frame = 0; frame < 10; ++frame)
	{
		const glm::vec3 hand {0.0f, 11.0f + static_cast<float>(frame), 0.0f};
		Step(rope, hand, k_PinEnd, FrameSeconds(0), FarBelow);
	}
	ExpectBits(rope.nodes.at(0).position, 0x3E99EF47u, 0x41958350u, 0u);
	ExpectBits(rope.nodes.at(0).velocity, 0xC0EE3AACu, 0x43401D26u, 0u);
	ExpectBits(rope.nodes.at(19).position, 0x411C18F8u, 0x411FC8C9u, 0u);
	ExpectBits(rope.nodes.at(19).velocity, 0u, 0xBEFADC08u, 0u);
	EXPECT_EQ(Bits(rope.tension), 0x3F0240CFu);
}

TEST(LeashRope, RestsOnTheGround)
{
	const glm::vec3 start {0.0f, 12.0f, 0.0f};
	const glm::vec3 end {10.0f, 12.0f, 0.0f};
	auto rope = Create(start, end, k_Slack, k_Max, {}, Flat);
	for (int frame = 0; frame < 100; ++frame)
	{
		Step(rope, start, end, FrameSeconds(33), Flat);
	}
	// The middle lies on the land, the ribbon's half width and the clearance over it
	for (size_t i = 17; i <= 23; ++i)
	{
		EXPECT_EQ(Bits(rope.nodes.at(i).position.y), 0x3F266666u);
	}
	ExpectBits(rope.nodes.at(0).position, 0x3CC619EBu, 0x4133A9A1u, 0u);
	ExpectBits(rope.nodes.at(0).velocity, 0x3E10674Eu, 0x40387C2Cu, 0u);
	ExpectBits(rope.nodes.at(10).position, 0x3FAD1D1Bu, 0x40818EA9u, 0u);
	ExpectBits(rope.nodes.at(19).position, 0x4097F6E0u, 0x3F266666u, 0u);
	ExpectBits(rope.nodes.at(19).velocity, 0xBF09EA7Eu, 0xC0FB003Du, 0u);
	ExpectBits(rope.nodes.at(39).position, 0x411BD819u, 0x41345378u, 0u);
}

TEST(LeashRope, SpeedCapIsThreeHundred)
{
	auto rope = Create(k_PinStart, k_PinEnd, k_Slack, k_Max, {}, FarBelow);
	// The hand jerked 100 to the side in a paused frame: the first mass reaches the cap in its one step
	Step(rope, {0.0f, 10.0f, 100.0f}, k_PinEnd, FrameSeconds(0), FarBelow);
	ExpectBits(rope.nodes.at(0).velocity, 0xBFBB5098u, 0xBA9B876Au, 0x4395FF8Bu);
	ExpectBits(rope.nodes.at(0).position, 0x3EF60282u, 0x411FFFFAu, 0x3FBFFF6Au);
	ExpectBits(rope.nodes.at(1).position, 0x3F79C18Fu, 0x411FFEFFu, 0u);
	ExpectBits(rope.nodes.at(1).velocity, 0u, 0xBD48E8A7u, 0u);
	EXPECT_EQ(Bits(rope.tension), 0x3F800000u);
	EXPECT_NEAR(glm::length(rope.nodes.at(0).velocity), k_MaxSpeed, 1e-2f);
}

TEST(LeashRope, LayingRaisesTheNextMassesOffTheGround)
{
	// Land rising along x, under the rope's start and over its end
	const auto slope = [](glm::vec2 point) { return point.x * 0.5f; };
	const auto rope = Create({0.0f, 1.0f, 0.0f}, {20.0f, 1.0f, 0.0f}, k_Slack, k_Max, {}, slope);
	// Each mass is laid where the line is, and only then is the line lifted over the land, one mass late
	for (size_t i = 0; i <= 2; ++i)
	{
		EXPECT_EQ(Bits(rope.nodes.at(i).position.y), 0x3F800000u);
	}
	ExpectBits(rope.nodes.at(39).position, 0x419C18F4u, 0x411A9854u, 0u);
}

TEST(LeashRope, FrameSecondsGiveTheOriginalStepCounts)
{
	constexpr std::array<int, 8> k_Milliseconds {0, 1, 5, 10, 16, 17, 33, 199};
	constexpr std::array<uint32_t, 8> k_Steps {1, 1, 2, 3, 4, 4, 7, 40};
	for (size_t i = 0; i < k_Milliseconds.size(); ++i)
	{
		EXPECT_EQ(SubSteps(FrameSeconds(k_Milliseconds.at(i))), k_Steps.at(i)) << k_Milliseconds.at(i) << " ms";
	}
	// 5 ms is just over 1/200 s as a float, so it takes two steps
	EXPECT_EQ(Bits(FrameSeconds(5)), 0x3BA3D70Bu);
}
