/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' game statistics records of a saved game's world file: each player's statistics object, its own word
// the player's number, then its player and the figures in the game's order, as the statistics give them
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the game statistics).

#include <cstdint>
#include <cstring>

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <SavedRecords.h>

#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::GameStatsRecord;

namespace
{
constexpr uint64_t k_Players = 8;

/// Reads the figures after the player from the statistics' bytes in the game's order
class Figures
{
public:
	explicit Figures(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}
	template <typename T>
	void Read(T& value)
	{
		if (_at + sizeof(T) > _bytes.size())
		{
			_short = true;
			return;
		}
		std::memcpy(&value, _bytes.data() + _at, sizeof(T));
		_at += sizeof(T);
	}
	template <typename T, size_t N>
	void Read(std::array<T, N>& values)
	{
		for (auto& value : values)
		{
			Read(value);
		}
	}
	void Read(gamesave::StatsGraph& graph)
	{
		Read(graph.data);
		Read(graph.size);
		Read(graph.index);
		Read(graph.samplesPerSlot);
		Read(graph.samplesInSlot);
	}
	[[nodiscard]] bool Whole() const { return !_short && _at == _bytes.size(); }

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _short {false};
};
} // namespace

void openblack::ecs::saved_world_write::RegisterGameStatsWriters(ClassWriters& writers, StatsBytes statsOf)
{
	writers.RegisterSource(
	    gamesave::k_GameStatsType,
	    [](const Registry& /*registry*/) {
		    std::vector<uint64_t> players;
		    for (uint64_t player = 0; player < k_Players; ++player)
		    {
			    players.push_back(player);
		    }
		    return players;
	    },
	    [statsOf = std::move(statsOf)](WriteContext& context, uint64_t player) -> std::optional<gamesave::SavedRecord> {
		    // a loaded player's statistics stay as the load kept them
		    if (auto kept = context.KeptRecordOf(SourceKey {.type = gamesave::k_GameStatsType, .id = player}); kept.has_value())
		    {
			    return kept;
		    }
		    const auto bytes = statsOf(static_cast<size_t>(player));
		    GameStatsRecord record;
		    record.thing.extra = static_cast<uint32_t>(player);
		    record.player = PlayerObject(context, static_cast<int>(player));
		    Figures figures(bytes);
		    figures.Read(record.playersThatLeft);
		    figures.Read(record.position);
		    figures.Read(record.lossReason);
		    figures.Read(record.townStats);
		    figures.Read(record.counts);
		    figures.Read(record.influence);
		    figures.Read(record.population);
		    figures.Read(record.results);
		    figures.Read(record.run);
		    figures.Read(record.casts);
		    figures.Read(record.last);
		    if (!figures.Whole())
		    {
			    context.Fail("the statistics of player " + std::to_string(player) + " are not the game's " +
			                 std::to_string(bytes.size()) + " bytes");
			    return std::nullopt;
		    }
		    return record;
	    });
}
