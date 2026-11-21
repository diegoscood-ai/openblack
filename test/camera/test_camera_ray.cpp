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
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"

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

TEST(CameraRay, TheEyeIsTheRaysStartWithAndWithoutADrawnView)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	EXPECT_EQ(camera.GetEye(), camera.GetOrigin());
	EXPECT_EQ(camera.GetEye(), camera.RayFromEye({0.3f, 0.6f}).origin);
	// a drawn view of its own: the eye is that view's, the same floats as the ray's start
	camera.SetDrawnView(
	    glm::lookAt(glm::vec3(-40.0f, 75.0f, 12.0f), glm::vec3(5.0f, 0.0f, 60.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto eye = camera.GetEye();
	EXPECT_EQ(eye, camera.RayFromEye({0.3f, 0.6f}).origin);
	EXPECT_NEAR(glm::distance(eye, glm::vec3(-40.0f, 75.0f, 12.0f)), 0.0f, 1e-3f);
	camera.SetDrawnView(std::nullopt);
	EXPECT_EQ(camera.GetEye(), camera.GetOrigin());
}

// The drawn camera's place: the camera's own position and focus, or those a drawn view of its own was drawn from,
// kept after the view is cleared until it is forgotten; the camera's own position is not touched
TEST(CameraRay, TheDrawnPlaceIsTheDrawnViewsUntilItIsForgotten)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	EXPECT_EQ(camera.GetDrawnOrigin(), camera.GetOrigin());
	EXPECT_EQ(camera.GetDrawnFocus(), camera.GetFocus());
	const auto origin = camera.GetOrigin();
	const glm::vec3 pathOrigin {-40.0f, 75.0f, 12.0f};
	const glm::vec3 pathFocus {5.0f, 0.0f, 60.0f};
	camera.SetDrawnView(glm::lookAt(pathOrigin, pathFocus, glm::vec3(0.0f, 1.0f, 0.0f)));
	camera.SetDrawnPlace({.origin = pathOrigin, .focus = pathFocus});
	EXPECT_EQ(camera.GetDrawnOrigin(), pathOrigin);
	EXPECT_EQ(camera.GetDrawnFocus(), pathFocus);
	EXPECT_EQ(camera.GetOrigin(), origin);
	// the view cleared, the place stays until the game camera is drawn again
	camera.SetDrawnView(std::nullopt);
	EXPECT_EQ(camera.GetDrawnOrigin(), pathOrigin);
	// the draw's copy keeps it too
	Camera copy;
	camera.CopyViewTo(copy);
	EXPECT_EQ(copy.GetDrawnOrigin(), pathOrigin);
	EXPECT_EQ(copy.GetDrawnFocus(), pathFocus);
	camera.ForgetDrawnPlace();
	EXPECT_EQ(camera.GetDrawnOrigin(), camera.GetOrigin());
	EXPECT_EQ(camera.GetDrawnFocus(), camera.GetFocus());
}

// The weather's point is the camera's position but while one is set
TEST(CameraRay, TheWeatherPointIsTheCamerasPositionButWhileOneIsSet)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	EXPECT_EQ(camera.GetWeatherPoint(), camera.GetOrigin());
	camera.SetWeatherPoint(glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(camera.GetWeatherPoint(), glm::vec3(1.0f, 2.0f, 3.0f));
	camera.SetWeatherPoint(std::nullopt);
	EXPECT_EQ(camera.GetWeatherPoint(), camera.GetOrigin());
}

TEST(CameraRay, ThePixelNearClipStartsAsTheGamesAndKeepsTheLastOneSet)
{
	Camera camera;
	EXPECT_EQ(camera.GetPixelNearClip(), 3.5f);
	camera.SetPixelNearClip(0.1f);
	EXPECT_EQ(camera.GetPixelNearClip(), 0.1f);
	// the draw's copy of the camera takes it too
	Camera copy;
	camera.CopyViewTo(copy);
	EXPECT_EQ(copy.GetPixelNearClip(), 0.1f);
}

TEST(CameraRay, TheKeptViewStaysAsTheCameraIsPutUntilItIsKeptAgainOrForgotten)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	// none kept: the view now
	EXPECT_EQ(camera.GetKeptView().eye, camera.GetOrigin());
	camera.KeepDrawnView();
	const auto kept = camera.GetRayView();
	// a command puts the camera elsewhere: the kept view is the one before
	camera.SetOrigin({-50.0f, 80.0f, 5.0f});
	camera.SetFocus({0.0f, 0.0f, 60.0f});
	EXPECT_EQ(camera.GetKeptView().eye, kept.eye);
	EXPECT_EQ(camera.GetKeptView().worldToCamera.m, kept.worldToCamera.m);
	EXPECT_NE(camera.GetRayView().eye, kept.eye);
	// the draw's copy keeps it too
	Camera copy;
	camera.CopyViewTo(copy);
	EXPECT_EQ(copy.GetKeptView().eye, kept.eye);
	camera.ForgetKeptView();
	EXPECT_EQ(camera.GetKeptView().eye, camera.GetOrigin());
	camera.KeepDrawnView();
	EXPECT_EQ(camera.GetKeptView().eye, camera.GetOrigin());
}

TEST(CameraRay, APixelOnTheNearPlaneOfTheViewNowIsTheCamerasOwnToTheBit)
{
	Camera camera;
	AlongZ(camera, 1.0f);
	camera.SetOrigin({-50.0f, 80.0f, 5.0f});
	camera.SetFocus({3.0f, 0.5f, 60.0f});
	constexpr glm::ivec2 k_Screen {800, 400};
	for (const auto pixel : {glm::ivec2 {0, 0}, glm::ivec2 {399, 201}, glm::ivec2 {799, 399}})
	{
		EXPECT_EQ(screen_point::PixelNearPoint(camera.GetRayView(), k_Screen, pixel, 3.5f),
		          camera.PixelNearPoint(camera.GetEye(), k_Screen, pixel, 3.5f));
	}
}
