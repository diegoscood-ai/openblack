/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object draw list's land clip (src/ECS/DrawList/LandClip.h) against the original's rules: undrawn open water is
// skipped, the split bit picks the diagonal, the screen area must be above 0 in emit order, the screen clamp is the
// camera's, the sides are only tested for a block that is partly outside, and a triangle that crosses a side is
// clipped and can still keep a piece. Then the levels of detail: the blend of a block's copy (its weight, its rounding,
// the band's ends, its x and z steps, a NaN line), the half and quarter grids, the seam table and its triangles, and
// the cell record the flags are read through, which does not follow the step or the seams. Last, the land pass's write
// of a block's result, which raises the land flag only when the result changes. Synthetic blocks and matrices; the one
// check on the original's data (Land1.lnd) skips without it.

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "ECS/DrawList/BlockCull.h"
#include "ECS/DrawList/LandClip.h"

using namespace openblack;
namespace draw_list = openblack::ecs::draw_list;

namespace
{
using Cells = std::array<lnd::LNDCell, draw_list::k_BlockCells>;

constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
constexpr glm::vec2 k_Screen640x480 {639.0f, 479.0f};

/// A flat block at altitude 0: no cell is water or split
Cells FlatCells()
{
	return Cells {};
}

/// Every cell flagged as undrawn open water
Cells WaterCells()
{
	Cells cells {};
	for (auto& cell : cells)
	{
		cell.flags = 0x02;
	}
	return cells;
}

void SetSplit(Cells& cells)
{
	for (auto& cell : cells)
	{
		cell.properties.split = 1;
	}
}

/// A matrix from its 12 cells (three rows of 3, then the translation), all others 0
affine::AffineMatrix Matrix(std::array<float, 12> m)
{
	affine::AffineMatrix matrix;
	matrix.m = m;
	return matrix;
}

/// X = x - 80 + dx, Y = z - 80 and a depth of 200 for a block at the map's origin: the block faces the camera and lies
/// on a 640 x 480 screen for dx = 0
affine::AffineMatrix Facing(float dx = 0.0f, float depth = 200.0f)
{
	return Matrix({1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -80.0f + dx, -80.0f, depth});
}

/// The same block seen from behind: X = 80 - x
affine::AffineMatrix FacingAway()
{
	return Matrix({-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 80.0f, -80.0f, 200.0f});
}

draw_list::DrawCamera Camera(const affine::AffineMatrix& worldToClipping, float nearW = 1.0f,
                             std::optional<glm::vec2> maxScreen = k_Screen640x480)
{
	draw_list::DrawCamera camera;
	camera.worldToClipping = worldToClipping;
	camera.nearW = nearW;
	camera.half = {320.0f, 240.0f};
	camera.maxScreen = maxScreen;
	return camera;
}

/// A line across x: v = x - pointX, the camera's side at larger x. Its band is 512 wide, so a weight is
/// (256 - v) / 2
draw_list::LodLine LineAcrossX(float pointX, float width = 512.0f)
{
	return {.along = {0.0f, 0.0f, 1.0f}, .point = {pointX, 0.0f, 0.0f}, .width = width, .halfWidth = width * 0.5f};
}

/// Every point of a block at the origin is on the camera's side of it, past the band
const draw_list::LodLine k_NearLine = LineAcrossX(-10000.0f);
/// Every point is past the band on the far side
const draw_list::LodLine k_FarLine = LineAcrossX(10000.0f);

std::optional<bool> Keeps(const Cells& cells, const draw_list::DrawCamera& camera, bool partlyOutside,
                          draw_list::BlockLod lod = {}, glm::vec2 blockMapPos = {0.0f, 0.0f},
                          const draw_list::LodLines& lines = {.inner = k_NearLine, .outer = k_NearLine})
{
	return draw_list::KeepsFrontTriangle(cells, blockMapPos, camera, partlyOutside, lod, lines);
}

/// X = (x + z) / 2 - 80, Y = h and a depth of 200: on a flat block every square is a line, and both corners of a
/// square's split diagonal fall on one screen point
affine::AffineMatrix AlongDiagonal()
{
	return Matrix({0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.0f, -80.0f, 0.0f, 200.0f});
}

/// The same seen from the other side: X = 80 - (x + z) / 2
affine::AffineMatrix AlongDiagonalMirrored()
{
	return Matrix({-0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.5f, 0.0f, 0.0f, 80.0f, 0.0f, 200.0f});
}

/// A flat block whose last point (record 288) is raised. Seen along the diagonal, only the grid's last square can keep
/// a triangle, and only when the flags it reads say it is not split
Cells LastPointRaised(bool split)
{
	Cells cells {};
	for (auto& cell : cells)
	{
		cell.properties.split = split ? 1 : 0;
	}
	cells.back().altitude = 100;
	return cells;
}

/// The altitudes of a block, row after row
std::array<uint8_t, draw_list::k_BlockCells> Altitudes(const Cells& cells)
{
	std::array<uint8_t, draw_list::k_BlockCells> altitudes {};
	std::ranges::transform(cells, altitudes.begin(), [](const lnd::LNDCell& cell) { return cell.altitude; });
	return altitudes;
}

/// The altitude of the point at row r and column c after the blend of a block whose points are all past the band:
/// the coarser grid's points stay, the others take the two points they lie between, added and halved
uint8_t FarBlend(const Cells& cells, size_t row, size_t column, size_t step)
{
	const size_t half = step / 2;
	const auto at = [&cells](size_t r, size_t c) { return static_cast<uint32_t>(cells[r * 17 + c].altitude); };
	const bool rowOnGrid = row % step == 0;
	const bool columnOnGrid = column % step == 0;
	if (rowOnGrid && columnOnGrid)
	{
		return cells[row * 17 + column].altitude;
	}
	if (rowOnGrid && column % step == half)
	{
		return static_cast<uint8_t>((at(row, column - half) + at(row, column + half)) >> 1);
	}
	if (row % step == half && column % step == half)
	{
		return static_cast<uint8_t>((at(row - half, column - half) + at(row + half, column + half)) >> 1);
	}
	if (row % step == half && columnOnGrid)
	{
		return static_cast<uint8_t>((at(row - half, column) + at(row + half, column)) >> 1);
	}
	return cells[row * 17 + column].altitude;
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

TEST(LandClip, aBlockFacingTheCameraKeepsATriangle)
{
	auto cells = FlatCells();
	EXPECT_EQ(Keeps(cells, Camera(Facing()), false), true);
	// all inside: the same result through the sides' tests
	EXPECT_EQ(Keeps(cells, Camera(Facing()), true), true);
	SetSplit(cells);
	EXPECT_EQ(Keeps(cells, Camera(Facing()), false), true);
	// the cells step on from the block's map position
	const auto moved = Matrix({1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -240.0f, -400.0f, 200.0f});
	EXPECT_EQ(Keeps(FlatCells(), Camera(moved), true, {}, {160.0f, 320.0f}), true);
}

TEST(LandClip, aBlockSeenFromBehindKeepsNone)
{
	auto cells = FlatCells();
	EXPECT_EQ(Keeps(cells, Camera(FacingAway()), false), false);
	EXPECT_EQ(Keeps(cells, Camera(FacingAway()), true), false);
	SetSplit(cells);
	EXPECT_EQ(Keeps(cells, Camera(FacingAway()), false), false);
}

TEST(LandClip, stepsPastTwoSeamBitsPastFifteenAndNoScreenClampGiveNoAnswer)
{
	const auto cells = FlatCells();
	const auto camera = Camera(Facing());
	EXPECT_FALSE(Keeps(cells, camera, false, {.meshLod = 3}).has_value());
	EXPECT_FALSE(Keeps(cells, camera, false, {.meshLodType = 16}).has_value());
	EXPECT_FALSE(Keeps(cells, camera, false, {.meshLod = 1, .meshBlending = 2, .meshLodType = 16}).has_value());
	// step 2 never reads the seam bits
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 2, .meshBlending = 4, .meshLodType = 16}), true);
	EXPECT_FALSE(Keeps(cells, Camera(Facing(), 1.0f, std::nullopt), false).has_value());
	// every level of detail the cull chooses gives an answer
	for (const draw_list::BlockLod lod :
	     {draw_list::BlockLod {.meshLod = 0, .meshBlending = 1}, draw_list::BlockLod {.meshLod = 1, .meshBlending = 2},
	      draw_list::BlockLod {.meshLod = 1, .meshBlending = 3}, draw_list::BlockLod {.meshLod = 2, .meshBlending = 4}})
	{
		EXPECT_EQ(Keeps(cells, camera, false, lod), true);
		EXPECT_EQ(Keeps(cells, Camera(FacingAway()), false, lod), false);
	}
}

