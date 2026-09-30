/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

/// An animal the scripts let go (ECS/ScriptHeld.h; research dev\tmp_dis\animals\script_flags.md).
namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Flock;
using components::Life;
using components::Transform;

void ReleaseFromScript(entt::entity entity)
{
	auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	// GScript::SetScriptState(this, 0x20 WANDER) (0x70F6CC) is not ported: fn_0041AA00 below sets its state again at
	// once. fn_0041AA00: nothing while +0x24 & 0x44 (in the physics or in the hand; openblack: the physics component
	// or the IN_HAND / FLYING states [approximated]) or g_game +0x14 & 0x8000 (the flag with which GScript::Process
	// skips the scripts' LookIn, 0x6EB6C7; openblack has no such flag, so it is never set: not tested)
	const auto top = static_cast<AnimalState>(brain->topState);
	if (physics::PhysicsObjects::IsFlying(entity) || top == AnimalState::InHand || top == AnimalState::Flying)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	const auto& info = detail::InfoOf(animal);
	if (detail::FlockOf(animal) == nullptr)
	{
		// Flock::Flock(Living*) 0x52F950 (0x41AA47): its own, radius info +0x25C (domainRadius), distance
		// (int)info +0x21C (flockDistance)
		const auto flockEntity = registry.Create();
		auto& flock = registry.Assign<Flock>(flockEntity);
		flock.domainCentre = registry.Get<const Transform>(entity).position;
		flock.savedDomainCentre = flock.domainCentre;
		flock.domainRadius = static_cast<uint16_t>(info.domainRadius);
		flock.flockDistance = static_cast<uint16_t>(static_cast<int32_t>(info.flockDistance));
		// +0x88 (maxMembers) is not written by Flock::Flock(Living*) (0x52F950), as PlaceInHand's own flock
		flock.members.push_back(entity);
		animal.flock = flockEntity;
	}
	const auto* life = registry.TryGet<const Life>(entity);
	if (life != nullptr && life->value <= 0.0f)
	{
		// 0x41AA89: SetDying; one that was dying already (status & 1) goes straight to DEAD
		const bool wasDying = (brain->status & 1) != 0;
		detail::SetDying(entity, *brain);
		if (wasDying)
		{
			detail::SetTopState(entity, *brain, AnimalState::Dead);
		}
		return;
	}
	// vt+0xB88 Animal::InteractDecideWhatToDo (0x417D80), called at once in the original; openblack runs its state
	// on the animal's next turn [approximated: one turn later]
	detail::SetTopState(entity, *brain, AnimalState::InteractDecideWhatToDo);
}

} // namespace openblack::ecs::animal_ai
