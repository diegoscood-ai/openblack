/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's pointer maths (Debug/TestbedPointer): the fixed cursor's fractions turn back into the same pixels as
// the hand's ray reads them, a sweep moves whole pixels and carries the rest, and the fixed cursor is put back as it
// was. Pure: no game, no input state.

#include <cstring>

#include <array>
#include <optional>
#include <string>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Debug/TestbedPointer.h"
#include "Input/GameCursor.h"
#include "Input/MouseButtons.h"

using namespace openblack;
using namespace openblack::testbed_pointer;

namespace
{
/// The pixel the hand's ray takes from the fixed cursor's text, as the game reads it
glm::ivec2 RayPixel(const std::string& text, glm::ivec2 windowSize)
{
	const auto fraction = ReadMouseAt(text.c_str());
	EXPECT_TRUE(fraction.has_value()) << text;
	return PixelOf(fraction.value_or(glm::vec2(0.0f)), windowSize);
}

/// Whether two fixed cursors are the same: both unset, or the same text
bool SameMouseAt(const char* a, const char* b)
{
	if (a == nullptr || b == nullptr)
	{
		return a == b;
	}
	return std::strcmp(a, b) == 0;
}
} // namespace

TEST(TestbedPointer, EveryPixelComesBackThroughTheFixedCursor)
{
	constexpr std::array<glm::ivec2, 5> k_Windows {{{1024, 768}, {1280, 720}, {1920, 1080}, {3840, 2160}, {7, 3}}};
	for (const auto window : k_Windows)
	{
		// Every column along one row, and every row along one column
		for (int x = 0; x < window.x; ++x)
		{
			const glm::ivec2 pixel {x, window.y / 2};
			EXPECT_EQ(RayPixel(MouseAtText(FractionOf(pixel, window)), window), pixel) << window.x << "x" << window.y;
		}
		for (int y = 0; y < window.y; ++y)
		{
			const glm::ivec2 pixel {window.x / 3, y};
			EXPECT_EQ(RayPixel(MouseAtText(FractionOf(pixel, window)), window), pixel) << window.x << "x" << window.y;
		}
	}
}

TEST(TestbedPointer, TheCornersComeBack)
{
	const glm::ivec2 window {1024, 768};
	for (const auto pixel : {glm::ivec2(0, 0), glm::ivec2(1023, 0), glm::ivec2(0, 767), glm::ivec2(1023, 767)})
	{
		EXPECT_EQ(PixelOf(FractionOf(pixel, window), window), pixel);
		EXPECT_EQ(RayPixel(MouseAtText(FractionOf(pixel, window)), window), pixel);
	}
}

TEST(TestbedPointer, TheTextReadsBackAsTheSameFraction)
{
	const glm::vec2 fraction = FractionOf({511, 383}, {1024, 768});
	const auto read = ReadMouseAt(MouseAtText(fraction).c_str());
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->x, fraction.x);
	EXPECT_EQ(read->y, fraction.y);
	EXPECT_FALSE(ReadMouseAt(nullptr).has_value());
	EXPECT_FALSE(ReadMouseAt("").has_value());
	EXPECT_FALSE(ReadMouseAt("0.5").has_value());
}

TEST(TestbedPointer, SharesGoToTheNearestPixelInsideTheWindow)
{
	const glm::ivec2 window {1000, 500};
	EXPECT_EQ(PixelAtShare({0.5f, 0.5f}, window), glm::ivec2(500, 250));
	EXPECT_EQ(PixelAtShare({0.1234f, 0.0f}, window), glm::ivec2(123, 0));
	EXPECT_EQ(PixelAtShare({0.1236f, 1.0f}, window), glm::ivec2(124, 500));
	EXPECT_EQ(ClampToWindow({1000, 500}, window), glm::ivec2(999, 499));
	EXPECT_EQ(ClampToWindow({-3, 20}, window), glm::ivec2(0, 20));
}

