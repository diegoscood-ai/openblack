/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <numbers>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/SharkRules.h"

using namespace openblack::ecs::shark_rules;

TEST(SharkRules, ASharkFacesTheWayItMoved)
{
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, 5}, 1.0f), std::numbers::pi_v<float> / 2.0f);
	// Turning the other way comes out as the angle from +x the long way round
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, -5}, 1.0f), 1.5f * std::numbers::pi_v<float>);
	// Standing still across the land, or only rising, it faces the way it did
	EXPECT_FLOAT_EQ(Heading({3, 0, 4}, {3, 7, 4}, 1.25f), 1.25f);
}

TEST(SharkRules, ASharkIsDrawnBetweenItsTurns)
{
	const auto drawn = Drawn({0, 0, 0}, 1.0f, {10, 0, 20}, 3.0f, 0.25f);
	EXPECT_FLOAT_EQ(drawn.x, 2.5f);
	EXPECT_FLOAT_EQ(drawn.y, 1.5f);
	EXPECT_FLOAT_EQ(drawn.z, 5.0f);
}

TEST(SharkRules, TheRendererDrawsASharkBetweenItsTurns)
{
	// as the mobile drawing leaves it: drawn where its turn ended, sheared on the slope
	openblack::ecs::components::DrawPosition draw;
	draw.position = {10, 0, 20};
	draw.drawPosition = {10, 0, 20};
	draw.shearX = 0.2f;
	draw.drawShearX = 0.2f;
	draw.drawShearZ = -0.1f;
	draw.hasDrawPose = true;
	const glm::mat3 facing(glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec3(-1, 0, 0));
	const auto drawn = Drawn({0, 0, 0}, 1.0f, {10, 0, 20}, 3.0f, 0.25f);
	PoseDrawing(draw, drawn, facing);
	// both the game's pose and the renderer's are a quarter of the way through the turn, facing its heading, upright
	EXPECT_EQ(draw.position, drawn);
	EXPECT_EQ(draw.drawPosition, drawn);
	EXPECT_EQ(draw.rotation, facing);
	EXPECT_EQ(draw.drawRotation, facing);
	EXPECT_EQ(draw.shearX, 0.0f);
	EXPECT_EQ(draw.drawShearX, 0.0f);
	EXPECT_EQ(draw.drawShearZ, 0.0f);
	EXPECT_TRUE(draw.hasDrawPose);
}

TEST(SharkRules, TheWakeRingsComeOnceMoreThanFiftyMillisecondsHaveGone)
{
	int32_t timer = 0;
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 30).has_value());
	EXPECT_EQ(timer, 30);
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 20).has_value());
	EXPECT_EQ(timer, 50);
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 30).has_value());
	EXPECT_EQ(timer, 80);
	const auto ring = WakeRing(timer, {1, 2, 3}, 0.5f, 30);
	ASSERT_TRUE(ring.has_value());
	EXPECT_EQ(timer, 60);
	// On the water under the point, turned with the shark
	EXPECT_EQ(ring->position, glm::vec3(1, 0, 3));
	EXPECT_FLOAT_EQ(ring->angle, 0.5f);
	EXPECT_FLOAT_EQ(ring->growth, 10.0f);
	EXPECT_FLOAT_EQ(ring->rate, 0.5f);
	// Half as deep as it is wide, as the original makes it
	EXPECT_FLOAT_EQ(ring->aspect, 0.5f);
	EXPECT_EQ(ring->cell, 0x31);
	EXPECT_EQ(ring->argb, 0x90FFFFFFu);
}
