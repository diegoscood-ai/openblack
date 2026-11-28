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

#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// Where a line meets the land, as the game finds it: the line runs over the map's cells nearest first, and each cell is
/// tested as two triangles of its corners' heights. The game uses this for what is under the cursor, where the camera's
/// moves meet the land and whether the land hides the sun.
///
/// Where this differs from raffclar's signatures: the heights are 16 bits, as the land editor's maps have altitudes
/// above 255, and the walk takes the map's cells per side (512 in the game's lands, more in the editor's). Metres go to
/// cells by a float tenth and the flat-triangle threshold is the float 0.0001 widened, as the game's own code does.
namespace openblack::land_line
{

/// The height units of the land's corners, from the corner at the cell itself across x, across z and across both
struct CellHeights
{
	uint16_t here;
	uint16_t acrossX;
	uint16_t acrossZ;
	uint16_t acrossBoth;
};

/// The corner heights of the map cell at x, z, none for a cell the land doesn't have
using CellLookup = std::function<std::optional<CellHeights>(int32_t x, int32_t z)>;

/// Cells along each side of the map
inline constexpr float k_MapCells = 512.0f;
/// The line is kept this far, in cells, inside the map's edges
inline constexpr float k_EdgeMargin = 0.1f;
/// The far edge of a 512-cell map; a map of another size has its own, its cells less the margin
inline constexpr float k_FarEdge = 511.9f;
/// Metres along a cell's side, and up a height unit of the land
inline constexpr float k_CellSize = 10.0f;
inline constexpr float k_HeightUnit = 0.67f;
/// The game's lands have this many cells along each side
inline constexpr uint16_t k_CellsPerSide = 512;

/// Where a stretch of the line, in cell units (x and z in cells, y in height units), meets the land of a cell: its x and
/// z in cell units, none when it misses. The cell is the triangle on the corners at the cell, across x and across z,
/// then the one on the corners across x, across both and across z, each reaching a tenth of a cell past its edges.
/// Nothing behind the stretch's start counts.
[[nodiscard]] std::optional<glm::vec2> HitInCell(int32_t x, int32_t z, glm::vec3 from, glm::vec3 to, const CellLookup& cells,
                                                 uint16_t cellsPerSide = k_CellsPerSide);

/// Where the line from one point to another, in cell units, first meets the land, the line carried on to the map's edge:
/// its x and z in cell units
[[nodiscard]] std::optional<glm::vec2> FirstHit(glm::vec3 from, glm::vec3 to, const CellLookup& cells,
                                                uint16_t cellsPerSide = k_CellsPerSide);

/// Where the line from one point to another, in metres, meets the land: its x and z in metres
[[nodiscard]] std::optional<glm::vec2> LandAlong(glm::vec3 from, glm::vec3 to, const CellLookup& cells,
                                                 uint16_t cellsPerSide = k_CellsPerSide);

/// Where a line going down, in metres, meets the sea's level, and whether that is no further across than 7500 from the
/// camera. None for a line going up or too nearly level.
struct SeaCrossing
{
	glm::vec2 point;
	bool inReach;
};
[[nodiscard]] std::optional<SeaCrossing> SeaLevelAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera);

/// The same, or else where the line meets the sea's level when it goes down, no further across than 7500 from the camera
[[nodiscard]] std::optional<glm::vec2> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera, const CellLookup& cells,
                                                      uint16_t cellsPerSide = k_CellsPerSide);

/// The land or sea the interface takes to be under a pixel, from the camera through the pixel's point on the near plane:
/// where that line meets the land (or the sea near the camera), else, with the sea, the sea's level wherever the line
/// goes down. Its y is the land's height there (heightAt).
[[nodiscard]] std::optional<glm::vec3> UnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea, const CellLookup& cells,
                                                  const std::function<float(glm::vec2)>& heightAt,
                                                  uint16_t cellsPerSide = k_CellsPerSide);

/// A point picked under the cursor, kept within 7680 of the map's middle at (2560, 0, 2560)
[[nodiscard]] glm::vec3 KeptInReach(glm::vec3 point);

/// The interface counts the land under the cursor this much further than its depth in the view, so a thing standing on
/// it is nearer
inline constexpr float k_LandDistanceAllowance = 2.3f;

/// The corner heights of an island's cells as the line test reads them: a cell outside its blocks has none
[[nodiscard]] CellLookup CellsOf(const LandIslandInterface& island);

} // namespace openblack::land_line
