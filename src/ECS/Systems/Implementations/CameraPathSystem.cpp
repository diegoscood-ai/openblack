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
#include <tuple>
#include <utility>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CameraFrameTime.h"
#include "Camera/CameraPathControl.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Systems/CameraStackInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The camera's stack of modes, which holds an entry for each placed path; none without the camera stack service
camera_stack::ModeStack* Stack()
{
	return Locator::cameraStack::has_value() ? &Locator::cameraStack::value().Modes() : nullptr;
}

/// The script camera's modes are on the camera's stack of modes
bool ScriptCameraThere()
{
	return Locator::scriptState::has_value() && Locator::cameraStack::has_value();
}
} // namespace

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

CameraPathSystem::~CameraPathSystem()
{
	if (auto* stack = Stack(); stack != nullptr)
	{
		for (const auto& placed : _placed)
		{
			std::ignore = stack->RemovePath(placed.owner);
		}
	}
}

uint32_t CameraPathSystem::Under(const Placed& placed)
{
	const auto* stack = Stack();
	return stack != nullptr ? stack->PathUnder(placed.owner) : 0U;
}

void CameraPathSystem::TakeOffStack(PathOwner owner)
{
	// A path that was the current mode over a dual camera leaves the dual camera the camera's zoomers as it left them
	if (auto* stack = Stack(); stack != nullptr && stack->RemovePath(owner) && ScriptCameraThere())
	{
		script_camera::TakeBackFromAnotherMode();
	}
}

void CameraPathSystem::ClearAllPlaced()
{
	for (const auto& placed : _placed)
	{
		TakeOffStack(placed.owner);
	}
	_placed.clear();
}

void CameraPathSystem::ClearPlaced()
{
	ClearAllPlaced();
	_covered = false;
}

bool CameraPathSystem::PathCurrent() const
{
	if (_placed.empty() || _covered || Under(_placed.back()) != 0)
	{
		return false;
	}
	// Creature Mode's follow or the dance camera pushed after it is the current mode over it
	const auto* stack = Stack();
	return stack == nullptr || !stack->OverPath(_placed.back().owner);
}

bool CameraPathSystem::HoldsCamera() const
{
	// Inside the citadel the temple's rooms have the camera: the path goes on behind it on the island's camera
	// (UpdateBehindTemple)
	return !game_clock::IsInsideCitadel() && PathCurrent();
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
	ClearAllPlaced();
	_state = CameraPathState::STOPPED;
	_elapsed = std::chrono::microseconds::zero();
	_path = entt::resource<CameraPath>();
}

void CameraPathSystem::StopPlayback()
{
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
	// It goes on top of the current mode, a dual camera included: a dual camera that drove the camera hands its zoomers
	// back for the path to go on from
	if (ScriptCameraThere())
	{
		script_camera::HandOverToAnotherMode();
	}
	_placed.push_back(Placed {.owner = owner, .path = std::move(path), .placement = placement, .pauseSeconds = pauseSeconds});
	// its entry on top of the camera's stack of modes; a full stack deletes its bottom mode first, whatever it is, through
	// that mode's owner
	if (auto* stack = Stack(); stack != nullptr && stack->PushPath(owner) != camera_stack::ModeStack::Dropped::None)
	{
		script_camera::DeleteDroppedModes();
	}
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

bool CameraPathSystem::RemoveInvalid()
{
	if (_placed.empty())
	{
		return false;
	}
	// Only the one that has the camera going counts as the current mode's; one under a new player's mode goes quietly
	const bool topGoes = !_placed.back().valid && Under(_placed.back()) == 0;
	for (const auto& placed : _placed)
	{
		if (!placed.valid)
		{
			TakeOffStack(placed.owner);
		}
	}
	std::erase_if(_placed, [](const Placed& placed) { return !placed.valid; });
	if (topGoes)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the turn took a miracle's path off the camera");
	}
	return topGoes;
}

bool CameraPathSystem::TakeOffCurrent()
{
	if (!HoldsCamera())
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path was taken off the camera");
	TakeOffStack(_placed.back().owner);
	_placed.pop_back();
	// Popped off a stack it leaves empty, with the player's own mode deleted by a full stack: a new player's mode starts
	// from the camera as it is
	if (ScriptCameraThere())
	{
		std::ignore = script_camera::StartNewPlayersModeIfGone();
	}
	return true;
}

void CameraPathSystem::DeleteLowest()
{
	if (_placed.empty())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a full camera stack deleted the lowest miracle's path");
	TakeOffStack(_placed.front().owner);
	_placed.erase(_placed.begin());
}

std::optional<CameraPathSystem::PlacedReadout> CameraPathSystem::CurrentPlaced() const
{
	if (_placed.empty() || Under(_placed.back()) != 0)
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
		if (!PushPlayersModeOverPaths())
		{
			return;
		}
	}
}

bool CameraPathSystem::PushPlayersModeOverPaths()
{
	const auto playersMode = _pushPlayersMode ? _pushPlayersMode() : 0U;
	if (playersMode == 0)
	{
		// no stack of modes to keep them under
		ClearAllPlaced();
		return false;
	}
	// the paths go under it on the stack
	if (auto* stack = Stack(); stack != nullptr && stack->CoverPaths(playersMode))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a new player's camera went over the miracles' paths");
	}
	return true;
}

bool CameraPathSystem::PutPlayersModeOver()
{
	if (!HoldsCamera())
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a new player's camera went over a path that had the camera");
	std::ignore = PushPlayersModeOverPaths();
	return true;
}

void CameraPathSystem::UpdatePlaced(Placed& placed, Camera& camera)
{
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

bool CameraPathSystem::UpdateBehindTemple(Camera& world, const std::chrono::microseconds& dt)
{
	if (!PathCurrent())
	{
		return false;
	}
	// The current mode's update goes on in the citadel: the path sends the island's camera on along it, from where its
	// animation was last drawn, and the zoomers move on
	UpdatePlaced(_placed.back(), world);
	world.UpdateZoomers(std::nullopt, camera_frame::Seconds(dt));
	return true;
}

void CameraPathSystem::Update(const std::chrono::microseconds& dt)
{
	if (HoldsCamera() && Locator::camera::has_value())
	{
		// The camera's update for the path that has it: its destinations, then the zoomers moved on
		auto& camera = Locator::camera::value();
		UpdatePlaced(_placed.back(), camera);
		camera.UpdateZoomers(std::nullopt, camera_frame::Seconds(dt));
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
