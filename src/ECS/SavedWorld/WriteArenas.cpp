/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The arenas' records of a saved game's world file, and the game's list of them, in the original's order
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the arenas).

#include "WriteArenas.h"

#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ArenaRecord;

namespace
{
/// The number a fight's arena and a land's arena have in their source ids
constexpr uint32_t k_LandArena = 0;
constexpr uint32_t k_FightArena = 1;
/// The first of an arena's three words: the arena's constructor sets it, and nothing changes it but a load
constexpr float k_FirstWord = 1.0f;
/// An arena holds its two fighters: the creature that made it, then its opponent
constexpr size_t k_Fighters = 2;

/// The arenas in the game's list order, the newest first: the creature fights' arenas, made in play after every land's
/// arena, the newest first, then the land's arenas, the last made first
std::vector<uint64_t> ArenaKeys(const ecs::Registry& registry)
{
	using Made = std::pair<uint32_t, uint64_t>;
	std::vector<Made> fights;
	registry.Each<const ecs::components::CreatureFighting>(
	    [&fights](entt::entity entity, const ecs::components::CreatureFighting& fighting) {
		    // a fight that took an arena already there has none of its own on the list, nor one whose arena has not
		    // appeared yet
		    if (fighting.madeArena && !fighting.awaitsArena && fighting.tookArenaOf == entt::null)
		    {
			    fights.emplace_back(fighting.arena.serial, FightArenaId(entity));
		    }
	    });
	std::vector<Made> land;
	registry.Each<const ecs::components::Arena>([&land](entt::entity entity, const ecs::components::Arena& arena) {
		land.emplace_back(arena.number, LandArenaId(entity));
	});
	std::ranges::sort(fights, std::ranges::greater {}, &Made::first);
	std::ranges::sort(land, std::ranges::greater {}, &Made::first);
	std::vector<uint64_t> ids;
	ids.reserve(fights.size() + land.size());
	std::ranges::transform(fights, std::back_inserter(ids), &Made::second);
	std::ranges::transform(land, std::back_inserter(ids), &Made::second);
	return ids;
}

/// A new arena as its constructor leaves it: its first word, no fight on, no fighters and an empty last word, which
/// nothing in play changes
ArenaRecord NewArena()
{
	ArenaRecord record;
	record.words = {std::bit_cast<uint32_t>(k_FirstWord), 0, 0};
	record.objects.numbers.assign(k_Fighters, 0);
	record.other = {};
	return record;
}

/// The fight that holds an arena now: of the fights that took it, and its own maker's, the one that started last, as
/// each fight that takes an arena stores its two creatures in it
const ecs::components::CreatureFighting* LatestFightIn(const ecs::Registry& registry, entt::entity owner, entt::entity* maker)
{
	const ecs::components::CreatureFighting* latest = nullptr;
	registry.Each<const ecs::components::CreatureFighting>([&](entt::entity entity,
	                                                           const ecs::components::CreatureFighting& fighting) {
		const bool holds =
		    fighting.madeArena && (fighting.tookArenaOf == owner || (entity == owner && fighting.tookArenaOf == entt::null));
		// a fight whose arena has not appeared yet holds none
		if (fighting.awaitsArena)
		{
			return;
		}
		if (holds && (latest == nullptr || fighting.startCount > latest->startCount))
		{
			latest = &fighting;
			*maker = entity;
		}
	});
	return latest;
}

/// A land's arena: the record its load kept (a new one for an arena the land script made), with its place as the
/// script or the load gave it, its place's flags and its radius; while a fight that took it is on, that fight on in it
/// and its two creatures, the one that started it first; else whether a fight is on, its fighters and its last word
/// stay as kept
std::optional<gamesave::SavedRecord> WriteLandArena(WriteContext& context, entt::entity entity, uint64_t id)
{
	const auto& registry = context.GetRegistry();
	const auto* arena = registry.Valid(entity) ? registry.TryGet<const ecs::components::Arena>(entity) : nullptr;
	if (arena == nullptr)
	{
		context.Fail(fmt::format("the arena of entity {} is gone", entt::to_integral(entity)));
		return std::nullopt;
	}
	std::optional<ArenaRecord> kept;
	if (auto record = context.KeptRecordOf(SourceKey {.type = ArenaRecord::k_Type, .id = id}); record.has_value())
	{
		kept = std::get<ArenaRecord>(std::move(*record));
	}
	auto record = kept.value_or(NewArena());
	record.position.position = {.x = arena->at.x, .z = arena->at.z, .altitude = arena->at.altitude};
	record.position.flags = PositionFlagsOf(registry, entity, kept.has_value() ? kept->position.flags : uint16_t {0});
	record.words[1] = std::bit_cast<uint32_t>(arena->radius);
	entt::entity maker = entt::null;
	if (const auto* fighting = LatestFightIn(registry, entity, &maker); fighting != nullptr)
	{
		record.words[2] = 1;
		record.objects.numbers = {context.Reference(maker), context.Reference(fighting->opponent)};
	}
	return record;
}

/// A creature fight's arena: made as the fight starts, at the middle of the two creatures in map units with no height
/// above the land, with its radius, a fight on in it and its two fighters, the creature that made it first
std::optional<gamesave::SavedRecord> WriteFightArena(WriteContext& context, entt::entity maker)
{
	const auto& registry = context.GetRegistry();
	const auto* fighting = registry.Valid(maker) ? registry.TryGet<const ecs::components::CreatureFighting>(maker) : nullptr;
	if (fighting == nullptr || !fighting->madeArena)
	{
		context.Fail(fmt::format("the arena of the fight of creature {} is gone", entt::to_integral(maker)));
		return std::nullopt;
	}
	auto record = NewArena();
	record.position.position = {.x = map_coords::ToFixed(fighting->arena.centre.x),
	                            .z = map_coords::ToFixed(fighting->arena.centre.y),
	                            .altitude = 0.0f};
	record.words[1] = std::bit_cast<uint32_t>(fighting->arena.radius);
	record.words[2] = 1;
	// its two creatures, or those of a later fight that took it
	entt::entity holder = maker;
	const auto* latest = LatestFightIn(registry, maker, &holder);
	const auto opponent = latest != nullptr ? latest->opponent : fighting->opponent;
	record.objects.numbers = {context.Reference(holder), context.Reference(opponent)};
	return record;
}

std::optional<gamesave::SavedRecord> WriteArena(WriteContext& context, uint64_t id)
{
	const auto part = PartOf(id);
	switch (part.index)
	{
	case k_LandArena:
		return WriteLandArena(context, part.owner, id);
	case k_FightArena:
		return WriteFightArena(context, part.owner);
	default:
		context.Fail(fmt::format("{} names no arena", id));
		return std::nullopt;
	}
}
} // namespace