TEST(LandClip, undrawnOpenWaterCellsAreSkipped)
{
	auto cells = WaterCells();
	EXPECT_EQ(Keeps(cells, Camera(Facing()), false), false);
	// only while the blending is 0
	for (const uint32_t blending : {1u, 2u, 3u, 4u})
	{
		EXPECT_EQ(Keeps(cells, Camera(Facing()), false, {.meshLod = blending / 2, .meshBlending = blending}), true) << blending;
	}
	cells[100].flags = 0;
	EXPECT_EQ(Keeps(cells, Camera(Facing()), false), true);
	// the other flags do not skip a cell
	cells = FlatCells();
	for (auto& cell : cells)
	{
		cell.flags = 0xFD;
	}
	EXPECT_EQ(Keeps(cells, Camera(Facing()), false), true);
}

TEST(LandClip, theSplitBitPicksTheDiagonal)
{
	// X = x - z and Y = h: the cell's corners at (0, 0) and (10, 10) fall on the same screen point, so the two
	// triangles across that diagonal have no area, while the other diagonal's are one front and one back
	const auto camera = Camera(Matrix({1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f}));
	auto cells = WaterCells();
	cells[0].flags = 0;
	// the corner at (0, 10) has height 6.7 and the one at (10, 0) 13.4
	cells[1].altitude = 10;
	cells[17].altitude = 20;
	EXPECT_EQ(Keeps(cells, camera, false), false);
	cells[0].properties.split = 1;
	EXPECT_EQ(Keeps(cells, camera, false), true);
}

