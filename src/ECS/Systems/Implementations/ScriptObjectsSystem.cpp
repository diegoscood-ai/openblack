/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ScriptObjectsSystem.h"

#include "ECS/Components/Flock.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "Locator.h"

namespace openblack::ecs::systems
{

bool ScriptObjectsSystem::Register(entt::entity object, bool createdByScript)
{
	// ours has no limit on the places, so an object always finds one
	script_held::AddScriptThing(object, createdByScript);
	return true;
}

void ScriptObjectsSystem::EnterNative(uint32_t native)
{
	// only kept: no native takes control of what it is given yet
	_native = native;
}

entt::entity ScriptObjectsSystem::Fetch(entt::entity object)
{
	// ours has no common check: each native tests the object it is given itself, so it is given back unchanged
	return object;
}

void ScriptObjectsSystem::AddReference(entt::entity object)
{
	script_held::IncrementReference(object);
}

void ScriptObjectsSystem::RemoveReference(entt::entity object)
{
	script_held::DecrementReference(object);
}

void ScriptObjectsSystem::ReleaseFromScript(entt::entity object)
{
	script_held::ReleaseFromScript(object);
}

void ScriptObjectsSystem::Replace([[maybe_unused]] entt::entity from, [[maybe_unused]] entt::entity to)
{
	// nothing in ours replaces one held object with another; raffclar's replace (a dead tree taking its tree's place)
	// comes with its own behaviour change
}

void ScriptObjectsSystem::Reset()
{
	// ours does nothing when the scripts' program starts again: the held objects keep their places
}

void ScriptObjectsSystem::ClearForNewLand()
{
	// ours does nothing more at a new land: the places are on the entities, which go with the old land
}

void ScriptObjectsSystem::ReleaseUnreferenced()
{
	script_held::Process();
}

bool ScriptObjectsSystem::Disband(entt::entity container)
{
	// a flock or a dance
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container) ||
	    (!registry.AllOf<components::Flock>(container) && !dances::IsDance(registry, container)))
	{
		return false;
	}
	script_containers::Disband(static_cast<uint32_t>(container));
	return true;
}

std::vector<ScriptObjectPlace> ScriptObjectsSystem::Places() const
{
	// ours has no table: the objects with a place, numbered from 1 in the order the registry keeps them
	std::vector<ScriptObjectPlace> places;
	Locator::entitiesRegistry::value().Each<const components::ScriptHeld>(
	    [&places](entt::entity object, const components::ScriptHeld& held) {
		    if (held.hasSlot)
		    {
			    places.push_back({
			        .place = static_cast<uint16_t>(places.size() + 1),
			        .object = object,
			        .createdByScript = held.createdByScript,
			        .references = held.references,
			    });
		    }
	    });
	return places;
}

} // namespace openblack::ecs::systems
