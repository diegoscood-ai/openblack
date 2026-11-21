/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/ComputerPlayerScores.h"
#include "ECS/ComputerPlayerSeen.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs
{
class Registry;
}

/// What a computer player's mind reads of the game: the answers its score rules and its look round the hand get from
/// the game's own state. docs/wiki/hand/computer-minds.md
namespace openblack::ecs::systems::computer_world
{

/// What the score rules read of the game, for the mind of the player
[[nodiscard]] std::unique_ptr<computer_player::ScoreWorld> MakeScoreWorld(Registry& registry, PlayerNames player);

/// What the mind's look round its hand reads of the land
[[nodiscard]] std::unique_ptr<computer_player::SeenWorld> MakeSeenWorld(const Registry& registry);

} // namespace openblack::ecs::systems::computer_world
