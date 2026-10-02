/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCamera.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "EngineConfig.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::script_camera
{

void Vec3Zoomer::SetPosition(const glm::vec3& v)
{
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetPosition(v[i]);
	}
}

void Vec3Zoomer::SetDestination(const glm::vec3& v, float seconds)
{
	// 0x407D60 for x and the same code inline for y and z (0x46147B..0x4616DC, 0x46173B..0x46199C)
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetDestinationWithSpeedAndTime(v[i], 0.0f, seconds);
	}
}

void Vec3Zoomer::Update(float seconds)
{
	for (auto& zoomer : axis)
	{
		zoomer.Update(seconds); // Zoomer::Update 0x442720
	}
}

glm::vec3 Vec3Zoomer::Value() const
{
	return {axis[0].value, axis[1].value, axis[2].value};
}

glm::vec3 Vec3Zoomer::Destination() const
{
	return {axis[0].destination, axis[1].destination, axis[2].destination};
}

State::State()
{
	fov.SetPosition(k_DefaultFov);
}

State::~State() = default;

State& Get()
{
	static State state;
	return state;
}

namespace
{
void DropPath(State& state)
{
	// fn_00461A60: the ScriptedCamera freed (fn_00446AC0) and +0x58 = 0
	state.positionRunner.reset();
	state.track.reset();
}
} // namespace

void Reset()
{
	auto& state = Get();
	DropPath(state);
	state.scriptMode = false;
	state.fov.SetPosition(k_DefaultFov); // GCamera ctor 0x441A78..0x441A83
}

bool Begin(const glm::vec3& origin, const glm::vec3& focus)
{
	auto& state = Get();
	if (state.scriptMode) // GCamera::CantExitCurrentMode 0x441B70 -> CameraModeScript::CanExit 0x461B70
	{
		return false;
	}
	DropPath(state); // ctor 0x461180: +0x58 = 0
	state.scriptMode = true;
	// (aproximado) the original's zoomers are already the drawn camera; openblack's player camera is not built on them
	state.position.SetPosition(origin);
	state.focus.SetPosition(focus);
	return true;
}

bool End()
{
	auto& state = Get();
	const bool wasScript = state.scriptMode;
	if (wasScript) // 0x6ECDBF..0x6ECE2E: Delete (vt+0x30) and a new CameraModeNew3 (0x4572E0)
	{
		state.scriptMode = false;
		DropPath(state);
	}
	SetFov(help::script_control::k_ScriptEndFov, help::script_control::k_ScriptEndFovTime); // 0x6ECE35..0x6ECE48
	return wasScript;
}

bool Active()
{
	return Get().scriptMode;
}

bool Drives()
{
	const auto& control = help::script_control::GetCameraControl();
	return Get().scriptMode && !help::script_control::IsFreeStartTask(control, control.owner);
}

void SetPosition(const glm::vec3& position)
{
	auto& state = Get();
	DropPath(state);
	state.position.SetPosition(position);
}

void SetFocus(const glm::vec3& focus)
{
	auto& state = Get();
	DropPath(state);
	state.focus.SetPosition(focus);
}

void MovePosition(const glm::vec3& position, float seconds)
{
	auto& state = Get();
	DropPath(state);
	state.position.SetDestination(position, seconds);
}

void MoveFocus(const glm::vec3& focus, float seconds)
{
	auto& state = Get();
	DropPath(state);
	state.focus.SetDestination(focus, seconds);
}

void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus)
{
	auto& state = Get();
	state.position.SetPosition(position);
	state.focus.SetPosition(focus);
}

void RunPath(int32_t number)
{
	auto& state = Get();
	DropPath(state); // fn_00461A80: Reset 0x461A30 (frees the old ScriptedCamera)
	state.track = LoadCameraTrack(number); // ScriptedCamera::Create 0x447060 ("Cannot load track No %d" when missing)
	if (state.track != nullptr)
	{
		state.positionRunner = std::make_unique<CameraWayRunner>(state.track->position);
	}
	state.pathMs = 0; // +0x5C
}

void SetFov(float fov, float seconds)
{
	auto& state = Get();
	// GCamera::SetCameraFov 0x443680: t == 0 or t < 0.001 sets it; else the quartic towards fov with speed 0 (inline
	// copy of 0x407D60, 0x4436CB..0x4437D7)
	state.fov.SetDestinationWithSpeedAndTime(fov, 0.0f, seconds);
}

bool ScriptArrived()
{
	const auto& state = Get();
	if (state.track != nullptr) // CameraModeScript::Arrived 0x461B40
	{
		return state.track->position.duration <= state.pathMs;
	}
	// CameraMode::Arrived 0x441700..0x441835
	const auto dp = state.position.Value() - state.position.Destination();
	const auto df = state.focus.Value() - state.focus.Destination();
	return glm::dot(dp, dp) < k_ArrivedDistanceSquared && glm::dot(df, df) < k_ArrivedDistanceSquared;
}

