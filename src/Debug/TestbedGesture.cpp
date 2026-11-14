/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedGesture.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include "Debug/GesturesModel.h"

using namespace openblack;
using namespace openblack::testbed_gesture;
using magic::gestures::Gesture;
using magic::gestures::GestureData;

bool testbed_gesture::DrawnWithAction(Gesture gesture)
{
	return gesture == magic::gestures::k_Circle;
}

std::vector<glm::ivec2> testbed_gesture::StrokeOf(std::span<const GestureData> templates, Gesture gesture,
                                                  glm::ivec2 windowSize)
{
	const auto* gestureTemplate = debug::gestures_window::FirstTemplate(templates, gesture);
	if (gestureTemplate == nullptr || windowSize.x <= 0 || windowSize.y <= 0)
	{
		return {};
	}
	const glm::vec2 size(windowSize);
	const auto points = debug::gestures_window::TemplateStroke(*gestureTemplate, k_StrokeWidth, size * 0.5f, size.x / size.y);
	if (points.size() < 2)
	{
		return {};
	}
	return debug::gestures_window::MousePositions(points);
}

size_t testbed_gesture::PositionAt(float seconds, size_t count)
{
	if (count == 0 || seconds <= 0.0f)
	{
		return 0;
	}
	const auto sent = static_cast<size_t>(std::floor(seconds / k_MessageSeconds));
	return sent == 0 ? 0 : std::min(sent - 1, count - 1);
}

std::optional<Drawing> testbed_gesture::StartDrawing(std::vector<glm::ivec2> pixels, Gesture gesture)
{
	if (pixels.size() < 2)
	{
		return std::nullopt;
	}
	return Drawing {.pixels = std::move(pixels), .gesture = gesture, .holdAction = DrawnWithAction(gesture)};
}

DrawStep testbed_gesture::Step(Drawing& drawing, float seconds, bool strokePlaying)
{
	DrawStep step;
	switch (drawing.stage)
	{
	case Drawing::Stage::Pressing:
		step.startStroke = true;
		step.pointer = drawing.pixels.front();
		drawing.stage = Drawing::Stage::Playing;
		drawing.seconds = 0.0f;
		break;
	case Drawing::Stage::Playing:
		if (!strokePlaying)
		{
			step.pointer = drawing.pixels.back();
			drawing.stage = Drawing::Stage::Holding;
			drawing.seconds = 0.0f;
			break;
		}
		drawing.seconds += seconds;
		step.pointer = drawing.pixels.at(PositionAt(drawing.seconds, drawing.pixels.size()));
		break;
	case Drawing::Stage::Holding:
		drawing.seconds += seconds;
		if (drawing.seconds >= k_HoldAfterSeconds)
		{
			step.releaseAction = drawing.holdAction;
			step.done = true;
			drawing.stage = Drawing::Stage::Done;
		}
		break;
	case Drawing::Stage::Done:
		step.done = true;
		break;
	}
	return step;
}

bool testbed_gesture::TookAGesture(float cooldownBefore, float cooldownNow)
{
	return cooldownNow > cooldownBefore;
}
