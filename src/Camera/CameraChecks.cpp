/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraChecks.h"

#include <tuple>

#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraFrameTime.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureMode.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraStackInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::camera_checks
{

namespace
{
/// The camera's stack of modes; none without the camera stack service
camera_stack::ModeStack* Stack()
{
	return Locator::cameraStack::has_value() ? &Locator::cameraStack::value().Modes() : nullptr;
}

/// The dance a dance camera is kept by is still about: not gone, nor marked to go
bool DanceThere(entt::entity dance)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(dance) && registry.AllOf<ecs::components::Dance>(dance) &&
	       !registry.AllOf<ecs::components::Unavailable>(dance);
}

/// The current mode's kind, with Creature Mode's follows those of `follows`
Top TopOf(const ecs::systems::CreatureModeSystemInterface* follows)
{
	if (Locator::scriptState::has_value())
	{
		// The script camera's own mode when it is the current one, pushed after the others, and drives the camera (not
		// with the camera test hooks)
		if (script_camera::Drives())
		{
			return script_camera::DualCurrent() ? Top::Dual : Top::Script;
		}
		if (script_camera::InteractionCurrent())
		{
			return Top::Interaction;
		}
		if (script_camera::Modes().PlayerCurrent())
		{
			return Top::Player;
		}
	}
	// The dance camera, pushed after every other mode
	if (const auto* stack = Stack(); stack != nullptr && stack->CurrentDance().has_value())
	{
		return Top::Dance;
	}
	// A placed path with no follow pushed after it
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera())
	{
		return Top::Path;
	}
	// The follow while it has the camera's view
	if (follows != nullptr && follows->GetView().has_value())
	{
		return Top::Follow;
	}
	return Top::Player;
}
} // namespace

Top CurrentTop()
{
	return TopOf(Locator::creatureModeSystem::has_value() ? &Locator::creatureModeSystem::value() : nullptr);
}

bool PutNewPlayersModeOver()
{
	return PutNewPlayersModeOver(Locator::creatureModeSystem::has_value() ? &Locator::creatureModeSystem::value() : nullptr);
}

bool PutNewPlayersModeOver(ecs::systems::CreatureModeSystemInterface* follows)
{
	if (!Locator::camera::has_value())
	{
		return false;
	}
	const auto top = TopOf(follows);
	switch (top)
	{
	case Top::Player:
		// a player's mode already
		return false;
	case Top::Interaction:
		// the creature interaction's camera cannot be left
		return false;
	case Top::Script:
		// nor a script's camera mode while it is still valid
		if (!Locator::scriptState::has_value() || !script_camera::Modes().CanLeaveCurrent())
		{
			return false;
		}
		break;
	case Top::Follow:
	case Top::Path:
	case Top::Dual:
	case Top::Dance:
		break;
	}
	// A mode of the script camera's that drove the camera hands its zoomers back: the new mode starts from the camera as it is
	if (Locator::scriptState::has_value())
	{
		script_camera::HandOverToAnotherMode();
	}
	// The follow whose model the camera has keeps it under the new mode, which goes over the placed paths too
	if (follows != nullptr && follows->GoUnderNewPlayersMode())
	{
		return true;
	}
	if (top == Top::Path)
	{
		return Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().PutPlayersModeOver();
	}
	if (top == Top::Follow || !Locator::scriptState::has_value())
	{
		return false;
	}
	// Over a dual camera: the player's own camera, as it is, is kept for the new mode's pop, and the paths under the dual
	// camera go under the new mode
	const auto playersMode = script_camera::PushPlayersMode();
	if (playersMode != 0 && script_camera::Modes().CoverPaths(playersMode))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a new player's camera went over the miracles' paths");
	}
	return playersMode != 0;
}

FrameUpdate CurrentFrameUpdate(bool scriptDrove)
{
	// Inside the citadel the temple's rooms have the camera, as for a placed path
	const auto* stack = Stack();
	const bool danceCurrent =
	    !scriptDrove && stack != nullptr && stack->CurrentDance().has_value() && !game_clock::IsInsideCitadel();
	return FrameUpdateFor(
	    scriptDrove, !scriptDrove && Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera(),
	    danceCurrent);
}

