/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureSwap.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LocalPlayer.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureNaming.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The action a creature plays as its mind leaves for the other body
constexpr size_t k_SwapAnimation = 109;
/// The beam between the two creatures lasts this many turns
constexpr int k_SwapBeamTurns = 70;
/// The action of the game's table a creature carries out while it swaps
constexpr std::string_view k_SwapAction = "SwapMindWithOtherCreature";
/// How pressing that plan is: its usefulness and priorities
constexpr float k_SwapPlanScore = 1.0f;

/// A body's size and alignment as they were before any spell on it changed them
creature_swap::Shape OwnShape(const ecs::Registry& registry, entt::entity creature)
{
	const auto& body = registry.Get<const Creature>(creature);
	const creature_spells::SavedBody now {.size = body.size, .strength = body.strength, .alignment = body.alignment};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	const auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, now) : now;
	return {.size = saved.size, .alignment = saved.alignment};
}

/// Whether a swap beam is still up: made, and its effect not yet over
bool BeamUp(const ecs::Registry& registry, entt::entity beam)
{
	return beam != entt::null && registry.Valid(beam) && ecs::IsAvailable(beam);
}

/// The bones of a creature as posed for the game turn; none before it is first posed
std::span<const glm::mat4> GameBones(const ecs::Registry& registry, entt::entity creature)
{
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	return pose != nullptr ? creature_turn_pose::Safe(*pose) : std::span<const glm::mat4> {};
}

/// Every third game turn of the easing, while its beam is up and the other is a creature, the beam fires a plasma from
/// a random bone of the creature to a random bone of the other, on the game's shared random numbers: the creature's
/// bone, the other's, then the alpha. A body not posed yet (which the original's always is) draws nothing
void FireSwapPlasma(const ecs::Registry& registry, entt::entity creature, const CreatureSwap& swap)
{
	if (!registry.Valid(swap.other) || !registry.AllOf<Creature>(swap.other) || !BeamUp(registry, swap.beam) ||
	    !creature_swap::IsPlasmaTurn(game_clock::Turn()))
	{
		return;
	}
	const auto own = GameBones(registry, creature);
	const auto other = GameBones(registry, swap.other);
	if (own.empty() || other.empty())
	{
		return;
	}
	const auto ownBone = glm::vec3(own[game_random::GameRand(static_cast<uint32_t>(own.size()))][3]);
	const auto otherBone = glm::vec3(other[game_random::GameRand(static_cast<uint32_t>(other.size()))][3]);
	const auto alphaDraw = game_random::GameRand(creature_swap::k_PlasmaAlphaDraws);
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddSpotVisualPlasma(swap.beam,
		                                                     creature_swap::SwapPlasma(ownBone, otherBone, alphaDraw));
	}
}

/// Whether no spell is on the creature any more
bool SpellFree(const ecs::Registry& registry, entt::entity creature)
{
	const auto* spells = registry.Valid(creature) ? registry.TryGet<const CreatureSpells>(creature) : nullptr;
	return spells == nullptr || std::ranges::all_of(spells->spells.slots, [](const creature_spells::Slot& slot) {
		       return slot.phase == creature_spells::Phase::Off;
	       });
}
} // namespace