TEST(LandClip, altitudesUpToThreeAreFlat)
{
	// as above, with the two raised corners at altitude 3: every point of the cell is at height 0, on one line
	const auto camera = Camera(Matrix({1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f}));
	auto cells = WaterCells();
	cells[0].flags = 0;
	cells[0].properties.split = 1;
	cells[1].altitude = 3;
	cells[17].altitude = 3;
	EXPECT_EQ(Keeps(cells, camera, false), false);
	cells[17].altitude = 4;
	EXPECT_EQ(Keeps(cells, camera, false), true);
}

TEST(LandClip, aTriangleWithNoAreaIsNotKept)
{
	// X = -80 everywhere: every point on one screen column
	const auto camera = Camera(Matrix({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -80.0f, -80.0f, 200.0f}));
	EXPECT_EQ(Keeps(FlatCells(), camera, false), false);
}

TEST(LandClip, screenPointsAreClampedToTheCamerasClamp)
{
	// the block is right of the screen: every point clamps to x = 639 and the triangles lose their area
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(480.0f)), false), false);
	// left of it: to x = 0
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(-720.0f)), false), false);
	// a wider clamp keeps it
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(480.0f), 1.0f, glm::vec2(10000.0f, 10000.0f)), false), true);
}

TEST(LandClip, aNaNDepthKeepsNothing)
{
	// on the screen a NaN clamps to 0; tested against the sides, it is near
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(0.0f, k_NaN)), false), false);
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(0.0f, k_NaN)), true), false);
}

TEST(LandClip, theSidesAreOnlyTestedForABlockPartlyOutside)
{
	// wholly right of the view, with a clamp wide enough to keep its area: projected as it is when the block is
	// fully inside, dropped when the sides are tested
	const auto camera = Camera(Facing(480.0f), 1.0f, glm::vec2(10000.0f, 10000.0f));
	EXPECT_EQ(Keeps(FlatCells(), camera, false), true);
	EXPECT_EQ(Keeps(FlatCells(), camera, true), false);
	// wholly behind the near clip
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(), 300.0f), true), false);
}

TEST(LandClip, aTriangleAcrossTheRightSideKeepsItsInsidePiece)
{
	// X = x + 195 against a depth of 200: only the points at x = 0 are inside, so only the cells of the first row
	// cross the side, and only their clipped pieces, between screen x 632 and 639, can be kept
	const auto camera = Camera(Facing(275.0f));
	auto cells = FlatCells();
	EXPECT_EQ(Keeps(cells, camera, true), true);
	SetSplit(cells);
	EXPECT_EQ(Keeps(cells, camera, true), true);
	cells = FlatCells();
	for (size_t column = 0; column < draw_list::k_BlockSide; ++column)
	{
		cells[column].flags = 0x02;
	}
	EXPECT_EQ(Keeps(cells, camera, true), false);
	// cell 1 is in the first row (x = 0), cell 17 in the second (x = 10)
	cells = WaterCells();
	cells[1].flags = 0;
	EXPECT_EQ(Keeps(cells, camera, true), true);
	cells = WaterCells();
	cells[17].flags = 0;
	EXPECT_EQ(Keeps(cells, camera, true), false);
}

