/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureInteractCameraModel.h"

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>

#include "Camera.h"
#include "CreatureCameraModel.h"
#include "DefaultWorldCameraModel.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "Locator.h"
#include "ScriptCamera.h"

using namespace openblack;

creature_interact::View creature_interact::ViewOf(const glm::vec3& position, float height, float heading)
{
	const auto focus = position + glm::vec3(0.0f, k_FocusHeightShare * height, 0.0f);
	const auto ahead = glm::vec3(0.0f, k_Up - (k_UpLessPerHeight * height), -k_Ahead);
	const auto offset = glm::normalize(ahead) * (k_DistanceHeights * height);
	return {.origin = focus + (glm::mat3(glm::eulerAngleY(heading)) * offset), .focus = focus};
}

creature_interact::Holder creature_interact::HolderOf(const CameraModel& model)
{
	if (dynamic_cast<const DefaultWorldCameraModel*>(&model) != nullptr)
	{
		return Holder::PlayersCamera;
	}
	if (dynamic_cast<const CreatureCameraModel*>(&model) != nullptr)
	{
		return Holder::CreatureMode;
	}
	return Holder::Other;
}

creature_interact::Holder creature_interact::CurrentHolder(const Camera& camera)
{
	const bool script = Locator::scriptState::has_value();
	return CurrentHolder(script && script_camera::ScriptModeCurrent(), script && script_camera::DualCurrent(),
	                     Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera(),
	                     HolderOf(camera.GetModel()));
}

bool creature_interact::WooshesFlyingBack(const glm::vec3& from, const glm::vec3& to)
{
	return glm::distance(from, to) > k_WooshDistance;
}

CreatureInteractCameraModel::CreatureInteractCameraModel(glm::vec3 origin, glm::vec3 focus)
    : _last {.origin = origin, .focus = focus}
{
}

void CreatureInteractCameraModel::SetTarget(std::optional<creature_interact::View> view)
{
	_target = view;
	if (view.has_value())
	{
		_last = *view;
	}
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> CreatureInteractCameraModel::Update(std::chrono::microseconds /*dt*/,
                                                                                              const Camera& /*camera*/)
{
	// Each frame it sets off afresh for where the creature now wants it; with no target the camera glides on
	if (!_target.has_value())
	{
		return std::nullopt;
	}
	return CameraInterpolationUpdateInfo {
	    .origin = _target->origin,
	    .focus = _target->focus,
	    .duration = std::chrono::duration_cast<std::chrono::microseconds>(
	        std::chrono::duration<float>(creature_interact::k_EaseSeconds)),
	};
}

void CreatureInteractCameraModel::SetFlight(glm::vec3 origin, glm::vec3 focus)
{
	SetTarget(creature_interact::View {.origin = origin, .focus = focus});
}
