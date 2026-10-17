/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ScriptControlSystem.h"

#include <string_view>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
void ScriptMessage(std::string_view message)
{
	if (const auto logger = spdlog::get("scripting"))
	{
		SPDLOG_LOGGER_WARN(logger, "{}", message);
	}
}
} // namespace

bool ScriptControlSystem::StartCameraControl(Camera& camera, const CameraRequest& request,
                                             [[maybe_unused]] GroundHeight groundAt)
{
	if (request.insideTemple)
	{
		// The temple's camera stays as it is, and only its own scripts take control of it
		if (!request.templeScript)
		{
			return false;
		}
		_cameraOwner = request.task;
		return true;
	}
	// The script camera mode starts from the camera's zoomers, unless the current mode can't be left. It reads the
	// ground itself as it moves, so it needs no ground height
	if (!script_camera::BeginFrom(camera.GetOriginZoomer(), camera.GetFocusZoomer()))
	{
		ScriptMessage("Note-Camera control Failed");
		return false;
	}
	_cameraOwner = request.task;
	return true;
}

bool ScriptControlSystem::EndCameraControl(Camera& camera, uint32_t task)
{
	if (_cameraOwner != task)
	{
		return false;
	}
	ReleaseCamera(camera);
	return true;
}

void ScriptControlSystem::ReleaseCamera([[maybe_unused]] Camera& camera)
{
	// The script mode goes and the player's starts from where the camera is; script_camera::End hands the zoomers back
	// to the game's camera itself, and says so when the script mode is not the current one
	script_camera::End();
	_cameraOwner = 0;
}

void ScriptControlSystem::StartGameSpeed(uint32_t task)
{
	if (_gameSpeedOwner == 0 || _gameSpeedOwner == task)
	{
		_gameSpeedOwner = task;
	}
}

bool ScriptControlSystem::EndGameSpeed(uint32_t task)
{
	if (_gameSpeedOwner != 0 && _gameSpeedOwner != task)
	{
		return false;
	}
	_gameSpeedOwner = 0;
	return true;
}

ScriptControlSystemInterface::Released ScriptControlSystem::TaskStopped(Camera& camera, uint32_t task)
{
	Released released;
	if (_cameraOwner == task)
	{
		ReleaseCamera(camera);
		released.camera = true;
	}
	if (_gameSpeedOwner == task)
	{
		_gameSpeedOwner = 0;
		released.gameSpeed = true;
	}
	return released;
}

void ScriptControlSystem::Reset()
{
	_cameraOwner = 0;
	_gameSpeedOwner = 0;
}
