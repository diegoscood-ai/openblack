/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GUtilsAngle.h"

#include <cmath>

#include "ECS/GUtilsDistance.h"

using namespace openblack;
using openblack::ecs::map_coords::MapCoords;

namespace
{
namespace map_coords = openblack::ecs::map_coords;

/// The tables are static data in the exe; the double formulas give every entry (checked against the dump of 0xC2307C
/// and 0xC31614, see test_gutils_angle.cpp)
constexpr double k_TwoPi = 6.283185307179586476925;

/// fcos / fsin of a float angle, extended in the original: the double value here, rounded to a float only by the
/// product with the distance (one rounding, as the `fmul m` after fcos)
float CosTimes(float radians, float metres)
{
	return static_cast<float>(std::cos(static_cast<double>(radians)) * static_cast<double>(metres));
}
float SinTimes(float radians, float metres)
{
	return static_cast<float>(std::sin(static_cast<double>(radians)) * static_cast<double>(metres));
}

/// __ftol 0x7A1400 (fistp with the rounding set to chop): towards 0, and the "integer indefinite" 0x80000000 for a NaN
/// or a value out of the int32 range
int32_t FtoL(float value)
{
	if (!(value > -2147483648.0f && value < 2147483648.0f))
	{
		return static_cast<int32_t>(0x80000000u);
	}
	return static_cast<int32_t>(value);
}
} // namespace

const std::array<uint16_t, 257>& gutils::ArcTanTable()
{
	static const auto k_Table = [] {
		std::array<uint16_t, 257> table {};
		for (size_t i = 0; i < table.size(); ++i)
		{
			// trunc(atan(i / 256) x 2048 / 2 pi)
			table[i] = static_cast<uint16_t>(std::trunc(std::atan(static_cast<double>(i) / 256.0) * 2048.0 / k_TwoPi));
		}
		return table;
	}();
	return k_Table;
}

const std::array<int32_t, 2560>& gutils::SinTable()
{
	static const auto k_Table = [] {
		std::array<int32_t, 2560> table {};
		for (size_t i = 0; i < table.size(); ++i)
		{
			// trunc(65536 sin(i x 2 pi / 2048)), the dividing by 2048 exact
			table[i] = static_cast<int32_t>(std::trunc(65536.0 * std::sin(static_cast<double>(i) * k_TwoPi / 2048.0)));
		}
		return table;
	}();
	return k_Table;
}

int32_t gutils::Cos(uint16_t angle)
{
	// [0xC31E14 + 4a] = SIN[a + 512]
	return SinTable()[static_cast<size_t>(angle & k_GameAngleMask) + 512];
}

int32_t gutils::Sin(uint16_t angle)
{
	return SinTable()[static_cast<size_t>(angle & k_GameAngleMask)]; // [0xC31614 + 4a]
}

uint16_t gutils::LHArcTan(int32_t dx, int32_t dz)
{
	const auto& table = ArcTanTable();
	// shl eax, 8; xor edx, edx; div ebx: unsigned, the shift keeps the low 32 bits
	const auto t = [&table](int32_t num, int32_t den) {
		return static_cast<int32_t>(table[(static_cast<uint32_t>(num) << 8u) / static_cast<uint32_t>(den)]);
	};
	const int32_t x = -dx; // 0x74D0C5 neg [ebp + 8]
	const int32_t z = dz;
	if (z == 0 && x == 0)
	{
		return 0; // 0x74D0CB..0x74D0DD
	}
	int32_t a;
	if (z >= 0)
	{
		if (x >= 0)
		{
			a = z >= x ? 0x200 + t(x, z) : 0x400 - t(z, x); // 0x74D0ED / 0x74D10E
		}
		else
		{
			a = z >= -x ? 0x200 - t(-x, z) : t(z, -x); // 0x74D12D / 0x74D153
		}
	}
	else
	{
		if (x >= 0)
		{
			a = -z >= x ? 0x600 - t(x, -z) : 0x400 + t(-z, x); // 0x74D172 / 0x74D18F
		}
		else
		{
			a = -z >= -x ? 0x600 + t(-x, -z) : 0x800 - t(-z, -x); // 0x74D1AE / 0x74D1C8
		}
	}
	return static_cast<uint16_t>(a & k_GameAngleMask); // 0x74D1E2
}

