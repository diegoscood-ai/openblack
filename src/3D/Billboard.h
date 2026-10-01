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
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class Camera;
}

/// Everything that is turned to the camera, one function per mode of the original (wiki: rendering-objects.md,
/// "Objetos que miran a la cámara"). Pure math: no rendering state.
///
/// Conventions. LH3D uses row vectors (p' = p M, fn_0084BA90: x' = m0 x + m3 y + m6 z + m9): row k of an LHMatrix is
/// the image of local axis k and m9..m11 the translation. glm uses column vectors, so column k here is row k there,
/// with the same memory. LH3D's rotations turn the other way from glm::rotate: SetAngleY(a) 0x674360 (rows (c, 0, s),
/// (0, 1, 0), (-s, 0, c)) is glm::rotate(-a, Y), and the in-place rotations (RotateY 0x5198F0, fn_0086AFA0,
/// UpdateRuleRotatePrincipalAxis 0x6A1150) are glm::rotate(-a, axis) on the left of the matrix.
namespace openblack::graphics::billboard
{

/// LH3DTech's camera of one pass, built once from the camera being drawn (the main one, or the mirrored one of the
/// reflection pass)
struct CameraFrame
{
	// eye, right, up and forward come from Camera::GetOrigin / GetRotationMatrix: in the reflection pass they stay the
	// main camera's (ReflectionXZCamera only mirrors GetViewMatrix), while view, inverseView, worldToCamera and mist are
	// mirrored. Do not mix the two groups in that pass
	glm::vec3 eye {0.0f};        ///< g_camera 0xEA1DB8
	glm::vec3 right {1.0f, 0.0f, 0.0f};   ///< Camera::GetRight (the transpose of the view rotation): screen right
	glm::vec3 up {0.0f, 1.0f, 0.0f};      ///< Camera::GetUp: screen up
	glm::vec3 forward {0.0f, 0.0f, 1.0f}; ///< Camera::GetForward: away from the eye (lookAtLH)
	/// W2C 0xEA1D28..0xEA1D54 (UpdateWorldToCamera 0x819690): columns (R, U, D) and -(R.eye, U.eye, D.eye); in
	/// openblack the view matrix, glm::lookAtLH (x right, y up, z forward, as the original's camera space)
	glm::mat4 view {1.0f};
	glm::mat4 inverseView {1.0f}; ///< glm::inverse(view): SetInverse(W2C) (LHMatrix::SetInverse 0x7FB290)
	glm::mat3 worldToCamera {1.0f}; ///< the W2C rotation, mat3(view)
	/// The near plane [0xE839E0], read back from the camera's projection. (aproximado) Game.cpp computes it as
	/// LandFeature::GetNearClipping 0x5E2F30 does but only re-sets the projection when it moves by more than 0.01, so it
	/// can lag [0xE839E0] by up to 0.01
	float nearZ {1.0f};
	/// 0xEA1C98 (UpdateCamera 0x819A62..0x819AF3, fn_00819F50 0x81A1AD..0x81A265): W2C with its columns swizzled to
	/// rows (A[3i], -A[3i+2], A[3i+1]) then inverted in place by fn_007FB3F0, so rows R, -D, U: in glm
	/// mat3(right, -forward, up). Taken from the columns of inverseView, as the mists and clouds always did (the
	/// transpose and the inverse of the view rotation agree to rounding)
	glm::mat3 mist {1.0f};

