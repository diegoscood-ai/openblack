/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The data paths of a saved game's world file: the walk of a living thing along a script's track, which the living's
// record names (docs/wiki/engine/saved-games.md#writing-the-world-file, the data path).

#include "WriteDataPaths.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>
#include <fmt/format.h>

#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::DataPathRecord;

namespace
{
/// The walkers of a track, in the registry's order
std::vector<uint64_t> DataPathKeys(const ecs::Registry& registry)
{
	std::vector<uint64_t> ids;
	registry.Each<const ecs::components::LivingWalkPath>(
	    [&ids](entt::entity living, const ecs::components::LivingWalkPath&) { ids.push_back(DataPathIdOf(living)); });
	return ids;
}

/// A living's walk along a track as the game keeps it: the track's number, the share of the way it stops at, whether it
/// walks it forwards, how far it has got and how far it gets a turn, in the track's milliseconds, and the speed its
/// step was worked out for. The thing's part stays as a load kept it
std::optional<gamesave::SavedRecord> WriteDataPath(WriteContext& context, uint64_t id)
{
	const auto living = PartOf(id).owner;
	const auto& registry = context.GetRegistry();
	const auto* walk = registry.Valid(living) ? registry.TryGet<const ecs::components::LivingWalkPath>(living) : nullptr;
	if (walk == nullptr)
	{
		context.Fail(fmt::format("the walk along a track of entity {} is gone", entt::to_integral(living)));
		return std::nullopt;
	}
	DataPathRecord record;
	if (auto kept = context.KeptRecordOf(SourceKey {.type = DataPathRecord::k_Type, .id = id}); kept.has_value())
	{
		record = std::get<DataPathRecord>(std::move(*kept));
	}
	record.trackNumber = walk->trackNumber;
	record.endPercentage = walk->path.walk.to;
	record.reverse = walk->path.walk.forward ? 1U : 0U;
	record.frame = walk->path.walk.current;
	record.framesPerTurn = walk->path.walk.step;
	record.last = static_cast<float>(walk->path.speed);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterDataPathWriters(ClassWriters& writers)
{
	writers.RegisterSource(DataPathRecord::k_Type, DataPathKeys, WriteDataPath);
}
