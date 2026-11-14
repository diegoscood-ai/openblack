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

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// A list of things, none of them there
template <size_t Count>
[[nodiscard]] constexpr std::array<entt::entity, Count> NoTreePuzzleThings() noexcept
{
	std::array<entt::entity, Count> things {};
	things.fill(entt::null);
	return things;
}

/// A tree puzzle's grid and its things, on the PuzzleGame of type 1 or 2 (ecs/TreePuzzle.h). The spots are listed the
/// rows across first
struct TreePuzzle
{
	/// The puzzle's place on the map, at the altitude the script gives, and the two steps of its grid in map units
	map_coords::MapCoords place {};
	int32_t cosineStep {0};
	int32_t sineStep {0};
	/// Each spot's count of changes, which gives its state
	std::array<int8_t, 9> counts {};
	/// The tree on each spot, and its place on the map as it was planted: it is off its spot once its place is any
	/// other (openblack keeps a thing's place as its world point, so this is the MapCoords its point gave as it was
	/// planted)
	std::array<entt::entity, 9> trees {NoTreePuzzleThings<9>()};
	std::array<map_coords::MapCoords, 9> placedAt {};
	/// The puff of mist over each spot and the bat for each spot, until the solution
	std::array<entt::entity, 9> mists {NoTreePuzzleThings<9>()};
	std::array<entt::entity, 9> bats {NoTreePuzzleThings<9>()};
	/// The lantern one step out from each corner
	std::array<entt::entity, 4> lanterns {NoTreePuzzleThings<4>()};
	/// A tree went or moved off its spot at least once: the puzzle is in progress
	bool started {false};
};

} // namespace openblack::ecs::components
