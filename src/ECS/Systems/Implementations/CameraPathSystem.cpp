/******************************************************************************
 * Copyright (c) 2018-2024 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraPathSystem.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CameraPathControl.h"
#include "Camera/ScriptCamera.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;

CameraPathSystem::CameraPathSystem()
    : CameraPathSystem([] { return !script_camera::ScriptModeCurrent(); })
{
}

CameraPathSystem::CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode)
    : _canLeaveCameraMode(std::move(canLeaveCameraMode))
{
}

void CameraPathSystem::Start(entt::id_type id)
{
	_path = Locator::resources::value().GetCameraPaths().Handle(id);
	if (!_path)
	{
		return;
	}

	_elapsed = std::chrono::microseconds::zero();
	_state = CameraPathState::PLAYING;
}

void CameraPathSystem::Stop()
{
	_placed.clear();
	_state = CameraPathState::STOPPED;
	_elapsed = std::chrono::microseconds::zero();
	_path = entt::resource<CameraPath>();
}

bool CameraPathSystem::Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement, float pauseSeconds)
{
	if (!path || path->GetPoints().empty() || !Locator::camera::has_value())
	{
		return false;
	}
	// A script's camera cannot be left: the path gives up, and is not asked for again
	if (!_canLeaveCameraMode())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a script holds the camera, a miracle's path gave up");
		return false;
	}
	// An owner places one path
	if (std::ranges::any_of(_placed, [owner](const Placed& placed) { return placed.owner == owner; }))
	{
		return false;
	}
	_placed.push_back(Placed {.owner = owner, .path = std::move(path), .placement = placement, .pauseSeconds = pauseSeconds});
	return true;
}

void CameraPathSystem::FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing)
{
	const auto placed = std::ranges::find_if(_placed, [owner](const Placed& candidate) { return candidate.owner == owner; });
	if (placed != _placed.end())
	{
		placed->pathMilliseconds = pathMilliseconds;
		placed->playing = playing;
	}
}

void CameraPathSystem::Release(PathOwner owner)
{
	if (std::erase_if(_placed, [owner](const Placed& placed) { return placed.owner == owner; }) != 0)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path let the camera go");
	}
}

std::optional<CameraPathSystem::PlacedReadout> CameraPathSystem::CurrentPlaced() const
{
	if (_placed.empty())
	{
		return std::nullopt;
	}
	const auto& placed = _placed.back();
	return PlacedReadout {.owner = placed.owner,
	                      .pathMilliseconds = placed.pathMilliseconds,
	                      .playing = placed.playing,
	                      .glided = placed.glided,
	                      .pauseSeconds = placed.pauseSeconds};
}

void CameraPathSystem::HandlePlayerControl(const PlayerControl& control)
{
	if (!_placed.empty() && camera_path::TakesCameraBack(control.movementKey, control.frameMilliseconds, control.grippingLand))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the player took the camera back from a miracle's path");
		_placed.clear();
	}
}

void CameraPathSystem::UpdatePlaced(Placed& placed)
{
	auto& camera = Locator::camera::value();
	std::optional<float> seconds;
	if (placed.playing)
	{
		// Playing: every update the camera is sent afresh to where the path is now, to arrive in a moment, so it
		// follows a little behind
		seconds = camera_path::k_PlacedLagSeconds;
	}
	else if (!placed.glided)
	{
		// Once before it plays: one glide from where the camera is onto the path at its time, arriving still after the
		// pause and a little more
		seconds = camera_path::PlacedGlideSeconds(placed.pauseSeconds);
		placed.glided = true;
	}
	if (!seconds.has_value())
	{
		return;
	}
	const auto sample = placed.path->SampleAt(std::chrono::milliseconds(placed.pathMilliseconds));
	const auto origin = camera_path::PlacePoint(placed.placement, sample.position);
	const auto focus = camera_path::PlacePoint(placed.placement, sample.focus);
	camera.GetOriginZoomer().SetDestinationWithTime(origin, *seconds);
	camera.GetFocusZoomer().SetDestinationWithTime(focus, *seconds);
}

void CameraPathSystem::Update(const std::chrono::microseconds& dt)
{
	if (!_placed.empty() && Locator::camera::has_value())
	{
		// The camera's update for the path that has it: its destinations, then the zoomers moved on
		UpdatePlaced(_placed.back());
		Locator::camera::value().UpdateZoomers(std::nullopt, std::chrono::duration<float>(dt).count());
		return;
	}
	if (_state == CameraPathState::STOPPED || !_path)
	{
		return;
	}
	if (_state != CameraPathState::PLAYING)
	{
		return;
	}

	// The path spreads its points evenly over its duration, and the camera follows it as the temple's does
	_elapsed += dt;
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(_elapsed);
	const auto sample = _path->SampleAt(elapsed);
	auto& camera = Locator::camera::value();
	camera.SetOrigin(sample.position);
	camera.SetFocus(sample.focus);
	if (elapsed >= _path->GetDuration())
	{
		Stop();
	}
}
