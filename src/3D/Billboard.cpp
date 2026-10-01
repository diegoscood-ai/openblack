/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Billboard.h"

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include "Camera/Camera.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr float k_HalfPi = std::numbers::pi_v<float> * 0.5f; ///< [0x8C78D8] = 1.5707964
constexpr float k_Pi = std::numbers::pi_v<float>;            ///< [0x8C36A0] = 3.1415927
constexpr float k_CellTableScale = 8.0f;                     ///< [0x8C2C70]
/// 0xC390CC / 0xC390DC: the corners' UV steps of an 8-cell row, v0..v3
constexpr std::array<float, 4> k_CornerU = {0.0f, 0.125f, 0.125f, 0.0f};
constexpr std::array<float, 4> k_CornerV = {0.0f, 0.0f, 0.125f, 0.125f};
constexpr float k_MoonScale = 4.0f;         ///< [0xFA2750] after fn_0086A3B0 0x86A3F4
constexpr float k_MoonTilt = -0.13089970f;  ///< [0x9A3BF0], x k = +1 (0x86AEF3..0x86AF0B)
constexpr float k_MoonMeshScale = 0.65f;    ///< [0x8AC420]
constexpr float k_MoonHaloHalf = 500.0f;    ///< [0x8C78EC]
constexpr float k_MoonHaloUv0 = 0.25f;      ///< 0x3E800000 (0x86A981)
constexpr float k_MoonHaloUv1 = 0.49375f;   ///< 0x3EFCCCCD (0x86A996)
constexpr float k_BubbleNearVertical = 1e-4f; ///< the double [0x8C79D8] = (double)1e-4f, and +-[0x8BF518] / [0x8D8738]
} // namespace

billboard::CameraFrame billboard::CameraFrame::From(const Camera& camera)
{
	CameraFrame frame;
	frame.eye = camera.GetOrigin();
	frame.right = camera.GetRight();
	frame.up = camera.GetUp();
	frame.forward = camera.GetForward();
	frame.view = camera.GetViewMatrix(Camera::Interpolation::Current);
	frame.inverseView = glm::inverse(frame.view);
	frame.worldToCamera = glm::mat3(frame.view);
	// glm::perspective (left-handed, depth -1..1): P[2][2] = (f + n) / (f - n), P[3][2] = -2 f n / (f - n)
	const auto& projection = camera.GetProjectionMatrix(Camera::Projection::Normal);
	frame.nearZ = -projection[3][2] / (projection[2][2] + 1.0f);
	const auto basis = glm::mat3(frame.inverseView);
	frame.mist = glm::mat3(basis[0], -basis[2], basis[1]);
	return frame;
}

std::array<glm::vec2, 4> billboard::CellUv(uint8_t cell, uint8_t cellsPerRow)
{
	const uint32_t n = cellsPerRow;
	const uint32_t c = cell & 0x3Fu;
	// 0x8408F9..0x84092F: 1/n, k = (1/n) x 8, col x (1/n), row x (1/n)
	const float inverse = 1.0f / static_cast<float>(n);
	const float k = inverse * k_CellTableScale;
	const glm::vec2 base(static_cast<float>(c % n) * inverse, static_cast<float>(c / n) * inverse);
	std::array<glm::vec2, 4> uv;
	for (size_t i = 0; i < uv.size(); ++i)
	{
		uv[i] = glm::vec2(base.x + k * k_CornerU[i], base.y + k * k_CornerV[i]);
	}
	return uv;
}

billboard::Quad billboard::Screen(const Sprite& sprite, const CameraFrame& frame)
{
	// angle 0: c = 1, s = 0 (0x840770)
	const float c = sprite.angle != 0.0f ? std::cos(sprite.angle) : 1.0f;
	const float s = sprite.angle != 0.0f ? std::sin(sprite.angle) : 0.0f;
	// the images of local x and y: view x = c x + s y, view y = -s x + c y
	const glm::vec3 x = frame.right * c - frame.up * s;
	const glm::vec3 y = frame.right * s + frame.up * c;
	const float hs = sprite.height * sprite.size;
	const float x0 = -sprite.size - sprite.origin.x;
	const float x1 = sprite.size - sprite.origin.x;
	const float y0 = hs - sprite.origin.y;
	const float y1 = -hs - sprite.origin.y;
	const auto& p = sprite.position;
	return {{p + x * x0 + y * y0, p + x * x1 + y * y0, p + x * x1 + y * y1, p + x * x0 + y * y1},
	        CellUv(sprite.cell, sprite.cellsPerRow)};
}

