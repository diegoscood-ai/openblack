/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Physics.h"

#include <cstdint>

#include <imgui.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

Physics::Physics() noexcept
    : Window("Physics", ImVec2(320.0f, 260.0f))
{
}

void Physics::Draw() noexcept
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		ImGui::Text("There is no physics");
		return;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	int moving = 0;
	int resting = 0;
	dynamics.ForEachEntry(
	    [&moving, &resting](const ecs::PhysicsEntry& entry) { (!entry.body.resting ? moving : resting) += 1; });
	ImGui::Text("Bodies: %d moving, %d resting", moving, resting);
	// the scripts' record of the last hit, as the hit natives read it (none at the start of a land)
	const auto hit = dynamics.GetHitObject();
	const auto hitter = dynamics.GetObjectWhichHit();
	if (hit == entt::null && hitter == entt::null)
	{
		ImGui::Text("Scripts' last hit: none");
	}
	else
	{
		ImGui::Text("Scripts' last hit: %u, hit by %u", static_cast<uint32_t>(hit), static_cast<uint32_t>(hitter));
	}

	ImGui::InputInt("Static type", &_type);
	ImGui::InputFloat("Height", &_height);
	ImGui::InputFloat("Scale", &_scale);
	ImGui::InputFloat3("Velocity", _velocity.data());
	ImGui::InputInt("Object (-1: a new one)", &_entity);
	if (ImGui::Button("Throw at the camera's focus") && Locator::camera::has_value() && Locator::terrainSystem::has_value())
	{
		auto object = static_cast<entt::entity>(_entity);
		auto& registry = Locator::entitiesRegistry::value();
		if (_entity < 0 || !registry.Valid(object))
		{
			auto focus = Locator::camera::value().GetFocus();
			focus.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(focus.x, focus.z));
			object = ecs::archetypes::MobileStaticArchetype::Create(focus, static_cast<MobileStaticInfo>(_type), _height, 0.0f,
			                                                        0.0f, 0.0f, _scale);
		}
		const auto* started = dynamics.AddObject(object, glm::vec3(_velocity[0], _velocity[1], _velocity[2]), glm::vec3(0.0f));
		ImGui::Text("%s", started != nullptr ? "Thrown" : "Refused");
	}
}

void Physics::Update() noexcept {}

void Physics::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Physics::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