uint16_t gutils::GetAngleFromDXDZ(int32_t dx, int32_t dz)
{
	return LHArcTan(dx, dz); // 0x74D20A, & 0xFFFF (0x74D212)
}

uint16_t gutils::GetAngleFromXZ(const MapCoords& from, const MapCoords& to)
{
	return GetAngleFromDXDZ(to.x - from.x, to.z - from.z); // 0x74D240..0x74D259
}

uint16_t gutils::GetAngleFromXZ(glm::ivec2 from, glm::ivec2 to)
{
	return GetAngleFromDXDZ(to.x - from.x, to.y - from.y);
}

uint16_t gutils::GetAngleFromXZ(glm::vec2 from, glm::vec2 to)
{
	return GetAngleFromXZ(map_coords::FromMetres(from), map_coords::FromMetres(to));
}

float gutils::Get3DAngleFromXZ(const MapCoords& from, const MapCoords& to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to)); // 0x74D289 / 0x74D28F
}

float gutils::Get3DAngleFromXZ(glm::ivec2 from, glm::ivec2 to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to));
}

float gutils::Get3DAngleFromXZ(glm::vec2 from, glm::vec2 to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to));
}

uint32_t gutils::ConvertAngle3DToGame(float radians)
{
	const float scaled = radians * k_Angle3DToGame; // fmul [0x99A1C8]
	// __ftol (towards 0), then & 0x7FF (0x74DC3F): a negative value wraps
	return static_cast<uint32_t>(FtoL(scaled)) & static_cast<uint32_t>(k_GameAngleMask);
}

float gutils::ConvertGameAngleTo3D(int32_t angle)
{
	// & 0x7FF, fild qword (exact), fmul [0x99A1CC]
	return static_cast<float>(angle & k_GameAngleMask) * k_GameAngleTo3D;
}

uint32_t gutils::ConvertScawenAngleToGameAngle(float radians)
{
	const float shifted = radians - k_ScawenOffset; // fsub [0x8C78D8], fstp to a float (0x74E29B)
	return ConvertAngle3DToGame(shifted);
}

float gutils::ConvertGameAngleToScawenAngle(uint16_t angle)
{
	// and 0xFFFF; shl 1; fild (exact); fmul [0x8C78DC]; fadd [0x8C78D8]: two roundings
	const float scaled = static_cast<float>(static_cast<int32_t>(angle) << 1) * k_HalfGameAngleTo3D;
	return scaled + k_ScawenOffset;
}

int32_t gutils::GetXFromAngle(uint16_t angle, int32_t distance)
{
	// imul (the low 32 bits), sar 16
	return static_cast<int32_t>(static_cast<uint32_t>(Cos(angle)) * static_cast<uint32_t>(distance)) >> 16;
}

int32_t gutils::GetZFromAngle(uint16_t angle, int32_t distance)
{
	return static_cast<int32_t>(static_cast<uint32_t>(Sin(angle)) * static_cast<uint32_t>(distance)) >> 16;
}

float gutils::GetXFromAngle(uint16_t angle, float distance)
{
	const float scaled = static_cast<float>(Cos(angle)) * distance; // fild; fmul d
	return scaled * (1.0f / 65536.0f);                               // fmul [0x8AC41C]
}

float gutils::GetZFromAngle(uint16_t angle, float distance)
{
	const float scaled = static_cast<float>(Sin(angle)) * distance;
	return scaled * (1.0f / 65536.0f);
}

glm::ivec2 gutils::StepFromAngle(uint16_t angle, int32_t whole)
{
	// sar ecx, 4; imul; sar eax, 0xC (0x74D3B4..0x74D3BA)
	const int32_t s = whole >> 4;
	const auto mul = [s](int32_t table) {
		return static_cast<int32_t>(static_cast<uint32_t>(table) * static_cast<uint32_t>(s)) >> 12;
	};
	return {mul(Cos(angle)), mul(Sin(angle))};
}

