/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureHelpSystem.h"

#include <array>
#include <chrono>
#include <string>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
constexpr uint32_t k_TurnsPerSecond =
    static_cast<uint32_t>(std::chrono::milliseconds(std::chrono::seconds(1)) / TimeSystemInterface::k_TurnDuration);
/// The messages kept for the debug readout
constexpr size_t k_ToldKept = 16;

/// The help talks only of the local player's creature: their first, as everything about "the player's creature" does
bool IsPlayersCreature(entt::entity creature)
{
	if (!Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature(players.GetLocalPlayer()) == creature;
}

/// A script holds the dialogue, or has the cinema bars in
bool DialogueTaken()
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		return false;
	}
	const auto* game = help::Get();
	const auto owner = game != nullptr ? game->GetWideScreenOwner() : 0u;
	return Locator::dialogueControlSystem::value().IsControlled(owner);
}

bool InsideTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

bool InView(const ecs::Registry& registry, entt::entity creature)
{
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::camera::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	return creature_help::LooksAt(camera.GetOrigin(), camera.GetFocus(), transform->position);
}

/// Fainted or asleep: the game's plans of dying and of sleeping, which hold back the help
bool OutOrAsleep(const ecs::Registry& registry, entt::entity creature)
{
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		const auto activity = mind->idle.activity;
		if (activity == creature_mind::Activity::Sleep || activity == creature_mind::Activity::Faint)
		{
			return true;
		}
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature); needs != nullptr)
	{
		return needs->rest == CreatureNeeds::Rest::Asleep || needs->rest == CreatureNeeds::Rest::Unconscious;
	}
	return false;
}

/// It is in one of its growing-up lesson scripts
bool InDevelopmentScript(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	return body != nullptr && body->inDevScript;
}

const GCreatureInfo* SpeciesInfo(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

creature_help::BodyThresholds ThresholdsOf(const GCreatureInfo* info)
{
	if (info == nullptr)
	{
		return {};
	}
	return {
	    .seconds = info->helpBodyProblemSeconds,
	    .lowEnergy = info->helpLowEnergy,
	    .veryLowEnergy = info->helpVeryLowEnergy,
	    .lowLife = info->helpLowLife,
	    .veryLowLife = info->helpVeryLowLife,
	    .tired = info->helpTired,
	    .veryTired = info->helpVeryTired,
	    .thirsty = info->helpThirsty,
	    .veryThirsty = info->helpVeryThirsty,
	};
}

std::optional<creature_help::Body> BodyOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	if (needs == nullptr || !needs->started)
	{
		return std::nullopt;
	}
	return creature_help::Body {
	    .energy = needs->needs.energy,
	    .life = needs->needs.life,
	    .exhaustion = needs->needs.exhaustion,
	    .dehydration = needs->needs.dehydration,
	};
}

/// A miracle's own text of the creature having learnt it, or nearly
std::optional<uint32_t> MiracleText(creature_help::Kind kind, uint32_t miracle)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& effects = Locator::infoConstants::value().magicEffect;
	if (miracle >= effects.size())
	{
		return std::nullopt;
	}
	const auto& effect = effects.at(miracle);
	return static_cast<uint32_t>(kind == creature_help::Kind::LessonMagicAction ? effect.creatureLearntEnum
	                                                                            : effect.creatureNearlyLearntEnum);
}

creature_help::Memory& MemoryOf(ecs::Registry& registry, entt::entity creature)
{
	if (auto* help = registry.TryGet<CreatureHelpMemory>(creature); help != nullptr)
	{
		return help->memory;
	}
	// Only the simulation reads it: the drawn things stay as they are
	return registry.AssignState<CreatureHelpMemory>(creature).memory;
}

/// How steadily the player's hand has been over the creature, followed every turn whatever the creature thinks
void FollowHandOver(ecs::Registry& registry, entt::entity creature)
{
	const bool over = Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().object == creature;
	auto& memory = MemoryOf(registry, creature);
	memory.handOver = creature_help::NextHandOver(memory.handOver, over);
}
} // namespace

std::optional<help::script_control::Vm> CreatureHelpSystem::SpeakingVm() const
{
	if (_vm.has_value())
	{
		return _vm;
	}
	if (!Locator::vm::has_value())
	{
		return std::nullopt;
	}
	return chlapi::ScriptVm();
}

