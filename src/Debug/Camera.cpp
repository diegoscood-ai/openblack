/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Camera.h"

#include <algorithm>
#include <chrono>
#include <span>

#include <entt/core/hashed_string.hpp>

#include "3D/CameraPath.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraDrag.h"
#include "Camera/CameraHelp.h"
#include "Camera/CameraModel.h"
#include "Camera/CameraPathControl.h"
#include "Camera/CameraZones.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/KeyboardMoveSpeed.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "Camera/TempleCameraModel.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Particles/Rules/Forest.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::debug::gui
{

Camera::Camera() noexcept
    : Window("Camera", ImVec2(50.0f, 25.0f))
    , _selectedCameraPath(entt::hashed_string("cam"))
{
}

void Camera::Draw() noexcept
{
	// The zones' fence the player's camera is kept inside: the file, the inclusion distance the scripts read and how
	// many times the camera was put back
	{
		const auto& script = player_camera::Get();
		const bool zones = Locator::cameraZoneSystem::has_value();
		ImGui::Text("Camera zone: %s, fence %s, inclusion distance %g, put back %u times",
		            zones && !Locator::cameraZoneSystem::value().GetFileName().empty()
		                ? Locator::cameraZoneSystem::value().GetFileName().c_str()
		                : "none",
		            zones && Locator::cameraZoneSystem::value().GetZones().fenceOn ? "on" : "off",
		            static_cast<double>(script.inclusionDistance), script.fenceHits);
	}
	// How far the player's camera is from the map's middle across the ground, and the disc it is kept inside
	if (Locator::camera::has_value() && Locator::terrainSystem::has_value())
	{
		const auto disc = camera_zones::MapDiscFor(Locator::terrainSystem::value().GetCellsPerSide());
		const auto across = camera_zones::AcrossFromMiddle(disc, Locator::camera::value().GetModel().GetTargetOrigin());
		ImGui::Text("Camera across the ground from the map's middle: %.1f, kept within %.0f", static_cast<double>(across),
		            static_cast<double>(disc.radius));
		// The disc's step rounds the origin and the focus every frame; its last change of them
		if (const auto* world = dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel()))
		{
			const auto& rounding = world->GetLastMiddleRounding();
			ImGui::Text("Last rounding through the map's middle: origin (%g, %g, %g), focus (%g, %g, %g)",
			            static_cast<double>(rounding.origin.x), static_cast<double>(rounding.origin.y),
			            static_cast<double>(rounding.origin.z), static_cast<double>(rounding.focus.x),
			            static_cast<double>(rounding.focus.y), static_cast<double>(rounding.focus.z));
		}
	}
	// Inside the temple: how far the room's camera has come toward a close look, how far along the curve to it its aim
	// is in a room of pictures, and how long the camera was last given to ease onto its aim
	if (Locator::camera::has_value())
	{
		if (const auto* temple = dynamic_cast<const TempleCameraModel*>(&Locator::camera::value().GetModel()))
		{
			const float blend = temple->GetSubMeshZoom();
			ImGui::Text("Temple close look: %s, blend %.3f, aim along the curve %.3f, easing in %.3f s",
			            temple->IsLookingAtSubMesh() ? "looking" : "not looking", static_cast<double>(blend),
			            static_cast<double>(TempleCameraModel::CloseLookAlong(blend)),
			            static_cast<double>(temple->GetEaseTime()));
		}
	}
	// What GET_CAMERA_POSITION would give the scripts now: the drawn camera, or the point outside the world inside the
	// citadel
	if (Locator::camera::has_value())
	{
		const auto read = script_camera::ReadCameraPoint(Locator::camera::value().GetOrigin(), game_clock::IsInsideCitadel());
		ImGui::Text("The scripts read the camera's position as (%g, %g, %g)", static_cast<double>(read.x),
		            static_cast<double>(read.y), static_cast<double>(read.z));
	}
	DrawCameraHelp();
	DrawCameraControls();
	if (!Locator::cameraPathSystem::has_value() || !Locator::resources::has_value())
	{
		ImGui::TextUnformatted("No camera path system");
		return;
	}
	auto& cameraPathSystem = Locator::cameraPathSystem::value();
	const float fontSize = ImGui::GetFontSize();
	ImGui::BeginChild("cameraPaths", ImVec2(fontSize * 10.0f, 0));

	const ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
	DrawCameraResourceList();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("cameraPathControls", ImVec2(0, 0), ImGuiChildFlags_Borders, flags);
	if (ImGui::BeginMenuBar())
	{
		ImGui::TextUnformatted("Controls");
		ImGui::EndMenuBar();
	}
	if (ImGui::Button("Run"))
	{
		// Inside the temple a path is relative to the temple, so the camera starts from it
		if (Locator::temple::has_value() && Locator::temple::value().Active() && Locator::camera::has_value())
		{
			Locator::camera::value().SetOrigin(Locator::temple::value().GetPosition());
		}
		cameraPathSystem.Start(_selectedCameraPath);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		cameraPathSystem.Stop();
	}
	ImGui::SameLine();
	const bool paused = cameraPathSystem.IsPaused();
	if (ImGui::Button(paused ? "Resume" : "Pause"))
	{
		if (paused)
		{
			cameraPathSystem.Play();
		}
		else
		{
			cameraPathSystem.Pause();
		}
	}

	DrawControls();
	DrawPlacedPath();
	DrawHandToolTip();
	ImGui::EndChild();
}

