/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesireFlagArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/TownDesireFlags.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity TownDesireFlagArchetype::Create(entt::entity town, TownDesireInfo desire, uint32_t point, MeshId mesh, float scale,
                                             const glm::vec3& place)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.AssignState<TownDesireFlag>(
	    entity,
	    TownDesireFlag {.town = town, .desire = desire, .point = point, .place = place, .madeAt = place, .objectScale = scale});
	registry.Assign<Transform>(entity, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// hidden until it is first placed and shown, so it is not drawn where it was made
	registry.Assign<NotDrawn>(entity);
	// in the map where it is made, as any object: at the tail of its cell's fixed list (its type, prayer, counts as
	// fixed) and in its land block's array, which the object draw list reads to step and draw it
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}

void TownDesireFlagArchetype::CreateForTown(entt::entity town, const glm::vec3& townPosition)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().townDesire;
	const auto townCoords = map_coords::FromMetres({townPosition.x, townPosition.z});
	// the ground under the start places, only with an island loaded (the placeholder throws, and has no materials)
	const LandIslandInterface* island =
	    Locator::terrainSystem::has_value() && !Locator::terrainSystem::value().GetMaterialInfo().empty()
	        ? &Locator::terrainSystem::value()
	        : nullptr;
	TownDesireFlags made;
	made.flags.fill(entt::null);
	for (size_t i = 0; i < info.size() && i < made.flags.size(); ++i)
	{
		const auto place = info.at(i).worshipSiteSlot;
		const auto point = ecs::town_desire_flags::PointOf(place);
		if (!point.has_value())
		{
			continue;
		}
		const auto start = map_coords::ToWorld(island, ecs::town_desire_flags::StartPlace(townCoords, place));
		made.flags.at(i) = Create(town, static_cast<TownDesireInfo>(i), *point, info.at(i).worshipSiteMesh,
		                          info.at(i).worshipSiteScale, start);
	}
	registry.AssignState<TownDesireFlags>(town, made);
}
