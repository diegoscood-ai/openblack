/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToBeDeleted.h"

#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDeath.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

void ToBeDeleted(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		// Tree::ToBeDeleted 0x74A210 / DeadTree::ToBeDeleted 0x510C90: out of its forest, the deletion listeners told
		DeleteTree(entity);
		return;
	}
	if (registry.AllOf<Villager>(entity))
	{
		// Villager::ToBeDeleted 0x7521B0 (ECS/Villager/VillagerDeath.h), when it is marked: DeleteDependancys 0x74FD60
		// (SET_DYING through the real exits, a mother's orphans, out of its abode / town / the vagrants), then
		// Living::ToBeDeleted 0x5EC0A0's StopReacting (vt +0x998: its reaction, the mourning too)
		ecs::villager::ToBeDeletedOverride(entity);
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// Animal::DeleteDependancys (0x417BA0): flock, prey and hunter links
		animal_ai::Forget(entity);
	}
	if (registry.AllOf<ScriptHighlight>(entity))
	{
		// ScriptHighlight::ToBeDeleted 0x709980: out of the highlights' list, its two effects closed
		script_highlight::OnToBeDeleted(entity);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	// CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548, out of its cells
	map_cells::RemoveMapObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}

} // namespace openblack::ecs
