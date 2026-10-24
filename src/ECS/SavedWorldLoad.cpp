/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedWorldLoad.h"

#include <algorithm>
#include <bit>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "Locator.h"

using namespace openblack::ecs;

namespace
{
/// The numbers of the first tree row and the first big forest row; the others of each follow it downwards
constexpr uint32_t k_FirstTreeRow = 2115;
constexpr uint32_t k_FirstBigForestRow = 369;
/// The game object's list of every forest, among its global lists
constexpr size_t k_ForestList = 4;
/// A thing that is out of play (held, or in a script's hands) or flown by the particles
constexpr uint8_t k_ThingUnavailable = 1U << 0U;
constexpr uint8_t k_ThingFlownByParticles = 1U << 4U;
/// A thing whose state may not change (held), and one the physics moves
constexpr uint16_t k_PositionUnavailable = 1U << 2U;
constexpr uint16_t k_PositionInPhysics = 1U << 6U;

/// An object the load puts on the map: one that is not held or in flight
template <typename Record>
bool StandsOnTheMap(const Record& object)
{
	return (object.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
	       (object.base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0;
}

/// The row of a table whose first row has that number: the rows are numbered downwards
template <typename Type>
std::optional<Type> RowOf(uint32_t info, uint32_t firstRow)
{
	const auto rows = static_cast<uint32_t>(Type::_COUNT);
	if (info > firstRow || info + rows <= firstRow)
	{
		return std::nullopt;
	}
	return static_cast<Type>(firstRow - info);
}

/// The made object keeps its saved creation number, its life (a Life only when not full) and its record
template <typename Record>
void KeepSaved(entt::entity entity, const Record& record)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& object = record.base.object;
	registry.AssignOrReplace<openblack::ecs::components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<openblack::ecs::components::Life>(entity, object.life);
	}
	registry.Assign<openblack::ecs::components::LoadedRecord<Record>>(entity, record);
}

openblack::map_coords::MapCoords CoordsOf(const openblack::gamesave::MapCoords& coords)
{
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}
} // namespace

std::optional<openblack::TreeInfo> saved_world_load::TreeTypeOfInfo(uint32_t info)
{
	return RowOf<TreeInfo>(info, k_FirstTreeRow);
}

std::optional<openblack::BigForestInfo> saved_world_load::BigForestTypeOfInfo(uint32_t info)
{
	return RowOf<BigForestInfo>(info, k_FirstBigForestRow);
}

saved_world_load::WorldPlan saved_world_load::PlanOf(const gamesave::WorldRead& world)
{
	WorldPlan plan {.census = world.Census()};
	const auto recordOf = [&world](gamesave::ObjectNumber number) -> const gamesave::SavedRecord* {
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			return nullptr;
		}
		return &*world.objects[number - 1];
	};

	// The forests: the game's list holds them newest first, and a load keeps its order
	std::vector<gamesave::ObjectNumber> listed;
	if (const auto* first = recordOf(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			listed = game->lists[k_ForestList].numbers;
		}
	}
	const std::unordered_set<gamesave::ObjectNumber> inList(listed.begin(), listed.end());
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr;
		if (forest == nullptr)
		{
			continue;
		}
		// every forest writes the two values of the whole game, and each one read sets them
		plan.lastTreeCreatedTurn = forest->firstGameWord;
		plan.nextForestId = forest->secondGameWord;
		if (!inList.contains(number))
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}
	for (const auto number : listed | std::views::reverse)
	{
		const auto* record = recordOf(number);
		if (const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr; forest != nullptr)
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}

	// The trees and big forests, in the order their links are put back: the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* tree = std::get_if<gamesave::TreeRecord>(record); tree != nullptr && StandsOnTheMap(*tree))
		{
			if (const auto type = TreeTypeOfInfo(tree->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedTree {.number = number, .type = *type, .record = *tree});
			}
		}
		else if (const auto* big = std::get_if<gamesave::BigForestRecord>(record); big != nullptr && StandsOnTheMap(*big))
		{
			if (const auto type = BigForestTypeOfInfo(big->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedBigForest {.number = number, .type = *type, .record = *big});
			}
		}
	}
	return plan;
}

saved_world_load::Census saved_world_load::Apply(const WorldPlan& plan, const Makers& makers)
{
	auto& forests = Locator::forestSystem::value();
	std::unordered_map<gamesave::ObjectNumber, uint32_t> forestKeys; // each saved forest's key
	for (const auto& [number, record] : plan.forests)
	{
		// the forest keeps its saved id, which another forest may share; the counter is put back below
		const auto key = forests.Create(record.id, map_coords::ToWorld(CoordsOf(record.container.position.position)));
		auto& forest = forests.All().at(key);
		forest.id = record.id;
		forest.scenic = record.isScenic != 0;
		forest.saved = record;
		forestKeys.emplace(number, key);
	}
	if (plan.lastTreeCreatedTurn.has_value())
	{
		forests.SetLastTreeCreatedTurn(*plan.lastTreeCreatedTurn);
	}
	if (plan.nextForestId.has_value())
	{
		forests.SetNextId(*plan.nextForestId);
	}

	const auto keyOf = [&forestKeys](gamesave::ObjectNumber number) {
		const auto found = forestKeys.find(number);
		return found != forestKeys.end() ? found->second : 0U;
	};
	uint32_t treesMade = 0;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> bigForests;
	for (const auto& object : plan.objects)
	{
		if (const auto* tree = std::get_if<PlannedTree>(&object); tree != nullptr)
		{
			if (makers.tree(*tree, keyOf(tree->record.forest.number)) != entt::null)
			{
				++treesMade;
			}
		}
		else if (const auto* big = std::get_if<PlannedBigForest>(&object); big != nullptr)
		{
			if (const auto made = makers.bigForest(*big, keyOf(big->record.forest.number)); made != entt::null)
			{
				bigForests.emplace(big->number, made);
			}
		}
	}
	// each forest points at the big forest its own record names, once every big forest is made
	for (const auto& [number, record] : plan.forests)
	{
		if (const auto found = bigForests.find(record.bigForest.number); found != bigForests.end())
		{
			forests.All().at(keyOf(number)).bigForest = found->second;
		}
	}

	Census census;
	for (const auto& [type, saved] : plan.census)
	{
		uint32_t made = 0;
		if (type == gamesave::TreeRecord::k_Type)
		{
			made = treesMade;
		}
		else if (type == gamesave::ForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(forestKeys.size());
		}
		else if (type == gamesave::BigForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(bigForests.size());
		}
		census.push_back({.type = type, .saved = saved, .made = made});
	}
	return census;
}

entt::entity saved_world_load::MakeTreeEntity(const PlannedTree& tree, uint32_t forestId)
{
	const auto& record = tree.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::TreeArchetype::Create(
	    forestId, position, tree.type, false, object.heading, record.secondValue, object.scale,
	    {.growing = false, .counter = static_cast<uint16_t>(record.firstValue)});
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeBigForestEntity(const PlannedBigForest& bigForest, uint32_t forestId)
{
	const auto& record = bigForest.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	// the word it keeps after its forest is its wood, a float
	const auto entity = archetypes::BigForestArchetype::Create(position, bigForest.type, object.heading, object.scale, forestId,
	                                                           std::bit_cast<float>(record.word));
	KeepSaved(entity, record);
	return entity;
}

saved_world_load::Makers saved_world_load::GameMakers()
{
	return {.tree = MakeTreeEntity, .bigForest = MakeBigForestEntity};
}
