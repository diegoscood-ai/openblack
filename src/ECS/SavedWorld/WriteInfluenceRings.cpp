/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence rings in a saved game's world file, written from their entities, and the game's list of them
// (docs/wiki/engine/saved-games.md#openblack, the influence rings).

#include <cstdint>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::InfluenceRingRecord;

namespace
{
/// A ring: its place as map coordinates (the height above the ground at it), the object it follows while that object
/// exists, its player, its radius and its anti word
std::optional<gamesave::SavedRecord> WriteInfluenceRing(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& ring = registry.Get<const ecs::components::InfluenceRing>(entity);
	std::optional<InfluenceRingRecord> kept;
	if (auto record = context.KeptRecordOf(entity, InfluenceRingRecord::k_Type); record.has_value())
	{
		kept = std::get<InfluenceRingRecord>(std::move(*record));
	}
	InfluenceRingRecord record = kept.value_or(InfluenceRingRecord {});
	const auto coords = map_coords::FromWorld(ring.position);
	record.position.position = {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
	record.attached = {ring.attached != entt::null && registry.Valid(ring.attached) ? context.Reference(ring.attached) : 0};
	record.player = {PlayerObject(context, static_cast<int>(ring.player))};
	record.radius = ring.radius;
	record.anti = static_cast<uint32_t>(ring.anti);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterInfluenceRingWriters(ClassWriters& writers)
{
	writers.Register(
	    InfluenceRingRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<ecs::components::InfluenceRing>(entity); },
	    WriteInfluenceRing);
	// the game's rings: the live ones, the newest first, then those the load kept, as the loaded file listed them
	writers.RegisterList(GlobalList::InfluenceRings, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto ring : influence::Rings())
		{
			numbers.push_back(context.Reference(ring));
		}
		if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
		{
			const auto& kept = game->lists.at(static_cast<size_t>(GlobalList::InfluenceRings)).numbers;
			numbers.insert(numbers.end(), kept.begin(), kept.end());
		}
		return numbers;
	});
}
