/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ToBeDeletedSystem.h"

#include <algorithm>

#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void ToBeDeletedSystem::Push(entt::entity entity)
{
	// ecs::ToBeDeleted pushes only an entity it has just marked unavailable, so it is never on the list already
	_deadList.Mark(entity);
}

void ToBeDeletedSystem::Process(bool drain)
{
	_deadList.Process(drain, [](entt::entity entity) {
		// the final delete: (approximate) openblack has no per-class delete, the entity goes
		if (auto& registry = Locator::entitiesRegistry::value(); registry.Valid(entity))
		{
			registry.Destroy(entity);
			registry.SetDirty();
		}
	});
}

void ToBeDeletedSystem::SetDeferred(bool deferred)
{
	_deferred = deferred;
}

bool ToBeDeletedSystem::Deferred() const
{
	return _deferred;
}
