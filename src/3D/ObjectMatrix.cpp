/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectMatrix.h"

#include <glm/geometric.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

#include "ECS/Components/Transform.h"

using namespace openblack;

namespace
{
constexpr float k_MinDeterminant = 1e-10f; ///< [0xC371D4]

// fn_007FA990's qword constants: float(pi / 2) and float(pi) as doubles (0x3FF921FB60000000, 0x400921FB60000000)
constexpr double k_HalfPiF = 1.5707963705062866;       ///< [0x8C7B48]
constexpr double k_MinusHalfPiF = -1.5707963705062866; ///< [0x9361E8]
constexpr double k_PiF = 3.1415927410125732;           ///< [0x8D45D0]
// fn_007FAA50 / fn_007FAAF0's dword constants
constexpr float k_NoYAngle = 1e-6f;            ///< [0x9A2BAC] 0x358637BD
constexpr float k_PiFloat = 3.14159274f;       ///< [0x8C36A0] 0x40490FDB
constexpr float k_MinusPiFloat = -3.14159274f; ///< [0x8C79A4] 0xC0490FDB
constexpr float k_TwoPiFloat = 6.28318548f;    ///< [0x8AB210] 0x40C90FDB
static_assert(static_cast<double>(k_PiFloat) == k_PiF && static_cast<double>(k_PiFloat / 2.0f) == k_HalfPiF);

/// A value of the FPU stack (fsin / fcos, extended) times a float, rounded once to 24 bits by the fmul
float Mul(double extended, float value)
{
	return static_cast<float>(extended * static_cast<double>(value));
}

/// One of LHMatrix's in-place pair turns, on component i and j of every row: (e_i, e_j) -> (c e_i + s e_j, c e_j - s e_i)
void TurnPair(glm::mat3& m, int i, int j, double c, double s)
{
	for (int r = 0; r < 3; ++r)
	{
		const float ei = m[r][i];
		const float ej = m[r][j];
		m[r][i] = Mul(c, ei) + Mul(s, ej);
		m[r][j] = Mul(c, ej) - Mul(s, ei);
	}
}

/// The same on rows i and j (each of the three columns of the LHMatrix): r_i' = c r_i + s r_j, r_j' = c r_j - s r_i
void TurnRowPair(glm::mat3& m, int i, int j, double c, double s)
{
	for (int k = 0; k < 3; ++k)
	{
		const float ei = m[i][k];
		const float ej = m[j][k];
		m[i][k] = Mul(c, ei) + Mul(s, ej);
		m[j][k] = Mul(c, ej) - Mul(s, ei);
	}
}
} // namespace

glm::mat3 lh_matrix::YXZ(float y, float x, float z)
{
	const double ca = std::cos(static_cast<double>(y)); // 0x7FAC15 (kept)
	const double sa = std::sin(static_cast<double>(y)); // 0x7FAC1B (kept)
	const auto cb = static_cast<float>(std::cos(static_cast<double>(x))); // fstp [esp] 0x7FAC23
	const double sb = std::sin(static_cast<double>(x)); // 0x7FAC2B (kept)
	const double cc = std::cos(static_cast<double>(z)); // 0x7FAC31 (kept)
	const auto sc = static_cast<float>(std::sin(static_cast<double>(z))); // fstp [esp + 8] 0x7FAC39
	const auto caCc = static_cast<float>(cc * ca);  // 0x7FAC3D..0x7FAC41
	const float scSb = Mul(sb, sc);                 // 0x7FAC45..0x7FAC49 (stays on the stack, rounded to 24 bits)
	const auto saCc = static_cast<float>(cc * sa);  // 0x7FAC4B..0x7FAC4F
	glm::mat3 m;
	m[0][0] = caCc - Mul(sa, scSb);                  // 0x7FAC53..0x7FAC5B
	m[0][1] = -(sc * cb);                            // 0x7FAC5D..0x7FAC67
	m[0][2] = Mul(ca, scSb) + saCc;                  // 0x7FAC6A..0x7FAC70
	m[1][0] = Mul(sb, saCc) + Mul(ca, sc);           // 0x7FAC73..0x7FAC81
	m[1][1] = Mul(cc, cb);                           // 0x7FAC84..0x7FAC88
	m[1][2] = Mul(sa, sc) - Mul(sb, caCc);           // 0x7FAC8B..0x7FAC99
	m[2][0] = -Mul(sa, cb);                          // 0x7FAC9C..0x7FACA4
	m[2][1] = static_cast<float>(sb);                // 0x7FACA7
	m[2][2] = Mul(ca, cb);                           // 0x7FACAC..0x7FACB2
	return m;
}

