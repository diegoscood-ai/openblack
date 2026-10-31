/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "Camera/ScreenPoint.h"
#include "ECS/DrawList/FrameInputs.h"

namespace affine = openblack::affine;
namespace draw_list = openblack::ecs::draw_list;
namespace screen_point = openblack::screen_point;

namespace
{
constexpr float k_Fov = 1.2f;
constexpr float k_Near = 7.5f;
/// A screen whose half width and half height are powers of two, so the pixel steps divide exactly
constexpr glm::ivec2 k_Screen {512, 256};
/// The identity rotation and no eye: the world point is the camera-space point
const affine::AffineMatrix k_Identity {};
constexpr glm::vec3 k_Origin {0.0f, 0.0f, 0.0f};
} // namespace

TEST(ScreenPoint, TheNearPlaneIsHalfTheScreenAndTheNearHalfWidth)
{
	const auto plane = screen_point::NearPlaneOf({640, 480}, k_Fov, k_Near);
	EXPECT_EQ(plane.halfWidth, 320.0f);
	EXPECT_EQ(plane.halfHeight, 240.0f);
	EXPECT_EQ(plane.nearClip, k_Near);
	EXPECT_EQ(plane.nearHalfWidth, draw_list::NearHalfWidth(k_Fov, k_Near));
	// the height's half is the width's over the width-to-height ratio, the ratio rounded first
	const float ratio = 640.0f / 480.0f;
	EXPECT_EQ(plane.nearHalfHeight, plane.nearHalfWidth / ratio);
}

TEST(ScreenPoint, TheMiddlePixelIsTheNearClipAlongTheView)
{
	const glm::vec3 eye(1000.0f, 120.0f, 1500.0f);
	const auto w2c = affine::WorldToCamera(eye, {1100.0f, 40.0f, 1600.0f});
	const auto plane = screen_point::NearPlaneOf({640, 480}, k_Fov, k_Near);
	const auto point = screen_point::PixelPoint(w2c, eye, plane, {320, 240}, 0.0f);
	// the view direction is the matrix's third column
	const auto& m = w2c.m;
	EXPECT_EQ(point.x, m[2] * k_Near + eye.x);
	EXPECT_EQ(point.y, m[5] * k_Near + eye.y);
	EXPECT_EQ(point.z, m[8] * k_Near + eye.z);
}

TEST(ScreenPoint, TheCornersAreTheNearPlanesHalfSizes)
{
	const auto plane = screen_point::NearPlaneOf(k_Screen, k_Fov, k_Near);
	const float tn = plane.nearHalfWidth;
	const float tm = plane.nearHalfHeight;
	EXPECT_EQ(tm, tn / 2.0f);

	const auto topLeft = screen_point::PixelPoint(k_Identity, k_Origin, plane, {0, 0}, 0.0f);
	EXPECT_EQ(topLeft, glm::vec3(-tn, tm, k_Near));
	const auto bottomRight = screen_point::PixelPoint(k_Identity, k_Origin, plane, k_Screen, 0.0f);
	EXPECT_EQ(bottomRight, glm::vec3(tn, -tm, k_Near));
}

TEST(ScreenPoint, NeighbouringPixelsAtTheMiddleDiffer)
{
	const auto plane = screen_point::NearPlaneOf(k_Screen, k_Fov, k_Near);
	const auto middle = screen_point::PixelPoint(k_Identity, k_Origin, plane, {256, 128}, 0.0f);
	const auto before = screen_point::PixelPoint(k_Identity, k_Origin, plane, {255, 127}, 0.0f);
	EXPECT_EQ(middle, glm::vec3(0.0f, 0.0f, k_Near));
	EXPECT_EQ(before.x, -plane.nearHalfWidth / 256.0f);
	EXPECT_EQ(before.y, plane.nearHalfHeight / 128.0f);
	EXPECT_NE(before.x, middle.x);
	EXPECT_NE(before.y, middle.y);
}

