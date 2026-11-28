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
#include <cstdint>

#include <array>
#include <bit>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/Zoomer.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

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

// A villager's drawn yaw turns by the original's step, its products rounded at the FPU's 24 bits: within a quarter turn
// 0.003 a millisecond, beyond it (|d| 2/pi + |d| 2/pi) 0.003 with the game's double 2/pi (the values worked out exactly
// from the original's operations; the float formula gave 0x3EC81153 for the last)
TEST(DrawnPosition, AVillagersDrawnYawTurnsByTheOriginalsStep)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(VillagerDrawTurnStep(std::bit_cast<float>(0x3F000000u), 16)), 0x3D449BA6u);
	EXPECT_EQ(std::bit_cast<uint32_t>(VillagerDrawTurnStep(std::bit_cast<float>(0x3FF33333u), 17)), 0x3DFCAD08u);
	EXPECT_EQ(std::bit_cast<uint32_t>(VillagerDrawTurnStep(std::bit_cast<float>(0x40466666u), 33)), 0x3EC81154u);
}

// A bird's draw tests the screen with its matrix rolled by its bank, as the original's bird draw rolls it for the draw
// alone: rows 0 and 1 as c r0 - s r1 and c r1 + s r0, the cosine a float and the sine as the FPU leaves it (the values
// worked out from the original's operations, for a bank of 0.375 and the oracle's cells of a yaw placement)
TEST(DrawnPosition, ABirdsDrawRollsItsMatrixByItsBankAsTheOriginal)
{
	const auto f = [](uint32_t bits) { return std::bit_cast<float>(bits); };
	glm::mat4 cells(1.0f);
	cells[0] = glm::vec4(f(0xBF6358F0u), f(0x80000000u), f(0xBE9D5BC0u), 0.0f);
	cells[1] = glm::vec4(f(0x00000000u), f(0x3F7093B2u), f(0x00000000u), 0.0f);
	cells[2] = glm::vec4(f(0x3E9D5BC0u), f(0x00000000u), f(0xBF6358F0u), 0.0f);
	const auto rolled = RollByBank(cells, 0.375f);
	const std::array<uint32_t, 3> row0 {0xBF538C6Au, 0xBEB03BC3u, 0xBE926C56u};
	const std::array<uint32_t, 3> row1 {0xBEA68ACFu, 0x3F5FDBD2u, 0xBDE68B53u};
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(rolled[0][i]), row0.at(static_cast<size_t>(i))) << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(rolled[1][i]), row1.at(static_cast<size_t>(i))) << i;
	}
	EXPECT_EQ(rolled[2], cells[2]);
	EXPECT_EQ(rolled[3], cells[3]);
}

// A body in flight is drawn at the scale the physics took when it went in (the original's drawing scales its rows by
// the body's own scale), not at a scale its Transform was given since; with none (a prediction) at its Transform's
TEST(DrawnPosition, AFlightIsDrawnAtTheBodysScale)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	auto& pose = registry.Assign<PhysicsDrawPose>(entity);
	pose.position = glm::vec3(4.0f, 5.0f, 6.0f);
	pose.rotation = affine::AngleY(0.5f);
	pose.scale = 1.5f;
	EXPECT_EQ(DrawnModel(registry, entity), affine::Model(pose.position, pose.rotation, glm::vec3(1.5f)));
	pose.scale.reset();
	EXPECT_EQ(DrawnModel(registry, entity), affine::Model(pose.position, pose.rotation, glm::vec3(2.0f)));
}

// A villager's drawn yaw starts at 0, as its constructor starts it, and turns from there by the draw's step; once the
// step reaches its y angle it is drawn at that angle as it is, not wrapped
TEST(DrawnPosition, AVillagersDrawnYawStartsAtZeroAndTakesItsAngleAsItIs)
{
	DrawPosition draw;
	const float first = StepDrawnYaw(draw, 1.0f, 16);
	EXPECT_EQ(first, VillagerDrawTurnStep(1.0f, 16));
	EXPECT_EQ(draw.yaw, first);
	EXPECT_TRUE(draw.hasObjectYaw);

	DrawPosition reached;
	reached.hasYaw = true;
	reached.yaw = affine::WrapAngle(7.0f);
	EXPECT_EQ(StepDrawnYaw(reached, 7.0f, 16), 7.0f);
	EXPECT_EQ(reached.yaw, 7.0f);
	EXPECT_EQ(reached.objectYaw, 7.0f);
}

// A villager's drawn yaw turns only as its draw turns it: the frame's pass leaves it where its last draw left it, and
// the draw in the object draw list turns it by the frame's step
TEST(DrawnPosition, AVillagersDrawnYawTurnsOnlyAsItIsDrawn)
{
	const openblack::test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const openblack::test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	Locator::terrainSystem::emplace<openblack::test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return 0.0f; });
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Villager>(villager);
	registry.Assign<WallHug>(villager, glm::vec2(0.0f), glm::vec2(0.0f), 1.0f, 0.1f);
	registry.Assign<DrawPosition>(villager);

	UpdateMobileDrawing(0.0f, 16.0f);
	EXPECT_EQ(registry.Get<const DrawPosition>(villager).yaw, 0.0f);
	DrawMobileTurns(villager, 0.0f, 16.0f);
	const auto& draw = registry.Get<const DrawPosition>(villager);
	EXPECT_EQ(draw.yaw, VillagerDrawTurnStep(1.0f, 16));
	EXPECT_EQ(draw.rotation, affine::AngleY(draw.objectYaw + glm::half_pi<float>()));
	const float drawnYaw = draw.yaw;
	UpdateMobileDrawing(0.0f, 16.0f);
	EXPECT_EQ(registry.Get<const DrawPosition>(villager).yaw, drawnYaw);
}

// A bird's bank moves on only as the bird is drawn, as its draw moves it: the frame's pass leaves it, the draw in the
// object draw list advances it by the frame's game time
TEST(DrawnPosition, ABirdsBankMovesOnlyAsItIsDrawn)
{
	const openblack::test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const openblack::test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	Locator::terrainSystem::emplace<openblack::test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return 0.0f; });
	const auto bird = registry.Create();
	registry.Assign<Transform>(bird, glm::vec3(10.0f, 5.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(bird, AnimalInfo::Dove, 10u);
	registry.Assign<AnimalBrain>(bird).bank.SetDestinationWithSpeedAndTime(0.5f, 0.0f, 2.0f);
	registry.Assign<DrawPosition>(bird);

	UpdateMobileDrawing(0.0f, 16.0f);
	EXPECT_EQ(registry.Get<const AnimalBrain>(bird).bank.time, 0.0f);
	DrawMobileTurns(bird, 0.0f, 16.0f);
	openblack::Zoomer expected;
	expected.SetDestinationWithSpeedAndTime(0.5f, 0.0f, 2.0f);
	expected.Update(16.0f * 0.001f);
	EXPECT_EQ(registry.Get<const AnimalBrain>(bird).bank.time, expected.time);
	EXPECT_EQ(registry.Get<const AnimalBrain>(bird).bank.value, expected.value);
}
