/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCreate.h"

#include <glm/vec3.hpp>

#include "ECS/PhysicsEntry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Physics/Ground.h"

using namespace openblack;

ecs::PhysicsEntry* ecs::physics::script_create::PutIntoPhysics(systems::DynamicsSystemInterface& dynamics, entt::entity object,
                                                               const openblack::physics::Ground& ground)
{
	if (!dynamics.CanBecomeAPhysicsObject(object))
	{
		return nullptr;
	}
	// the start of physics in the world, standing still: AddObject refuses an immovable thing and one already in the
	// physics
	auto* po = dynamics.AddObject(object, glm::vec3(0.0f), glm::vec3(0.0f));
	if (po == nullptr)
	{
		return nullptr;
	}
	// onto the land along its slope (lowered as well as raised), the turn-start pose following, then clear of what is
	// under it
	po->body.SettleOnLand(ground, false, true);
	dynamics.SyncTurnStart(*po);
	dynamics.RaiseClearOfWhatIsUnder(*po);
	return po;
}

ecs::PhysicsEntry* ecs::physics::script_create::PutIntoPhysicsWhereItIs(systems::DynamicsSystemInterface& dynamics,
                                                                        entt::entity object)
{
	if (!dynamics.CanBecomeAPhysicsObject(object))
	{
		return nullptr;
	}
	// standing still where it is; only clear of what is under it
	auto* po = dynamics.AddObject(object, glm::vec3(0.0f), glm::vec3(0.0f));
	if (po == nullptr)
	{
		return nullptr;
	}
	dynamics.RaiseClearOfWhatIsUnder(*po);
	return po;
}

bool ecs::physics::script_create::MobileObjectStartsPhysics(MobileObjectInfo row, MobileObjectInfo kind)
{
	// the class the row is made as: poo by its row, a field crop and a creed by the row's kind, else a plain mobile object
	if (row == MobileObjectInfo::LumpOfPoo)
	{
		return true;
	}
	if (kind == MobileObjectInfo::Crop || kind == MobileObjectInfo::Creed)
	{
		return false;
	}
	// a plain mobile object starts it, the three mushroom rows aside
	return row != MobileObjectInfo::Champi && row != MobileObjectInfo::MagicMushroom && row != MobileObjectInfo::Toadstool;
}

bool ecs::physics::script_create::MakesARock(MobileStaticInfo row, MobileStaticInfo kind)
{
	// the mobile static table's kind of the rock class
	constexpr int k_RockKind = 2;
	return row != MobileStaticInfo::Bonfire && static_cast<int>(kind) == k_RockKind;
}
