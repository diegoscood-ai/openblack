/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/SkyDome.h"

using namespace openblack;

TEST(SkyDome, AlignmentPairs)
{
	// 0 good, 1 neutral, 2 evil: the first dome whole and the second over it
	EXPECT_EQ(sky_dome::AlignmentPair(-0.5f), (sky_dome::Pair {.lower = 0, .upper = 1, .weight = 0}));
	EXPECT_EQ(sky_dome::AlignmentPair(0.5f), (sky_dome::Pair {.lower = 0, .upper = 1, .weight = 127}));
	// Neutral is still good into neutral, all neutral
	EXPECT_EQ(sky_dome::AlignmentPair(1.0f), (sky_dome::Pair {.lower = 0, .upper = 1, .weight = 255}));
	EXPECT_EQ(sky_dome::AlignmentPair(1.25f), (sky_dome::Pair {.lower = 1, .upper = 2, .weight = 63}));
	EXPECT_EQ(sky_dome::AlignmentPair(3.0f), (sky_dome::Pair {.lower = 1, .upper = 2, .weight = 255}));
}

TEST(SkyDome, DarknessOfAGoodSky)
{
	// Evil and neutral skies are not darkened, nor one a little good
	EXPECT_EQ(sky_dome::Darkness(2.0f), 0);
	EXPECT_EQ(sky_dome::Darkness(1.0f), 0);
	EXPECT_EQ(sky_dome::Darkness(0.8f), 0);
	// Good at 0.75 of the way: (0.75 - 0.6) * 225
	EXPECT_EQ(sky_dome::Darkness(0.5f), 33);
	// The float 0.6 is a little over it, so even a wholly good sky only reaches 89
	EXPECT_EQ(sky_dome::Darkness(0.0f), 89);
	EXPECT_EQ(sky_dome::Darkness(-1.0f), 90);
	// The product is rounded to a float before it is cut: 46.9999999 in doubles, 47 in floats
	EXPECT_EQ(sky_dome::Darkness(0.38222217559814453f), 47);
}

TEST(SkyDome, ClearDayIsUntinted)
{
	const auto tint = sky_dome::TintOf({.hazeColour = glm::vec3(60.0f, 70.0f, 80.0f),
	                                    .overcast = 0.0f,
	                                    .flash = 0,
	                                    .darkness = 0,
	                                    .fog = true,
	                                    .weather = true});
	EXPECT_EQ(tint, (sky_dome::Tint {.modulate = glm::u8vec3(255), .add = glm::u8vec3(0)}));
}

TEST(SkyDome, OvercastTurnsTowardsTheHaze)
{
	// A full overcast of 255: white towards the haze, rounded down, and the haze added, rounded down
	const auto tint = sky_dome::TintOf({.hazeColour = glm::vec3(60.9f, 100.0f, 255.0f),
	                                    .overcast = 1.0f,
	                                    .flash = 0,
	                                    .darkness = 0,
	                                    .fog = true,
	                                    .weather = true});
	EXPECT_EQ(tint.modulate, glm::u8vec3(60, 100, 255));
	EXPECT_EQ(tint.add, glm::u8vec3(59, 99, 254));
	// No further past a full one
	const auto past = sky_dome::TintOf({.hazeColour = glm::vec3(60.9f, 100.0f, 255.0f),
	                                    .overcast = 1.27f,
	                                    .flash = 0,
	                                    .darkness = 0,
	                                    .fog = true,
	                                    .weather = true});
	EXPECT_EQ(past, tint);
	// Without the fog setting the overcast changes nothing
	const auto noFog = sky_dome::TintOf(
	    {.hazeColour = glm::vec3(60.0f), .overcast = 1.0f, .flash = 0, .darkness = 0, .fog = false, .weather = true});
	EXPECT_EQ(noFog, sky_dome::Tint {});
}

TEST(SkyDome, OvercastBelowZeroWrapsTheChannels)
{
	// -0.56 is -142 of 255: the colours leave their 8-bit channels and keep only their low bytes, darkened after that
	const auto tint = sky_dome::TintOf({.hazeColour = glm::vec3(20.0f, 100.0f, 200.0f),
	                                    .overcast = -0.56f,
	                                    .flash = 0,
	                                    .darkness = 0,
	                                    .fog = true,
	                                    .weather = true});
	EXPECT_EQ(tint.modulate, glm::u8vec3(129, 84, 29));
	EXPECT_EQ(tint.add, glm::u8vec3(244, 200, 145));
	const auto dark = sky_dome::TintOf({.hazeColour = glm::vec3(20.0f, 100.0f, 200.0f),
	                                    .overcast = -0.56f,
	                                    .flash = 0,
	                                    .darkness = 90,
	                                    .fog = true,
	                                    .weather = true});
	EXPECT_EQ(dark.modulate, glm::u8vec3(101, 66, 22));
	EXPECT_EQ(dark.add, glm::u8vec3(192, 157, 114));
}

TEST(SkyDome, DarknessDarkensAndFlashWhitens)
{
	// 90 * 90 / 150 = 54 of 256 off, rounded up: 255 - 54 = 201
	const auto dark = sky_dome::TintOf(
	    {.hazeColour = glm::vec3(0.0f), .overcast = 0.0f, .flash = 0, .darkness = 90, .fog = true, .weather = true});
	EXPECT_EQ(dark.modulate, glm::u8vec3(201));
	EXPECT_EQ(sky_dome::TintOf(
	              {.hazeColour = glm::vec3(0.0f), .overcast = 0.0f, .flash = 0, .darkness = 90, .fog = true, .weather = false})
	              .modulate,
	          glm::u8vec3(255));
	// A full flash: the first nearly white, the second added half as far
	const auto flash = sky_dome::TintOf(
	    {.hazeColour = glm::vec3(0.0f), .overcast = 0.0f, .flash = 255, .darkness = 90, .fog = true, .weather = true});
	EXPECT_EQ(flash.modulate, glm::u8vec3(201 + ((54 * 255) >> 8)));
	EXPECT_EQ(flash.add, glm::u8vec3((255 * 127) >> 8));
}

TEST(SkyDome, SunAndMoonDimThroughAnOvercast)
{
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(200.0f, 0.0f, true), 200.0f);
	// A full overcast leaves a ninth, in whole steps
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(200.0f, 1.0f, true), 22.0f);
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(255.0f, 0.5f, true), 51.0f);
	// No further past a full one, not at all without the fog setting, nor below 0
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(200.0f, 2.0f, true), 22.0f);
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(200.0f, 1.0f, false), 200.0f);
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(200.0f, -0.5f, true), 200.0f);
	// The alpha is a whole number first, as the game's colour byte is
	EXPECT_FLOAT_EQ(sky_dome::ThroughOvercast(127.9f, 0.0f, true), 127.0f);
}
