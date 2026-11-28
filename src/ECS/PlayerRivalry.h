/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/PlayerRivalry.h"
#include "Enums.h"

// What a player keeps of the others and of their own influence (components::PlayerRivalry): an effect of a player that
// harms a thing of another adds its harm to that other's ledger, and each time a player's influence power is worked out
// its running average goes halfway to it. Pure rules on the record; the player service keeps one record per player.

namespace openblack::ecs::player_rivalry
{

/// A thing of the player was harmed by an effect of `by`: the harm adds to what the player holds against them. A number
/// out of range adds nothing
void AddHarm(components::PlayerRivalry& rivalry, PlayerNames by, float harm);

/// The player's influence power was worked out: its running average goes halfway to it
void NoteInfluencePower(components::PlayerRivalry& rivalry, float power);

} // namespace openblack::ecs::player_rivalry
