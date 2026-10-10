/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's gesture drawing (Debug/TestbedGesture): a gesture's stroke across the middle of the window, the pointer
// following the stroke player's messages, the Action button held for the circle only and let go after the last
// message, and a recognition told by the recogniser's cooldown. Pure: made-up templates, no game, no input state.

#include <cmath>
#include <cstddef>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Debug/GesturesModel.h"
#include "Debug/TestbedGesture.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureTemplates.h"

using namespace openblack;
using namespace openblack::testbed_gesture;
using namespace openblack::magic::gestures;

namespace
{
constexpr glm::ivec2 k_Window {1024, 768};
constexpr float k_Ratio = 1024.0f / 768.0f;
constexpr float k_Frame = 0.016f;

/// A made-up template: key points at the corners of a unit polyline, with their headings and turns
GestureData TemplateOf(Gesture gesture, const std::vector<glm::vec2>& corners)
{
	GestureData tpl;
	tpl.SetToZero();
	tpl.gesture = gesture;
	tpl.positionMode = 2;
	tpl.checkDirection = true;
	float previous = 0.0f;
	for (size_t k = 0; k < corners.size(); ++k)
	{
		KeySample s;
		s.x = corners[k].x;
		s.z = corners[k].y;
		if (k + 1 < corners.size())
		{
			const auto d = corners[k + 1] - corners[k];
			const float heading = Atan2Positive(d.x, d.y);
			s.direction = Octant(heading);
			s.turn = k == 0 ? 0.0f : WrapDifference(previous, heading);
			previous = heading;
		}
		tpl.Append(s);
	}
	return tpl;
}

std::vector<GestureData> MadeUpTemplates()
{
	return {
	    TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}),
	    TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}),
	};
}

/// A drawing of so many made-up positions
Drawing DrawingOf(Gesture gesture, size_t count)
{
	std::vector<glm::ivec2> pixels;
	for (size_t i = 0; i < count; ++i)
	{
		pixels.emplace_back(static_cast<int>(i) * 10, 100);
	}
	auto drawing = StartDrawing(pixels, gesture);
	EXPECT_TRUE(drawing.has_value());
	return drawing.value_or(Drawing {});
}
} // namespace

TEST(TestbedGesture, OnlyTheCircleIsDrawnWithTheActionButtonHeld)
{
	EXPECT_TRUE(DrawnWithAction(k_Circle));
	EXPECT_FALSE(DrawnWithAction(k_Spiral));
	EXPECT_FALSE(DrawnWithAction(k_InverseSpiral));
	EXPECT_FALSE(DrawnWithAction(k_Scribble));
	EXPECT_FALSE(DrawnWithAction(k_RShape));
	EXPECT_FALSE(DrawnWithAction(k_None));
}

TEST(TestbedGesture, NoStrokeWithoutATemplateOrAWindow)
{
	const auto templates = MadeUpTemplates();
	EXPECT_TRUE(StrokeOf(templates, k_Scribble, k_Window).empty());
	EXPECT_TRUE(StrokeOf({}, k_Circle, k_Window).empty());
	EXPECT_TRUE(StrokeOf(templates, k_Circle, {0, 768}).empty());
	EXPECT_TRUE(StrokeOf(templates, k_Circle, {1024, 0}).empty());
}

TEST(TestbedGesture, TheStrokeIsTheTemplateAcrossTheMiddleOfTheWindow)
{
	const auto templates = MadeUpTemplates();
	const auto stroke = StrokeOf(templates, k_Circle, k_Window);
	// As the Gestures window draws it: its first template, the stroke's width wide round the window's middle
	const auto points =
	    debug::gestures_window::TemplateStroke(templates[1], k_StrokeWidth, glm::vec2(k_Window) * 0.5f, k_Ratio);
	EXPECT_EQ(stroke, debug::gestures_window::MousePositions(points));
	ASSERT_GE(stroke.size(), 2u);
	// The template's top left corner, 160 pixels left of the middle and 160 / (1024 / 768) above it
	EXPECT_EQ(stroke.front(), glm::ivec2(352, 264));
	// Every position within the template's box round the middle, give or take the rounding to whole pixels
	for (const auto pixel : stroke)
	{
		EXPECT_GE(pixel.x, 352);
		EXPECT_LE(pixel.x, 672);
		EXPECT_GE(pixel.y, 264);
		EXPECT_LE(pixel.y, 504);
	}
}

TEST(TestbedGesture, TheDrawnCircleIsRecognisedAsTheCircleOnly)
{
	const auto templates = MadeUpTemplates();
	GestureSystem system;
	for (const auto& pixel : StrokeOf(templates, k_Circle, k_Window))
	{
		system.AddSample(glm::vec3(pixel.x, 0.0f, pixel.y), pixel);
	}
	const auto matches = debug::gestures_window::MatchesNow(templates, BuildFromSystem(system, k_Ratio), k_Ratio);
	ASSERT_EQ(matches.size(), 1u);
	EXPECT_EQ(matches[0].gesture, k_Circle);
	EXPECT_FALSE(matches[0].mirrored);
}

