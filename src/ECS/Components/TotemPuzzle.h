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

#include "3D/MapCoords.h"
#include "ECS/Components/TreePuzzle.h"

namespace openblack::ecs::components
{

/// The Japanese totem puzzle's six totems and what it keeps of them, on the PuzzleGame of type 8 (ecs/TotemPuzzle.h)
struct TotemPuzzle
{
	/// The puzzle's place on the map, at the altitude the script gives, and its size
	map_coords::MapCoords place {};
	float scale {1.0f};
	/// The height the puzzle keeps for each totem, 0 to 4: a totem raised or lowered to another height moves the others
	std::array<int32_t, 6> heights {};
	/// The six totems in their arc, and the sparkle over each that stands at its highest
	std::array<entt::entity, 6> totems {NoTreePuzzleThings<6>()};
	std::array<entt::entity, 6> sparkles {NoTreePuzzleThings<6>()};
	/// Where each totem stands on the map, and its highest step and height when the puzzle last saw it: what a totem that
	/// has gone is made again with
	std::array<map_coords::MapCoords, 6> places {};
	std::array<int32_t, 6> maxHeights {};
	std::array<int32_t, 6> seenHeights {};
	/// A totem has been moved at least once: the puzzle is in progress
	bool started {false};
};

} // namespace openblack::ecs::components
