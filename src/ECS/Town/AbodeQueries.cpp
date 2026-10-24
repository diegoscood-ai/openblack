/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeQueries.h"

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerRoutine.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::abode_queries
{
using namespace components;

namespace
{
/// The abode's GAbodeInfo. (inferred) openblack's abode keeps no info pointer: the record of its number
/// and mesh, else the first of that number (as FireObjectTraits.cpp's AbodeInfoOf)
const GAbodeInfo* InfoOf(entt::entity abode)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* a = registry.TryGet<const Abode>(abode);
	if (a == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const GAbodeInfo* first = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != a->type)
		{
			continue;
		}
		if (mesh != nullptr && resources::HashIdentifier(info.meshId) == mesh->id)
		{
			return &info;
		}
		if (first == nullptr)
		{
			first = &info;
		}
	}
	return first;
}
} // namespace

bool IsAvailable(entt::entity abode)
{
	return ecs::IsAvailable(abode);
}

bool IsBuilt(entt::entity abode)
{
	return abodes::IsBuilt(abode);
}

bool IsFunctional(entt::entity abode)
{
	// IsAvailable && IsBuilt && thresholdForStopBeingFunctional < the life
	if (!IsAvailable(abode) || !IsBuilt(abode))
	{
		return false;
	}
	const auto* info = InfoOf(abode);
	if (info == nullptr || !(info->thresholdForStopBeingFunctional < life::LifeOf(abode)))
	{
		return false;
	}
	// and IsBuilt again
	return IsBuilt(abode);
}

glm::ivec2 GetArrivePos(entt::entity abode)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(abode);
	if (transform == nullptr)
	{
		return glm::ivec2(0);
	}
	const auto position = town_queries::PosOf(abode);
	// A door point, then x != 0 && z != 0
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return position;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return position;
	}
	const auto& door = meshes.Handle(mesh->id)->GetDoorPos();
	if (!door.has_value())
	{
		return position;
	}
	// The mesh's door point through the abode's placement matrix, at its map position, made whole. The angle it was
	// placed with while its rotation is still that one; otherwise the angle the rotation gives back
	const auto* placed = registry.TryGet<const Abode>(abode);
	const float yAngle = placed != nullptr && affine::AngleY(placed->yAngle) == transform->rotation
	                         ? placed->yAngle
	                         : map_cells::detail::YAngleOf(transform->rotation);
	const glm::ivec2 result = villager_routine::DoorPoint(*door, position, yAngle, transform->scale.x);
	if (result.x == 0 || result.y == 0)
	{
		return position;
	}
	return result;
}

glm::ivec2 GetPosOutside(entt::entity abode, float p1, float p2, float p3)
{
	// The door, then a spot out from it (PosOutside: a turn in p1 shares, p2 m and up to p3 more)
	const auto door = GetArrivePos(abode);
	return villager_routine::PosOutside(town_queries::PosOf(abode), door, p1, p2, p3,
	                                    [](float limit) { return game_random::GameFloatRand(limit); });
}
entt::entity FindClosestAbode(const map_coords::MapCoords& pos, std::optional<Tribe> tribe, bool roomForAdults, bool functional)
{
	// from far beyond any map, strictly nearer
	constexpr float k_Farthest = 1e12f;
	float best = k_Farthest;
	entt::entity closest = entt::null;
	map_cells::ForEachTown([&](entt::entity town) {
		for (const auto abode : town_stats::AbodesOf(town))
		{
			const float distance = gutils::GetDistanceInMetres(object::MapCoordsOf(abode), pos);
			if (!(distance < best))
			{
				continue;
			}
			// (inferred) the abode's tribe is its town's
			if (tribe.has_value() && town_queries::TribeOf(town) != *tribe)
			{
				continue;
			}
			if (functional && !IsFunctional(abode))
			{
				continue;
			}
			if (roomForAdults && (abode_villagers::GetRoomLeftForAdults(abode) <= 0 || !IsFunctional(abode)))
			{
				continue;
			}
			best = distance;
			closest = abode;
		}
		return true;
	});
	return closest;
}
} // namespace openblack::ecs::abode_queries