	[[nodiscard]] static CameraFrame From(const Camera& camera);
};

/// The useful fields of LH3DSprite (0x34 bytes; defaults of SetToZero 0x8404F0)
struct Sprite
{
	glm::vec3 position {0.0f};   ///< +0x00
	float size {1.0f};           ///< +0x0C the half width
	float height {1.0f};         ///< +0x10 the stretch: half height = size x height
	float angle {0.0f};          ///< +0x14 Screen: the roll on the screen; Horizontal: the yaw
	glm::vec2 origin {0.0f};     ///< +0x18 / +0x1C, world units, subtracted from the local corners
	uint32_t argb {0xFFFFFFFFu}; ///< +0x20 the colour of the four vertices (+0x24 the specular, not used here)
	uint8_t cell {0};            ///< +0x28 bits 0-5
	bool horizontal {false};     ///< +0x28 bit 0x40 (SetHorozontal 0x6AA093, InitialiseCircles 0x54BA84, fn_00824740 0x8247EF)
	uint8_t cellsPerRow {8};     ///< +0x30 (a byte)
};

/// Four world-space corners in LH3DSprite::Draw's order v0..v3 and their UVs. Screen: top left, top right, bottom
/// right, bottom left. Horizontal: (-x, -z), (+x, -z), (+x, +z), (-x, +z) of the sprite's own axes.
struct Quad
{
	std::array<glm::vec3, 4> corners;
	std::array<glm::vec2, 4> uv;
};
/// The two triangles of LH3DSprite::Draw (0x840B47 / 0x840B57)
inline constexpr std::array<int, 6> k_SpriteTriangles = {0, 1, 2, 0, 2, 3};

/// LH3DSprite::Draw 0x8408D3..0x84092F: col = cell % n, row = cell / n, u = col (1/n) + (8/n) {0, .125, .125, 0}[i]
/// (0xC390CC), v = row (1/n) + (8/n) {0, 0, .125, .125}[i] (0xC390DC), 8 = [0x8C2C70]
[[nodiscard]] std::array<glm::vec2, 4> CellUv(uint8_t cell, uint8_t cellsPerRow);

/// Mode A, LH3DSprite::Draw 0x840530 with flag 0x40 clear: the quad lies in the plane of the screen at the sprite's
/// depth (parallel to the screen, not turned to the eye). Local x = {-s - ox, s - ox}, y = {hs - oy, -hs - oy}
/// (0x840831..0x8408CF); with the angle a, local x goes to (cos a, -sin a) on the screen and local y to (sin a, cos a)
/// (0x84071D..0x84082B: m0 = c fx, m1 = -s fy, m3 = s fx, m4 = c fy). No near test (see SpriteQuad).
[[nodiscard]] Quad Screen(const Sprite& sprite, const CameraFrame& frame);
/// 0x84055D..0x840585: mode A draws nothing when the sprite's depth in the camera is at or before the near plane
[[nodiscard]] bool InFrontOfNear(const glm::vec3& position, const CameraFrame& frame);
/// Mode B, flag 0x40 (0x8405FE..0x840704): Ry(angle) with rows (c, 0, s) / (0, 1, 0) / (-s, 0, c) and the position,
/// the quad in the local XZ plane: x = {-s - ox, s - ox}, z = {-hs - oy, hs - oy} (0x84085D). Ignores the camera.
[[nodiscard]] Quad Horizontal(const Sprite& sprite);
/// LH3DSprite::Draw as a whole: Horizontal with flag 0x40, otherwise Screen, nothing when Screen fails the near test
[[nodiscard]] std::optional<Quad> SpriteQuad(const Sprite& sprite, const CameraFrame& frame);

/// The vs_sprite.sc model matrix of a Screen sprite without origin offset: T(position) Rz(-angle) S(half width, half
/// height, 1) (no turn when the angle is 0, 0x840770). The shader adds u_invView x (model x (x, y, 0, 0)) to the
/// translation, on the plane -1..1 with v = 0 at the top: that is Screen with ox = oy = 0
[[nodiscard]] glm::mat4 ScreenSpriteModel(const glm::vec3& position, const glm::vec2& halfSize, float angle);

/// LH3DSprite::DrawSpecial1 0x840CC0: the quad of mode B in the XZ plane of a given matrix (columns = its rows), turned
/// about its local Y by the angle when it is not 0 (0x840CEF..0x840D82: r0' = c r0 + s r2, r2' = c r2 - s r0). The
/// sprite's own position is not used. (The original's matrix already includes world to clip; here it is the world
/// matrix.)
[[nodiscard]] Quad PlaneOfMatrix(const Sprite& sprite, const glm::mat4& matrix);

/// Mode C, the inline billboards (TownCentre::DrawPSys 0x69BE76..0x69BE8A, fn_00466BB0, TownDesireFlags::Draw
/// 0x746BFC, fn_00719E90, ScriptHighlight::Draw): theta = atan2(eye.z - p.z, eye.x - p.x) + pi/2 ([0x8C78D8]) and the
/// axes Ry(theta) (SetAngleY's layout): local +Z points from the eye to the object in XZ. No user is ported yet.
[[nodiscard]] float YawToEyeAngle(const glm::vec3& position, const glm::vec3& eye);
[[nodiscard]] glm::mat3 YawToEye(const glm::vec3& position, const glm::vec3& eye);

/// Particle3DObj::DrawAt 0x679FD0 with FaceCamera (+0x4D), 0x67A032..0x67A1C3: the frame turned about its local Y so
/// that it faces the camera in x, z, and its Y axis x HeightStretch (axes = the LHMatrix rows as columns)
void ParticleYaw(glm::mat3& axes, const glm::vec3& position, const glm::vec3& eye, float heightStretch);

/// Particle3DObj::DrawAt with FaceCameraSprite (+0x4C), 0x67A250..0x67A451: the identity x the scale, then every row
/// (x, y) := (cos phi x + sin phi y, cos phi y - sin phi x) with phi = pi/2 - atan2(d.y, |d.xz|) (0x67A367), then
/// fn_0067A4A0(psi): (x, z) := (cos psi x - sin psi z, sin psi x + cos psi z), psi = atan2(d.z, d.x), d = eye - p.
/// Local +Y points at the eye, local Z stays horizontal. The PSR's rotation and stretch are dropped. No spell file
/// sets FaceCameraSprite.
[[nodiscard]] glm::mat3 FullSprite(const glm::vec3& position, const glm::vec3& eye, float scale);

/// The miracle bubble, fn_00518720 (called from OneOffSpellSeed::Draw 0x518E90)
struct LookAt
{
	glm::mat3 axes;    ///< mat3(U x D, -D, U): local +Y towards the eye
	glm::vec3 offset;  ///< drawn = axes (scale v) + position + offset: the pivot is the box centre
};
/// fn_00518720: W = the box centre in the world (0x518746..0x5187B8, the object's matrix: position + scale x centre for
/// the orb, which has no rotation), d = W - eye (0x518834..0x518869); when |d.x| and |d.z| are both under 1e-4 (the
/// double [0x8C79D8]) d.x is replaced by +1e-4 ([0x8BF518]) when it is above 0, else -1e-4 ([0x8D8738]); D =
/// normalize(d), U = normalize(Y - (Y.D) D) (Y the static (0, 1, 0) at 0xCC62D0); the matrix with columns
/// (U x D, -D, U) is inverted in place by fn_007FB3F0 (0x518B0C), so its rows are U x D, -D, U; then M = T(-c) R
/// (fn_007FAFF0), x the scale (fn_00518B90, 9 cells), translation W - c R s (fn_00518BF0, fn_0044CF90). After the 1e-4
/// push d and U are never zero, so the original's zero test (0x5188BC..0x5188ED) cannot fire and has no port.
[[nodiscard]] LookAt LookAtCentre(const glm::vec3& position, const glm::vec3& boxCentre, float scale, const glm::vec3& eye);

/// The power-up bands of a spell seed graphic, fn_0051A830 (on while the byte [0xBE8E8E] is set: 1, never written), called
/// by DrawSpellGraphic 0x51A773 with the band's matrix: its translation T is taken out (0x51A848..0x51A85F), d = T - eye
/// (g_camera 0xEA1DB8), pushed off the vertical as the bubble's (|d.x| and |d.z| under 1e-4, the double [0x8C79D8]: d.x =
/// +1e-4 [0x8BF518] when above 0, else -1e-4 [0x8D8738]), D = d / sqrt(d.y^2 + d.z^2 + d.x^2), U' = Y - (Y.D) D with Y
/// the static (0, 1, 0) at 0xCC62C0, U = U' / sqrt(U'.z^2 + U'.y^2 + U'.x^2); the matrix with columns (-D, U, U x D)
/// (0x51AABC..0x51AB56) inverted in place by fn_007FB3F0 (0x51AB5A), so its rows are -D, U, U x D; then the band's
/// 9 cells M = M R (fn_0046D9D0: each row times R) and T put back (0x51AB6A..0x51AB7C). In glm terms: R (the returned
/// axes, columns -D, U, U x D) times the band's own rotation and scale. After the push the zero tests (0x51A911..
/// 0x51A942, 0x51AA3F..0x51AA78) cannot fire and are not ported
[[nodiscard]] glm::mat3 BandToEye(const glm::vec3& position, const glm::vec3& eye);

/// fn_0086AC60 0x86AC67..0x86AEBD: v = position in the camera, n = normalize(v), t = normalize(n.z, 0, -n.x), u = n x t;
/// the halo's matrix (t, u, n) in the camera space goes to the world with SetInverse(W2C) (0x86AE64, fn_007FAFF0
/// 0x86AE6F) and its 9 rotation cells x [0xFA2750] (4.0: fn_0086A3B0 writes 3.0 at 0x86A3D6, then 4.0 at 0x86A3F4).
/// Local +Z points from the eye to the moon, local X is horizontal in the view. @return the axes (columns)
[[nodiscard]] glm::mat3 MoonBasis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position);
/// The moon mesh's matrix, fn_0086AC60 0x86AEC6..0x86AF97: the halo's (fn_005FEDA0), tilted by fn_0086AFA0(alpha)
/// (r0' = cos a r0 - sin a r1, r1' = sin a r0 + cos a r1) with alpha = [0x9A3BF0] = -0.1309 (the drawn moon is the
/// [0xFA2774] = 0 call: k = +1), then RotateY(phase + pi) 0x5198F0 ([0x8C36A0]; r0' = c r0 + s r2, r2' = c r2 - s r0),
/// then x 0.65 ([0x8AC420], 9 cells). In glm terms: basis Rz(+7.5 deg) Ry(-(phase + pi)) S(0.65).
[[nodiscard]] glm::mat4 MoonModel(const glm::mat3& basis, const glm::vec3& position, float phase);
/// fn_0086A930: the halo's corners v0 = p - 500 (r0 + r1), v1 = p + 500 (r0 - r1), v2 = p + 500 (r1 - r0),
/// v3 = p + 500 (r0 + r1) (0x86AA3C..0x86AB67, 500 = [0x8C78EC], the rows already x 4: half width 2000) and the UVs of
/// 0xEDC304: (0.25, 0.25), (0.49375, 0.25), (0.25, 0.49375), (0.49375, 0.49375) (0.49375 = 0x3EFCCCCD, 0x86A996)
[[nodiscard]] Quad MoonHalo(const glm::mat3& basis, const glm::vec3& position);
/// The halo's triangles (0xEDC310, 0x86A933..0x86A974)
inline constexpr std::array<int, 6> k_MoonHaloTriangles = {0, 1, 3, 3, 2, 0};

