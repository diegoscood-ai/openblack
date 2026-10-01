/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// The formulas of UR_Flocking (PSys/Rules/Flock.cpp), for the tests. Wiki: docs/bw1-notes/magic.md ("Bandadas").

namespace openblack::psys::flocking
{
/// fn_00683520 (distance, type, invert): x = max(distance, 0.01) x ScaleModifier; type 0 -> 1, 1 -> x, 2 -> x^2;
/// 1 / that unless inverted
[[nodiscard]] float Accn(float distance, int type, bool invert, float scaleModifier);

/// UR_Flocking::UpdateBanking 0x684160: the atom's rotation (openblack's columns = the LH rows) from its new velocity
/// and the acceleration it just took: SetAngleY(-pi / 2) x Rx(bank) x Rz(-pitch) x SetAngleY(yaw), with yaw =
/// atan2(v.z, v.x), pitch = atan2(v.y, |v.xz|) x ReducePitchBy and bank = atan((a.z v.x - a.x v.z) / |v.xz| /
/// GravityForBanking) (0 with no horizontal speed). The local +Z axis points along the velocity.
[[nodiscard]] glm::mat3 BankingRotation(const glm::vec3& velocity, const glm::vec3& acceleration, float reducePitchBy,
                                        float gravityForBanking);
} // namespace openblack::psys::flocking
