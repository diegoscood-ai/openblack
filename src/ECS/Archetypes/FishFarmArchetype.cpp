/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmArchetype.h"

#include <limits>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishFarms.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FishFarmArchetype::Create(const glm::vec3& position, uint32_t info)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	const float y = island.GetHeightAt(glm::vec2(position.x, position.z));
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, glm::vec3(position.x, y, position.z), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& farm = registry.Assign<FishFarm>(entity);
	farm.info = info;
	// the starting stock: its food value for any food
	farm.food = ecs::fish_farms::FullStock();
	// the nearest town of any tribe, in x and z, replaces the script's town
	float nearest = std::numeric_limits<float>::max();
	registry.Each<const Town, const Transform>([&](entt::entity town, const Town&, const Transform& transform) {
		const glm::vec2 delta(transform.position.x - position.x, transform.position.z - position.z);
		const float distance2 = glm::dot(delta, delta);
		if (distance2 < nearest)
		{
			nearest = distance2;
			farm.town = town;
		}
	});

	// Rings round the farm: the first direction with sea (altitude 0) at two radii in a row gives the shoal centre, at
	// the second of them. The sea flattening is off during the search
	const auto centre = ecs::FindShoalCentre(
	    glm::vec2(position.x, position.z), [&island](glm::vec2 point) { return island.GetUnflattenedHeightAt(point) == 0.0f; });
	if (centre.has_value())
	{
		FishShoal shoal;
		ecs::InitFishShoal(shoal, glm::vec3(centre->x, y, centre->y));
		farm.shoal = shoal;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish farm at ({}, {}): shoal at ({}, {}, {})", position.x, position.z,
		                   shoal.centre.x, shoal.centre.y, shoal.centre.z);
	}
	// the farm goes at the head of the fixed list of its own cell
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}