TEST(ScreenPoint, ADepthScalesThePointByDepthOverTheNearClip)
{
	const auto plane = screen_point::NearPlaneOf(k_Screen, k_Fov, k_Near);
	const float tn = plane.nearHalfWidth;
	const float tm = plane.nearHalfHeight;
	// four times the near clip: an exact scale
	EXPECT_EQ(screen_point::PixelPoint(k_Identity, k_Origin, plane, {0, 0}, 30.0f), glm::vec3(-tn * 4.0f, tm * 4.0f, 30.0f));
	// a scale that rounds: depth / near first, then each axis times it
	const float scale = 20.0f / k_Near;
	EXPECT_EQ(screen_point::PixelPoint(k_Identity, k_Origin, plane, {0, 0}, 20.0f), glm::vec3(-tn * scale, tm * scale, 20.0f));
	// any depth but 0 takes the branch, a negative one too
	const float behind = -15.0f / k_Near;
	EXPECT_EQ(screen_point::PixelPoint(k_Identity, k_Origin, plane, {0, 0}, -15.0f),
	          glm::vec3(-tn * behind, tm * behind, -15.0f));
	// 0 is the near plane
	EXPECT_EQ(screen_point::PixelPoint(k_Identity, k_Origin, plane, {0, 0}, 0.0f), glm::vec3(-tn, tm, k_Near));
}

TEST(ScreenPoint, TheForwardTermIsSummedFirstAndTheRightTermLast)
{
	// A camera-space point (-1, 0.5, 1): the plane's sizes are picked so the pixel (0, 0) gives it exactly
	const screen_point::NearPlane plane {
	    .halfWidth = 256.0f, .halfHeight = 128.0f, .nearClip = 1.0f, .nearHalfWidth = 1.0f, .nearHalfHeight = 0.5f};
	// Every row: right 1, up 2^-23, forward 1, so the up term is 2^-24, half a step of 1. Forward plus up rounds back
	// to 1 and the right term then cancels it: 0. Summed right first, -1 + 2^-24 is exact and adding 1 leaves 2^-24
	// (checked with float32 arithmetic)
	constexpr float k_Tiny = 1.0f / 8388608.0f;
	const affine::AffineMatrix m {{1.0f, k_Tiny, 1.0f, 1.0f, k_Tiny, 1.0f, 1.0f, k_Tiny, 1.0f, 0.0f, 0.0f, 0.0f}};
	const auto point = screen_point::PixelPoint(m, k_Origin, plane, {0, 0}, 0.0f);
	EXPECT_EQ(point, glm::vec3(0.0f));

	const float x = -1.0f;
	const float y = 0.5f;
	const float z = 1.0f;
	const float rightFirst = (1.0f * x + k_Tiny * y) + 1.0f * z;
	EXPECT_EQ(rightFirst, k_Tiny * 0.5f);
	EXPECT_NE(rightFirst, point.x);
}