void CreatureHelpSystem::Raise(entt::entity creature, const creature_help::Message& message)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	const auto turn = game_clock::Turn();
	auto& memory = MemoryOf(registry, creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const creature_help::Situation situation {
	    .helpOn = _helpOn,
	    .insideTemple = InsideTemple(),
	    .playersCreature = IsPlayersCreature(creature),
	    .stage = mind != nullptr ? mind->developmentPhase : creature_help::k_HelpStage,
	    .outOrAsleep = OutOrAsleep(registry, creature),
	    .dialogueTaken = DialogueTaken(),
	    .handOnCreature =
	        Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature,
	    .raisedThisTurn = _toldTurn == turn,
	    .scriptControlled = ecs::script_held::IsControlledByScript(creature),
	    .inDevelopmentScript = InDevelopmentScript(registry, creature),
	    .inView = InView(registry, creature),
	};
	const auto toldLately = creature_help::ToldLately(memory, message.kind, message.parameter, turn, k_TurnsPerSecond);
	switch (creature_help::Decide(message, situation, toldLately))
	{
	case creature_help::Outcome::Tell:
		creature_help::MarkTold(memory, message.kind, message.parameter, turn);
		_toldTurn = turn;
		Tell(creature, message);
		break;
	case creature_help::Outcome::Stack:
		creature_help::Stack(memory, message, turn, k_TurnsPerSecond);
		break;
	case creature_help::Outcome::Drop:
		break;
	}
}

void CreatureHelpSystem::Tell(entt::entity creature, const creature_help::Message& message)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = creature_help::Info(message.kind);
	const auto vm = SpeakingVm();
	std::optional<uint32_t> text;
	switch (info.telling)
	{
	case creature_help::Telling::Nothing:
	case creature_help::Telling::Unsaid:
		break;
	case creature_help::Telling::StageScript:
	{
		const auto land = Locator::mapScriptSystem::has_value()
		                      ? static_cast<uint32_t>(Locator::mapScriptSystem::value().Globals().landNumber)
		                      : 0u;
		if (const auto script = creature_help::StageScript(message.kind, land);
		    script.has_value() && vm.has_value() && vm->startScript)
		{
			for (const float pushed : creature_help::StagePushed(message.kind, land))
			{
				if (vm->pushFloat)
				{
					vm->pushFloat(pushed);
				}
			}
			vm->startScript(*script, help::script_control::k_SinglePlayerScriptTypes);
			if (creature_help::StageStartsLesson(message.kind, land))
			{
				ecs::player_creature::SetInDevScript(registry, creature, true);
			}
		}
		break;
	}
	case creature_help::Telling::PointAtCreature:
	case creature_help::Telling::JustText:
	{
		const bool miracle = message.kind == creature_help::Kind::LessonMagicAction ||
		                     message.kind == creature_help::Kind::NearlyLearntMagicAction;
		text = creature_help::Text(message.kind, message.parameter,
		                           miracle ? MiracleText(message.kind, message.parameter) : std::nullopt);
		// The help script that says it, with the text's number as its argument; other help scripts are left running
		if (text.has_value() && vm.has_value() && vm->pushFloat && vm->startScript)
		{
			vm->pushFloat(static_cast<float>(*text));
			vm->startScript(info.telling == creature_help::Telling::PointAtCreature ? "HelpForCreaturePointAtCreature"
			                                                                        : "HelpForCreatureJustText",
			                help::script_control::k_SinglePlayerScriptTypes);
		}
		break;
	}
	}
	// Told what the creature wants, the player is shown what it does: its newest action counts as seen by them
	if (message.kind == creature_help::Kind::CurrentDesire ||
	    message.kind == creature_help::Kind::SomethingInterestingOffScreen)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr && mind->learnt.has_value())
		{
			creature_learning::ShownToPlayer(mind->learnt->contexts);
		}
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature help: kind {} parameter {} text {}", static_cast<int>(message.kind),
	                    message.parameter, text.value_or(0));
	_told.push_back(
	    {.creature = creature, .kind = message.kind, .parameter = message.parameter, .text = text, .turn = game_clock::Turn()});
	if (_told.size() > k_ToldKept)
	{
		_told.erase(_told.begin());
	}
}

void CreatureHelpSystem::ProcessTurn()
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
	if (creature.has_value() && Locator::entitiesRegistry::value().Valid(*creature))
	{
		UpdateCreature(*creature);
	}
}

