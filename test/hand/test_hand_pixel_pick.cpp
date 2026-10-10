/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_pixel_pick, the pixel test the hand's cursor search gives a tree (issue #130): the projection, the pixel in a
// triangle, the perspective-correct u, v at the pixel, the 64 x 64 mask's lookup and the whole test of one triangle,
// over fake triangles and masks; and the depth it gives as a distance along the pick's ray. The rules are in
// docs/bw1-notes/hand-and-interface.md, "A tree under the cursor".

#include <cmath>
#include <cstddef>

#include <array>
#include <limits>
#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ECS/HandPixelPick.h"

using namespace openblack::ecs::hand_pixel_pick;

namespace
{
constexpr Screen k_Screen {.halfSize = glm::vec2(320.0f, 240.0f), .last = glm::vec2(639.0f, 479.0f), .nearClip = 1.0f};

ScreenVertex At(float x, float y, float rhw = 1.0f, float u = 0.0f, float v = 0.0f)
{
	return {.x = x, .y = y, .rhw = rhw, .u = u, .v = v};
}

/// A mask with only the cell (column, row) not 0
Mask OneCell(size_t column, size_t row)
{
	Mask mask {};
	mask[row * 64 + column] = 0xF0;
	return mask;
}

/// A view-space point (x, y, depth) with a focal length of 1: its clipping-space point
ClipVertex View(glm::vec3 point, glm::vec2 uv)
{
	return {.clip = glm::vec4(point.x, point.y, 0.0f, point.z), .uv = uv};
}

/// The triangle a (160, 120), b (480, 120), c (160, 360) on the screen at depth 10, facing the camera, with u along x
/// and v along y
std::array<ClipVertex, 3> FacingTriangle()
{
	return {ClipVertex {.clip = glm::vec4(-5.0f, 5.0f, 0.0f, 10.0f), .uv = glm::vec2(0.0f, 0.0f)},
	        ClipVertex {.clip = glm::vec4(5.0f, 5.0f, 0.0f, 10.0f), .uv = glm::vec2(1.0f, 0.0f)},
	        ClipVertex {.clip = glm::vec4(-5.0f, -5.0f, 0.0f, 10.0f), .uv = glm::vec2(0.0f, 1.0f)}};
}
} // namespace

TEST(HandPixelPick, OutcodeOfEachSide)
{
	EXPECT_EQ(Outcode(glm::vec4(0.0f, 0.0f, 0.0f, 2.0f), 1.0f), 0u);
	EXPECT_EQ(Outcode(glm::vec4(0.0f, 0.0f, 0.0f, 0.5f), 1.0f), k_OutsideNear);
	EXPECT_EQ(Outcode(glm::vec4(3.0f, 0.0f, 0.0f, 2.0f), 1.0f), k_OutsideRight);
	EXPECT_EQ(Outcode(glm::vec4(-3.0f, 0.0f, 0.0f, 2.0f), 1.0f), k_OutsideLeft);
	EXPECT_EQ(Outcode(glm::vec4(0.0f, 3.0f, 0.0f, 2.0f), 1.0f), k_OutsideTop);
	EXPECT_EQ(Outcode(glm::vec4(0.0f, -3.0f, 0.0f, 2.0f), 1.0f), k_OutsideBottom);
	// on a side is inside
	EXPECT_EQ(Outcode(glm::vec4(2.0f, -2.0f, 0.0f, 2.0f), 2.0f), 0u);
}

TEST(HandPixelPick, ProjectionYDownAndClampedInsideTheView)
{
	const auto centre = Project({.clip = glm::vec4(0.0f, 0.0f, 0.0f, 4.0f), .uv = glm::vec2(0.25f, 0.75f)}, k_Screen, true);
	EXPECT_FLOAT_EQ(centre.x, 320.0f);
	EXPECT_FLOAT_EQ(centre.y, 240.0f);
	EXPECT_FLOAT_EQ(centre.rhw, 0.25f); // near clip / w
	EXPECT_FLOAT_EQ(centre.u, 0.25f);
	EXPECT_FLOAT_EQ(centre.v, 0.75f);
	// y = w is the top row
	EXPECT_FLOAT_EQ(Project({.clip = glm::vec4(0.0f, 4.0f, 0.0f, 4.0f)}, k_Screen, true).y, 0.0f);
	// x = w is 640, one past the last pixel: kept on it when the corner is inside the view, not when it is off a side
	EXPECT_FLOAT_EQ(Project({.clip = glm::vec4(4.0f, 0.0f, 0.0f, 4.0f)}, k_Screen, true).x, 639.0f);
	EXPECT_FLOAT_EQ(Project({.clip = glm::vec4(8.0f, 0.0f, 0.0f, 4.0f)}, k_Screen, false).x, 960.0f);
}