bool billboard::InFrontOfNear(const glm::vec3& position, const CameraFrame& frame)
{
	const float depth = (frame.view * glm::vec4(position, 1.0f)).z;
	return depth > frame.nearZ;
}

billboard::Quad billboard::Horizontal(const Sprite& sprite)
{
	const float c = sprite.angle != 0.0f ? std::cos(sprite.angle) : 1.0f;
	const float s = sprite.angle != 0.0f ? std::sin(sprite.angle) : 0.0f;
	const glm::vec3 x(c, 0.0f, s);
	const glm::vec3 z(-s, 0.0f, c);
	const float hs = sprite.height * sprite.size;
	const float x0 = -sprite.size - sprite.origin.x;
	const float x1 = sprite.size - sprite.origin.x;
	const float z0 = -hs - sprite.origin.y;
	const float z1 = hs - sprite.origin.y;
	const auto& p = sprite.position;
	return {{p + x * x0 + z * z0, p + x * x1 + z * z0, p + x * x1 + z * z1, p + x * x0 + z * z1},
	        CellUv(sprite.cell, sprite.cellsPerRow)};
}

std::optional<billboard::Quad> billboard::SpriteQuad(const Sprite& sprite, const CameraFrame& frame)
{
	if (sprite.horizontal)
	{
		return Horizontal(sprite);
	}
	if (!InFrontOfNear(sprite.position, frame))
	{
		return std::nullopt;
	}
	return Screen(sprite, frame);
}

glm::mat4 billboard::ScreenSpriteModel(const glm::vec3& position, const glm::vec2& halfSize, float angle)
{
	// local x -> (cos, -sin) on the screen is a turn by -angle about the view's Z; angle 0 is no turn (0x840770)
	auto model = glm::translate(glm::mat4(1.0f), position);
	if (angle != 0.0f)
	{
		model = model * glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0.0f, 0.0f, 1.0f));
	}
	return model * glm::scale(glm::mat4(1.0f), glm::vec3(halfSize, 1.0f));
}

billboard::Quad billboard::PlaneOfMatrix(const Sprite& sprite, const glm::mat4& matrix)
{
	glm::vec3 r0(matrix[0]);
	glm::vec3 r2(matrix[2]);
	if (sprite.angle != 0.0f)
	{
		const float c = std::cos(sprite.angle);
		const float s = std::sin(sprite.angle);
		const glm::vec3 turned = c * r0 + s * r2;
		r2 = c * r2 - s * r0;
		r0 = turned;
	}
	const float hs = sprite.height * sprite.size;
	const float x0 = -sprite.size - sprite.origin.x;
	const float x1 = sprite.size - sprite.origin.x;
	const float z0 = -hs - sprite.origin.y;
	const float z1 = hs - sprite.origin.y;
	const glm::vec3 t(matrix[3]);
	return {{t + r0 * x0 + r2 * z0, t + r0 * x1 + r2 * z0, t + r0 * x1 + r2 * z1, t + r0 * x0 + r2 * z1},
	        CellUv(sprite.cell, sprite.cellsPerRow)};
}

float billboard::YawToEyeAngle(const glm::vec3& position, const glm::vec3& eye)
{
	return std::atan2(eye.z - position.z, eye.x - position.x) + k_HalfPi;
}

