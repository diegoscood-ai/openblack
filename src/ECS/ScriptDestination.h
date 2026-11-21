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

#include "3D/MapCoords.h"

namespace openblack::ecs
{
class Registry;
}

/// The destination of a living thing as the scripts read it (GET_OBJECT_DESTINATION): a map position with a height
/// above the land. The game keeps one such place for each living thing:
/// - A villager: the goal of its walk, with the height above the land a script's walk gives it (ECS/WalkGoal.h).
/// - An animal: the goal of its move, and the height above the land that it wants to have there.
/// - A creature: the place where it last set off to (CreatureLocomotion::lastDestination).
/// Any other thing has no destination.
namespace openblack::ecs::script_destination
{

/// The destination of a thing, none for a thing that does not live
[[nodiscard]] std::optional<map_coords::MapCoords> Of(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs::script_destination