std::optional<SourceKey> openblack::ecs::saved_world_write::ArenaKeyOf(const Registry& registry, entt::entity creature)
{
	const auto* fighting =
	    registry.Valid(creature) ? registry.TryGet<const ecs::components::CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr || !fighting->madeArena || fighting->awaitsArena)
	{
		return std::nullopt;
	}
	// the arena it took, a land's or another fight's, or its own
	if (fighting->tookArenaOf != entt::null && registry.Valid(fighting->tookArenaOf))
	{
		if (registry.AllOf<ecs::components::Arena>(fighting->tookArenaOf))
		{
			return LandArenaKeyOf(registry, fighting->tookArenaOf);
		}
		return SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(fighting->tookArenaOf)};
	}
	return SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(creature)};
}

std::optional<SourceKey> openblack::ecs::saved_world_write::LandArenaKeyOf(const Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	// the arena a fight made is written as its maker's fight's arena
	if (const auto* made = registry.TryGet<const ecs::components::CreatureFightArena>(entity))
	{
		return SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(made->maker)};
	}
	if (!registry.AllOf<ecs::components::Arena>(entity))
	{
		return std::nullopt;
	}
	return SourceKey {.type = ArenaRecord::k_Type, .id = LandArenaId(entity)};
}

void openblack::ecs::saved_world_write::RegisterArenaWriters(ClassWriters& writers)
{
	writers.RegisterSource(ArenaRecord::k_Type, ArenaKeys, WriteArena);
	writers.RegisterList(GlobalList::Arenas, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto id : context.Keys(ArenaRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ArenaRecord::k_Type, .id = id}));
		}
		return numbers;
	});
}