TEST(LandClip, clippedPointsAreClampedToo)
{
	// the pieces of the test above: a clamp left of x 632 puts all their points on one column
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(275.0f), 1.0f, glm::vec2(631.0f, 479.0f)), true), false);
	EXPECT_EQ(Keeps(FlatCells(), Camera(Facing(275.0f), 1.0f, glm::vec2(633.0f, 479.0f)), true), true);
}

TEST(LandClip, aTriangleAcrossTheNearClipKeepsItsInsidePiece)
{
	// the depth is 5 - z: only the points at z = 0 are beyond the near clip of 1, so only the cells of the first column
	// cross it
	const auto camera = Camera(Matrix({0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.01f, -1.0f, -0.8f, 0.8f, 5.0f}));
	auto cells = FlatCells();
	EXPECT_EQ(Keeps(cells, camera, true), true);
	for (size_t row = 0; row < draw_list::k_BlockSide; ++row)
	{
		cells[row * draw_list::k_BlockSide].flags = 0x02;
	}
	EXPECT_EQ(Keeps(cells, camera, true), false);
}

TEST(LandClip, theBlendsWeightIsRoundedToTheNearestTiesToEven)
{
	// the point at row 0, column 1 lies between the points at columns 0 and 2, both 255; at v = 103 the weight is
	// 76.5, which goes to 76 (255 x 76 / 256 = 75), and at v = 101 it is 77.5, which goes to 78 (77)
	for (const auto& [pointX, expected] : {std::pair {-103.0f, 75}, std::pair {-101.0f, 77}})
	{
		Cells cells {};
		cells[0].altitude = 255;
		cells[2].altitude = 255;
		draw_list::BlendTowardsCoarserGrid(cells, {0.0f, 0.0f}, LineAcrossX(pointX), true);
		EXPECT_EQ(cells[1].altitude, expected) << pointX;
	}
	// on the line the weight is 128: (128 x 100 + 128 x 255) / 256 = 177.5, rounded down
	Cells cells {};
	cells[0].altitude = 255;
	cells[1].altitude = 100;
	cells[2].altitude = 255;
	draw_list::BlendTowardsCoarserGrid(cells, {0.0f, 0.0f}, LineAcrossX(0.0f), true);
	EXPECT_EQ(cells[1].altitude, 177);
}

TEST(LandClip, theBandsEndsLeaveThePointOrTakeTheMiddle)
{
	// at v = 256, half the band, the point is left; at v = -256 it takes the middle, (255 + 254) / 2 rounded down
	Cells cells {};
	cells[0].altitude = 255;
	cells[1].altitude = 100;
	cells[2].altitude = 254;
	auto left = cells;
	draw_list::BlendTowardsCoarserGrid(left, {0.0f, 0.0f}, LineAcrossX(-256.0f), true);
	EXPECT_EQ(left[1].altitude, 100);
	auto middle = cells;
	draw_list::BlendTowardsCoarserGrid(middle, {0.0f, 0.0f}, LineAcrossX(256.0f), true);
	EXPECT_EQ(middle[1].altitude, 254);
}

TEST(LandClip, theBlendStepsXAndZFromEachPassesFirstPoint)
{
	// along x the line is v = point.z - z: the first pass starts at z = 10 (v = 103, 75) and the third at z = 0
	// (v = 113, a weight of 71.5 that goes to 72, so 71)
	{
		Cells cells {};
		cells[0].altitude = 255;
		cells[2].altitude = 255;
		cells[34].altitude = 255;
		const draw_list::LodLine line {.along = {1.0f, 0.0f, 0.0f}, .point = {0.0f, 0.0f, 113.0f}, .width = 512.0f};
		draw_list::BlendTowardsCoarserGrid(cells, {0.0f, 0.0f}, line, true);
		EXPECT_EQ(cells[1].altitude, 75);
		EXPECT_EQ(cells[17].altitude, 71);
	}
	// the third pass starts at x = 10: v = 10 + 93
	{
		Cells cells {};
		cells[0].altitude = 255;
		cells[34].altitude = 255;
		draw_list::BlendTowardsCoarserGrid(cells, {0.0f, 0.0f}, LineAcrossX(-93.0f), true);
		EXPECT_EQ(cells[17].altitude, 75);
	}
	// and every pass from the block's map position: v = 160 - 57
	{
		Cells cells {};
		cells[0].altitude = 255;
		cells[2].altitude = 255;
		draw_list::BlendTowardsCoarserGrid(cells, {160.0f, 320.0f}, LineAcrossX(57.0f), true);
		EXPECT_EQ(cells[1].altitude, 75);
	}
}

