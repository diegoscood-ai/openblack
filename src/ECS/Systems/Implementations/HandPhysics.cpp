/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's side of the physics system: what thrown trees, pots and wood do when they hit a store or come to rest
// (their physics impact and end of physics, which the game's physics class hooks call), and the hand's hooks of
// ecs::physics::from_hand

#define LOCATOR_IMPLEMENTATIONS

#include <array>

#include <spdlog/spdlog.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HeldApply.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

std::vector<entt::entity> HandSystem::GetThrownObjects() const noexcept
{
	// everything the hand throws is a physics object: there is no other flight
	return {};
}

bool HandSystem::TreeReactToImpact(entt::entity entity, ecs::PhysicsEntry& po)
{
	// a tree or dead tree hitting any store of wood turns into wood (deleted, its resource taken, its ghost left)
	auto& registry = Locator::entitiesRegistry::value();
	const auto* hitBy = ecs::physics::hitter::HitterEntry(Locator::dynamicsSystem::value().GetState().objects, po);
	const bool hitValid = hitBy != nullptr && registry.Valid(hitBy->entity);
	if (!ecs::held_apply::TreeImpactTakes(hitValid,
	                                      hitValid && ecs::resource_stores::IsResourceStore(hitBy->entity, ResourceType::Wood)))
	{
		return true;
	}
	// the store takes the tree's resource with the PhysicsEntry still alive (the store's Supply help trigger reads
	// it), the thrower's interface as `is`
	pot_resource::Dropper dropper;
	if (po.byPlayer)
	{
		dropper = InterfaceStatus();
	}
	// openblack's roots of an uprooted tree go first (they are not part of the original's tree), as DepositInStore
	DropRoots(entity, false);
	// a building without a site already answers no to wood above, so a store that answered yes takes it; the bounce
	// below is only a guard
	if (!ecs::resource_stores::DeleteObjectAndTakeResource(hitBy->entity, entity, dropper))
	{
		return true;
	}
	registry.SetDirty();
	Locator::dynamicsSystem::value().RemoveObject(entity);
	return false;
}

entt::entity HandSystem::TreeEndPhysics(entt::entity entity, ecs::PhysicsEntry& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto position = registry.Get<const Transform>(entity).position;
	{
		// LANDED (only a gentle release sets it) on land and with no fire (hot or burning, ECS/Fire) -> planted again
		// (altitude 0, smoke, the forest, SPOT_VISUAL 0x2C, alignment: Replant); otherwise it becomes a DeadTree (the
		// same entity keeps its fire). A magic tree (the forest miracle's) only stops where it fell, neither replanted
		// nor dead: not this tree.
		const bool landedOnLand = (po.flags & ecs::PhysicsEntry::k_Landed) != 0 && IsLand(position);
		if (landedOnLand && fire::Find(entity) == nullptr)
		{
			// the body's player: the hand's that threw it, or threw what hit it (the only hand is player one's); none
			// otherwise
			Replant(entity, po.byPlayer ? std::optional(PlayerNames::PLAYER_ONE) : std::nullopt);
		}
		else
		{
			UpdateRoots(entity, true);
			MakeDeadTree(entity, po.body.velocity, false);
		}
		return entity;
	}
}

void HandSystem::TreeMoved(entt::entity entity)
{
	UpdateRoots(entity);
}

void HandSystem::TreeDropSound(entt::entity entity)
{
	// the drop sound (LANDED on land, still a tree after its end of physics): bank InGame, owned by the tree, 3D, not
	// tracked, sample 83 G_PlantTree_01 + tick count % 3, at the tree's point
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 83 + static_cast<int>(audio::TickCount() % 3)};
	options.owner = audio::Owner::Thing(entity);
	options.is3D = true;
	options.track = false;
	options.position = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
	audio::PlaySoundEffect(options);
}

bool HandSystem::PotReactToImpact(entt::entity entity, ecs::PhysicsEntry& po, const ecs::ImpactInfo& impact)
{
	// a thrown pot or pile that hits a store of its type, or a pot of its type, goes into it (the delivery, the
	// thrower's interface as `is`); else nothing happens to it
	const auto hit = impact.hitBy;
	const auto facts = ecs::held_apply::FactsFor(entity, hit);
	if (!ecs::held_apply::PotImpactTakes(ecs::IsAvailable(hit), ecs::IsAvailable(entity), facts))
	{
		return true;
	}
	pot_resource::Dropper dropper;
	if (po.byPlayer)
	{
		dropper = InterfaceStatus();
	}
	if (!ecs::resource_stores::DeleteObjectAndTakeResource(hit, entity, dropper))
	{
		return true;
	}
	Locator::dynamicsSystem::value().RemoveObject(entity);
	return false;
}

entt::entity HandSystem::PotEndPhysics(entt::entity entity, bool byHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto position = registry.Get<const Transform>(entity).position;
	if (HandPotBecomesPile(PotInfoOf(entity), IsLand(position)))
	{
		// a hand pot out of the water becomes a pile (AddResourceToPos)
		PutDownHandPot(entity, byHand ? InterfaceStatus() : pot_resource::Dropper {});
		return registry.Valid(entity) ? entity : entt::entity {entt::null};
	}
	return entity;
}

void HandSystem::RegisterPhysicsHandlers() noexcept
{
	ecs::physics::from_hand::HandHooks hooks;
	hooks.putDownHandPot = [this](entt::entity entity) { PutDownHandPot(entity); };
	// every pot class: the original's InitialisePhysicsFromHand is every pot's own
	hooks.isPot = [](entt::entity entity) { return Locator::entitiesRegistry::value().AllOf<Pot>(entity); };
	ecs::physics::from_hand::SetHandHooks(std::move(hooks));
	Locator::dynamicsSystem::value().LoadConstants();
}