bool CreatureMindSystem::SwapMinds(entt::entity from, entt::entity to)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ready = [&registry](entt::entity creature) {
		return creature != entt::null && registry.Valid(creature) &&
		       registry.AllOf<Creature, CreatureMindState, Transform>(creature);
	};
	if (from == to || !ready(from) || !ready(to))
	{
		return false;
	}
	for (const auto creature : {from, to})
	{
		SetUpMind(creature, registry.Get<const Creature>(creature).species, registry.Get<CreatureMindState>(creature));
	}

	// The leashes come off both first, for each that has a player
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		for (const auto creature : {from, to})
		{
			if (creature_swap::HasPlayer(registry.Get<const Creature>(creature).owner) && leashes.IsLeashed(creature))
			{
				leashes.TakeOff(creature);
			}
		}
	}

	// What each body keeps for itself: its name, and its size and alignment as they are now, which the mind file it
	// takes up does not change. Each mind takes the other body's size and alignment, as they were before any spell, to
	// grow into over the swap.
	auto nameFrom = ecs::creature_naming::NameOf(registry, from);
	auto nameTo = ecs::creature_naming::NameOf(registry, to);
	const auto ownFrom = OwnShape(registry, from);
	const auto ownTo = OwnShape(registry, to);
	const auto speciesFrom = registry.Get<const Creature>(from).species;
	const auto speciesTo = registry.Get<const Creature>(to).species;

	auto* physiology = Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
	const auto* leash = Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr;
	auto fileFrom = ecs::player_creature::ToMindFile(registry, *this, physiology, leash, from);
	auto fileTo = ecs::player_creature::ToMindFile(registry, *this, physiology, leash, to);
	if (!fileFrom.has_value() || !fileTo.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Creatures {} and {} can't swap minds: one has no mind yet",
		                   entt::to_integral(from), entt::to_integral(to));
		return false;
	}

	// Each takes up the other's mind file at once, its mind and what its body takes of the file, then has its own name
	// back
	const auto takeUp = [this, &registry, physiology](entt::entity creature, creaturemind::MindFileData file,
	                                                  std::u16string ownName) {
		auto shared = std::make_shared<const creaturemind::MindFileData>(std::move(file));
		auto& mind = registry.Get<CreatureMindState>(creature);
		// the new mind replaces any stage that waited for an earlier file
		mind.scriptStage.reset();
		mind.pendingFile = shared;
		TakeUpFile(creature, mind);
		if (const auto body = creature_mind_body::FromMindFile(*shared); body.has_value())
		{
			ecs::player_creature::TakeUpBody(registry, physiology, creature, *body);
		}
		static_cast<void>(ecs::player_creature::SetName(registry, creature, std::move(ownName)));
	};
	takeUp(from, std::move(*fileTo), std::move(nameFrom));
	takeUp(to, std::move(*fileFrom), std::move(nameTo));

	// each keeps the beam of an earlier swap, which the next beam closes
	const auto beamOf = [&registry](entt::entity creature) {
		const auto* earlier = registry.TryGet<const CreatureSwap>(creature);
		return earlier != nullptr ? earlier->beam : entt::null;
	};
	const auto beamFrom = beamOf(from);
	const auto beamTo = beamOf(to);
	registry.AssignOrReplaceState<CreatureSwap>(from, CreatureSwap {.other = to, .target = ownTo, .beam = beamFrom});
	registry.AssignOrReplaceState<CreatureSwap>(to, CreatureSwap {.other = from, .target = ownFrom, .beam = beamTo});

	// The mind that came from the first creature is as far towards learning each miracle as it was, for its new species
	if (const auto* tables = GetTables(); tables != nullptr)
	{
		auto& mind = registry.Get<CreatureMindState>(to);
		if (mind.learnt.has_value())
		{
			auto& seen = mind.learnt->knowledge.miraclesSeen;
			const auto multiplierBefore = creature_mind_tables::MiracleMultiplier(creature::InfoRow(speciesFrom));
			const auto multiplierAfter = creature_mind_tables::MiracleMultiplier(creature::InfoRow(speciesTo));
			for (size_t miracle = 1; miracle < std::min(seen.size(), tables->miracles.size()); ++miracle)
			{
				const auto timesToSee = tables->miracles[miracle].timesToSee;
				seen[miracle].count = creature_swap::RescaledSightings(
				    seen[miracle].count, creature_watching::TimesNeeded(timesToSee, multiplierBefore),
				    creature_watching::TimesNeeded(timesToSee, multiplierAfter));
			}
		}
	}

	// Both carry out the swap, as a plan whose score is 1 rather than the most a score can be
	for (const auto& [creature, other] : std::array {std::pair {from, to}, std::pair {to, from}})
	{
		static_cast<void>(
		    ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer,
		                             .action = k_SwapAction,
		                             .actionObject = other,
		                             .score = k_SwapPlanScore,
		                             .agenda = std::vector {creature_mind::Step {.kind = creature_mind::Step::Kind::SwapMind,
		                                                                         .animation = k_SwapAnimation}}}));
	}

	// The owners swap: the player now has the second creature, which they can lead as they did the first
	const auto ownerFrom = registry.Get<const Creature>(from).owner;
	const auto ownerTo = registry.Get<const Creature>(to).owner;
	if (ownerFrom != ownerTo)
	{
		if (Locator::leashSystem::has_value())
		{
			auto& leashes = Locator::leashSystem::value();
			leashes.SetOwner(from, ownerTo);
			leashes.SetOwner(to, ownerFrom);
			leashes.ClaimOnArrival(to);
			leashes.ClaimOnArrival(from);
		}
		else
		{
			registry.Get<Creature>(from).owner = ownerTo;
			registry.Get<Creature>(to).owner = ownerFrom;
		}
	}
	// Each player is given the other creature, the first creature's player first, even when one player owns both: that
	// player then has the first creature as theirs. As each player is given one, the totems of the player's towns wear
	// its icon
	for (const auto creature : {to, from})
	{
		const auto owner = registry.Get<const Creature>(creature).owner;
		if (!creature_swap::HasPlayer(owner))
		{
			continue;
		}
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().AddCreature(creature);
		}
		if (Locator::villageTotemSystem::has_value())
		{
			Locator::villageTotemSystem::value().PlayersCreatureSet(owner);
		}
	}
	// The leashes a player has taught go with the creature they lead
	auto* leashFrom = registry.TryGet<CreatureLeash>(from);
	auto* leashTo = registry.TryGet<CreatureLeash>(to);
	if (leashFrom != nullptr && leashTo != nullptr)
	{
		std::swap(leashFrom->known, leashTo->known);
	}

	// The second creature takes notice of reactions from now on, and the first takes none, whatever each did before
	registry.RemoveState<CreatureReactionsOff>(to);
	if (!registry.AllOf<CreatureReactionsOff>(from))
	{
		registry.AssignState<CreatureReactionsOff>(from);
	}

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creatures {} and {} swapped minds", entt::to_integral(from),
	                   entt::to_integral(to));
	return true;
}