TEST(LandClip, aBlockPastTheBandTakesTheCoarserGridsMiddles)
{
	Cells cells {};
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].altitude = static_cast<uint8_t>((i * 37 + (i / 17) * 11) & 0xFF);
		// the centres always use the same diagonal
		cells[i].properties.split = (i % 3) == 0 ? 1 : 0;
	}
	for (const bool towardsHalfGrid : {true, false})
	{
		auto blended = cells;
		draw_list::BlendTowardsCoarserGrid(blended, {0.0f, 0.0f}, k_FarLine, towardsHalfGrid);
		const size_t step = towardsHalfGrid ? 2 : 4;
		for (size_t row = 0; row < draw_list::k_BlockSide; ++row)
		{
			for (size_t column = 0; column < draw_list::k_BlockSide; ++column)
			{
				EXPECT_EQ(blended[row * 17 + column].altitude, FarBlend(cells, row, column, step))
				    << "step " << step << " row " << row << " column " << column;
			}
		}
		// on the camera's side nothing changes
		auto near = cells;
		draw_list::BlendTowardsCoarserGrid(near, {0.0f, 0.0f}, k_NearLine, towardsHalfGrid);
		EXPECT_EQ(Altitudes(near), Altitudes(cells));
	}
}

TEST(LandClip, aNaNLineLeavesTheBlockAsItIs)
{
	Cells cells {};
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].altitude = static_cast<uint8_t>(i & 0xFF);
	}
	for (const bool towardsHalfGrid : {true, false})
	{
		auto blended = cells;
		draw_list::BlendTowardsCoarserGrid(blended, {0.0f, 0.0f}, LineAcrossX(k_NaN), towardsHalfGrid);
		EXPECT_EQ(Altitudes(blended), Altitudes(cells));
	}
}

TEST(LandClip, blendingOneAndThreeClipTheBlendedCopy)
{
	const auto camera = Camera(AlongDiagonal());
	// a raised point at row 16, column 15: half-way between two points of the half grid
	Cells cells {};
	cells[287].altitude = 100;
	EXPECT_EQ(Keeps(cells, camera, false, {.meshBlending = 1}, {}, {.inner = k_NearLine, .outer = k_FarLine}), true);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshBlending = 1}, {}, {.inner = k_FarLine, .outer = k_NearLine}), false);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshBlending = 0}, {}, {.inner = k_FarLine, .outer = k_FarLine}), true);
	// the block itself is not changed
	EXPECT_EQ(cells[287].altitude, 100);
	// at column 14: a point of the half grid, half-way between two of the quarter grid
	cells = Cells {};
	cells[286].altitude = 100;
	const draw_list::BlockLod blendingThree {.meshLod = 1, .meshBlending = 3};
	EXPECT_EQ(Keeps(cells, camera, false, blendingThree, {}, {.inner = k_FarLine, .outer = k_NearLine}), true);
	EXPECT_EQ(Keeps(cells, camera, false, blendingThree, {}, {.inner = k_NearLine, .outer = k_FarLine}), false);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 1, .meshBlending = 2}, {}, {.inner = k_FarLine, .outer = k_FarLine}),
	          true);
	// the half grid reads every second record: column 15 is not one of its points
	cells = Cells {};
	cells[287].altitude = 100;
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 1, .meshBlending = 2}), false);
}

TEST(LandClip, theSeamTable)
{
	using draw_list::SeamEdges;
	const auto same = [](const SeamEdges& a, const SeamEdges& b) {
		return a.fans == b.fans && a.skipFirstRow == b.skipFirstRow && a.skipLastRow == b.skipLastRow &&
		       a.skipFirstColumn == b.skipFirstColumn && a.skipLastColumn == b.skipLastColumn;
	};
	const std::array<SeamEdges, 16> expected {{
	    {},
	    {.fans = true, .skipFirstRow = true},
	    {.fans = true, .skipFirstColumn = true},
	    {.fans = true, .skipFirstRow = true, .skipFirstColumn = true},
	    {.fans = true, .skipLastColumn = true},
	    {.fans = true, .skipFirstRow = true, .skipLastColumn = true},
	    {},
	    {},
	    {.fans = true, .skipLastRow = true},
	    {},
	    {.fans = true, .skipLastRow = true, .skipFirstColumn = true},
	    {},
	    {.fans = true, .skipLastRow = true, .skipLastColumn = true},
	    {},
	    {},
	    {},
	}};
	for (uint32_t bits = 0; bits < 16; ++bits)
	{
		const auto edges = draw_list::SeamEdgesFor(bits);
		ASSERT_TRUE(edges.has_value()) << bits;
		EXPECT_TRUE(same(*edges, expected.at(bits))) << bits;
	}
	EXPECT_FALSE(draw_list::SeamEdgesFor(16).has_value());
}

