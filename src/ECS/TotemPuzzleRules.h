/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <span>

#include "3D/MapCoords.h"
#include "Common/GameAngle.h"

/// The rules of the Japanese totem puzzle (PuzzleGame type 8): six totems of five heights each, 0 to 4, in an arc.
/// Raising or lowering one totem moves the others by the puzzle's table, every height counted round in fives; the
/// puzzle is solved when all six stand at their highest. Pure. Wiki: docs/wiki/world/totem-puzzle.md
namespace openblack::ecs::totem_puzzle
{

inline constexpr size_t k_Totems = 6;
/// The heights a totem takes, 0 to its highest, 4
inline constexpr int32_t k_Heights = 5;
inline constexpr int32_t k_Highest = 4;
/// The step of the arc between two totems, in the game's angle units of the doubled turn, and its end
inline constexpr int32_t k_ArcStep = 0x400;
inline constexpr int32_t k_ArcEnd = 0x1800;
/// The sparkle over a totem at its highest: the spot visual's magnitude, and the turns of the one the solution makes
inline constexpr float k_SparkleMagnitude = 5.0f;
inline constexpr int32_t k_SolvedSparkleTurns = 3;

/// The table of type 8: row i is what a move of totem i by one height moves each totem by
inline constexpr std::array<std::array<int32_t, k_Totems>, k_Totems> k_Moves {{
    {0, 4, 0, 0, 0, 2},
    {0, 0, 0, 0, 0, 0},
    {1, 1, 0, 2, 0, 0},
    {0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 1},
    {0, 3, 0, 1, 0, 0},
}};

/// The index into the game's sine table of the totem at an arc step: the step over 6, as the game's unsigned
/// multiplication works it out
[[nodiscard]] constexpr uint32_t ArcIndex(uint32_t arcStep) noexcept
{
	return static_cast<uint32_t>((static_cast<uint64_t>(arcStep) * 0xAAAAAAABull) >> 32) >> 2;
}

/// The SIN table's entries before its cosines
inline constexpr uint32_t k_CosineOffset = 512;

/// The place of a totem in the arc, from the puzzle's place and size: the arc step's sine and cosine, from the game's
/// table, times the size and doubled, each truncated and added to the place's x and z, at the place's altitude
[[nodiscard]] inline map_coords::MapCoords ArcPlace(const map_coords::MapCoords& place, float scale, size_t totem)
{
	const auto& table = game_math::SinTable();
	const auto index = ArcIndex(static_cast<uint32_t>(totem) * static_cast<uint32_t>(k_ArcStep));
	const auto across = map_coords::FtoL(static_cast<float>(table.at(index)) * scale * 2.0f);
	const auto along = map_coords::FtoL(static_cast<float>(table.at(index + k_CosineOffset)) * scale * 2.0f);
	return {.x = static_cast<int32_t>(static_cast<uint32_t>(place.x) + static_cast<uint32_t>(across)),
	        .z = static_cast<int32_t>(static_cast<uint32_t>(place.z) + static_cast<uint32_t>(along)),
	        .altitude = place.altitude};
}

/// The angle about y a totem of the arc is turned to: the angle from the puzzle's place to the totem's, in the world's
/// x and z, less a quarter turn. The differences are float ones; the arctangent is taken in double and rounded to float
/// once, by the subtraction after it
[[nodiscard]] inline float FacingFromMiddle(const map_coords::MapCoords& middle, const map_coords::MapCoords& at)
{
	const float dx = map_coords::ToMetres(at.x) - map_coords::ToMetres(middle.x);
	const float dz = map_coords::ToMetres(at.z) - map_coords::ToMetres(middle.z);
	return static_cast<float>(std::atan2(static_cast<double>(dz), static_cast<double>(dx)) -
	                          static_cast<double>(game_math::k_PhysicsAngleOffset));
}

/// The heights after totem `moved` was set to `now`: its kept height becomes `now` counted round, and each other totem's
/// kept height moves by the table's entry times the move, counted round. The move is now - kept + 5
[[nodiscard]] constexpr std::array<int32_t, k_Totems> AfterMove(std::array<int32_t, k_Totems> heights, size_t moved,
                                                                int32_t now) noexcept
{
	const int32_t move = (now - heights.at(moved)) + k_Heights;
	heights.at(moved) = (heights.at(moved) + move) % k_Heights;
	for (size_t other = 0; other < k_Totems; ++other)
	{
		if (other != moved)
		{
			heights.at(other) = (k_Moves.at(moved).at(other) * move + heights.at(other)) % k_Heights;
		}
	}
	return heights;
}

/// Solved when every kept height is the highest
[[nodiscard]] constexpr bool Solved(std::span<const int32_t, k_Totems> heights) noexcept
{
	for (const auto height : heights)
	{
		if (height != k_Highest)
		{
			return false;
		}
	}
	return true;
}

} // namespace openblack::ecs::totem_puzzle
