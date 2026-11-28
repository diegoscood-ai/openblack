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

#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"

// The data paths of a saved game's world file: each the walk of a living thing along a script's track, part of the
// living, named by its source id.
namespace openblack::ecs::saved_world_write
{
/// The source id of the data path of a living that walks a track
[[nodiscard]] constexpr uint64_t DataPathIdOf(entt::entity living)
{
	return PartId(living, 0);
}
} // namespace openblack::ecs::saved_world_write