TEST(LandClip, theSeamTriangles)
{
	// the count of each set, and its first and last triangles, as the original's tables hold them
	struct Expected
	{
		uint32_t meshLod;
		uint32_t bits;
		size_t count;
		draw_list::SeamTriangle first;
		draw_list::SeamTriangle last;
	};
	const std::array<Expected, 16> sets {{
	    {0, 1, 24, {0, 2, 18}, {16, 33, 32}},
	    {0, 8, 24, {256, 274, 272}, {270, 271, 288}},
	    {0, 2, 24, {0, 18, 34}, {256, 273, 272}},
	    {0, 4, 24, {16, 50, 32}, {270, 288, 287}},
	    {0, 3, 46, {0, 2, 18}, {256, 273, 272}},
	    {0, 5, 46, {0, 2, 18}, {270, 288, 287}},
	    {0, 10, 46, {256, 274, 272}, {238, 239, 256}},
	    {0, 12, 46, {256, 274, 272}, {253, 254, 270}},
	    {1, 1, 12, {0, 2, 10}, {8, 17, 16}},
	    {1, 8, 12, {64, 74, 72}, {70, 71, 80}},
	    {1, 2, 12, {0, 10, 18}, {64, 73, 72}},
	    {1, 4, 12, {8, 26, 16}, {70, 80, 79}},
	    {1, 3, 22, {0, 2, 10}, {64, 73, 72}},
	    {1, 5, 22, {0, 2, 10}, {70, 80, 79}},
	    {1, 10, 22, {64, 74, 72}, {54, 55, 64}},
	    {1, 12, 22, {64, 74, 72}, {61, 62, 70}},
	}};
	for (const auto& set : sets)
	{
		const auto fan = draw_list::SeamFan(set.meshLod, set.bits);
		ASSERT_EQ(fan.size(), set.count) << set.meshLod << " " << set.bits;
		EXPECT_EQ(fan.front(), set.first) << set.meshLod << " " << set.bits;
		EXPECT_EQ(fan.back(), set.last) << set.meshLod << " " << set.bits;
		// every point is on the step's grid
		const size_t points = set.meshLod == 0 ? 289 : 81;
		for (const auto& triangle : fan)
		{
			EXPECT_TRUE(std::ranges::all_of(triangle, [points](uint16_t point) { return point < points; }));
		}
	}
	// no fans for the sets with no entry, nor at step 2
	for (const uint32_t bits : {0u, 6u, 7u, 9u, 11u, 13u, 14u, 15u, 16u})
	{
		EXPECT_TRUE(draw_list::SeamFan(0, bits).empty()) << bits;
		EXPECT_TRUE(draw_list::SeamFan(1, bits).empty()) << bits;
	}
	for (uint32_t bits = 0; bits < 16; ++bits)
	{
		EXPECT_TRUE(draw_list::SeamFan(2, bits).empty()) << bits;
	}
}

TEST(LandClip, seamTrianglesAreKeptWhateverTheCellFlags)
{
	// all open water at blending 0: only the seam triangles are left, and they face the camera
	const auto cells = WaterCells();
	const auto camera = Camera(Facing());
	for (uint32_t bits = 0; bits < 16; ++bits)
	{
		const bool hasFans = draw_list::SeamEdgesFor(bits)->fans;
		EXPECT_EQ(Keeps(cells, camera, false, {.meshLodType = bits}), hasFans) << bits;
		EXPECT_EQ(Keeps(cells, camera, true, {.meshLodType = bits}), hasFans) << bits;
	}
}

