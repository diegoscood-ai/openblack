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

#include <bit>
#include <iterator>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/WallHugRules.h"
#include "WallHugArcGolden.h"

namespace golden = openblack::test::wall_hug_arc_golden;
using openblack::ecs::wall_hug::ArcAngle;

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
