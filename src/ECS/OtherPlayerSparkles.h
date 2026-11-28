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

#include <glm/vec3.hpp>

#include "Enums.h"

/// The sparkles of the other gods (rules in player_sparkles): each turn a sparkle where each of them reports its camera,
/// drawn with its symbol by the renderer from the dynamics system's pools
namespace openblack::ecs::other_player_sparkles
{

/// Where a god's interface reports its camera: for a computer god, where its hand was at the end of its last turn of
/// steps, else the camera over its citadel; none for a god with neither (not known, see the wiki's computer hands page)
[[nodiscard]] std::optional<glm::vec3> ReportedCamera(PlayerNames player);

/// Once a game turn, after the hand's turn: nothing while the local player is in its temple or the falling spell's film
/// plays, or while its hand directs a creature fight; else, from the first player to the last before the neutral one, every god
/// but the local player that shows sparkles (player_sparkles::Shows) gets one at its reported camera, in a tint of its colour
/// on this land, and its pool is drawn there. Each takes two numbers of the local random stream, the tint's and the kind's, and
/// the pool's add takes one of the C runtime's rand.
void AddForTurn();

/// Whether a god's pool is drawn with its symbol: not while the local player's hand directs a creature fight, and only
/// for a god that shows sparkles now; its sparkles are drawn either way
[[nodiscard]] bool SymbolShown(PlayerNames player);

/// The local player goes into its temple: every other god's sparkles go
void OnGoInsideCitadel();

} // namespace openblack::ecs::other_player_sparkles
