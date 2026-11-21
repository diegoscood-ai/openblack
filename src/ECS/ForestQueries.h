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

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/GameDistance.h"

/// The forests' read-only lookups as the original answers them: the closest forest to a point and the order of a
/// forest's trees. Pure functions over what the caller gathers, so they are tested with fake forests.
namespace openblack::ecs::forest_queries
{
/// A forest's two tree lists' sizes: the trees that have stopped growing and the ones still growing
struct TreeCounts
{
	uint32_t grown {0};
	uint32_t growing {0};

	/// Every tree of the forest
	[[nodiscard]] constexpr uint32_t Total() const { return grown + growing; }
};

/// A forest as the closest-forest lookup reads it
struct ForestCandidate
{
	uint32_t key;
	/// Its creation order: the game's forest list is newest first
	uint32_t created;
	glm::vec3 centre;
	uint32_t trees;
};

/// The forest with trees whose centre is nearest `pos` across the land (x and z, the game's whole-number distance in
/// metres), strictly nearer than `maxDistance`. The forests are tried newest first and only a strictly nearer one
/// replaces the one found, so of two as near the newer one is kept. Forests with no trees are skipped.
[[nodiscard]] inline std::optional<uint32_t> ClosestForestWithTrees(std::span<const ForestCandidate> forests, glm::vec3 pos,
                                                                    float maxDistance)
{
	std::vector<const ForestCandidate*> newestFirst;
	newestFirst.reserve(forests.size());
	for (const auto& forest : forests)
	{
		newestFirst.push_back(&forest);
	}
	std::ranges::stable_sort(newestFirst, [](const auto* lhs, const auto* rhs) { return lhs->created > rhs->created; });
	std::optional<uint32_t> found;
	for (const auto* forest : newestFirst)
	{
		const float distance = game_math::GetDistanceInMetres(pos, forest->centre);
		if (distance < maxDistance && forest->trees != 0)
		{
			found = forest->key;
			maxDistance = distance;
		}
	}
	return found;
}

/// A tree of a forest as the tree order reads it
struct ForestTree
{
	entt::entity entity;
	/// In the growing list (still growing below its full size), else in the grown list
	bool growing;
	/// When it was made: of two trees as near the centre, the one made first comes first
	uint32_t made;
	/// Its distance to the forest's centre (the game's whole-number distance in metres)
	float distance;
};

/// A forest's trees in the order the original indexes them: the growing list first, then the grown list, each
/// nearest the centre first and, at the same distance, in the order they were made
[[nodiscard]] inline std::vector<entt::entity> OrderedTrees(std::vector<ForestTree> trees)
{
	std::ranges::sort(trees, {}, &ForestTree::made);
	std::ranges::stable_sort(trees, {}, &ForestTree::distance);
	std::ranges::stable_partition(trees, &ForestTree::growing);
	std::vector<entt::entity> ordered;
	ordered.reserve(trees.size());
	for (const auto& tree : trees)
	{
		ordered.push_back(tree.entity);
	}
	return ordered;
}

/// How many trees are in each of the two lists
[[nodiscard]] inline TreeCounts CountTrees(std::span<const ForestTree> trees)
{
	TreeCounts counts;
	for (const auto& tree : trees)
	{
		++(tree.growing ? counts.growing : counts.grown);
	}
	return counts;
}
} // namespace openblack::ecs::forest_queries
