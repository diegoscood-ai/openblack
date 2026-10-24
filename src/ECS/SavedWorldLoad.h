/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
}

// The world's objects of a saved game made again as openblack's entities, so far the forests and their trees: first a
// pure plan from the world file's records, in the order the game puts its loaded objects' links back, then the
// entities made from it. See docs/wiki/engine/saved-games.md#what-a-load-does.
namespace openblack::ecs::saved_world_load
{

/// The tree type a tree row's number in the game's tables names, none for a number that is not a tree row. The game
/// numbers its rows from the last one of its tables, so the 23 tree rows run from 2115 (the first) down to 2093
[[nodiscard]] std::optional<TreeInfo> TreeTypeOfInfo(uint32_t info);
/// The big forest type its row's number names, the same way: the 4 big forest rows run from 369 (the first) down
[[nodiscard]] std::optional<BigForestInfo> BigForestTypeOfInfo(uint32_t info);

/// A forest of the save, by its number in the world file
struct PlannedForest
{
	gamesave::ObjectNumber number {0};
	gamesave::ForestRecord record;
};

/// A tree of the save, by its number in the world file, with the type its row names
struct PlannedTree
{
	gamesave::ObjectNumber number {0};
	TreeInfo type {TreeInfo::Beech};
	gamesave::TreeRecord record;
};

/// A big forest of the save, by its number in the world file, with the type its row names
struct PlannedBigForest
{
	gamesave::ObjectNumber number {0};
	BigForestInfo type {BigForestInfo::Info1};
	gamesave::BigForestRecord record;
};

/// An object the load makes, of a class it knows how to make
using PlannedObject = std::variant<PlannedTree, PlannedBigForest>;

/// What a load makes of the world file
struct WorldPlan
{
	/// The forests, from the last of the game's forest list to its first, so that the newest is made last; a forest
	/// the list does not name comes first
	std::vector<PlannedForest> forests;
	/// The trees and big forests that stand on the map, in the order the game puts their links back (the order the
	/// map cells get them in): one held, in flight, or of a row that is not its class's is not made yet
	std::vector<PlannedObject> objects;
	/// The two values of the whole game every forest writes, as the last forest read leaves them: the turn the last
	/// tree was planted by a forest and the id the next forest takes
	std::optional<uint32_t> lastTreeCreatedTurn;
	std::optional<uint32_t> nextForestId;
	/// How many records of each class the save holds
	std::map<uint32_t, uint32_t> census;
};

/// The plan of a world file as far as it was read
[[nodiscard]] WorldPlan PlanOf(const gamesave::WorldRead& world);

/// Of a class of records: how many the save holds and how many the load made
struct CensusLine
{
	uint32_t type {0};
	uint32_t saved {0};
	uint32_t made {0};

	bool operator==(const CensusLine&) const = default;
};
using Census = std::vector<CensusLine>;

/// What makes each class of object: a tree or a big forest of the plan, of the forest of that key (0: none);
/// entt::null when it is not made
struct Makers
{
	std::function<entt::entity(const PlannedTree& tree, uint32_t forestId)> tree;
	std::function<entt::entity(const PlannedBigForest& bigForest, uint32_t forestId)> bigForest;
};

/// Makes the plan's forests in the forest list (each with its saved id, scenic flag and record), puts back the two
/// values of the whole game, then makes the objects in their order through `makers`, each of the forest its record
/// names (by the key the forest list gave it), and last gives each forest the big forest its record names. Of every
/// class of the save, how many were made
Census Apply(const WorldPlan& plan, const Makers& makers);

/// The game's tree: the tree archetype at its saved place, angle, size and maximum, with its saved growth counter and
/// no random draw, neither growing nor marked as standing in a town (the save keeps neither flag, and the game makes a
/// loaded tree with both clear); its saved creation number and life, and its record kept on it
entt::entity MakeTreeEntity(const PlannedTree& tree, uint32_t forestId);
/// The game's big forest: the big forest archetype at its saved place, angle and size, of its saved forest (it makes
/// none) with its saved wood; its saved creation number and life, and its record kept on it
entt::entity MakeBigForestEntity(const PlannedBigForest& bigForest, uint32_t forestId);
/// The game's makers: MakeTreeEntity and MakeBigForestEntity
[[nodiscard]] Makers GameMakers();

} // namespace openblack::ecs::saved_world_load
