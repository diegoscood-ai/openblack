/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::components
{

/// A PuzzleGame (puzzlegame.cpp, 0x588 bytes, list g_game+0x205D14), made by the CHL CREATE / CREATE_WITH_ANGLE_AND_SCALE
/// of type 32 (fn_006F11A0 0x6F184C -> fn_006D6680(pos, sub_type, angle x 2048 / 2 pi, scale)). Only the fish puzzle
/// (type 14) is ported (ecs/PuzzleGames.h).
struct PuzzleGame
{
	script::PuzzleGameType type {script::PuzzleGameType::None}; ///< +0x48
	glm::vec3 position {0.0f};  ///< +0x14 as a point: (x, altitude + y, z) (MapCoords::ConvertToLHPoint 0x6041C0)
	int32_t angle {0};          ///< the 2048-step angle
	float scale {1.0f};
	bool played {false};        ///< +0x3C: once set the puzzle is not processed again
	entt::entity bait {entt::null};                          ///< type 14: +0x408
	std::array<entt::entity, 2> shoals {entt::null, entt::null}; ///< type 14: +0x400, +0x404
};

} // namespace openblack::ecs::components
