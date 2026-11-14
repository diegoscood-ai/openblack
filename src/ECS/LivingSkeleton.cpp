/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingSkeleton.h"

#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerDeath.h"
#include "Locator.h"

namespace openblack::ecs::living_skeleton
{

void Set(entt::entity thing, bool skeleton)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// A villager changes its look and its scale for its age with the mark
	if (registry.AllOf<components::Villager>(thing))
	{
		villager::SetSkeleton(thing, skeleton);
		return;
	}
	// An animal only takes the mark
	if (auto* brain = registry.TryGet<components::AnimalBrain>(thing); brain != nullptr)
	{
		brain->status = WithSkeleton(brain->status, skeleton);
	}
}

bool Is(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return false;
	}
	if (registry.AllOf<components::Villager>(thing))
	{
		return villager::IsSkeleton(thing);
	}
	const auto* brain = registry.TryGet<const components::AnimalBrain>(thing);
	return brain != nullptr && HasSkeleton(brain->status);
}

} // namespace openblack::ecs::living_skeleton
