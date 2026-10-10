/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "ECS/DrawList/BlockCull.h"

/// The object draw list's land clip: whether a visible land block leaves at least one front-facing land triangle on the
/// screen once its triangles are clipped to the view. The land pass works it out once per drawn frame for every visible
/// block, and a block whose result changes from the last frame it was visible forces the list to be rebuilt. Pure maths
/// on values, no Locator. Every float operation keeps the original's order, one float rounding each, as
/// src/3D/AffineMatrix.h does. docs/bw1-notes/original-frame.md, "Stage A".
///
/// The original has two forms of this code and picks one by the CPU: the arithmetic here is the form it runs on CPUs
/// without its SSE path (every Intel Core). The SSE form makes the same tests with its own float operations.
///
/// Every level of detail the block cull chooses (BlockCull.h) is ported: the full, half and quarter grids, the blend of
/// a block's copy towards the coarser grid while the block is in a line's band or across it, and the seam triangles
/// towards a coarser neighbour.
namespace openblack::ecs::draw_list
{

/// Points along a land block's side: one per cell corner, the next block's first included
constexpr size_t k_BlockSide = 17;
/// The cell records of a land block, row after row, the rows along x and the cells of a row along z. Cell
/// `row * 17 + column` sits at (x + 10 row, z + 10 column) from the block's map position. Its corner point takes the
/// cell's altitude
constexpr size_t k_BlockCells = k_BlockSide * k_BlockSide;

/// The sides of a block a set of seam bits leaves out, and whether it adds seam triangles there
struct SeamEdges
{
	/// The seam triangles of SeamFan are added
	bool fans {};
	/// The first row of grid squares (the neighbour at block x - 1) is left out
	bool skipFirstRow {};
	/// The last row (the neighbour at block x + 1)
	bool skipLastRow {};
	/// The first column of grid squares (the neighbour at block z - 1)
	bool skipFirstColumn {};
	/// The last column (the neighbour at block z + 1)
	bool skipLastColumn {};
};

/// The edges for seam bits 0..15, from the original's fixed table. Only a coarser neighbour on one side, or on two sides
/// that meet at a corner, has an entry: 1, 2, 4 and 8 alone, and 3, 5, 10 and 12. Every other set, including both
/// opposite sides, leaves out nothing and adds nothing. Empty past 15, which no seam pass makes
[[nodiscard]] std::optional<SeamEdges> SeamEdgesFor(uint32_t seamBits);

/// One seam triangle: three points of the block's grid, in the order they are emitted
using SeamTriangle = std::array<uint16_t, 3>;

/// The seam triangles a block adds, as points of its grid (row x points a side + column), for step 0 (17 points a
/// side) or step 1 (9). Empty when the seam bits have no fans (SeamEdgesFor) and at step 2, which never gets seams.
/// The set is chosen by the lower x bit first, then the higher x bit, and within each the higher z bit before the
/// lower z one: 24 triangles along one side and 46 along two at step 0, 12 and 22 at step 1
[[nodiscard]] std::span<const SeamTriangle> SeamFan(uint32_t meshLod, uint32_t seamBits);

/// The blend of a block's copy towards the coarser grid, made on the copy before the land clip for blending 1 (towards
/// the half grid against the inner line, `towardsHalfGrid`) and 3 (towards the quarter grid against the outer line).
/// Only altitudes change. With s the coarser step (2 or 4) and h = s / 2, three passes each blend the points between
/// two points of the coarser grid, which no pass writes, so the passes do not depend on each other:
///
/// - rows 0, s, .. 16 and columns h, h + s, ..: towards the points h columns either side;
/// - rows h, h + s, .. and columns h, h + s, ..: towards the points h rows and h columns back and h rows and h columns
///   on, the same diagonal whatever the cell's split bit;
/// - rows h, h + s, .. and columns 0, s, .. 16: towards the points h rows either side.
///
/// Each pass steps its x and z on from its first point, s x 10 at a time: x from (x - point.x) in the first pass and
/// ((h x 10 + x) - point.x) in the others, z from ((h x 10 + z) - point.z) in the first two passes and (z - point.z)
/// in the third. With v = (x along.z) - (z along.x) and m the two points' altitudes added and halved (rounded down), a
/// point with v at or past half the band's width on the camera's side is left as it is, one with v at or past it on
/// the far side takes m, and one in between takes ((256 - t) a + t m) / 256 on its altitude a, rounded down, with
/// t = (half - v) (256 / width) rounded to the nearest integer, ties to even. A NaN v falls in between, with the
/// integer the x87 gives a NaN (-2^31), which leaves the altitude as it is
void BlendTowardsCoarserGrid(std::span<lnd::LNDCell, k_BlockCells> cells, glm::vec2 blockMapPos, const LodLine& line,
                             bool towardsHalfGrid);

/// Whether the block leaves at least one front-facing land triangle on the screen. Empty for a step past 2, for seam
/// bits past 15 at steps 0 and 1, and while the camera has no screen clamp (DrawCamera::maxScreen).
///
/// - Blending 1 and 3 first blend a copy of the block (BlendTowardsCoarserGrid), towards the half grid against the
///   inner line for 1 and towards the quarter grid against the outer line for 3. Everything after reads the copy.
/// - The grid has a point every 1 << meshLod cells: 17, 9 or 5 a side, each at (x, h, z) with x and z stepping on from
///   the block's map position by 10, 20 or 40, and h = altitude x 0.67 for an altitude above 3 and 0 otherwise. It
///   goes through the world-to-clipping matrix to (X, Y, d).
/// - When the block is partly outside the view (BlockCull::partlyOutside), each point gets the sides it is outside of:
///   near when d is not at or beyond the near clip (a NaN depth counts as near); right when X > d, and only otherwise
///   left when -d > X; top when Y > d, and only otherwise bottom when -d > Y. A fully inside block gets none.
/// - A point outside no side goes to the screen: with r = 1 / d, sx = (r X + 1) half.x and sy = half.y - (r Y) half.y.
///   Each is then clamped: below 0, or NaN, to 0, and above maxScreen to maxScreen.
/// - At steps 0 and 1 the seam bits (SeamEdgesFor) leave out the first or last row or column of grid squares; step 2
///   ignores them. Each square left, rows along x and squares along z, makes two triangles, unless it is flagged as
///   undrawn open water while the blending is 0. The square with corners a = (x, z), b = (x, z + d), c = (x + d, z)
///   and its fourth corner is cut from b to c when it is split, and from a to its fourth corner otherwise.
/// - The split and open-water flags do not come from the square's own cell. They are read from a cell record that
///   starts at the block's first one and moves on one record a square and one more at each row's end, whatever the
///   step and the seams: at step 1 the square in row r and column c reads record 9 r + c, at step 2 record 5 r + c,
///   and a left-out first row or column moves every read back by a row, or by one more record each row. The original
///   does this.
/// - After the squares come the seam triangles (SeamFan), which no flag leaves out.
/// - When none of a square's 4 corners (a seam triangle's 3 points) is outside, a triangle counts when its screen area
///   is above 0: for the points P0, P1 and P2 in the order they are emitted,
///   (P2.y - P0.y) (P1.x - P0.x) - (P1.y - P0.y) (P2.x - P0.x) > 0, strictly, so 0 and NaN fail.
/// - Otherwise a triangle whose 3 points are all outside one side is dropped. Any other is clipped side by side (near,
///   right, left, top, bottom): each cut point is worked out between a point inside that side and one outside it, and
///   goes to the screen the same way when it is outside no side left to cut. Each final piece gets the same area test.
[[nodiscard]] std::optional<bool> KeepsFrontTriangle(std::span<const lnd::LNDCell, k_BlockCells> cells, glm::vec2 blockMapPos,
                                                     const DrawCamera& camera, bool partlyOutside, const BlockLod& lod,
                                                     const LodLines& lines);

/// The land pass's write for one visible block once its land clip is known: bit 0 (k_InViewBit) takes the result,
/// and when the result is not bit 1 (k_LandClipBit), the result the block had the last frame it was visible, bit 1
/// takes it too. Returns whether bit 1 changed, which raises the land flag that forces the list's rebuild
[[nodiscard]] bool TakeLandClipResult(BlockState& block, bool keepsFrontTriangle);

} // namespace openblack::ecs::draw_list
