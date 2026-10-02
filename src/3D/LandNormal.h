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

#include <glm/vec3.hpp>

/// LH3DIsland::GetNormal 0x803630 (wiki: engine-math.md, "Normal del terreno"): the flat normal of the landscape
/// triangle under a MapCoords, from the raw corner heights (no sea flattening), normalised through the two tables of the
/// island's initialisation fn_00803890. LandIsland::GetNormalAt finds the cell (as GetAltitude 0x803090 does) and calls
/// OfCell. Pure math.
namespace openblack::land_normal
{

constexpr float k_HeightUnit = 0.67f; ///< [0xC3720C]

/// The table at 0xE9B2D8 (fn_00803890 0x8038E3..0x803934): T1[i] = 1 / sqrt((0.67 i)^2 + 100) for i = 0..255, each
/// step rounded to 24 bits: the inverse length of a cell edge that climbs i height units. (port guard) The altitudes
/// of BWLandEditor maps can climb more than 255 units: those are computed with the same formula
[[nodiscard]] float EdgeScale(uint32_t climb);
/// The table at 0xE9A2D8 (0x803936..0x80396F): T2[0] = 1, T2[j] = 1 / sqrt(j 0.000977517 [0x9A2BEC]) for j = 1..1023
[[nodiscard]] float LengthScale(uint32_t index);

/// The normal of a cell (0x803698..0x803871). fracX / fracZ are the 16-bit fractions of the MapCoords, `split` the
/// cell's split bit (+6 & 0x80) and h.. the corner heights in height units (h01 = z + 1, h10 = x + 1):
/// - split: B = h11 at (10, 10) when fz > 0xFFFF - fx, else h00 at (0, 0); P = h10 at (10, 0), Q = h01 at (0, 10);
///   without split: B = h10 at (10, 0) when fx > fz, else h01 at (0, 10); P = h11 at (10, 10), Q = h00 at (0, 0);
/// - P' = (Px - Bx, (hP - hB) 0.67, Pz - Bz), Q' likewise, n = (Q' x P') T1[|hP - hB|] T1[|hQ - hB|] (an almost unit
///   vector: P' and Q' are perpendicular in xz);
/// - k = fistp(((nz nz + nx nx) + ny ny) 1023 [0x9A2BE8]) (to nearest), n *= T2[k], and n = -n when n.y < 0
[[nodiscard]] glm::vec3 OfCell(uint32_t fracX, uint32_t fracZ, bool split, int32_t h00, int32_t h01, int32_t h10,
                               int32_t h11);

} // namespace openblack::land_normal
