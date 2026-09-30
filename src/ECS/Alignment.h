/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

/// The players' alignment, good (+1) to evil (-1): GAlignment (GPlayer +0x60), research dev\tmp_dis\trees2 and
/// docs/bw1-notes/objects-and-resources.md. Acts add to a pending change that the player's turn applies, capped.
namespace openblack::ecs::alignment
{

/// GAlignment +0x08, the alignment (-1..1); 0 for a new game (GGame::Init 0x54FEA0 takes the profile's, 0 without one)
[[nodiscard]] float Get(PlayerNames player);

/// GAlignment::CrudeSet 0x4146F0: sets it, clamped to -1..1
void Set(PlayerNames player, float value);

/// GAlignment::CrudeUpdate 0x4146B0: adds to it at once, clamped to -1..1 (SET_ALIGNMENT, the network packets)
void AddNow(PlayerNames player, float change);

/// fn_00414660: an act's change weighed by the current alignment: pushing the way it already leans counts less,
/// v (1 - |a| / 2); pulling it back counts more, v (1 + |a| / 2)
[[nodiscard]] float Weigh(float alignment, float change);

/// GAlignment::Update (player, tree, good) 0x4145A0: +-GPlayerInfo::treePullPutAlignmentChange, weighed, added to the
/// pending change. Uprooting with the hand is evil (Tree::InterfaceSetInMagicHand), planting a tree good
/// (Tree::EndPhysics, Tree::ApplyWaterSpell).
void UpdateForTree(PlayerNames player, bool good);

/// Adds an already weighed change to the pending one (GAlignment +0x0C)
void AddPending(PlayerNames player, float change);

/// GPlayer::Process 0x6496C5 -> GAlignment::ProcessForPlayer 0x4141A0 -> GAlignment::Process 0x414140, once per turn:
/// the pending change clamped to -1..1, times GPlayerInfo::maxAlignmentChangePerGameTurn, is added (CrudeUpdate) and
/// the pending change is cleared.
void ProcessTurn();

} // namespace openblack::ecs::alignment
