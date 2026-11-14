/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <span>
#include <utility>
#include <vector>

#include <SavedRecords.h>
#include <entt/entity/entity.hpp>

#include "Audio/Services/ThingMusic.h"

namespace openblack::ecs::saved_world_load
{
struct PlannedScriptMusic;
}

// The audio of a saved game's world file (docs/wiki/engine/saved-games.md#openblack), put back as the original's load
// reads it: the music attached to things, the list whole, head first, each with its music type, its thing, whether it
// is enabled, finished and started, and the place it plays at when it has one; and the script music and the town the
// music last played for.
namespace openblack::ecs::load_thing_music
{

/// A thing's music of the save, by its number in the world file, with its record
using Planned = std::pair<gamesave::ObjectNumber, gamesave::ThingMusicInfoRecord>;

/// The list of the music attached to things from the saved ones, in their order: each of the thing its record names,
/// with its values as saved, its place from its map coordinates (x and z in 16.16 units, the height above the land)
/// and its number in the file as its key; one whose thing the load did not make is left out
[[nodiscard]] std::vector<audio::ThingMusicInfo> InfosOf(std::span<const Planned> saved,
                                                         const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);

/// The music's list of things in place of the game's, when the game's music is on
void Restore(std::vector<audio::ThingMusicInfo> infos);

/// The script music, its type and whether it plays, and the town the music last played for (null: none), as the
/// game's audio load reads them, when the game's music is on; the alignment music playing is unset and the last one none
void RestoreScriptMusic(const saved_world_load::PlannedScriptMusic& music, entt::entity town);

} // namespace openblack::ecs::load_thing_music
