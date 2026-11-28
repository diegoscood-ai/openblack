/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <SavedRecords.h>

#include "ECS/Components/PlayerMagic.h"
#include "Game/GameStats.h"

namespace openblack::gamesave
{
struct WorldRead;
}

// The players' game statistics of a saved game's world file (docs/wiki/engine/saved-games.md#openblack), put back as
// the original's load reads them: every figure of the player's statistics, both histories where they stood, the spells
// cast and the chants used, and the figures of the game's whole run, which each player's record carries again (the last
// one read stands).
namespace openblack::ecs::load_game_stats
{

/// A player's statistics, the figures of the whole run and the player's spells cast and chants used, from its record.
/// The towns' totals are taken as the save kept them, standing in the turn they were summed in. The objects created are put
/// back as the original's load reads them: the record's count stands for the `entitiesMade` the registry has made by
/// then, and the count goes on from it (game_stats::ObjectsCreated)
void PutBack(const gamesave::GameStatsRecord& record, game_stats::Stats& stats, game_stats::Statics& statics,
             components::PlayerMagic& magic, uint32_t entitiesMade = 0);

/// Every player's statistics record of the world read put back into the game's statistics, each to the player its
/// player's record names (a record naming none is left out); how many were put back
size_t PutBackAll(const gamesave::WorldRead& read);

} // namespace openblack::ecs::load_game_stats
