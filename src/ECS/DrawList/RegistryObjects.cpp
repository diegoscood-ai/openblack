/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RegistryObjects.h"

#include "ECS/Components/Creature.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

namespace openblack::ecs::draw_list
{

RegistryObjects::RegistryObjects(Registry& registry)
    : _registry(registry)
{
}

bool RegistryObjects::Exists(entt::entity entity) const
{
	return entity != entt::null && _registry.Valid(entity);
}

bool RegistryObjects::Available(entt::entity entity) const
{
	// An object waiting on the dead list is still in the registry, but is gone for the list
	return Exists(entity) && !_registry.AllOf<components::Unavailable>(entity);
}

bool RegistryObjects::DontDraw(entt::entity entity) const
{
	return Available(entity) && _registry.AllOf<components::DontDraw>(entity);
}

bool RegistryObjects::IsHuman(entt::entity entity) const
{
	// Every villager's 3D object is marked human as it is made, the special ones included; nothing else is
	return Exists(entity) && _registry.AllOf<components::Villager>(entity);
}

bool RegistryObjects::IsComplex(entt::entity entity) const
{
	// The creature's body is the one complex 3D object among the game's objects. The advisor spirits' are too, but
	// they are drawn by the help system and are not in the registry
	return Exists(entity) && _registry.AllOf<components::Creature>(entity);
}

bool RegistryObjects::Listed(entt::entity entity) const
{
	return Exists(entity) && _registry.AllOf<components::DrawListed>(entity);
}

void RegistryObjects::SetListed(entt::entity entity, bool listed)
{
	// A destroyed object has lost its mark with its components; the same index made again is another entity. An
	// unavailable one keeps its mark until it is cleared
	if (!Exists(entity))
	{
		return;
	}
	if (listed)
	{
		_registry.AssignOrReplaceState<components::DrawListed>(entity);
	}
	else
	{
		_registry.RemoveState<components::DrawListed>(entity);
	}
}

} // namespace openblack::ecs::draw_list
