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

#include <tuple>

#include "SaveFields.h"

// The records of the forests and their trees, which a town's forests list pulls in: each forest, the big forest it
// belongs to and every tree of its two lists. See docs/wiki/engine/saved-games.md#the-forests-and-the-trees.
namespace openblack::gamesave
{

/// A forest: the trees it keeps sorted by their distance from it, in two lists
struct ForestRecord
{
	static constexpr uint32_t k_Type = 0x4E;
	ThingPart thing;
	ContainerFields container;
	uint32_t isScenic {0};
	Ref footpathLink;
	uint32_t id {0};
	RefList trees;
	RefList otherTrees;
	Ref bigForest;
	/// Two words that every forest writes from the whole game
	uint32_t firstGameWord {0};
	uint32_t secondGameWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.container, record.isScenic, record.footpathLink, record.id, record.trees, record.otherTrees,
		                record.bigForest, record.firstGameWord, record.secondGameWord);
	}
	bool operator==(const ForestRecord&) const = default;
};

/// A big forest: the building of several cells that stands for a forest
struct BigForestRecord
{
	static constexpr uint32_t k_Type = 0x4D;
	ThingPart thing;
	MultiMapFixedFields base;
	Ref forest;
	uint32_t word {0};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.forest, record.word); }
	bool operator==(const BigForestRecord&) const = default;
};

/// A tree: a fixed object with three values and its forest
struct TreeRecord
{
	static constexpr uint32_t k_Type = 0x4F;
	ThingPart thing;
	FixedFields base;
	int16_t firstValue {0};
	float secondValue {0.0f};
	Ref forest;
	/// The low half of a word the tree keeps: the game writes two of its four bytes
	uint16_t thirdValue {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.firstValue, record.secondValue, record.forest, record.thirdValue);
	}
	bool operator==(const TreeRecord&) const = default;
};

} // namespace openblack::gamesave