glm::ivec2 gutils::StepFromAngle8(uint16_t angle, int32_t whole)
{
	// sar ecx, 8; imul; sar eax, 8 (0x74D3F4..0x74D3FA)
	const int32_t s = whole >> 8;
	const auto mul = [s](int32_t table) {
		return static_cast<int32_t>(static_cast<uint32_t>(table) * static_cast<uint32_t>(s)) >> 8;
	};
	return {mul(Cos(angle)), mul(Sin(angle))};
}

int32_t gutils::GetXByAngleMetersDistance(uint16_t angle, float metres)
{
	const float cells = metres / k_MetresPerCell;               // fdiv [0x99A1BC]
	return FtoL(static_cast<float>(Cos(angle)) * cells); // fild; fmulp; __ftol
}

int32_t gutils::GetZByAngleMetersDistance(uint16_t angle, float metres)
{
	const float cells = metres / k_MetresPerCell;
	return FtoL(static_cast<float>(Sin(angle)) * cells);
}

MapCoords gutils::GetPosFromGameAngle(uint16_t angle, int32_t whole)
{
	const auto step = StepFromAngle(angle, whole); // 0x74D668 / 0x74D671
	return {step.x, step.y, 0.0f};
}

MapCoords gutils::GetPosFromGameAngle(uint16_t angle, float metres)
{
	return GetPosFromGameAngle(angle, ConvertMetersToWholeDistance(metres)); // 0x74DC80
}

MapCoords gutils::GetPosFromAngle(float radians, float metres)
{
	// fcos; fmul m; fmul 65536 [0x8AC408]; fdiv 10 [0x99A1BC]; __ftol (0x74D58F..0x74D5A1), then the same with fsin
	return {map_coords::ToFixedGUtils(CosTimes(radians, metres)), map_coords::ToFixedGUtils(SinTimes(radians, metres)),
	        0.0f};
}

void gutils::AddDistanceFromAngle(MapCoords& pos, float radians, float metres)
{
	// fcos; fmul m; fild p.x; fmul 10; fmul 2^-16 (= ToMetres, one rounding); faddp; fmul 65536; fdiv 10; __ftol
	// (0x74D510..0x74D53B), then z with fsin (0x74D540..0x74D569)
	const float dx = CosTimes(radians, metres);
	const float x = dx + map_coords::ToMetres(pos.x);
	pos.x = map_coords::ToFixedGUtils(x);
	const float dz = SinTimes(radians, metres);
	const float z = dz + map_coords::ToMetres(pos.z);
	pos.z = map_coords::ToFixedGUtils(z);
}

glm::vec3 gutils::GetLHPointFromAngle(float radians, float metres)
{
	return {CosTimes(radians, metres), 0.0f, SinTimes(radians, metres)}; // 0x74D628..0x74D643
}

uint32_t gutils::GetAngleDifference(int32_t a, int32_t b)
{
	// sub; cdq; xor; sub (|a - b|); cmp 0x400; jbe (0x74D748..0x74D757)
	const int32_t d = a - b;
	const auto magnitude = static_cast<uint32_t>(d < 0 ? -d : d);
	return magnitude > 0x400u ? 0x800u - magnitude : magnitude;
}

int32_t gutils::GetAngleDirection(int32_t from, int32_t to)
{
	int32_t d = to - from; // 0x74D6F0..0x74D6F4
	if (d == 0)
	{
		return 0;
	}
	const auto magnitude = static_cast<uint32_t>(d < 0 ? -d : d);
	if (magnitude > 0x400u) // cmp 0x400; jbe 0x74D728
	{
		d += d < 0 ? 0x800 : -0x800; // 0x74D711 / 0x74D722
	}
	return d < 0 ? -1 : 1; // setl; dec; and 2; dec
}
