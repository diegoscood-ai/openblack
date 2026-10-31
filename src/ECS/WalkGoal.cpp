/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WalkGoal.h"

#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{

void walk_goal::Set(entt::entity walker, glm::vec2& goal, glm::vec2 to)
{
	goal = to;
	SetAltitude(walker, 0.0f);
}

void walk_goal::SetAltitude(entt::entity walker, float altitude)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(walker))
	{
		return;
	}
	if (altitude != 0.0f)
	{
		registry.AssignOrReplaceState<components::WalkGoalAltitude>(walker, altitude);
	}
	else if (registry.AllOf<components::WalkGoalAltitude>(walker))
	{
		registry.RemoveState<components::WalkGoalAltitude>(walker);
	}
}

float walk_goal::AltitudeOf(entt::entity walker)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* goal = registry.Valid(walker) ? registry.TryGet<const components::WalkGoalAltitude>(walker) : nullptr;
	return goal != nullptr ? goal->altitude : 0.0f;
}

} // namespace openblack::ecs