/// LH3DMist::Draw fn_007FA300 0x7FA38F: the mist's 9 cells are 0xEA1C98 (CameraFrame::mist)
[[nodiscard]] const glm::mat3& MistBasis(const CameraFrame& frame);
/// fn_007FA300's effect branch (+0x80 & 2) 0x7FA483..0x7FA539: row 0 keeps the size, rows 1 and 2 take
/// size / (1 + (k - 1)(1 - |d.y| / |d|)), d = mist - eye, with no clamp. (aproximado) 1 / |d| by std::sqrt, not the
/// table of InverseSquareRoot 0x841170; (inferido) d = 0 returns the size.
[[nodiscard]] float MistShrunkSize(float size, float k, const glm::vec3& toMist);

/// UR_OrientSpriteWithVelocity::ModifyAtomCore 0x69A790 (0x69A8ED..0x69A94B) and fn_006840E0 (UR_Flocking): the vector
/// w in the camera, x = A0 w.x + A3 w.y + A6 w.z = w.right, y = A1 w.x + A4 w.y + A7 w.z = w.up (the W2C rotation),
/// then SetAngleY(atan2(-y, x) + pi/2) ([0x8C78D8]). Screen then puts the sprite's +y along (x, y). @return the angle
[[nodiscard]] float ScreenVelocity(const glm::vec3& w, const glm::vec3& right, const glm::vec3& up);

