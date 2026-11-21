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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/WallHugRules.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::map_collide
{
struct Shape;
}

/// The circles a thing standing fixed on the land covers on the ground, read from its collide data: the shape the map
/// builds for it when it goes on the map (ecs::map_cells::CollideDataOf). Walkers go round them. Villagers and animals
/// read the same circles, in the same order. After a cell's things come its water and the land's edge, and those of
/// the eight cells round it: a circle in the middle of each such cell.
namespace openblack::ecs::thing_circles
{

/// The circles of a collide shape, in the order the walkers come across them: each circle of its row when it has one,
/// else its one circle. A fence's count as the land's edge does when a walker going round hands over to it
[[nodiscard]] std::vector<wall_hug::BlockingCircle> CirclesOf(const map_collide::Shape& shape, bool fence);

/// The circles a thing on the map covers: none for a field (walked over) or for a thing without collide data
[[nodiscard]] std::vector<wall_hug::BlockingCircle> CirclesOf(const Registry& registry, entt::entity thing);

/// One circle a walker can come across in a map cell, and whose it is
struct CellCircle
{
	wall_hug::BlockingCircle circle;
	/// The thing it belongs to; none for water or the land's edge
	entt::entity owner {entt::null};
	/// Which of the thing's circles it is, from 0
	uint16_t index {0};
	/// For water or the land's edge, the map cell it stands in (each index in 16 bits): the circle of a cell is always
	/// the same one
	glm::ivec2 cell {0};
};

/// Where the circles of a map cell come from
struct CellSources
{
	/// The cell's fixed list from its head, the next taken before fn runs; fn returns false to stop
	std::function<void(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn)> forEachFixed;
	/// A thing's circles, as CirclesOf
	std::function<std::vector<wall_hug::BlockingCircle>(entt::entity thing)> circlesOf;
	/// Whether a map cell (each index in 16 bits) is water or off the land
	std::function<bool(glm::ivec2 cell)> blocked;
};

/// The map's own: the cells' fixed lists, their things' collide data and the island's water (without an island,
/// nothing is water)
[[nodiscard]] CellSources MapSources();

/// The circles a walker comes across in a map cell, in the game's order: those of each thing of the cell's fixed list,
/// in the list's order, a thing's circles one after the other; then, of the cell itself and its eight neighbours (the
/// same, then +x, -x, +z, -z, then -x -z, -x +z, +x -z, +x +z), each that is water or off the land, as a circle of
/// wall_hug::k_LandscapeBlockerRadius in its middle
[[nodiscard]] std::vector<CellCircle> CirclesInCell(glm::ivec2 cell, const CellSources& sources);
/// The same from the map
[[nodiscard]] std::vector<CellCircle> CirclesInCell(glm::ivec2 cell);

} // namespace openblack::ecs::thing_circles
