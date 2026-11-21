/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodCamera.h"

#include <utility>

#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/GodReportedCamera.h"
#include "ECS/Components/Transform.h"
#include "ECS/OtherPlayerSparkles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/PlayerSparkles.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;

std::optional<god_camera::Camera> god_camera::Choose(PlayerNames owner, PlayerNames local,
                                                     const std::optional<Camera>& gameCamera,
                                                     const std::optional<glm::vec3>& reportedOrigin,
                                                     const std::optional<glm::vec3>& citadelFocus)
{
	if (owner == PlayerNames::NEUTRAL || owner == local)
	{
		return gameCamera;
	}
	if (!reportedOrigin.has_value() || !citadelFocus.has_value())
	{
		return std::nullopt;
	}
	return Camera {.origin = *reportedOrigin, .focus = *citadelFocus};
}

std::optional<god_camera::Camera> god_camera::Of(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* body = registry.Valid(creature) ? registry.TryGet<const components::Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	std::optional<Camera> gameCamera;
	if (Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		gameCamera = Camera {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	}
	if (body->owner == PlayerNames::NEUTRAL || body->owner == local)
	{
		return Choose(body->owner, local, gameCamera, std::nullopt, std::nullopt);
	}
	// The focus its citadel set as it was made, kept after the citadel goes
	const auto set = CitadelSetOf(body->owner);
	return Choose(body->owner, local, gameCamera, other_player_sparkles::ReportedCamera(body->owner),
	              set.has_value() ? std::optional(set->focus) : std::nullopt);
}

void god_camera::NoteCitadelMade(PlayerNames player, glm::vec3 place)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = magic::players::EntityOf(player);
	if (entity == entt::null || !registry.Valid(entity))
	{
		return;
	}
	const auto focus = map_coords::ToWorld(map_coords::FromWorld(place));
	// the local player's own interface status looks at it too: its camera, and its turn's hand
	if (player == PlayerNames::PLAYER_ONE && Locator::handSystem::has_value())
	{
		Locator::handSystem::value().LookAtCitadel(focus);
	}
	registry.AssignOrReplace<components::GodReportedCamera>(
	    entity, components::GodReportedCamera {.origin = player_sparkles::CameraOverCitadel(focus), .focus = focus});
}

std::optional<god_camera::Camera> god_camera::CitadelSetOf(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto entity = magic::players::EntityOf(player);
	const auto* set =
	    entity != entt::null && registry.Valid(entity) ? registry.TryGet<const components::GodReportedCamera>(entity) : nullptr;
	if (set != nullptr)
	{
		return Camera {.origin = set->origin, .focus = set->focus};
	}
	// A citadel made before its god's player was: the camera it sets, from where it stands
	const auto citadel = worship::citadel::Of(player);
	const auto* place = citadel != entt::null ? registry.TryGet<const components::Transform>(citadel) : nullptr;
	if (place == nullptr)
	{
		return std::nullopt;
	}
	const auto focus = map_coords::ToWorld(map_coords::FromWorld(place->position));
	return Camera {.origin = player_sparkles::CameraOverCitadel(focus), .focus = focus};
}
