/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reactions' records of a saved game's world file, and the game's list of them, in the original's order: the
// newest first, each with its initiator, the living things that follow it from the oldest joiner on, whether it is
// stealthy, its type, its dance, the turn it started, whether it is active, its player and its radius
// (docs/wiki/engine/saved-games.md#the-workshops-and-the-reactions). A reaction is an object of no entity: its key
// is its id (effects::reactions), SourceKey{ReactionRecord, id}, by which every record that names it refers to it.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>

#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/VillagerMourningState.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Components/VillagerTeleportState.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteArenas.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::effects::reactions::Reaction;
using gamesave::ReactionRecord;
using gamesave::SavedRecord;

namespace
{
/// The reactions' ids in the game's list order: the newest first; none in a world with no reactions' service
std::vector<uint64_t> ReactionKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> ids;
	if (!openblack::Locator::reactionSystem::has_value())
	{
		return ids;
	}
	for (const auto& reaction : ecs::effects::reactions::All() | std::views::reverse)
	{
		ids.push_back(reaction.id);
	}
	return ids;
}

/// The living things that follow a reaction, from the one that joined it first, each with the stamp it joined with
std::vector<entt::entity> FollowersOf(const ecs::Registry& registry, uint32_t reaction)
{
	std::vector<std::pair<uint32_t, entt::entity>> followers;
	const auto add = [&followers, reaction](entt::entity living, uint32_t followed, uint32_t joinOrder) {
		if (followed == reaction)
		{
			followers.emplace_back(joinOrder, living);
		}
	};
	registry.Each<const ecs::components::LivingReaction>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::VillagerFireState>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::VillagerShieldState>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::VillagerTeleportState>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::VillagerMourningState>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::AnimalBrain>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	registry.Each<const ecs::components::CreatureReaction>(
	    [&add](entt::entity living, const auto& state) { add(living, state.reaction, state.joinOrder); });
	std::ranges::stable_sort(followers, {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<entt::entity> livings;
	livings.reserve(followers.size());
	for (const auto& [order, living] : followers)
	{
		livings.push_back(living);
	}
	return livings;
}

/// A loaded reaction's followers as its load kept them, for a reaction openblack has put none back on: the kept list
/// in its order, less those gone since, less those that follow another reaction now (of the entity a number names; a
/// number that names none is kept), less an animal that follows no reaction now (an animal follows every kind of
/// reaction it takes up through its brain's one reaction, so with none it is in no list, as the game's StopReacting
/// takes a living off its reaction's list), and less a villager or a creature that follows no reaction now and an earlier
/// list of the save (one written before this one) names already (not every kind of reaction keeps a state on them in
/// openblack, so whether one follows none is undetermined: the first list keeps it). A living follows one reaction at a
/// time, so no two of the original's lists name the same one
std::vector<gamesave::ObjectNumber>
KeptFollowersToWrite(std::span<const gamesave::ObjectNumber> kept, const std::function<bool(gamesave::ObjectNumber)>& gone,
                     const std::function<std::optional<entt::entity>(gamesave::ObjectNumber)>& entityOf,
                     const std::function<bool(entt::entity)>& followsAnother,
                     const std::function<bool(entt::entity)>& followsNone, const std::function<bool(entt::entity)>& isAnimal,
                     const std::function<bool(gamesave::ObjectNumber)>& listedEarlier)
{
	std::vector<gamesave::ObjectNumber> numbers;
	for (const auto number : kept)
	{
		if (gone(number))
		{
			continue;
		}
		if (const auto living = entityOf(number);
		    living.has_value() &&
		    (followsAnother(*living) || (followsNone(*living) && (isAnimal(*living) || listedEarlier(number)))))
		{
			continue;
		}
		numbers.push_back(number);
	}
	return numbers;
}

/// Whether the living follows a reaction other than this one that is still there, by any of the states that follow one
bool FollowsAnotherReaction(const ecs::Registry& registry, entt::entity living, uint32_t reaction)
{
	if (!registry.Valid(living))
	{
		return false;
	}
	const auto other = [reaction](uint32_t followed) {
		return followed != 0 && followed != reaction && ecs::effects::reactions::Find(followed) != nullptr;
	};
	const auto follows = [&registry, living, &other]<typename State>() {
		const auto* state = registry.TryGet<const State>(living);
		return state != nullptr && other(state->reaction);
	};
	return follows.template operator()<ecs::components::LivingReaction>() ||
	       follows.template operator()<ecs::components::VillagerFireState>() ||
	       follows.template operator()<ecs::components::VillagerShieldState>() ||
	       follows.template operator()<ecs::components::VillagerTeleportState>() ||
	       follows.template operator()<ecs::components::VillagerMourningState>() ||
	       follows.template operator()<ecs::components::AnimalBrain>() ||
	       follows.template operator()<ecs::components::CreatureReaction>();
}

/// Whether the living follows no reaction that is still there, by any of the states that follow one
bool FollowsNoReaction(const ecs::Registry& registry, entt::entity living)
{
	if (!registry.Valid(living))
	{
		return false;
	}
	const auto follows = [&registry, living]<typename State>() {
		const auto* state = registry.TryGet<const State>(living);
		return state != nullptr && state->reaction != 0 && ecs::effects::reactions::Find(state->reaction) != nullptr;
	};
	return !(follows.template operator()<ecs::components::LivingReaction>() ||
	         follows.template operator()<ecs::components::VillagerFireState>() ||
	         follows.template operator()<ecs::components::VillagerShieldState>() ||
	         follows.template operator()<ecs::components::VillagerTeleportState>() ||
	         follows.template operator()<ecs::components::VillagerMourningState>() ||
	         follows.template operator()<ecs::components::AnimalBrain>() ||
	         follows.template operator()<ecs::components::CreatureReaction>());
}

/// Whether a reaction written before this one (earlier in the reactions' list) writes its loaded list, having no
/// followers put back on it, and that list keeps the number
bool ListedByAnEarlierKeptReaction(const WriteContext& context, uint64_t reaction, gamesave::ObjectNumber number)
{
	for (const auto id : context.Keys(ReactionRecord::k_Type))
	{
		if (id == reaction)
		{
			return false;
		}
		if (!FollowersOf(context.GetRegistry(), static_cast<uint32_t>(id)).empty())
		{
			continue;
		}
		const auto record = context.KeptRecordOf(SourceKey {.type = ReactionRecord::k_Type, .id = id});
		const auto* other = record.has_value() ? std::get_if<ReactionRecord>(&*record) : nullptr;
		if (other != nullptr && std::ranges::contains(other->livings.numbers, number))
		{
			return true;
		}
	}
	return false;
}

/// A reaction: its thing part as its load kept it (zero for one made since), its initiator, its followers from the
/// oldest joiner, its stealth, type, dance, the turn it started, whether it is active, its player (0 for its null
/// player, which is not the neutral one) and its radius. A loaded reaction whose followers openblack has not put back
/// on their living things keeps the followers its load kept, and one whose initiator or dance its load did not make
/// names them as kept
std::optional<SavedRecord> WriteReaction(WriteContext& context, uint64_t id)
{
	if (!openblack::Locator::reactionSystem::has_value())
	{
		context.Fail("reaction " + std::to_string(id) + " has no reactions' service to be written from");
		return std::nullopt;
	}
	const auto* reaction = ecs::effects::reactions::Find(static_cast<uint32_t>(id));
	if (reaction == nullptr)
	{
		context.Fail("reaction " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	std::optional<ReactionRecord> kept;
	if (auto record = context.KeptRecordOf(SourceKey {.type = ReactionRecord::k_Type, .id = id}); record.has_value())
	{
		kept = std::get<ReactionRecord>(std::move(*record));
	}
	ReactionRecord record;
	if (kept.has_value())
	{
		record.thing = kept->thing;
	}
	// an initiator or a dance its load did not make an entity of is named as the load kept it; one gone since is none,
	// as the dance's link goes with its dance and a deleted initiator's reactions are removed with it
	const auto& registry = context.GetRegistry();
	const auto made = [&registry](entt::entity entity) { return entity != entt::null && registry.Valid(entity); };
	// a land's arena that started one is named as an arena, and the hand (its pick-up reaction) as its player's
	// interface status, the object the game makes that reaction with: the local hand's is the first player's
	const auto arena = made(reaction->initiator) ? LandArenaKeyOf(registry, reaction->initiator) : std::nullopt;
	const bool hand = made(reaction->initiator) && registry.AllOf<ecs::components::Hand>(reaction->initiator);
	record.object.number = arena.has_value() ? context.Reference(*arena)
	                       : hand            ? context.Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type,
	                                                                        .id = static_cast<uint64_t>(PlayerNames::PLAYER_ONE)})
	                       : made(reaction->initiator) ? context.Reference(reaction->initiator)
	                       : kept.has_value()          ? context.UnlessGone(kept->object.number)
	                                                   : 0;
	const auto followers = FollowersOf(context.GetRegistry(), reaction->id);
	if (followers.empty() && kept.has_value())
	{
		// as its load kept them, without those gone since (a living leaves its reaction's followers as it is deleted)
		// and without those that follow another reaction now (a living leaves one reaction as it joins another)
		record.livings.numbers = KeptFollowersToWrite(
		    kept->livings.numbers, [&context](gamesave::ObjectNumber number) { return context.IsGone(number); },
		    [&context](gamesave::ObjectNumber number) -> std::optional<entt::entity> {
			    const auto target = context.TargetOf(number);
			    return target.has_value() && target->entity != entt::null ? std::optional(target->entity) : std::nullopt;
		    },
		    [&registry, reaction](entt::entity living) { return FollowsAnotherReaction(registry, living, reaction->id); },
		    [&registry](entt::entity living) { return FollowsNoReaction(registry, living); },
		    [&registry](entt::entity living) { return registry.AllOf<ecs::components::AnimalBrain>(living); },
		    [&context, id](gamesave::ObjectNumber number) { return ListedByAnEarlierKeptReaction(context, id, number); });
	}
	else
	{
		for (const auto living : followers)
		{
			record.livings.numbers.push_back(context.Reference(living));
		}
	}
	record.firstWord = reaction->stealth ? 1 : 0;
	record.secondWord = static_cast<uint32_t>(reaction->type);
	record.second.number = made(reaction->dance) ? context.Reference(reaction->dance)
	                       : kept.has_value()    ? context.UnlessGone(kept->second.number)
	                                             : 0;
	record.thirdWord = reaction->turnCreated;
	record.fourthWord = reaction->available ? 1 : 0;
	// a reaction of nobody's (a creature a script made for no player, the creature guide) has the game's null player
	// pointer, which a save writes as no object, as for every other thing of nobody's
	const bool nobodys =
	    reaction->player.has_value() && static_cast<size_t>(std::to_underlying(*reaction->player)) >= gamesave::k_PlayerCount;
	record.third.number =
	    reaction->player.has_value() && !nobodys
	        ? context.Reference(SourceKey {.type = gamesave::k_PlayerType, .id = static_cast<uint64_t>(*reaction->player)})
	        : 0;
	record.lastWord = std::bit_cast<uint32_t>(reaction->radius);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterReactionWriters(ClassWriters& writers)
{
	writers.RegisterSource(ReactionRecord::k_Type, ReactionKeys, WriteReaction);
	writers.RegisterList(GlobalList::Reactions, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto id : context.Keys(ReactionRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ReactionRecord::k_Type, .id = id}));
		}
		return numbers;
	});
}
