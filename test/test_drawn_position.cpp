/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::DrawnPosition is DrawnModel's translation for every source (HandDrawPose, PhysicsDrawPose, DrawPosition with
// its shear, Transform), and the priority between them; and the whole matrix as it stands and from a physics pose

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
glm::mat3 Turn(float angle)
{
	return glm::mat3(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.3f, 1.0f, 0.2f)));
}

void ExpectSame(const Registry& registry, entt::entity entity, const glm::vec3& expected)
{
	const auto position = DrawnPosition(registry, entity);
	const auto model = glm::vec3(DrawnModel(registry, entity)[3]);
	EXPECT_EQ(position, model);
	EXPECT_EQ(position, expected);
}
} // namespace

TEST(DrawnPosition, EverySourceIsDrawnModelsTranslation)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f, 2.0f, 3.0f), Turn(0.4f), glm::vec3(1.5f));
	ExpectSame(registry, entity, glm::vec3(1.0f, 2.0f, 3.0f));

	auto& draw = registry.Assign<DrawPosition>(entity);
	draw.position = glm::vec3(4.0f, 5.0f, 6.0f);
	draw.rotation = Turn(1.1f);
	draw.shearX = 0.25f;
	draw.shearZ = -0.5f;
	ExpectSame(registry, entity, glm::vec3(4.0f, 5.0f, 6.0f));

	registry.Assign<PhysicsDrawPose>(entity, glm::vec3(7.0f, 8.0f, 9.0f), Turn(2.0f));
	ExpectSame(registry, entity, glm::vec3(7.0f, 8.0f, 9.0f));

	registry.Assign<HandDrawPose>(entity, glm::vec3(10.0f, 11.0f, 12.0f), Turn(-0.7f));
	ExpectSame(registry, entity, glm::vec3(10.0f, 11.0f, 12.0f));
}

// raffclar's cases (0222f9f5, test_physics_draw_matrix.cpp): a thing out of the physics is drawn as it stands, a
// moving body from its pose
TEST(DrawnPosition, AThingOutOfThePhysicsIsDrawnAsItStands)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(10.0f, 2.0f, -4.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(DrawnModel(registry, entity), glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, -4.0f)));
}

TEST(DrawnPosition, AMovingBodyIsDrawnFromItsPose)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(10.0f, 2.0f, -4.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<PhysicsDrawPose>(entity, glm::vec3(1.0f, -0.5f, 3.0f), glm::mat3(2.0f));
	const auto drawn = DrawnModel(registry, entity);
	EXPECT_EQ(drawn[3], glm::vec4(1.0f, -0.5f, 3.0f, 1.0f));
	EXPECT_EQ(drawn[0], glm::vec4(2.0f, 0.0f, 0.0f, 0.0f));
}

