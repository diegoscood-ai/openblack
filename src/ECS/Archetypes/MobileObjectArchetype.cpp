/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileObjectArchetype.h"

#include <glm/gtx/euler_angles.hpp>

#include "3D/ObjectMatrix.h"
#include "AbodeArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MobileObjectArchetype::Create(const glm::vec3& position, MobileObjectInfo type, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().mobileObject.at(static_cast<size_t>(type));

	// CallVirtualFunctionsForCreation -> Game3DObject::SetPosition 0x63B680 with x = z = 0 = AngleY(a)
	registry.Assign<Transform>(entity, position, lh_matrix::AngleY(yAngleRadians), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<MobileObject>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	// CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9): InsertMapObject (vt +0x544, 0x607250 -> 0x636830)
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