TEST(LandClip, seamTrianglesTakeTheLeftOutEdgesPlace)
{
	// the last square is split, so it keeps nothing; with the last row left out, the seam triangles along it reach the
	// raised point and one faces the camera. Step 2 has no seams, so its last square is drawn and keeps nothing
	const auto cells = LastPointRaised(true);
	const auto camera = Camera(AlongDiagonalMirrored());
	EXPECT_EQ(Keeps(cells, camera, false, {}), false);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLodType = draw_list::k_SeamHigherX}), true);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 1, .meshBlending = 2}), false);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 1, .meshBlending = 2, .meshLodType = draw_list::k_SeamHigherX}), true);
	EXPECT_EQ(Keeps(cells, camera, false, {.meshLod = 2, .meshBlending = 4, .meshLodType = draw_list::k_SeamHigherX}), false);
}

TEST(LandClip, seamTrianglesAcrossASideAreClipped)
{
	// as in aTriangleAcrossTheRightSideKeepsItsInsidePiece: only the points at x = 0 are inside. With the first row
	// left out, every square left is outside the right side, and only the seam triangles along x = 0 keep a piece
	const auto camera = Camera(Facing(275.0f));
	const auto cells = WaterCells();
	EXPECT_EQ(Keeps(cells, camera, true), false);
	EXPECT_EQ(Keeps(cells, camera, true, {.meshLodType = draw_list::k_SeamLowerX}), true);
	EXPECT_EQ(Keeps(FlatCells(), camera, true, {.meshLodType = draw_list::k_SeamLowerX}), true);
	EXPECT_EQ(Keeps(cells, camera, true, {.meshLodType = draw_list::k_SeamHigherX}), false);
}

TEST(LandClip, theFlagsAreReadThroughARecordThatIgnoresTheStep)
{
	// seen along the diagonal, the last square keeps a triangle only when the flags it reads say it is not split.
	// Every cell is split but one
	const auto camera = Camera(AlongDiagonal());
	const auto unsplitAt = [](size_t record) {
		auto cells = LastPointRaised(true);
		cells.at(record).properties.split = 0;
		return cells;
	};
	EXPECT_EQ(Keeps(LastPointRaised(false), camera, false), true);
	EXPECT_EQ(Keeps(LastPointRaised(true), camera, false), false);
	// step 0: the last square (row 15, column 15) reads its own cell
	EXPECT_EQ(Keeps(unsplitAt(270), camera, false), true);
	// step 1: the last square (row 7, column 7) reads record 9 x 7 + 7, not its cell's 14 x 17 + 14
	const draw_list::BlockLod half {.meshLod = 1, .meshBlending = 2};
	EXPECT_EQ(Keeps(unsplitAt(70), camera, false, half), true);
	EXPECT_EQ(Keeps(unsplitAt(252), camera, false, half), false);
	// step 2: row 3, column 3 reads record 5 x 3 + 3, not 12 x 17 + 12
	const draw_list::BlockLod quarter {.meshLod = 2, .meshBlending = 4};
	EXPECT_EQ(Keeps(unsplitAt(18), camera, false, quarter), true);
	EXPECT_EQ(Keeps(unsplitAt(216), camera, false, quarter), false);
	// the same through the sides' tests
	EXPECT_EQ(Keeps(unsplitAt(70), camera, true, half), true);
	EXPECT_EQ(Keeps(unsplitAt(252), camera, true, half), false);
}

TEST(LandClip, aLeftOutEdgeMovesTheFlagsRecord)
{
	const auto camera = Camera(AlongDiagonal());
	// the first column left out: 15 squares a row, so the record falls one behind each row: the last square (row 15,
	// column 15) reads record 15 x 16 + 14
	{
		const draw_list::BlockLod lod {.meshLodType = draw_list::k_SeamLowerZ};
		auto cells = LastPointRaised(true);
		cells[254].properties.split = 0;
		EXPECT_EQ(Keeps(cells, camera, false, lod), true);
		cells = LastPointRaised(true);
		cells[270].properties.split = 0;
		EXPECT_EQ(Keeps(cells, camera, false, lod), false);
	}
	// the first row left out: the record is a row behind, so the last square reads record 14 x 17 + 15. The open-water
	// flag is read the same way
	{
		const draw_list::BlockLod lod {.meshLodType = draw_list::k_SeamLowerX};
		const auto drawnOnlyAt = [](size_t record) {
			auto cells = LastPointRaised(false);
			for (auto& cell : cells)
			{
				cell.flags = 0x02;
			}
			cells.at(record).flags = 0;
			return cells;
		};
		EXPECT_EQ(Keeps(drawnOnlyAt(253), camera, false, lod), true);
		EXPECT_EQ(Keeps(drawnOnlyAt(270), camera, false, lod), false);
		// with no seam the last square reads its own record
		EXPECT_EQ(Keeps(drawnOnlyAt(270), camera, false), true);
	}
}

