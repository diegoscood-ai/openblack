/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rivers' and the banks of mist's records of a saved game's world file, written from their entities in the
// original's order (docs/wiki/engine/saved-games.md#openblack, the rivers and the banks of mist).

#include <cstring>

#include <array>
#include <bit>
#include <optional>
#include <variant>

#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
template <typename Record>
[[nodiscard]] Record KeptOrNew(WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return {};
}

/// A river: its points, each its place (x, the ground's height, z) and then the word the game writes from its memory
/// (the point's link to the next, which a load does not read: kept from the record, 0 for a river made since); its id
/// and its last point's index
std::optional<gamesave::SavedRecord> WriteStream(WriteContext& context, entt::entity entity)
{
	const auto& stream = context.GetRegistry().Get<const ecs::components::Stream>(entity);
	auto record = KeptOrNew<gamesave::StreamRecord>(context, entity);
	const auto keptNodes = record.nodes.values;
	record.nodes.values.clear();
	for (size_t i = 0; i < stream.points.size(); ++i)
	{
		std::array<uint8_t, 16> node {};
		if (i < keptNodes.size())
		{
			node = keptNodes[i];
		}
		const std::array<float, 3> point {stream.points[i].x, stream.points[i].y, stream.points[i].z};
		std::memcpy(node.data(), point.data(), sizeof(point));
		record.nodes.values.push_back(node);
	}
	record.words[0] = static_cast<uint32_t>(stream.id);
	record.words[1] = static_cast<uint32_t>(stream.points.size()) - 1;
	return record;
}

/// A bank of mist: its place (kept from its record while it stands where it was loaded, as a place in metres does not
/// give back the same map coordinates), its size, its colour and its edge-on shrink
std::optional<gamesave::SavedRecord> WriteMist(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& mist = registry.Get<const ecs::components::Mist>(entity);
	const auto kept = context.KeptRecordOf(entity, gamesave::MistRecord::k_Type);
	auto record = KeptOrNew<gamesave::MistRecord>(context, entity);
	if (!kept.has_value())
	{
		// made since the load: its place and its altitude over the land
		const auto& position = registry.Get<const ecs::components::Transform>(entity).position;
		const float ground = Locator::terrainSystem::has_value()
		                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
		                         : 0.0f;
		record.position.position = {
		    .x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z), .altitude = position.y - ground};
	}
	record.words[0] = std::bit_cast<uint32_t>(mist.size);
	record.words[1] = mist.colour;
	// the shrink as the script gave it: openblack keeps 3 when the script's value was 1 (no shrink)
	record.words[2] = std::bit_cast<uint32_t>(mist.edgeShrink ? mist.k : 1.0f);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterRiverAndMistWriters(ClassWriters& writers)
{
	writers.Register(
	    gamesave::StreamRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Stream>(entity); }, WriteStream);
	writers.Register(
	    gamesave::MistRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Mist>(entity); }, WriteMist);
}
