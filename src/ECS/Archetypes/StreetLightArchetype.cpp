/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StreetLightArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/StreetLight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity StreetLightArchetype::Create(const glm::vec3& position)
{
	const auto entity = CreateLoaded(position);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<StreetLight>(entity).creationStamp = ++registry.Context().streetLightCreationCounter;
	return entity;
}

entt::entity StreetLightArchetype::CreateLoaded(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<StreetLight>(entity);
	return entity;
}
