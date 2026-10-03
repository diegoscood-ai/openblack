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

#include <vector>

#include <glm/vec3.hpp>

/// The camera shakes of LH3DLib (runblack.exe W120): LH3DCameraChecker (0x24 bytes, list head g_first [0xEB99A8]).
///
/// - SHAKE_CAMERA 0x6EE0F0 -> PSysGlobal::StartCameraShake 0x68F400 -> LH3DCameraChecker::Create 0x821050.
/// - Once a drawn frame LH3DTech::UpdateCamera 0x819920 (from GCamera::Update 0x442622, outside the citadel and not in a
///   play back) hands the drawn position and focus to fn_008210C0 (the real AdjustCameraPosTarget: bw1-decomp's symbol
///   0x437E70 is another function) before they become g_camera [0xEA1DB8] / [0xEA1DC4]: the shake moves the drawn
///   camera only, never GCamera's zoomers, so it does not build up.
/// - LH3DRender::StartFrame 0x82F270 -> fn_00821270 counts the shakes down by g_delta_time and frees the spent ones.
/// - Other creators, not ported: CameraModeNew3::Update 0x45FC84 (the force field: 100, 1.0, 400 ms), AddSoundToAtom
///   0x69DDBD and fn_006E63C0 0x6E6453.
namespace openblack::camera_shake
{

/// PSysGlobal::StartCameraShake 0x68F40C: seconds x 1000 ([0x8AB228]) to ms
constexpr float k_MsPerSecond = 1000.0f;

/// LH3DCameraChecker (bw1-decomp Lionhead/LH3DLib/development/LH3DCameraChecker.h)
struct Checker
{
	float maxDistance = 0.0f; ///< +0x04: the camera shakes only within it of `point`
	glm::vec3 point {0.0f};   ///< +0x08
	float amplitude = 0.0f;   ///< +0x14
	int32_t totalMs = 0;      ///< +0x18
	int32_t remainingMs = 0;  ///< +0x1C
	bool yOnly = false;       ///< +0x20
};

/// LH3DCameraChecker::Create 0x821050: LH3DMem::Alloc(0x24) zeroed, then put at the head of the list (0x82106D..
/// 0x821078): +4 = max distance, +8 = point, +0x14 = amplitude, +0x18 = +0x1C = ms, +0x20 = y only
void Create(float maxDistance, const glm::vec3& point, float amplitude, int32_t ms, bool yOnly);

/// PSysGlobal::StartCameraShake(point, radius, amplitude, seconds) 0x68F400: Create(radius, point, amplitude,
/// fistp(seconds x 1000), 0). (inferido) fistp under the default round to nearest control word
void StartCameraShake(const glm::vec3& point, float radius, float amplitude, float seconds);

/// fn_008210C0(&position, &target) (LH3DTech::UpdateCamera 0x819A0B): nothing without shakes or with [0xC383B8] == 0
/// (1 in the .data and never written: always on here, inferido). The shake nearest to `lastDrawn` (g_camera, the camera
/// drawn the frame before: fn_004C2B90 subtracts, fn_004A1BA0 is the length; the first of equal ones, i.e. the newest)
/// moves the camera when that distance is below its +4: a = remaining / total x amplitude (0x8211A9..0x8211B7), then
/// Random(-a, a) (?Random@@YAMMM@Z 0x81D180, game_random::crt::Random) on y of both with "y only" (position first), else on
/// the position's z, y, x and the target's z, y, x in that order (0x8211F7..0x821264). No fall-off with the distance
void Adjust(const glm::vec3& lastDrawn, glm::vec3& position, glm::vec3& target);

/// fn_00821270 (LH3DRender::StartFrame 0x82F270): with [0xC383B8] on, each shake's +0x1C -= g_delta_time (the frame's
/// wall ms, 0x821287); <= 0 (jg 0x821298) frees it
void Tick(uint32_t frameMs);

/// Not original: no shakes (a new game, the tests); the original's list only empties by Tick
void Reset();

/// The list, newest first (tests)
[[nodiscard]] const std::vector<Checker>& Checkers();

} // namespace openblack::camera_shake
