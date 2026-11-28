/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ring of light round a worship site's altar (src/Worship/TotemRing.h), as the original's altar draws it
// (docs/wiki/magic/worship.md#the-ring-of-light-round-the-altar).

#include <cmath>
#include <cstdint>

#include <limits>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/LightSheet.h"
#include "Worship/TotemRing.h"

using namespace openblack::worship;
using openblack::particles::LightSheet;

TEST(TotemRing, TenPointsOnACircleOfThreeRoundTheDanceTheLastOnTheFirst)
{
	const glm::vec3 centre(1000.5f, 12.25f, 2000.75f);
	const auto points = totem_ring::Points(centre);
	ASSERT_EQ(points.size(), 10u);
	// the first point straight along x, at the height of the middle of the dance
	EXPECT_EQ(points.front(), glm::vec3(centre.x + 3.0f, centre.y, centre.z));
	for (const auto& point : points)
	{
		EXPECT_EQ(point.y, centre.y);
		EXPECT_NEAR(glm::distance(point, centre), 3.0f, 1e-3f);
	}
	// a ninth of a turn apart, so the tenth closes the ring on the first
	EXPECT_NEAR(points.back().x, points.front().x, 1e-3f);
	EXPECT_NEAR(points.back().z, points.front().z, 1e-3f);
	EXPECT_NEAR(points.at(1).z - centre.z, 3.0f * std::sin(6.2831855f / 9.0f), 1e-4f);
}

TEST(TotemRing, TheStrengthIsHowFullTheBatteryIsKeptToZeroToOne)
{
	EXPECT_FLOAT_EQ(totem_ring::Strength(25.0f, 100.0f), 0.25f);
	EXPECT_EQ(totem_ring::Strength(150.0f, 100.0f), 1.0f);
	EXPECT_EQ(totem_ring::Strength(0.0f, 100.0f), 0.0f);
	// an empty battery of no size is not a number, which gives no light
	EXPECT_EQ(totem_ring::Strength(0.0f, 0.0f), 0.0f);
	EXPECT_EQ(totem_ring::Strength(-1.0f, 100.0f), 0.0f);
}

TEST(TotemRing, TheColourIsThePlayersAndThrobsWithTheStrainPulseOnlyWhileOverloaded)
{
	// not overloaded: the player's colour, without its alpha
	EXPECT_EQ(totem_ring::Colour(0xFFC08040u, false, 0.5f), 0xC08040u);
	// a pulse of a half is 127 of 255: each channel times 127, over 256, cut
	EXPECT_EQ(totem_ring::Colour(0xFFC08040u, true, 0.5f), 0x5F3F1Fu);
	// a full pulse keeps 255 of 256 of each channel, an empty one leaves black
	EXPECT_EQ(totem_ring::Colour(0x00FFFFFFu, true, 1.0f), 0xFEFEFEu);
	EXPECT_EQ(totem_ring::Colour(0xFFFFFFFFu, true, 0.0f), 0x000000u);
}

TEST(TotemRing, TheStarsSlideDownAsFastAsTheSiteIsStrained)
{
	EXPECT_EQ(totem_ring::SlideSpeed(0.0f), -0.0f);
	EXPECT_EQ(totem_ring::SlideSpeed(2.5f), -2.5f);
	// a sheet that is given no speed slides up once a second, as every other sheet of light
	LightSheet sheet;
	sheet.Start({glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)}, 0xFFFFFFu, 10.0f, 0.03f);
	EXPECT_EQ(sheet.SlideSpeed(), LightSheet::k_SlideSpeed);
	sheet.SetSlideSpeed(totem_ring::SlideSpeed(0.5f));
	EXPECT_EQ(sheet.SlideSpeed(), -0.5f);
}
