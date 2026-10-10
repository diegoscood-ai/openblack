/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a recognised gesture's trail gives its light sheet (src/Particles/GestureTrail.h), against the wiki
// (docs/bw1-notes/magic.md, "The light sheet"). Synthetic paths only.

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureShapes.h"
#include "Particles/GestureTrail.h"

using namespace openblack;
namespace trail = openblack::particles::gesture_trail;

namespace
{
/// An uneven bent path: segments of different lengths, so the even points fall inside them
magic::gestures::Path BentPath()
{
	magic::gestures::Path path;
	path.Add({0.0f, 0.0f, 0.0f});
	path.Add({3.0f, 0.0f, 0.0f});
	path.Add({3.0f, 1.5f, 4.0f});
	path.Add({-2.0f, 1.5f, 6.5f});
	path.Add({-2.0f, 7.0f, 6.5f});
	return path;
}
} // namespace

TEST(GestureTrailMaths, TheSheetRisesAndFallsOverItsLife)
{
	EXPECT_FLOAT_EQ(trail::SheetStrength(0.0f, 4.0f), 0.0f);
	EXPECT_FLOAT_EQ(trail::SheetStrength(2.0f, 4.0f), 1.0f);
	EXPECT_FLOAT_EQ(trail::SheetStrength(1.0f, 4.0f), 0.75f);
	EXPECT_FLOAT_EQ(trail::SheetStrength(6.0f, 4.0f), 0.0f);
}

TEST(GestureTrailMaths, TheSheetStrengthIsClampedOutsideItsLife)
{
	EXPECT_EQ(trail::SheetStrength(0.0f, 7.0f), 0.0f);
	EXPECT_EQ(trail::SheetStrength(7.0f, 7.0f), 0.0f);
	EXPECT_EQ(trail::SheetStrength(3.5f, 7.0f), 1.0f);
	EXPECT_EQ(trail::SheetStrength(-1.0f, 7.0f), 0.0f);
	EXPECT_EQ(trail::SheetStrength(20.0f, 7.0f), 0.0f);
	// Symmetric about half its life
	EXPECT_FLOAT_EQ(trail::SheetStrength(1.0f, 8.0f), trail::SheetStrength(7.0f, 8.0f));
}

TEST(GestureTrailMaths, TheSheetsConstants)
{
	EXPECT_EQ(trail::k_SheetPoints, 50);
	EXPECT_EQ(trail::k_SheetShiftSeconds, 0.03f);
}

TEST(GestureTrailMaths, TheSheetStandsOnFiftyPointsFromEndToEnd)
{
	const auto ideal = BentPath();
	const auto points = trail::SheetPoints(ideal);
	ASSERT_EQ(points.size(), 50u);
	EXPECT_EQ(points.front(), ideal[0]);
	EXPECT_EQ(points.back(), ideal[ideal.Size() - 1]);
}

TEST(GestureTrailMaths, TheSheetPointsAreTheSameFloatsAsBefore)
{
	// The rule took them as point i x (1 / 49) along the shape, in single precision
	const auto ideal = BentPath();
	std::vector<glm::vec3> before;
	for (int i = 0; i < 50; ++i)
	{
		before.push_back(ideal.At(static_cast<float>(i) * (1.0f / 49.0f)));
	}
	EXPECT_EQ(trail::SheetPoints(ideal), before);
}

TEST(GestureTrailMaths, AnEmptyShapeGivesPointsAtTheOrigin)
{
	const magic::gestures::Path empty {};
	const auto points = trail::SheetPoints(empty);
	ASSERT_EQ(points.size(), 50u);
	for (const auto& point : points)
	{
		EXPECT_EQ(point, glm::vec3(0.0f));
	}
}