glm::mat3 lh_matrix::AngleY(float a)
{
	const auto c = static_cast<float>(std::cos(static_cast<double>(a))); // fst [+0x64], fstp [+0x44] 0x674384
	const auto s = static_cast<float>(std::sin(static_cast<double>(a))); // fst [+0x4C] 0x674390
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

glm::mat3 lh_matrix::AngleXYZ(float x, float y, float z)
{
	const auto cx = static_cast<float>(std::cos(static_cast<double>(x))); // 0x674224 / 0x674227
	const auto sx = static_cast<float>(std::sin(static_cast<double>(x))); // 0x674230 / 0x674235
	glm::mat3 m {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, cx, -sx), glm::vec3(0.0f, sx, cx)};
	// (e0, e2) -> (cy e0 - sy e2, cy e2 + sy e0): TurnPair(0, 2) with -sy
	TurnPair(m, 0, 2, std::cos(static_cast<double>(y)), -std::sin(static_cast<double>(y)));
	// (e0, e1) -> (cz e0 + sz e1, cz e1 - sz e0)
	TurnPair(m, 0, 1, std::cos(static_cast<double>(z)), std::sin(static_cast<double>(z)));
	return m;
}

void lh_matrix::RotateY(glm::mat3& m, float a)
{
	RotateY(m, std::cos(static_cast<double>(a)), std::sin(static_cast<double>(a))); // 0x5198FC..0x519956
}

void lh_matrix::RotateY(glm::mat3& m, double c, double s)
{
	TurnRowPair(m, 0, 2, c, s);
}

void lh_matrix::RotateZ(glm::mat3& m, double c, double s)
{
	// r0' = c r0 - s r1, r1' = c r1 + s r0: the pair (0, 1) with -s
	TurnRowPair(m, 0, 1, c, -s);
}

void lh_matrix::RotateX(glm::mat3& m, double c, double s)
{
	// r1' = c r1 - s r2, r2' = c r2 + s r1: the pair (1, 2) with -s
	TurnRowPair(m, 1, 2, c, -s);
}

void lh_matrix::RotateZ(glm::mat3& m, float a)
{
	// r0' = c r0 - s r1, r1' = c r1 + s r0: the pair (0, 1) with -s (0x86AFAC..0x86B008)
	TurnRowPair(m, 0, 1, std::cos(static_cast<double>(a)), -std::sin(static_cast<double>(a)));
}

namespace
{
/// MakeInverseSqrtLookupTable 0x8411D0, [0xEEA394]
const std::array<uint8_t, 128>& InverseSqrtTable()
{
	static const std::array<uint8_t, 128> k_Table = [] {
		std::array<uint8_t, 128> table {};
		for (uint32_t i = 0; i < table.size(); ++i)
		{
			// 0x8411E7..0x8411F2: (i | 0x1F80) << 17, 0.5 .. 2; fsqrt and fdivr of the double 1.0 [0x8AB680], each
			// rounded to a float by the 24-bit precision, then fstp
			const float x = std::bit_cast<float>((i | 0x1F80u) << 17);
			const float y = 1.0f / std::sqrt(x);
			// 0x841206..0x841213: + 0x2000, >> 15, the low byte
			table.at(i) = static_cast<uint8_t>((std::bit_cast<uint32_t>(y) + 0x2000u) >> 15);
		}
		table[0x40] = 0xFF; // 0x841224
		return table;
	}();
	return k_Table;
}
} // namespace

