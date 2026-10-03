/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's side of the physics system: what thrown trees, pots and wood do when they hit a store or come to rest
// (Tree/DeadTree/Pot ReactToPhysicsImpact and EndPhysics), and the hand's hooks of physics::from_hand

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <array>

#include <spdlog/spdlog.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

std::vector<entt::entity> HandSystem::GetThrownObjects() const noexcept
{
	// everything the hand throws is a physics object (Object::ThrowObjectFromHand 0x6385E0): there is no other flight
	return {};
}

void HandSystem::RegisterPhysicsHandlers() noexcept
{
	using physics::PhysicsClass;
	using physics::PhysicsObjects;
	// Tree / DeadTree::ReactToPhysicsImpact: hitting a wood store turns it into wood (DeleteObjectAndTakeResource)
	const auto treeImpact = [this](entt::entity entity, physics::PhysicsObject& po, const physics::ImpactInfo&) {
		auto& registry = Locator::entitiesRegistry::value();
		if (po.hitBy != nullptr && registry.Valid(po.hitBy->entity) && registry.AllOf<StoragePit>(po.hitBy->entity))
		{
			PhysicsObjects::RemoveObject(entity);
			DepositInStore(entity, po.hitBy->entity);
			return false;
		}
		return true;
	};
	PhysicsObjects::ClassHandlers tree;
	tree.reactToImpact = treeImpact;
	tree.endPhysics = [this](entt::entity entity, physics::PhysicsObject& po) {
		auto& registry = Locator::entitiesRegistry::value();
		const auto position = registry.Get<const Transform>(entity).position;
		{
			// Tree::EndPhysics 0x74B830: LANDED (only a gentle release sets it, InitialisePhysicsFromHand 0x6372F2) on
			// land (IsLand 0x74B8A5) and with no FireEffect (+0x44, hot or burning, ECS/Fire) -> planted again (altitude
			// 0, SmokyStuff, the forest, SPOT_VISUAL 0x2C, alignment: Replant); otherwise fn_00510B70 makes it a DeadTree
			// (0x74BBD9; the same entity keeps its fire: fn_00730960 moves it in the DeadTree ctor 0x510880). A tree
			// with +0x5C & 2 (0x74B882) only gets Fixed::EndPhysics (stays where it fell, neither replanted nor dead):
			// that bit is set by the MagicTree ctor alone (0x5FCF8D), the trees of the forest miracle, not in this tree.
			const bool landedOnLand = (po.flags & physics::PhysicsObject::Landed) != 0 && IsLand(position);
			if (landedOnLand && fire::Find(entity) == nullptr)
			{
				Replant(entity);
			}
			else
			{
				UpdateRoots(entity, true);
				MakeDeadTree(entity, po.body.velocity, false);
			}
			// PhysicsObject::RemoveObject 0x646B2E..0x646B44: LANDED on land -> Tree::DropSfx 0x74BC60, replanted or not:
			// GAudio::PlaySoundEffect 0x429E30 (0x74BD03) with bank InGame (GAudio+0x3AC), owner the tree (+0x20), is3D 1,
			// track 0, sample 83 G_PlantTree_01 + GetTickCount() % 3, at the tree's point (x, altitude + height, z)
			if (landedOnLand)
			{
				audio::PlayOptions options;
				options.sample = {audio::Bank(audio::SfxBank::InGame), 83 + static_cast<int>(audio::TickCount() % 3)};
				options.owner = audio::Owner::Thing(entity);
				options.is3D = true;
				options.track = false;
				const auto* now = registry.TryGet<const Transform>(entity);
				options.position = now != nullptr ? now->position : position;
				audio::PlaySoundEffect(options);
			}
			return entity;
		}
	};
	tree.moved = [this](entt::entity entity) { UpdateRoots(entity); };
	PhysicsObjects::SetClassHandlers(PhysicsClass::Tree, std::move(tree));
	PhysicsObjects::ClassHandlers deadTree;
	deadTree.reactToImpact = treeImpact;
	PhysicsObjects::SetClassHandlers(PhysicsClass::DeadTree, std::move(deadTree));
	PhysicsObjects::ClassHandlers pot;
	pot.endPhysics = [this](entt::entity entity, physics::PhysicsObject&) {
		auto& registry = Locator::entitiesRegistry::value();
		const auto position = registry.Get<const Transform>(entity).position;
		if (const auto type = PotInfoOf(entity); (type == PotInfo::HandWood || type == PotInfo::HandFood) && IsLand(position))
		{
			// Pot::EndPhysics: a hand pot out of the water becomes a pile (AddResourceToPos)
			PutDownHandPot(entity);
			return registry.Valid(entity) ? entity : entt::entity {entt::null};
		}
		return entity;
	};
	PhysicsObjects::SetClassHandlers(PhysicsClass::Pot, std::move(pot));

	physics::from_hand::HandHooks hooks;
	hooks.putDownHandPot = [this](entt::entity entity) { PutDownHandPot(entity); };
	hooks.isHandPot = [](entt::entity entity) {
		const auto type = PotInfoOf(entity);
		return type == PotInfo::HandWood || type == PotInfo::HandFood;
	};
	physics::from_hand::SetHandHooks(std::move(hooks));
	PhysicsObjects::LoadConstants();
}
