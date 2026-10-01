/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GUtilsDistance.h"

#include <cmath>

#include <algorithm>

using namespace openblack;
using openblack::ecs::map_coords::JustMapXZ;
using openblack::ecs::map_coords::MapCoords;

namespace
{
namespace map_coords = openblack::ecs::map_coords;

/// fn_0074E2D0: the 16.16 x or z of a map cell's centre, (short(cell) << 16) + 0x8000
int32_t CellCentre(int16_t cell)
{
	return (static_cast<int32_t>(cell) << 16) + 0x8000;
}

/// The clamp of SigmoidThreshold, twice over (0x74F18C..0x74F1B8 and 0x74F1BC..0x74F1E8): `fcom -1 [0x8AB678];
/// test ah, 1` takes -1 when the value is below (and when the compare is unordered, so a NaN becomes -1), then
/// `fcom 1 [0x8AA390]; test ah, 0x41` keeps it while it is below or equal to 1
float SigmoidClamp(float value)
{
	if (!(value >= -1.0f))
	{
		return -1.0f;
	}
	return value > 1.0f ? 1.0f : value;
}
} // namespace

const std::array<uint32_t, 1024>& gutils::InvSqrtTable()
{
	// The original builds it once into 0xDA5A10 behind the flag [0xDA6A10]; a function-local static does the same.
	// `fsqrt; fdivr 1` runs at 24 bits, so a double square root cast back to float gives the same 1024 entries
	static const auto s_table = [] {
		std::array<uint32_t, 1024> table {};
		for (uint32_t i = 0; i < table.size(); ++i)
		{
			// 0x74F5AE..0x74F5CC: (0x3F800000 & 0xFF003FFF) | ((i & 0x3FF) << 14) = 0x3F000000 | i << 14, so f is in
			// [0.5, 1) for i < 512 and in [1, 2) above
			const auto f = std::bit_cast<float>((0x3F800000u & 0xFF003FFFu) | ((i & 0x3FFu) << 14u));
			const auto r = static_cast<float>(1.0 / std::sqrt(static_cast<double>(f))); // fsqrt; fdivr; fst dword
			table[i] = r == 1.0f ? 0x7FE000u : (std::bit_cast<uint32_t>(r) & 0x7FE000u);
		}
		return table;
	}();
	return s_table;
}

float gutils::InvSqrt(float value)
{
	const auto bits = std::bit_cast<uint32_t>(value);
	// 0x74F626..0x74F645
	const uint32_t exponent = ((0xBE000000u - (bits & 0x7F800000u)) >> 1u) & 0x7F800000u;
	return std::bit_cast<float>(exponent | InvSqrtTable()[(bits >> 14u) & 0x3FFu]);
}

int32_t gutils::Hypotenuse(int32_t dx, int32_t dz)
{
	// fild; fmul [0x99A1D4] = 2^-16. fild is exact and the scale is a power of two, so rounding the integer to a float
	// first (above 2^24 units, 2560 m) gives the same value as the original's single rounding
	const float x = static_cast<float>(dx) * 1.52587890625e-05f;
	const float z = static_cast<float>(dz) * 1.52587890625e-05f;
	const float squared = z * z + x * x; // 0x74F695..0x74F69F, stored as a float at 0x74F69F
	// fdivr qword [0x99A1D8] = 65536.0 (a double load, but the FPU is at 24 bits); jmp __ftol 0x7A1400 truncates
	return static_cast<int32_t>(65536.0f / InvSqrt(squared));
}

float gutils::Hypotenuse(float a, float b)
{
	// 0x74F6C0..0x74F6EC: fabs; fcomp [0x8BF518]; test ah, 0x41 on both sides
	if (std::abs(a) <= k_HypotenuseEpsilon && std::abs(b) <= k_HypotenuseEpsilon)
	{
		return 0.0f;
	}
	const float squared = a * a + b * b; // 0x74F6ED..0x74F700, stored as a float
	return 1.0f / InvSqrt(squared);      // fdivr [0x8AA390]
}

