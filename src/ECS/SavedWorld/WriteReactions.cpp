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
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>

#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/VillagerMourningState.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Components/VillagerTeleportState.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::effects::reactions::Reaction;
using gamesave::ReactionRecord;
using gamesave::SavedRecord;

namespace
{
/// The reactions' ids in the game's list order: the newest first
std::vector<uint64_t> ReactionKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> ids;
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

/// A reaction: its thing part as its load kept it (zero for one made since), its initiator, its followers from the
/// oldest joiner, its stealth, type, dance, the turn it started, whether it is active, its player (0 for its null
/// player, which is not the neutral one) and its radius. A loaded reaction whose followers openblack has not put back
/// on their living things keeps the followers its load kept, and one whose initiator or dance its load did not make
/// names them as kept
std::optional<SavedRecord> WriteReaction(WriteContext& context, uint64_t id)
{
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
	// an initiator or a dance its load did not make an entity of is named as the load kept it
	const auto& registry = context.GetRegistry();
	const auto made = [&registry](entt::entity entity) { return entity != entt::null && registry.Valid(entity); };
	record.object.number = made(reaction->initiator) ? context.Reference(reaction->initiator)
	                       : kept.has_value()        ? kept->object.number
	                                                 : 0;
	const auto followers = FollowersOf(context.GetRegistry(), reaction->id);
	if (followers.empty() && kept.has_value())
	{
		record.livings = kept->livings;
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
	                       : kept.has_value()    ? kept->second.number
	                                             : 0;
	record.thirdWord = reaction->turnCreated;
	record.fourthWord = reaction->available ? 1 : 0;
	record.third.number =
	    reaction->player.has_value()
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