float lh_matrix::InverseSquareRoot(float value)
{
	const auto bits = std::bit_cast<uint32_t>(value);
	// 0x841179..0x8411AA
	const uint32_t exponent = ((bits >> 23) & 0xFFu) << 22;
	const uint32_t guess = ((0x5F000000u - exponent) & 0xFF800000u) |
	                       (static_cast<uint32_t>(InverseSqrtTable()[(bits >> 17) & 0x7Fu]) << 15);
	const float y = std::bit_cast<float>(guess);
	// 0x8411B0..0x8411C2: fmul y, fmul y, fsubr 3, fmul y, fmul 0.5
	float r = value * y;
	r = r * y;
	r = 3.0f - r;
	r = r * y;
	return r * 0.5f;
}

void lh_matrix::NormaliseRows(glm::mat3& m)
{
	for (int row = 0; row < 3; ++row)
	{
		m[row] *= InverseSquareRoot(glm::dot(m[row], m[row]));
	}
}

void lh_matrix::TurnRows(glm::mat3& m, int axis, double c, double s)
{
	switch (axis)
	{
	case 2: // 0x6A116B..0x6A1211, 0x6A6293..0x6A62AC
		TurnPair(m, 0, 1, c, s);
		break;
	case 1: // 0x6A1218..0x6A12C0: (x, z) -> (c x - s z, c z + s x)
		TurnPair(m, 0, 2, c, -s);
		break;
	default: // fn_006A12F0, 0x6A6317..0x6A6330
		TurnPair(m, 1, 2, c, s);
		break;
	}
}

void lh_matrix::TurnRows(glm::mat3& m, int axis, float a)
{
	TurnRows(m, axis, std::cos(static_cast<double>(a)), std::sin(static_cast<double>(a)));
}

glm::mat3 lh_matrix::AxisAngle(const glm::vec3& axis, float a)
{
	const double c = std::cos(static_cast<double>(a)); // 0x7FB189 (kept)
	const double s = std::sin(static_cast<double>(a)); // 0x7FB18F (kept)
	const float xx = axis.x * axis.x;                  // [esp + 0x20] 0x7FB197
	const float yy = axis.y * axis.y;                  // [esp + 8]
	const float zz = axis.z * axis.z;                  // [esp + 0x18]
	const float xy = axis.x * axis.y;                  // [esp] 0x7FB1BC
	const float xz = axis.x * axis.z;                  // [esp + 4]
	const float zy = axis.z * axis.y;                  // [esp + 0xC]
	const float sx = Mul(s, axis.x);                   // [esp + 0x14] 0x7FB1D7
	const float sy = Mul(s, axis.y);                   // [esp + 0x10]
	const float sz = Mul(s, axis.z);                   // on the stack 0x7FB1E4
	const float t = xy - Mul(c, xy);                   // 0x7FB1F9..0x7FB1FF
	const float u = xz - Mul(c, xz);                   // 0x7FB20A..0x7FB210
	const float v = zy - Mul(c, zy);                   // 0x7FB239..0x7FB23F
	glm::mat3 m;
	m[0][0] = Mul(c, 1.0f - xx) + xx; // 0x7FB1E7..0x7FB1F7
	m[1][0] = t + sz;                 // [ecx + 0xC] 0x7FB203..0x7FB207
	m[2][0] = u - sy;                 // [ecx + 0x18] 0x7FB218..0x7FB21C
	m[0][1] = t - sz;                 // [ecx + 4] 0x7FB21F..0x7FB221
	m[1][1] = Mul(c, 1.0f - yy) + yy; // 0x7FB226..0x7FB236
	m[2][1] = v + sx;                 // [ecx + 0x1C] 0x7FB243..0x7FB249
	m[0][2] = u + sy;                 // [ecx + 8] 0x7FB24C..0x7FB254
	m[1][2] = v - sx;                 // [ecx + 0x14] 0x7FB257..0x7FB25B
	m[2][2] = Mul(c, 1.0f - zz) + zz; // 0x7FB25E..0x7FB26E
	return m;
}