bool MakeDanceCamera(entt::entity dance)
{
	auto* stack = Stack();
	if (stack == nullptr || !stack->CanLeaveCurrent())
	{
		return false;
	}
	// A dual camera that drove the camera hands its zoomers back: the dance camera holds the camera as it is
	const bool scriptCamera = Locator::scriptState::has_value();
	if (scriptCamera)
	{
		script_camera::HandOverToAnotherMode();
		script_camera::ModeChanged();
	}
	// On top of the stack; a full stack deletes its bottom mode first, whatever it is, through that mode's owner
	if (stack->PushDance(dance) != camera_stack::ModeStack::Dropped::None && scriptCamera)
	{
		script_camera::DeleteDroppedModes();
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the dance camera is the current camera mode");
	return true;
}

void UpdateDanceCamera(Camera& camera, std::chrono::microseconds dt, int32_t interfaceHandState)
{
	auto* stack = Stack();
	const auto dance = stack != nullptr ? stack->CurrentDance() : std::nullopt;
	if (dance.has_value())
	{
		const bool there = DanceThere(*dance);
		const bool useCamera = there && Locator::entitiesRegistry::value().Get<ecs::components::Dance>(*dance).useCamera;
		const auto frame = DanceFrameOf(there, useCamera, creature_mode::GripPutsPlayersCameraOver(interfaceHandState));
		if (frame.ends)
		{
			stack->EndDance(*dance);
		}
		if (frame.clearsStack)
		{
			ClearStack(false);
		}
	}
	// No destination of its own: the zoomers go on to where they were heading
	camera.UpdateZoomers(std::nullopt, camera_frame::Seconds(dt));
}

void CheckModes()
{
	const auto before = CurrentTop();
	// Every owner deletes its modes that are no longer valid, and restarts nothing. The current mode is the one current
	// as the pass began, whatever goes before it: only its cleanup runs, as the original's single pass from the bottom
	// of the stack meets it last
	if (auto* stack = Stack(); stack != nullptr)
	{
		stack->BeginValidityPass();
	}
	Went went;
	bool scriptWent = false;
	if (Locator::scriptState::has_value())
	{
		const auto stacked = script_camera::Modes().Size();
		went.script = script_camera::Validate();
		scriptWent = script_camera::Modes().Size() != stacked;
	}
	bool pathsWent = false;
	if (Locator::cameraPathSystem::has_value())
	{
		auto& paths = Locator::cameraPathSystem::value();
		const auto placed = paths.PlacedCount();
		went.path = paths.RemoveInvalid();
		pathsWent = paths.PlacedCount() != placed;
	}
	bool followWent = false;
	if (Locator::creatureModeSystem::has_value())
	{
		auto& follow = Locator::creatureModeSystem::value();
		const bool wasActive = follow.IsActive();
		went.follow = follow.RemoveInvalid();
		followWent = wasActive && !follow.IsActive();
	}
	// The dance cameras that ended, or whose dance went
	bool dancesWent = false;
	if (auto* stack = Stack(); stack != nullptr)
	{
		const auto others = stack->OtherCount();
		went.dance = stack->RemoveInvalidDances(&DanceThere);
		dancesWent = stack->OtherCount() != others;
	}
	if (auto* stack = Stack(); stack != nullptr)
	{
		stack->EndValidityPass();
	}
	// A stack the pass left empty, with the player's own mode deleted by a full stack, starts a new player's mode
	const bool anyWent = scriptWent || pathsWent || followWent || dancesWent;
	const bool newPlayersMode = anyWent && Locator::scriptState::has_value() && script_camera::NewPlayersModeIfGone();
	const auto after = CurrentTop();
	switch (RestartAfter(before, after, went, newPlayersMode))
	{
	case Restart::None:
		break;
	case Restart::Path:
		// The path sets its close near plane again, which the frame's camera update sets back to the camera's height's
		// before any draw, as the original's frame does: nothing more
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path is the current camera mode again");
		break;
	case Restart::Player:
		if (Locator::camera::has_value())
		{
			// The player's camera starts again and flies back to the view it had
			auto& camera = Locator::camera::value();
			camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
			ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
		}
		break;
	case Restart::NewPlayersMode:
		if (Locator::camera::has_value())
		{
			// From the camera as it is, its mode's seconds from 0
			script_camera::Get().modeSeconds = 0.0f;
			Locator::camera::value().GetModel().Restart();
			ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
		}
		break;
	}
	// The mode's time is one count for the whole stack: zeroed when the mode over the follow (a dual camera, a script's
	// camera mode, a newer follow) went on and run on while it had the view; the turn's check does not zero it, so the
	// follow current again goes on from it
	if (after == Top::Follow && TopWent(before, went) && Locator::camera::has_value() && Locator::scriptState::has_value())
	{
		if (auto* follow = dynamic_cast<CreatureCameraModel*>(&Locator::camera::value().GetModel()); follow != nullptr)
		{
			follow->SetSecondsInMode(script_camera::Get().modeSeconds);
		}
	}
}

void ClearStack(bool newGame)
{
	// Inside the citadel the island's camera behind the temple holds the current mode's model
	Camera* world = Locator::temple::has_value() ? Locator::temple::value().GetWorldCamera() : nullptr;
	// The follows give the camera back with nothing restarted, before the script camera's modes they may be under go
	if (Locator::creatureModeSystem::has_value())
	{
		if (world != nullptr)
		{
			Locator::creatureModeSystem::value().LeaveAllBehindTemple(*world);
		}
		else
		{
			Locator::creatureModeSystem::value().LeaveAll();
		}
	}
	if (Locator::cameraPathSystem::has_value())
	{
		Locator::cameraPathSystem::value().ClearPlaced();
	}
	if (auto* stack = Stack(); stack != nullptr)
	{
		stack->ClearDances();
	}
	if (newGame)
	{
		script_camera::Reset();
	}
	else
	{
		script_camera::ClearModes();
	}
	// A new player's mode, from the camera as it is: the player's own model starts again (another one, the temple's or
	// the editor's, is left to its owner). Inside the citadel it is the island's camera's, which the way out starts again
	Camera* camera = world != nullptr ? world : (Locator::camera::has_value() ? &Locator::camera::value() : nullptr);
	if (camera != nullptr)
	{
		if (auto* player = dynamic_cast<DefaultWorldCameraModel*>(&camera->GetModel()); player != nullptr)
		{
			player->Restart();
		}
	}
}

void LeaveCitadel(std::optional<world_camera::Place> picked)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto top = CurrentTop();
	const auto leave =
	    world_camera::OnLeave(picked, top == Top::Player ? world_camera::Model::World : world_camera::Model::Other);
	if (leave.cutTo.has_value())
	{
		// at the place at once: each zoomer's value, start and destination
		camera.SetOrigin(leave.cutTo->origin);
		camera.SetFocus(leave.cutTo->focus);
	}
	if (leave.startAgain)
	{
		// the player's mode starts again from there, keeping a watched fight unless a place was picked, its help icons by
		// the hand with it
		camera.GetModel().StartAgain(!picked.has_value());
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
		return;
	}
	if (!leave.newWorldModel)
	{
		return;
	}
	// The zoomers a dual camera drives are the camera's: cut to the place too, so the new mode, which takes them back from
	// it, starts there
	if (top == Top::Dual && Locator::scriptState::has_value())
	{
		script_camera::TakeBackFromAnotherMode();
	}
	// A new player's mode over the current one (a follow, a placed path, a dual camera), which stays under it, from the
	// camera at the place; none over a script's camera mode still going or the creature interaction's camera, which
	// cannot be left
	std::ignore = PutNewPlayersModeOver();
}

bool CameraModelIsCurrent(const camera_stack::ModeStack* stack)
{
	if (stack == nullptr)
	{
		return true;
	}
	// Another system's entry on top: only a follow moves the camera through the camera's model
	if (stack->OtherCurrent())
	{
		return stack->OtherEntries().back().kind == camera_stack::StackedMode::Kind::Follow;
	}
	// The player's own mode, a new player's mode on top, or the creature interaction's camera; not a script's camera
	// mode nor a dual camera
	return stack->Empty() || stack->PlayerCurrent() || stack->InteractionCurrent();
}

void UpdateBehindTemple(Camera& world, std::chrono::microseconds dt)
{
	// In the citadel the camera's mode reads none of the player's controls
	world.SkipActions();
	if (CameraModelIsCurrent(Stack()))
	{
		world.Update(dt);
	}
	else if (!Locator::cameraPathSystem::has_value() || !Locator::cameraPathSystem::value().UpdateBehindTemple(world, dt))
	{
		// a placed path that is the current mode runs its own update on it; another mode's own update sets where the
		// camera goes, and the zoomers go on towards it
		world.UpdateZoomers(std::nullopt, camera_frame::Seconds(dt));
	}
	world.KeepDrawnView();
}

} // namespace openblack::camera_checks
