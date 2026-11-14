/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/IntroRules.h"

using namespace openblack::ecs;
namespace light = openblack::ecs::intro_rules::light;

namespace
{
/// Draws are the middle of their range, so that a test can tell the sizes the flicker gives
float Middle(float a, float b)
{
	return (a + b) * 0.5f;
}

light::Light FallingLight()
{
	return light::Make(intro_rules::k_BoySpot, intro_rules::k_LightWay, Middle);
}
} // namespace

TEST(IntroRules, TheLightStartsFourKilometresUpItsWayAndAimsAtTheBoy)
{
	const auto made = FallingLight();
	EXPECT_NEAR(made.way.x, -0.69714f, 1e-4f);
	EXPECT_NEAR(made.way.y, -0.16731f, 1e-4f);
	EXPECT_NEAR(made.way.z, -0.69714f, 1e-4f);
	EXPECT_NEAR(made.start.x, 4204.75f, 0.01f);
	EXPECT_NEAR(made.start.y, 669.3f, 0.1f);
	EXPECT_NEAR(made.start.z, 4841.92f, 0.01f);
	EXPECT_EQ(made.state, light::State::Falling);
}

TEST(IntroRules, TheLightFallsSteadilyAndArrivesTenMetresShort)
{
	auto falling = FallingLight();
	falling.elapsed = 1000;
	const auto second = light::HeadPosition(falling);
	// The engine's inverse square root leaves the way a hair over unit length
	EXPECT_NEAR(glm::length(second - falling.start), 450.0f, 0.05f);
	EXPECT_FALSE(falling.arrived);
	falling.elapsed = 8866;
	static_cast<void>(light::HeadPosition(falling));
	EXPECT_FALSE(falling.arrived);
	falling.elapsed = 8867;
	const auto landed = light::HeadPosition(falling);
	EXPECT_TRUE(falling.arrived);
	EXPECT_NEAR(landed.x, 1423.31f, 0.01f);
	EXPECT_NEAR(landed.y, 1.67f, 0.01f);
	EXPECT_NEAR(landed.z, 2060.47f, 0.01f);
}

TEST(IntroRules, TheHandHoldsTheBoyBelowItsGripUntilItLetsGo)
{
	// A bone moved up a metre, the grip point a metre along it, the hand at a place
	const std::array<glm::mat4, 2> bones {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 20.0f));
	const auto held = intro_rules::GripPoint(1817, bones, 1, glm::vec3(1.0f, 0.0f, 0.0f), model);
	ASSERT_TRUE(held.has_value());
	EXPECT_NEAR(held->x, 11.0f, 1e-5f);
	EXPECT_NEAR(held->y, 1.0f - 0.65f, 1e-5f);
	EXPECT_NEAR(held->z, 20.0f, 1e-5f);
	EXPECT_FALSE(intro_rules::GripPoint(1818, bones, 1, glm::vec3(0.0f), model).has_value());
	EXPECT_FALSE(intro_rules::GripPoint(0, bones, 2, glm::vec3(0.0f), model).has_value());
}