TEST(HandPixelPick, PixelInTriangleEitherWinding)
{
	const auto a = At(0.0f, 0.0f);
	const auto b = At(10.0f, 0.0f);
	const auto c = At(0.0f, 10.0f);
	EXPECT_TRUE(HoldsPixel(a, b, c, glm::vec2(2.0f, 2.0f)));
	EXPECT_TRUE(HoldsPixel(a, c, b, glm::vec2(2.0f, 2.0f)));
	EXPECT_FALSE(HoldsPixel(a, b, c, glm::vec2(8.0f, 8.0f)));
	EXPECT_FALSE(HoldsPixel(a, c, b, glm::vec2(8.0f, 8.0f)));
	// the edge opposite the first corner counts, and so does the third corner
	EXPECT_TRUE(HoldsPixel(a, b, c, glm::vec2(5.0f, 5.0f)));
	EXPECT_TRUE(HoldsPixel(a, b, c, glm::vec2(0.0f, 10.0f)));
}

TEST(HandPixelPick, FirstEdgeCountsInOneWindingOnly)
{
	// on the edge first to second corner the first cross product is 0, which decides "not above 0": wound so that it
	// would be above 0, the triangle loses its pixels on that edge, and its first two corners
	const auto a = At(0.0f, 0.0f);
	const auto b = At(10.0f, 0.0f);
	const auto c = At(0.0f, 10.0f);
	EXPECT_FALSE(HoldsPixel(a, b, c, glm::vec2(5.0f, 0.0f)));
	EXPECT_TRUE(HoldsPixel(a, c, b, glm::vec2(5.0f, 0.0f)));
	EXPECT_FALSE(HoldsPixel(a, b, c, glm::vec2(0.0f, 0.0f)));
	EXPECT_FALSE(HoldsPixel(a, b, c, glm::vec2(10.0f, 0.0f)));
}

TEST(HandPixelPick, FacingIsClockwiseOnTheScreen)
{
	const auto a = At(0.0f, 0.0f);
	const auto b = At(10.0f, 0.0f);
	const auto c = At(0.0f, 10.0f);
	EXPECT_TRUE(FacesCamera(a, b, c));
	EXPECT_FALSE(FacesCamera(a, c, b));
	EXPECT_FALSE(FacesCamera(a, b, At(20.0f, 0.0f))); // no area
}

TEST(HandPixelPick, InterpolationIsLinearAtOneDepth)
{
	const auto at = Interpolate(At(0.0f, 0.0f, 1.0f, 0.0f, 0.0f), At(10.0f, 0.0f, 1.0f, 1.0f, 0.0f),
	                            At(0.0f, 10.0f, 1.0f, 0.0f, 1.0f), glm::vec2(2.0f, 3.0f));
	EXPECT_NEAR(at.u, 0.2f, 1e-6f);
	EXPECT_NEAR(at.v, 0.3f, 1e-6f);
	EXPECT_NEAR(at.rhw, 1.0f, 1e-6f);
}

