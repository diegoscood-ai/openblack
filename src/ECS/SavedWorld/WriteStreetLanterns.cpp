/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The street lanterns' records of a saved game's world file: an object of a mobile static row, whether it is a country
// lantern and whether it is dark enough for the lanterns to be heard
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the street lantern).

#include <cstdint>

#include <optional>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "ECS/Components/StreetLantern.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

void openblack::ecs::saved_world_write::RegisterStreetLanternWriters(ClassWriters& writers, std::function<bool()> soundsOn)
{
	writers.Register(
	    gamesave::StreetLanternRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::StreetLantern>(entity); },
	    [soundsOn = std::move(soundsOn)](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    const auto& lantern = context.GetRegistry().Get<const components::StreetLantern>(entity);
		    std::optional<gamesave::StreetLanternRecord> kept;
		    if (auto record = context.KeptRecordOf(entity, gamesave::StreetLanternRecord::k_Type); record.has_value())
		    {
			    kept = std::get<gamesave::StreetLanternRecord>(std::move(*record));
		    }
		    gamesave::StreetLanternRecord record = kept.value_or(gamesave::StreetLanternRecord {});
		    record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
		    // an object's target is where it was made
		    record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr, record.position.position,
		                                 saved_world_load::MobileStaticInfoRowOf(lantern.info));
		    record.word = lantern.country ? 1 : 0;
		    record.mark = soundsOn() ? 1 : 0;
		    return record;
	    });
}
