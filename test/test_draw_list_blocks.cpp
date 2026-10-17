/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object draw list's block stage (src/ECS/DrawList/BlockCull.h) against the original's rules: the box cull's
// outcodes (strict compares, "left only if not right", a NaN depth counted as near, no far side), the box centre's
// distance with its sum order pinned as bits, a culled block keeping its old state, the nearest-first insertion with
// ties and NaN, and the near clip's bands. Then the level of detail: the two lines' values at a land's creation,
// their rebuild from the camera (bits pinned, the axis tie and the straight-down camera), the corner sides with ties on
// the band's ends, every step and blending the corners give, and the seam bits. Synthetic values only; the one check on
// the original's data (Land1.lnd) skips without it.

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <bit>
#include <filesystem>
#include <limits>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "ECS/DrawList/BlockCull.h"

using namespace openblack;
namespace draw_list = openblack::ecs::draw_list;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();

/// A matrix that leaves a point as it is: X = x, Y = y and the depth is z
affine::AffineMatrix Identity()
{
	return {};
}

/// The identity plus a translation
affine::AffineMatrix Translated(glm::vec3 t)
{
	affine::AffineMatrix m;
	m.m[9] = t.x;
	m.m[10] = t.y;
	m.m[11] = t.z;
	return m;
}

/// 8 corners, the first `count` at `a` and the rest at `b`
std::array<glm::vec3, 8> Corners(glm::vec3 a, size_t count, glm::vec3 b)
{
	std::array<glm::vec3, 8> corners {};
	for (size_t i = 0; i < corners.size(); ++i)
	{
		corners.at(i) = i < count ? a : b;
	}
	return corners;
}

std::array<glm::vec3, 8> AllAt(glm::vec3 a)
{
	return Corners(a, 8, a);
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// A line across z: along (1, 0, 0) through (0, 0, z), so v = z - z' for a corner at z', whatever its x
draw_list::LodLine LineAtZ(float z, float halfWidth = 25.0f)
{
	return {.along = {1.0f, 0.0f, 0.0f}, .point = {0.0f, 0.0f, z}, .width = 2.0f * halfWidth, .halfWidth = halfWidth};
}

/// The inner line at z = 1000 and the outer one at z = 2000, each band 25 either side
draw_list::LodLines LinesAtZ(float inner = 1000.0f, float outer = 2000.0f)
{
	return {.inner = LineAtZ(inner), .outer = LineAtZ(outer)};
}

/// The step and blending the corners of the block at (0, z) give
std::pair<uint32_t, uint32_t> LodAtZ(float z, const draw_list::LodLines& lines = LinesAtZ())
{
	const auto lod = draw_list::ChooseBlockLod(glm::vec2(0.0f, z), lines);
	EXPECT_EQ(lod.meshLodType, 0u);
	return {lod.meshLod, lod.meshBlending};
}

std::filesystem::path Land1Path()
{
	if (const char* env = std::getenv("OPENBLACK_LAND1_LND"))
	{
		return env;
	}
	// the game folder every data-backed test reads; without it the test that needs Land1 skips
	if (const char* game = std::getenv("OPENBLACK_GAME_PATH"); game != nullptr && *game != '\0')
	{
		return std::filesystem::path(game) / "Data" / "Landscape" / "Land1.lnd";
	}
	return {};
}
} // namespace

TEST(DrawListBlocks, CornerOnAPlaneIsInside)
{
	// d at the near clip is not near; X == d is not right and X == -d is not left; the same for Y
	auto cull = draw_list::CullBlock(AllAt(glm::vec3(1.0f, 1.0f, 1.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_FALSE(cull.partlyOutside);
	cull = draw_list::CullBlock(AllAt(glm::vec3(-1.0f, -1.0f, 1.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_FALSE(cull.partlyOutside);
	// just past each plane, every corner: culled
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, 0.0f, 0.999f)), Identity(), 1.0f).visible);
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(1.001f, 0.0f, 1.0f)), Identity(), 1.0f).visible);
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(-1.001f, 0.0f, 1.0f)), Identity(), 1.0f).visible);
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, 1.001f, 1.0f)), Identity(), 1.0f).visible);
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, -1.001f, 1.0f)), Identity(), 1.0f).visible);
}

