/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forests', the trees' and the big forests' records of a saved game's world file, in the original's order
// (docs/wiki/engine/saved-games.md#openblack, the forests and trees).

#include <cstdint>

#include <algorithm>
#include <bit>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <SavedRecords.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::BigForestRecord;
using gamesave::ForestRecord;
using gamesave::TreeRecord;

namespace
{
template <typename Record>
[[nodiscard]] std::optional<Record> KeptAs(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return std::nullopt;
}

[[nodiscard]] gamesave::ObjectNumber ForestReference(WriteContext& context, uint32_t forestKey)
{
	return forestKey != 0 ? context.Reference(SourceKey {.type = ForestRecord::k_Type, .id = forestKey}) : 0;
}

/// A tree: its fixed object's parts, its turns to its next growth step, the size it grows to, its forest and its flags
/// word (the low half the game writes: kept as loaded; for a tree made since its wind slot in bits 2 to 5, the bend slot
/// of bits 6 to 9 being 0 for a tree drawn since it was last marked)
std::optional<gamesave::SavedRecord> WriteTree(WriteContext& context, entt::entity entity)
{
	const auto& tree = context.GetRegistry().Get<const ecs::components::Tree>(entity);
	const auto kept = KeptAs<TreeRecord>(context, entity);
	TreeRecord record = kept.value_or(TreeRecord {});
	record.base = FixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                            ecs::saved_world_load::TreeInfoRowOf(tree.type));
	record.firstValue = static_cast<int16_t>(tree.growCounter);
	record.secondValue = tree.maxSize;
	record.forest.number = ForestReference(context, tree.forestId);
	if (!kept.has_value())
	{
		record.thirdValue = static_cast<uint16_t>(tree.windSlot << 2);
	}
	return record;
}

