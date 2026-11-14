/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectMeasures.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

float object_measures::TwoDRadius([[maybe_unused]] const Registry& registry, entt::entity entity)
{
	return ecs::object::Get2DRadius(entity);
}

float object_measures::Height([[maybe_unused]] const Registry& registry, entt::entity entity)
{
	return ecs::object::GetHeight(entity);
}

float object_measures::RoutePlanRadius([[maybe_unused]] const Registry& registry, entt::entity entity, entt::entity creature)
{
	return ecs::object::GetRoutePlanRadius(entity, creature);
}

bool object_measures::WalkedUpToItself(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<Abode, Field, Feature, MobileStatic, Tree>(entity);
}

std::optional<glm::vec3> object_measures::PositionOf(const Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}
