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

#include <array>
#include <functional>
#include <memory>

#include <glm/vec3.hpp>

#include "Common/Zoomer.h"

namespace openblack
{
class Camera;
struct CameraTrack;
class CameraWayRunner;
} // namespace openblack

/// The script camera of runblack.exe W120 (docs/bw1-notes/script-camera.md, dev\tmp_dis\camara\original.md): the part of
/// GCamera (0x1D8 bytes) the script moves and the script camera mode CameraModeScript (0x461180, vtable 0x8C7D5C).
///
/// - GCamera keeps the camera in Zoomers (LH3DLib, Common/Zoomer.h): focus x/y/z at +0x88/+0xB8/+0xE8, position at
///   +0x118/+0x148/+0x178 and the field of view (radians) at +0x1A8. GCamera::Update 0x441F80 moves them once a frame.
/// - START_CAMERA_CONTROL pushes a CameraModeScript (fn_00461140); while it lives the player's mode (CameraModeNew3)
///   does not move the camera and no other task gets it (CameraModeScript::CanExit 0x461B70). Releasing it
///   (fn_006ECD70) gives the player a new CameraModeNew3 that starts where the script left the camera.
/// - openblack has no mode stack: the player's mode is Camera + DefaultWorldCameraModel, and this module stands for
///   GCamera's zoomers only while a script mode lives (the player's Camera has its own Zoomer3d, not these: inferred
///   to make no visible difference, the hand-over copies the drawn camera both ways).
namespace openblack::script_camera
{

/// GCamera::SetCameraFov(fn_00443670, 0) in the ctor 0x441A78: fn_00443670 = fld [0x8C762C] = 1.2217305 (70 degrees)
constexpr float k_DefaultFov = 1.2217305f;
/// GCamera::Update 0x441FB0..0x441FC1: the camera's seconds of a frame are at most 0.1 ([0x8AB22C])
constexpr float k_MaxFrameSeconds = 0.1f;
/// CameraMode::Arrived 0x441700 and GCamera::Update 0x4421D5: [0x8AA3B0] = 0.001, a squared distance
constexpr float k_ArrivedDistanceSquared = 0.001f;
/// GCamera::Update 0x44222C..0x44232A: the world's disc around (2560, 0, 2560) ([0x8C7618]); a destination farther
/// than sqrt(1.225e7) = 3500 ([0x8C7614]) is pulled back to d / (|d| * 0.000285796 [0x8C7610]) in 3 s (0x40400000)
constexpr float k_DiscCentre = 2560.0f;
constexpr float k_DiscRadiusSquared = 1.225e7f;
constexpr float k_DiscScale = 0.000285795948f; // 0x3995D6E2
constexpr float k_DiscSeconds = 3.0f;
/// GCamera::Update 0x44242B..0x4424AB: the drawn camera stays 1 m ([0x8AA390]) above the landscape
constexpr float k_GroundClearance = 1.0f;
/// SET_CAMERA_LENS 0x6EE2E0 / MOVE_CAMERA_LENS 0x6EE280: degrees to radians [0x92B20C]
constexpr float k_DegreesToRadians = 0.0174532924f; // 0x3C8EFA35

/// The state: GCamera's zoomers, the script mode and its camera path
struct State
{
	Zoomer3d position; ///< GCamera +0x118
	Zoomer3d focus;    ///< GCamera +0x88
	Zoomer fov;          ///< GCamera +0x1A8, radians
	/// The CameraModeScript is alive (+0x48 = 1 from the ctor 0x461180; Delete 0x4611E0 sets 0)
	bool scriptMode = false;
	/// RUN_CAMERA_PATH: the track (+0x58, ScriptedCamera::Create 0x447060), null = none, and the Running of its
	/// position way (ScriptedCamera +4). The focus way's Running (+8) is never evaluated by UpdatePath 0x461AB0
	std::shared_ptr<const CameraTrack> track;
	std::unique_ptr<CameraWayRunner> positionRunner;
	/// +0x5C: the game ms run along the path (CameraModeScript::UpdatePath 0x461AB0)
	int32_t pathMs = 0;
	/// STORE_CAMERA_DETAILS 0x6EE330: GScript +0x54 (the drawn position) and +0x60 (the drawn focus); not the FOV
	glm::vec3 storedPosition {0.0f};
	glm::vec3 storedFocus {0.0f};
	/// The FOV last given to the renderer (radians). The config's FOV is only rewritten when the zoomer leaves it, so a
	/// player's own FOV stays until a script changes the lens
	float appliedFov = k_DefaultFov;

