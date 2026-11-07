/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageTotemArchetype.h"

#include <cstddef>
#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/DrawnWith.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/VillageTotem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

void VillageTotemArchetype::Create(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale)
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
			// the morphed town centre's extra point: (H(p) - H(pos)) + p.y
			const auto ground = land_morph::Altitude(Locator::terrainSystem::value());
			point.y = land_morph::Raised(ground, point, ground(glm::vec2(transform.position.x, transform.position.z)));
		}
	}
	const auto rotation = affine::AngleY(yAngleRadians);

	const auto plinth = registry.Create();
	ecs::object_index::Assign(plinth); // the totem is one object (the hand on top is part of it)
	registry.Assign<Transform>(plinth, point, rotation, glm::vec3(scale));
	registry.Assign<Mesh>(plinth, resources::HashIdentifier(statues.at(tribe).plinth), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	const auto top = registry.Create();
	registry.Assign<Transform>(top, point + glm::vec3(0.0f, ecs::village_totem::k_IconAbovePlinth, 0.0f), rotation,
	                           glm::vec3(scale));
	registry.Assign<Mesh>(top, resources::HashIdentifier(MeshId::BuildingSpellHand), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	// drawn in the totem's draw, right after its plinth
	registry.Assign<DrawnWith>(top, plinth, static_cast<uint8_t>(3));
	// it is given to its town's player before its town centre has it as its totem
	registry.Assign<VillageTotem>(plinth, townCentre, top, point.y, 0.0f).onCentre = false;
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().AddToPlayer(plinth);
	}
	registry.Get<VillageTotem>(plinth).onCentre = true;
}