/// The chains' ribbon, fn_0067B3F0 (reads g_camera at 0x67B4BA): the side of each end of a segment is
/// (eye - joint) x (tail - head) (0x67B86C..0x67B924, the view from that joint, head then tail), normalised
/// (0x67B943 InverseSquareRoot, whose table error the fsqrt of 0x67BA57 then takes out, see RibbonHalfWidth): the same
/// direction as normalize(cross(normalize(segment), normalize(joint - eye))); nothing when either is (almost) zero
/// (openblack's 1e-4 guard)
[[nodiscard]] std::optional<glm::vec3> RibbonSide(const glm::vec3& segment, const glm::vec3& joint, const glm::vec3& eye);
/// The ribbon's half width, fn_0067B3F0 0x67B9E6..0x67BA70: each vertex is joint +- side x (joint +0xC) / |side|, the
/// joint's +0xC the PSR scale (ParticleChainJoint::DrawAt 0x679E9A copies DrawData +4 -> +0x30), so the scale itself
[[nodiscard]] float RibbonHalfWidth(float scale);

/// RenderParticleVolBlendMesh::DrawAt 0x67CCB0 (0x67CCBB..0x67D013, [0xC029C4] = 1 so always on): a = normalize(the
/// PSR's row 0), d = normalize(eye - p), b = normalize(a x d), c = d x b (fn_00460710 then runs on c: (inferido) a
/// normalisation, a no-op on that unit vector); rows c, b, d x the PSR scale. No spell file uses
/// ParticleVolBlendMeshCreator.
[[nodiscard]] glm::mat3 VolBlend(const glm::vec3& axisX, const glm::vec3& position, const glm::vec3& eye, float scale);

} // namespace openblack::graphics::billboard