int32_t gutils::GetDistance(const MapCoords& a, const MapCoords& b)
{
	return Hypotenuse(b.x - a.x, b.z - a.z); // 0x74CCC3..0x74CCC9
}

int32_t gutils::GetDistanceToCell(const MapCoords& a, JustMapXZ cell)
{
	return Hypotenuse(CellCentre(cell.x) - a.x, CellCentre(cell.z) - a.z); // 0x74CD20..0x74CD43
}

float gutils::GetDistanceInMetres(const MapCoords& a, const MapCoords& b)
{
	return ConvertWholeDistanceToMeters(GetDistance(a, b));
}

float gutils::GetDistanceInMetres(glm::vec3 a, glm::vec3 b)
{
	return GetDistanceInMetres(map_coords::FromMetres({a.x, a.z}), map_coords::FromMetres({b.x, b.z}));
}

float gutils::GetDistanceInMetres(glm::vec2 a, glm::vec2 b)
{
	return GetDistanceInMetres(map_coords::FromMetres(a), map_coords::FromMetres(b));
}

float gutils::GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b)
{
	return GetDistanceInMetres(MapCoords {a.x, a.y, 0.0f}, MapCoords {b.x, b.y, 0.0f});
}

float gutils::GetDistanceInMetresToCell(const MapCoords& a, JustMapXZ cell)
{
	return ConvertWholeDistanceToMeters(GetDistanceToCell(a, cell));
}

float gutils::GetDistance(glm::vec3 a, glm::vec3 b)
{
	// 0x74CDE8..0x74CDFA: the differences are stored as floats before the call (LHPoint keeps x at +0 and z at +8)
	const float dx = b.x - a.x;
	const float dz = b.z - a.z;
	return Hypotenuse(dx, dz);
}

float gutils::GetMetresDistanceSq(const MapCoords& a, const MapCoords& b)
{
	// 0x605FB6..0x605FE5: mx is stored as a float (0x605FC2) and mz stays in st0, which at 24 bits is the same
	const float mx = ConvertWholeDistanceToMeters(b.x - a.x);
	const float mz = ConvertWholeDistanceToMeters(b.z - a.z);
	return mz * mz + mx * mx;
}

float gutils::SigmoidThreshold(float a, float b)
{
	// 0x74F174: fcomp 1; test ah, 0x40 (C3, set as well when the compare is unordered)
	if (a == 1.0f || std::isnan(a))
	{
		return 0.0f;
	}
	const float v = SigmoidClamp(SigmoidClamp(b) - a); // fsub [esp + 4] at 0x74F1B8
	// fadd 1; fmul 20.5 [0x99A1D0]; __ftol; cmp eax, 0x28; jbe (unsigned)
	const auto index = static_cast<uint32_t>(static_cast<int32_t>((v + 1.0f) * 20.5f));
	return k_Sigmoid[std::min(index, 40u)];
}

float gutils::GetDistanceModifier(float distance, float maximum)
{
	// 0x74F290..0x74F2A9: fcomp; test ah, 1 keeps the distance only while it is strictly below the maximum
	const float m = distance < maximum ? distance : maximum;
	const float b = 1.0f - m / maximum; // fdiv; fsubr 1 [0x8AA390]; fstp dword (a float) at 0x74F2B4
	return SigmoidThreshold(0.5f, b);   // push 0x3F000000 at 0x74F2B7: the threshold is 0.5
}

float gutils::DistanceChangeToBelief(float x, float y)
{
	const float b = -(x / y);            // fdiv; fchs; fstp dword (a float) at 0x438775..0x43877B
	return SigmoidThreshold(-0.9f, b);   // push 0xBF666666 at 0x43877E
}

float gutils::CreatureSigmoidThreshold(float a, float b)
{
	// 0x4F78C4: fcomp 0; test ah, 0x41 (b < 0 or b == 0)
	if (b <= 0.0f)
	{
		return 0.0f;
	}
	return SigmoidThreshold(a, b);
}
