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

#include <optional>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/WallHug.h"

/// When a walker is where it was going, as the game decides it. Pure.
namespace openblack::ecs::walk_arrival
{

/// Whether a walk to a goal is over for the state waiting on it. A walker within a step of its goal takes its last step
/// onto it on the next turn: the walk is over once it stands on its goal, not when it first comes within the step. A
/// walker with no walk under way (no state) is done. The positions are the walk's own, on the ground plane
[[nodiscard]] bool WalkIsOver(std::optional<components::MoveState> state, glm::vec2 position, glm::vec2 goal);

/// Whether a walker at `position` is there at `goal`: the x / z distance is less than its speed (whole map units a
/// turn) plus `extra` (map units too), strictly. The differences are whole units; the game's maths runs at a float's
/// precision, so each square, their sum, the reach and its square are each rounded to a float before the compare
[[nodiscard]] bool AreWeThere(const map_coords::MapCoords& position, const map_coords::MapCoords& goal, uint16_t speed,
                              float extra);

} // namespace openblack::ecs::walk_arrival
