/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/MapCoords.h"

/// The original's GUtils angle family (the Utils unit 0x74D0C0..0x74E2D0): LHArcTan on its table, the COS / SIN tables,
/// the conversions, the routines that turn an angle into a position, and the difference / direction of two angles.
///
/// - A game angle is an 11-bit integer, 2048 to the circle, kept as a u16 (MobileWallHug +0x5C): 0 = +x, 0x200 = +z,
///   0x400 = -x, 0x600 = -z. It is atan2(dz, dx) in 2048ths, through the arctangent table (at most 2.27 steps off).
/// - A 3D angle is float radians, the same way round (0 = +x, growing towards +z), in [0, 2 pi) out of GUtils.
/// - A "Scawen" angle is the 3D angle + pi / 2 (the creature, PBall, Dove::Dying, GetFacingDirection).
///
/// No game-angle routine uses the FPU but the conversions: LHArcTan is integer only (`shl 8; div`). The FPU runs at 24
/// bits (fn_007DEE00, see GUtilsDistance.h), but fsin / fcos are not rounded by the precision control: their result is
/// extended and is rounded to a float once, by the `fmul` after them. So the routines below that call fcos / fsin take
/// the cosine in double and round the product once; everything else is float or integer, no FMA.
///
/// Not ported (no caller in the original): 0x74D2A0 (the angle between two LHPoints) and 0x74D770 (turn towards a
/// target by a maximum step); 0x74D480 (the "octagonal" unit step) has one caller, fn_005E1890, not ported either.
/// Not this family: LH3DMath::GetYAngle 0x841290 and fn_007FAA50 (LH3D), Atan2Positive 0x7DB770 (gestures),
/// PuzzleGame's own conversion 0x6F184C.
namespace openblack::gutils
{

/// 2048 game angles to the circle (0x800)
constexpr int32_t k_GameAngleCircle = 0x800;
/// The mask of LHArcTan 0x74D1E2, ConvertAngle3DToGame 0x74DC3F and ConvertGameAngleTo3D 0x74DC57
constexpr int32_t k_GameAngleMask = 0x7FF;
/// [0x99A1CC] = 0x3B490FDB = 2 pi (float) / 2048, ConvertGameAngleTo3D 0x74DC6C
constexpr float k_GameAngleTo3D = 0.0030679617f;
/// [0x99A1C8] = 0x43A2F983 = 2048 / 2 pi, ConvertAngle3DToGame 0x74DC34
constexpr float k_Angle3DToGame = 325.94931f;
/// [0x8C78DC] = 0x3AC90FDB = pi (float) / 2048, ConvertGameAngleToScawenAngle 0x74E2C3
constexpr float k_HalfGameAngleTo3D = 0.0015339808f;
/// [0x8C78D8] = 0x3FC90FDB = pi / 2, the Scawen offset (0x74E295, 0x74E2C9)
constexpr float k_ScawenOffset = 1.5707964f;

/// The arctangent table 0xC2307C: 257 u16, T[i] = trunc(atan(i / 256) x 1024 / pi), 0..256 (static data in the exe;
/// built here in double, which gives every entry: 0 differences with trunc, 122 with round). Only LHArcTan reads it
[[nodiscard]] const std::array<uint16_t, 257>& ArcTanTable();
/// The SIN table 0xC31614: 2560 i32 (a circle and a quarter), S[i] = trunc(65536 sin(i 2 pi / 2048)) (static data;
/// built here in double, which gives every entry). COS 0xC31E14 is the same table 512 entries on: C[i] = S[i + 512]
[[nodiscard]] const std::array<int32_t, 2560>& SinTable();
/// [0xC31E14 + 4 a]: 65536 cos. The original indexes with a & 0xFFFF (its callers pass 0..0x7FF; 2048 and up read past
/// the table); here a & 0x7FF (inferido: the same for every value the game passes)
[[nodiscard]] int32_t Cos(uint16_t angle);
/// [0xC31614 + 4 a]: 65536 sin, a & 0x7FF as Cos
[[nodiscard]] int32_t Sin(uint16_t angle);

/// LHArcTan(dx, dz) 0x74D0C0 (cdecl, the u16 in ax): x = -dx, z = dz; 0 when both are 0; then the octant rules on
/// t(n, d) = T[(uint32)(n << 8) / d] (`shl 8; div`, unsigned), the compares signed (`jl`), a tie |dx| == |dz| to the
/// first branch, and & 0x7FF (0x74D1E2). `n << 8` keeps the low 32 bits, as the `shl` does
[[nodiscard]] uint16_t LHArcTan(int32_t dx, int32_t dz);
/// GUtils::GetAngleFromDXDZ 0x74D200: LHArcTan(dx, dz) & 0xFFFF
[[nodiscard]] uint16_t GetAngleFromDXDZ(int32_t dx, int32_t dz);
/// GUtils::GetAngleFromXZ(const MapCoords& from, const MapCoords& to) 0x74D240: GetAngleFromDXDZ(to.x - from.x,
/// to.z - from.z). 0x74D220 is the same with the four integers (from.x, from.z, to.x, to.z)
[[nodiscard]] uint16_t GetAngleFromXZ(const ecs::map_coords::MapCoords& from, const ecs::map_coords::MapCoords& to);
/// The same on two MapCoords kept as an ivec2 (x, z)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::ivec2 from, glm::ivec2 to);
/// The same on two (x, z) points in metres: each one a MapCoords first (map_coords::FromMetres), then the difference.
/// Not the angle of the metre difference: ToFixed(b) - ToFixed(a) may be one unit off ToFixed(b - a)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::vec2 from, glm::vec2 to);
/// GUtils::Get3DAngleFromXZ 0x74D270: ConvertGameAngleTo3D(GetAngleFromDXDZ(to - from)). NOT atan2 in float: the
/// angle is quantised to 2048 steps, with the table's error
[[nodiscard]] float Get3DAngleFromXZ(const ecs::map_coords::MapCoords& from, const ecs::map_coords::MapCoords& to);
[[nodiscard]] float Get3DAngleFromXZ(glm::ivec2 from, glm::ivec2 to);
[[nodiscard]] float Get3DAngleFromXZ(glm::vec2 from, glm::vec2 to);

/// GUtils::ConvertAngle3DToGame 0x74DC30: ftol(r x 325.94931 [0x99A1C8]) & 0x7FF. Truncated towards 0, and a negative
/// value wraps through the mask (-0.5 rad -> -162 -> 1886)
[[nodiscard]] uint32_t ConvertAngle3DToGame(float radians);
/// GUtils::ConvertGameAngleTo3D 0x74DC50: (a & 0x7FF) (fild qword, exact) x 0.0030679617 [0x99A1CC], one rounding.
/// Bit for bit float(a) x 2 pi (float) / 2048
[[nodiscard]] float ConvertGameAngleTo3D(int32_t angle);
/// GUtils::ConvertScawenAngleToGameAngle 0x74E290: ConvertAngle3DToGame(float(r - pi / 2 [0x8C78D8]))
[[nodiscard]] uint32_t ConvertScawenAngleToGameAngle(float radians);
/// GUtils::ConvertGameAngleToScawenAngle 0x74E2B0: float(float((a & 0xFFFF) << 1) x 0.0015339808 [0x8C78DC]) + pi / 2
/// [0x8C78D8] (no & 0x7FF)
[[nodiscard]] float ConvertGameAngleToScawenAngle(uint16_t angle);

/// fn_0074D320 / fn_0074D340: (COS[a] x d) >> 16 / (SIN[a] x d) >> 16 (32-bit `imul`, `sar`: towards -infinity). Only
/// 0x74D480 calls them
[[nodiscard]] int32_t GetXFromAngle(uint16_t angle, int32_t distance);
[[nodiscard]] int32_t GetZFromAngle(uint16_t angle, int32_t distance);
/// fn_0074D360 / fn_0074D380: float(float(COS[a]) x d) x 2^-16 [0x8AC41C] (fild exact, two products)
[[nodiscard]] float GetXFromAngle(uint16_t angle, float distance);
[[nodiscard]] float GetZFromAngle(uint16_t angle, float distance);
/// fn_0074D3A0 / fn_0074D3C0: ((whole >> 4) x COS[a]) >> 12 and the same with SIN, both shifts arithmetic (`sar`):
/// the MobileWallHug step (InitStepsXZ 0x60BFD2 inline). (x, z)
[[nodiscard]] glm::ivec2 StepFromAngle(uint16_t angle, int32_t whole);
/// fn_0074D3E0 / fn_0074D400: ((whole >> 8) x COS[a]) >> 8, the same with SIN (`sar`). (x, z)
[[nodiscard]] glm::ivec2 StepFromAngle8(uint16_t angle, int32_t whole);
/// GUtils::GetXByAngleMetersDistance 0x74D420 / GetZByAngleMetersDistance 0x74D450: ftol(float(COS[a]) x float(m /
/// 10 [0x99A1BC]))
[[nodiscard]] int32_t GetXByAngleMetersDistance(uint16_t angle, float metres);
[[nodiscard]] int32_t GetZByAngleMetersDistance(uint16_t angle, float metres);
/// fn_0074D650 (angle, whole): {StepFromAngle(a, whole).x, .z, 0}
[[nodiscard]] ecs::map_coords::MapCoords GetPosFromGameAngle(uint16_t angle, int32_t whole);
/// fn_0074D6A0 (angle, float metres): the same with whole = ConvertMetersToWholeDistance(m) (0x74DC80: ftol(m / 10 x
/// 65536)). The `sar 4` drops the low 4 bits of the distance
[[nodiscard]] ecs::map_coords::MapCoords GetPosFromGameAngle(uint16_t angle, float metres);

/// GUtils::GetPosFromAngle(float a, float m) 0x74D580: x = ftol(float(cos(a) m) x 65536 [0x8AC408] / 10 [0x99A1BC]),
/// z the same with sin, altitude 0 (ftol(0 / 10)). fcos / fsin are extended and the `fmul m` rounds once, so the
/// cosine is taken in double here (std::cos in float rounds twice: 0.19 % of the values one unit off). The
/// GetDistanceInMetres(0, p) it calls on the way (0x74D5F4) is thrown away
[[nodiscard]] ecs::map_coords::MapCoords GetPosFromAngle(float radians, float metres);
/// GUtils::AddDistanceFromAngle(MapCoords* p, float a, float m) 0x74D510: p.x = ftol((float(cos(a) m) +
/// float(p.x x 10) x 2^-16) x 65536 / 10), the same on z with sin; the altitude stays. The cosine in double as
/// GetPosFromAngle
void AddDistanceFromAngle(ecs::map_coords::MapCoords& pos, float radians, float metres);
/// fn_0074D620 (LHPoint* out, float a, float m): (float(cos(a) m), 0, float(sin(a) m)), the cosine in double
[[nodiscard]] glm::vec3 GetLHPointFromAngle(float radians, float metres);

/// fn_0074D740 (nombre inferido GetAngleDifference): d = |a - b| (integers, no mask), 0x800 - d when d > 0x400
/// (unsigned `jbe`), so 0..0x400 for two angles in 0..0x7FF
[[nodiscard]] uint32_t GetAngleDifference(int32_t a, int32_t b);
/// fn_0074D6F0 (nombre inferido GetAngleDirection): d = to - from; 0 -> 0; when |d| > 0x400 (unsigned `jbe`) d wraps
/// by 0x800 towards 0; then -1 if d < 0, else +1. With |d| == 0x400 it does not wrap: +0x400 gives +1, -0x400 gives -1
[[nodiscard]] int32_t GetAngleDirection(int32_t from, int32_t to);

} // namespace openblack::gutils
