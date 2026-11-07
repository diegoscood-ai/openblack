/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>

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

TEST(SkyDome, NoBlendColourFollowsTheSkyType)
{
	// By day and up to half way to dusk every scale is at least 1: the land's colour as it is
	EXPECT_EQ(sky_dome::NoBlendColour(0xFF8090A0u, 0.0f), glm::u8vec3(0x80, 0x90, 0xA0));
	EXPECT_EQ(sky_dome::NoBlendColour(0xFF8090A0u, 0.5f), glm::u8vec3(0x80, 0x90, 0xA0));
	// Past dusk blue goes first, then green, then red; at night red and green are halved, blue too
	EXPECT_EQ(sky_dome::NoBlendColour(0xFFFFFFFFu, 1.37f), glm::u8vec3(0xFF, 0xFF, 0xF7));
	EXPECT_EQ(sky_dome::NoBlendColour(0xFF515151u, 1.73f), glm::u8vec3(0x3E, 0x3B, 0x38));
	EXPECT_EQ(sky_dome::NoBlendColour(0xFF6A7BFEu, 2.0f), glm::u8vec3(0x35, 0x3D, 0x7F));
}

namespace
{
/// A case of the game's dome draw at the two lowest detail levels: its inputs and the colours it drew each dome with
struct NoBlendCase
{
	float skyType;
	uint32_t landColour;
	float overcast;
	bool fog;
	glm::vec3 haze;
	bool weather;
	uint8_t darkness;
	uint8_t flash;
	uint32_t modulate; ///< 0x..RRGGBB
	uint32_t add;      ///< 0x..RRGGBB
};

glm::u8vec3 Rgb(uint32_t colour)
{
	return {(colour >> 16) & 0xFFu, (colour >> 8) & 0xFFu, colour & 0xFFu};
}
} // namespace

TEST(SkyDome, NoBlendTintMatchesTheOriginal)
{
	// The colours the original's dome draw gave, read from the exe with these inputs
	constexpr std::array<NoBlendCase, 10> k_Cases {{
	    {0.0f, 0xFF8090A0u, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0x8090A0u, 0x000000u},
	    {0.5f, 0xFF8090A0u, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0x8090A0u, 0x000000u},
	    {1.0f, 0xFFC8B4A0u, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0xC8B4A0u, 0x000000u},
	    {1.37f, 0xFFFFFFFFu, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0xFFFFF7u, 0x000000u},
	    {1.73f, 0xFF515151u, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0x3E3B38u, 0x000000u},
	    {2.0f, 0xFF6A7BFEu, 0.0f, false, {0.0f, 0.0f, 0.0f}, false, 0, 0, 0x353D7Fu, 0x000000u},
	    {0.3f, 0xFFDDCC77u, 0.6f, true, {40.7f, 50.2f, 61.9f}, false, 0, 0, 0x706F54u, 0x171D24u},
	    {1.6f, 0xFF9988FFu, 0.25f, true, {12.0f, 200.0f, 99.0f}, true, 47, 0, 0x647FA7u, 0x012E16u},
	    {0.9f, 0xFF336699u, 1.0f, true, {255.0f, 0.0f, 128.0f}, true, 89, 120, 0xE277ADu, 0xD63B89u},
	    {1.234f, 0xFF10F0E0u, 0.0f, false, {0.0f, 0.0f, 0.0f}, true, 60, 255, 0xFEFEFEu, 0x7E7E7Eu},
	}};
	for (const auto& c : k_Cases)
	{
		const auto tint = sky_dome::TintOf({
		    .colour = sky_dome::NoBlendColour(c.landColour, c.skyType),
		    .hazeColour = c.haze,
		    .overcast = c.overcast,
		    .flash = c.flash,
		    .darkness = c.darkness,
		    .fog = c.fog,
		    .weather = c.weather,
		});
		EXPECT_EQ(tint.modulate, Rgb(c.modulate)) << "sky type " << c.skyType;
		EXPECT_EQ(tint.add, Rgb(c.add)) << "sky type " << c.skyType;
	}
}
