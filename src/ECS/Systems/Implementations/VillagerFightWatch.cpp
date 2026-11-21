/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFightWatch.h"

#include <optional>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/FightWatchRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Town/TownCreatureAttitude.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerAnimation.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerReactions.h"

namespace openblack::ecs::villager_fight_watch
{
using components::LivingAction;

namespace
{
/// It faces the arena at the fastest of the looks
constexpr uint32_t k_LookFast = 2;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// An arena as the watchers see it: its middle in map units and its radius
struct ArenaPlace
{
	glm::ivec2 place;
	float radius;
};

/// The arena a thing is, a land's or one a fight made; none for anything else
std::optional<ArenaPlace> ArenaPlaceOf(entt::entity thing)
{
	const auto& registry = std::as_const(Entities());
	if (thing == entt::null || !registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* land = registry.TryGet<const components::Arena>(thing))
	{
		return ArenaPlace {.place = {land->at.x, land->at.z}, .radius = land->radius};
	}
	if (const auto* made = registry.TryGet<const components::CreatureFightArena>(thing))
	{
		return ArenaPlace {.place = made->place, .radius = made->radius};
	}
	return std::nullopt;
}

/// The fight on in an arena thing, with its two creatures
std::optional<systems::CreatureFightSystemInterface::ArenaView> FightIn(entt::entity thing)
{
	return Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().FightIn(thing) : std::nullopt;
}

/// How many of the reaction's followers joined it after this one: its place in the follower list, the newest first
std::optional<uint32_t> FollowerIndexOf(entt::entity villager, uint32_t reaction)
{
	const auto& registry = std::as_const(Entities());
	const auto* mine = registry.TryGet<const components::LivingReaction>(villager);
	if (mine == nullptr || mine->reaction != reaction || reaction == 0)
	{
		return std::nullopt;
	}
	uint32_t later = 0;
	const auto count = [&later, reaction, mine](uint32_t followed, uint32_t joinOrder) {
		later += followed == reaction && joinOrder > mine->joinOrder ? 1 : 0;
	};
	registry.Each<const components::LivingReaction>(
	    [&count](entt::entity, const auto& state) { count(state.reaction, state.joinOrder); });
	registry.Each<const components::AnimalBrain>(
	    [&count](entt::entity, const auto& state) { count(state.reaction, state.joinOrder); });
	registry.Each<const components::CreatureReaction>(
	    [&count](entt::entity, const auto& state) { count(state.reaction, state.joinOrder); });
	return later;
}

/// The turn a creature was last struck by a blow in a fight; 0 before any
uint32_t StruckTurnOf(entt::entity creature)
{
	const auto* record = std::as_const(Entities()).TryGet<const components::CreatureFightRecord>(creature);
	return record != nullptr ? record->struckTurn : 0;
}

uint32_t Random(uint32_t range)
{
	return villager::GameRand(range);
}
} // namespace

void SetupReactToFight(entt::entity villager, entt::entity arena, uint32_t reaction)
{
	villager_reactions::AddReaction(villager, reaction, VillagerStates::InitialiseFightReaction, arena);
}

uint8_t ReactToFightPriority([[maybe_unused]] entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !ArenaPlaceOf(found->initiator).has_value() || !FightIn(found->initiator).has_value())
	{
		return 0;
	}
	const auto& row = Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToFight));
	return static_cast<uint8_t>(row.priority & 0xFFu);
}

void WatchCreaturesFight(entt::entity villager, entt::entity arena)
{
	// the first reaction of the game's list that the arena started, the oldest first
	for (const auto& reaction : Locator::reactionSystem::value().GetReactions())
	{
		if (reaction.initiator == arena)
		{
			SetupReactToFight(villager, arena, reaction.id);
			return;
		}
	}
}

uint32_t InitialiseFightReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto arena = ArenaPlaceOf(villager_reactions::SlotObject(villager));
	if (!arena.has_value())
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 0;
	}
	const auto index = FollowerIndexOf(villager, villager_reactions::SlotReaction(villager));
	if (!index.has_value())
	{
		return 0;
	}
	const auto place = fight_watch::PlaceOnRing(arena->place, arena->radius, fight_watch::PlaceAngle(*index));
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Villager {} goes to watch the fight in arena {}, its place {} on the ring", entt::to_integral(villager),
		              entt::to_integral(villager_reactions::SlotObject(villager)), *index);
	}
	static_cast<void>(villager::SetupMoveToWithHug(villager,
	                                               glm::vec2(map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)),
	                                               VillagerStates::PerformFightReaction));
	return 1;
}

uint32_t PerformFightReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (const auto arena = ArenaPlaceOf(villager_reactions::SlotObject(villager)); arena.has_value())
	{
		static_cast<void>(villager::LookAtPos(villager, arena->place, k_LookFast));
	}
	return 1;
}

AnimId WatchFightAnimation(entt::entity villager)
{
	const auto thing = villager_reactions::SlotObject(villager);
	const auto fight = ArenaPlaceOf(thing).has_value() ? FightIn(thing) : std::nullopt;
	if (!fight.has_value() || fight->maker == entt::null || fight->opponent == entt::null)
	{
		return AnimId::PStand;
	}
	std::optional<fight_watch::TownFeelings> feelings;
	if (const auto town = villager::GetTown(villager); town != entt::null)
	{
		feelings = fight_watch::TownFeelings {.first = town_creature_attitude::AttitudeOf(town, fight->maker),
		                                      .second = town_creature_attitude::AttitudeOf(town, fight->opponent)};
	}
	const auto side = fight_watch::Supports(feelings, &Random);
	const auto supported = side == fight_watch::Side::First ? fight->maker : fight->opponent;
	const auto other = side == fight_watch::Side::First ? fight->opponent : fight->maker;
	const auto now = game_clock::Turn();
	const auto perSecond = 1000u / game_clock::MsPerTurn();
	return villager_animation::WatchFightClip(fight_watch::SecondsSince(now, StruckTurnOf(supported), perSecond),
	                                          fight_watch::SecondsSince(now, StruckTurnOf(other), perSecond), &Random);
}
} // namespace openblack::ecs::villager_fight_watch
