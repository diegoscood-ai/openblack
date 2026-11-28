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

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

#include "Creature/LeashKeys.h"
#include "ECS/CameraDomes.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/CreatureBodyKnock.h"
#include "ECS/CreatureWebPageWriter.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GamePackets.h"
#include "Input/LocalPlayer.h"
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

std::vector<entt::entity> TurnCreatures()
{
	// found through the const registry, which makes no storage when there is none
	std::vector<entt::entity> creatures;
	// a creature marked to go is off the living list, as every living is once marked, so it has no turn
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::Creature>([&creatures](entt::entity entity, const components::Creature&) {
		    if (ecs::IsAvailable(entity))
		    {
			    creatures.push_back(entity);
		    }
	    });
	// newest first, as a new living goes at the head of the living list; one without an index (-1) at the tail, in the
	// registry's order (stable)
	std::ranges::stable_sort(creatures, std::ranges::greater {}, [](entt::entity entity) { return object_index::Of(entity); });
	return creatures;
}

Systems Services()
{
	return Systems {
	    .leash = Required<Locator::leashSystem>("Locator::leashSystem"),
	    .physiology = Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem"),
	    .animation = Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem"),
	    .skin = Required<Locator::creatureSkinSystem>("Locator::creatureSkinSystem"),
	    .mind = Required<Locator::creatureMindSystem>("Locator::creatureMindSystem"),
	    .hands = Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem"),
	    .locomotion = Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem"),
	    .fight = Required<Locator::creatureFightSystem>("Locator::creatureFightSystem"),
	    .fizz = Required<Locator::creatureFizzSystem>("Locator::creatureFizzSystem"),
	    .audio = Required<Locator::creatureAudioSystem>("Locator::creatureAudioSystem"),
	    .magic = Required<Locator::magicSystem>("Locator::magicSystem"),
	};
}

std::optional<Systems> ServicesIfPresent()
{
	if (!Locator::leashSystem::has_value() || !Locator::creaturePhysiologySystem::has_value() ||
	    !Locator::creatureAnimationSystem::has_value() || !Locator::creatureSkinSystem::has_value() ||
	    !Locator::creatureMindSystem::has_value() || !Locator::creatureObjectActionSystem::has_value() ||
	    !Locator::creatureLocomotionSystem::has_value() || !Locator::creatureFightSystem::has_value() ||
	    !Locator::creatureFizzSystem::has_value() || !Locator::creatureAudioSystem::has_value() ||
	    !Locator::magicSystem::has_value())
	{
		return std::nullopt;
	}
	return Services();
}

std::vector<entt::entity> CreaturesOffTheList()
{
	// a creature with no creation index is no living (the tattoo screen's): it has its turn after the living list's
	std::vector<entt::entity> creatures;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::Creature>([&creatures](entt::entity entity, const components::Creature&) {
		    if (ecs::IsAvailable(entity) && object_index::Of(entity) < 0)
		    {
			    creatures.push_back(entity);
		    }
	    });
	return creatures;
}

void BeginTurn(const Systems& services)
{
	// the creatures whose routes are being searched share the turn's search turns, counted before any creature's turn
	services.locomotion.ShareSearchTurns();
	// the sounds the turn before queued that the drawing has not reached play first, before any body queues this turn's
	services.audio.PlayQueued();
	services.leash.BeginCreatureTurns();
	services.physiology.BeginCreatureTurns();
	services.animation.BeginCreatureTurns();
	services.skin.BeginCreatureTurns();
	services.mind.BeginCreatureTurns();
	services.locomotion.BeginCreatureTurns();
	services.fight.BeginCreatureTurns();
	services.fizz.BeginCreatureTurns();
}

void EndTurn(const Systems& services)
{
	services.leash.EndCreatureTurns();
	services.physiology.EndCreatureTurns();
	services.animation.EndCreatureTurns();
	services.skin.EndCreatureTurns();
	services.mind.EndCreatureTurns();
	services.locomotion.EndCreatureTurns();
	services.hands.EndCreatureTurns();
	services.fight.EndCreatureTurns();
	services.fizz.EndCreatureTurns();
	// once every creature has had its turn: the eyes of what is no creature (a falling creature's copy)
	services.animation.PoseTurnShared();
}

void ProcessTurn(Profiler& profiler)
{
	// the creatures of the turn, newest first, taken once at its start: one made during the turn has its first turn the
	// next one, as a living put at the list's head behind the walk
	const auto creatures = TurnCreatures();
	const auto services = Services();
	auto leashStage = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
	auto needsStage = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
	auto mindStage = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
	auto plannerStage = profiler.BeginScoped(Profiler::Stage::CreaturePlannerUpdate);
	auto learningStage = profiler.BeginScoped(Profiler::Stage::CreatureLearningUpdate);
	auto handsStage = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
	auto walkStage = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
	auto fightStage = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
	BeginTurn(services);
	// each creature's whole turn, then the next creature's, as the original's creature turn (issue #6)
	for (const auto creature : creatures)
	{
		ProcessCreature(services, creature);
	}
	EndTurn(services);
}