double lh_matrix::ArcTanOctant(float a, float b)
{
	// the fcomp / fnstsw / test pairs: `test ah, 1` = C0 (less or unordered), `test ah, 0x41` = C0 | C3 (also equal);
	// written so that a NaN takes the same branch as in the original
	if (a >= b && !(-b > a)) // 0x7FA990..0x7FA99D (fcomp b; test ah 1; jne), 0x7FA99F..0x7FA9AE (-b; test ah 0x41; je)
	{
		return std::atan(static_cast<double>(b / a)); // fdiv 0x7FA9B4; fld1; fpatan 0x7FA9BA (not rounded)
	}
	if (b >= a && !(-a > b)) // 0x7FA9BD..0x7FA9CA, 0x7FA9CC..0x7FA9DB
	{
		// fdiv 0x7FA9E1; fpatan 0x7FA9E7; fsubr 0x7FA9E9 (24 bits)
		return static_cast<float>(k_HalfPiF - std::atan(static_cast<double>(a / b)));
	}
	if (-b >= a && !(a >= b)) // 0x7FA9F0..0x7FA9FF (-b; test ah 1; jne), 0x7FAA01..0x7FAA0E (fcomp b; test ah 1; je)
	{
		const double t = std::atan(static_cast<double>(b / a)); // fdiv 0x7FAA1E; fpatan 0x7FAA29
		// fcomp [0x8AA398] 0x7FAA14, test ah 1 0x7FAA26: b < 0 -> fsub 0x7FAA34, else fadd 0x7FAA2D (24 bits)
		return static_cast<float>(!(b >= 0.0f) ? t - k_PiF : t + k_PiF);
	}
	// 0x7FAA3B..0x7FAA47: fdiv; fpatan; fsubr (24 bits)
	return static_cast<float>(k_MinusHalfPiF - std::atan(static_cast<double>(a / b)));
}

double lh_matrix::GetYAngle(const glm::vec3& v)
{
	const float xx = v.x * v.x;       // 0x7FAA5D..0x7FAA61
	const float zz = v.z * v.z;       // 0x7FAA65..0x7FAA67
	const float horizontal = xx + zz; // faddp 0x7FAA69
	if (!(horizontal > k_NoYAngle))   // fcomp [0x9A2BAC] 0x7FAA6B; test ah 0x41; jne 0x7FAA91
	{
		return 0.0; // fld [0x8AA398] 0x7FAA91
	}
	return ArcTanOctant(-v.z, v.x); // 0x7FAA7A..0x7FAA88: [esp] = -z (fchs; fstp), [esp + 4] = x
}

float lh_matrix::WrapAngle(float a)
{
	if (a > k_PiFloat) // fcomp [0x8C36A0] 0x7FAAF4; test ah 0x41; jne 0x7FAB0C
	{
		return a - k_TwoPiFloat; // fsub [0x8AB210] 0x7FAB05
	}
	if (!(a >= k_MinusPiFloat)) // fcomp [0x8C79A4] 0x7FAB0C; test ah 1; je 0x7FAB24
	{
		return a + k_TwoPiFloat; // fadd [0x8AB210] 0x7FAB1D
	}
	return a; // fld a 0x7FAB24
}

