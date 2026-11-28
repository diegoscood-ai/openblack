/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics' world in the game as far as its camera goes: the camera the bodies pass by is the drawn camera. The
// locator is only used to inject the camera

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "ECS/Systems/Implementations/GameDynamicsWorld.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

// The fly-by whoosh is measured from the drawn camera: while the falling spell's path camera is drawn its place, else
// the game camera's position; none without a camera
TEST(GameDynamicsWorld, TheBodiesPassTheDrawnCamera)
{
	const test::RestoreService<Locator::camera> restoreCamera;
	const ecs::systems::GameDynamicsWorld world {};
	Locator::camera::reset();
	EXPECT_EQ(world.CameraOrigin(), std::nullopt);
	auto& camera = Locator::camera::emplace(glm::vec3(1000.0f, 0.0f, 1000.0f));
	camera.SetOrigin({1000.0f, 120.0f, 900.0f});
	EXPECT_EQ(world.CameraOrigin(), std::optional(camera.GetOrigin()));
	const Camera::Place drawn {.origin = glm::vec3(900.0f, 300.0f, 800.0f), .focus = glm::vec3(1000.0f, 0.0f, 1000.0f)};
	camera.SetDrawnPlace(drawn);
	EXPECT_EQ(world.CameraOrigin(), std::optional(drawn.origin));
	camera.ForgetDrawnPlace();
	EXPECT_EQ(world.CameraOrigin(), std::optional(camera.GetOrigin()));
}
