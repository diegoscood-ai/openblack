/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flocks' records of a saved game's world file, and the game's list of them, newest first, in the original's order
// (docs/wiki/engine/saved-games.md#the-town-centre-the-flocks-and-the-building-sites): the container part (its domain's centre,
// no info, its player), three words the game works out as it writes them, its shepherd, town and temple, its members from the
// head, then its own values. openblack loads no flock yet: every flock written is one made in play, and a loaded game's flocks
// are written from the records its load kept, with their list.

#include <cstdint>

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Temple.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::Flock;
using gamesave::FlockRecord;
using gamesave::SavedRecord;

namespace
{
/// The id of no info: an animal's own flock's, and the default flock info's (flocks::k_DefaultFlockInfo)
constexpr uint32_t k_NoInfo = 0xFFFFFFFF;
/// The three words the game works out as it writes a flock: the same three for every flock
constexpr std::array<uint32_t, 3> k_ComputedWords {0, 0xFFFFFFFF, 0xFFFFFFFF};

/// A place in the map units a record keeps
gamesave::MapCoords SavedCoords(glm::vec3 point)
{
	const auto coords = map_coords::FromWorld(point);
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// The temple whose alignment flock it is, none for any other flock
entt::entity HeartOf(const ecs::Registry& registry, entt::entity flock)
{
	entt::entity found = entt::null;
	registry.Each<const ecs::components::CitadelHeart>([&found, flock](entt::entity heart, const auto& component) {
		if (found == entt::null && component.flock == flock)
		{
			found = heart;
		}
	});
	return found;
}

/// The number a record names an object by, none (0) for none
gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

/// A flock made in play: its domain's centre as its place; no info; its player; the three worked-out words; no shepherd
/// (openblack has none), its town and its temple; its members from the head; the most members it has had, the
/// leader's turns since it last moved the herd, its domain's radius, its members' distance, its flying height, no
/// shepherd to follow, its calm; the centre it was made at, twice (the original's second copy is never written after
/// it is made); its followers' state, its state after a leg and its follow mode; no saved target; and its script id (0
/// for an animal's own flock)
std::optional<SavedRecord> WriteFlock(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& flock = registry.Get<const Flock>(entity);
	FlockRecord record;
	record.container.position.position = SavedCoords(flock.domainCentre);
	record.container.info = flock.info.value_or(k_NoInfo);
	record.container.owner.number =
	    flock.player.has_value()
	        ? context.Reference(SourceKey {.type = gamesave::k_PlayerType, .id = static_cast<uint64_t>(*flock.player)})
	        : 0;
	record.computed = k_ComputedWords;
	record.shepherd.number = 0;
	record.town.number = ReferenceOrNone(context, flock.town);
	record.citadelHeart.number = ReferenceOrNone(context, HeartOf(registry, entity));
	for (const auto member : ecs::flocks::MembersFromHead(entity))
	{
		record.members.numbers.push_back(context.Reference(member));
	}
	record.maxMembers = flock.maxMembers;
	record.word = flock.leaderTurns;
	record.domainRadius = flock.domainRadius;
	record.flockDistance = flock.flockDistance;
	record.flyingHeight = flock.altitude;
	record.followShepherd = 0;
	record.secondWord = static_cast<uint32_t>(flock.calm);
	record.savedDomainCentre = SavedCoords(flock.savedDomainCentre);
	record.otherCentre = record.savedDomainCentre;
	record.words = {flock.followState, flock.afterMove, flock.followMode};
	record.savedThing.number = 0;
	record.lastWord = flock.id < 0 ? 0U : static_cast<uint32_t>(flock.id);
	return record;
}

/// The game's list of flocks, newest first. A loaded game's flocks are not made again yet: with them the list is
/// written as its load kept it, and a flock made since stops the write, as it would have to go in among them
std::vector<gamesave::ObjectNumber> FlockList(WriteContext& context)
{
	const auto& registry = context.GetRegistry();
	std::vector<std::pair<uint32_t, entt::entity>> made;
	registry.Each<const Flock>(
	    [&made](entt::entity entity, const Flock& flock) { made.emplace_back(flock.creationStamp, entity); });
	std::vector<gamesave::ObjectNumber> numbers;
	if (const auto* first = context.KeptWorldRecord(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			const auto& kept = game->lists[static_cast<size_t>(GlobalList::Flocks)].numbers;
			if (!kept.empty())
			{
				if (!made.empty())
				{
					context.Fail("a loaded game's flocks are not made again yet, and " + std::to_string(made.size()) +
					             " flock(s) made since would go in among them");
					return {};
				}
				return kept;
			}
		}
	}
	std::ranges::sort(made, std::ranges::greater {}, &std::pair<uint32_t, entt::entity>::first);
	for (const auto& [stamp, entity] : made)
	{
		numbers.push_back(context.Reference(entity));
	}
	return numbers;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterFlockWriters(ClassWriters& writers)
{
	writers.Register(
	    FlockRecord::k_Type, [](const Registry& registry, entt::entity entity) { return registry.AllOf<Flock>(entity); },
	    WriteFlock);
	writers.RegisterList(GlobalList::Flocks, FlockList);
}