void CreatureHelpSystem::UpdateCreature(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	// A creature a script controls thinks nothing of its own, help included; how steadily the hand is over it is still
	// followed
	if (mind == nullptr || !mind->desires.has_value() || ecs::script_held::IsControlledByScript(creature))
	{
		FollowHandOver(registry, creature);
		return;
	}
	const auto turn = game_clock::Turn();

	// The first waiting message is raised again, unless a script holds the dialogue; whatever becomes of it, it is done
	// waiting
	if (!DialogueTaken())
	{
		if (const auto waiting = creature_help::FirstWaiting(MemoryOf(registry, creature)); waiting.has_value())
		{
			const auto entry = MemoryOf(registry, creature).entries.at(static_cast<size_t>(waiting->kind)).at(waiting->index);
			Raise(creature,
			      {.kind = waiting->kind, .parameter = entry.parameter, .thing = entry.thing, .position = entry.position});
			creature_help::Settle(MemoryOf(registry, creature), *waiting, turn);
		}
	}

	[&]() {
		const auto* info = SpeciesInfo(registry, creature);
		auto& memory = MemoryOf(registry, creature);
		// A problem with its body, at most every two minutes
		if (const auto body = BodyOf(registry, creature); body.has_value())
		{
			const auto since = creature_help::WholeSeconds(turn, memory.physicalProblemTurn, k_TurnsPerSecond);
			if (const auto problem = creature_help::BodyProblemNow(*body, ThresholdsOf(info), since); problem.has_value())
			{
				Raise(creature, {.kind = creature_help::Kind::PhysicalProblem, .parameter = static_cast<uint32_t>(*problem)});
				MemoryOf(registry, creature).physicalProblemTurn = turn;
				return;
			}
		}
		// The hand kept over it, it is told what the creature wants and why
		const auto& desires = *mind->desires;
		if (memory.handOver >= creature_help::k_HandOverToTell && mind->planner.current.has_value() && !DialogueTaken())
		{
			const auto desire = mind->planner.current->desire;
			const auto& state = desires[desire];
			const auto source = creature_learning::DominantSource(state);
			const auto type = source.has_value() ? state.sources.at(*source).type : creature_learning::k_NoTellingSource;
			if (type != creature_learning::k_NoTellingSource)
			{
				Raise(creature, {.kind = creature_help::Kind::CurrentDesire, .parameter = static_cast<uint32_t>(desire)});
			}
			return;
		}
		// A prompt on how to look after it, at most every two minutes
		std::array<float, creature_desires::k_DesireCount> values {};
		std::array<bool, creature_desires::k_DesireCount> active {};
		for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
		{
			values.at(i) = desires.desires.at(i).value;
			active.at(i) = desires.desires.at(i).activated;
		}
		const auto dominant = static_cast<Desire>(creature_help::DominantDesire(values, active));
		const auto applies = creature_help::TutorialApplies({
		    .stage = mind->developmentPhase,
		    .wantsToBuildHome = dominant == Desire::BuildHome,
		    .wantsWater = dominant == Desire::Water,
		    .wantsToPoo = dominant == Desire::Poo,
		    .carrying = Locator::creatureObjectActionSystem::has_value() &&
		                Locator::creatureObjectActionSystem::value().GetHeld(creature).has_value(),
		});
		const auto seconds = info != nullptr ? info->helpTutorialSeconds : creature_help::BodyThresholds {}.seconds;
		if (const auto prompt = creature_help::TutorialNow(memory, applies, turn, k_TurnsPerSecond, seconds);
		    prompt.has_value())
		{
			Raise(creature, {.kind = creature_help::Kind::Tutorial, .parameter = static_cast<uint32_t>(*prompt)});
			auto& after = MemoryOf(registry, creature);
			after.tutorialTurn = turn;
			after.tutorialToldTurn.at(*prompt) = turn;
		}
	}();

	// How steadily the player's hand has been over it
	FollowHandOver(registry, creature);
}

void CreatureHelpSystem::PromptKnown(entt::entity creature, creature_help::Prompt prompt)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature))
	{
		MemoryOf(registry, creature).tutorialDone.at(static_cast<size_t>(prompt)) = true;
	}
}

void CreatureHelpSystem::Fainting(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (const auto body = BodyOf(registry, creature); body.has_value())
	{
		if (const auto problem = creature_help::FaintingProblem(*body); problem.has_value())
		{
			Raise(creature, {.kind = creature_help::Kind::PhysicalProblem, .parameter = static_cast<uint32_t>(*problem)});
		}
	}
	// The game also says it was knocked out, but only once it lies dying, which holds that message back: never heard
}
