/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The player's worship icons as a whole (GPlayer.cpp 0x64AEE0, 0x64BAB0..0x64BFE0): what the gesture selection (lane
// M2: GInterface::ProcessPowerUpSystem 0x5CF300, OpenSelection 0x5CF010, SelectionStage 0x5CFAE0, the power-up
// gestures fn_005D0000) and the request packets 0x25 / 0x26 / 0x6A / 0x1E ask. `player` stands for the
// GInterfaceStatus: openblack's hand is the local player's.

namespace openblack::worship::player
{
/// GPlayer::ProcessSpellIcons 0x64AEE0 (from Spell::ProcessSpells, before the maintain requests): every player's
/// citadel (Citadel::ProcessSpellIcons)
void ProcessSpellIcons();

/// The icons of the player's six worship sites (player +0xA48 -> +0x34[6] -> +0xE0 list by +0x110), in that order
[[nodiscard]] std::vector<entt::entity> Icons(PlayerNames player);

/// fn_0064BAB0: an icon is charging for the player's hand (the scribble cancel, casting.md §4)
[[nodiscard]] bool AnyIconChargingFor(PlayerNames player);
/// fn_0064BB10: the largest charge fraction of the icons charging for the player (fn_0077F690)
[[nodiscard]] float MaxChargeFraction(PlayerNames player);
/// fn_0064BDE0: some icon is ValidForRequestSpell(status, -1, 1)
[[nodiscard]] bool AnyRequestableIcon(PlayerNames player);
/// fn_0064BE40: some icon whose seed's selectionGesture is `category` (1 SPIRAL, 2 INVERSE_SPIRAL) is requestable
[[nodiscard]] bool AnyRequestableIconOfCategory(PlayerNames player, GestureType category);
/// fn_0064BEC0: some site has a functional icon of that seed that is requestable (-1: no)
[[nodiscard]] bool IconValidForRequest(PlayerNames player, SpellSeedType seed);
/// GPlayer::FindBestSpellIconForSpellSeed 0x64BF40: of the six sites, the requestable icon of that seed at the site
/// with the most chants (fn_0077CBC0(0)); entt::null
[[nodiscard]] entt::entity FindBestSpellIconForSpellSeed(PlayerNames player, SpellSeedType seed);
/// fn_0064BDB0 (packet 0x25 -> 0x5DABA0): the best icon's fn_0077FB40(status, -1, 1): the seed into the hand when
/// full, else it starts charging
bool RequestSpell(PlayerNames player, SpellSeedType seed);
/// fn_0064BD90 (the R gesture's condition) / fn_0064BD50 (packet 0x26): the same with the interface's last seed type
/// (GInterfaceStatus +?, fn_005DCA40; set by SpellSeed::InterfaceSetInMagicHand / OutMagicHand)
[[nodiscard]] bool CanRepeat(PlayerNames player);
bool RepeatLastSpell(PlayerNames player);
void SetLastSeedType(PlayerNames player, SpellSeedType seed); ///< fn_005DCA20
[[nodiscard]] SpellSeedType LastSeedType(PlayerNames player); ///< fn_005DCA40
/// packet 0x6A (0x5DAC30 -> fn_0077FC30): the held seed's icon charges to that power-up level
bool SetChargingPowerUp(entt::entity icon, PlayerNames player, int powerUp);
/// fn_0064BCC0 (packet 0x1E, the scribble): the charge of the player's that started last is cancelled
bool CancelMostRecentCharge(PlayerNames player);
/// GPlayer::CancelAllSpellsCharging 0x64BC60 (CLEAR_PLAYER_SPELL_CHARGING)
bool CancelAllSpellsCharging(PlayerNames player);
/// GPlayer::IsAtLeastOneSpellBeginToBeCharged 0x64BB90 / IsSpellBeginToBeCharged 0x64BBF0 (IS_SPELL_CHARGING /
/// IS_THAT_SPELL_CHARGING): an icon (of that magic, fn_00726380) whose store is above 0
[[nodiscard]] bool IsAtLeastOneSpellBeginToBeCharged(PlayerNames player);
[[nodiscard]] bool IsSpellBeginToBeCharged(PlayerNames player, MagicType type);

/// GPlayer::SetMagicTypeEnabled's tail (0x64C300): every icon of the player's six sites redraws its levels (fn_0077B8A0)
void OnMagicTypesChanged(PlayerNames player);

/// A land is loaded
void Reset();
} // namespace openblack::worship::player
