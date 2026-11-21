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

#include "ECS/Registry.h"
#include "ECS/WallHugBookkeeping.h"

namespace openblack::ecs::wall_hug
{
/// The walker's move state as the game numbers it, from its move state tag; none without one
[[nodiscard]] std::optional<WalkState> WalkStateOf(const Registry& registry, entt::entity walker);

/// Keeps the walker's move state from its tags, while it has one, as the state the original keeps after the walk
/// (components::WalkStateAfterWalk)
void KeepWalkState(Registry& registry, entt::entity walker);

/// The walker lets go of the circle it holds and forgets the turns to it and its entry distance, as the circle hug's
/// reset does
void ResetCircleHug(Registry& registry, entt::entity walker);

/// Drops these move state tags of the walker, keeping the move state they gave first
template <typename... Tags>
void DropWalkTags(Registry& registry, entt::entity walker)
{
	KeepWalkState(registry, walker);
	registry.Remove<Tags...>(walker);
}
} // namespace openblack::ecs::wall_hug
