/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Placed.h"

#include "3D/MapCoords.h"
#include "ECS/Components/PlacedAt.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using components::PlacedAt;
using components::Transform;

bool placed::MovedFrom(glm::vec3 placedAt, glm::vec3 position)
{
	return map_coords::ToFixed(position.x) != map_coords::ToFixed(placedAt.x) ||
	       map_coords::ToFixed(position.z) != map_coords::ToFixed(placedAt.z);
}

void ecs::PlaceWhereItIs(Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return;
	}
	if (const auto* transform = registry.TryGet<const Transform>(entity))
	{
		registry.AssignOrReplaceState<PlacedAt>(entity, transform->position);
	}
}

bool ecs::MovedSincePlaced(const Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* placedAt = registry.TryGet<const PlacedAt>(entity);
	return transform != nullptr && placedAt != nullptr && placed::MovedFrom(placedAt->position, transform->position);
}