	State();
	~State();
};

/// The one GCamera of the running game
State& Get();

/// A new map (GCamera ctor 0x441A78: the FOV at 70 degrees at once, no script mode; GScript::Reset 0x6EB2D0)
void Reset();

/// fn_00461140 (START_CAMERA_CONTROL 0x6ECCBA): null when GCamera::CantExitCurrentMode 0x441B70, i.e. a script mode
/// is alive (CameraModeScript::CanExit 0x461B70 = +0x48 == 0; the player's CameraModeNew3 can always be left). The new
/// mode does not touch the zoomers (0x461180, 0x44B800): they start from the drawn camera `origin` / `focus`
/// (aproximado: the original's zoomers keep heading for the destination the player had).
bool Begin(const glm::vec3& origin, const glm::vec3& focus);
/// fn_006ECD70 0x6ECDB1..0x6ECE48: the script mode deleted (vt+0x30) when it is the current one; the FOV back to 70
/// degrees in 0.5 s either way (0x6ECE35). Returns true when a script mode was deleted (the player's mode is then
/// created from where the camera is: CameraModeNew3 0x4572E0 -> Initialise 0x456640)
bool End();
[[nodiscard]] bool Active();
/// A script mode drives the camera: alive, and not the land's opening under mod game.skip-intro's "free start" (not
/// original: that opening keeps its mode but leaves the camera to the player)
[[nodiscard]] bool Drives();

/// CameraModeScript::SetCameraPosition 0x461370 / SetCameraFocus 0x4612B0: the path dropped (fn_00461A60) and the
/// zoomers set (Zoomer::SetPosition)
void SetPosition(const glm::vec3& position);
void SetFocus(const glm::vec3& focus);
/// CameraModeScript::MoveCameraPosition 0x4616F0 / MoveCameraFocus 0x461430: the path dropped and each zoomer heads for
/// the point with speed 0 in `seconds` (camera seconds: the wall clock)
void MovePosition(const glm::vec3& position, float seconds);
void MoveFocus(const glm::vec3& focus, float seconds);
/// GCamera::SetPositionAndFocus 0x4438C0: all six zoomers set (no matter the mode). (aproximado) The original does
/// nothing when both are already the drawn camera (it only clears IsMoving +0x74); here they are set anyway
void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus);
/// CameraModeScript::RunPath fn_00461A80: the track `number` of camera.edt, from its start
void RunPath(int32_t number);
/// GCamera::SetCameraFov 0x443680 (`fov` radians, `seconds` of game time)
void SetFov(float fov, float seconds);

/// CameraModeScript::Arrived 0x461B40: with a path, its duration <= the ms run; else CameraMode::Arrived 0x441700:
/// position and focus both within sqrt(0.001) of their zoomers' destinations
[[nodiscard]] bool ScriptArrived();

/// One frame of GCamera::Update 0x441F80 for the zoomers: the mode's Update (vt+0x08: CameraModeScript::Update 0x461290
/// -> UpdatePath 0x461AB0 with the frame's game ms), then the six zoomers (camera seconds, at most 0.1), the disc of the
/// world, and the FOV zoomer (game seconds, 0x4424F6..0x4425C3). The camera mode's follows (CameraModeFollow::Update
/// 0x44C160) are not ported yet.
void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds);

/// The camera GCamera::Update draws (0x4421D5..0x4424AB): position and focus of the zoomers, the position nudged when
/// they meet, and both lifted so that the (nudged) position stays `k_GroundClearance` above `groundAt(x, z)`. A NaN in
/// the zoomers gives the last good drawn camera (0x4420D9..0x4421D5, statics 0xC59B48 / 0xC59B38, first (1000, 0, 1000))
struct Drawn
{
	glm::vec3 origin;
	glm::vec3 focus;
};
[[nodiscard]] Drawn DrawnCamera(const std::function<float(float, float)>& groundAt);

/// Game.cpp, once a frame instead of the player's Camera::Update when a script mode drives (Drives) and the citadel is
/// not shown (GCamera::Update 0x442337..0x4423F4 keeps the drawn camera there): Frame + DrawnCamera written to the camera. Always: the FOV to the
/// projection when it changed. Returns true when the script drove the camera (the player's model must not)
bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds);

} // namespace openblack::script_camera
