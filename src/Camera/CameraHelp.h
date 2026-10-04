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

/// CameraHelp's statics (runblack.exe W120): the camera features the player may use (EnabledFeatures [0x9CDD6C]) and the
/// auto-pitch parameters ([0x9CDD64] / [0x9CDD68]). SET_INTERFACE_INTERACTION 0x70B220, GScript::CleanGameForScriptReboot
/// 0x6EBCAE and GGame::ClearMap write them. Research: dev\documentacion\intro\spec_misc_ops.md §063.
///
/// (pending, camera owner: on its list) only the storage is ported: DefaultWorldCameraModel does not read the bits
/// yet. The readers in the original: HandStateCamera::Update (bits 0x1, 0x2, 0x4), HandStateHolding::Update (0x1, 0x2),
/// GInterface::CalculateCanSelectLock (0x1), fn_005D6980 (0x4), fn_005C9D00 (0x2), CameraModeNew3::GetCameraFeatures.
/// (pending) their save and load with the game (GGame::Save).
namespace openblack::camera_help
{

/// The bits by the SCRIPT_INTERFACE_LEVEL names that set them (inferred: consistent over the 16 levels of 0x70B7A8;
/// pending until the readers are read)
enum class Feature : int32_t
{
	Pitch = 0x01,          ///< JUST_PITCH
	Rotate = 0x02,         ///< JUST_ROTATE
	Zoom = 0x04,           ///< JUST_ZOOM (with 0x20; pending which zoom path each is)
	GrabLand = 0x08,       ///< JUST_GRAB
	DoubleClickFly = 0x10, ///< JUST_DOUBLE_CLICK_AND_DRAG (with 0x08)
	Zoom2 = 0x20,          ///< JUST_ZOOM (with 0x04)
	AutoPitch = 0x40,      ///< fn_004473F0
};
/// The bit of a Feature in EnabledFeatures
[[nodiscard]] constexpr int32_t Bit(Feature feature)
{
	return static_cast<int32_t>(feature);
}
/// [0x9CDD6C]'s initial value, ClearMap's and SET_INTERFACE_INTERACTION(NORMAL)'s (0x80 / 0x100: only here, pending)
constexpr int32_t k_NormalFeatures = 0x1BF;
/// [0x9CDD64] / [0x9CDD68] initial values (pi / 6, 75)
constexpr float k_DefaultAutoPitchParam1 = 0.523599f;
constexpr float k_DefaultAutoPitchParam2 = 75.0f;

/// CameraHelp::EnableCameraFeatures 0x447430: EnabledFeatures = (EnabledFeatures & ~mask) | features. Every
/// SET_INTERFACE_INTERACTION level passes mask -1: the whole set is replaced (auto-pitch included)
void EnableCameraFeatures(int32_t features, int32_t mask);
/// fn_004473F0(param1, param2, on): EnableCameraFeatures(on ? 0x40 : 0, on ? 0 : 0x40), then AutoPitchParam1 / 2
void SetAutoPitch(float param1, float param2, bool on);
[[nodiscard]] int32_t GetEnabledFeatures();
[[nodiscard]] bool IsFeatureEnabled(Feature feature);
[[nodiscard]] float GetAutoPitchParam1();
[[nodiscard]] float GetAutoPitchParam2();
/// The executable's initial values (openblack: a new game, the tests)
void Reset();

} // namespace openblack::camera_help
