/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptMarkerArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity ScriptMarkerArchetype::Create(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	// the tag tells it from other things (its script object type, script_type::TypeOf); nothing draws it
	registry.AssignState<ScriptMarker>(entity);
	return entity;
}