void Camera::DrawCameraHelp()
{
	if (!ImGui::CollapsingHeader("The camera help"))
	{
		return;
	}
	const auto yesNo = [](bool on) { return on ? "yes" : "no"; };
	// What the player's camera told the camera help in its last update: whether that update had its own frame's
	// controls, where their input came from and what it reported
	const auto* world = Locator::camera::has_value()
	                        ? dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel())
	                        : nullptr;
	if (world == nullptr)
	{
		ImGui::TextUnformatted("Another camera has the view: the player's camera tells the camera help nothing");
	}
	else if (const auto& heard = world->GetLastHelpHeard(); !heard.controls)
	{
		ImGui::TextUnformatted("Last update: no controls of its own frame (a script's camera, a hand demo or a camera path "
		                       "had the camera, or it had just been handed over), so nothing heard");
	}
	else
	{
		using help_profile::CameraInput;
		using help_profile::CameraReason;
		const auto from = [&heard](CameraInput input) { return (heard.inputs & static_cast<uint32_t>(input)) != 0; };
		ImGui::Text("Last update's input (0x%02X): keys %s, the land gripped %s, both buttons %s, the wheel %s, the middle "
		            "button %s",
		            heard.inputs, yesNo(from(CameraInput::Keyboard)), yesNo(from(CameraInput::MouseButton)),
		            yesNo(from(CameraInput::BothMouseButtons)), yesNo(from(CameraInput::WheelSpin)),
		            yesNo(from(CameraInput::WheelDown)));
		const auto made = std::span(heard.reports.reasons).first(heard.reports.count);
		const auto reported = [&made](CameraReason reason) { return std::ranges::find(made, reason) != made.end(); };
		ImGui::Text("Last update's reports: zoom %s, turn %s (right %s, left %s), tilt %s", yesNo(reported(CameraReason::Zoom)),
		            yesNo(reported(CameraReason::Rotate)), yesNo(reported(CameraReason::RotateCW)),
		            yesNo(reported(CameraReason::RotateCCW)), yesNo(reported(CameraReason::Pitch)));
	}
	// The counts the scripts read (GET_TOTAL_EVENTS): the game turns in which each happened
	if (Locator::scriptState::has_value())
	{
		using help_profile::Event;
		const auto turns = [](Event event) {
			return static_cast<double>(help_profile::TotalEvents(static_cast<int32_t>(event)).value_or(0.0f));
		};
		ImGui::Text("Turns counted: turn %g (right %g, left %g), tilt %g, zoom %g, drag %g", turns(Event::Rotate),
		            turns(Event::RotateCW), turns(Event::RotateCCW), turns(Event::Pitch), turns(Event::Zoom),
		            turns(Event::Drag));
	}
}

