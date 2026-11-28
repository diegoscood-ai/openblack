/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a path's keypoints are matched with the gesture templates (src/Magic/Gestures/GestureMatch.h), against the wiki
// (docs/wiki/magic/gestures.md#the-match): the templates of a gesture tried in the file's order, the small turns that
// may be skipped, the match starting at any keypoint, the first direction and the aspect class. Synthetic keypoint lists
// only: each list is built from its turns, so no path has to be drawn.

#include <cstdint>

#include <initializer_list>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureTemplates.h"

using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Ratio = 4.0f / 3.0f;
constexpr float k_RightAngle = glm::half_pi<float>();

/// A keypoint list of a gesture: a start, a keypoint for each turn and an end, every keypoint at the origin and heading
/// up unless set otherwise
GestureData Keypoints(Gesture gesture, std::initializer_list<float> turns)
{
	GestureData data;
	data.SetToZero();
	data.gesture = gesture;
	data.positionMode = 2;
	data.Append({});
	for (const float turn : turns)
	{
		data.Append({.turn = turn});
	}
	data.Append({});
	return data;
}

/// The same with each keypoint at its own point on the screen (x, z), in order
GestureData Placed(Gesture gesture, std::initializer_list<float> turns, std::initializer_list<std::pair<float, float>> points)
{
	auto data = Keypoints(gesture, turns);
	int k = 0;
	for (const auto& [x, z] : points)
	{
		data.samples[static_cast<size_t>(k)].x = x;
		data.samples[static_cast<size_t>(k)].z = z;
		++k;
	}
	return data;
}

bool Matches(const std::vector<GestureData>& list, Gesture gesture, const GestureData& input, Result& result)
{
	return MatchGesture(list, gesture, input, result, k_Ratio);
}
} // namespace

TEST(GestureMatch, AGesturesTemplatesAreTriedInTheFilesOrderAndTheFirstThatFitsIsTaken)
{
	// another gesture's template that would fit, one of the circle's that does not, then two of the circle's that do
	const std::vector<GestureData> list {
	    Keypoints(k_Spiral, {k_RightAngle, k_RightAngle}),
	    Keypoints(k_Circle, {3.0f, 3.0f}),
	    Keypoints(k_Circle, {k_RightAngle, k_RightAngle}),
	    Keypoints(k_Circle, {k_RightAngle, k_RightAngle}),
	};
	const auto input = Keypoints(k_None, {k_RightAngle, k_RightAngle});
	Result result;
	ASSERT_TRUE(Matches(list, k_Circle, input, result));
	EXPECT_EQ(result.gesture, k_Circle);
	EXPECT_EQ(result.templateIndex, 2u);
	EXPECT_FALSE(result.reversed);
	// the spiral's own template is the first of the list
	ASSERT_TRUE(Matches(list, k_Spiral, input, result));
	EXPECT_EQ(result.templateIndex, 0u);
	// none of the gesture's templates fits: nothing is recognised
	EXPECT_FALSE(Matches({list[1]}, k_Circle, input, result));
	EXPECT_EQ(result.gesture, k_None);
}

TEST(GestureMatch, ASmallTurnUnderAboutThirtyDegreesMayBeSkipped)
{
	// about 29.5 degrees, as the original's constant
	EXPECT_EQ(k_SmallTurn, 0.515417576f);
	EXPECT_FLOAT_EQ(k_MaxError, 3.0f * glm::pi<float>() / 16.0f);
	const std::vector<GestureData> list {Keypoints(k_Circle, {k_RightAngle, k_RightAngle})};
	Result result;
	// a small extra turn between the template's two corners is taken in with the next one
	EXPECT_TRUE(Matches(list, k_Circle, Keypoints(k_None, {k_RightAngle, 0.5f, k_RightAngle}), result));
	EXPECT_EQ(result.start, 0u);
	EXPECT_EQ(result.end, 4u);
	// a turn just above the small turn is not, and the path strays too far from the template
	EXPECT_FALSE(Matches(list, k_Circle, Keypoints(k_None, {k_RightAngle, 0.55f, k_RightAngle}), result));
}

