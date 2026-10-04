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
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Abodes.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownStores.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"
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

	const auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& m = extraMetrics.at(i);
		// (inferido) the metric's point turned by the object's rotation and not scaled; worship::SpecialPoint scales it
		// (Game3DObject::GetSpecialPos 0x63B040), the store's own call is not read
		auto translation = lh_matrix::AngleY(yAngleRadians) * glm::vec3(m[3]);
		pit.woodPiles.at(i) = PotArchetype::Create(position + translation, yAngleRadians, type, 0, true);
		++i;
	}
	assert(i == pit.woodPiles.size());
	const auto& m = extraMetrics.at(5);
	auto translation = lh_matrix::AngleY(yAngleRadians) * glm::vec3(m[3]); // (inferido) as the wood piles above
	pit.foodPile = PotArchetype::Create(position + translation, yAngleRadians, info.potForResourceFood, 0, true);
	// The store's totals are spread over its piles as StoragePit::AddResource does.
	ecs::StoragePitStore::AddResource(entity, ResourceType::Wood, woodAmount);
	ecs::StoragePitStore::AddResource(entity, ResourceType::Food, foodAmount);
}

namespace
{
/// TownCentre::CreateTotemIfNecessary 0x743DA0 (a built town centre of a town, as MakeFunctional 0x743E80 calls it) ->
/// TotemStatue::Create 0x737CC0: the tribe's plinth (GTotemStatueInfo, table 0xDA1D18 by Abode::GetTribeType) at the
/// town centre's special point 6 (TownCentre::GetTotemPos 0x743F20, through its full matrix; raised like the
/// morphing town centre, by the land under the point minus the land under the origin), with its Y angle and scale
/// (0x737B20), and the icon on the plinth's top. Both static, no foundation sink (not an Abode). Facing the worship
/// site (AddToPlayer 0x738130) and the creature's icon (SetPlayersCreature 0x7381C0) wait for those systems: the
/// icon is the hand (BuildingSpellHand) of a player without a creature.
void CreateTotemStatue(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto tribe = static_cast<size_t>(info.tribeType);
	const auto& statues = Locator::infoConstants::value().totemStatue;
	if (tribe >= statues.size() || !Locator::terrainSystem::has_value() ||
	    Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return;
	}
	const auto& transform = registry.Get<Transform>(townCentre);
	glm::vec3 point = transform.position;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto centreMesh = resources::HashIdentifier(info.meshId);
	if (meshes.Contains(centreMesh))
	{
		if (const auto& extra = meshes.Handle(centreMesh)->GetExtraMetrics(); extra.size() > 6)
		{
			point = transform.position + transform.rotation * (glm::vec3(extra[6][3]) * transform.scale);
			// GetExtraPos 0x80FF20 of the morphed town centre (IsStaticMorphable 0x81002E): (H(p) - H(pos)) + p.y
			// (0x8100B7..0x8100D0)
			const auto ground = land_morph::Altitude(Locator::terrainSystem::value());
			point.y = land_morph::Raised(ground, point, ground(glm::vec2(transform.position.x, transform.position.z)));
		}
	}
	const auto rotation = lh_matrix::AngleY(yAngleRadians); // TotemStatue: Object::GetWorldMatrix 0x638200

	const auto plinth = registry.Create();
	ecs::object_index::Assign(plinth); // TotemStatue is one Object (the hand on top is part of it)
	registry.Assign<Transform>(plinth, point, rotation, glm::vec3(scale));
	registry.Assign<Mesh>(plinth, resources::HashIdentifier(statues.at(tribe).plinth), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	const auto top = registry.Create();
	registry.Assign<Transform>(top, point + glm::vec3(0.0f, TotemStatue::k_PlinthTop, 0.0f), rotation, glm::vec3(scale));
	registry.Assign<Mesh>(top, resources::HashIdentifier(MeshId::BuildingSpellHand), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	registry.Assign<TotemStatue>(plinth, townCentre, top, point.y, 0.0f);
}
} // namespace

void AbodeArchetype::MakeTownCentreFunctional(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians,
                                              float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	// TownCentre::CreateTotemIfNecessary 0x743DA0: only when the centre has no totem yet (a repaired centre is made
	// functional again, Abode::Repaired 0x4047B0)
	bool hasTotem = false;
	registry.Each<const TotemStatue>(
	    [&](const TotemStatue& totem) { hasTotem = hasTotem || totem.townCentre == townCentre; });
	if (!hasTotem)
	{
		CreateTotemStatue(townCentre, info, yAngleRadians, scale);
	}
	// 0x743EAB: the town's +0x9A4 = this when still null (as CREATE_TOWN_CENTRE 0x71577C)
	const auto townId = registry.Get<Abode>(townCentre).townId;
	if (const auto town = registry.Context().towns.find(townId); town != registry.Context().towns.end())
	{
		if (auto& component = registry.Get<Town>(town->second); component.centre == entt::null)
		{
			component.centre = townCentre;
		}
	}
	// MakeFunctional: then one spell icon per spell seed the town already has (at most 6). Their creation indexes are
	// taken once, when the centre is first made functional (its totem made now): a repaired centre or CHL 22 >= 1 again
	// makes no new icon (AddSpell), so the indexes do not move
	if (!hasTotem)
	{
		ecs::object_index::OnTownCentre(townId);
	}
	// TownCentre::MakeFunctional 0x743E80's spell part (Worship/TownCentreSpellIcon.cpp): those icons and the
	// town's worship site. The icons take no creation index of their own (object_index counts them above).
	worship::town_centre::MakeFunctional(townCentre);
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount, bool underConstruction)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (openblack) the Abode class's tap handlers, once (the vtable's InterfaceValidToTap / InterfaceTap)
	abodes::RegisterTapHandler();
	abodes::ConnectDrawMeshListener();

