/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' writer: the game's eight players, each from the record its load kept or a new game's record, with the
// values openblack models put where the save writes them. See docs/wiki/engine/saved-games.md#the-players-records.

#include <numeric>
#include <utility>
#include <variant>

#include <CreatureRecords.h>
#include <InterfaceRecords.h>
#include <TownRecords.h>
#include <fmt/format.h>

#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack::ecs;
using namespace openblack::ecs::saved_world_write;

void openblack::ecs::saved_world_write::RegisterPlayerWriters(ClassWriters& writers, PlayerSources sources)
{
	writers.RegisterSource(
	    gamesave::k_PlayerType,
	    [](const Registry& /*registry*/) {
		    // the game always has its eight players, in their numbers' order
		    std::vector<uint64_t> numbers(gamesave::k_PlayerCount);
		    std::iota(numbers.begin(), numbers.end(), uint64_t {0});
		    return numbers;
	    },
	    [sources = std::move(sources)](WriteContext& context, uint64_t id) -> std::optional<gamesave::SavedRecord> {
		    if (id >= saved_players::k_Players)
		    {
			    context.Fail(fmt::format("there is no player {}", id));
			    return std::nullopt;
		    }
		    const auto player = static_cast<PlayerNames>(id);
		    const auto kept = context.KeptRecordOf(SourceKey {.type = gamesave::k_PlayerType, .id = id});
		    const auto* keptPlayer = kept.has_value() ? std::get_if<gamesave::PlayerRecord>(&*kept) : nullptr;
		    auto record = keptPlayer != nullptr ? *keptPlayer
		                  : sources.newRecord   ? sources.newRecord(player)
		                                        : saved_players::NewRecord(player, 0, {});
		    if (sources.values)
		    {
			    if (const auto values = sources.values(player); values.has_value())
			    {
				    record = saved_players::RecordOf(*values, std::move(record));
			    }
		    }
		    if (sources.objects)
		    {
			    const auto objects = sources.objects(context.GetRegistry(), player);
			    record.citadel = context.Reference(objects.citadel, gamesave::CitadelRecord::k_Type);
			    record.towns.clear();
			    for (const auto town : objects.towns)
			    {
				    record.towns.push_back(context.Reference(town, gamesave::TownRecord::k_Type));
			    }
			    // a load does not make the creatures yet: a loaded player's creature stays as its record names it, and
			    // one the player got since a load with none is named
			    if (keptPlayer == nullptr || record.creature == 0)
			    {
				    record.creature = context.Reference(objects.creature, gamesave::CreatureRecord::k_Type);
			    }
		    }
		    // A new game's player has its own computer player, statistics and interface (in the interfaces' place of
		    // its number), each the object of its class of the player's number; a loaded player's stay as its record
		    // names them, as do its teleports
		    if (keptPlayer == nullptr)
		    {
			    record.computerPlayer = context.Reference(SourceKey {.type = gamesave::k_ComputerPlayerType, .id = id});
			    record.gameStats = context.Reference(SourceKey {.type = gamesave::k_GameStatsType, .id = id});
			    record.interfaces.assign(saved_players::k_Interfaces, 0);
			    record.interfaces.at(id) =
			        context.Reference(SourceKey {.type = gamesave::PlayerInterfaceRecord::k_Type, .id = id});
		    }
		    return record;
	    });
}
