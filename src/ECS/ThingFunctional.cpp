/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingFunctional.h"

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"

namespace openblack::ecs::thing_functional
{

bool OfThing(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return false;
	}
	if (registry.AllOf<components::Abode>(thing))
	{
		return abode_queries::IsFunctional(thing);
	}
	Facts facts {.available = IsAvailable(thing)};
	if (registry.AllOf<components::VillageTotem>(thing))
	{
		facts.kind = Kind::MultiMapFixed;
		facts.built = abodes::IsBuilt(thing);
		facts.life = life::LifeOf(thing);
		facts.nonFunctionalBelow = abodes::GetPercentRepairedForNonFunctional(thing);
	}
	else if (registry.AllOf<components::Villager>(thing))
	{
		facts.kind = Kind::Villager;
		facts.villagerTop = static_cast<uint32_t>(villager::GetState(thing, villager::Index::Top));
	}
	else if (registry.AnyOf<components::Creature, components::Animal>(thing))
	{
		facts.kind = Kind::Living;
		facts.life = life::LifeOf(thing);
	}
	return IsFunctional(facts);
}

} // namespace openblack::ecs::thing_functional
