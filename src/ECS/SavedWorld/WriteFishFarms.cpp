/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fish farms' records of a saved game's world file: a building of several cells, its fishermen, its town, a float
// its constructor sets to 1 and nothing changes, and its stock of fish
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the fish farm).

#include <cstdint>

#include <optional>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <VillageRecords.h>

#include "ECS/Components/FishFarm.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;
using gamesave::FishFarmRecord;

namespace
{
/// The number of the fish farm info's row in the game's tables (info.dat has the one); a farm's info counts downwards
constexpr uint32_t k_FirstFishFarmRow = 1759;
/// A building made whole: the fixed object's flag the constructor leaves
constexpr uint8_t k_Constructed = 0x08;
/// The float the fish farm's constructor sets and nothing else writes
constexpr float k_ConstructorsFloat = 1.0f;

std::optional<gamesave::SavedRecord> WriteFishFarm(WriteContext& context, entt::entity entity)
{
	const auto& farm = context.GetRegistry().Get<const components::FishFarm>(entity);
	std::optional<FishFarmRecord> kept;
	if (auto record = context.KeptRecordOf(entity, FishFarmRecord::k_Type); record.has_value())
	{
		kept = std::get<FishFarmRecord>(std::move(*record));
	}
	FishFarmRecord record = kept.value_or(FishFarmRecord {});
	record.base =
	    MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr, k_FirstFishFarmRow - farm.info);
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
		record.first = k_ConstructorsFloat;
	}
	// a loaded farm's fishermen as its record names them, as the load makes no fisherman's place in it, without those
	// gone since (a fisherman leaves its farm as its fishing ends, which its deletion ends)
	if (!kept.has_value())
	{
		record.villagers.numbers.clear();
		for (const auto fisherman : farm.fishermen)
		{
			record.villagers.numbers.push_back(context.Reference(fisherman));
		}
	}
	else
	{
		context.DropGone(record.villagers);
	}
	record.town.number = farm.town != entt::null ? context.Reference(farm.town) : 0;
	record.second = farm.food;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterFishFarmWriters(ClassWriters& writers)
{
	writers.Register(
	    FishFarmRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::FishFarm>(entity); },
	    WriteFishFarm);
}
