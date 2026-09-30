/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/PlayerAlignment.h"
#include "Enums.h"

// GAlignment (Alignment.cpp 0x414140..0x4146F0): the players' alignment, good (+1) to evil (-1): what applied effects
// and trees add to the change pending this turn, and the player's turn that folds it in. The one API for it (the
// "arboles" session's ECS/Alignment was folded in here). Wiki: docs/bw1-notes/magic.md, objects-and-resources.md.

namespace openblack::ecs::effects
{
struct EffectValues;

namespace alignment
{
/// fn_00414660: a change v scaled by the current alignment A (+8): v of A's sign (0 counts as positive) -> v (1 - |A| / 2),
/// of the opposite sign -> v (1 + |A| / 2)
[[nodiscard]] float ScaleChange(const components::PlayerAlignment& alignment, float change);

/// GAlignment::Update 0x414410 (object, values, life before): nothing unless the life changed. With
/// K = |life0 - life| + GPlayerInfo.applyEffectAlignmentChangeAddition (player 0's +0x64 -> +0x1C) and col = the
/// object's info alignmentType, `pending` gets ScaleChange(values[i] x GAlignmentInfo[i][col] x K) for crush, hit, heal
/// and fly away, and ScaleChange(ConvertTemperatureToDamage(burn) x GAlignmentInfo[0][col] x K). The alignment history
/// (CAlignmentHistory 0xC4CD40, fn_00414E10) is not kept.
void Update(components::PlayerAlignment& alignment, entt::entity object, const EffectValues& values, float lifeBefore);

/// The player's GAlignment (GPlayer +0x60): the player entity's components::PlayerAlignment (Magic/Core/Players)
[[nodiscard]] components::PlayerAlignment& Of(PlayerNames player);
/// GPlayer::GetAlignmentValue 0x64D6A0: +8, -1..1 (0 for a new game: GGame::Init 0x54FEA0 takes the profile's)
[[nodiscard]] float Get(PlayerNames player);
/// GAlignment::CrudeSet 0x4146F0: +8 = the value clamped to -1..1
void CrudeSet(PlayerNames player, float value);
/// GAlignment::CrudeUpdate 0x4146B0: +8 += the change, clamped to -1..1 at once (SET_ALIGNMENT, the network packets)
void CrudeUpdate(PlayerNames player, float change);
/// GAlignment::Update (player, tree, good) 0x4145A0: +-GPlayerInfo::treePullPutAlignmentChange, ScaleChange-d, into
/// `pending`. Uprooting with the hand is evil (Tree::InterfaceSetInMagicHand 0x74B730), planting good
/// (Tree::EndPhysics 0x74BBB6). TODO: CAlignmentHistory::Add 0x415260.
void UpdateForTree(PlayerNames player, bool good);
/// GAlignment::ProcessForPlayer 0x4141A0 -> GAlignment::Process 0x414140: the pending change clamped to -1..1, times
/// GPlayerInfo::maxAlignmentChangePerGameTurn, is CrudeUpdate-d and the pending change goes back to 0
void ProcessForPlayer(PlayerNames player);
/// GPlayer::Process 0x6496C5, once per turn for every player (Magic/MagicLoop.cpp, slot 3 GPlayer::ProcessPlayers)
void ProcessPlayers();
} // namespace alignment
} // namespace openblack::ecs::effects