glm::mat3 billboard::YawToEye(const glm::vec3& position, const glm::vec3& eye)
{
	const float theta = YawToEyeAngle(position, eye);
	const float c = std::cos(theta);
	const float s = std::sin(theta);
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

void billboard::ParticleYaw(glm::mat3& axes, const glm::vec3& position, const glm::vec3& eye, float heightStretch)
{
	// Particle3DObj::DrawAt 0x67A032..0x67A1C3. r2 = the Z row (x, y, z normalised when not all zero; only x, z used), d
	// = position - camera in x, z (normalised when not zero); theta = atan2(d.z, d.x) - atan2(r2.z, r2.x); then per
	// component r0' = cos r0 + sin r2, r2' = cos r2 - sin r0 (a turn in the local XZ plane), and r1 x HeightStretch
	glm::vec3 r2 = axes[2];
	if (r2 != glm::vec3(0.0f))
	{
		r2 /= glm::length(r2);
	}
	glm::vec2 d(position.x - eye.x, position.z - eye.z);
	if (d != glm::vec2(0.0f))
	{
		d /= glm::length(d);
	}
	const float theta = std::atan2(d.y, d.x) - std::atan2(r2.z, r2.x);
	const float c = std::cos(theta);
	const float s = std::sin(theta);
	const glm::vec3 r0 = axes[0];
	axes[0] = c * r0 + s * axes[2];
	axes[2] = c * axes[2] - s * r0;
	axes[1] *= heightStretch;
}

glm::mat3 billboard::FullSprite(const glm::vec3& position, const glm::vec3& eye, float scale)
{
	const glm::vec3 d = eye - position;
	// 0x67A2A3..0x67A34A: the identity x the scale (the translation, 0, too)
	std::array<glm::vec3, 3> rows = {glm::vec3(scale, 0.0f, 0.0f), glm::vec3(0.0f, scale, 0.0f), glm::vec3(0.0f, 0.0f, scale)};
	// 0x67A34D..0x67A369: phi = pi/2 - atan2(d.y, sqrt(d.z^2 + d.x^2))
	const float phi = k_HalfPi - std::atan2(d.y, std::sqrt(d.z * d.z + d.x * d.x));
	const float cp = std::cos(phi);
	const float sp = std::sin(phi);
	for (auto& row : rows) // 0x67A36F..0x67A407
	{
		const float x = row.x;
		row.x = cp * x + sp * row.y;
		row.y = cp * row.y - sp * x;
	}
	// fn_0067A4A0(atan2(d.z, d.x)) 0x67A40A..0x67A41C
	const float psi = std::atan2(d.z, d.x);
	const float cs = std::cos(psi);
	const float ss = std::sin(psi);
	for (auto& row : rows)
	{
		const float x = row.x;
		row.x = cs * x - ss * row.z;
		row.z = ss * x + cs * row.z;
	}
	return {rows[0], rows[1], rows[2]};
}

billboard::LookAt billboard::LookAtCentre(const glm::vec3& position, const glm::vec3& boxCentre, float scale,
                                          const glm::vec3& eye)
{
	const glm::vec3 centre = scale * boxCentre;
	glm::vec3 d = position + centre - eye;
	// 0x518875..0x5188B4: nearly straight above or below, x is pushed off the vertical so that U exists. After it d and
	// U cannot be zero, so the original's all-zero test (0x5188BC..0x5188ED, only reached when |d.x| or |d.z| >= 1e-4)
	// never fires and is not ported
	if (std::abs(d.x) < k_BubbleNearVertical && std::abs(d.z) < k_BubbleNearVertical)
	{
		d.x = d.x > 0.0f ? k_BubbleNearVertical : -k_BubbleNearVertical;
	}
	const glm::vec3 direction = glm::normalize(d);
	const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f) - glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), direction) * direction;
	const glm::vec3 u = glm::normalize(up);
	LookAt result;
	result.axes = glm::mat3(glm::cross(u, direction), -direction, u);
	result.offset = centre - result.axes * centre;
	return result;
}

glm::mat3 billboard::BandToEye(const glm::vec3& position, const glm::vec3& eye)
{
	glm::vec3 d = position - eye;
	// 0x51A8C2..0x51A90F: the same push off the vertical as the bubble's
	if (std::abs(d.x) < k_BubbleNearVertical && std::abs(d.z) < k_BubbleNearVertical)
	{
		d.x = d.x > 0.0f ? k_BubbleNearVertical : -k_BubbleNearVertical;
	}
	// 0x51A948..0x51A988
	const float inverse = 1.0f / std::sqrt(d.y * d.y + d.z * d.z + d.x * d.x);
	const glm::vec3 direction = d * inverse;
	// 0x51A9CD..0x51AA3B: k = -(Y.D), U' = k D + Y
	constexpr glm::vec3 k_Y(0.0f, 1.0f, 0.0f);
	const float k = -(k_Y.y * direction.y + k_Y.z * direction.z + k_Y.x * direction.x);
	const glm::vec3 up = k * direction + k_Y;
	// 0x51AA7A..0x51AAB2
	const glm::vec3 u = up * (1.0f / std::sqrt(up.z * up.z + up.y * up.y + up.x * up.x));
	// 0x51AABC..0x51AB2A: U x D (m2 = U.y D.z - U.z D.y, m5 = U.z D.x - U.x D.z, m8 = U.x D.y - U.y D.x)
	return glm::mat3(-direction, u, glm::cross(u, direction));
}

glm::mat3 billboard::MoonBasis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position)
{
	const glm::vec3 v(view * glm::vec4(position, 1.0f)); // 0x86AC67..0x86ACEB
	glm::vec3 n = v;
	if (v != glm::vec3(0.0f)) // 0x86AD26..0x86AD58
	{
		n = v * (1.0f / std::sqrt(v.z * v.z + v.y * v.y + v.x * v.x));
	}
	glm::vec3 t(n.z, 0.0f, -n.x); // 0x86AD62..0x86AD7E
	if (t != glm::vec3(0.0f))     // 0x86ADAF..0x86ADD7
	{
		t = t * (1.0f / std::sqrt(t.z * t.z + t.y * t.y + t.x * t.x));
	}
	const glm::vec3 u = glm::cross(n, t); // 0x86ADDB..0x86AE4F: m3 = ny tz - nz ty, m4 = nz tx - nx tz, m5 = nx ty - ny tx
	return glm::mat3(inverseView) * glm::mat3(t, u, n) * k_MoonScale;
}

