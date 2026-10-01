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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
namespace lnd
{
struct LNDCell;
}
} // namespace openblack

/// The sea and land predicates of the landscape cells (MapCoords 0x6035B0..0x603840, MapCell::Collide 0x601BD0,
/// GSoundMap::GetSurfaceType 0x71D8E0), exactly as the original reads them, the map border included: one place for the
/// physics, the hand, the sounds and the AI instead of each module's own rule.
///
/// All read the landscape cell g_ptr_blocks[g_index_block[x >> 4][z >> 4]] + ((x & 15) * 17 + (z & 15)) * 8 with x, z
/// the high words of the 16.16 MapCoords (world / 10): a cell off 0..511 (openblack: 0..GetCellsPerSide() - 1) or
/// without a block is "no cell". Byte +4 is the altitude (openblack: GetCellAltitude, the same with 8 altitude bits),
/// byte +6 the LND properties (0x10 hasWater, 0x20 coastLine).
///
/// The overloads with a LandIslandInterface are the pure predicates (unit tests); the others use the Locator's island
/// and answer like an empty map when none is loaded (IsWater true, the rest false).
namespace openblack::ecs::sea_cells
{

/// MapCoords(LHPoint) (0x603160): the cell of a world point (ecs::map_coords::CellOf: the unsigned high words of
/// ftol(x * 6553.6f), so negative -> off the map)
[[nodiscard]] glm::ivec2 CellOf(glm::vec3 point);
/// fistp(x * 0.1), fistp(z * 0.1): the cell rounded to the nearest, as AttemptToAddSoundEvent (0x6465DF) reads it
[[nodiscard]] glm::ivec2 RoundedCellOf(glm::vec3 point);

/// The landscape cell, nullptr off the map or where no block is
[[nodiscard]] const lnd::LNDCell* CellAt(const LandIslandInterface& island, glm::ivec2 cell);
/// The cell's altitude in height units (0.67); 0 for no cell (like IsDryLand's "xor al, al")
[[nodiscard]] uint16_t AltitudeAt(const LandIslandInterface& island, glm::ivec2 cell);

/// MapCoords::InBounds (0x6042C0): the cell is inside the game map ([g_game+0x59C8] x [g_game+0x59C4] cells, unsigned
/// compare; openblack: GetCellsPerSide()), a block or not
[[nodiscard]] bool InBounds(const LandIslandInterface& island, glm::ivec2 cell);
/// MapCoords::IsWater (0x6035B0): properties & 0x10; no cell = 1
[[nodiscard]] bool IsWater(const LandIslandInterface& island, glm::ivec2 cell);
/// MapCoords::IsLand (0x603720): !(properties & 0x10); no cell = 0
[[nodiscard]] bool IsLand(const LandIslandInterface& island, glm::ivec2 cell);
/// MapCoords::IsDryLand (0x603620): altitude >= 4, the water bit is not read; no cell = 0
[[nodiscard]] bool IsDryLand(const LandIslandInterface& island, glm::ivec2 cell);
/// MapCoords::IsCoastal (0x6036A0): !(properties & 0x10) && (properties & 0x20) (a land cell on the coast line); no
/// cell = 0
[[nodiscard]] bool IsCoastal(const LandIslandInterface& island, glm::ivec2 cell);

/// MapCell::Collide bits (0x601BD0), the CollideType values
enum CollideBits : uint32_t
{
	k_CollideWater = 0x01, ///< IsWater (the water bit, or no landscape cell)
	k_CollideLand = 0x02,  ///< not water
	k_CollideField = 0x04, ///< an OBJECT_TYPE 0x12 (Field) in the map cell
	k_CollideFixed = 0x08, ///< MapCell::Collide(MapCoords) 0x601CE0: CollideWithFixed (0x601D10) when asked for
	k_CollideEdge = 0x10,  ///< the map cell is off the game map (fn_00601E00: x < [g_game+0x59C8], z < [+0x59C4])
	k_CollideTree = 0x20,  ///< an OBJECT_TYPE 6 (ForestTree) in the map cell
};
/// The landscape part of MapCell::Collide (0x601BD0): 0x10 off the game map, else 1 on water (or no landscape cell)
/// or 2. The object bits (0x04 Field, 0x20 tree, 0x08 fixed) come from the map cell's objects: callers that need them
/// add them (TODO(sea-cells): no map-cell object lists in openblack yet).
[[nodiscard]] uint32_t CollideLandscape(const LandIslandInterface& island, glm::ivec2 cell);

/// GSoundMap::GetSurfaceType (0x71D8E0): 6 (DEEP_WATER) for no cell, 7 (SHALLOW_WATER) where !IsLand, else the
/// surfaceSound of the cell's material (info.dat), 3 when it is not 1..8
[[nodiscard]] int32_t GetSurfaceType(const LandIslandInterface& island, glm::vec3 point);

/// GScript::GetLandHeight (0x6FB1F0), the script's GET_LAND_HEIGHT: the cell (int)(x * 0.1), (int)(z * 0.1) (truncated
/// towards 0, so -10 < x < 0 is still cell 0); off 0..511, without a block or at altitude 0 -> -10.0 (0xC1200000, "the
/// sea"), else LH3DIsland::GetAltitude of the point (openblack: GetHeightAt)
[[nodiscard]] float ScriptLandHeight(const LandIslandInterface& island, glm::vec3 point);

// With the Locator's island
[[nodiscard]] bool InBounds(glm::vec3 point);
[[nodiscard]] bool IsWater(glm::vec3 point);
[[nodiscard]] bool IsLand(glm::vec3 point);
[[nodiscard]] bool IsDryLand(glm::vec3 point);
[[nodiscard]] bool IsCoastal(glm::vec3 point);
[[nodiscard]] uint32_t CollideLandscape(glm::vec3 point);
[[nodiscard]] int32_t GetSurfaceType(glm::vec3 point);

} // namespace openblack::ecs::sea_cells