void ProcessCreature(const Systems& services, entt::entity creature)
{
	// as every living's, the reaction it follows ends first if it is over, just before its own turn
	services.mind.ProcessReaction(creature);
	// (openblack, guard) deleted at once by its own reaction while the deferral is off; the original's object stays in
	// memory until the dead list is processed and goes on to its turn
	if (!Locator::entitiesRegistry::value().Valid(creature))
	{
		return;
	}
	// its player's start size, while none is noted yet, before anything of the turn changes a size; its knock's marks
	// from its body as posed the turn before; its home at its temple's pen
	player_statistics::NoteCreatureStartSize(creature);
	creature_body_knock::TakeMarksOf(creature);
	player_creature::FollowTemplePen(creature);
	// the miracles it has taken on ease in, hold and wear off; what it holds of its own casts follows its hands, from its
	// body as posed the turn before; its size in the pen; a step towards the local creature's size
	magic::spell_creature::ProcessCreature(creature);
	services.magic.KeepCreatureBeamsOf(creature);
	player_creature::ShrinkInPen(creature);
	player_creature::AutoscaleOf(creature);
	// its leash and the walk back within it, a cheat's dominant desire let go of by a turn, then its needs and growth
	services.leash.ProcessCreatureTurn(creature);
	services.mind.ProcessCheatDesire(creature);
	services.physiology.ProcessCreatureTurn(creature);
	// its mind: the decision and its agenda, a knock-out when it passes out, its joint action, its plans, its learning
	services.mind.ProcessCreatureTurn(creature);
	// its sub-action: what the hands set about, its walk, the hands' turn, the fight's part (its stage included), then the
	// hands' end
	services.hands.ProcessCreatureApproach(creature);
	services.locomotion.ProcessCreatureTurn(creature);
	services.hands.ProcessCreatureTurn(creature);
	services.fight.ProcessCreatureSubAction(creature);
	services.hands.FinishCreatureTurn(creature);
	// its body from its attributes: the fatness it shows, its skin's healing
	services.physiology.ProcessShownFatnessOf(creature);
	services.skin.ProcessCreatureTurn(creature);
	// its fizz; fizzed right out for good, the rest of its turn is not taken
	services.fizz.ProcessCreatureTurn(creature);
	if (services.fizz.FizzedOut(creature))
	{
		return;
	}
	// its body's update: its breathing, its fight's states, its knock's marks, its animations by the turn with the
	// sounds they pass queued, then its pose (shape, blink, sway, the pose, a creature it holds placed), then its knock
	services.animation.ProcessCreatureTurn(creature);
	services.fight.ProcessCreatureBody(creature);
	creature_body_knock::TakeMarksOf(creature);
	systems::CreatureAnimationSystemInterface::TurnMoments passed;
	if (auto moments = services.animation.AdvanceTurnOf(creature))
	{
		passed.emplace_back(creature, std::move(*moments));
	}
	systems::CreatureAnimationSystemInterface::TurnInputs atEnd;
	if (auto inputs = AnimationInputsOf(creature, 1.0f))
	{
		atEnd.emplace_back(creature, std::move(*inputs));
	}
	services.audio.QueueTurnOf(creature, passed, atEnd);
	services.animation.PoseCreature(creature, atEnd);
	creature_body_knock::KnockOf(creature);
	// its alignment moved by the plan it carried out; every so many turns the local creature's mind and physique saved
	// and its page written; the dome over its body that keeps the player's camera out
	ecs::effects::alignment::ProcessCreature(creature);
	creature_web_page_writer::ProcessCreature(creature);
	ecs::camera_domes::PlaceCreature(Locator::entitiesRegistry::value(), creature);
}

std::optional<components::CreatureAnimationInputs> AnimationInputsOf(entt::entity creature, float turnFraction)
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(creature) || !lookup.AllOf<components::Creature, components::CreatureAnimation>(creature))
	{
		return std::nullopt;
	}
	const auto& animation = lookup.Get<const components::CreatureAnimation>(creature);
	components::CreatureAnimationInputs playing {.body = animation.body, .slots = animation.slots};
	// in the frame's order, each over what the one before set
	Required<Locator::creatureFightSystem>("Locator::creatureFightSystem").AnimationAt(creature, turnFraction, playing);
	Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem")
	    .AnimationAt(creature, turnFraction, playing);
	Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem")
	    .AnimationAt(creature, turnFraction, playing);
	return playing;
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
	// what the creatures act on sets the animations they play, which the animation then poses
	auto& objectActions = Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem");
	objectActions.UpdateDraw(turnFraction);
	// the sounds the drawing has reached play before the body is drawn again, so that a footstep's print is laid under
	// the body as it was last drawn
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureAudioUpdate);
		Required<Locator::creatureAudioSystem>("Locator::creatureAudioSystem").Update(turnFraction, gameTime);
	}
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureAnimationUpdate);
		Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").Update(turnFraction, gameTime);
	}
	// what they hold rides in the hand where the body was just posed
	objectActions.UpdateHeldDraw();
	{
		auto timed = profiler.BeginScoped(Profiler::Stage::CreatureHairUpdate);
		Required<Locator::creatureHairSystem>("Locator::creatureHairSystem").Update(gameTime);
	}
	// the footprints are drawn and fade only in the world's view (not inside the citadel nor during the falling spell's
	// film), where the land is drawn
	if (game_clock::SequenceMode() == game_clock::k_SequenceModeNone)
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
	// L, V and B do nothing while a script holds the wide screen
	const auto* helpSystem = help::Get();
	const auto key = creature_leash::KeyToAct(
	    helpSystem != nullptr && helpSystem->IsScriptWideScreen(),
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
	// L sends the hand's click on the creature, which the next game turn takes as the leash key (and as the end of the
	// leash picker's wait); V and B step the leash through the leash service
	if (*key == creature_leash::LeashKey::Leash)
	{
		game_packets::Push({.type = game_packets::Type::CreatureLeashClick, .player = player});
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
	(void)leash.Shake(player);
}
} // namespace openblack::ecs::creature_loop
