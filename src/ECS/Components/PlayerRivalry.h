/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "Enums.h"

namespace openblack::ecs::components
{

/// What a player keeps of the other players and of their own influence, the whole game (one per PlayerNames in the
/// player system, kept across lands; ecs::player_rivalry): the harm each player's effects did this player's things, and
/// the running average of this player's influence power. A computer player weighs both when it picks a player to defeat.
struct PlayerRivalry
{
	/// The harm done to this player's things by each player's effects, by that player's number
	std::array<float, static_cast<size_t>(PlayerNames::_COUNT)> harmFrom {};
	/// Halfway to the influence power each time the power is worked out
	float averageInfluencePower {0.0f};
};

} // namespace openblack::ecs::components
