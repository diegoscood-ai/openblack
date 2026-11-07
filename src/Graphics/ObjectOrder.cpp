/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectOrder.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace openblack::graphics::object_order
{

PhysicsSplit SplitPhysics(std::span<const Row> rows, std::span<const uint8_t> drawn, size_t rowCount,
                          std::span<const entt::entity> physics, std::span<const entt::entity> sunk)
{
	enum class Part : uint8_t
	{
		List,
		Physics,
		Sunk,
	};
	std::vector<entt::entity> flying(physics.begin(), physics.end());
	std::ranges::sort(flying);
	std::vector<entt::entity> under(sunk.begin(), sunk.end());
	std::ranges::sort(under);
	std::vector<Part> parts(rowCount, Part::List);
	for (const auto& row : rows)
	{
		if (row.index >= rowCount)
		{
			continue;
		}
		if (std::ranges::binary_search(flying, row.object))
		{
			parts[row.index] = Part::Physics;
		}
		// a sunk one in neither draw
		if (std::ranges::binary_search(under, row.object))
		{
			parts[row.index] = Part::Sunk;
		}
	}
	PhysicsSplit split {.list = std::vector<uint8_t>(rowCount, 0),
	                    .physics = std::vector<uint8_t>(rowCount, 0),
	                    .queued = std::vector<uint8_t>(rowCount, 0)};
	for (size_t row = 0; row < rowCount; ++row)
	{
		const bool isDrawn = row >= drawn.size() || drawn[row] != 0;
		split.list[row] = isDrawn && parts[row] == Part::List ? 1 : 0;
		split.physics[row] = isDrawn && parts[row] == Part::Physics ? 1 : 0;
		split.queued[row] = isDrawn && parts[row] != Part::Sunk ? 1 : 0;
	}
	return split;
}

std::vector<Key> RowKeys(const FrameOrder& order, std::span<const Row> rows, size_t rowCount)
{
	std::unordered_map<entt::entity, std::pair<Band, uint32_t>> places;
	places.reserve(order.visited.size() + order.tail.size() + order.physics.size());
	for (const auto& [band, objects] : {std::pair {Band::List, order.visited}, std::pair {Band::Tail, order.tail},
	                                    std::pair {Band::AfterLeashes, order.physics}})
	{
		for (uint32_t i = 0; i < objects.size(); ++i)
		{
			// an object drawn twice keeps its first place
			places.try_emplace(objects[i], band, i);
		}
	}
	std::vector<Key> keys(rowCount, Key {.band = Band::NotPlaced, .place = 0, .step = k_OwnStep});
	for (const auto& row : rows)
	{
		if (row.index >= rowCount)
		{
			continue;
		}
		const auto found = places.find(row.object);
		keys[row.index] = found != places.end()
		                      ? Key {.band = found->second.first, .place = found->second.second, .step = row.step}
		                      : Key {.band = Band::NotPlaced, .place = 0, .step = row.step};
	}
	return keys;
}

std::vector<Key> RowKeys(std::span<const entt::entity> visited, std::span<const Row> rows, size_t rowCount)
{
	return RowKeys(FrameOrder {.visited = visited}, rows, rowCount);
}

std::vector<entt::entity> TailOrder(std::span<const entt::entity> roots, std::span<const entt::entity> walkers,
                                    std::span<const entt::entity> souls, std::span<const entt::entity> fallingChests,
                                    std::span<const entt::entity> marks)
{
	std::vector<entt::entity> tail;
	tail.reserve(roots.size() + walkers.size() + souls.size() + fallingChests.size() + marks.size());
	for (const auto part : {roots, walkers, souls, fallingChests, marks})
	{
		tail.insert(tail.end(), part.begin(), part.end());
	}
	return tail;
}

size_t SegmentOf(Key key, std::span<const Key> blendedKeys)
{
	// the blended ones strictly before it: one at the same key is drawn after it
	return static_cast<size_t>(std::ranges::lower_bound(blendedKeys, key) - blendedKeys.begin());
}

} // namespace openblack::graphics::object_order
