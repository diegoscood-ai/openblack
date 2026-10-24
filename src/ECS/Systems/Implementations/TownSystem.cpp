/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownSystem.h"

#include <limits>

#include "3D/MapCoords.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerHome.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity TownSystem::FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const
{
	return town_villagers::FindAbodeWithSpaceInTown(town, villager, leastScore);
}

entt::entity TownSystem::FindClosestTown(const glm::vec3& point) const
{
	return map_cells::GetNearestTown(map_coords::FromWorld(point), std::numeric_limits<float>::max());
}

bool TownSystem::AddVillagerToTown(entt::entity town, entt::entity villager)
{
	return town_villagers::AddVillagerToTown(town, villager);
}

void TownSystem::AddVillagerToAbode(entt::entity abode, entt::entity villager)
{
	abode_villagers::AddVillagerToAbode(abode, villager);
}

bool TownSystem::MakeHomeless(entt::entity villager)
{
	return ecs::villager::MakeHomelessNoStateChange(villager);
}

namespace
{
/// Our clear-area filters: anything fixed but a tree (BlocksTownClearArea), or any object (IsObject)
town_queries::ClearAreaFilter FilterOf(TownSystemInterface::ClearAreaFilter filter)
{
	if (filter == TownSystemInterface::ClearAreaFilter::BlocksTown)
	{
		return &town_queries::BlocksTownClearArea;
	}
	return &town_queries::IsObject;
}
} // namespace

bool TownSystem::CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter, entt::entity ignore) const
{
	return town_queries::CheckForClearArea(town_queries::ToMapCoords(point), radius, FilterOf(filter), ignore);
}

std::optional<glm::vec2> TownSystem::FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
                                                   ClearAreaFilter filter, entt::entity ignore) const
{
	const auto found =
	    town_queries::FindClearArea(town_queries::ToMapCoords(point), searchRadius, step, radius, FilterOf(filter), ignore);
	if (!found.has_value())
	{
		return std::nullopt;
	}
	return town_queries::ToMetres(*found);
}

glm::vec2 TownSystem::GetCongregationPos(entt::entity town)
{
	return town_queries::ToMetres(town_queries::GetCongregationPos(town));
}

void TownSystem::BuildingCreated(entt::entity town, glm::vec2 position, float radius)
{
	plans::CheckWhenNewBuildingCreated(town, map_coords::FromMetres(position), radius);
}

bool TownSystem::IsInStateOfEmergency(entt::entity town) const
{
	const auto* t = Locator::entitiesRegistry::value().TryGet<const Town>(town);
	return t != nullptr && town_queries::IsInStateOfEmergency(*t);
}

void TownSystem::SetInStateOfEmergency(entt::entity town)
{
	town_emergency::SetInStateOfEmergency(town);
}

void TownSystem::ProcessTurn()
{
	// Each town, the players' in order and the neutral one's last
	map_cells::ForEachTown([](entt::entity town) {
		town_emergency::ProcessTownEmergency(town);
		return true;
	});
}
