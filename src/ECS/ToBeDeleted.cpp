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
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerHome.h"
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
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		// Villager::DeleteDependancys (0x74FD60), the part openblack has: Abode::RemoveDeletedVillagerFromAbode 0x404220
		// (the pair, the counts, the list, Town::RemoveVillager), then out of the town's homeless list and of the
		// vagrants (0x74FE4B)
		// (aproximado hasta V12) in the original the change to 13 SET_DYING runs the exit of the state left (ExitAtHome
		// 0x761B40 with row 13 +0xC0 = 0: Villager::LeaveHome 0x751FD0; SetDying is not read, R4) before the deletion;
		// RemoveDeletedVillagerFromAbode 0x404220 does not touch PresentAtHome (+0xB6). openblack's deaths outside
		// FlushDeaths (life::Kill) come straight here: LeaveHome first (nothing when FlushDeaths did it: bit 4 clear)
		ecs::villager::LeaveHome(entity);
		const auto abode = villager->abode;
		if (abode != entt::null && registry.Valid(abode) && registry.AllOf<Abode>(abode))
		{
			abode_villagers::RemoveDeletedVillagerFromAbode(abode, entity);
		}
		town_villagers::ForgetVillager(entity);
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// Animal::DeleteDependancys (0x417BA0): flock, prey and hunter links
		animal_ai::Forget(entity);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	// CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548, out of its cells
	map_cells::RemoveMapObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}

} // namespace openblack::ecs
