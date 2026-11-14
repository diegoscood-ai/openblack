/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The music attached to things, as the audio's part of the game's tail lists it: each thing's music record
// (docs/wiki/engine/saved-games.md#writing-the-world-file).

#include <cstdint>

#include <algorithm>
#include <bit>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <fmt/format.h>

#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ThingMusicInfoRecord;

void openblack::ecs::saved_world_write::RegisterThingMusicWriters(ClassWriters& writers,
                                                                  std::function<std::vector<audio::ThingMusicInfo>()> infos)
{
	// the list's infos by their keys, head first, as the audio's save walks its list
	writers.RegisterSource(
	    ThingMusicInfoRecord::k_Type,
	    [infos](const Registry& /*unused*/) {
		    std::vector<uint64_t> keys;
		    for (const auto& info : infos())
		    {
			    keys.push_back(info.key);
		    }
		    return keys;
	    },
	    [infos](WriteContext& context, uint64_t id) -> std::optional<gamesave::SavedRecord> {
		    const auto all = infos();
		    const auto found =
		        std::ranges::find(all, id, [](const audio::ThingMusicInfo& info) -> uint64_t { return info.key; });
		    if (found == all.end())
		    {
			    context.Fail(fmt::format("the music attached to things has no info {}", id));
			    return std::nullopt;
		    }
		    // the thing part as the load kept it; a new info's is the class's own
		    ThingMusicInfoRecord record;
		    if (auto kept = context.KeptRecordOf(SourceKey {.type = ThingMusicInfoRecord::k_Type, .id = id}); kept.has_value())
		    {
			    record = std::get<ThingMusicInfoRecord>(std::move(*kept));
		    }
		    // its music type, its thing, whether it is enabled, finished and started, whether it plays at a place of its
		    // own, and that place in map coordinates as the game keeps it
		    record.word = static_cast<uint32_t>(found->type);
		    const auto thing = static_cast<entt::entity>(found->thing);
		    record.object.number = context.GetRegistry().Valid(thing) ? context.Reference(thing) : 0;
		    record.words = {static_cast<uint32_t>(found->enabled), static_cast<uint32_t>(found->finished),
		                    static_cast<uint32_t>(found->started), static_cast<uint32_t>(found->hasPlayPosition)};
		    record.point = {std::bit_cast<float>(found->playCoords.x), std::bit_cast<float>(found->playCoords.z),
		                    found->playCoords.altitude};
		    return record;
	    });
}
