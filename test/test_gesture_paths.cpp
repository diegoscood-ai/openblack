/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <span>
#include <vector>

#include <GestureFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Gestures/GesturePaths.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureTemplateFile.h"

using namespace openblack;
using namespace openblack::gesture;

namespace
{
constexpr float k_ScreenAspect = 4.0f / 3.0f;

/// The recogniser's key points of a path drawn in whole pixels
magic::gestures::GestureData Record(std::span<const glm::vec2> path)
{
	magic::gestures::GestureSystem samples;
	for (const auto& point : path)
	{
		samples.AddSample(glm::vec3(point.x, 0.0f, point.y), glm::ivec2(point));
	}
	return magic::gestures::BuildFromSystem(samples, k_ScreenAspect);
}

std::span<const magic::gestures::KeySample> KeysOf(const magic::gestures::GestureData& data)
{
	return std::span(data.samples).first(data.count);
}

/// Up, right and down: a fork drawn as the game's templates were, by recording a drawing and keeping its key points
gestures::GestureTemplate ForkTemplate(bool checkAspect)
{
	const std::array<glm::vec2, 4> fork {glm::vec2 {100.0f, 400.0f}, {100.0f, 100.0f}, {300.0f, 100.0f}, {300.0f, 400.0f}};
	const auto keys = Record(Trace(fork));
	return MakeTemplate(GestureType::ForkDown, KeysOf(keys), true, false, checkAspect, k_ScreenAspect);
}
} // namespace

TEST(GesturePaths, MakeTemplateFitsTheKeyPointsIntoABoxFromZeroToOne)
{
	const std::array<magic::gestures::KeySample, 3> keys {
	    magic::gestures::KeySample {.x = 100.0f, .z = 400.0f, .turn = 0.7f, .direction = 0},
	    magic::gestures::KeySample {.x = 100.0f, .z = 200.0f, .turn = 1.5f, .direction = 2},
	    magic::gestures::KeySample {.x = 200.0f, .z = 200.0f, .turn = -0.3f, .direction = 2},
	};
	const auto made = MakeTemplate(GestureType::Circle, keys, true, false, true, k_ScreenAspect);
	ASSERT_EQ(made.PointCount(), 3u);
	EXPECT_EQ(made.Gesture(), static_cast<uint8_t>(GestureType::Circle));
	EXPECT_EQ(made.PositionModeValue(), 2);
	EXPECT_TRUE(made.ChecksDirection());
	EXPECT_FALSE(made.AllowsMirror());
	EXPECT_TRUE(made.ChecksAspectRatio());
	// The larger side, 200 pixels tall, is the box's 1
	EXPECT_FLOAT_EQ(made.points[0].x, 0.0f);
	EXPECT_FLOAT_EQ(made.points[0].z, 1.0f);
	EXPECT_FLOAT_EQ(made.points[1].x, 0.0f);
	EXPECT_FLOAT_EQ(made.points[1].z, 0.0f);
	EXPECT_FLOAT_EQ(made.points[2].x, 0.5f);
	EXPECT_FLOAT_EQ(made.points[2].z, 0.0f);
	// The ends turn nowhere; the corner keeps its turn and every point its direction
	EXPECT_FLOAT_EQ(made.points[0].turn, 0.0f);
	EXPECT_FLOAT_EQ(made.points[1].turn, 1.5f);
	EXPECT_FLOAT_EQ(made.points[2].turn, 0.0f);
	EXPECT_EQ(made.points[1].direction, 2u);
	// The aspect is the recogniser's, from the box in pixels
	const magic::gestures::BoundingBox box {.minX = 100.0f, .minZ = 200.0f, .maxX = 200.0f, .maxZ = 400.0f};
	EXPECT_FLOAT_EQ(made.aspectRatio, magic::gestures::Aspect(box, k_ScreenAspect));
}

TEST(GesturePaths, MakeTemplateOfNoKeyPointsIsEmpty)
{
	const auto made = MakeTemplate(GestureType::Circle, {}, true, true, true, k_ScreenAspect);
	EXPECT_EQ(made.PointCount(), 0u);
	EXPECT_EQ(made.Gesture(), 0);
}

TEST(GesturePaths, TraceGestureDrawsATemplateTheRecogniserKnows)
{
	const std::vector<gestures::GestureTemplate> templates {ForkTemplate(true)};
	const auto path = TraceGesture(templates, GestureType::ForkDown, {512.0f, 384.0f}, 300.0f, k_ScreenAspect);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(*path, TraceTemplate(templates.front(), {512.0f, 384.0f}, 300.0f));
	// Drawn again, the path is the fork
	const auto keys = Record(*path);
	magic::gestures::Result result;
	std::vector<magic::gestures::GestureData> list {magic::gestures::TemplateFromFile(templates.front())};
	EXPECT_TRUE(magic::gestures::MatchGesture(list, static_cast<magic::gestures::Gesture>(GestureType::ForkDown), keys, result,
	                                          k_ScreenAspect));
}

TEST(GesturePaths, TraceGestureOfAGestureWithNoTemplateIsNone)
{
	const std::vector<gestures::GestureTemplate> templates {ForkTemplate(true)};
	EXPECT_FALSE(TraceGesture(templates, GestureType::Circle, {512.0f, 384.0f}, 300.0f, k_ScreenAspect).has_value());
	EXPECT_FALSE(TraceGesture({}, GestureType::ForkDown, {512.0f, 384.0f}, 300.0f, k_ScreenAspect).has_value());
}

TEST(GesturePaths, TraceGestureOfATemplateItsOwnShapeDoesNotFitIsNone)
{
	// A template that wants a wide path, though its points draw one taller than wide
	auto wide = ForkTemplate(true);
	wide.aspectRatio = 5.0f;
	const std::vector<gestures::GestureTemplate> templates {wide};
	EXPECT_FALSE(TraceGesture(templates, GestureType::ForkDown, {512.0f, 384.0f}, 300.0f, k_ScreenAspect).has_value());
	// Without the aspect check the same shape is drawn
	wide.checkAspectRatio = 0;
	const std::vector<gestures::GestureTemplate> unchecked {wide};
	EXPECT_TRUE(TraceGesture(unchecked, GestureType::ForkDown, {512.0f, 384.0f}, 300.0f, k_ScreenAspect).has_value());
}
