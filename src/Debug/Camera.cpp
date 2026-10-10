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

#include <entt/core/hashed_string.hpp>

#include "3D/CameraPath.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraPathControl.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
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
	ImGui::EndChild();
}

void Camera::DrawPlacedPath()
{
	if (!ImGui::CollapsingHeader("Placed path"))
	{
		return;
	}
	auto& cameraPathSystem = Locator::cameraPathSystem::value();
	ImGui::Text("Holds the camera: %s", cameraPathSystem.HoldsCamera() ? "yes" : "no");
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
	const auto watches = psys::forest_camera::Watches();
	const auto watch = std::ranges::find(watches, placed->owner, &psys::forest_camera::Watch::owner);
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