	// If there is no town, assign to closest
	if (registry.Context().towns.find(townId) == registry.Context().towns.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		// fn_00552FF0 (the land script's CREATE_ABODE / CREATE_TOWN_CENTRE without their town, cases 8 and 9: 0x7156A6,
		// 0x7157FF): the global town list, the first always taken, then GetDistanceInMetres < best. MapCoords x, z only
		// (FromMetres: the altitude is not read)
		const auto town =
		    ecs::map_cells::FindNearestTownInList(ecs::map_coords::FromMetres(glm::vec2(position.x, position.z)));
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
	    registry.Assign<Transform>(entity, position, lh_matrix::AngleY(yAngleRadians), glm::vec3(scale)); // 0x638200
	auto& abode = registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount);
	abode.info = type; // +0x28
	if (underConstruction)
	{
		// MultiMapFixed ctor 0x52E1E0: bit 1 set -> +0x5C = 0 and bit 3 clear (the percent argument is not used); the
		// life stays the Object ctor's 1.0 (V6_pending §3). TownStats counts it from its MakeFunctional (+0x7C bit 1)
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
		// Abode::CallVirtualFunctionsForCreation (0x403270): an abode that doesn't follow the land sinks to the lowest
		// ground under the corners of its mesh box (Game3DObject::GetAltitudeFondation 0x63ABC0, never above the
		// origin's), but by at most max(0.2 x its 2D radius, 0.8) (Get2DRadius, Object 0x638180: scale x the larger
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
		const float radius = ecs::object::Get2DRadius(entity); // vt +0x64 (0x40327E, 0x40329A)
		registry.Get<Transform>(entity).position.y = ground + std::max(lowest, -std::max(0.2f * radius, 0.8f));
	}

	// Abode::CallVirtualFunctionsForCreation 0x403200: the smoke of a mesh with a chimney, at its final place
	if (const auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(resourceId))
	{
		ecs::chimney_smoke::Attach(entity, *meshes.Handle(resourceId), registry.Get<Transform>(entity),
		                           info.abodeType == AbodeType::Workshop);
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		// Abode::Init 0x403130 -> MakeFunctional of a whole one (the script's): StoragePit::MakeFunctional 0x732F30 ->
		// Town::SetStoragePit 0x73EA60 (town +0x30 = this, the last one wins; the temporary pots). A plan's at Built
		if (const auto town = registry.Context().towns.find(townId);
		    !underConstruction && town != registry.Context().towns.end())
		{
			ecs::town_stores::SetStoragePit(town->second, entity);
		}
		break;
	case AbodeType::Creche:
		// Creche::MakeFunctional 0x50AB50: town +0x744 = this when it is still null (0x50AB72; the first one wins), for
		// a whole one (the script's); a plan's at Built
		if (const auto town = registry.Context().towns.find(townId);
		    !underConstruction && town != registry.Context().towns.end())
		{
			auto& component = registry.Get<Town>(town->second);
			if (component.creche == entt::null)
			{
				component.creche = entity;
			}
		}
		break;
	case AbodeType::TownCentre:
		// TownCentre::MakeFunctional 0x743E80 of a whole one; a plan's at Built (abodes::MakeFunctional)
		if (!underConstruction)
		{
			MakeTownCentreFunctional(entity, info, yAngleRadians, scale);
		}
		break;
	case AbodeType::Graveyard:
		// the Graveyard class (+0xC4 its dead, 0x595CB0..0x595FB0); a whole one's MakeFunctional 0x595E00 (+0x748, one
		// dead); a plan's at Built
		registry.Assign<Graveyard>(entity);
		if (!underConstruction)
		{
			ecs::graveyard::MakeFunctional(entity);
		}
		break;
	case AbodeType::Workshop:
		// its ShowNeedsVisuals and wood pile (openblack doesn't make them yet)
		ecs::object_index::Skip(2);
		break;
	default:
		break;
	}
	// the ScriptHighlight of the civic buildings with a did-you-know (all tribes but the African one's meshes have the
	// entrance point it needs)
	const bool civic = info.abodeType == AbodeType::StoragePit || info.abodeType == AbodeType::Creche ||
	                   info.abodeType == AbodeType::Workshop || info.abodeType == AbodeType::Wonder ||
	                   info.abodeType == AbodeType::Graveyard;
	if (civic && info.tribeType != Tribe::AFRICAN)
	{
		ecs::object_index::Skip(1);
	}

	// Abode::CallVirtualFunctionsForCreation -> MultiMapFixed 0x52E890+0x184: InsertMapObject (vt +0x544, 0x52E650),
	// the head of every cell of its NewCollideDescriptor. (inferido) After the store's piles, the totem and the spell
	// icons made above: their order against it is not read
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}