TEST(HandPixelPick, InterpolationIsPerspectiveCorrect)
{
	// the second corner four times as far: at the pixel (4, 2) the screen weights are 0.4, 0.4, 0.2, so
	// rhw = 0.4 + 0.4 x 0.25 + 0.2 = 0.7 and u = 0.4 x 0.25 / 0.7 = 1/7, not the 0.4 of a flat interpolation
	const auto a = At(0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	const auto b = At(10.0f, 0.0f, 0.25f, 1.0f, 0.0f);
	const auto c = At(0.0f, 10.0f, 1.0f, 0.0f, 1.0f);
	const auto at = Interpolate(a, b, c, glm::vec2(4.0f, 2.0f));
	EXPECT_NEAR(at.rhw, 0.7f, 1e-6f);
	EXPECT_NEAR(at.u, 1.0f / 7.0f, 1e-6f);
	EXPECT_NEAR(at.v, 0.2f / 0.7f, 1e-6f);
	// the corners' order does not change it
	const auto turned = Interpolate(c, a, b, glm::vec2(4.0f, 2.0f));
	EXPECT_NEAR(turned.u, at.u, 1e-6f);
	EXPECT_NEAR(turned.v, at.v, 1e-6f);
}

TEST(HandPixelPick, MaskCellTruncatesAndClamps)
{
	EXPECT_EQ(MaskCell(0.5f), 32);
	EXPECT_EQ(MaskCell(0.0156f), 0); // 0.998 truncated
	EXPECT_EQ(MaskCell(0.999f), 63);
	EXPECT_EQ(MaskCell(1.0f), 63); // 64 clamped
	EXPECT_EQ(MaskCell(3.5f), 63); // no wrap
	EXPECT_EQ(MaskCell(-0.01f), 0);
	EXPECT_EQ(MaskCell(-2.0f), 0);
	EXPECT_EQ(MaskCell(std::numeric_limits<float>::quiet_NaN()), 0);
	EXPECT_EQ(MaskCell(std::numeric_limits<float>::infinity()), 0);
}

TEST(HandPixelPick, MaskLookupIsVRowsUColumns)
{
	const auto mask = OneCell(20, 10);
	EXPECT_TRUE(MaskOpaque(mask, 20.5f / 64.0f, 10.5f / 64.0f));
	EXPECT_FALSE(MaskOpaque(mask, 10.5f / 64.0f, 20.5f / 64.0f));
	EXPECT_FALSE(MaskOpaque(mask, 21.5f / 64.0f, 10.5f / 64.0f));
}

TEST(HandPixelPick, NearClipCutsOneOrTwoCornersOff)
{
	const std::array<ClipVertex, 3> oneBehind = {View(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec2(0.0f)),
	                                             View(glm::vec3(1.0f, 0.0f, 5.0f), glm::vec2(1.0f, 0.0f)),
	                                             View(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f))};
	const auto quad = ClipToNear(oneBehind, 1.0f);
	ASSERT_EQ(quad.count, 4u);
	EXPECT_NEAR(quad.vertices[2].clip.w, 1.0f, 1e-6f);
	EXPECT_NEAR(quad.vertices[3].clip.w, 1.0f, 1e-6f);
	// the cut on the edge from the third corner back to the first: 1/5 of the way from the third
	EXPECT_NEAR(quad.vertices[3].uv.y, 0.8f, 1e-6f);
	const std::array<ClipVertex, 3> twoBehind = {View(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec2(0.0f)),
	                                             View(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
	                                             View(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f))};
	EXPECT_EQ(ClipToNear(twoBehind, 1.0f).count, 3u);
}