TEST(TestbedPointer, ASweepMovesWholePixelsAndCarriesTheRest)
{
	// 10 pixels across and 10 back up over a second, in quarters: 2.5 a step (shares a float holds exactly)
	auto sweep = StartSweep({10.0f / 1024.0f, -10.0f / 512.0f}, 1.0f, {1024, 512});
	EXPECT_FLOAT_EQ(sweep.pixelsPerSecond.x, 10.0f);
	EXPECT_FLOAT_EQ(sweep.pixelsPerSecond.y, -10.0f);
	constexpr std::array<int, 4> k_Across {2, 3, 2, 3};
	glm::ivec2 total {0};
	for (size_t i = 0; i < k_Across.size(); ++i)
	{
		const auto step = Advance(sweep, 0.25f);
		EXPECT_EQ(step.moved.x, k_Across.at(i)) << i;
		// Cut towards nothing, as whole pixels are: the same steps the other way
		EXPECT_EQ(step.moved.y, -k_Across.at(i)) << i;
		EXPECT_EQ(step.done, i + 1 == k_Across.size()) << i;
		total += step.moved;
	}
	EXPECT_EQ(total, glm::ivec2(10, -10));
}

TEST(TestbedPointer, ASweepNeverGoesBeyondItsEnd)
{
	auto sweep = StartSweep({0.25f, 0.0f}, 2.0f, {1024, 768});
	const auto first = Advance(sweep, 0.5f);
	EXPECT_EQ(first.moved, glm::ivec2(64, 0));
	EXPECT_FALSE(first.done);
	const auto rest = Advance(sweep, 10.0f);
	EXPECT_EQ(rest.moved, glm::ivec2(192, 0));
	EXPECT_TRUE(rest.done);
}

TEST(TestbedPointer, ASweepOverNoTimeMovesAtOnce)
{
	auto sweep = StartSweep({0.5f, 0.25f}, 0.0f, {100, 100});
	const auto step = Advance(sweep, 1.0f / 60.0f);
	EXPECT_EQ(step.moved, glm::ivec2(50, 25));
	EXPECT_TRUE(step.done);
}

TEST(TestbedPointer, TheFixedCursorIsPutBackAsItWas)
{
	const std::array<const char*, 2> environments {nullptr, "0.5,0.5"};
	const std::array<std::optional<std::string>, 3> overrides {std::nullopt, std::string {}, std::string("0.1,0.2")};
	for (const char* environment : environments)
	{
		for (const auto& before : overrides)
		{
			const char* was = input::MouseAtFrom(before, environment);
			const auto saved = SaveMouseAt(was);
			// The scenario holds the pointer meanwhile
			const std::optional<std::string> during = MouseAtText(FractionOf({10, 20}, {100, 100}));
			EXPECT_NE(input::MouseAtFrom(during, environment), nullptr);
			// Then puts back what it saved, over its own
			const std::optional<std::string> after = RestoredMouseAt(saved);
			EXPECT_TRUE(SameMouseAt(input::MouseAtFrom(after, environment), was))
			    << (environment != nullptr ? environment : "unset") << " / " << before.value_or("none");
		}
	}
}

TEST(TestbedPointer, ButtonsAreNumberedAsTheMouseNumbersThem)
{
	EXPECT_EQ(ButtonOf(1), input::MouseButton::Left);
	EXPECT_EQ(ButtonOf(2), input::MouseButton::Middle);
	EXPECT_EQ(ButtonOf(3), input::MouseButton::Right);
	EXPECT_EQ(ButtonOf(0), input::MouseButton::Other);
	EXPECT_EQ(ButtonOf(4), input::MouseButton::Other);
}

TEST(TestbedPointer, TheHandGripsWithTheLeftAndActsWithTheRight)
{
	input::MouseButtonsState buttons;
	EXPECT_FALSE(HandButtonsOf(buttons).gripping);
	EXPECT_FALSE(HandButtonsOf(buttons).action);
	input::ApplyMouseButton(buttons, {.button = ButtonOf(1), .down = true});
	EXPECT_TRUE(HandButtonsOf(buttons).gripping);
	EXPECT_FALSE(HandButtonsOf(buttons).action);
	input::ApplyMouseButton(buttons, {.button = ButtonOf(3), .down = true});
	EXPECT_TRUE(HandButtonsOf(buttons).action);
	input::ApplyMouseButton(buttons, {.button = ButtonOf(1), .down = false});
	input::ApplyMouseButton(buttons, {.button = ButtonOf(3), .down = false});
	EXPECT_FALSE(HandButtonsOf(buttons).gripping);
	EXPECT_FALSE(HandButtonsOf(buttons).action);
}
