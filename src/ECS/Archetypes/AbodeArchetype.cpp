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
#include <stdexcept>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/AbodeSounds.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"
#include "VillageTotemArchetype.h"
#include "Worship/TownCentreSpellIcon.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = Locator::resources::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& m = extraMetrics.at(i);
		// (inferred) the metric's point turned by the object's rotation and not scaled; worship::SpecialPoint scales it,
		// the store's own call is not read
		auto translation = affine::AngleY(yAngleRadians) * glm::vec3(m[3]);
		pit.woodPiles.at(i) = PotArchetype::Create(position + translation, yAngleRadians, type, 0, true);
		++i;
	}
	assert(i == pit.woodPiles.size());
	const auto& m = extraMetrics.at(5);
	auto translation = affine::AngleY(yAngleRadians) * glm::vec3(m[3]); // (inferred) as the wood piles above
	pit.foodPile = PotArchetype::Create(position + translation, yAngleRadians, info.potForResourceFood, 0, true);
	// The store's totals are spread over its piles as StoragePitStore::AddResource does.
	ecs::StoragePitStore::AddResource(entity, ResourceType::Wood, woodAmount);
	ecs::StoragePitStore::AddResource(entity, ResourceType::Food, foodAmount);
}

void AbodeArchetype::MakeTownCentreFunctional(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	// only when the centre has no totem yet (a repaired centre is made functional again)
	bool hasTotem = false;
	registry.Each<const VillageTotem>(
	    [&](const VillageTotem& totem) { hasTotem = hasTotem || totem.townCentre == townCentre; });
	if (!hasTotem)
	{
		VillageTotemArchetype::Create(townCentre, info, yAngleRadians, scale);
	}
	// the town's centre = this when still null (as CREATE_TOWN_CENTRE)
	const auto townId = registry.Get<Abode>(townCentre).townId;
	if (const auto town = ecs::town_queries::TownByKey(townId); town != entt::null)
	{
		if (auto& component = registry.Get<Town>(town); component.centre == entt::null)
		{
			component.centre = townCentre;
		}
	}
	// then one spell icon per spell seed the town already has (at most 6). Their creation indexes are taken once, when
	// the centre is first made functional (its totem made now): a repaired centre or CHL 22 >= 1 again makes no new icon
	// (AddSpell), so the indexes do not move
	if (!hasTotem)
	{
		ecs::object_index::OnTownCentre(townId);
	}
	// the spell part (Worship/TownCentreSpellIcon.cpp): those icons and the town's worship site. The icons take no
	// creation index of their own (object_index counts them above).
	worship::town_centre::MakeFunctional(townCentre);
}