TEST(GestureMatch, TheMatchMayStartAtAnyKeypointOfThePath)
{
	const std::vector<GestureData> list {Keypoints(k_Circle, {k_RightAngle, k_RightAngle})};
	Result result;
	// a sharp turn drawn before the gesture: the match starts at the keypoint after it
	ASSERT_TRUE(Matches(list, k_Circle, Keypoints(k_None, {2.5f, k_RightAngle, k_RightAngle}), result));
	EXPECT_EQ(result.start, 1u);
	EXPECT_EQ(result.end, 4u);
	// without it the match starts at the path's start
	ASSERT_TRUE(Matches(list, k_Circle, Keypoints(k_None, {k_RightAngle, k_RightAngle}), result));
	EXPECT_EQ(result.start, 0u);
	EXPECT_EQ(result.end, 3u);
}

TEST(GestureMatch, ATemplateThatChecksDirectionNeedsThePathToSetOffTheSameWay)
{
	// the template sets off to the right (octant 2)
	auto tpl = Keypoints(k_Circle, {k_RightAngle, k_RightAngle});
	tpl.checkDirection = true;
	tpl.samples[0].direction = 2;
	auto input = Keypoints(k_None, {k_RightAngle, k_RightAngle});
	input.samples[1].direction = 4;
	Result result;

	input.samples[0].direction = 2;
	EXPECT_TRUE(Matches({tpl}, k_Circle, input, result));
	// setting off to the left, the same turns do not match
	input.samples[0].direction = 6;
	EXPECT_FALSE(Matches({tpl}, k_Circle, input, result));
	// a template that does not check the direction takes them
	tpl.checkDirection = false;
	EXPECT_TRUE(Matches({tpl}, k_Circle, input, result));

	// drawn the other way round, a template that may be mirrored needs the mirrored direction: 8 - 2, to the left
	tpl.checkDirection = true;
	tpl.allowReverse = true;
	auto mirrored = Keypoints(k_None, {-k_RightAngle, -k_RightAngle});
	mirrored.samples[0].direction = 6;
	ASSERT_TRUE(Matches({tpl}, k_Circle, mirrored, result));
	EXPECT_TRUE(result.reversed);
	mirrored.samples[0].direction = 2;
	EXPECT_FALSE(Matches({tpl}, k_Circle, mirrored, result));
}

TEST(GestureMatch, AScribbleTemplateThatChecksItsAspectNeedsStrokesWiderThanTall)
{
	// a back-and-forth template whose shape is wide (above 4)
	auto scribble = Keypoints(k_Scribble, {3.0f, -3.0f});
	scribble.checkAspect = true;
	scribble.aspect = 5.0f;
	EXPECT_EQ(k_AspectThin, 0.15f);
	EXPECT_EQ(k_AspectWide, 4.0f);
	Result result;
	// strokes 200 pixels across and 30 down: (200 + 1) / ((30 + 1) x 4/3), above 4
	const auto wide = Placed(k_None, {3.0f, -3.0f}, {{0.0f, 0.0f}, {200.0f, 10.0f}, {0.0f, 20.0f}, {200.0f, 30.0f}});
	EXPECT_TRUE(Matches({scribble}, k_Scribble, wide, result));
	// the same strokes about as wide as tall, or taller than wide, are not a scribble
	const auto square = Placed(k_None, {3.0f, -3.0f}, {{0.0f, 0.0f}, {100.0f, 25.0f}, {0.0f, 50.0f}, {100.0f, 75.0f}});
	EXPECT_FALSE(Matches({scribble}, k_Scribble, square, result));
	const auto tall = Placed(k_None, {3.0f, -3.0f}, {{0.0f, 0.0f}, {10.0f, 200.0f}, {20.0f, 0.0f}, {30.0f, 200.0f}});
	EXPECT_FALSE(Matches({scribble}, k_Scribble, tall, result));
	// a template that does not check its aspect takes them
	scribble.checkAspect = false;
	EXPECT_TRUE(Matches({scribble}, k_Scribble, tall, result));
}