/// A big forest: its object of several cells' parts, its forest and the wood it holds
std::optional<gamesave::SavedRecord> WriteBigForest(WriteContext& context, entt::entity entity)
{
	const auto& bigForest = context.GetRegistry().Get<const ecs::components::BigForest>(entity);
	const auto kept = KeptAs<BigForestRecord>(context, entity);
	BigForestRecord record = kept.value_or(BigForestRecord {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    ecs::saved_world_load::BigForestInfoRowOf(static_cast<BigForestInfo>(bigForest.type)));
	record.forest.number = ForestReference(context, bigForest.forestId);
	record.word = std::bit_cast<uint32_t>(bigForest.wood);
	return record;
}

/// The forests' keys in the game's list order: the newest first
std::vector<uint64_t> ForestKeys(const ecs::Registry& /*registry*/)
{
	std::vector<std::pair<uint32_t, uint32_t>> made; // creation order, key
	for (const auto& [key, forest] : Locator::forestSystem::value().All())
	{
		made.emplace_back(forest.created, key);
	}
	std::ranges::sort(made, std::ranges::greater {});
	std::vector<uint64_t> keys;
	keys.reserve(made.size());
	for (const auto& [created, key] : made)
	{
		keys.push_back(key);
	}
	return keys;
}

/// A tree's place in map coordinates, as its record writes it
gamesave::MapCoords TreeCoords(const WriteContext& context, entt::entity entity)
{
	const auto kept = KeptAs<TreeRecord>(context, entity);
	return PositionPartOf(context, entity, kept.has_value() ? &kept->base.position : nullptr).position;
}

/// A forest: its container's parts (its place, its row and its owner, as loaded), whether it is a town's scenic forest,
/// its footpath link (as loaded), its id, its trees in two lists, the grown ones and the growing ones, each nearest
/// its centre first, a tree after those as near as it, its big forest, and the whole game's turn of the last tree a
/// forest planted and the id the next forest takes. A tree stays in the list it was in, but a growing one of a forest
/// that is not scenic goes to the grown list once it has grown, as the forests' turn moves it; a tree new since goes
/// in the list its growth says, after the ones already there
std::optional<gamesave::SavedRecord> WriteForest(WriteContext& context, uint64_t id)
{
	auto& forests = Locator::forestSystem::value();
	const auto key = static_cast<uint32_t>(id);
	const auto found = forests.All().find(key);
	if (found == forests.All().end())
	{
		context.Fail("forest " + std::to_string(key) + " is gone");
		return std::nullopt;
	}
	const auto& forest = found->second;
	if (!forest.saved.has_value())
	{
		// a forest made in play: the row and the owner its container writes are not read yet
		context.Fail("forest " + std::to_string(key) + " was made in play: its container's row is not read yet");
		return std::nullopt;
	}
	ForestRecord record = *forest.saved;
	record.isScenic = forest.scenic ? 1 : 0;
	record.id = forest.id;
	record.bigForest.number = context.Reference(forest.bigForest);
	record.firstGameWord = forests.LastTreeCreatedTurn();
	record.secondGameWord = forests.NextId();

	// the trees of the forest, each with the list it goes in and its order of insertion there
	const auto& registry = context.GetRegistry();
	const auto& keptGrown = forest.saved->trees.numbers;
	const auto& keptGrowing = forest.saved->otherTrees.numbers;
	struct Member
	{
		entt::entity entity;
		bool growing;
		uint64_t rank;
		float distance;
	};
	std::vector<Member> members;
	const auto& centre = forest.saved->container.position.position;
	const auto centreCoords = map_coords::MapCoords {.x = centre.x, .z = centre.z, .altitude = centre.altitude};
	registry.Each<const ecs::components::Tree, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Tree& tree, const ecs::components::Transform& transform) {
		    if (tree.forestId != key)
		    {
			    return;
		    }
		    // a loaded tree's number is its loaded one (asking it numbers nothing new); a tree made since is in no
		    // kept list, and is numbered only as the lists are written
		    const bool loaded = registry.AllOf<ecs::components::LoadedRecord<TreeRecord>>(entity);
		    const auto number = loaded ? context.Reference(entity) : gamesave::ObjectNumber {0};
		    const auto inGrown = loaded ? std::ranges::find(keptGrown, number) : keptGrown.end();
		    const auto inGrowing = loaded ? std::ranges::find(keptGrowing, number) : keptGrowing.end();
		    const bool stillGrowing = tree.growing && transform.scale.x < tree.maxSize;
		    const auto* index = registry.TryGet<const ecs::components::ObjectCreationIndex>(entity);
		    Member member {.entity = entity};
		    if (inGrown != keptGrown.end())
		    {
			    member.growing = false;
			    member.rank = static_cast<uint64_t>(inGrown - keptGrown.begin());
		    }
		    else if (inGrowing != keptGrowing.end() && (forest.scenic || stillGrowing))
		    {
			    member.growing = true;
			    member.rank = static_cast<uint64_t>(inGrowing - keptGrowing.begin());
		    }
		    else
		    {
			    member.growing = stillGrowing;
			    member.rank = (uint64_t {1} << 32) + (index != nullptr ? index->value : 0);
		    }
		    const auto at = TreeCoords(context, entity);
		    member.distance = gutils::GetDistanceInMetres(map_coords::MapCoords {.x = at.x, .z = at.z, .altitude = at.altitude},
		                                                  centreCoords);
		    members.push_back(member);
	    });
	std::ranges::sort(members, {}, &Member::rank);
	std::ranges::stable_sort(members, {}, &Member::distance);
	record.trees.numbers.clear();
	record.otherTrees.numbers.clear();
	for (const auto& member : members)
	{
		(member.growing ? record.otherTrees : record.trees).numbers.push_back(context.Reference(member.entity));
	}
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterForestWriters(ClassWriters& writers)
{
	writers.Register(
	    TreeRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Tree>(entity); }, WriteTree);
	writers.Register(
	    BigForestRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::BigForest>(entity); },
	    WriteBigForest);
	writers.RegisterSource(ForestRecord::k_Type, ForestKeys, WriteForest);
	writers.RegisterList(GlobalList::Forests, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto id : context.Keys(ForestRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ForestRecord::k_Type, .id = id}));
		}
		return numbers;
	});
}
