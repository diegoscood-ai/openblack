/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The arc a walker going round a circle looks along, pinned to the original's bits: for every pair of 100 directions
// and 100 arc ends, the original's own normalisation, dot product, arccosine and cross product, run in the original
// executable in the order its look along a circle takes them, going clockwise and anticlockwise

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <iterator>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/WallHugRules.h"
#include "WallHugArcGolden.h"

namespace golden = openblack::test::wall_hug_arc_golden;
using openblack::ecs::wall_hug::ArcAngle;
using openblack::ecs::wall_hug::OrbitTurns;

namespace
{

[[nodiscard]] float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

} // namespace

TEST(WallHugArc, TheArcAngleGivesTheOriginalsBits)
{
	constexpr size_t k_Ends = std::size(golden::k_Ends);
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_Directions); ++i)
	{
		const glm::vec2 dir {F(golden::k_Directions[i][0]), F(golden::k_Directions[i][1])};
		for (size_t j = 0; j < k_Ends; ++j)
		{
			const glm::vec2 end {F(golden::k_Ends[j][0]), F(golden::k_Ends[j][1])};
			const auto& expected = golden::k_Arcs[i * k_Ends + j];
			const uint32_t clockwise = Bits(ArcAngle(end, dir, true));
			const uint32_t anticlockwise = Bits(ArcAngle(end, dir, false));
			if (clockwise != expected[0] || anticlockwise != expected[1])
			{
				if (++mismatches <= 10)
				{
					ADD_FAILURE() << "direction " << i << " end " << j << ": " << std::hex << clockwise << " " << anticlockwise
					              << ", the original's " << expected[0] << " " << expected[1];
				}
			}
		}
	}
	EXPECT_EQ(mismatches, 0);
}

TEST(WallHugArc, AProductPastAFullCosineGivesNotANumber)
{
	// The animals' look takes the product of two vectors as they come, unclamped: one unit past 1 or -1 the original's
	// arccosine gives the FPU's default not a number, which the turns' compare then counts as too close
	constexpr uint32_t k_NotANumber = 0xFFC00000U;
	const glm::vec2 dir {1.0f, 0.0f};
	for (const float x : {F(0x3F800001U), F(0xBF800001U)})
	{
		EXPECT_EQ(Bits(ArcAngle({x, 0.0f}, dir, true)), k_NotANumber);
		EXPECT_EQ(Bits(ArcAngle({x, 0.0f}, dir, false)), k_NotANumber);
	}
}

// The orbit's turns in the game's order, each product and the quotient a float: here the double product of the same
// values rounds one unit of the last place higher (1113075050)
TEST(WallHugArc, TheOrbitsTurnsRoundEachStepToAFloatAsTheGameDoes)
{
	const auto angle = std::bit_cast<float>(1066537973u);
	const auto radius = std::bit_cast<float>(1084220773u);
	EXPECT_EQ(std::bit_cast<uint32_t>(OrbitTurns(angle, radius, 461)), 1113075049u);
}

// The orbit's turns as the original works them out on its 24-bit FPU: its own steps (0x616547..0x616587) run in place
// on the v1.42 exe through the oracle, for random angles, radii and speeds; 20 of the 24 cases are ones a chain in
// double missed (docs/wiki/beings/wall-hugging.md#walking-round-obstacles-mobilewallhug)
TEST(WallHugArc, TheOrbitsTurnsAreTheOriginalsOwnToTheBit)
{
	struct Case
	{
		uint32_t angle;
		uint32_t radius;
		uint16_t speed;
		uint32_t turns;
	};
	constexpr std::array k_Cases = {
	    Case {0x408E8499u, 0x4191B629u, 2015, 0x432FE371u}, Case {0x3F08B082u, 0x42144C0Eu, 282, 0x43995904u},
	    Case {0x400438D9u, 0x412E5952u, 1105, 0x42B20640u}, Case {0x40416A61u, 0x421A579Bu, 2851, 0x4332B384u},
	    Case {0x40A4F2BDu, 0x3F074FEDu, 832, 0x4164EB03u},  Case {0x3E3A1DB5u, 0x420F9236u, 2145, 0x41549ABEu},
	    Case {0x3E6898CFu, 0x41CD8773u, 1666, 0x4174DC9Du}, Case {0x406CB126u, 0x3E938BC0u, 346, 0x4157530Fu},
	    Case {0x40A77110u, 0x3EBA0ECCu, 1850, 0x408FB355u}, Case {0x40950088u, 0x41B28418u, 2600, 0x432E99A9u},
	    Case {0x3F02BCCFu, 0x3D8774F9u, 64, 0x401393DAu},   Case {0x4030AA7Au, 0x403568A0u, 2128, 0x41808428u},
	    Case {0x3E7B3452u, 0x41809E65u, 30, 0x440F9922u},   Case {0x40B4473Eu, 0x40C5B933u, 2485, 0x4274CE99u},
	    Case {0x3FB6F022u, 0x400C8077u, 1898, 0x40E71ED6u}, Case {0x40052962u, 0x401071F1u, 2235, 0x4112E075u},
	    Case {0x3F58A373u, 0x41B8C498u, 888, 0x42C05373u},  Case {0x404F43B8u, 0x4021A237u, 2273, 0x417B8A16u},
	    Case {0x3F08C536u, 0x420F8063u, 2153, 0x421B946Bu}, Case {0x3FB36867u, 0x403F982Fu, 2693, 0x40D9D6ECu},
	    Case {0x40A758BDu, 0x40F9A53Au, 1621, 0x42DBED04u}, Case {0x3E68FF5Du, 0x41FEC0BDu, 1902, 0x418526FCu},
	    Case {0x40C1F0ACu, 0x3E31D2D7u, 2353, 0x3FFA23F0u}, Case {0x408EF49Fu, 0x4201B74Eu, 2056, 0x4399EDCFu},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(OrbitTurns(std::bit_cast<float>(c.angle), std::bit_cast<float>(c.radius), c.speed)),
		          c.turns);
	}
}
