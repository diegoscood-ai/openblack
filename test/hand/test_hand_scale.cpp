/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's scale for its distance from the view, pinned to the original's bits: each value is the original's own
// setting of the hand's distance from the view, run whole in the original executable. And the camera state's ease of
// that distance over 0.4 s, short of the land under the cursor

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <iterator>

#include <gtest/gtest.h>

#include "Common/Zoomer.h"
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

// One step of the whole 0.4 s from rest lands on the distance wanted; a step of 0.1 s goes 67/256 of the way, as the
// quartic that ends at no speed and no acceleration does
TEST(HandCameraStateEase, TheDistanceEasesOverFourTenthsOfASecond)
{
	using openblack::ecs::hand_land_point::CameraStateDistance;
	openblack::Zoomer whole;
	whole.SetPosition(10.0f);
	EXPECT_FLOAT_EQ(CameraStateDistance(whole, 20.0f, 0.4f, 1000.0f), 20.0f);
	openblack::Zoomer part;
	part.SetPosition(10.0f);
	EXPECT_NEAR(CameraStateDistance(part, 20.0f, 0.1f, 1000.0f), 12.6171875f, 1e-4f);
}

// The ease starts again from where the distance is, at its speed, every step: four steps of 0.1 s are short of the
// distance wanted, where one of 0.4 s reaches it
TEST(HandCameraStateEase, EachStepEasesAgainFromWhereTheDistanceIs)
{
	using openblack::ecs::hand_land_point::CameraStateDistance;
	openblack::Zoomer distance;
	distance.SetPosition(10.0f);
	float drawn = 0.0f;
	for (int step = 0; step < 4; ++step)
	{
		drawn = CameraStateDistance(distance, 20.0f, 0.1f, 1000.0f);
	}
	EXPECT_NEAR(drawn, 19.6226f, 1e-3f);
	EXPECT_GT(distance.speed, 0.0f);
}

// The hand is never drawn past the land under the cursor, while the eased distance goes on from where it would be
TEST(HandCameraStateEase, TheHandIsNeverDrawnPastTheLandUnderTheCursor)
{
	using openblack::ecs::hand_land_point::CameraStateDistance;
	openblack::Zoomer distance;
	distance.SetPosition(10.0f);
	EXPECT_FLOAT_EQ(CameraStateDistance(distance, 20.0f, 0.4f, 15.0f), 15.0f);
	EXPECT_FLOAT_EQ(distance.value, 20.0f);
	// nearer than the land, the eased distance itself
	EXPECT_FLOAT_EQ(CameraStateDistance(distance, 20.0f, 0.4f, 25.0f), 20.0f);
}