glm::mat4x3 lh_matrix::Inverse(const glm::mat4x3& m)
{
	// m0..m8 are the rows' cells (m[r][c] = cell 3r + c), t0..t2 the translation
	const float m0 = m[0][0];
	const float m1 = m[0][1];
	const float m2 = m[0][2];
	const float m3 = m[1][0];
	const float m4 = m[1][1];
	const float m5 = m[1][2];
	const float m6 = m[2][0];
	const float m7 = m[2][1];
	const float m8 = m[2][2];
	// 0x7FB290..0x7FB2C6: a = m8 m4 - m7 m5; det = ((m2 m7 - m8 m1) m3 + (m5 m1 - m2 m4) m6) + a m0
	const float a = m8 * m4 - m7 * m5;
	float det = (m2 * m7 - m8 * m1) * m3 + (m5 * m1 - m2 * m4) * m6;
	det = det + a * m0;
	// 0x7FB2C8..0x7FB2EE: fcompp with 1e-10 first ("test ah, 0x41": kept when 1e-10 <= |det|, or unordered)
	if (k_MinDeterminant > std::abs(det))
	{
		det = det < 0.0f ? -k_MinDeterminant : k_MinDeterminant;
	}
	const float inv = 1.0f / det; // fdivr [0x8AA390] 0x7FB2F0
	glm::mat4x3 out;
	out[0][0] = a * inv;                     // 0x7FB2F8
	out[1][0] = (m6 * m5 - m8 * m3) * inv;   // [ecx + 0xC] 0x7FB2FC..0x7FB30C
	out[2][0] = (m7 * m3 - m6 * m4) * inv;   // [ecx + 0x18] 0x7FB30F..0x7FB31F
	out[0][1] = (m2 * m7 - m8 * m1) * inv;   // [ecx + 4] 0x7FB322..0x7FB332
	out[1][1] = (m8 * m0 - m6 * m2) * inv;   // [ecx + 0x10] 0x7FB335..0x7FB344
	out[2][1] = (m6 * m1 - m0 * m7) * inv;   // [ecx + 0x1C] 0x7FB347..0x7FB356
	out[0][2] = (m5 * m1 - m2 * m4) * inv;   // [ecx + 8] 0x7FB359..0x7FB369
	out[1][2] = (m2 * m3 - m0 * m5) * inv;   // [ecx + 0x14] 0x7FB36C..0x7FB37B
	out[2][2] = (m0 * m4 - m3 * m1) * inv;   // [ecx + 0x20] 0x7FB37E..0x7FB38D
	const float t0 = m[3][0];
	const float t1 = m[3][1];
	const float t2 = m[3][2];
	out[3][0] = -((t1 * out[1][0] + t2 * out[2][0]) + t0 * out[0][0]); // 0x7FB392..0x7FB3A9
	out[3][1] = -((out[2][1] * t2 + t0 * out[0][1]) + out[1][1] * t1); // 0x7FB3AC..0x7FB3C4
	out[3][2] = -((t0 * out[0][2] + out[2][2] * t2) + out[1][2] * t1); // 0x7FB3C7..0x7FB3DF
	return out;
}

glm::mat4 lh_matrix::SetPosition(const glm::vec3& p, float a, float s)
{
	glm::mat4 m(0.0f);
	// fcomp [0x8AA390] 0x423151: s != 1 adds p to the zeroed translation (fadd 0x423198 / 0x423315), s == 1 copies it
	const glm::vec3 t = s != 1.0f ? glm::vec3(0.0f) + p : p;
	if (a == 0.0f) // fcomp [0x8AA398] 0x423145: diag(s), no turn
	{
		m[0][0] = s;
		m[1][1] = s;
		m[2][2] = s;
		m[3] = glm::vec4(t, 1.0f);
		return m;
	}
	// diag(s), then RotateY in place (0x4231B3..0x42321C): rows (c s, 0, s s), (0, s, 0), (-(s s), 0, c s)
	const double c = std::cos(static_cast<double>(a));
	const double sn = std::sin(static_cast<double>(a));
	m[0] = glm::vec4(Mul(c, s), 0.0f, Mul(sn, s), 0.0f);
	m[1] = glm::vec4(0.0f, s, 0.0f, 0.0f);
	m[2] = glm::vec4(-Mul(sn, s), 0.0f, Mul(c, s), 0.0f);
	m[3] = glm::vec4(t, 1.0f);
	return m;
}

glm::mat4 lh_matrix::Model(const glm::vec3& p, const glm::mat3& r, const glm::vec3& s)
{
	return {glm::vec4(r[0] * s.x, 0.0f), glm::vec4(r[1] * s.y, 0.0f), glm::vec4(r[2] * s.z, 0.0f), glm::vec4(p, 1.0f)};
}

glm::mat4 lh_matrix::Model(const ecs::components::Transform& transform)
{
	return Model(transform.position, transform.rotation, transform.scale);
}
