/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeArchetype.h"

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& m = extraMetrics.at(i);
		auto translation = static_cast<glm::vec3>(glm::eulerAngleY(-yAngleRadians) * m[3]);
		pit.woodPiles.at(i) = PotArchetype::Create(position + translation, yAngleRadians, type, 0, true);
		++i;
	}
	assert(i == pit.woodPiles.size());
	const auto& m = extraMetrics.at(5);
	auto translation = static_cast<glm::vec3>(glm::eulerAngleY(-yAngleRadians) * m[3]);
	pit.foodPile = PotArchetype::Create(position + translation, yAngleRadians, info.potForResourceFood, 0, true);
	// The store's totals are spread over its piles as StoragePit::AddResource does.
	ecs::StoragePitStore::AddResource(entity, ResourceType::Wood, woodAmount);
	ecs::StoragePitStore::AddResource(entity, ResourceType::Food, foodAmount);
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();

	// If there is no town, assign to closest
	if (registry.Context().towns.find(townId) == registry.Context().towns.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		const auto town = Locator::townSystem::value().FindClosestTown(position);
		if (town != entt::null)
		{
			townId = registry.Get<Town>(town).id;
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Function {} has could not find closest town.", __func__, townId);
			return entt::null;
		}
	}

	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(type));
	bool morphsWithTerrain = false;
	morphsWithTerrain |= info.abodeType == AbodeType::Graveyard;
	morphsWithTerrain |= info.abodeType == AbodeType::StoragePit;
	if (info.abodeType == AbodeType::Wonder)
	{
		morphsWithTerrain |= info.tribeType == Tribe::CELTIC;
		morphsWithTerrain |= info.tribeType == Tribe::JAPANESE;
		morphsWithTerrain |= info.tribeType == Tribe::INDIAN;
		morphsWithTerrain |= info.tribeType == Tribe::NORSE;
		morphsWithTerrain |= info.tribeType == Tribe::TIBETAN;
	}
	morphsWithTerrain |= info.abodeType == AbodeType::Workshop;
	morphsWithTerrain |= info.abodeType == AbodeType::Citadel;
	morphsWithTerrain |= info.abodeType == AbodeType::Creche;
	morphsWithTerrain |= info.abodeType == AbodeType::FootballPitch;
	morphsWithTerrain |= info.abodeType == AbodeType::TownCentre;
	morphsWithTerrain |= info.abodeType == AbodeType::Field;

	const auto& transform =
	    registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount);
	auto resourceId = resources::HashIdentifier(info.meshId);
	const auto& mesh = registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (morphsWithTerrain)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}
	// (only with an island loaded: the placeholder UnloadedIsland throws, and has no materials)
	else if (Locator::terrainSystem::has_value() && !Locator::terrainSystem::value().GetMaterialInfo().empty() &&
	         Locator::resources::value().GetMeshes().Contains(resourceId))
	{
		// Abode::CallVirtualFunctionsForCreation (0x403270): an abode that doesn't follow the land sinks to the lowest
		// ground under the corners of its mesh box (Game3DObject::GetAltitudeFondation 0x63ABC0, never above the
		// origin's), but by at most max(0.2 x its 2D radius, 0.8) (Object::Get2DRadius 0x638180: scale x the larger
		// half-extent in x or z). It replaces the script's altitude.
		const auto& island = Locator::terrainSystem::value();
		const auto box = Locator::resources::value().GetMeshes().Handle(resourceId)->GetBoundingBox();
		const glm::vec2 origin(position.x, position.z);
		const float ground = island.GetHeightAt(origin);
		float lowest = 0.0f;
		for (const auto& corner : {glm::vec3(box.minima.x, 0.0f, box.minima.z), glm::vec3(box.maxima.x, 0.0f, box.minima.z),
		                           glm::vec3(box.minima.x, 0.0f, box.maxima.z), glm::vec3(box.maxima.x, 0.0f, box.maxima.z)})
		{
			const glm::vec3 world = position + transform.rotation * (corner * transform.scale);
			lowest = std::min(lowest, island.GetHeightAt(glm::vec2(world.x, world.z)) - ground);
		}
		const auto half = box.Size() * 0.5f;
		const float radius = scale * std::max(half.x, half.z);
		registry.Get<Transform>(entity).position.y = ground + std::max(lowest, -std::max(0.2f * radius, 0.8f));
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		break;
	default:
		break;
	}

	return entity;
}
