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
#include <glm/mat4x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct Transform;
}

/// The LHMatrix of runblack.exe as openblack keeps it (wiki: engine-math.md, "Matrices LH"). Pure math.
///
/// Conventions. An LHMatrix is 3 rows and a translation, for row vectors (p' = p M): row k is the image of local axis k.
/// glm uses column vectors, so column k of a glm matrix is row k of the LHMatrix, with the same memory (a glm::mat4x3 is
/// the 12 floats of an LHMatrix, column 3 being the translation). Rx/Ry/Rz(t) below are glm's right-handed rotations
/// (glm::rotate(t, axis), glm::eulerAngleY(t)); LH3D turns the other way, so its angle a is glm's -a.
///
/// Precision. The game runs with the FPU at 24 bits (fn_007DEE00), so every product and sum is a float one, but
/// fsin/fcos are not rounded by the precision control: a value the original keeps on the FPU stack is taken here in
/// double and rounded once by the product that uses it, and a value it stores (fstp dword) is a float, as gutils does
/// (engine-math.md, "Ángulos de GUtils"). (aproximado) double is not the 80-bit register: the last bit may differ in rare
/// cases.
namespace openblack::lh_matrix
{

/// LHMatrix::SetYXZMatrixOnly(y, x, z) 0x7FAC10 = glm::eulerAngleYXZ(-y, -x, -z) = Ry(-y) Rx(-x) Rz(-z), cell by cell in
/// the original's order (a = y, b = x, c = z): m0 = (ca cc) - ((sc sb) sa), m1 = -(sc cb), m2 = ((sc sb) ca) + (sa cc),
/// m3 = ((sa cc) sb) + (sc ca), m4 = cc cb, m5 = (sc sa) - ((ca cc) sb), m6 = -(cb sa), m7 = sb, m8 = cb ca. cb and sc
/// are stored as floats (0x7FAC23, 0x7FAC39), and so are ca cc and sa cc (0x7FAC41, 0x7FAC4F); ca, sa, sb and cc stay on
/// the FPU stack. The translation is not touched (the caller's)
[[nodiscard]] glm::mat3 YXZ(float y, float x, float z);

/// AtomCore::SetAngleY 0x674360: rows (c, 0, s), (0, 1, 0), (-s, 0, c) with c and s stored as floats (0x674384,
/// 0x674390) = glm::eulerAngleY(-a) = Ry(-a). The same rotation as LH3DObject::SetPosition 0x423140,
/// Object::GetWorldMatrix 0x638200 and the SetScale + RotateY of the creations, and as YXZ(a, 0, 0)
[[nodiscard]] glm::mat3 AngleY(float a);

/// AtomCore::SetAngleXYZ(x, y, z) 0x674200 = Rz(-z) Ry(-y) Rx(-x): rows (1, 0, 0), (0, cx, -sx), (0, sx, cx) with cx and
/// sx stored (0x674224..0x674235); then each row's (e0, e2) -> (cy e0 - sy e2, cy e2 + sy e0) (0x674244..0x6742A2) and
/// each row's (e0, e1) -> (cz e0 + sz e1, cz e1 - sz e0) (0x6742D2..0x674330), cy, sy, cz and sz on the FPU stack
[[nodiscard]] glm::mat3 AngleXYZ(float x, float y, float z);

/// LHMatrix::RotateY(a) 0x5198F0, in place: r0' = c r0 + s r2, r2' = c r2 - s r0 (rows; c and s on the FPU stack); r1
/// and the translation stay. In glm: m * Ry(-a), a turn about the matrix's own Y axis (on the right)
void RotateY(glm::mat3& m, float a);
/// fn_0086AFA0(a) 0x86AFA0, in place: r0' = c r0 - s r1, r1' = c r1 + s r0 = m * Rz(-a) (on the right)
void RotateZ(glm::mat3& m, float a);
/// The same two turns with c and s given, for the inline copies that keep them with other precisions (HelpDude::Update1's
/// HUD pose stores c as a float and keeps s on the FPU stack: 0x5BE467..0x5BE4DE, 0x5BE604..0x5BE69D)
void RotateY(glm::mat3& m, double c, double s);
void RotateZ(glm::mat3& m, double c, double s);
/// (openblack name: the original only has it inline, Update1's pitch 0x5BE3DF..0x5BE456) in place: r1' = c r1 - s r2,
/// r2' = c r2 + s r1, the RotateZ pattern on rows 1 and 2; r0 and the translation stay
void RotateX(glm::mat3& m, double c, double s);

/// The world turn of UpdateRuleRotatePrincipalAxis 0x6A1150 and AppearanceRuleTumble 0x6A6200: in every row, the two
/// components about the axis turn (axis 2 = Z: (x, y) -> (c x + s y, c y - s x); 1 = Y: (x, z) -> (c x - s z,
/// c z + s x); 0 = X: (y, z) -> (c y + s z, c z - s y)) = R_axis(-a) * m (on the left). The callers keep c and s with
/// different precisions (0x6A117C stores c as a float, 0x6A627E keeps both), so they are given here
void TurnRows(glm::mat3& m, int axis, double c, double s);
/// The same with c and s of the angle on the FPU stack (AppearanceRuleTumble 0x6A627A..0x6A6284)
void TurnRows(glm::mat3& m, int axis, float a);

/// fn_007FB180(axis, a) 0x7FB180: the Rodrigues matrix by rows (m0 = ((1 - xx) c) + xx, m3 = (xy - xy c) + s z ...,
/// 0x7FB1E7..0x7FB26E) = glm::rotate(-a, axis); the translation 0 (0x7FB273..0x7FB279). `axis` is unit length
[[nodiscard]] glm::mat3 AxisAngle(const glm::vec3& axis, float a);

/// LH3DMath's InverseSquareRoot 0x841170: 1 / sqrt(x) from a 128-byte table of the exponent's last bit and the
/// mantissa's first 6 (index (bits >> 17) & 0x7F, 0x84118E..0x84119D) under the exponent (0x5F000000 - (e << 22)) &
/// 0xFF800000 (0x841179..0x8411A0), then one Newton step ((3 - (x y) y) y) 0.5 ([0x8C2C50] = 3, [0x8AA3B4] = 0.5,
/// 0x8411B0..0x8411C2), every product a float one (the FPU at 24 bits). The table is MakeInverseSqrtLookupTable's
/// (0x8411D0): ((bits(1 / sqrt(x)) + 0x2000) >> 15) & 0xFF for x = bits((i | 0x1F80) << 17) (0.5 <= x < 2), and then
/// entry 0x40 = 0xFF (0x841224). It comes out a little below the true value
[[nodiscard]] float InverseSquareRoot(float x);

/// fn_007FB5C0 0x7FB5C0, in place: each row (glm's column) times InverseSquareRoot 0x841170 of its length squared
/// (0x7FB5E5 / 0x7FB620 / 0x7FB65B), no re-orthogonalisation
void NormaliseRows(glm::mat3& m);

/// LHMatrix::SetInverse 0x7FB290 of an LHMatrix (glm::mat4x3: the 3 rows, then the translation): the adjugate over the
/// determinant, with |det| < 1e-10 [0xC371D4] clamped to +-1e-10 (the sign of det, + for 0; 0x7FB2C8..0x7FB2EE), and
/// the translation -(t A^-1) (0x7FB392..0x7FB3DF)
[[nodiscard]] glm::mat4x3 Inverse(const glm::mat4x3& m);

/// LH3DObject::SetPosition(p, a, s) 0x423140 (vt+0x20 of every LH3DObject): T(p) Ry(-a) S(s). Four branches on a == 0
/// and s == 1 (0x423145 / 0x423151); with a != 0 the rotation is RotateY in place on diag(s) (0x4231B3..0x42321C), so the
/// cells are c s and s s with c and s on the FPU stack. The translation: with s != 1 0 + p (the zeroed cells fadd p,
/// 0x423195..0x4231B0 and 0x423312..0x42332D: -0 becomes +0), with s == 1 p copied (mov, 0x42325A..0x423268)
[[nodiscard]] glm::mat4 SetPosition(const glm::vec3& p, float a, float s);

/// What every Set* of the original writes: the rotation's rows times the scale (the scale multiplies the rows, glm's
/// columns) and the position straight into the translation (0x423195, 0x6382B7, 0x607606): T(p) R S
[[nodiscard]] glm::mat4 Model(const glm::vec3& p, const glm::mat3& r, const glm::vec3& s);
[[nodiscard]] glm::mat4 Model(const ecs::components::Transform& transform);

} // namespace openblack::lh_matrix
