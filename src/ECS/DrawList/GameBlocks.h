/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

// The objects of each land block, in the order the map filed them, which the draw list walks block by block. The map
// files an object here at every cell it enters and leaves (ecs::map_cells), so an object in several cells of one block
// is kept once only when its inserts follow one another. Pure: no Locator. docs/bw1-notes/original-frame.md, "Stage B".

namespace openblack::ecs::draw_list
{
/// Land blocks per side of the map's block table
constexpr uint32_t k_BlockTableSide = 32;
/// The cell index shifted right by this gives the block index (16 cells per block side)
constexpr uint32_t k_CellsPerBlockShift = 4;
/// Slots in the block table, (block x << 5) + block z
constexpr size_t k_BlockSlots = static_cast<size_t>(k_BlockTableSide) * k_BlockTableSide;

/// One block's objects, in the order they were filed
class BlockArray
{
public:
	/// Appends the object, unless it is the object inserted last (with nothing removed since). Any insert forgets the
	/// last removed object
	void Insert(entt::entity object);
	/// Takes the object out: the last entry moves into its place. Any remove forgets the last inserted object, and
	/// nothing more happens when the object is the one removed last. With one entry, that entry goes whichever object
	/// it is. The count always drops by one: from an array of two or more without the object the last entry goes, and
	/// from an empty array the count goes below zero (an insert then only brings it back up)
	void Remove(entt::entity object);
	/// No objects, and no last inserted or removed object
	void Clear();

	/// The objects, as many as the count when it is above zero
	[[nodiscard]] std::span<const entt::entity> Objects() const { return _objects; }
	/// The count, which a remove from an empty array takes below zero
	[[nodiscard]] int32_t Count() const { return _count; }

private:
	/// The first entries of the array, max(count, 0) of them
	std::vector<entt::entity> _objects;
	int32_t _count {0};
	entt::entity _lastInserted {entt::null};
	entt::entity _lastRemoved {entt::null};
};

/// The block-table slot of a cell: (x >> 4) * 32 + (z >> 4), the cell's coordinates read as unsigned. None (the objects
/// outside every block) when either block index is past 31 or the land has no block there (`hasBlock`)
[[nodiscard]] std::optional<uint16_t> BlockOf(glm::ivec2 cell, bool hasBlock);

/// The block table and the array of the objects outside every block
struct GameBlocks
{
	std::array<BlockArray, k_BlockSlots> blocks;
	BlockArray global;

	/// A slot's array (BlockOf), or the outside one for none
	[[nodiscard]] BlockArray& Of(std::optional<uint16_t> slot);
	[[nodiscard]] const BlockArray& Of(std::optional<uint16_t> slot) const;
	/// Every array cleared (a land is cleared)
	void Clear();
};
} // namespace openblack::ecs::draw_list