bool AbodeArchetype::MorphsWithTerrain(AbodeType type, Tribe tribe)
{
	bool morphsWithTerrain = false;
	morphsWithTerrain |= type == AbodeType::Graveyard;
	morphsWithTerrain |= type == AbodeType::StoragePit;
	if (type == AbodeType::Wonder)
	{
		// only the African, Aztec, Egyptian and Greek wonders stand on the land; every other tribe's, a wonder of no tribe
		// included, drapes over it
		morphsWithTerrain |=
		    tribe != Tribe::AFRICAN && tribe != Tribe::AZTEC && tribe != Tribe::EGYPTIAN && tribe != Tribe::GREEK;
	}
	morphsWithTerrain |= type == AbodeType::Workshop;
	morphsWithTerrain |= type == AbodeType::Citadel;
	morphsWithTerrain |= type == AbodeType::Creche;
	morphsWithTerrain |= type == AbodeType::FootballPitch;
	morphsWithTerrain |= type == AbodeType::TownCentre;
	morphsWithTerrain |= type == AbodeType::Field;
	return morphsWithTerrain;
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount, bool underConstruction,
                                    bool nearestTownWhenNone)
{
	auto& registry = Locator::entitiesRegistry::value();
	abodes::ConnectDrawMeshListener();

	// If there is no town, assign to closest (unless it is to stay of no town)
	if (!nearestTownWhenNone && ecs::town_queries::TownByKey(townId) == entt::null)
	{
		townId = Abode::k_NoTown;
	}
	else if (ecs::town_queries::TownByKey(townId) == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		// the land script's CREATE_ABODE / CREATE_TOWN_CENTRE without their town: the global town list, the first
		// always taken, then GetDistanceInMetres < best. MapCoords x, z only (FromMetres: the altitude is not read)
		const auto town = ecs::map_cells::FindNearestTownInList(map_coords::FromMetres(glm::vec2(position.x, position.z)));
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
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(type));
	const bool morphsWithTerrain = MorphsWithTerrain(info.abodeType, info.tribeType);

	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	auto& abode = registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount);
	abode.info = type;
	abode.yAngle = yAngleRadians;
	// the thing keeps the angle it was placed at
	placed_turn::KeepAngles(registry, entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	// the abode joins its town (at the head of its structures, then the town's area)
	town_placement::SetTownArea(ecs::town_queries::TownByKey(townId), entity);
	if (underConstruction)
	{
		// a plan: built 0% and not yet complete (the percent argument is not used); the life stays the new object's
		// 1.0. TownStats counts it from its MakeFunctional
		abode.buildFlags = Abode::k_UnderConstruction;
		abode.percentBuilt = 0.0f;
		abode.addedToTownStats = false;
	}
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
		// an abode that doesn't follow the land sinks to the lowest ground under the corners of its mesh box (never
		// above the origin's), but by at most max(0.2 x its 2D radius, 0.8) (Get2DRadius: scale x the larger half-extent
		// in x or z). It replaces the script's altitude.
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
		const float radius = ecs::object::Get2DRadius(entity);
		registry.Get<Transform>(entity).position.y = ground + std::max(lowest, -std::max(0.2f * radius, 0.8f));
	}

	// the smoke of a mesh with a chimney, at its final place
	if (const auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(resourceId))
	{
		Locator::chimneySmokeSystem::value().Attach(entity, *meshes.Handle(resourceId), registry.Get<Transform>(entity),
		                                            info.abodeType == AbodeType::Workshop);
	}
	// a windmill hums from the moment it is made, right after its smoke (a building just made is never unavailable)
	if (AbodeSounds::HasWindmillHum(info.abodeType, false))
	{
		if (!Locator::soundTagSystem::has_value())
		{
			throw std::logic_error("AbodeArchetype: no sound tag system for the windmill's hum");
		}
		AbodeSounds::StartWindmillHum(Locator::soundTagSystem::value(), entity);
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		// a whole one (the script's) is made functional: the town's storage pit = this (the last one wins; the
		// temporary pots). A plan's at Built
		if (const auto town = ecs::town_queries::TownByKey(townId); !underConstruction && town != entt::null)
		{
			ecs::town_stores::SetStoragePit(town, entity);
		}
		break;
	case AbodeType::Creche:
		// a whole one (the script's) is made functional: the town's creche = this when it is still null (the first one
		// wins); a plan's at Built
		if (const auto town = ecs::town_queries::TownByKey(townId); !underConstruction && town != entt::null)
		{
			auto& component = registry.Get<Town>(town);
			if (component.creche == entt::null)
			{
				component.creche = entity;
			}
		}
		break;
	case AbodeType::TownCentre:
		// a whole one is made functional; a plan's at Built (abodes::MakeFunctional)
		if (!underConstruction)
		{
			MakeTownCentreFunctional(entity, info, yAngleRadians, scale);
		}
		break;
	case AbodeType::Graveyard:
		// the Graveyard component (its dead); a whole one's MakeFunctional (one dead); a plan's at Built
		registry.Assign<Graveyard>(entity);
		if (!underConstruction)
		{
			ecs::graveyard::MakeFunctional(entity);
		}
		break;
	case AbodeType::Workshop:
		// the Workshop part, its ShowNeedsVisuals' creation index ((not ported) the object) then its wood pile, in that
		// order. A whole one's MakeFunctional (the town's list); a plan's at Built
		ecs::workshops::Create(entity);
		if (!underConstruction)
		{
			ecs::workshops::MakeFunctional(entity);
		}
		break;
	case AbodeType::Wonder:
		// SetPower(scale) and, built, the wonder added to its player
		ecs::wonders::Create(entity, scale);
		break;
	default:
		break;
	}
	// InsertMapObject: the head of every cell of its collide descriptor. (inferred) After the store's piles, the totem
	// and the spell icons made above: their order against it is not read
	ecs::map_cells::InsertMapObject(entity);
	// the workshop's insert above runs before its wood pile's: the pile goes into the cells after the workshop (its
	// creation index stays before the highlight's)
	if (info.abodeType == AbodeType::Workshop)
	{
		ecs::workshops::OnInsertedInMap(entity);
	}
	// a storage pit, a plan's or a whole one, takes its town's desire flags to their places on it as it is made
	if (info.abodeType == AbodeType::StoragePit && Locator::townDesireSystem::has_value())
	{
		if (const auto town = ecs::town_queries::TownByKey(townId); town != entt::null)
		{
			Locator::townDesireSystem::value().OnStoragePitMade(town, entity);
		}
	}
	// last: the did-you-know sign (its creation index after everything above) and the street lantern
	ecs::abodes::CreateAbodeSurroundingObjects(entity);
	return entity;
}
