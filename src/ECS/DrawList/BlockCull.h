/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

/// The object draw list's first stage: which land blocks the camera can see this frame, how far each one is, and the
/// visible ones sorted nearest first. Pure maths on values, no Locator. Every float operation keeps the original's
/// order, one float rounding each, as src/3D/AffineMatrix.h does. docs/bw1-notes/original-frame.md, "Stage A".
///
/// The cull also chooses each block's level of detail: two lines across the land, rebuilt every frame from the camera,
/// split it into full detail near the camera, half and quarter detail further away, with a band around each line where
/// a block is blended towards the coarser grid. Once every block is culled, the seam pass marks each block's sides
/// that face a coarser neighbour. Not ported, because none of it changes which land triangles survive: the fog band
/// each block is given, and the two other lines that only choose the land's detail texture and its bump map.
namespace openblack::ecs::draw_list
{

/// Half a land block's side, in world units
constexpr float k_BlockHalfSide = 80.0f;
/// A block's height in world units per unit of its highest altitude
constexpr float k_BlockHeightScale = 0.67f;
/// A land block's side, in world units
constexpr float k_BlockSideLength = 160.0f;
/// Land blocks along each side of the map's block grid
constexpr int32_t k_BlocksPerSide = 32;
/// The block grid's lookup: entry x * 32 + z
constexpr size_t k_BlockLookupSize = static_cast<size_t>(k_BlocksPerSide) * static_cast<size_t>(k_BlocksPerSide);

/// Bit 0 of a block's visibility: the block passed this frame's cull, so the sort takes it
constexpr uint32_t k_InViewBit = 1u;
/// Bit 1: the land clip's last result for the block. The land pass writes it; the cull keeps it
constexpr uint32_t k_LandClipBit = 2u;
/// Bit 2: set and cleared together with bit 0. What reads it is not known
constexpr uint32_t k_InViewSecondBit = 4u;

/// The camera of one drawn frame, as the draw list reads it
struct DrawCamera
{
	/// The drawn eye and its focus
	glm::vec3 eye {};
	glm::vec3 focus {};
	/// The unit view direction the level-of-detail lines are rebuilt from (RebuildLodLines): the world-to-camera
	/// matrix's third column, that is the focus less the eye, normalised with that matrix's guards
	glm::vec3 viewDirection {};
	/// A world point through it gives (X, Y, Z), Z the camera depth
	affine::AffineMatrix worldToClipping;
	/// The near clip the cull and the on-screen test compare the depth with (NearClipFor)
	float nearW {};
	/// Half the viewport's width and height, the projection's scale
	glm::vec2 half {};
	/// The largest screen point the land clip keeps: the viewport's width and height less one, as floats. While it is
	/// empty the land clip gives no answer
	std::optional<glm::vec2> maxScreen;
	/// The land is drawn with its reflection: the blocks' boxes reach as far below 0 as above it
	bool landReflection {};
};

/// A land block's level of detail, as its block record keeps it (meshLOD, meshBlending and meshLODType)
struct BlockLod
{
	/// The grid's step is 1 << meshLod cells: 0, 1 or 2
	uint32_t meshLod {};
	/// 0: full detail. 1: full detail blended towards the half grid (the block is in the inner line's band, or crosses
	/// it). 2: half detail. 3: half detail blended towards the quarter grid (the outer line's band, or across it). 4:
	/// quarter detail, and the value a culled block is given. While it is 0, the cells flagged as undrawn open water are
	/// left out. For 1 and 3 the original first reshapes a copy of the block
	uint32_t meshBlending {};
	/// The seam bits (k_SeamLowerX and the others): the sides whose neighbour has a coarser grid. They leave out the
	/// cells along some of the block's sides and add seam triangles there
	uint32_t meshLodType {};
};

/// Seam bit: the neighbour at block x - 1 has a coarser grid
constexpr uint32_t k_SeamLowerX = 1u;
/// Seam bit: the neighbour at block z - 1
constexpr uint32_t k_SeamLowerZ = 2u;
/// Seam bit: the neighbour at block z + 1
constexpr uint32_t k_SeamHigherZ = 4u;
/// Seam bit: the neighbour at block x + 1
constexpr uint32_t k_SeamHigherX = 8u;

/// A culled block's step and blending. Its seam bits are left as they were
constexpr uint32_t k_CulledMeshLod = 0u;
constexpr uint32_t k_CulledMeshBlending = 4u;

/// One of the two lines across the land that a block's level of detail is chosen against. Each frame it is placed
/// across the view, `distance` ahead of the camera along the view direction, where the plane facing the view at that
/// distance meets the ground plane y = 0
struct LodLine
{
	/// The line's horizontal direction, (view.z, 0, -view.x), unit length once the line is found
	glm::vec3 along {1.0f, 0.0f, 0.0f};
	/// A point on the line
	glm::vec3 point {};
	/// How far ahead of the camera, along the view direction, the line is placed
	float distance {};
	/// The full width of the band around the line, over which the blend of a block's copy ramps
	float width {};
	/// Half the band's width: a corner within it of the line (either side, ends included) is in the band
	float halfWidth {};
	// whether the last rebuild found the line is not kept: nothing reads it
};

/// The two lines: inside the inner one is full detail, between them half detail, beyond the outer one quarter detail
struct LodLines
{
	LodLine inner;
	LodLine outer;
};

/// Where a point is against a line: on the camera's side beyond the band, in the band, or past it
enum class LineSide : uint8_t
{
	Near,
	Band,
	Far,
};

/// What the cull found for one block's box
struct BlockCull
{
	/// No single side has all 8 corners outside it
	bool visible {};
	/// Visible, with at least one corner outside a side. False when not visible
	bool partlyOutside {};
};

/// What the draw list keeps per land block from frame to frame
struct BlockState
{
	/// The distance from the eye to the box's centre at the last frame the block was visible
	float distance {};
	/// k_InViewBit, k_LandClipBit and k_InViewSecondBit
	uint32_t visibility {};
	/// The last visible cull found a corner outside a side
	bool partlyOutside {};
	/// The level of detail. Every frame's cull and seam pass rewrite all three fields before anything reads them, so
	/// their start value does not matter
	BlockLod lod {};
};

/// The cull of a block's box, from its 8 corners (graphics::haze::BlockCorners). Each corner goes through the matrix
/// (X, Y and the depth d) and sets its bit in up to three masks: near when d is not at or beyond the near clip (so a
/// NaN depth counts as near); right when X > d, and only otherwise left when -d > X; top when Y > d, and only
/// otherwise bottom when -d > Y. There is no far side. The block is culled when one mask holds all 8 corners. The
/// corners' order does not change the result
[[nodiscard]] BlockCull CullBlock(const std::array<glm::vec3, 8>& corners, const affine::AffineMatrix& worldToClipping,
                                  float nearW);

/// The distance from the eye to a block's box centre: (x + 80, 0 with the land reflection or else h / 2, z + 80), with
/// h = highest altitude x 0.67 and (x, z) the block's map position. sqrt((dz dz + dy dy) + dx dx), not squared
[[nodiscard]] float BlockDistance(glm::vec2 blockMapPos, int32_t highestAltitude, bool landReflection, glm::vec3 eye);

/// One frame's cull of one block. A visible block sets bits 0 and 2, takes this frame's partly-outside result, its
/// new distance, and its step and blending against this frame's lines (ChooseBlockLod). A culled block clears bits 0
/// and 2, takes step 0 and blending 4 and keeps everything else, its old distance and bit 1 included. Either keeps its
/// seam bits, which the seam pass rewrites once every block is culled (MarkSeams)
[[nodiscard]] BlockState NextBlockState(const BlockState& previous, glm::vec2 blockMapPos, int32_t highestAltitude,
                                        const DrawCamera& camera, const LodLines& lines);

/// The lines before any land is created: the inner one 300 ahead and the outer one 600 ahead, each band 50 wide (25
/// each side), `along` (1, 0, 0) and the point 0
constexpr LodLines k_StartLodLines {
    .inner = {.distance = 300.0f, .width = 50.0f, .halfWidth = 25.0f},
    .outer = {.distance = 600.0f, .width = 50.0f, .halfWidth = 25.0f},
};

/// The lines once a land is created, from the detail index 0..4 (the original's start-up turns a missing or larger
/// index into 4). At index 4 both are set afresh: the inner line 1600 ahead and the outer one 2500 ahead, each band
/// 550 wide (275 each side), `along` (1, 0, 0) and the point 0. At any other index the land's creation leaves them as
/// they are. openblack's detail levels 5 and 6 have no counterpart in the original
[[nodiscard]] LodLines LodLinesAtLandCreation(const LodLines& current, int32_t detailIndex);

/// The line rebuilt for this frame from the eye and the unit view direction f, once per frame before any block is
/// culled. P = (eye.x + distance f.x, eye.y + distance f.y, distance f.z + eye.z), the plane's offset
/// d = -((P.z f.z + P.y f.y) + P.x f.x) and the direction n = up x f = (f.z 1 - 0 f.y, 0 f.x - 0 f.z, 0 f.y - f.x 1),
/// with its squares sx, sy and sz. When sz > sx, sz > sy and sz > 0.0001, strictly, the point is (d / n.z, 0, 0) (the
/// reciprocal first, then times each component); otherwise, when sx > 0.0001, it is (0, 0, -d / n.x). Then n is
/// scaled by 1 / sqrt((sz + sy) + sx). When neither holds (the view within about 0.01 of straight down), the direction
/// is left as the unscaled n and the point stays last frame's
[[nodiscard]] LodLine RebuildLodLine(const LodLine& previous, glm::vec3 eye, glm::vec3 viewDirection);

/// Both lines rebuilt for this frame (RebuildLodLine)
[[nodiscard]] LodLines RebuildLodLines(const LodLines& previous, glm::vec3 eye, glm::vec3 viewDirection);

/// Where the point (x, z) is against the line, from v = (x - point.x) along.z - (z - point.z) along.x, positive on the
/// camera's side: near when v > halfWidth, far when v < -halfWidth or v is NaN, in the band otherwise
[[nodiscard]] LineSide SideOfLine(const LodLine& line, float x, float z);

/// A visible block's step and blending from where its 4 ground corners, (x, z), (x, z + 160), (x + 160, z) and
/// (x + 160, z + 160), are against the lines. The inner line first: any corner in its band, or corners on both sides,
/// give (blending 1, step 0); all near give (0, 0); all far give (2, 1) and the outer line decides: any corner in its
/// band, or on both sides, give (3, 1); all far give (4, 2); all near keep (2, 1). The seam bits come back 0
[[nodiscard]] BlockLod ChooseBlockLod(glm::vec2 blockMapPos, const LodLines& lines);

/// A block's seam bits, from its four neighbours on the block grid: x - 1, z - 1, z + 1 and x + 1, in that order. A
/// neighbour counts when it is on the grid (0..31 both ways), the lookup gives it a block (indices from 1 into
/// `blocks`, 0 for none) and its step is larger than `meshLod`. A culled neighbour has step 0, so it never counts
[[nodiscard]] uint32_t SeamBits(glm::ivec2 blockCoords, uint32_t meshLod, std::span<const uint16_t, k_BlockLookupSize> lookup,
                                std::span<const BlockState> blocks);

/// The seam pass: every block's seam bits (SeamBits), once every block of the frame is culled. `blockCoords[i]` is
/// block i's place on the block grid. It reads only the steps, which it does not write, so the order does not matter
void MarkSeams(std::span<BlockState> blocks, std::span<const glm::ivec2> blockCoords,
               std::span<const uint16_t, k_BlockLookupSize> lookup);

/// A block's state before its first frame on a land, as every land load leaves it: the block is read whole from the
/// land file, then its partly-outside flag, its visibility (the land clip's last result included) and its level of
/// detail are cleared. Only the distance keeps the file's value, which nothing reads before the block's first visible
/// cull rewrites it
[[nodiscard]] BlockState InitialBlockState(float valueSorting);

/// The visible blocks (k_InViewBit), as indices into `blocks`, nearest first. Each block in index order is inserted
/// before the first one already in the list that is not nearer than it: on a tie the later block goes first, and a
/// block whose distance is NaN goes last. `out` is cleared first
void SortVisible(std::span<const BlockState> blocks, std::vector<uint16_t>& out);

/// The near clip from the camera's height over the ground: 0.1 while a script asks for close clipping; otherwise 0.3
/// at or below the ground (and for a NaN height), 3.5 above 20, and (h x 0.05) x 3.2 + 0.3 in between. The rule is
/// near_clipping::NearPlane's. The caller passes the previous frame's drawn eye's height over the ground read at the
/// quantised map point (map_coords::MetresToFixedForHandLookup)
[[nodiscard]] float NearClipFor(float heightAboveQuantisedGround, bool closeClipping);

} // namespace openblack::ecs::draw_list