TEST(DrawListBlocks, LeftOnlyWhenNotRight)
{
	// a corner behind the camera (d = -2) at X = 0 is both "X > d" and "-d > X"; only right is set. With the other 4
	// corners left of the view, setting left for it too would fill the left mask and cull the block
	const glm::vec3 behind(0.0f, 0.0f, -2.0f);
	auto cull = draw_list::CullBlock(Corners(behind, 4, glm::vec3(-10.0f, 0.0f, 5.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_TRUE(cull.partlyOutside);
	// the same for Y: top, and only otherwise bottom
	cull = draw_list::CullBlock(Corners(behind, 4, glm::vec3(0.0f, -10.0f, 5.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_TRUE(cull.partlyOutside);
}

TEST(DrawListBlocks, OneSideForAllCornersCulls)
{
	EXPECT_FALSE(
	    draw_list::CullBlock(Corners(glm::vec3(10.0f, 0.0f, 5.0f), 4, glm::vec3(20.0f, 3.0f, 6.0f)), Identity(), 1.0f).visible);
	// every corner outside some side, but no side with all of them: visible
	const auto cull =
	    draw_list::CullBlock(Corners(glm::vec3(10.0f, 0.0f, 5.0f), 4, glm::vec3(-10.0f, 0.0f, 5.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_TRUE(cull.partlyOutside);
}

TEST(DrawListBlocks, PartlyOutside)
{
	const glm::vec3 inside(0.0f, 0.0f, 5.0f);
	auto cull = draw_list::CullBlock(AllAt(inside), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_FALSE(cull.partlyOutside);
	// one corner out of each side in turn
	for (const auto out : {glm::vec3(0.0f, 0.0f, 0.5f), glm::vec3(6.0f, 0.0f, 5.0f), glm::vec3(-6.0f, 0.0f, 5.0f),
	                       glm::vec3(0.0f, 6.0f, 5.0f), glm::vec3(0.0f, -6.0f, 5.0f)})
	{
		cull = draw_list::CullBlock(Corners(out, 1, inside), Identity(), 1.0f);
		EXPECT_TRUE(cull.visible);
		EXPECT_TRUE(cull.partlyOutside);
	}
}

TEST(DrawListBlocks, NaNDepthIsNear)
{
	// a NaN depth is not at or beyond the near clip, so it sets near, and no other compare holds
	auto cull = draw_list::CullBlock(Corners(glm::vec3(0.0f, 0.0f, k_NaN), 1, glm::vec3(0.0f, 0.0f, 5.0f)), Identity(), 1.0f);
	EXPECT_TRUE(cull.visible);
	EXPECT_TRUE(cull.partlyOutside);
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, 0.0f, k_NaN)), Identity(), 1.0f).visible);
}

TEST(DrawListBlocks, DepthIsTheThirdColumnAndTranslation)
{
	// d = z + t.z: with t.z = -3 a corner at z = 3.5 has d = 0.5, nearer than the near clip
	EXPECT_FALSE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, 0.0f, 3.5f)), Translated({0.0f, 0.0f, -3.0f}), 1.0f).visible);
	EXPECT_TRUE(draw_list::CullBlock(AllAt(glm::vec3(0.0f, 0.0f, 4.5f)), Translated({0.0f, 0.0f, -3.0f}), 1.0f).visible);
}

TEST(DrawListBlocks, DistanceToTheBoxCentre)
{
	// centre (160 + 80, 100 x 0.67 x 0.5 = 33.5, 320 + 80); the eye 3 and 4 away: 5, not 25
	EXPECT_EQ(draw_list::BlockDistance(glm::vec2(160.0f, 320.0f), 100, false, glm::vec3(243.0f, 37.5f, 400.0f)), 5.0f);
	// with the land reflection the centre is at y = 0
	EXPECT_EQ(draw_list::BlockDistance(glm::vec2(160.0f, 320.0f), 100, true, glm::vec3(243.0f, 4.0f, 400.0f)), 5.0f);
}

TEST(DrawListBlocks, DistanceSumOrder)
{
	// (dz dz + dy dy) + dx dx, then the root. Here the other order, (dx dx + dy dy) + dz dz, is one bit lower
	const glm::vec3 eye(159.6f, 177.2f, 248.8f);
	const float distance = draw_list::BlockDistance(glm::vec2(0.0f), 0, true, eye);
	EXPECT_EQ(std::bit_cast<uint32_t>(distance), 0x4380ACE8u);
	const float dx = 80.0f - eye.x;
	const float dy = 0.0f - eye.y;
	const float dz = 80.0f - eye.z;
	EXPECT_EQ(std::bit_cast<uint32_t>(std::sqrt((dx * dx + dy * dy) + dz * dz)), 0x4380ACE7u);
}

TEST(DrawListBlocks, VisibleBlockTakesTheNewState)
{
	// the block at (0, 0) with height 0: X in -80..80 and the depth 1000..1160, inside every side
	draw_list::DrawCamera camera;
	camera.worldToClipping = Translated({-80.0f, 0.0f, 1000.0f});
	camera.nearW = 1.0f;
	camera.eye = glm::vec3(80.0f, 3.0f, 84.0f);
	const draw_list::BlockState previous {
	    .distance = 7.0f, .visibility = 0x8u | draw_list::k_LandClipBit, .partlyOutside = true};
	auto next = draw_list::NextBlockState(previous, glm::vec2(0.0f), 0, camera, LinesAtZ());
	EXPECT_EQ(next.visibility, 0x8u | draw_list::k_LandClipBit | draw_list::k_InViewBit | draw_list::k_InViewSecondBit);
	EXPECT_FALSE(next.partlyOutside);
	EXPECT_EQ(next.distance, 5.0f);
	// X 1000..1160 against the depth 1000..1160: some corners right of the view, not all
	camera.worldToClipping = Translated({1000.0f, 0.0f, 1000.0f});
	next = draw_list::NextBlockState(previous, glm::vec2(0.0f), 0, camera, LinesAtZ());
	EXPECT_TRUE(next.partlyOutside);
}

TEST(DrawListBlocks, CulledBlockKeepsItsDistanceAndLandClipBit)
{
	// every depth below 0: culled
	draw_list::DrawCamera camera;
	camera.worldToClipping = Translated({0.0f, 0.0f, -1000.0f});
	camera.nearW = 1.0f;
	camera.eye = glm::vec3(80.0f, 3.0f, 84.0f);
	const draw_list::BlockState previous {.distance = 7.0f,
	                                      .visibility = 0x8u | draw_list::k_LandClipBit | draw_list::k_InViewBit |
	                                                    draw_list::k_InViewSecondBit,
	                                      .partlyOutside = true};
	const auto next = draw_list::NextBlockState(previous, glm::vec2(0.0f), 0, camera, LinesAtZ());
	EXPECT_EQ(next.visibility, 0x8u | draw_list::k_LandClipBit);
	EXPECT_TRUE(next.partlyOutside);
	EXPECT_EQ(next.distance, 7.0f);
}

TEST(DrawListBlocks, VisibleBlockTakesItsLodAndKeepsItsSeams)
{
	draw_list::DrawCamera camera;
	camera.worldToClipping = Translated({-80.0f, 0.0f, 1000.0f});
	camera.nearW = 1.0f;
	const draw_list::BlockState previous {.lod = {.meshLod = 2, .meshBlending = 4, .meshLodType = 5}};
	// the block's z 0..160, well inside the inner line at 1000: full detail
	auto next = draw_list::NextBlockState(previous, glm::vec2(0.0f), 0, camera, LinesAtZ());
	EXPECT_EQ(next.lod.meshLod, 0u);
	EXPECT_EQ(next.lod.meshBlending, 0u);
	EXPECT_EQ(next.lod.meshLodType, 5u);
	// both lines behind the block: quarter detail
	next = draw_list::NextBlockState({}, glm::vec2(0.0f), 0, camera, LinesAtZ(-500.0f, -300.0f));
	EXPECT_EQ(next.lod.meshLod, 2u);
	EXPECT_EQ(next.lod.meshBlending, 4u);
}

TEST(DrawListBlocks, CulledBlockTakesStepZeroBlendingFourAndKeepsItsSeams)
{
	draw_list::DrawCamera camera;
	camera.worldToClipping = Translated({0.0f, 0.0f, -1000.0f});
	camera.nearW = 1.0f;
	const draw_list::BlockState previous {.lod = {.meshLod = 2, .meshBlending = 3, .meshLodType = 9}};
	const auto next = draw_list::NextBlockState(previous, glm::vec2(0.0f), 0, camera, LinesAtZ());
	EXPECT_EQ(next.lod.meshLod, 0u);
	EXPECT_EQ(next.lod.meshBlending, 4u);
	EXPECT_EQ(next.lod.meshLodType, 9u);
}

TEST(DrawListBlocks, ALandLoadClearsEverythingButTheDistance)
{
	const auto state = draw_list::InitialBlockState(12.5f);
	EXPECT_EQ(state.visibility, 0u); // the land clip's last result too: a block's first kept triangle is a change
	EXPECT_FALSE(state.partlyOutside);
	EXPECT_EQ(state.lod.meshLod, 0u);
	EXPECT_EQ(state.lod.meshBlending, 0u);
	EXPECT_EQ(state.lod.meshLodType, 0u);
	EXPECT_EQ(state.distance, 12.5f);
}

TEST(DrawListBlocks, SortNearestFirstLaterBlockFirstOnATie)
{
	const auto in = [](float distance) {
		return draw_list::BlockState {.distance = distance, .visibility = draw_list::k_InViewBit};
	};
	const std::vector<draw_list::BlockState> blocks {
	    in(5.0f), in(3.0f), {.distance = 0.5f, .visibility = draw_list::k_LandClipBit}, in(5.0f), in(1.0f)};
	std::vector<uint16_t> out {9, 9, 9};
	draw_list::SortVisible(blocks, out);
	// block 2 is not in view; block 3 ties with block 0 and goes before it
	EXPECT_EQ(out, (std::vector<uint16_t> {4, 1, 3, 0}));
}

TEST(DrawListBlocks, SortNaN)
{
	const auto in = [](float distance) {
		return draw_list::BlockState {.distance = distance, .visibility = draw_list::k_InViewBit};
	};
	std::vector<uint16_t> out;
	// a NaN distance compares unordered with every block, so the walk passes them all: it goes last
	draw_list::SortVisible(std::vector {in(2.0f), in(k_NaN), in(1.0f)}, out);
	EXPECT_EQ(out, (std::vector<uint16_t> {2, 0, 1}));
	// and a NaN already in the list is walked past
	draw_list::SortVisible(std::vector {in(k_NaN), in(3.0f), in(2.0f)}, out);
	EXPECT_EQ(out, (std::vector<uint16_t> {0, 2, 1}));
}

TEST(DrawListBlocks, NearClipBands)
{
	EXPECT_EQ(draw_list::NearClipFor(10.0f, true), 0.1f);
	EXPECT_EQ(draw_list::NearClipFor(0.0f, false), 0.3f);
	EXPECT_EQ(draw_list::NearClipFor(-5.0f, false), 0.3f);
	EXPECT_EQ(draw_list::NearClipFor(k_NaN, false), 0.3f);
	// (h x 0.05) x 3.2 + 0.3
	EXPECT_EQ(std::bit_cast<uint32_t>(draw_list::NearClipFor(0.5f, false)), 0x3EC28F5Du);
	EXPECT_EQ(std::bit_cast<uint32_t>(draw_list::NearClipFor(10.0f, false)), 0x3FF33334u);
	EXPECT_EQ(draw_list::NearClipFor(20.0f, false), 3.5f);
	EXPECT_EQ(draw_list::NearClipFor(25.0f, false), 3.5f);
}

TEST(DrawListBlocks, LodLinesAtLandCreation)
{
	const auto& start = draw_list::k_StartLodLines;
	EXPECT_EQ(start.inner.distance, 300.0f);
	EXPECT_EQ(start.inner.width, 50.0f);
	EXPECT_EQ(start.inner.halfWidth, 25.0f);
	EXPECT_EQ(start.outer.distance, 600.0f);
	EXPECT_EQ(start.outer.width, 50.0f);
	EXPECT_EQ(start.outer.halfWidth, 25.0f);
	EXPECT_EQ(start.inner.along, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(start.inner.point, glm::vec3(0.0f));

	// index 4 sets both afresh, the direction and point included
	draw_list::LodLines moved = start;
	moved.inner.along = moved.outer.along = glm::vec3(0.0f, 0.0f, -1.0f);
	moved.inner.point = moved.outer.point = glm::vec3(5.0f, 0.0f, 6.0f);
	const auto lines = draw_list::LodLinesAtLandCreation(moved, 4);
	EXPECT_EQ(lines.inner.distance, 1600.0f);
	EXPECT_EQ(lines.inner.width, 550.0f);
	EXPECT_EQ(lines.inner.halfWidth, 275.0f);
	EXPECT_EQ(lines.outer.distance, 2500.0f);
	EXPECT_EQ(lines.outer.width, 550.0f);
	EXPECT_EQ(lines.outer.halfWidth, 275.0f);
	EXPECT_EQ(lines.inner.along, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(lines.outer.point, glm::vec3(0.0f));

	// any other index leaves them as they are
	for (const int32_t index : {0, 1, 2, 3})
	{
		const auto kept = draw_list::LodLinesAtLandCreation(moved, index);
		EXPECT_EQ(kept.inner.distance, 300.0f) << index;
		EXPECT_EQ(kept.outer.distance, 600.0f) << index;
		EXPECT_EQ(kept.inner.point, glm::vec3(5.0f, 0.0f, 6.0f)) << index;
		EXPECT_EQ(kept.outer.along, glm::vec3(0.0f, 0.0f, -1.0f)) << index;
	}
}

TEST(DrawListBlocks, LodLineAlongTheAxes)
{
	// looking along +z from (100, 50, 200): the line 1600 ahead is z = 1800, across x
	draw_list::LodLine line = draw_list::k_StartLodLines.inner;
	line.distance = 1600.0f;
	auto rebuilt = draw_list::RebuildLodLine(line, glm::vec3(100.0f, 50.0f, 200.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(rebuilt.along, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(rebuilt.point, glm::vec3(0.0f, 0.0f, 1800.0f));
	EXPECT_EQ(rebuilt.distance, 1600.0f);
	EXPECT_EQ(rebuilt.halfWidth, 25.0f);
	// the camera's side is near
	EXPECT_EQ(draw_list::SideOfLine(rebuilt, 9999.0f, 1700.0f), draw_list::LineSide::Near);
	EXPECT_EQ(draw_list::SideOfLine(rebuilt, -9999.0f, 1900.0f), draw_list::LineSide::Far);

	// looking along +x: the line is x = 100 + 1600, across z
	rebuilt = draw_list::RebuildLodLine(line, glm::vec3(100.0f, 50.0f, 200.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(rebuilt.along, glm::vec3(0.0f, 0.0f, -1.0f));
	EXPECT_EQ(rebuilt.point.x, 1700.0f);
	EXPECT_EQ(draw_list::SideOfLine(rebuilt, 1600.0f, 9999.0f), draw_list::LineSide::Near);
	EXPECT_EQ(draw_list::SideOfLine(rebuilt, 1800.0f, -9999.0f), draw_list::LineSide::Far);
}

TEST(DrawListBlocks, LodLineBits)
{
	const glm::vec3 eye(1234.5f, 87.25f, 2345.75f);
	draw_list::LodLine line = draw_list::k_StartLodLines.inner;
	line.distance = 1600.0f;
	// the line runs more across x than z: the point is on x = 0
	auto rebuilt = draw_list::RebuildLodLine(line, eye, glm::vec3(0.48f, -0.6f, 0.64f));
	EXPECT_EQ(Bits(rebuilt.along.x), 0x3F4CCCCCu);
	EXPECT_EQ(Bits(rebuilt.along.y), 0x0u);
	EXPECT_EQ(Bits(rebuilt.along.z), 0xBF199999u);
	EXPECT_EQ(Bits(rebuilt.point.x), 0x0u);
	EXPECT_EQ(Bits(rebuilt.point.z), 0x45B1CEA0u);
	// more across z: the point is on z = 0, and the reciprocal's sign reaches the zero components
	line.distance = 2500.0f;
	rebuilt = draw_list::RebuildLodLine(line, eye, glm::vec3(0.64f, -0.6f, 0.48f));
	EXPECT_EQ(Bits(rebuilt.along.x), 0x3F199999u);
	EXPECT_EQ(Bits(rebuilt.along.z), 0xBF4CCCCCu);
	EXPECT_EQ(Bits(rebuilt.point.x), 0x45D51220u);
	EXPECT_EQ(Bits(rebuilt.point.y), 0x80000000u);
	EXPECT_EQ(Bits(rebuilt.point.z), 0x80000000u);
}

TEST(DrawListBlocks, LodLineTieGoesToTheXAxis)
{
	// equal squares: the strict compare does not take z = 0, so the point is on x = 0
	draw_list::LodLine line = draw_list::k_StartLodLines.inner;
	line.distance = 1600.0f;
	const auto rebuilt = draw_list::RebuildLodLine(line, glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.6f, 0.0f, 0.6f));
	EXPECT_EQ(Bits(rebuilt.along.x), 0x3F3504F3u);
	EXPECT_EQ(Bits(rebuilt.along.z), 0xBF3504F3u);
	EXPECT_EQ(Bits(rebuilt.point.x), 0x0u);
	EXPECT_EQ(Bits(rebuilt.point.z), 0x44F50001u);
}

TEST(DrawListBlocks, StraightDownKeepsLastFramesPoint)
{
	draw_list::LodLine line = draw_list::k_StartLodLines.inner;
	line.distance = 1600.0f;
	line.point = glm::vec3(7.0f, 8.0f, 9.0f);
	// 0.01 squared is the threshold itself, and the compare is strict
	auto rebuilt = draw_list::RebuildLodLine(line, glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.01f, -0.9999f, 0.0f));
	EXPECT_EQ(rebuilt.point, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(Bits(rebuilt.along.x), 0x0u);
	EXPECT_EQ(Bits(rebuilt.along.z), 0xBC23D70Au);
	// both below: the direction stays unscaled
	rebuilt = draw_list::RebuildLodLine(line, glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.0099f, -0.9999f, 0.0099f));
	EXPECT_EQ(rebuilt.point, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(rebuilt.along.x, 0.0099f);
	EXPECT_EQ(rebuilt.along.z, -0.0099f);
	// straight down: the same
	rebuilt = draw_list::RebuildLodLine(line, glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	EXPECT_EQ(rebuilt.point, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(rebuilt.along.x, 0.0f);
	EXPECT_EQ(rebuilt.along.z, 0.0f);
	// both lines at once
	const auto lines = draw_list::RebuildLodLines(LinesAtZ(), glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	EXPECT_EQ(lines.inner.point.z, 1000.0f);
	EXPECT_EQ(lines.outer.point.z, 2000.0f);
}

TEST(DrawListBlocks, SideOfLineTiesAreInTheBand)
{
	const auto line = LineAtZ(1000.0f);
	EXPECT_EQ(draw_list::SideOfLine(line, 0.0f, 974.0f), draw_list::LineSide::Near);
	EXPECT_EQ(draw_list::SideOfLine(line, 0.0f, 975.0f), draw_list::LineSide::Band);
	EXPECT_EQ(draw_list::SideOfLine(line, 0.0f, 1000.0f), draw_list::LineSide::Band);
	EXPECT_EQ(draw_list::SideOfLine(line, 0.0f, 1025.0f), draw_list::LineSide::Band);
	EXPECT_EQ(draw_list::SideOfLine(line, 0.0f, 1026.0f), draw_list::LineSide::Far);
	// a NaN distance is far
	EXPECT_EQ(draw_list::SideOfLine(LineAtZ(k_NaN), 0.0f, 0.0f), draw_list::LineSide::Far);
	// across x: v = (x - point.x) along.z
	const draw_list::LodLine acrossX {.along = {0.0f, 0.0f, -1.0f}, .point = {1000.0f, 0.0f, 0.0f}, .halfWidth = 25.0f};
	EXPECT_EQ(draw_list::SideOfLine(acrossX, 974.0f, 5.0f), draw_list::LineSide::Near);
	EXPECT_EQ(draw_list::SideOfLine(acrossX, 1025.0f, 5.0f), draw_list::LineSide::Band);
	EXPECT_EQ(draw_list::SideOfLine(acrossX, 1026.0f, 5.0f), draw_list::LineSide::Far);
}

TEST(DrawListBlocks, LodFromTheInnerLine)
{
	using Lod = std::pair<uint32_t, uint32_t>;
	// all 4 corners near: full detail
	EXPECT_EQ(LodAtZ(0.0f), Lod(0u, 0u));
	EXPECT_EQ(LodAtZ(814.0f), Lod(0u, 0u));
	// the far side's corners on the band's near end: in the band
	EXPECT_EQ(LodAtZ(815.0f), Lod(0u, 1u));
	// the near side's corners on the band's far end: in the band
	EXPECT_EQ(LodAtZ(1025.0f), Lod(0u, 1u));
	// near and far corners, none in the band: across the line
	EXPECT_EQ(LodAtZ(920.0f), Lod(0u, 1u));
	// all far: half detail, unless the outer line says otherwise
	EXPECT_EQ(LodAtZ(1026.0f), Lod(1u, 2u));
}

TEST(DrawListBlocks, LodFromTheOuterLine)
{
	using Lod = std::pair<uint32_t, uint32_t>;
	EXPECT_EQ(LodAtZ(1814.0f), Lod(1u, 2u));
	EXPECT_EQ(LodAtZ(1815.0f), Lod(1u, 3u));
	EXPECT_EQ(LodAtZ(1850.0f), Lod(1u, 3u));
	EXPECT_EQ(LodAtZ(1920.0f), Lod(1u, 3u));
	EXPECT_EQ(LodAtZ(2025.0f), Lod(1u, 3u));
	EXPECT_EQ(LodAtZ(2026.0f), Lod(2u, 4u));
	// the outer line is only asked when every corner is past the inner one
	EXPECT_EQ(LodAtZ(920.0f, LinesAtZ(1000.0f, 1000.0f)), Lod(0u, 1u));
	// NaN lines: every corner far of both
	EXPECT_EQ(LodAtZ(0.0f, LinesAtZ(k_NaN, k_NaN)), Lod(2u, 4u));
	// the corners at x + 160 count too
	const draw_list::LodLine acrossX {.along = {0.0f, 0.0f, -1.0f}, .point = {1000.0f, 0.0f, 0.0f}, .halfWidth = 25.0f};
	const auto lod = draw_list::ChooseBlockLod(glm::vec2(815.0f, 0.0f), {.inner = acrossX, .outer = acrossX});
	EXPECT_EQ(lod.meshLod, 0u);
	EXPECT_EQ(lod.meshBlending, 1u);
}

namespace
{
using Lookup = std::array<uint16_t, draw_list::k_BlockLookupSize>;

/// Puts block `index` (from 1) at (x, z) of the block grid
void Place(Lookup& lookup, int x, int z, uint16_t index)
{
	lookup.at(static_cast<size_t>(x) * 32 + static_cast<size_t>(z)) = index;
}

draw_list::BlockState WithStep(uint32_t meshLod, uint32_t meshBlending = 0)
{
	return {.lod = {.meshLod = meshLod, .meshBlending = meshBlending}};
}
} // namespace

TEST(DrawListBlocks, SeamBitsTowardsCoarserNeighbours)
{
	Lookup lookup {};
	Place(lookup, 5, 5, 1);
	Place(lookup, 4, 5, 2);
	Place(lookup, 5, 4, 3);
	Place(lookup, 5, 6, 4);
	Place(lookup, 6, 5, 5);
	std::vector<draw_list::BlockState> blocks {WithStep(0), WithStep(1), WithStep(1), WithStep(2), WithStep(1)};
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 0, lookup, blocks), 15u);
	// one bit each
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 1, lookup, blocks), draw_list::k_SeamHigherZ);
	blocks = {WithStep(0), WithStep(1), WithStep(0), WithStep(0), WithStep(0)};
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 0, lookup, blocks), draw_list::k_SeamLowerX);
	blocks = {WithStep(0), WithStep(0), WithStep(1), WithStep(0), WithStep(0)};
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 0, lookup, blocks), draw_list::k_SeamLowerZ);
	blocks = {WithStep(0), WithStep(0), WithStep(0), WithStep(0), WithStep(1)};
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 0, lookup, blocks), draw_list::k_SeamHigherX);
	// an equal or finer neighbour, and a culled one (step 0 whatever its blending), give none
	blocks = {WithStep(1), WithStep(1), WithStep(0), WithStep(0, 4), WithStep(1)};
	EXPECT_EQ(draw_list::SeamBits({5, 5}, 1, lookup, blocks), 0u);
	// no block there
	EXPECT_EQ(draw_list::SeamBits({20, 20}, 0, lookup, blocks), 0u);
}

TEST(DrawListBlocks, SeamBitsStayOnTheGrid)
{
	const std::vector<draw_list::BlockState> blocks {WithStep(2), WithStep(2)};
	// the lookup entry before (5, 0) is (4, 31), which is not its z - 1 neighbour
	Lookup lookup {};
	Place(lookup, 4, 31, 1);
	EXPECT_EQ(draw_list::SeamBits({5, 0}, 0, lookup, blocks), 0u);
	EXPECT_EQ(draw_list::SeamBits({4, 30}, 0, lookup, blocks), draw_list::k_SeamHigherZ);
	// the entry after (5, 31) is (6, 0), which is not its z + 1 neighbour
	lookup = {};
	Place(lookup, 6, 0, 1);
	EXPECT_EQ(draw_list::SeamBits({5, 31}, 0, lookup, blocks), 0u);
	EXPECT_EQ(draw_list::SeamBits({6, 1}, 0, lookup, blocks), draw_list::k_SeamLowerZ);
	// nothing is read past the grid's corners
	lookup = {};
	Place(lookup, 0, 0, 1);
	Place(lookup, 31, 31, 2);
	EXPECT_EQ(draw_list::SeamBits({0, 0}, 0, lookup, blocks), 0u);
	EXPECT_EQ(draw_list::SeamBits({31, 31}, 0, lookup, blocks), 0u);
	EXPECT_EQ(draw_list::SeamBits({1, 0}, 0, lookup, blocks), draw_list::k_SeamLowerX);
	EXPECT_EQ(draw_list::SeamBits({30, 31}, 0, lookup, blocks), draw_list::k_SeamHigherX);
}

TEST(DrawListBlocks, MarkSeamsRewritesEveryBlock)
{
	Lookup lookup {};
	Place(lookup, 5, 5, 1);
	Place(lookup, 6, 5, 2);
	Place(lookup, 7, 5, 3);
	std::vector<draw_list::BlockState> blocks {WithStep(0), WithStep(1), WithStep(2)};
	blocks[0].lod.meshLodType = 4;
	blocks[2].lod.meshLodType = 15;
	const std::vector<glm::ivec2> coords {{5, 5}, {6, 5}, {7, 5}};
	draw_list::MarkSeams(blocks, coords, lookup);
	EXPECT_EQ(blocks[0].lod.meshLodType, draw_list::k_SeamHigherX);
	EXPECT_EQ(blocks[1].lod.meshLodType, draw_list::k_SeamHigherX);
	EXPECT_EQ(blocks[2].lod.meshLodType, 0u);
	// the steps are left as they were
	EXPECT_EQ(blocks[1].lod.meshLod, 1u);
}

TEST(DrawListBlocks, Land1BlocksInFileOrder)
{
	// the sort takes the blocks in index order, so its tie order is the original's when our block i is its block
	// i + 1 (its slot 0 is never used)
	const auto path = Land1Path();
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "no Land1.lnd at " << path.string();
	}
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(path), lnd::LNDResult::Success);
	const auto& blocks = file.GetBlocks();
	ASSERT_FALSE(blocks.empty());
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		EXPECT_EQ(blocks[i].index, i + 1) << "block " << i;
	}
}
