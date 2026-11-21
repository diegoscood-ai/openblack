/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a recognised gesture's trail gives its light sheet and its sparkles, and how the chain behind the drawing hand
// lays its links (src/Particles/GestureTrail.h, src/Particles/Utility.h), against the wiki
// (docs/wiki/magic/hand-casting.md#utility-effects, docs/wiki/magic/hand-casting.md#the-light-sheet-as-read-from-the-exe).
// Synthetic paths and plain values only.

#include <cmath>

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureShapes.h"
#include "Particles/GestureTrail.h"
#include "Particles/Utility.h"

using namespace openblack;
namespace trail = openblack::particles::gesture_trail;
namespace chain = openblack::particles::gesture_chain;

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

TEST(GestureTrailMaths, TheSparklesAreSizedByAHundredthOfTheShapesLength)
{
	EXPECT_FLOAT_EQ(trail::ShapeScale(40.0f), 0.4f);
	EXPECT_FLOAT_EQ(trail::ShapeScale(100.0f), 1.0f);
	EXPECT_FLOAT_EQ(trail::ShapeScale(250.0f), 2.5f);
	EXPECT_EQ(trail::ShapeScale(0.0f), 0.0f);
}

TEST(GestureTrailMaths, TheSparklesFlowFromThePathToTheShapeOverTimeToIdeal)
{
	// the game's TimeToIdeal (0.6 s) and InterpGain (0.9)
	constexpr float k_TimeToIdeal = 0.6f;
	constexpr float k_Gain = 0.9f;
	EXPECT_EQ(trail::IdealShare(2.0f, 2.0f, k_TimeToIdeal), 0.0f);
	EXPECT_NEAR(trail::IdealShare(2.3f, 2.0f, k_TimeToIdeal), 0.5f, 1e-6f);
	EXPECT_EQ(trail::IdealShare(2.7f, 2.0f, k_TimeToIdeal), 1.0f);
	// and stay on the shape afterwards
	EXPECT_EQ(trail::IdealShare(9.0f, 2.0f, k_TimeToIdeal), 1.0f);
	// pulled towards a smooth start and end: the ends and the middle stay, a quarter of the way is 0.25 - 0.09375 x 0.9
	EXPECT_EQ(trail::EasedIdealShare(0.0f, k_Gain), 0.0f);
	EXPECT_EQ(trail::EasedIdealShare(0.5f, k_Gain), 0.5f);
	EXPECT_EQ(trail::EasedIdealShare(1.0f, k_Gain), 1.0f);
	EXPECT_NEAR(trail::EasedIdealShare(0.25f, k_Gain), 0.165625f, 1e-6f);
	// no gain leaves the share as it is; a full gain is the smoothstep
	EXPECT_EQ(trail::EasedIdealShare(0.25f, 0.0f), 0.25f);
	EXPECT_NEAR(trail::EasedIdealShare(0.25f, 1.0f), 0.15625f, 1e-6f);
}

TEST(GestureTrailMaths, TheTrailIsRevealedFromBothEnds)
{
	// at the start nothing shows
	EXPECT_EQ(trail::RevealAlpha(0.0f, 0.0f, 0.0f), 0u);
	EXPECT_EQ(trail::RevealAlpha(0.5f, 0.0f, 0.0f), 0u);
	// half way: both ends at their brightest, fading to nothing a quarter in from each, the middle still dark
	EXPECT_EQ(trail::RevealAlpha(0.0f, 0.5f, 100.0f), 100u);
	EXPECT_EQ(trail::RevealAlpha(1.0f, 0.5f, 100.0f), 100u);
	EXPECT_EQ(trail::RevealAlpha(0.25f, 0.5f, 100.0f), 50u);
	EXPECT_EQ(trail::RevealAlpha(0.75f, 0.5f, 100.0f), 50u);
	EXPECT_EQ(trail::RevealAlpha(0.5f, 0.5f, 100.0f), 0u);
	// all the way, with the game's MaxAlpha: the two ends meet in the middle
	EXPECT_EQ(trail::RevealAlpha(0.5f, 1.0f, 176.087f), 176u);
	EXPECT_EQ(trail::RevealAlpha(0.0f, 1.0f, 176.087f), 176u);
	// never above 255
	EXPECT_EQ(trail::RevealAlpha(0.5f, 1.0f, 300.0f), 255u);
}