TEST(HandPixelPick, TriangleHitWithoutMaskGivesTheDepth)
{
	const auto hit = TriangleHit(FacingTriangle(), false, nullptr, k_Screen, glm::vec2(242.5f, 181.875f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(*hit, 10.0f, 1e-4f);
	EXPECT_FALSE(TriangleHit(FacingTriangle(), false, nullptr, k_Screen, glm::vec2(400.0f, 300.0f)).has_value());
}

TEST(HandPixelPick, TriangleHitOnlyWhereTheMaskIsNotZero)
{
	// the point (242.5, 181.875) is at u = v = 16.5 / 64: the middle of the cell (16, 16)
	const auto there = OneCell(16, 16);
	const auto elsewhere = OneCell(17, 16);
	const Mask clear {};
	EXPECT_TRUE(TriangleHit(FacingTriangle(), false, &there, k_Screen, glm::vec2(242.5f, 181.875f)).has_value());
	EXPECT_FALSE(TriangleHit(FacingTriangle(), false, &elsewhere, k_Screen, glm::vec2(242.5f, 181.875f)).has_value());
	EXPECT_FALSE(TriangleHit(FacingTriangle(), false, &clear, k_Screen, glm::vec2(242.5f, 181.875f)).has_value());
}

TEST(HandPixelPick, BackFacesOnlyWhenTwoSided)
{
	auto back = FacingTriangle();
	std::swap(back[1], back[2]);
	EXPECT_FALSE(TriangleHit(back, false, nullptr, k_Screen, glm::vec2(242.5f, 181.875f)).has_value());
	EXPECT_TRUE(TriangleHit(back, true, nullptr, k_Screen, glm::vec2(242.5f, 181.875f)).has_value());
}

TEST(HandPixelPick, AllCornersOffOneSideIsNoHit)
{
	// all three right of the view, even with the pixel where they would land
	const std::array<ClipVertex, 3> right = {ClipVertex {.clip = glm::vec4(11.0f, 5.0f, 0.0f, 10.0f)},
	                                         ClipVertex {.clip = glm::vec4(20.0f, 5.0f, 0.0f, 10.0f)},
	                                         ClipVertex {.clip = glm::vec4(11.0f, -5.0f, 0.0f, 10.0f)}};
	EXPECT_FALSE(TriangleHit(right, true, nullptr, k_Screen, glm::vec2(680.0f, 200.0f)).has_value());
}

TEST(HandPixelPick, TriangleAcrossTheNearClip)
{
	// the third corner behind the near clip (depth 0.5 of 1); the point 0.6 a + 0.2 b + 0.2 c is in front, at depth 4.1
	// with u = v = 0.2: the cell (12, 12)
	const std::array<ClipVertex, 3> triangle = {View(glm::vec3(-1.0f, 1.0f, 5.0f), glm::vec2(0.0f, 0.0f)),
	                                            View(glm::vec3(1.0f, 1.0f, 5.0f), glm::vec2(1.0f, 0.0f)),
	                                            View(glm::vec3(-1.0f, -1.0f, 0.5f), glm::vec2(0.0f, 1.0f))};
	const glm::vec3 point(-0.6f, 0.6f, 4.1f);
	const auto onScreen = Project(View(point, glm::vec2(0.0f)), k_Screen, false);
	const glm::vec2 pixel(onScreen.x, onScreen.y);
	const auto there = OneCell(12, 12);
	const auto elsewhere = OneCell(13, 12);
	const auto hit = TriangleHit(triangle, true, &there, k_Screen, pixel);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(*hit, 4.1f, 1e-3f);
	EXPECT_FALSE(TriangleHit(triangle, true, &elsewhere, k_Screen, pixel).has_value());
}

TEST(HandPixelPick, AlongRayIsTheDepthOverTheRaysDepthPerUnit)
{
	// a ray 0.8 deep per unit (36.87 degrees off the view's axis): depth 40 is 50 along it from the eye
	const auto fromEye = AlongRay(40.0f, 0.0f, 0.8f);
	ASSERT_TRUE(fromEye.has_value());
	EXPECT_FLOAT_EQ(*fromEye, 50.0f);
	// the same point from a start already 2 deep
	const auto fromStart = AlongRay(42.0f, 2.0f, 0.8f);
	ASSERT_TRUE(fromStart.has_value());
	EXPECT_FLOAT_EQ(*fromStart, 50.0f);
}

TEST(HandPixelPick, AlongRayOrdersAsTheDepths)
{
	// two trees on one ray: the one nearer in depth is nearer along the ray, by the same ratio
	const auto nearer = AlongRay(44.66f, 0.0f, 0.6f);
	const auto further = AlongRay(52.31f, 0.0f, 0.6f);
	ASSERT_TRUE(nearer.has_value());
	ASSERT_TRUE(further.has_value());
	EXPECT_LT(*nearer, *further);
	EXPECT_NEAR(*further / *nearer, 52.31f / 44.66f, 1e-5f);
}

TEST(HandPixelPick, AlongRayNothingNotAheadOfTheStart)
{
	EXPECT_FALSE(AlongRay(2.0f, 2.0f, 0.8f).has_value());
	EXPECT_FALSE(AlongRay(1.0f, 2.0f, 0.8f).has_value());
	// a ray that does not go deeper gives no finite distance
	EXPECT_FALSE(AlongRay(10.0f, 0.0f, 0.0f).has_value());
	EXPECT_FALSE(AlongRay(0.0f, 0.0f, 0.0f).has_value());
}

TEST(HandPixelPick, TriangleDepthBackToTheRayIsTheDistanceToItsPoint)
{
	// the point (-1, 1, 10) on the facing triangle is at the pixel (288, 216); the ray from the eye through it is
	// dir.z deep per unit, so its depth comes back as |point|
	const glm::vec3 point(-1.0f, 1.0f, 10.0f);
	const auto depth = TriangleHit(FacingTriangle(), false, nullptr, k_Screen, glm::vec2(288.0f, 216.0f));
	ASSERT_TRUE(depth.has_value());
	const auto along = AlongRay(*depth, 0.0f, glm::normalize(point).z);
	ASSERT_TRUE(along.has_value());
	EXPECT_NEAR(*along, glm::length(point), 1e-4f);
}