TEST(ScreenPoint, AWorldPointAtADepthIsDrawnBackOnItsPoint)
{
	// a camera looking down and across, through a perspective and through the same with its depth reversed
	const glm::vec3 eye(1000.0f, 120.0f, 1500.0f);
	const glm::vec3 focus(1100.0f, 40.0f, 1600.0f);
	const glm::vec3 forward = glm::normalize(focus - eye);
	const glm::mat4 view = glm::lookAt(eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 perspective = glm::perspective(0.9f, 4.0f / 3.0f, 1.5f, 10000.0f);
	const glm::mat4 reversed =
	    glm::mat4(1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, -0.5f, 0.f, 0.f, 0.f, 0.5f, 1.f) * perspective;
	for (const auto& projection : {perspective, reversed})
	{
		const auto toWorld = screen_point::ClipToWorldOf(view, projection);
		for (const auto& [clip, depth] : {std::pair {glm::vec2(0.25f, -0.5f), 37.5f}, std::pair {glm::vec2(-1.0f, 1.0f), 3.0f},
		                                  std::pair {glm::vec2(0.0f, 0.0f), 2500.0f}})
		{
			const glm::vec3 world = screen_point::WorldAtDepth(toWorld, clip, depth);
			// in front of the eye at its depth along the view, whatever the view's own axes, and drawn there: in front
			// of the camera (a positive clip w, inside the depth range, so not clipped away) and on the same point of
			// the screen, as near as a float world point some thousand units from the origin can be
			EXPECT_NEAR(glm::dot(world - eye, forward), depth, 1e-3f + depth * 1e-6f);
			const glm::vec4 drawn = projection * (view * glm::vec4(world, 1.0f));
			EXPECT_GT(drawn.w, 0.0f);
			EXPECT_NEAR(drawn.w, depth, 1e-3f + depth * 1e-6f);
			EXPECT_LE(std::abs(drawn.z), drawn.w);
			EXPECT_NEAR(drawn.x / drawn.w, clip.x, 3e-4f);
			EXPECT_NEAR(drawn.y / drawn.w, clip.y, 3e-4f);
		}
	}
}

// A pre-transformed vertex, as the original's Direct3D 7 places its screen texts, keeps its pixels in a clip space whose
// pixel centres are half a pixel in: pixel j's centre is at j there, at j + 0.5 here
TEST(ScreenPoint, APretransformedVertexKeepsItsPixelInAHalfPixelInClipSpace)
{
	const glm::vec2 corner = screen_point::PretransformedToClip(0.0f, 0.0f, 1024.0f, 768.0f);
	EXPECT_FLOAT_EQ(corner.x, -1.0f + 1.0f / 1024.0f);
	EXPECT_FLOAT_EQ(corner.y, 1.0f - 1.0f / 768.0f);
	const glm::vec2 glyph = screen_point::PretransformedToClip(370.0f, 568.0f, 1024.0f, 768.0f);
	EXPECT_FLOAT_EQ(glyph.x, -0.2763671875f);
	EXPECT_FLOAT_EQ(glyph.y, -0.48046875f);
	// back to the window: the pixel whose centre is 571.5 down is row 571's centre
	const float row = (1.0f - screen_point::PretransformedToClip(0.0f, 571.0f, 1024.0f, 768.0f).y) * 768.0f / 2.0f;
	EXPECT_FLOAT_EQ(row, 571.5f);
}

// The mouse ray: the pixel's point on the near plane less the eye, normalised as the game does. The numbers are the
// original's own (the oracle on its mouse ray builder, with the same near plane, axes and eye set as its globals)
TEST(ScreenPoint, TheMouseRayIsTheNearPlanePointLessTheEyeNormalisedAsTheOriginal)
{
	const screen_point::NearPlane plane {
	    .halfWidth = 512.0f, .halfHeight = 384.0f, .nearClip = 7.5f, .nearHalfWidth = 5.25144f, .nearHalfHeight = 3.93858f};
	affine::AffineMatrix axes;
	axes.m = {0.8f, 0.0f, -0.6f, -0.36f, 0.8f, -0.48f, 0.48f, 0.6f, 0.64f, 0.0f, 0.0f, 0.0f};
	const glm::vec3 eye(1000.5f, 120.25f, 1500.75f);
	const auto direction = screen_point::RayDirection(eye, screen_point::PixelPoint(axes, eye, plane, {700, 250}, 0.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(direction.x), 0xBEC08647u);
	EXPECT_EQ(std::bit_cast<uint32_t>(direction.y), 0xBECFF84Cu);
	EXPECT_EQ(std::bit_cast<uint32_t>(direction.z), 0x3F5534AEu);
	// a point on the eye gives no direction, and stays as it is
	EXPECT_EQ(screen_point::RayDirection(eye, eye), glm::vec3(0.0f));
}

// The original's box draw puts each corner half a pixel on: a box from pixel 100 covers the pixels from 101 on, as a
// pre-transformed vertex at 100.5 does on Direct3D 7, which is one at 101 here
TEST(ScreenPoint, TheBoxDrawsCornersAreHalfAPixelOn)
{
	EXPECT_EQ(screen_point::BoxDrawCorner(100.0f), 100.5f);
	EXPECT_EQ(screen_point::BoxDrawCorner(0.0f), 0.5f);
	EXPECT_EQ(screen_point::BoxDrawCorner(-3.0f), -2.5f);
	// the box's left edge in this clip space: a whole pixel on from the corner it was given
	const float left =
	    (screen_point::PretransformedToClip(screen_point::BoxDrawCorner(100.0f), 0.0f, 1024.0f, 768.0f).x + 1.0f) * 1024.0f /
	    2.0f;
	EXPECT_FLOAT_EQ(left, 101.0f);
}
