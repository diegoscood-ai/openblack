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

#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"

// The arenas of a saved game's world file. Arenas are not entities of their own class in the file's writers: a land's
// arena (one the land script made, or one a load made) is an arena entity, and a creature fight's arena is part of the
// fight of the creature that made it. Both are objects of one source, named by these ids (see
// docs/wiki/engine/saved-games.md#writing-the-world-file).
namespace openblack::ecs::saved_world_write
{
/// The source id of a land's arena: its entity
[[nodiscard]] constexpr uint64_t LandArenaId(entt::entity arena)
{
	return PartId(arena, 0);
}

/// The source id of a creature fight's arena: the creature that made it
[[nodiscard]] constexpr uint64_t FightArenaId(entt::entity maker)
{
	return PartId(maker, 1);
}

/// The arena a creature's record names: the arena of its fight when it made it, else none. Only the creature that
/// makes a fight's arena names it; its opponent names none
[[nodiscard]] std::optional<SourceKey> ArenaKeyOf(const Registry& registry, entt::entity creature);
} // namespace openblack::ecs::saved_world_write
