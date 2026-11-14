/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's scale for its distance from the view, pinned to the original's bits: each value is the original's own
// setting of the hand's distance from the view, run whole in the original executable

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <iterator>

#include <gtest/gtest.h>

#include "ECS/HandLandPoint.h"
#include "HandScaleGolden.h"
#include "support/OracleCaseGenerator.h"

namespace golden = openblack::test::hand_scale_golden;
using openblack::ecs::hand_land_point::ScaleForViewDistance;
using openblack::test::OracleCaseGenerator;

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

constexpr size_t k_Stepped = 8000;
constexpr std::array<float, 7> k_Reaches = {1800.0f, 1000.0f, 300.0f, 150.0f, 40.0f, 5.0f, 1.0f};

} // namespace

TEST(HandScale, TheScaleForTheViewDistanceGivesTheOriginalsBits)
{
	OracleCaseGenerator generator {0x48414E44U};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_Scales); ++i)
	{
		// distances stepped through the floats from 2 to 10 at the full reach, then any up to 2048 at other reaches
		const bool stepped = i < k_Stepped;
		const float distance = stepped ? F(0x40000000U + static_cast<uint32_t>(i) * 2359U) : generator.Unit(24) * 2048.0f;
		const float reach = stepped ? 1800.0f : k_Reaches.at((i - k_Stepped) % k_Reaches.size());
		const uint32_t scale = Bits(ScaleForViewDistance(distance, reach));
		if (scale != golden::k_Scales[i] && ++mismatches <= 10)
		{
			ADD_FAILURE() << "distance " << distance << " reach " << reach << ": " << std::hex << scale << ", the original's "
			              << golden::k_Scales[i];
		}
	}
	EXPECT_EQ(mismatches, 0);
}
