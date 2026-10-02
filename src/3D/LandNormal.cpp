/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandNormal.h"

#include <cmath>
#include <cstdlib>

#include <array>

using namespace openblack;

namespace
{
constexpr float k_EdgeSquared = 100.0f;         ///< [0x8AB41C]: a cell edge of 10 m, squared
constexpr float k_CellEdge = 10.0f;             ///< [0x8AB414]
constexpr float k_LengthSteps = 1023.0f;        ///< [0x9A2BE8]
constexpr float k_LengthStep = 0.000977517106f; ///< [0x9A2BEC] = 0x3A802008 (1 / 1023)
constexpr uint32_t k_EdgeEntries = 0x100;       ///< "cmp eax, 0x100" 0x803916
constexpr uint32_t k_LengthEntries = 0x400;     ///< 0xE9A2D8..0xE9B2D8

/// 0x8038E7..0x80392D: fild i; fmul 0.67; squared; fadd 100; fsqrt; fdivr 1 (every step at 24 bits)
float EdgeFormula(uint32_t climb)
{
	const float rise = static_cast<float>(climb) * land_normal::k_HeightUnit;
	return 1.0f / std::sqrt(rise * rise + k_EdgeSquared);
}

const std::array<float, k_EdgeEntries>& EdgeTable()
{
	static const auto table = [] {
		std::array<float, k_EdgeEntries> t {};
		for (uint32_t i = 0; i < k_EdgeEntries; ++i)
		{
			t[i] = EdgeFormula(i);
		}
		return t;
	}();
	return table;
}

const std::array<float, k_LengthEntries>& LengthTable()
{
	static const auto table = [] {
		std::array<float, k_LengthEntries> t {};
		t[0] = 1.0f; // mov [0xE9A2D8], 0x3F800000 (0x803936)
		for (uint32_t j = 1; j < k_LengthEntries; ++j)
		{
			// 0x803949..0x80396C: fild j; fmul [0x9A2BEC]; fsqrt; fdivr 1.0 (the double [0x8AB680], rounded to 24 bits)
			t[j] = 1.0f / std::sqrt(static_cast<float>(j) * k_LengthStep);
		}
		return t;
	}();
	return table;
}
} // namespace

float land_normal::EdgeScale(uint32_t climb)
{
	return climb < k_EdgeEntries ? EdgeTable()[climb] : EdgeFormula(climb);
}

float land_normal::LengthScale(uint32_t index)
{
	// (port guard) |n|^2 <= 1 keeps the index at 1023 or less in the original
	return LengthTable()[index < k_LengthEntries ? index : k_LengthEntries - 1];
}

glm::vec3 land_normal::OfCell(uint32_t fracX, uint32_t fracZ, bool split, int32_t h00, int32_t h01, int32_t h10, int32_t h11)
{
	// B (the triangle's base corner): its height and position; P, Q the other two (0x803698..0x803752)
	int32_t hB;
	int32_t hP;
	int32_t hQ;
	float bx;
	float bz;
	float pz; // the first "fld" of the branch: P's z
	float qz; // [ebp - 0x1C]: Q's z
	if (split)
	{
		hP = h10; // [+0x8C]
		hQ = h01; // [+0xC]
		pz = 0.0f;
		qz = k_CellEdge;
		// "cmp edx, ebx; jle" with ebx = 0xFFFF - fx (0x8036C5..0x8036CF)
		if (static_cast<int32_t>(fracZ) > 0xFFFF - static_cast<int32_t>(fracX))
		{
			hB = h11; // [+0x94]
			bx = k_CellEdge;
			bz = k_CellEdge;
		}
		else
		{
			hB = h00; // [+4]
			bx = 0.0f;
			bz = 0.0f;
		}
	}
	else
	{
		hP = h11;
		hQ = h00;
		pz = k_CellEdge;
		qz = 0.0f;
		// "cmp bx, [edi + 4]; jbe" (0x803716..0x803727)
		if (fracX > fracZ)
		{
			hB = h10;
			bx = k_CellEdge;
			bz = 0.0f;
		}
		else
		{
			hB = h01;
			bx = 0.0f;
			bz = k_CellEdge;
		}
	}
	const int32_t dP = hP - hB; // 0x803754
	const int32_t dQ = hQ - hB; // 0x803756
	// 0x803766..0x80377C: T1[|dP|] * T1[|dQ|]
	const float scale = EdgeScale(static_cast<uint32_t>(std::abs(dP))) * EdgeScale(static_cast<uint32_t>(std::abs(dQ)));
	const float qx = -bx;                                      // fchs 0x803781 ([ebp - 0x18])
	const float qy = static_cast<float>(dQ) * k_HeightUnit;    // 0x803786..0x80378F ([ebp - 0x14])
	const float qzRel = qz - bz;                               // 0x803792..0x803798 ([ebp - 0x10])
	const float px = k_CellEdge - bx;                          // 0x80379B..0x8037A3 ([ebp - 0x24])
	const float py = static_cast<float>(dP) * k_HeightUnit;    // 0x8037A8..0x8037B1 ([ebp - 0x20])
	const float pzRel = pz - bz;                               // 0x8037B4..0x8037B7 ([ebp - 0x1C])
	glm::vec3 n;
	n.x = (pzRel * qy - py * qzRel) * scale; // 0x8037BA..0x8037C8
	n.y = (qzRel * px - pzRel * qx) * scale; // 0x8037CA..0x8037DE
	n.z = (py * qx - qy * px) * scale;       // 0x8037E1..0x8037F5
	// 0x8037F8..0x803829: ((nz nz + nx nx) + ny ny) * 1023, stored as a float, then fistp (to nearest)
	const float squared = ((n.z * n.z + n.x * n.x) + n.y * n.y) * k_LengthSteps;
	const auto index = static_cast<uint32_t>(std::lrint(squared));
	const float length = LengthScale(index); // 0x80382E
	n.x = length * n.x;                      // 0x803835..0x80383C
	n.y = length * n.y;
	n.z = length * n.z;
	if (n.y < 0.0f) // fcomp [0x8AA398] 0x803852
	{
		n = -n; // 0x80385F..0x803871
	}
	return n;
}
