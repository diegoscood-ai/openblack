/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "3D/Lightning.h"

using namespace openblack;

TEST(Lightning, FlashFlickersAsItFades)
{
	auto flash = lightning::Strike(glm::vec3(0.0f), 500.0f, lightning::k_ThunderStrength);
	EXPECT_FLOAT_EQ(lightning::Brightness(flash), 1.0f);
	// A game turn at a time, as the storms update
	std::vector<float> brightnesses;
	for (int i = 0; i < 9; ++i)
	{
		flash = lightning::Advance(flash, 0.1f);
		brightnesses.push_back(lightning::Brightness(flash));
	}
	EXPECT_NEAR(brightnesses[0], 0.9f, 1e-6f);
	EXPECT_NEAR(brightnesses[1], 0.8f, 1e-6f);
	// Down to a tenth after 0.2 seconds, then back up and fading out
	EXPECT_NEAR(brightnesses[2], 0.1f, 1e-6f);
	EXPECT_NEAR(brightnesses[3], 0.1f, 1e-6f);
	EXPECT_NEAR(brightnesses[4], 0.5f, 1e-6f);
	EXPECT_NEAR(brightnesses[6], 0.3f, 1e-6f);
	// Out once older than 0.8 s
	EXPECT_FALSE(flash.active);
	EXPECT_FLOAT_EQ(brightnesses[8], 0.0f);
}

TEST(Lightning, BoltsFlashAtHalfStrength)
{
	const auto flash = lightning::Strike(glm::vec3(0.0f), 500.0f, lightning::k_BoltStrength);
	EXPECT_FLOAT_EQ(lightning::Brightness(flash), 0.5f);
	EXPECT_EQ(lightning::LandLightFlash(lightning::Brightness(flash)), 127);
	EXPECT_EQ(lightning::LandLightFlash(1.0f), 255);
	EXPECT_EQ(lightning::LandLightFlash(-1.0f), 0);
}
