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
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity TownDesireFlagArchetype::Create(entt::entity town, TownDesireInfo desire, uint32_t point, MeshId mesh)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.AssignState<TownDesireFlag>(entity, TownDesireFlag {.town = town, .desire = desire, .point = point});
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// hidden until it is first placed and shown, so it is not drawn where it was made
	registry.Assign<NotDrawn>(entity);
	return entity;
}