TEST(TestbedGesture, ThePointerIsAtTheLastMessageSent)
{
	EXPECT_EQ(PositionAt(0.0f, 10), 0u);
	EXPECT_EQ(PositionAt(-1.0f, 10), 0u);
	EXPECT_EQ(PositionAt(k_MessageSeconds * 0.5f, 10), 0u);
	EXPECT_EQ(PositionAt(k_MessageSeconds * 1.5f, 10), 0u);
	EXPECT_EQ(PositionAt(k_MessageSeconds * 2.5f, 10), 1u);
	EXPECT_EQ(PositionAt(k_MessageSeconds * 9.5f, 10), 8u);
	EXPECT_EQ(PositionAt(k_MessageSeconds * 100.0f, 10), 9u);
	EXPECT_EQ(PositionAt(1.0f, 0), 0u);
}

TEST(TestbedGesture, ADrawingNeedsTwoPositions)
{
	EXPECT_FALSE(StartDrawing({}, k_Circle).has_value());
	EXPECT_FALSE(StartDrawing({{1, 2}}, k_Circle).has_value());
	const auto circle = StartDrawing({{1, 2}, {3, 4}}, k_Circle);
	ASSERT_TRUE(circle.has_value());
	EXPECT_TRUE(circle->holdAction);
	EXPECT_EQ(circle->stage, Drawing::Stage::Pressing);
	const auto spiral = StartDrawing({{1, 2}, {3, 4}}, k_Spiral);
	ASSERT_TRUE(spiral.has_value());
	EXPECT_FALSE(spiral->holdAction);
}

TEST(TestbedGesture, ACircleIsPressedPlayedHeldAndLetGo)
{
	auto drawing = DrawingOf(k_Circle, 20);
	// The frame after the press: the stroke starts at its first position
	auto step = Step(drawing, k_Frame, false);
	EXPECT_TRUE(step.startStroke);
	EXPECT_EQ(step.pointer, std::optional<glm::ivec2>(glm::ivec2(0, 100)));
	EXPECT_FALSE(step.releaseAction);
	EXPECT_FALSE(step.done);
	EXPECT_EQ(drawing.stage, Drawing::Stage::Playing);

	// The pointer follows the messages as they are sent, and the stroke starts only once
	float seconds = 0.0f;
	for (int frame = 0; frame < 10; ++frame)
	{
		step = Step(drawing, k_Frame, true);
		seconds += k_Frame;
		EXPECT_FALSE(step.startStroke);
		EXPECT_FALSE(step.releaseAction);
		EXPECT_FALSE(step.done);
		EXPECT_EQ(step.pointer, std::optional<glm::ivec2>(drawing.pixels.at(PositionAt(seconds, 20))));
	}

	// Every position sent: the pointer at the last, the button still held
	step = Step(drawing, k_Frame, false);
	EXPECT_EQ(step.pointer, std::optional<glm::ivec2>(glm::ivec2(190, 100)));
	EXPECT_FALSE(step.releaseAction);
	EXPECT_FALSE(step.done);
	EXPECT_EQ(drawing.stage, Drawing::Stage::Holding);

	// Held a little longer, then let go
	int frames = 0;
	do
	{
		step = Step(drawing, k_Frame, false);
		++frames;
	} while (!step.done && frames < 100);
	EXPECT_TRUE(step.done);
	EXPECT_TRUE(step.releaseAction);
	EXPECT_FALSE(step.pointer.has_value());
	EXPECT_EQ(frames, static_cast<int>(std::ceil(k_HoldAfterSeconds / k_Frame)));
	EXPECT_EQ(drawing.stage, Drawing::Stage::Done);

	// Done stays done, with nothing more to let go
	step = Step(drawing, k_Frame, false);
	EXPECT_TRUE(step.done);
	EXPECT_FALSE(step.releaseAction);
}

TEST(TestbedGesture, AGestureWithoutTheButtonLetsNothingGo)
{
	auto drawing = DrawingOf(k_Scribble, 5);
	EXPECT_TRUE(Step(drawing, k_Frame, false).startStroke);
	EXPECT_FALSE(Step(drawing, k_Frame, true).done);
	EXPECT_FALSE(Step(drawing, k_Frame, false).done);
	DrawStep step;
	for (int frame = 0; frame < 100 && !step.done; ++frame)
	{
		step = Step(drawing, k_Frame, false);
		EXPECT_FALSE(step.releaseAction);
	}
	EXPECT_TRUE(step.done);
}

TEST(TestbedGesture, ARecognitionStartsTheCooldownAgain)
{
	EXPECT_FALSE(TookAGesture(0.0f, 0.0f));
	EXPECT_TRUE(TookAGesture(0.0f, 0.4f));
	// Running down is no recognition, starting again from part way down is
	EXPECT_FALSE(TookAGesture(0.4f, 0.384f));
	EXPECT_TRUE(TookAGesture(0.1f, 0.4f));
	EXPECT_FALSE(TookAGesture(0.0f, -0.016f));
}