glm::mat4 billboard::MoonModel(const glm::mat3& basis, const glm::vec3& position, float phase)
{
	glm::vec3 r0 = basis[0];
	glm::vec3 r1 = basis[1];
	glm::vec3 r2 = basis[2];
	// fn_0086AFA0(alpha) 0x86AFA0..0x86B008
	const float ca = std::cos(k_MoonTilt);
	const float sa = std::sin(k_MoonTilt);
	const glm::vec3 tilted = ca * r0 - sa * r1;
	r1 = sa * r0 + ca * r1;
	r0 = tilted;
	// LHMatrix::RotateY(phase + pi) 0x5198F0
	const float b = phase + k_Pi;
	const float cb = std::cos(b);
	const float sb = std::sin(b);
	const glm::vec3 turned = cb * r0 + sb * r2;
	r2 = cb * r2 - sb * r0;
	r0 = turned;
	glm::mat4 model(glm::mat3(r0, r1, r2) * k_MoonMeshScale); // 0x86AF30..0x86AF97
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

billboard::Quad billboard::MoonHalo(const glm::mat3& basis, const glm::vec3& position)
{
	const glm::vec3& r0 = basis[0];
	const glm::vec3& r1 = basis[1];
	return {{position - (r1 + r0) * k_MoonHaloHalf, (r0 * k_MoonHaloHalf - r1 * k_MoonHaloHalf) + position,
	         (r1 * k_MoonHaloHalf - r0 * k_MoonHaloHalf) + position, (r1 + r0) * k_MoonHaloHalf + position},
	        {glm::vec2(k_MoonHaloUv0, k_MoonHaloUv0), glm::vec2(k_MoonHaloUv1, k_MoonHaloUv0),
	         glm::vec2(k_MoonHaloUv0, k_MoonHaloUv1), glm::vec2(k_MoonHaloUv1, k_MoonHaloUv1)}};
}

const glm::mat3& billboard::MistBasis(const CameraFrame& frame)
{
	return frame.mist;
}

float billboard::MistShrunkSize(float size, float k, const glm::vec3& toMist)
{
	const float length2 = glm::dot(toMist, toMist);
	if (length2 == 0.0f)
	{
		return size; // (inferido) a guard for d = 0 only: the original's InverseSquareRoot would give 0 x inf
	}
	// (aproximado) std::sqrt in place of InverseSquareRoot 0x841170 (the 128-byte table of MakeInverseSqrtLookupTable
	// 0x8411D0 and one Newton step, 0x8411B0..0x8411C2)
	return size / (1.0f + (k - 1.0f) * (1.0f - std::abs(toMist.y) / std::sqrt(length2)));
}

float billboard::ScreenVelocity(const glm::vec3& w, const glm::vec3& right, const glm::vec3& up)
{
	const float x = glm::dot(w, right);
	const float y = glm::dot(w, up);
	return std::atan2(-y, x) + k_HalfPi;
}

std::optional<glm::vec3> billboard::RibbonSide(const glm::vec3& segment, const glm::vec3& joint, const glm::vec3& eye)
{
	const auto side = glm::cross(glm::normalize(segment), glm::normalize(joint - eye));
	if (glm::length(side) < 1e-4f)
	{
		return std::nullopt;
	}
	return glm::normalize(side);
}

float billboard::RibbonHalfWidth(float scale)
{
	return scale;
}

glm::mat3 billboard::VolBlend(const glm::vec3& axisX, const glm::vec3& position, const glm::vec3& eye, float scale)
{
	const auto normalised = [](const glm::vec3& v) { return v != glm::vec3(0.0f) ? glm::normalize(v) : v; };
	const glm::vec3 a = normalised(axisX);                  // 0x67CCBB..0x67CD5E
	const glm::vec3 d = normalised(eye - position);         // 0x67CD95..0x67CE45
	const glm::vec3 b = normalised(glm::cross(a, d));       // 0x67CEA2..0x67CF53
	const glm::vec3 c = glm::cross(d, b);                   // 0x67CF57..0x67CF99
	return glm::mat3(c, b, d) * scale;                      // 0x67CFAD..0x67D013 (fn_00518B90)
}
