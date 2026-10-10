/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"

using namespace openblack;

namespace
{
constexpr float k_FarClip = 1000.0f;

/// A camera at (10, 20, 30) looking along +z: its right is +x and its up +y. A 90 degree horizontal field of view on a
/// screen twice as wide as it is high
void AlongZ(Camera& camera, float nearClip, float farClip = k_FarClip)
{
	camera.SetProjectionMatrixPerspective(90.0f, 2.0f, nearClip, farClip);
	camera.SetOrigin({10.0f, 20.0f, 30.0f});
	camera.SetFocus({10.0f, 20.0f, 40.0f});
}
} // namespace

TEST(CameraRay, StartsAtTheEyeThroughTheFieldOfView)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	// three quarters across and a quarter down: tan 45 x 0.5 to the right, tan 45 / 2 x 0.5 up, one ahead
	const auto ray = camera.RayFromEye({0.75f, 0.25f});
	EXPECT_EQ(ray.origin, camera.GetOrigin());
	const float length = std::sqrt(0.5f * 0.5f + 0.25f * 0.25f + 1.0f);
	EXPECT_NEAR(ray.direction.x, 0.5f / length, 1e-6f);
	EXPECT_NEAR(ray.direction.y, 0.25f / length, 1e-6f);
	EXPECT_NEAR(ray.direction.z, 1.0f / length, 1e-6f);
	EXPECT_NEAR(glm::length(ray.direction), 1.0f, 1e-6f);
}

TEST(CameraRay, TheScreenEdgeIsHalfTheFieldOfViewAndNothingClampsBeyondIt)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	const auto edge = camera.RayFromEye({1.0f, 0.5f}).direction;
	EXPECT_NEAR(edge.x, std::sqrt(0.5f), 1e-6f);
	EXPECT_NEAR(edge.y, 0.0f, 1e-6f);
	EXPECT_NEAR(edge.z, std::sqrt(0.5f), 1e-6f);
	// half a screen further out, twice the tangent
	const auto beyond = camera.RayFromEye({1.5f, 0.5f}).direction;
	EXPECT_NEAR(beyond.x / beyond.z, 2.0f, 1e-5f);
	// the top edge: tan 45 over the aspect
	const auto top = camera.RayFromEye({0.5f, 0.0f}).direction;
	EXPECT_NEAR(top.y / top.z, 0.5f, 1e-6f);
}

TEST(CameraRay, DoesNotDependOnTheClipPlanes)
{
	Camera nearCamera;
	AlongZ(nearCamera, 0.3f);
	Camera farCamera;
	AlongZ(farCamera, 3.5f, 100.0f);
	for (const glm::vec2 screen : {glm::vec2(0.5f, 0.5f), glm::vec2(0.1f, 0.9f), glm::vec2(0.8f, 0.3f)})
	{
		const auto a = nearCamera.RayFromEye(screen);
		const auto b = farCamera.RayFromEye(screen);
		EXPECT_EQ(a.origin, b.origin);
		EXPECT_EQ(a.direction, b.direction);
	}
}

TEST(CameraRay, TheDeprojectedRayIsTheSameLineStartedAboutTwoNearClipsAhead)
{
	// what the game's rays used before: a start on the plane where the depth is half way, 2 n f / (f + n) ahead
	constexpr float k_Near = 3.5f;
	Camera camera;
	AlongZ(camera, k_Near);
	const glm::vec2 screen(0.2f, 0.7f);
	glm::vec3 start;
	glm::vec3 direction;
	camera.DeprojectScreenToWorld(screen, start, direction);
	const auto ray = camera.RayFromEye(screen);
	EXPECT_NEAR(glm::distance(direction, ray.direction), 0.0f, 1e-5f);
	EXPECT_NEAR(start.z - ray.origin.z, 2.0f * k_Near * k_FarClip / (k_FarClip + k_Near), 1e-2f);
	const auto along = glm::dot(start - ray.origin, ray.direction);
	EXPECT_NEAR(glm::distance(ray.origin + ray.direction * along, start), 0.0f, 1e-2f);
}

TEST(CameraRay, ATurnedCameraCastsThroughThePointItDraws)
{
	Camera camera;
	constexpr glm::vec4 k_Viewport(0.0f, 0.0f, 1280.0f, 720.0f);
	camera.SetProjectionMatrixPerspective(70.0f, k_Viewport.z / k_Viewport.w, 1.0f, k_FarClip);
	camera.SetOrigin({1000.0f, 120.0f, 900.0f});
	camera.SetFocus({1040.0f, 30.0f, 960.0f});
	// the centre ray looks at the focus
	const auto centre = camera.RayFromEye({0.5f, 0.5f});
	const auto look = glm::normalize(camera.GetFocus() - camera.GetOrigin());
	EXPECT_NEAR(glm::distance(centre.direction, look), 0.0f, 1e-6f);
	// a point along another ray is drawn back under that screen point
	const glm::vec2 screen(0.2f, 0.7f);
	const auto ray = camera.RayFromEye(screen);
	glm::vec3 drawn;
	ASSERT_TRUE(camera.ProjectWorldToScreen(ray.origin + ray.direction * 100.0f, k_Viewport, drawn));
	EXPECT_NEAR(drawn.x, screen.x * k_Viewport.z, 0.01f);
	EXPECT_NEAR(drawn.y, screen.y * k_Viewport.w, 0.01f);
}