namespace
{
void UpdatePath(State& state, uint32_t gameMs)
{
	// CameraModeScript::UpdatePath 0x461AB0: the game ms of the frame (g_game_time_inc), the sample clamped to the
	// position way's duration, the focus on the focus way at the position Running's segment and parameter
	// (fn_008439C0), and both set at once (GCamera::SetPositionAndFocus 0x4438C0)
	state.pathMs += static_cast<int32_t>(gameMs);
	const auto sample = std::clamp(state.pathMs, 0, state.track->position.duration);
	const auto position = state.positionRunner->Get(sample);
	const auto focus = state.track->focus.Bezier(state.positionRunner->Segment(), state.positionRunner->Parameter());
	SetPositionAndFocus(position, focus);
}
} // namespace

void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	const float dt = std::min(cameraSeconds, k_MaxFrameSeconds); // 0x441FB0..0x441FC1
	if (state.scriptMode && state.track != nullptr)
	{
		UpdatePath(state, gameMs); // CameraModeScript::Update 0x461290
	}
	state.position.Update(dt); // 0x441FEE..0x442029
	state.focus.Update(dt);
	// 0x44222C..0x44232A: the position's destination kept inside the disc of the world
	const glm::vec3 centre(k_DiscCentre, 0.0f, k_DiscCentre);
	const auto d = state.position.Destination() - centre;
	if (const float d2 = glm::dot(d, d); d2 > k_DiscRadiusSquared)
	{
		state.position.SetDestination(d / (std::sqrt(d2) * k_DiscScale) + centre, k_DiscSeconds);
	}
	state.fov.Update(gameSeconds); // 0x4424F6..0x4425C3: g_game_time_inc * 0.001, not the camera's seconds
}

Drawn DrawnCamera(const std::function<float(float, float)>& groundAt)
{
	const auto& state = Get();
	static glm::vec3 s_goodOrigin(1000.0f, 0.0f, 1000.0f); // 0xC59B48
	static glm::vec3 s_goodFocus(1000.0f, 0.0f, 1000.0f);  // 0xC59B38
	Drawn drawn {state.position.Value(), state.focus.Value()};
	// 0x4420D9..0x4421D5: a NaN component -> the last good one
	for (int i = 0; i < 3; ++i)
	{
		drawn.origin[i] = std::isnan(drawn.origin[i]) ? s_goodOrigin[i] : drawn.origin[i];
		drawn.focus[i] = std::isnan(drawn.focus[i]) ? s_goodFocus[i] : drawn.focus[i];
	}
	s_goodOrigin = drawn.origin;
	s_goodFocus = drawn.focus;
	// 0x4421D5..0x442228: position and focus never the same point
	if (const auto d = drawn.origin - drawn.focus; glm::dot(d, d) < k_ArrivedDistanceSquared)
	{
		drawn.origin.x -= 1.0f;
		drawn.origin.y += 1.0f;
	}
	// 0x44242B..0x4424AB, measured under the nudged position (not in CameraModeFree, nor with GCamera+0x78 set: neither
	// exists here)
	if (const float lift = groundAt(drawn.origin.x, drawn.origin.z) + k_GroundClearance - drawn.origin.y; lift > 0.0f)
	{
		drawn.origin.y += lift;
		drawn.focus.y += lift;
	}
	return drawn;
}

bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	Frame(cameraSeconds, gameMs, gameSeconds);

	// 0x442337..0x4423F4: inside the citadel (g_game+0x205A28 == 1, here the temple interior) the drawn camera stays
	const bool insideCitadel = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool drive = Drives() && !insideCitadel;
	if (drive)
	{
		const auto drawn = DrawnCamera([](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z))
			                                           : -1e30f;
		});
		camera.SetOrigin(drawn.origin);
		camera.SetFocus(drawn.focus);
	}

	// LH3DTech::ChangeFov 0x8195B0 with the zoomer's value (0x4425C3; not inside the citadel, 0x4424F6); openblack keeps
	// the field of view in the config's degrees, which every rebuild of the projection reads
	if (!insideCitadel && state.fov.value != state.appliedFov && Locator::config::has_value() &&
	    Locator::windowing::has_value())
	{
		state.appliedFov = state.fov.value;
		auto& config = Locator::config::value();
		config.cameraXFov = state.fov.value == k_DefaultFov ? 70.0f : glm::degrees(state.fov.value);
		camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
		                                      config.cameraNearClip, config.cameraFarClip);
	}
	return drive;
}

} // namespace openblack::script_camera
