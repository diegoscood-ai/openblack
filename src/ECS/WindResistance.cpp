/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WindResistance.h"

#include <stdexcept>

#include "ECS/Components/NoWindResistance.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The player's entity, or entt::null
entt::entity PlayerEntity(PlayerNames player)
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	entt::entity entity = entt::null;
	try
	{
		entity = Locator::playerSystem::value().GetPlayer(player);
	}
	catch (const std::out_of_range&)
	{
		return entt::null; // no such player on this land
	}
	return entity != entt::null && Locator::entitiesRegistry::value().Valid(entity) ? entity : entt::null;
}
} // namespace

void wind_resistance::Set(PlayerNames player, uint32_t value)
{
	const auto entity = PlayerEntity(player);
	if (entity != entt::null)
	{
		Locator::entitiesRegistry::value().AssignOrReplaceState<components::NoWindResistance>(entity, value);
	}
}

uint32_t wind_resistance::Get(PlayerNames player)
{
	const auto entity = PlayerEntity(player);
	if (entity == entt::null)
	{
		return 0;
	}
	const auto* kept = Locator::entitiesRegistry::value().TryGet<const components::NoWindResistance>(entity);
	return kept != nullptr ? kept->value : 0;
}

void wind_resistance::AfterThrowFromHand(entt::entity object, PlayerNames handPlayer)
{
	if (Get(handPlayer) == 0 || !Locator::dynamicsSystem::has_value())
	{
		return;
	}
	if (auto* entry = Locator::dynamicsSystem::value().Find(object); entry != nullptr)
	{
		entry->body.ClearDrag();
	}
}
