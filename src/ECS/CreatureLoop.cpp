/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLoop.h"

#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <utility>

#include "Creature/LeashKeys.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/Spells/SpellCreature.h"
#include "Profiler.h"

namespace openblack::ecs::creature_loop
{
namespace
{
/// A creature service the loop calls. Release builds have no entt assert, so a missing one stops the game with its
/// name rather than a null dereference.
template <typename Slot>
auto& Required(const char* slot)
{
	if (!Slot::has_value())
	{
		std::fprintf(stderr, "ecs::creature_loop: no %s in the locator\n", slot);
		std::abort();
	}
	return Slot::value();
}
} // namespace

void ProcessTurn(Profiler& profiler)
{
	// each creature's home follows its player's temple's pen while the temple stands
	player_creature::FollowTemplePens();
	// in the pen it is drawn smaller
	player_creature::ShrinkInPens();
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
		Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem").ProcessTurn();
	}
	Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").ProcessTurn();
	Required<Locator::creatureSkinSystem>("Locator::creatureSkinSystem").ProcessTurn();
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
		Required<Locator::leashSystem>("Locator::leashSystem").ProcessTurn();
	}
	auto& mind = Required<Locator::creatureMindSystem>("Locator::creatureMindSystem");
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		mind.ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreaturePlannerUpdate);
		mind.PlanTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLearningUpdate);
		mind.LearnTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem").ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem").ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Required<Locator::creatureFightSystem>("Locator::creatureFightSystem").ProcessTurn();
	}
	// the miracles the creatures have taken on ease in, hold and wear off
	magic::spell_creature::ProcessTurn();
	// then a creature a script set to follow the local player's creature's size takes a step towards it, from the size
	// its spells have just left
	player_creature::Autoscale();
	// then, after the minds and what the creatures did, the fatness each body shows takes its step
	Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem").ProcessShownFatness();
	// last, the bodies are posed for the turn, at the size they have come to and as they play at the turn's end: the
	// readers that move to this pose read, before this, the turn before's
	Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").PoseTurn(AnimationInputsAt(1.0f));
}

systems::CreatureAnimationSystemInterface::TurnInputs AnimationInputsAt(float turnFraction)
{
	systems::CreatureAnimationSystemInterface::TurnInputs inputs;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::Creature, const components::CreatureAnimation>(
	        [&inputs](entt::entity entity, const components::Creature&, const components::CreatureAnimation& animation) {
		        inputs.emplace_back(entity,
		                            components::CreatureAnimationInputs {.body = animation.body, .slots = animation.slots});
	        });
	if (inputs.empty())
	{
		return inputs;
	}
	// in the frame's order, each over what the one before set
	const auto& fight = Required<Locator::creatureFightSystem>("Locator::creatureFightSystem");
	const auto& locomotion = Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem");
	const auto& objectActions = Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem");
	for (auto& [entity, playing] : inputs)
	{
		fight.AnimationAt(entity, turnFraction, playing);
		locomotion.AnimationAt(entity, turnFraction, playing);
		objectActions.AnimationAt(entity, turnFraction, playing);
	}
	return inputs;
}

void UpdateFrame(float turnFraction, uint32_t frameGameMs, Profiler& profiler)
{
	auto stage = profiler.BeginScoped(Profiler::Stage::CreatureFrame);
	const auto gameTime = std::chrono::duration<float, std::milli>(static_cast<float>(frameGameMs));
	// the fight, the walk, the body and what the creatures act on keep their own stages for the turn, which runs in the
	// same profiler frame: their frame steps are timed under the creatures' frame only
	Required<Locator::creatureFightSystem>("Locator::creatureFightSystem")
	    .Update(turnFraction, static_cast<float>(frameGameMs));
	Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem").Update(turnFraction);
	Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem")
	    .Update(static_cast<float>(frameGameMs) * 0.001f);
	// what the creatures act on sets the animations they play, which the animation then poses
	auto& objectActions = Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem");
	objectActions.UpdateDraw(turnFraction);
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureAnimationUpdate);
		Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").Update(gameTime);
	}
	// what they hold rides in the hand where the body was just posed
	objectActions.UpdateHeldDraw();
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureHairUpdate);
		Required<Locator::creatureHairSystem>("Locator::creatureHairSystem").Update(gameTime);
	}
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureAudioUpdate);
		Required<Locator::creatureAudioSystem>("Locator::creatureAudioSystem").Update(gameTime);
	}
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureFootprintsUpdate);
		Required<Locator::footprintSystem>("Locator::footprintSystem").Update(gameTime);
	}
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureSkinUpdate);
		Required<Locator::creatureSkinSystem>("Locator::creatureSkinSystem").Update();
	}
}

void UpdateLeash(float frameGameSeconds, Profiler& profiler)
{
	auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLeashFrame);
	Required<Locator::leashSystem>("Locator::leashSystem").Update(frameGameSeconds);
}

void OnLoadMap()
{
	Required<Locator::footprintSystem>("Locator::footprintSystem").Reset();
}

void ProcessLeashKeys(const input::GameActionInterface& actions)
{
	const auto key = creature_leash::PressedKey(
	    [&actions](input::BindableActionMap action) { return actions.Get(action) && actions.GetChanged(action); });
	if (!key.has_value() || !Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leash = Locator::leashSystem::value();
	const auto player = openblack::creature::LocalPlayer();
	const auto led = leash.PlayersCreature(player);
	if (!led.has_value() || !leash.IsLeashable(*led))
	{
		return;
	}
	leash.PressKey(player, *key);
}

void ReleaseLeashHeldInHand(PlayerNames player)
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leash = Locator::leashSystem::value();
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value() || !leash.IsLeashed(*creature) || leash.TiedTo(*creature).has_value())
	{
		return;
	}
	(void)leash.TakeOffHeldLeash(player);
}
} // namespace openblack::ecs::creature_loop
