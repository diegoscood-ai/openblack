/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToBeDeleted.h"

#include <algorithm>
#include <vector>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
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

namespace
{
/// g_game->GameLists.GameThings (head +0x205D1C, count +0x205D20): index 0 is the head
struct DeadThing
{
	entt::entity entity;
	bool passed; ///< GameThing flags +0xA bit 1: it has been through one pass
};
std::vector<DeadThing> g_DeadList;
bool g_Deferred = false;

/// GameThing::ProcessDead 0x56FAA0 on g_DeadList[index]
void ProcessDead(size_t index, bool drain)
{
	auto& thing = g_DeadList[index];
	if (!thing.passed && !drain)
	{
		thing.passed = true;
		return;
	}
	const auto entity = thing.entity;
	g_DeadList.erase(g_DeadList.begin() + static_cast<std::ptrdiff_t>(index));
	// Delete() (vt +8: `delete this`, or the class's own, e.g. Abode::Delete 0x402C10): (approximate) openblack has no
	// per-class Delete, the entity goes
	if (auto& registry = Locator::entitiesRegistry::value(); registry.Valid(entity))
	{
		registry.Destroy(entity);
		registry.SetDirty();
	}
}
} // namespace

void ToBeDeleted(entt::entity entity, bool now)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	// GameThing::ToBeDeleted 0x56FB70: already unavailable, nothing (it is on the list)
	if (registry.AllOf<Unavailable>(entity))
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
	// CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548, out of its cells
	map_cells::RemoveMapObject(entity);
	if (now || !g_Deferred)
	{
		// `now` (Delete at once, 0x56FB70), or the deferral still off (Hito 3 step 3): out of the physics too
		physics::PhysicsObjects::RemoveObject(entity);
		registry.Destroy(entity);
		registry.SetDirty();
		return;
	}
	// 0x56FB70: UNAVAILABLE (+0xA bit 0), bit 1 clear, pushed at the head. The physics body stays: the original does not
	// take it out here; PhysicsObject::GameTurnUpdate drops the unavailable ones at the start of the next turn (0x645018;
	// Fisicas, documentacion/physics/dead_list_physics.md)
	registry.Assign<Unavailable>(entity);
	g_DeadList.insert(g_DeadList.begin(), DeadThing {entity, false});
}

bool IsAvailable(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && !registry.AllOf<Unavailable>(entity);
}

void ProcessDeadList(bool drain)
{
	do
	{
		// 0x56FB10: from the head, the next taken before each one; things a Delete marks go to the head, unseen this pass
		std::vector<entt::entity> pass;
		pass.reserve(g_DeadList.size());
		for (const auto& thing : g_DeadList)
		{
			pass.push_back(thing.entity);
		}
		for (const auto entity : pass)
		{
			const auto it = std::find_if(g_DeadList.begin(), g_DeadList.end(),
			                             [entity](const DeadThing& thing) { return thing.entity == entity; });
			if (it != g_DeadList.end())
			{
				ProcessDead(static_cast<size_t>(it - g_DeadList.begin()), drain);
			}
		}
	} while (drain && !g_DeadList.empty());
}

void SetDeferredDeletion(bool deferred)
{
	g_Deferred = deferred;
}

bool DeferredDeletion()
{
	return g_Deferred;
}

} // namespace openblack::ecs
