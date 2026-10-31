/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/WallHugBookkeeping.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::wall_hug
{
/// The walker's move state as the game numbers it, from its move state tag; none without one
[[nodiscard]] std::optional<WalkState> WalkStateOf(const Registry& registry, entt::entity walker);
} // namespace openblack::ecs::wall_hug
