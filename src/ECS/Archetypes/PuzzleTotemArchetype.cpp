/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleTotemArchetype.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The land under a point; 0 with no land loaded
float LandUnder(const glm::vec3& point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z))
	                                           : 0.0f;
}
} // namespace

entt::entity PuzzleTotemArchetype::Create(const glm::vec3& ground, AbodeInfo info, float yAngleRadians, float scale)
{
	// a whole totem building of no town (the abode's own sinking to the land under its corners included)
	const auto entity =
	    AbodeArchetype::Create(Abode::k_NoTown, ground, info, yAngleRadians, scale, 0, 0, false, /*nearestTownWhenNone=*/false);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplaceState<Indestructible>(entity);
	const auto position = registry.Get<const Transform>(entity).position;
	const float rest = position.y - LandUnder(position);
	registry.AssignState<PuzzleTotem>(entity, PuzzleTotem {.restAltitude = rest, .altitude = rest});
	Place(entity);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Puzzle totem {} ({}) at ({:.1f}, {:.1f})", static_cast<uint32_t>(entity),
	                    static_cast<int>(info), ground.x, ground.z);
	return entity;
}

float PuzzleTotemArchetype::TravelOf(entt::entity totem)
{
	const auto half = ecs::object::ObjectHalfExtents(totem);
	return half.has_value() ? ecs::puzzle_totem::Travel(half->y, ecs::object::GetScale(totem)) : 0.0f;
}

void PuzzleTotemArchetype::Place(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.TryGet<PuzzleTotem>(totem);
	if (component == nullptr)
	{
		return;
	}
	const float share = ecs::puzzle_totem::Share(*component);
	PlaceAt(totem, ecs::puzzle_totem::AltitudeAt(component->restAltitude, share, TravelOf(totem)));
}

void PuzzleTotemArchetype::PlaceAt(entt::entity totem, float altitude)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.TryGet<PuzzleTotem>(totem);
	auto* transform = registry.TryGet<Transform>(totem);
	if (component == nullptr || transform == nullptr)
	{
		return;
	}
	component->altitude = altitude;
	const float y = LandUnder(transform->position) + altitude;
	if (transform->position.y != y)
	{
		transform->position.y = y;
		registry.SetDirty();
	}
}