void Camera::DrawCameraControls()
{
	if (!ImGui::CollapsingHeader("The player camera's controls"))
	{
		return;
	}
	const auto* world = Locator::camera::has_value()
	                        ? dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel())
	                        : nullptr;
	if (world == nullptr)
	{
		ImGui::TextUnformatted("Another camera has the view");
		return;
	}
	// The frames a script's camera, a hand demo or a camera path had the camera as they began, or the model was handed
	// it: no controls are read, and nothing read before moves the camera then
	ImGui::Text("Frames whose controls were not read: %u", world->GetFramesWithoutControls());
	// After a drag given up or at the fence every control but the wheel is dropped until a frame with none of them; the
	// zoom asked for in the last frame, the wheel's included
	const auto cues = world->GetHandCues();
	ImGui::Text("Held back after a drag given up or at the fence: %s, zoom asked for %g",
	            cues.icons.has_value() && cues.icons->blocked ? "yes" : "no",
	            cues.icons.has_value() ? static_cast<double>(cues.icons->zoom) : 0.0);
	// The end of a script's camera control makes the player's camera again: how many times, and the game turn of the last
	const auto& restarts = world->GetRestarts();
	ImGui::Text("Restarted on a script's camera end: %u times, the last in game turn %u", restarts.count, restarts.lastTurn);
	// The camera's easing slows to 1.5 s when it starts again and quickens to 0.3 s over its first 1.5 s; a watch of a
	// fight starts it again, and a flight's start sets it as it sets the time since the flight began
	ImGui::Text("Easing time: %.2f s, since the flight began: %.2f s, watching a fight: %s",
	            static_cast<double>(std::chrono::duration<float>(world->GetEasingTime()).count()),
	            static_cast<double>(world->GetFlightSeconds()), world->IsWatchingFight() ? "yes" : "no");
	// The turn, tilt and movement keys held now, each pair added up as the camera adds them (both of a pair held: 0)
	if (Locator::gameActionSystem::has_value())
	{
		const auto& actions = Locator::gameActionSystem::value();
		ImGui::Text("Turn keys: %+.0f, tilt keys: %+.0f, forwards keys: %+.0f, sideways keys: %+.0f",
		            static_cast<double>(KeyPairInput(actions.Get(input::BindableActionMap::ROTATE_LEFT),
		                                             actions.Get(input::BindableActionMap::ROTATE_RIGHT))),
		            static_cast<double>(KeyPairInput(actions.Get(input::BindableActionMap::TILT_DOWN),
		                                             actions.Get(input::BindableActionMap::TILT_UP))),
		            static_cast<double>(KeyPairInput(actions.Get(input::BindableActionMap::MOVE_FORWARDS),
		                                             actions.Get(input::BindableActionMap::MOVE_BACKWARDS))),
		            static_cast<double>(KeyPairInput(actions.Get(input::BindableActionMap::MOVE_RIGHT),
		                                             actions.Get(input::BindableActionMap::MOVE_LEFT))));
	}
}

void Camera::DrawHandToolTip()
{
	if (!ImGui::CollapsingHeader("The hand's tooltip over the land"))
	{
		return;
	}
	namespace hand_tooltips = ecs::systems::hand_tooltips;
	if (Locator::handSystem::has_value())
	{
		ImGui::Text("Hand state: %d", Locator::handSystem::value().GetInterfaceHandState());
	}
	// over a thing the hand can pick up, the script's limits show the land's tooltips in place of its own
	const auto interfaceFlags = interface_active::GetFlags();
	ImGui::Text("The interface flags: 0x%X (the scripts limit the interface: %s)", static_cast<unsigned>(interfaceFlags),
	            (interfaceFlags & 2u) != 0 ? "yes" : "no");
	const uint32_t tricons = Locator::camera::has_value() ? Locator::camera::value().GetModel().GetHandCues().tricons : 0;
	const auto features = static_cast<uint32_t>(camera_help::GetEnabledFeatures());
	const auto yesNo = [](bool on) { return on ? "yes" : "no"; };
	ImGui::Text("The camera hints: tilt %s, turn %s", yesNo((tricons & camera_drag::tricon::k_Pitch) != 0),
	            yesNo((tricons & camera_drag::tricon::k_Rotate) != 0));
	ImGui::Text("The scripts allow: tilt %s, turn %s, zoom %s, the land grab and the movement keys %s",
	            yesNo((features & camera_drag::feature::k_Pitch) != 0), yesNo((features & camera_drag::feature::k_Rotate) != 0),
	            yesNo((features & camera_drag::feature::k_Zoom) != 0), yesNo((features & camera_drag::feature::k_Strafe) != 0));
	const auto view = help_profile::PlayerCameraView();
	if (view.has_value())
	{
		ImGui::Text("The player's camera: the middle of the screen on the land %s, %.2f from it, pitch %.3f",
		            yesNo(view->screenCentreOnLand), static_cast<double>(view->headingDistance),
		            static_cast<double>(view->pitch));
	}
	else
	{
		ImGui::TextUnformatted("The player's camera: none (another camera has the view, or the help is not told)");
	}
	// what the land texts give for these facts, with no fish under the hand
	const auto tip = hand_tooltips::LandToolTip({
	    .tricons = tricons,
	    .features = features,
	    .centreOnLand = view.has_value() && view->screenCentreOnLand,
	    .centreDistance = view.has_value() ? view->headingDistance : 0.0f,
	    .pitch = view.has_value() ? view->pitch : 0.0f,
	});
	ImGui::Text("The land text: 0x%X, button %d, arrows 0x%X", tip.text, tip.action, tip.align);
}

