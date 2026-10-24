/*******************************************************************************
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

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ScreenPick.h"
#include "Creature/CaveCopyPick.h"

using namespace openblack;

namespace
{
/// A camera at the origin looking down -z, 800 by 600, the cursor in the middle unless said otherwise
screen_pick::View View(glm::vec2 cursor = {400.0f, 300.0f})
{
	constexpr float k_Near = 1.0f;
	const auto projection = glm::perspectiveRH_NO(glm::radians(60.0f), 800.0f / 600.0f, k_Near, 1000.0f);
	const auto view = glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return {.worldToClip = projection * view,
	        .resolution = {800.0f, 600.0f},
	        .near = k_Near,
	        .xScale = projection[0][0],
	        .camera = glm::vec3(0.0f),
	        .cursor = cursor};
}

/// One drawn part of a single triangle, drawn from both sides
cave_copy_pick::Part Triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
	cave_copy_pick::Part part;
	part.corners = {a, b, c};
	part.indices = {0, 1, 2};
	part.primitives = {{.first = 0, .count = 3, .twoSided = true}};
	return part;
}

const cave_copy_pick::Sphere k_AroundTheMiddle {.centre = {0.0f, 0.0f, -10.0f}, .radius = 2.0f, .origin = {0.0f, 0.0f, -10.0f}};
} // namespace

TEST(CaveCopyPick, TheCursorOverATriangleOfTheBodyFindsIt)
{
	const std::vector parts {Triangle({-1.0f, -1.0f, -10.0f}, {1.0f, -1.0f, -10.0f}, {0.0f, 1.0f, -10.0f})};
	EXPECT_TRUE(cave_copy_pick::CursorOver(View(), k_AroundTheMiddle, parts));
}

TEST(CaveCopyPick, OverTheSphereButBesideEveryTriangleItIsNotFound)
{
	const std::vector parts {Triangle({0.5f, -1.0f, -10.0f}, {1.5f, -1.0f, -10.0f}, {1.0f, 1.0f, -10.0f})};
	EXPECT_FALSE(cave_copy_pick::CursorOver(View(), k_AroundTheMiddle, parts));
	EXPECT_FALSE(cave_copy_pick::CursorOver(View(), k_AroundTheMiddle, {}));
}

TEST(CaveCopyPick, OutsideTheSphereTheTrianglesAreNotTested)
{
	// The triangle covers the cursor, but the sphere is off to the side
	const std::vector parts {Triangle({-1.0f, -1.0f, -10.0f}, {1.0f, -1.0f, -10.0f}, {0.0f, 1.0f, -10.0f})};
	const cave_copy_pick::Sphere aside {.centre = {5.0f, 0.0f, -10.0f}, .radius = 1.0f, .origin = {5.0f, 0.0f, -10.0f}};
	EXPECT_FALSE(cave_copy_pick::CursorOver(View(), aside, parts));
}

TEST(CaveCopyPick, APrimitivePastItsIndicesIsPassedBy)
{
	auto part = Triangle({-1.0f, -1.0f, -10.0f}, {1.0f, -1.0f, -10.0f}, {0.0f, 1.0f, -10.0f});
	part.primitives = {{.first = 3, .count = 3, .twoSided = true}};
	const std::vector parts {part};
	EXPECT_FALSE(cave_copy_pick::CursorOver(View(), k_AroundTheMiddle, parts));
}

TEST(CaveCopyPick, CornersArePlacedByTheirBoneAndTheBodysMatrix)
{
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
	const std::array bones {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f))};
	const std::array<uint16_t, 3> skin {1, 0, 7};
	const std::array base {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
	const auto posed = cave_copy_pick::PoseCorners(model, bones, true, skin, base, {}, {});
	ASSERT_EQ(posed.size(), 3u);
	EXPECT_EQ(posed[0], glm::vec3(11.0f, 5.0f, 0.0f));
	EXPECT_EQ(posed[1], glm::vec3(10.0f, 1.0f, 0.0f));
	// A bone past the pose's takes the first
	EXPECT_EQ(posed[2], glm::vec3(10.0f, 0.0f, 1.0f));

	// A sub-mesh that isn't skinned, or a body without bones, takes the body's matrix alone
	const auto rigid = cave_copy_pick::PoseCorners(model, bones, false, skin, base, {}, {});
	EXPECT_EQ(rigid[0], glm::vec3(11.0f, 0.0f, 0.0f));
	const auto boneless = cave_copy_pick::PoseCorners(model, {}, true, skin, base, {}, {});
	EXPECT_EQ(boneless[0], glm::vec3(11.0f, 0.0f, 0.0f));
}

TEST(CaveCopyPick, CornersArePulledTowardsTheShapesMeshes)
{
	const std::array base {glm::vec3(0.0f)};
	const std::array thin {glm::vec3(2.0f, 0.0f, 0.0f)};
	// A shape mesh whose corners don't match the base's in number stands in as the base
	const std::array mismatched {glm::vec3(0.0f, 8.0f, 0.0f), glm::vec3(0.0f, 8.0f, 0.0f)};
	const std::array<std::span<const glm::vec3>, 3> shapes {mismatched, thin, {}};
	const auto posed = cave_copy_pick::PoseCorners(glm::mat4(1.0f), {}, false, {}, base, shapes,
	                                               {.evilGood = 1.0f, .thinFat = -0.5f, .weakStrong = 1.0f});
	ASSERT_EQ(posed.size(), 1u);
	EXPECT_EQ(posed[0], glm::vec3(1.0f, 0.0f, 0.0f));
}
