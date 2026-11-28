/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/HandLight.h"

using openblack::graphics::HandLight;

namespace
{
constexpr auto k_Hand = glm::vec3(1000.0f, 80.0f, 2000.0f);
} // namespace

TEST(HandLight, MapIsCentredOnTheHand)
{
	// 12 vertices 10 units apart span 110 units, half of them each side of the hand
	EXPECT_EQ(HandLight::GetOrigin(k_Hand), glm::vec2(945.0f, 1945.0f));
}

TEST(HandLight, ComesUpAsTheLandDarkens)
{
	EXPECT_FLOAT_EQ(HandLight::GetStrength(0xFFFFFFu), 0.0f);
	EXPECT_FLOAT_EQ(HandLight::GetStrength(0x787878u), 0.0f);
	// A mean of 112, eight fifteenths of the way down
	EXPECT_FLOAT_EQ(HandLight::GetStrength(0x707070u), 8.0f / 15.0f);
	EXPECT_FLOAT_EQ(HandLight::GetStrength(0x696969u), 1.0f);
	EXPECT_FLOAT_EQ(HandLight::GetStrength(0x000000u), 1.0f);
}

TEST(HandLight, TheMeanIsRoundedDown)
{
	// (110 + 111 + 111) / 3 is 110 and two thirds, which counts as 110
	EXPECT_EQ(HandLight::GetStrength(0x6E6F6Fu), 10.0f * (1.0f / 15.0f));
	// 119 and two thirds counts as 119: the light is already on, at a fifteenth
	EXPECT_EQ(HandLight::GetStrength(0x787877u), 1.0f * (1.0f / 15.0f));
	// (105 + 106 + 105) / 3 is 105 and a third, which counts as 105: full
	EXPECT_EQ(HandLight::GetStrength(0x696A69u), 1.0f);
}

TEST(HandLight, StrengthIsAFifteenthPerWholeValueOfTheMean)
{
	// A product by a fifteenth rather than a quotient by 15, which differs in the last bit for some values: three
	// fifteenths is not 3 / 15
	EXPECT_EQ(HandLight::GetStrength(0x757575u), 3.0f * (1.0f / 15.0f));
	EXPECT_NE(HandLight::GetStrength(0x757575u), 3.0f / 15.0f);
	for (uint32_t mean = 105; mean < 120; ++mean)
	{
		const uint32_t grey = (mean << 16) | (mean << 8) | mean;
		EXPECT_EQ(HandLight::GetStrength(grey), static_cast<float>(120 - mean) * (1.0f / 15.0f)) << mean;
		// a sum one or two over three times the mean still counts as the mean
		EXPECT_EQ(HandLight::GetStrength(grey + 1), HandLight::GetStrength(grey)) << mean;
		EXPECT_EQ(HandLight::GetStrength(grey + 2), HandLight::GetStrength(grey)) << mean;
	}
}

TEST(HandLight, StrengthFromTheColourAsFractions)
{
	// Each channel is rounded to its byte: a mean of 90 gives (120 - 90) / 15 = 2, so full
	EXPECT_FLOAT_EQ(HandLight::GetStrength(glm::vec3(90.0f / 255.0f)), 1.0f);
	EXPECT_FLOAT_EQ(HandLight::GetStrength(glm::vec3(120.0f / 255.0f)), 0.0f);
	EXPECT_FLOAT_EQ(HandLight::GetStrength(glm::vec3(111.0f / 255.0f)), 0.6f);
	// The bytes' mean is rounded down as from the packed colour
	EXPECT_EQ(HandLight::GetStrength(glm::vec3(110.0f, 111.0f, 111.0f) / 255.0f), 10.0f * (1.0f / 15.0f));
	EXPECT_EQ(HandLight::GetStrength(glm::vec3(110.0f, 111.0f, 111.0f) / 255.0f), HandLight::GetStrength(0x6E6F6Fu));
	EXPECT_EQ(HandLight::GetStrength(glm::vec3(117.0f / 255.0f)), HandLight::GetStrength(0x757575u));
}
