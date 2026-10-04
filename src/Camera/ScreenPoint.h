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

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

/// LH3DTech's screen <-> 3D functions, which work with the depth along the camera's forward axis (camera space z), not
/// with a ray length: Get3DPointFromScreen 0x81B370 and the projection of LH3DSprite::Draw 0x840930..0x8409D0 /
/// ProjectPoint 0x819390. Shared by the falling spell (Magic/Objects/FallingSpell.cpp, in its own camera space) and
/// the advisor spirits (Help/SpiritsRuntime.cpp, in the world through the drawn camera). Pure maths, no Locator.
///
/// The lens: ChangeFov 0x8195B0 / UpdateViewPort 0x81909C: T = tan(fov / 2) (0x8195B8..0x8195D7), fx [0xE83A00] = 1 / T,
/// fy [0xE83A04] = aspect / T with aspect [0xE839EC] = W / H; Get3DPointFromScreen uses [0xC3812C] = near T and
/// [0xC38130] = near T / aspect.
namespace openblack::screen_point
{

struct Lens
{
	float halfW;
	float halfH;
	float tanHalf;
	float aspect;
};

/// The lens of a W x H screen and a horizontal field of view in radians (Camera::GetHorizontalFieldOfView)
[[nodiscard]] Lens LensOf(int width, int height, float horizontalFov);

/// Get3DPointFromScreen 0x81B370 in camera space: ((x - hW) near T / hW, (hH - y) near T / aspect / hH) x depth / near,
/// z = depth (the camera's rotation and g_camera then take it to the world, 0x81B3BE..0x81B43A). (approximate) the near
/// cancels out; the original's rounding goes through it
[[nodiscard]] glm::vec3 CameraPointFromScreen(const Lens& lens, int32_t x, int32_t y, float depth);

/// LH3DSprite::Draw 0x840930..0x8409D0 without clipping: sx = (X / Z + 1) hW, sy = hH - Y / Z hH, X = fx x, Y = fy y
[[nodiscard]] glm::vec2 ProjectCamera(const Lens& lens, const glm::vec3& v);

/// The camera-space point of a world point: worldToCamera (p - eye) (the W2C 0xEA1D28 of UpdateWorldToCamera 0x819690)
[[nodiscard]] glm::vec3 ToCamera(const graphics::billboard::CameraFrame& frame, const glm::vec3& world);
/// The world point of a camera-space point: eye + right x + up y + forward z (0x81B3BE..0x81B43A)
[[nodiscard]] glm::vec3 ToWorld(const graphics::billboard::CameraFrame& frame, const glm::vec3& camera);

/// Get3DPointFromScreen 0x81B370 in the world: the pixel at that camera depth
[[nodiscard]] glm::vec3 PointFromScreen(const graphics::billboard::CameraFrame& frame, const Lens& lens, int32_t x,
                                        int32_t y, float depth);

/// What LH3DTech::ProjectPoint 0x819390 gives: the pixel and the camera depth. (inferred) nullopt for a point at or
/// behind the eye (z <= 0), where the division would fail; the original's other outputs are not read
struct Projected
{
	glm::vec2 pixel {0.0f};
	float depth {0.0f};
};
[[nodiscard]] std::optional<Projected> Project(const graphics::billboard::CameraFrame& frame, const Lens& lens,
                                               const glm::vec3& world);

} // namespace openblack::screen_point
