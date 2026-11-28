/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingTurn.h"

#include <algorithm>

#include <entt/entity/entity.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureLoop.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/WalkArrival.h"
#include "Locator.h"
#include "Magic/Spells/SpellFlock.h"

namespace openblack::ecs::living_turn
{
using namespace components;

std::vector<entt::entity> LivingList()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> list;
	registry.Each<const Villager, const LivingAction>([&list](entt::entity entity, const Villager&, const LivingAction&) {
		if (ecs::IsAvailable(entity))
		{
			list.push_back(entity);
		}
	});
	// the creatures, livings too, by their creation index with the rest; one with none (the tattoo screen's) is no living
	registry.Each<const Creature>([&list](entt::entity entity, const Creature&) {
		if (ecs::IsAvailable(entity) && object_index::Of(entity) >= 0)
		{
			list.push_back(entity);
		}
	});
	registry.Each<const Animal, const Transform>([&registry, &list](entt::entity entity, const Animal&, const Transform&) {
		// (openblack, guard) an animal is never a villager; one that were would be listed once
		if (ecs::IsAvailable(entity) && !registry.AllOf<Villager, LivingAction>(entity))
		{
			list.push_back(entity);
		}
	});
	// newest first (a new living goes at the head); an entity without an index (-1) at the tail, in
	// the order above (stable)
	std::stable_sort(list.begin(), list.end(),
	                 [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

void WalkList(const std::vector<entt::entity>& list, const std::function<void(entt::entity)>& run)
{
	// on the list = not unlinked: a living is unlinked when it is marked for deletion (ecs::IsAvailable false), or the
	// entity is gone (openblack's deletion at once while the deferral is off)
	const auto linked = [](entt::entity entity) { return ecs::IsAvailable(entity); };
	size_t i = 0;
	while (i < list.size())
	{
		const auto current = list[i];
		// the next one is read before the turn: the first one after it still on the list
		size_t next = i + 1;
		while (next < list.size() && !linked(list[next]))
		{
			++next;
		}
		run(current);
		if (next < list.size() && !linked(list[next]))
		{
			// unlinked during this turn: the loop holds it and its next is cleared. Still
			// in memory (the dead list) it takes its turn and the loop ends; gone already (the deferral off), it ends now
			if (Locator::entitiesRegistry::value().Valid(list[next]))
			{
				run(list[next]);
			}
			return;
		}
		i = next;
	}
}

void ProcessLiving(float visualTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const uint32_t turn = villager::CurrentTurn();
	// (openblack) the villagers' test hooks, before the list as LivingActionSystem::Update ran them
	villager::RunDebugHooks(turn);
	const bool animals = animal_ai::BeginAnimalsTurn(visualTime);
	// the creatures' turn's shared start before any living's turn: the search turns shared out among them, as the
	// original shares them before its walk, and every creature service's turn started
	const auto creatures = creature_loop::ServicesIfPresent();
	if (creatures.has_value())
	{
		creature_loop::BeginTurn(*creatures);
	}
	WalkList(LivingList(), [&registry, turn, animals, &creatures](entt::entity entity) {
		// a creature has its whole turn at its place in the list, its reaction first (issue #6). Its turn's start is
		// not copied: nothing reads a creature's, which is drawn from its pose
		if (registry.AllOf<Creature>(entity))
		{
			if (creatures.has_value())
			{
				creature_loop::ProcessCreature(*creatures, entity);
			}
			return;
		}
		// the position becomes the start of this turn's move (DrawPosition::turnStart). It may assign
		// DrawPosition: never inside a view over DrawPosition; the loop walks its own vector
		BeginLivingTurn(entity);
		// a flock miracle's animal: its stored point, which its shield test starts from, becomes its place too
		if (auto* flock = registry.TryGet<magic::SpellFlockAnimal>(entity); flock != nullptr)
		{
			magic::spell_flock::TurnStarted(*flock, registry.Get<const Transform>(entity).position);
		}
		if (registry.AllOf<Villager, LivingAction>(entity))
		{
			villager::ProcessReaction(entity);
			// (openblack, guard) deleted at once by its own reaction while the deferral is off; the original's object
			// stays in memory until ProcessDeadList and goes on to its ProcessState. Likewise (guard) an unlinked next
			// that still takes its turn does not move: every PathfindingSystem::Step phase excludes Unavailable, where
			// the original's walk would step it
			if (!registry.Valid(entity))
			{
				return;
			}
			villager::ProcessState(entity, turn);
		}
		else if (animals && registry.AllOf<Animal, Transform>(entity))
		{
			animal_ai::ProcessAnimal(entity); // its reaction and state
		}
	});
	if (animals)
	{
		animal_ai::EndAnimalsTurn();
	}
	if (creatures.has_value())
	{
		// what is no living (the tattoo screen's creature) after the walk, then the creatures' turn's shared end
		for (const auto creature : creature_loop::CreaturesOffTheList())
		{
			creature_loop::ProcessCreature(*creatures, creature);
		}
		creature_loop::EndTurn(*creatures);
	}
	// (pending) the original sets a game value to 20 here; its reader is not known
}

void MoveToStep(entt::entity villager)
{
	// (openblack, guard) the unit tests run state functions without the systems
	if (Locator::pathfindingSystem::has_value())
	{
		Locator::pathfindingSystem::value().Step(villager);
	}
}

uint32_t WallHugMoveToResult(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (transform == nullptr)
	{
		return 0;
	}
	const auto at = glm::xz(transform->position);
	if (const auto* finalStep = registry.TryGet<const MoveStateFinalStepTag>(villager);
	    finalStep != nullptr && walk_arrival::WalkIsOver(MoveState::FinalStep, at, finalStep->stepGoal))
	{
		return 0xA;
	}
	if (const auto* arrived = registry.TryGet<const MoveStateArrivedTag>(villager);
	    arrived != nullptr && walk_arrival::WalkIsOver(MoveState::Arrived, at, arrived->stepGoal))
	{
		return 0xA;
	}
	return 1;
}
} // namespace openblack::ecs::living_turn