TEST(LandClip, Land1BlocksGiveAnAnswerAtEveryLevelOfDetail)
{
	const auto path = Land1Path();
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "no Land1.lnd at " << path.string();
	}
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(path), lnd::LNDResult::Success);
	const auto& blocks = file.GetBlocks();
	ASSERT_FALSE(blocks.empty());
	const draw_list::LodLines lines {.inner = LineAcrossX(80.0f, 550.0f), .outer = LineAcrossX(80.0f, 550.0f)};
	for (const auto& block : blocks)
	{
		const glm::vec2 blockMapPos {block.mapX, block.mapZ};
		// the camera faces the block's own corner
		const auto camera = Camera(
		    Matrix({1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -80.0f - block.mapX, -80.0f - block.mapZ, 200.0f}));
		const auto moved = [&](draw_list::LodLine line) {
			line.point.x += block.mapX;
			return line;
		};
		const draw_list::LodLines blockLines {.inner = moved(lines.inner), .outer = moved(lines.outer)};
		for (const draw_list::BlockLod lod :
		     {draw_list::BlockLod {.meshLod = 0, .meshBlending = 0}, draw_list::BlockLod {.meshLod = 0, .meshBlending = 1},
		      draw_list::BlockLod {.meshLod = 1, .meshBlending = 2}, draw_list::BlockLod {.meshLod = 1, .meshBlending = 3},
		      draw_list::BlockLod {.meshLod = 2, .meshBlending = 4}})
		{
			for (uint32_t bits = 0; bits < 16; ++bits)
			{
				auto withSeams = lod;
				withSeams.meshLodType = bits;
				EXPECT_TRUE(Keeps(block.cells, camera, true, withSeams, blockMapPos, blockLines).has_value()) << block.index;
			}
		}
		// the blend past the band takes the coarser grid's middles on real altitudes too
		for (const bool towardsHalfGrid : {true, false})
		{
			auto blended = block.cells;
			draw_list::BlendTowardsCoarserGrid(blended, blockMapPos, k_FarLine, towardsHalfGrid);
			const size_t step = towardsHalfGrid ? 2 : 4;
			for (size_t i = 0; i < blended.size(); ++i)
			{
				ASSERT_EQ(blended[i].altitude, FarBlend(block.cells, i / 17, i % 17, step)) << block.index << " " << i;
			}
		}
	}
}

TEST(LandClip, aResultRaisesTheLandFlagOnlyWhenItChanges)
{
	// after a land load bit 1 is 0: the first triangle kept raises the flag, and bit 2 is left alone
	draw_list::BlockState block {.visibility = draw_list::k_InViewBit | draw_list::k_InViewSecondBit};
	EXPECT_TRUE(draw_list::TakeLandClipResult(block, true));
	EXPECT_EQ(block.visibility, draw_list::k_InViewBit | draw_list::k_LandClipBit | draw_list::k_InViewSecondBit);
	// the same result again does not
	EXPECT_FALSE(draw_list::TakeLandClipResult(block, true));
	EXPECT_EQ(block.visibility, draw_list::k_InViewBit | draw_list::k_LandClipBit | draw_list::k_InViewSecondBit);
	// none kept: bit 0 takes it as well, and the change raises the flag
	EXPECT_TRUE(draw_list::TakeLandClipResult(block, false));
	EXPECT_EQ(block.visibility, draw_list::k_InViewSecondBit);
	// none again, with bit 0 set again by the next cull: bit 0 is cleared, bit 1 already holds it, so no flag
	block.visibility |= draw_list::k_InViewBit;
	EXPECT_FALSE(draw_list::TakeLandClipResult(block, false));
	EXPECT_EQ(block.visibility, draw_list::k_InViewSecondBit);
	// nothing else of the block changes
	block = {.distance = 12.5f, .visibility = draw_list::k_InViewBit, .partlyOutside = true, .lod = {1, 3, 8}};
	EXPECT_TRUE(draw_list::TakeLandClipResult(block, true));
	EXPECT_EQ(block.distance, 12.5f);
	EXPECT_TRUE(block.partlyOutside);
	EXPECT_EQ(block.lod.meshLod, 1u);
	EXPECT_EQ(block.lod.meshBlending, 3u);
	EXPECT_EQ(block.lod.meshLodType, 8u);
}
