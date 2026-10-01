/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// GAlignment (GPlayer +0x60 points at it; one per PlayerNames in Magic/Core/Players, kept across lands): the player's
/// alignment, -1 evil .. 1 good (+8), and the change gathered
/// this turn (+0xC) that the player's turn folds in (GAlignment::Process 0x414140, capped by
/// GPlayerInfo.maxAlignmentChangePerGameTurn). GAlignment::Update 0x414410 / 0x4145A0 add to `pending`
/// (ECS/Effects/Alignment.h).
struct PlayerAlignment
{
	float value {0.0f};   ///< +8
	float pending {0.0f}; ///< +0xC
};

} // namespace openblack::ecs::components
