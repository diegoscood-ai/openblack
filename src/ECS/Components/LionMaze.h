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
#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/LionMazeRules.h"

namespace openblack::ecs::components
{

/// A walled maze's grid, walls, bells and pieces, on the PuzzleGame of type 12 (the lion maze) or 10 and 11 (the Theseus
/// mazes) (ecs/LionMaze.h)
struct LionMaze
{
	/// The walls' rows and columns: x and y 0 to 7
	static constexpr size_t k_WallLines = 9;
	using WallGrid = std::array<std::array<entt::entity, k_WallLines>, k_WallLines>;
	using PlaceGrid = std::array<std::array<map_coords::MapCoords, k_WallLines>, k_WallLines>;
	[[nodiscard]] static constexpr WallGrid NoWalls()
	{
		WallGrid grid {};
		for (auto& line : grid)
		{
			line = NoTreePuzzleThings<k_WallLines>();
		}
		return grid;
	}

	/// Its squares and map
	lion_maze::Kind kind {lion_maze::k_LionMaze};
	/// The puzzle's place on the map, its game angle and size, and its half square's steps
	map_coords::MapCoords place {};
	uint16_t angle {0};
	float scale {1.0f};
	int32_t cosineStep {0};
	int32_t sineStep {0};
	/// The walls across x and across y, by the square whose near side they stand on, and the place each was put at
	WallGrid wallsAlongX {NoWalls()};
	WallGrid wallsAlongY {NoWalls()};
	PlaceGrid wallsAlongXAt {};
	PlaceGrid wallsAlongYAt {};
	/// The palms at the corners and the lanterns along the sides
	std::array<entt::entity, 4> palms {NoTreePuzzleThings<4>()};
	std::array<entt::entity, 4> lanterns {NoTreePuzzleThings<4>()};
	/// The bells, the four ways first, and the sound owner each rings with once it has rung
	std::array<entt::entity, lion_maze::k_Bells> bells {NoTreePuzzleThings<lion_maze::k_Bells>()};
	std::array<std::optional<uint32_t>, lion_maze::k_Bells> bellSoundOwners {};
	/// Where each bell was put: a bell never moves off it
	std::array<map_coords::MapCoords, lion_maze::k_Bells> bellPlaces {};
	/// The wolf and the sheep, the squares they are on, and where the sheep was put
	entt::entity wolf {entt::null};
	entt::entity sheep {entt::null};
	lion_maze::Square wolfSquare {};
	lion_maze::Square sheepSquare {};
	map_coords::MapCoords sheepPlacedAt {};
	/// The way the wolf is to face, and its walk to its square
	uint16_t wolfAngle {0};
	lion_maze::Walk walk {lion_maze::Walk::Turning};
	/// The Theseus mazes' villager, his square, the way he is to face and his walk, and the marker over the square
	/// last clicked
	entt::entity villager {entt::null};
	lion_maze::Square villagerSquare {};
	uint16_t villagerAngle {0};
	lion_maze::Walk villagerWalk {lion_maze::Walk::Turning};
	entt::entity marker {entt::null};
	/// A bell has been rung: the puzzle is in progress
	bool started {false};
};

} // namespace openblack::ecs::components