TEST(GestureTrailMaths, TheSparklesWiggleThenDisperseAndShrinkAway)
{
	// the game's WigglePhaseSpeed (1.5), DispersalTime (3 s) and ShrinkTimeAfterDispersal (2 s)
	const auto swell = [](float age) { return (std::cos(age * 1.5f) + 1.0f) * 0.5f; };
	EXPECT_FLOAT_EQ(trail::WiggleStrength(0.0f, 1.5f, 3.0f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(trail::WiggleStrength(2.0f, 1.5f, 3.0f, 2.0f), swell(2.0f));
	// from DispersalTime on the wiggle shrinks to nothing over the shrink time
	EXPECT_FLOAT_EQ(trail::WiggleStrength(3.0f, 1.5f, 3.0f, 2.0f), swell(3.0f));
	EXPECT_FLOAT_EQ(trail::WiggleStrength(4.0f, 1.5f, 3.0f, 2.0f), swell(4.0f) * 0.5f);
	EXPECT_EQ(trail::WiggleStrength(5.0f, 1.5f, 3.0f, 2.0f), 0.0f);
	EXPECT_EQ(trail::WiggleStrength(6.0f, 1.5f, 3.0f, 2.0f), 0.0f);
}

TEST(GestureTrailMaths, AfterACoupleOfSecondsTheSparklesFlashAndDieAway)
{
	// the flash runs from 2.4 s to 4.5 s
	EXPECT_EQ(trail::k_PulseStart, 2.4f);
	EXPECT_EQ(trail::k_PulseLength, 2.1f);
	EXPECT_FALSE(trail::CollectionPulseAlpha(2.0f, 255).has_value());
	EXPECT_FALSE(trail::CollectionPulseAlpha(2.4f, 255).has_value());
	// from the game's CollectionAlphaPulse (255) down, in whole steps
	EXPECT_EQ(trail::CollectionPulseAlpha(2.5f, 255).value_or(-1.0f), 242.0f);
	EXPECT_EQ(trail::CollectionPulseAlpha(3.0f, 255).value_or(-1.0f), 182.0f);
	EXPECT_EQ(trail::CollectionPulseAlpha(4.5f, 255).value_or(-1.0f), 0.0f);
	EXPECT_EQ(trail::CollectionPulseAlpha(10.0f, 255).value_or(-1.0f), 0.0f);
	EXPECT_EQ(trail::CollectionPulseAlpha(3.0f, 0).value_or(-1.0f), 0.0f);
}

TEST(GestureChain, ALinkIsLaidEachTimeTheHandHasMovedMinEmitDist)
{
	const glm::vec3 last(0.0f);
	// 20 links over a DieAge of 2 s: at most one link a step of 0.1 s
	EXPECT_FLOAT_EQ(chain::LinkStep(0.0f, last, {5.0f, 0.0f, 0.0f}, 2.0f, 20, 2.0f, 0.1f, true), 1.0f);
	// half of MinEmitDist moved since the last link: half a link owed
	EXPECT_FLOAT_EQ(chain::LinkStep(1.0f, last, {1.0f, 0.0f, 0.0f}, 2.0f, 20, 2.0f, 0.1f, true), 0.5f);
	// a long move is held to the most links a step
	EXPECT_FLOAT_EQ(chain::LinkStep(1.0f, last, {30.0f, 0.0f, 0.0f}, 2.0f, 20, 2.0f, 0.1f, true), 1.0f);
	EXPECT_FLOAT_EQ(chain::LinkStep(1.0f, last, {30.0f, 0.0f, 0.0f}, 2.0f, 10, 4.0f, 0.1f, true), 0.25f);
	// none once the chain stops emitting
	EXPECT_EQ(chain::LinkStep(1.0f, last, {30.0f, 0.0f, 0.0f}, 2.0f, 20, 2.0f, 0.1f, false), 0.0f);
}

TEST(GestureChain, TheHeadGoesFiveSecondsAfterTheEmissionStops)
{
	EXPECT_EQ(chain::k_HeadLinger, 5.0f);
	// emitting, it stays at any age
	EXPECT_FALSE(chain::HeadGone(true, 100.0f, 0.0f));
	// stopped at 3 s: still there at 8 s, gone after
	EXPECT_FALSE(chain::HeadGone(false, 3.5f, 3.0f));
	EXPECT_FALSE(chain::HeadGone(false, 8.0f, 3.0f));
	EXPECT_TRUE(chain::HeadGone(false, 8.5f, 3.0f));
}

TEST(GestureChain, TheChainIsSizedByTheCameraDistanceTable)
{
	// {0, 50, 500, 1500} -> {0.2, 1, 1, 1.5}, in straight lines between and held beyond
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(-10.0f), 0.2f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(0.0f), 0.2f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(25.0f), 0.6f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(50.0f), 1.0f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(300.0f), 1.0f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(500.0f), 1.0f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(1000.0f), 1.25f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(1500.0f), 1.5f);
	EXPECT_FLOAT_EQ(particles::utility::TrailScale(4000.0f), 1.5f);
}
