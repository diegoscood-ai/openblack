/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How far ahead the world camera finds the land (Camera/ClickDistance.h): each hit's distance, the harmonic mean with
// the no-hit distance, and the move towards the focus distance by the pitch, in the original's order of operations and
// with its floats (the pitch of the view below is the original's, from its own camera function)

#include <cstdint>

#include <array>
#include <bit>
#include <span>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ClickDistance.h"

using namespace openblack;

TEST(ClickDistance, TheConstantsAreTheGames)
{
	EXPECT_EQ(click_distance::k_Rays, 16);
	EXPECT_EQ(click_distance::k_NoHitDistance, 50.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(click_distance::k_PitchShare), 0x3D594CAFu);
}

TEST(ClickDistance, AHitIsAsFarAsItsPointFromTheOrigin)
{
	EXPECT_EQ(click_distance::HitDistance({1.0f, 2.0f, 3.0f}, {4.0f, 6.0f, 15.0f}), 13.0f);
}

TEST(ClickDistance, TheMeanIsHarmonicWithOneMoreHitAtTheNoHitDistance)
{
	// No hit gives the no-hit distance
	EXPECT_EQ(click_distance::MeanDistance({}), 50.0f);
	// 1 / ((1/50 + 1/80 + 1/100 + 1/250) / 4), each step a float
	const std::array<float, 3> hits {80.0f, 100.0f, 250.0f};
	EXPECT_EQ(std::bit_cast<uint32_t>(click_distance::MeanDistance(std::span<const float>(hits))), 0x42AC0B03u);
}

TEST(ClickDistance, TheSteeperTheViewTheNearerTheFocusDistance)
{
	// The view's pitch 0x3EB48127 times the share is 0x3C99378C of the way from 120.5 to 300
	const glm::vec3 origin {1123.5f, 160.25f, 905.75f};
	const glm::vec3 focus {1010.0f, 95.5f, 1040.25f};
	EXPECT_EQ(std::bit_cast<uint32_t>(click_distance::TowardsFocus(120.5f, 300.0f, origin, focus)), 0x42F7B6E7u);
	// Looking up, the mean is kept; straight down, near enough, the share is the pitch's, under 1
	EXPECT_EQ(click_distance::TowardsFocus(120.5f, 300.0f, focus, origin), 120.5f);
	EXPECT_LT(click_distance::TowardsFocus(120.5f, 300.0f, {0.0f, 100.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 300.0f);
}