// The drawing's own pose (smooth between turns, ECS/Components/DrawPosition.h) is only the renderer's: the game's
// readers keep the original's pose
TEST(DrawnPosition, TheDrawingsPoseIsOnlyTheRenderers)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& draw = registry.Assign<DrawPosition>(entity);
	draw.position = glm::vec3(4.0f, 5.0f, 6.0f);
	draw.drawPosition = glm::vec3(7.0f, 8.0f, 9.0f);
	// not set yet: the drawing too takes the original's pose
	EXPECT_EQ(glm::vec3(DrawnModel(registry, entity, true, PoseOf::Drawing)[3]), glm::vec3(4.0f, 5.0f, 6.0f));
	draw.hasDrawPose = true;
	EXPECT_EQ(glm::vec3(DrawnModel(registry, entity, true, PoseOf::Drawing)[3]), glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(DrawnPosition(registry, entity), glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(glm::vec3(DrawnBodyModel(registry, entity)[3]), glm::vec3(4.0f, 5.0f, 6.0f));
}

// A turn is drawn smoothly only when every move of it was one of the living's own steps, each from where the last
// one ended, and it still stands where the last one put it
TEST(DrawnPosition, OnlyATurnOfItsOwnStepsIsDrawnSmoothly)
{
	const glm::vec3 start(0.0f, 1.0f, 0.0f);
	const glm::vec3 end(2.0f, 1.0f, 0.0f);
	// no move: smooth while it stands where the turn started
	EXPECT_TRUE(DrawnSmoothly(true, false, false, start, start, start));
	// moved without a step of its own (a jump): not smooth
	EXPECT_FALSE(DrawnSmoothly(true, false, false, start, start, end));
	// its own steps, and it is where the last one put it
	EXPECT_TRUE(DrawnSmoothly(true, true, false, start, end, end));
	// moved again after its last step (a jump after the walk)
	EXPECT_FALSE(DrawnSmoothly(true, true, false, start, end, glm::vec3(9.0f, 1.0f, 0.0f)));
	// a step that did not start where the last one ended (a jump before the walk)
	EXPECT_FALSE(DrawnSmoothly(true, true, true, start, end, end));
	// before its first turn
	EXPECT_FALSE(DrawnSmoothly(false, false, false, start, start, start));
}

TEST(DrawnPosition, AStepFromWhereTheLastEndedKeepsTheTurnSmooth)
{
	Registry registry;
	const auto entity = registry.Create();
	auto& transform = registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& draw = registry.Assign<DrawPosition>(entity);
	draw.started = true;
	// the first step starts at the turn's start
	transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
	NoteContinuousMove(registry, entity, glm::vec3(0.0f));
	EXPECT_TRUE(draw.continuousMoved);
	EXPECT_FALSE(draw.continuousBroken);
	EXPECT_EQ(draw.continuousEnd, glm::vec3(1.0f, 0.0f, 0.0f));
	// the next one where it ended
	transform.position = glm::vec3(2.0f, 0.0f, 0.0f);
	NoteContinuousMove(registry, entity, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_FALSE(draw.continuousBroken);
	// a step from somewhere else: something else moved it in between
	transform.position = glm::vec3(6.0f, 0.0f, 0.0f);
	NoteContinuousMove(registry, entity, glm::vec3(5.0f, 0.0f, 0.0f));
	EXPECT_TRUE(draw.continuousBroken);
	// a jump the game tells of starts the turn afresh
	SnapTurnStart(registry, entity);
	EXPECT_FALSE(draw.continuousBroken);
	EXPECT_FALSE(draw.continuousMoved);
	EXPECT_EQ(draw.turnStart, glm::vec3(6.0f, 0.0f, 0.0f));
}

// An animal turns once per turn: it is drawn turning between the turn's start and end rotations, the shortest way
TEST(DrawnPosition, AnAnimalIsDrawnTurningBetweenItsTurns)
{
	const glm::mat3 from = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.2f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::mat3 to = glm::mat3(glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto halfway = TurnBetween(from, to, 0.5f);
	const auto expected = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.6f, glm::vec3(0.0f, 1.0f, 0.0f)));
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			EXPECT_NEAR(halfway[i][j], expected[i][j], 1e-5f) << i << j;
			EXPECT_NEAR(TurnBetween(from, to, 0.0f)[i][j], from[i][j], 1e-5f) << i << j;
			EXPECT_NEAR(TurnBetween(from, to, 1.0f)[i][j], to[i][j], 1e-5f) << i << j;
		}
	}
	// past a half turn it goes the short way round
	const glm::mat3 behind = glm::mat3(glm::rotate(glm::mat4(1.0f), 3.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::mat3 ahead = glm::mat3(glm::rotate(glm::mat4(1.0f), -3.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto across = TurnBetween(behind, ahead, 0.5f);
	EXPECT_NEAR(std::abs(across[0][0]), 1.0f, 1e-5f); // facing straight back, not forward
	EXPECT_LT(across[0][0], 0.0f);
}
