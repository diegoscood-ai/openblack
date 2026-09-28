/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

// Environment-variable test hooks (OPENBLACK_HAND_TEST_*, OPENBLACK_CAMERA_FLY, ...), run once when the landscape exists.
void HandSystem::RunDebugHooks() noexcept
{
	if (const char* at = std::getenv("OPENBLACK_HAND_TEST_ROCK"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			const float y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
			if (std::getenv("OPENBLACK_HAND_TEST_NO_BOULDER") == nullptr)
			archetypes::MobileStaticArchetype::Create(glm::vec3(x + 6.0f, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + 6.0f, z)), z), MobileStaticInfo::Boulder1Chalk, 0.0f, 0.0f, 0.0f,
			                                          0.0f, 1.0f);
			Locator::entitiesRegistry::value().SetDirty();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: boulder spawned at ({}, {}, {})", x, y, z);
			// ...and a wood pile next to it, to test the multi pick up (HandWood) and special_hold fill.
			const float px = x;
			const float py = Locator::terrainSystem::value().GetHeightAt(glm::vec2(px, z));
			// OPENBLACK_HAND_TEST_FOOD=1 spawns a food pile instead.
			const bool food = std::getenv("OPENBLACK_HAND_TEST_FOOD") != nullptr;
			archetypes::PotArchetype::Create(glm::vec3(px, py, z), 0.0f, food ? PotInfo::MagicFood : PotInfo::WoodPile_1, food ? 1000 : 4000);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: {} pile spawned at ({}, {}, {})", food ? "food" : "wood", px, py, z);
		}
	}
	// Debug: OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz" flies the camera there (close-up screenshots).
	if (const char* fly = std::getenv("OPENBLACK_CAMERA_FLY"); fly != nullptr && Locator::camera::has_value())
	{
		glm::vec3 o(0.0f);
		glm::vec3 f(0.0f);
		if (std::sscanf(fly, "%f,%f,%f,%f,%f,%f", &o.x, &o.y, &o.z, &f.x, &f.y, &f.z) == 6)
		{
			Locator::camera::value().GetModel().SetFlight(o, f);
		}
	}
	// Debug: OPENBLACK_PRINT_ALTITUDE="x,z" logs the landscape height there (LH3DIsland::GetAltitude).
	if (const char* at = std::getenv("OPENBLACK_PRINT_ALTITUDE"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Altitude at ({}, {}): {:.7f}", x, z,
			                   Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)));
		}
	}
	// Debug: OPENBLACK_DUMP_STATIC_GAPS=1 logs, for every MobileStatic, the gap between its lowest vertex and the
	// landscape under it (negative = buried), with its rotation and with the transposed rotation.
	if (std::getenv("OPENBLACK_DUMP_STATIC_GAPS") != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& meshes = Locator::resources::value().GetMeshes();
		const auto& terrain = Locator::terrainSystem::value();
		registry.Each<const Transform, const Mesh, const MobileStatic>(
		    [&](entt::entity, const Transform& t, const Mesh& mesh, const MobileStatic& statics) {
			    if (!meshes.Contains(mesh.id))
			    {
				    return;
			    }
			    const auto l3d = meshes.Handle(mesh.id);
			    float gapA = std::numeric_limits<float>::max();
			    float gapB = std::numeric_limits<float>::max();
			    const auto rt = glm::transpose(t.rotation);
			    for (const auto& sub : l3d->GetSubMeshes())
			    {
				    for (const auto& v : sub->GetCollisionPositions())
				    {
					    const auto a = t.position + t.rotation * (t.scale * v);
					    const auto b = t.position + rt * (t.scale * v);
					    gapA = std::min(gapA, a.y - terrain.GetHeightAt(glm::vec2(a.x, a.z)));
					    gapB = std::min(gapB, b.y - terrain.GetHeightAt(glm::vec2(b.x, b.z)));
				    }
			    }
			    SPDLOG_LOGGER_INFO(spdlog::get("game"), "Static gap: type {} mesh {} at ({:.2f},{:.2f}) scale {:.3f} gap {:.3f} transposed {:.3f} localMinY {:.3f}",
			                       static_cast<int>(statics.type),
			                       static_cast<int>(Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics.type)).meshId),
			                       t.position.x, t.position.z, t.scale.x, gapA, gapB, l3d->GetBoundingBox().minima.y);
		    });
	}
	// Debug: OPENBLACK_HAND_TEST_STORE_TAKE="wood,food" takes that much from every storage pit (store visuals).
	if (const char* take = std::getenv("OPENBLACK_HAND_TEST_STORE_TAKE"); take != nullptr)
	{
		uint32_t wood = 0;
		uint32_t food = 0;
		if (std::sscanf(take, "%u,%u", &wood, &food) == 2)
		{
			std::vector<entt::entity> stores;
			Locator::entitiesRegistry::value().Each<const StoragePit>([&](entt::entity e, const StoragePit&) { stores.push_back(e); });
			for (const auto store : stores)
			{
				const auto w = StoragePitStore::RemoveResource(store, ResourceType::Wood, wood);
				const auto f = StoragePitStore::RemoveResource(store, ResourceType::Food, food);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store took wood {} food {}, left wood {} food {}", w, f,
				                   StoragePitStore::GetResource(store, ResourceType::Wood),
				                   StoragePitStore::GetResource(store, ResourceType::Food));
			}
		}
	}
	// Debug: OPENBLACK_HAND_TEST_TREE="x,z" plants a beech there, and logs the nearest village store.
	if (const char* at = std::getenv("OPENBLACK_HAND_TEST_TREE"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
			archetypes::TreeArchetype::Create(1, position, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			Locator::entitiesRegistry::value().SetDirty();
			float best = std::numeric_limits<float>::max();
			glm::vec3 store(0.0f);
			Locator::entitiesRegistry::value().Each<const StoragePit, const Transform>(
			    [&](entt::entity, const StoragePit&, const Transform& t) {
				    if (glm::distance(t.position, position) < best)
				    {
					    best = glm::distance(t.position, position);
					    store = t.position;
				    }
			    });
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: beech planted at ({}, {}); nearest store at ({:.1f}, {:.1f}) d={:.1f}",
			                   x, z, store.x, store.z, best);
			// OPENBLACK_HAND_TEST_TREE="x,z,dead": a second beech 8 m away, felled as if thrown towards +x.
			if (std::strstr(at, "dead") != nullptr)
			{
				const glm::vec3 p2(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z + 8.0f)), z + 8.0f);
				MakeDeadTree(archetypes::TreeArchetype::Create(1, p2, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f),
				             glm::vec3(1.0f, 0.0f, 0.0f));
			}
			// OPENBLACK_HAND_TEST_TREE="x,z,roots": MSH_T_ROOTS and MSH_T_ROOTS_PILE next to the tree, bounding boxes logged.
			if (std::strstr(at, "roots") != nullptr)
			{
				auto& registry = Locator::entitiesRegistry::value();
				auto& meshes = Locator::resources::value().GetMeshes();
				for (const auto& [id, dx] : {std::pair {MeshId::TreeRoots, -6.0f}, std::pair {MeshId::TreeRootsPile, 6.0f},
				                            std::pair {MeshId::TreeBeech, 12.0f}})
				{
					const auto meshId = resources::HashIdentifier(id);
					if (!meshes.Contains(meshId))
					{
						continue;
					}
					const auto& box = meshes.Handle(meshId)->GetBoundingBox();
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: mesh {} box min ({:.2f},{:.2f},{:.2f}) max ({:.2f},{:.2f},{:.2f})",
					                   static_cast<int>(id), box.minima.x, box.minima.y, box.minima.z, box.maxima.x, box.maxima.y,
					                   box.maxima.z);
					if (dx != 0.0f)
					{
						const auto e = registry.Create();
						const glm::vec3 p(x + dx, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + dx, z)), z);
						registry.Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
						registry.Assign<Mesh>(e, meshId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
						if (std::strstr(at, "alpha") != nullptr)
						{
							registry.Assign<Alpha>(e, std::getenv("OPENBLACK_TEST_ALPHA") ? static_cast<float>(std::atof(std::getenv("OPENBLACK_TEST_ALPHA"))) : 0.5f);
						}
					}
				}
				registry.SetDirty();
			}
			// OPENBLACK_HAND_TEST_TREE="x,z,store": an oak put straight into the nearest village store.
			if (std::strstr(at, "store") != nullptr)
			{
				const auto oak = archetypes::TreeArchetype::Create(1, store, TreeInfo::Oak, true, 0.0f, 1.0f, 1.0f);
				if (const auto pit = FindWoodStore(store); pit)
				{
					const auto dump = [&](const char* when) {
						std::string piles;
						for (const auto pile : Locator::entitiesRegistry::value().Get<StoragePit>(*pit).woodPiles)
						{
							piles += fmt::format(" {}", Locator::entitiesRegistry::value().Get<Pot>(pile).amount);
						}
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store wood piles {}:{} abode wood {}", when, piles,
						                   Locator::entitiesRegistry::value().Get<Abode>(*pit).woodAmount);
					};
					dump("before");
					DepositInStore(oak, *pit);
					dump("after");
				}
			}
		}
	}
}