creature_mind::Senses::SwapView CreatureMindSystem::SwapViewOf(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* swap = registry.TryGet<const CreatureSwap>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (swap == nullptr || body == nullptr)
	{
		return {.spellsGone = SpellFree(registry, creature), .settled = true};
	}
	return {.spellsGone = SpellFree(registry, creature) && SpellFree(registry, swap->other),
	        .settled = creature_swap::IsSettled({.size = body->size, .alignment = body->alignment}, swap->target)};
}

void CreatureMindSystem::StepSwap(entt::entity creature, const creature_mind::Commands& commands)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (commands.finishSpells)
	{
		// Every spell on it is brought to its end
		if (auto* spells = registry.TryGet<CreatureSpells>(creature); spells != nullptr)
		{
			static_cast<void>(creature_spells::TryFinishAll(spells->spells));
		}
	}
	if (commands.startSwapBeam && Locator::particleSystem::has_value())
	{
		const auto& body = registry.Get<const Creature>(creature);
		auto& particles = Locator::particleSystem::value();
		// the beam of an earlier swap that is still up closes first
		auto* swap = registry.TryGet<CreatureSwap>(creature);
		if (swap != nullptr && BeamUp(registry, swap->beam))
		{
			particles.CloseDownSpotVisual(swap->beam);
		}
		const auto beam =
		    particles.StartSpotVisual(SpotVisualType::MagicBeamCreatureSwap, registry.Get<const Transform>(creature).position,
		                              k_SwapBeamTurns, entt::null, 1.0f);
		if (swap != nullptr)
		{
			swap->beam = beam;
		}
		particles.SetSpotVisualPlayer(beam, static_cast<int>(creature_swap::BeamPlayer(body.owner, creature::LocalPlayer())));
	}
	if (commands.easeSwap)
	{
		const auto* swap = registry.TryGet<const CreatureSwap>(creature);
		if (swap == nullptr)
		{
			return;
		}
		auto& body = registry.Get<Creature>(creature);
		const auto shape = creature_swap::Ease({.size = body.size, .alignment = body.alignment}, swap->target);
		// the body is given its new size at once, as a script's scale gives it, and shows its new alignment from its
		// next drawing
		body.alignment = shape.alignment;
		static_cast<void>(ecs::player_creature::SetCreatureScale(registry, creature, shape.size));
		FireSwapPlasma(registry, creature, *swap);
	}
}