void Camera::DrawPlacedPath()
{
	if (!ImGui::CollapsingHeader("Placed path"))
	{
		return;
	}
	auto& cameraPathSystem = Locator::cameraPathSystem::value();
	ImGui::Text("Holds the camera: %s", cameraPathSystem.HoldsCamera() ? "yes" : "no");
	ImGui::Text("Under the creature interaction's camera: %s", cameraPathSystem.IsCovered() ? "yes" : "no");
	const auto placed = cameraPathSystem.CurrentPlaced();
	if (!placed.has_value())
	{
		ImGui::TextUnformatted("No placed path");
		return;
	}
	ImGui::Text("Owner: %016llx", static_cast<unsigned long long>(placed->owner));
	// before it plays, what is left of the camera's glide onto the path
	float glideLeft = 0.0f;
	if (placed->glided && !placed->playing && Locator::camera::has_value())
	{
		const auto& zoomer = Locator::camera::value().GetOriginZoomer().axis[0];
		glideLeft = std::max(zoomer.duration - zoomer.time, 0.0f);
	}
	ImGui::Text("Glide left (seconds): %.2f of %.2f", glideLeft, camera_path::PlacedGlideSeconds(placed->pauseSeconds));
	ImGui::Text("Path time (milliseconds): %d", placed->pathMilliseconds);
	ImGui::Text("Playing: %s", placed->playing ? "yes" : "no");
	// the particle that placed it, if it is one: the last frame its draw reached
	const auto watches = particles::forest_camera::Watches();
	const auto watch = std::ranges::find(watches, placed->owner, &particles::forest_camera::Watch::owner);
	if (watch != watches.end())
	{
		ImGui::Text("Last frame drawn: %d of %d", watch->drawnFrame, camera_path::k_FramesPerCycle - 1);
	}
	else
	{
		ImGui::TextUnformatted("Last frame drawn: no particle");
	}
	if (ImGui::Button("Release"))
	{
		cameraPathSystem.Release(placed->owner);
	}
}

void Camera::DrawControls()
{
	auto const& cameraPaths = Locator::resources::value().GetCameraPaths();
	const auto& cameraPath = cameraPaths.Handle(_selectedCameraPath);
	if (cameraPath)
	{
		ImGui::BeginChild("cameraPathControls");
		const auto& pointCount = cameraPath->GetPoints().size();
		ImGui::Text("Points: %zu", pointCount);
		auto duration = std::chrono::duration<unsigned int, std::milli>(cameraPath->GetDuration()).count();
		ImGui::Text("Duration (milliseconds): %u", duration);
		ImGui::EndChild();
	}
	else
	{
		ImGui::Text("No paths selected");
	}
}

void Camera::DrawCameraResourceList()
{
	auto meshSize = ImGui::GetItemRectSize();
	const ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
	const bool isChildVisible =
	    ImGui::BeginChild("cameraPathList", ImVec2(meshSize.x, meshSize.y), ImGuiChildFlags_Borders, flags);
	if (ImGui::BeginMenuBar())
	{
		ImGui::TextUnformatted("Camera Paths");
		ImGui::EndMenuBar();
	}
	if (isChildVisible)
	{
		auto const& cameraPaths = Locator::resources::value().GetCameraPaths();
		cameraPaths.Each([this](entt::id_type id, const CameraPath& cameraPath) {
			const auto& name = cameraPath.GetDebugName();
			if (ImGui::Selectable(!name.empty() ? name.c_str() : "unnamed path", id == _selectedCameraPath))
			{
				_selectedCameraPath = id;
			}
		});
	}
	ImGui::EndChild();
}

void Camera::Update() noexcept {}

void Camera::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Camera::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}

} // namespace openblack::debug::gui
