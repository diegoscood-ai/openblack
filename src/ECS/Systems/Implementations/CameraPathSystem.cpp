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
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/Publish.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;

CameraPathSystem::CameraPathSystem()
    : CameraPathSystem([] { return !script_camera::ScriptModeCurrent(); })
{
}

CameraPathSystem::CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode)
    : CameraPathSystem(std::move(canLeaveCameraMode),
                       [] { return Locator::scriptState::has_value() ? script_camera::PushPlayersMode() : 0U; })
{
}

CameraPathSystem::CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode, PushPlayersMode pushPlayersMode)
    : _canLeaveCameraMode(std::move(canLeaveCameraMode))
    , _pushPlayersMode(std::move(pushPlayersMode))
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
	// A script's camera cannot be left, nor the creature interaction's: the path gives up, and is not asked for again
	if (!_canLeaveCameraMode() || _covered)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a script holds the camera, a miracle's path gave up");
		return false;
	}
	// An owner places one path
	if (std::ranges::any_of(_placed, [owner](const Placed& placed) { return placed.owner == owner; }))
	{
		return false;
	}
	_placed.push_back(Placed {.owner = owner,
	                          .path = std::move(path),
	                          .placement = placement,
	                          .pauseSeconds = pauseSeconds,
	                          .pushed = Locator::scriptState::has_value() ? script_camera::OtherModePushed() : 0U});
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
	// Only marked: the path still has the camera, and still follows, until the turn's checks take it off
	bool marked = false;
	for (auto& placed : _placed)
	{
		if (placed.owner == owner && placed.valid)
		{
			placed.valid = false;
			marked = true;
		}
	}
	if (marked)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path let the camera go, it goes at the turn");
	}
}

void CameraPathSystem::CheckPaths()
{
	if (_placed.empty())
	{
		return;
	}
	// Only the one that has the camera going restarts what is under it; one under a new player's mode goes quietly
	const bool topGoes = !_placed.back().valid && _placed.back().underPlayersMode == 0;
	std::erase_if(_placed, [](const Placed& placed) { return !placed.valid; });
	if (!topGoes)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the turn took a miracle's path off the camera");
	// A path under it has the camera again from where it is. With none, the player's camera starts again and flies back
	// to the view it had, unless another camera (the creature interaction's, a script's) has the view over them
	if (HoldsCamera() || _covered || !_canLeaveCameraMode() || !Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
}

bool CameraPathSystem::TakeOffCurrent()
{
	if (!HoldsCamera())
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path was taken off the camera");
	_placed.pop_back();
	return true;
}

void CameraPathSystem::DeleteLowest()
{
	if (_placed.empty())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a full camera stack deleted the lowest miracle's path");
	_placed.erase(_placed.begin());
}

void CameraPathSystem::LeaveUnderPlayersCamera(uint32_t playersMode)
{
	bool covered = false;
	for (auto& placed : _placed)
	{
		if (placed.underPlayersMode == 0)
		{
			placed.underPlayersMode = playersMode;
			covered = true;
		}
	}
	if (covered)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a new player's camera went over the miracles' paths");
	}
}

void CameraPathSystem::UncoverFromPlayersCamera(uint32_t playersMode)
{
	if (playersMode == 0)
	{
		return;
	}
	for (auto& placed : _placed)
	{
		if (placed.underPlayersMode == playersMode)
		{
			placed.underPlayersMode = 0;
		}
	}
}

std::optional<CameraPathSystem::PlacedReadout> CameraPathSystem::CurrentPlaced() const
{
	if (_placed.empty() || _placed.back().underPlayersMode != 0)
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
	if (!HoldsCamera() || control.scriptCameraOver)
	{
		return;
	}
	const int modes = camera_path::PlayersModesOnTakeBack(control.movementKey, control.frameMilliseconds, control.grippingLand);
	if (modes == 0)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the player took the camera back from a miracle's path");
	// A new player's mode on top for each, the paths staying under the first
	for (int i = 0; i < modes; ++i)
	{
		const auto playersMode = _pushPlayersMode ? _pushPlayersMode() : 0U;
		if (playersMode == 0)
		{
			// no stack of modes to keep them under
			_placed.clear();
			return;
		}
		LeaveUnderPlayersCamera(playersMode);
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
	if (HoldsCamera() && Locator::camera::has_value())
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
