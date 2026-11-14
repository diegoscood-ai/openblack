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

// The ribbon's corners as the original's own ribbon builder makes them (tools/oracle, with the land's light and height
// and the draw call stubbed, the land at 0), for a sagging rope with uneven masses near the world's origin, where the
// last bits of the side still show in the corners, an eye above and to the side, and one point whose two neighbours are
// at the same place. The line from the eye and the rope's direction are each made one long (their squares added from
// z to x and from x to z), the side across them with its squares added z, x, then y, and the length along the rope
// adds its squares from z to x.
TEST(LeashRope, RibbonCornersAreTheOriginals)
{
	// The rope's 42 points, from its start to its end, as float bits
	constexpr std::array<std::array<uint32_t, 3>, k_PointCount> k_Points {{
	    {0xbf1fea00, 0x3a1c0000, 0x3b8e0000}, {0xbf196a00, 0x3b1c0000, 0x3bb10000}, {0xbf0e3100, 0xbcc66000, 0xbbc48000},
	    {0xbf06cd00, 0xbccb8000, 0xbaa20000}, {0xbefa9c00, 0xbcc14000, 0xbb170000}, {0xbef6ce00, 0xbd625000, 0x3b460000},
	    {0xbedc3000, 0xbd87c800, 0x3c6f0000}, {0xbed60000, 0xbd3b2000, 0xbc4b0000}, {0xbebd3c00, 0xbd7c0000, 0x3b660000},
	    {0xbeae4400, 0xbd834000, 0xbbaf0000}, {0xbea10400, 0xbd890800, 0xbc554000}, {0xbe91fe00, 0xbdb40000, 0xbba68000},
	    {0xbe716400, 0xbd8e3000, 0x3c2e4000}, {0xbe512000, 0xbdd34000, 0xbc25c000}, {0xbe482c00, 0xbda8c000, 0xbc2d8000},
	    {0xbe2c5800, 0xbdc90800, 0xbc780000}, {0xbdf1c000, 0xbdb94000, 0x3b440000}, {0xbdd62000, 0xbddd0800, 0xbc2c0000},
	    {0xbd490000, 0xbdbc4800, 0x3c16c000}, {0xbd1d2000, 0xbdb98800, 0x3c6b8000}, {0x3b470000, 0xbde99000, 0x3bd60000},
	    {0xbd1d2000, 0xbdb98800, 0x3c6b8000}, {0x3d4d4000, 0xbddb0000, 0xbc274000}, {0x3dc01000, 0xbdcc6800, 0xba9a0000},
	    {0x3df35000, 0xbdbb8800, 0x3c354000}, {0x3e2d4400, 0xbdcf7800, 0xbbf90000}, {0x3e471400, 0xbdbe4800, 0x3b250000},
	    {0x3e656800, 0xbdd27800, 0x3c104000}, {0x3e74c800, 0xbd9d8000, 0xbc674000}, {0x3e959400, 0xbd91e000, 0x39400000},
	    {0x3ea14e00, 0xbda18800, 0x3c49c000}, {0x3eadd400, 0xbd86a800, 0x3b960000}, {0x3ec0cc00, 0xbdafd800, 0xbc31c000},
	    {0x3ecd7a00, 0xbd819000, 0xbc218000}, {0x3ee53c00, 0xbd442000, 0xbc554000}, {0x3eee4a00, 0xbd5bb000, 0x3c028000},
	    {0x3f000900, 0xbd49e000, 0xbb820000}, {0x3f09fe00, 0xbcf1a000, 0xbb340000}, {0x3f0f8d00, 0xbc7a8000, 0x3bdb0000},
	    {0x3f164000, 0xbcdb6000, 0xbbe98000}, {0x3f1cde00, 0xbc84c000, 0x3c040000}, {0x3f272000, 0x3b360000, 0xbc728000},
	}};
	// Each point's ribbon corner either side (x, y, z), its shadow's corners either side (x, z) and its texture's u
	constexpr std::array<std::array<uint32_t, 11>, k_PointCount> k_Corners {{
	    {0xbf1d20c7, 0xbd9fd258, 0xbdfbc7b4, 0xbf22b339, 0x3da24258, 0x3e06c3da, 0xbf21872c, 0x3e1de6da, 0xbf1e4cd4, 0xbe1506da,
	     0x00000000},
	    {0xbf22e0ab, 0xbd316306, 0xbe07c9d8, 0xbf0ff355, 0x3d44e306, 0x3e12d9d8, 0xbf13bf32, 0x3e1d7303, 0xbf1f14ce, 0xbe126303,
	     0x3b50b272},
	    {0xbf163406, 0xbd97e8c8, 0xbe136184, 0xbf062dfa, 0x3cd2e322, 0x3e071984, 0xbf0ab34b, 0x3e12d2c3, 0xbf11aeb5, 0xbe1f1ac3,
	     0x3c1fbfe4},
	    {0xbf04c4e2, 0xbdcf66f0, 0xbe052e66, 0xbf08d51e, 0x3d534de0, 0x3e02a666, 0xbf08f271, 0x3e181823, 0xbf04a78f, 0xbe1aa023,
	     0x3c5bafd2},
	    {0xbf039d6e, 0xbda19210, 0xbe0ee3f9, 0xbeedfd24, 0x3d01e421, 0x3e0a2bf9, 0xbf00f5a7, 0x3e168b16, 0xbef34cb3, 0xbe1b4316,
	     0x3c93dd78},
	    {0xbefb7399, 0xbdf7f1ee, 0xbe069e43, 0xbef22867, 0x3c2d0f70, 0x3e0cce43, 0xbf05e71c, 0x3e16d6ec, 0xbee1cdc9, 0xbe10a6ec,
	     0x3cb59c8a},
	    {0xbee8d654, 0xbdf966ac, 0xbdfb0532, 0xbecf89ac, 0xbc314aa4, 0x3e1b6299, 0xbeca3a0f, 0x3e244731, 0xbeee25f1, 0xbe066731,
	     0x3ced4ef3},
	    {0xbee07b13, 0xbdd49138, 0xbe18bbed, 0xbecb84ed, 0x3c4b89bc, 0x3dfeb7da, 0xbec82857, 0x3e0a659f, 0xbee3d7a9, 0xbe23c59f,
	     0x3d091b28},
	    {0xbebf67d4, 0xbe058db7, 0xbe04c645, 0xbebb102c, 0x3bf1b6e0, 0x3e0bf645, 0xbec42e9e, 0x3e1c905e, 0xbeb64962, 0xbe15605e,
	     0x3d246852},
	    {0xbec55a9c, 0xbdd5721e, 0xbe121497, 0xbe972d64, 0xbcc4378a, 0x3e072497, 0xbe983562, 0x3e0da8fc, 0xbec4529e, 0xbe1898fc,
	     0x3d341a1f},
	    {0xbead12e6, 0xbdfbfc1c, 0xbe19b668, 0xbe94f51a, 0xbc309f20, 0x3dfe1cd0, 0xbea1605b, 0x3e0c452c, 0xbea0a7a5, 0xbe26ed2c,
	     0x3d41ff74},
	    {0xbe81564d, 0xbe335748, 0xbdfb405b, 0xbea2a5b3, 0xba28b880, 0x3de6705b, 0xbea8152e, 0x3e0de7d8, 0xbe77cda4, 0xbe184fd8,
	     0x3d54e882},
	    {0xbe82c726, 0xbe03f8a4, 0xbe00ae5a, 0xbe5d39b4, 0xbc2375bc, 0x3e16765a, 0xbe67d9c8, 0x3e2431af, 0xbe7aee38, 0xbe0e69af,
	     0x3d71164a},
	    {0xbe8e3f2c, 0xbdf46517, 0xbe0f2aca, 0xbe05c1a8, 0xbdb21ae9, 0x3df4e594, 0xbe097b7c, 0x3dfb04c0, 0xbe8c6242, 0xbe123a60,
	     0x3d857f74},
	    {0xbe522b6e, 0xbe1730f2, 0xbe14c9b7, 0xbe3e2c92, 0xbc8c7870, 0x3dfe336e, 0xbe32e93b, 0x3e0d4718, 0xbe5d6ec5, 0xbe22f718,
	     0x3d8b438f},
	    {0xbe1dd249, 0xbe348d7c, 0xbe11cae1, 0xbe3addb7, 0xbca3d420, 0x3de595c2, 0xbe46e028, 0x3e07ca8b, 0xbe11cfd8, 0xbe26ca8b,
	     0x3d9363e8},
	    {0xbdf4218f, 0xbe24cc6e, 0xbe048509, 0xbdef5e71, 0xbca39c90, 0x3e0aa509, 0xbe040594, 0x3e1c41ec, 0xbddb74d8, 0xbe1621ec,
	     0x3da135d1},
	    {0xbdc1ada6, 0xbe3c45ff, 0xbe0ed19e, 0xbdea925a, 0xbd030804, 0x3df2a33c, 0xbdf1acd5, 0x3e0e3b28, 0xbdba932b, 0xbe23bb28,
	     0x3da7d517},
	    {0x3c4fa9a0, 0xbe40124e, 0xbdb533cb, 0xbde2f534, 0x3b729360, 0x3ddae3cb, 0xbdd08dd2, 0x3e1934e3, 0x3b71ba40, 0xbe065ce3,
	     0x3db77345},
	    {0xbd8b6346, 0xbe1380be, 0xbdfb134a, 0xbc0de5d2, 0xbd181d0a, 0x3e1af9a5, 0xbcfb5cfe, 0x3e281e14, 0xbd3c9181, 0xbe0aae14,
	     0x3dba83ca},
	    {0x3e01f506, 0xbe4b661c, 0x3bd60000, 0xbdf77a0c, 0xbcf14f24, 0x3bd60000, 0xbe167d9a, 0x3bd60000, 0x3e1cb59a, 0x3bd60000,
	     0x3dc6dc00},
	    {0xbda450d6, 0xbe0aca5e, 0xbdfad24a, 0x3b661ad0, 0xbd3af686, 0x3e1ad925, 0x3c42e4cc, 0x3e1f70f8, 0xbdb57c9a, 0xbe0200f8,
	     0x3dd33437},
	    {0x3cf53f54, 0xbe2a29be, 0xbe160bdc, 0x3d8ff02b, 0xbd435909, 0x3e0123dc, 0x3d8a8657, 0x3e0e182e, 0x3d057352, 0xbe23002e,
	     0x3deb146d},
	    {0x3e16a28c, 0xbe44b1e1, 0xbdda9457, 0x3d25b5cf, 0xbbf6c3e0, 0x3dd5c457, 0x3d4a6504, 0x3e118622, 0x3e0d76bf, 0xbe13ee22,
	     0x3df6a417},
	    {0x3dd92b9e, 0xbe1fa9cf, 0xbdfd9a04, 0x3e06ba31, 0xbcdef188, 0x3e157502, 0x3e06b0fa, 0x3e245fc0, 0x3dd93e0c, 0xbe0db7c0,
	     0x3dfe13df},
	    {0x3e1c712f, 0xbe271de8, 0xbe12add6, 0x3e3e16d1, 0xbd216860, 0x3e031dd6, 0x3e3e83e7, 0x3e10d8db, 0x3e1c0419, 0xbe2068db,
	     0x3e0607ab},
	    {0x3e691ab8, 0xbe36acce, 0xbdedee71, 0x3e250d48, 0xbbf36650, 0x3df83e71, 0x3e1b0ceb, 0x3e15bb9f, 0x3e731b15, 0xbe10939f,
	     0x3e09ab39},
	    {0x3e44bf3a, 0xbe1f8d17, 0xbe02e613, 0x3e830863, 0xbd4baba5, 0x3e14ee13, 0x3e8d8564, 0x3e18f25c, 0x3e2fc538, 0xbe06ea5c,
	     0x3e0dbe99},
	    {0x3e8577d5, 0xbe217456, 0xbe0df9de, 0x3e5ea056, 0x3b7d1560, 0x3de223bd, 0x3e8408bc, 0x3e09ee61, 0x3e617e88, 0xbe26d661,
	     0x3e12913b},
	    {0x3ea91ad0, 0xbe21efe8, 0xbded8198, 0x3e820d30, 0x3c807f40, 0x3dee4198, 0x3e78b584, 0x3e114455, 0x3eaecd3e, 0xbe10e455,
	     0x3e19a4c7},
	    {0x3eac01c2, 0xbe2361a4, 0xbde632b9, 0x3e969a3e, 0x3aecd240, 0x3e0c515c, 0x3e9a359b, 0x3e258d6f, 0x3ea86665, 0xbe0c556f,
	     0x3e1d1a37},
	    {0x3e91b7eb, 0xbdd50ee0, 0xbe04c61d, 0x3ec9f015, 0xbce1047e, 0x3e0e261d, 0x3ec8e4df, 0x3e146ecb, 0x3e92c321, 0xbe0b0ecb,
	     0x3e20ca71},
	    {0x3eb19e09, 0xbe101f26, 0xbe16cc41, 0x3ecff9f7, 0xbcfdc6d0, 0x3e009441, 0x3ed242f4, 0x3e0a7764, 0x3eaf550c, 0xbe20af64,
	     0x3e26895d},
	    {0x3ee3b932, 0xbe1ac98e, 0xbdfcabd9, 0x3eb73ace, 0x3cc9cc6c, 0x3dd44bd9, 0x3ecfd051, 0x3e0f6f64, 0x3ecb23af, 0xbe239f64,
	     0x3e2ad46c},
	    {0x3efc162a, 0xbe0b3d27, 0xbe00f13e, 0x3ece61d6, 0x3d24b49c, 0x3dcc927d, 0x3ed0a853, 0x3e06a7f6, 0x3ef9cfad, 0xbe214ff6,
	     0x3e31199f},
	    {0x3ef84794, 0xbe08c242, 0xbdf0907c, 0x3ee44c6c, 0x3cd75214, 0x3e08983e, 0x3ee13957, 0x3e1f8471, 0x3efb5aa9, 0xbe0f3471,
	     0x3e34b051},
	    {0x3f015da1, 0xbdfc015a, 0xbe09b378, 0x3efd68bd, 0x3cc88568, 0x3e019378, 0x3f059058, 0x3e13efea, 0x3ef5034f, 0xbe1c0fea,
	     0x3e396c07},
	    {0x3f1b9f55, 0xbdfa8190, 0xbdc96f1b, 0x3ef0b957, 0x3d81b190, 0x3dbe2f1b, 0x3f035893, 0x3e14782a, 0x3f10a36d, 0xbe1a182a,
	     0x3e3f0295},
	    {0x3f0d6a32, 0xbdaaa5e1, 0xbe01c7fb, 0x3f11afce, 0x3d580bc2, 0x3e0f77fb, 0x3f130c59, 0x3e1fce2a, 0x3f0c0da7, 0xbe121e2a,
	     0x3e428b3d},
	    {0x3f16cf68, 0xbdcb74b9, 0xbe0db56f, 0x3f15b098, 0x3d3b8972, 0x3dfe3ade, 0x3f153c90, 0x3e123fe8, 0x3f174370, 0xbe20d7e8,
	     0x3e469b57},
	    {0x3f226bd5, 0xbdc6852a, 0xbdee92ee, 0x3f17502b, 0x3d84252a, 0x3e07c977, 0x3f214eec, 0x3e20d1bd, 0x3f186d14, 0xbe1051bd,
	     0x3e4aacf7},
	    {0x3f18823c, 0xbd16b185, 0xbe174af5, 0x3f35bdc4, 0x3d2d7185, 0x3df1f5ea, 0x3f3a26f0, 0x3dec8636, 0x3f141910, 0xbe14931b,
	     0x3e510f8b},
	}};
	const auto point = [&](size_t i) {
		const auto& p = k_Points.at(i);
		return glm::vec3(std::bit_cast<float>(p[0]), std::bit_cast<float>(p[1]), std::bit_cast<float>(p[2]));
	};
	Rope rope {};
	rope.start = point(0);
	rope.end = point(k_PointCount - 1);
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		rope.nodes.at(i).position = point(i + 1);
	}
	const auto ribbon = BuildRibbon(rope, glm::vec3(3.7f, 5.3f, -2.9f), Flat);
	for (size_t i = 0; i < k_PointCount; ++i)
	{
		const auto& want = k_Corners.at(i);
		const auto& plus = ribbon.rope.at(i * 2).position;
		const auto& minus = ribbon.rope.at((i * 2) + 1).position;
		const auto& shadowPlus = ribbon.shadow.at(i * 2).position;
		const auto& shadowMinus = ribbon.shadow.at((i * 2) + 1).position;
		const std::array<uint32_t, 11> got {Bits(plus.x),
		                                    Bits(plus.y),
		                                    Bits(plus.z),
		                                    Bits(minus.x),
		                                    Bits(minus.y),
		                                    Bits(minus.z),
		                                    Bits(shadowPlus.x),
		                                    Bits(shadowPlus.z),
		                                    Bits(shadowMinus.x),
		                                    Bits(shadowMinus.z),
		                                    Bits(ribbon.rope.at(i * 2).uv.x)};
		EXPECT_EQ(got, want) << "point " << i;
		EXPECT_EQ(Bits(shadowPlus.y), 0x3DCCCCCDu) << "point " << i;
	}
}
