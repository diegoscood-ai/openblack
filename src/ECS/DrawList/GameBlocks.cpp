/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameBlocks.h"

#include <algorithm>

namespace openblack::ecs::draw_list
{
void BlockArray::Insert(entt::entity object)
{
	_lastRemoved = entt::null;
	if (object == _lastInserted)
	{
		return;
	}
	// Below zero the write lands before the array: no entry is kept
	if (_count >= 0)
	{
		_objects.push_back(object);
	}
	++_count;
	_lastInserted = object;
}

void BlockArray::Remove(entt::entity object)
{
	_lastInserted = entt::null;
	if (object == _lastRemoved)
	{
		return;
	}
	if (_count == 1)
	{
		// The one entry goes, whichever object it is
		_objects.clear();
	}
	else if (_count > 1)
	{
		const auto found = std::find(_objects.begin(), _objects.end(), object);
		if (found != _objects.end())
		{
			*found = _objects.back();
		}
		// Without a match the last entry is the one that goes
		_objects.pop_back();
	}
	--_count;
	_lastRemoved = object;
}

void BlockArray::Clear()
{
	_objects.clear();
	_count = 0;
	_lastInserted = entt::null;
	_lastRemoved = entt::null;
}

std::optional<uint16_t> BlockOf(glm::ivec2 cell, bool hasBlock)
{
	const uint32_t x = static_cast<uint32_t>(cell.x) >> k_CellsPerBlockShift;
	const uint32_t z = static_cast<uint32_t>(cell.y) >> k_CellsPerBlockShift;
	if (x >= k_BlockTableSide || z >= k_BlockTableSide || !hasBlock)
	{
		return std::nullopt;
	}
	return static_cast<uint16_t>(x * k_BlockTableSide + z);
}

BlockArray& GameBlocks::Of(std::optional<uint16_t> slot)
{
	return slot.has_value() && *slot < blocks.size() ? blocks[*slot] : global;
}

const BlockArray& GameBlocks::Of(std::optional<uint16_t> slot) const
{
	return slot.has_value() && *slot < blocks.size() ? blocks[*slot] : global;
}

void GameBlocks::Clear()
{
	for (auto& block : blocks)
	{
		block.Clear();
	}
	global.Clear();
}
} // namespace openblack::ecs::draw_list
