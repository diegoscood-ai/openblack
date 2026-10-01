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
#include <glm/vec3.hpp>

#include "Enums.h"

// WorshipSpellIcon (WorshipSpellIcon.cpp 0x77F140..0x780640) and the SpellIcon base it shares with the town centre's
// icons (SpellIcon.cpp 0x725FF0..0x726D30): charging from the site's prayer power, the fully charged seed into the
// hand, cancelling, the chant store. `player` stands for the GInterfaceStatus (Worship/InterfaceStatus.h).

namespace openblack::worship::icon
{
/// WorshipSpellIcon::Create 0x77F2B0 (pos, &spellIcon[0], seed, site, slot, scale 1): the icon object (mesh 203 at the
/// site's scale and angle), its SpellSeedGraphic (Create3DSpellObject 0x726210), in the site's list
/// (WorshipSite::AddSpellIcon 0x77C430) and its power-up levels drawn (UpdateGraphicsWithPULevels 0x77F320)
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seed, entt::entity site, int16_t slot);

/// WorshipSpellIcon::ToBeDeleted 0x77F230: out of the site, its seeds go, then SpellIcon::ToBeDeleted 0x7260A0 (its
/// graphic goes)
void ToBeDeleted(entt::entity icon);

/// SpellIcon::GetPlayer 0x726540: the site's player
[[nodiscard]] PlayerNames PlayerOf(entt::entity icon);
/// SpellIcon::GetSpellSeedType 0x726360
[[nodiscard]] SpellSeedType SeedTypeOf(entt::entity icon);
/// SpellIcon::GetMagicInfoFromPULevel 0x7262A0 -> its magic type
[[nodiscard]] MagicType MagicTypeOf(entt::entity icon, int powerUp);

/// WorshipSpellIcon::UpdateGraphicsWithPULevels 0x77F320: the graphic shows the highest power-up level the player has
/// enabled (0..2, or -1), at alpha 0.5
void UpdateGraphicsWithPULevels(entt::entity icon);

/// WorshipSpellIcon::Process 0x77F390 (every turn, from WorshipSite::ProcessSpellIcons): the removal countdown, the
/// neutral self-charge (game flag 0x2000), and while charging: the chants into the held seed, and when full the seed's
/// power-up (held) or the seed into the hand, the voice, the charge ends. 3 when it removed itself.
int Process(entt::entity icon);

/// WorshipSpellIcon::GetHeldSpellSeed 0x77F490: a seed of this icon in a hand of the icon's player
[[nodiscard]] entt::entity GetHeldSpellSeed(entt::entity icon);
/// GetChantRequiredForSpellSeed 0x77F840: the held seed's need at the charged level, else costToCreate of that level
[[nodiscard]] float GetChantRequired(entt::entity icon);
/// GetChantNeededForSpellSeed 0x77FE40: the held seed's need, else required - store
[[nodiscard]] float GetChantNeeded(entt::entity icon);
/// fn_0077FE80: the charge fraction (store / required; for a held seed need / seed need at that level)
[[nodiscard]] float ChargeFraction(entt::entity icon);
/// fn_0077F6D0: charging (for that player, or any with `anyPlayer`)
[[nodiscard]] bool IsCharging(entt::entity icon, PlayerNames player, bool anyPlayer);

/// WorshipSpellIcon::AddToChantStore 0x77FDA0. The original's quirk is kept: below the requirement it returns what
/// was added; over it the store stops at the requirement and it returns the excess `x - (required - store)`, and
/// that is what the site is charged.
float AddToChantStore(entt::entity icon, float chants);
/// RemoveFromChantStore 0x77FE10: takes min(x, store) out of the store and returns it
float RemoveFromChantStore(entt::entity icon, float chants);
/// ReturnAllChantsToWorshipSite 0x77FD60: the store back into the site's battery
void ReturnAllChantsToWorshipSite(entt::entity icon);
/// UseCreateChants 0x77FCE0: the store into the seed, at most its need
float UseCreateChants(entt::entity icon, entt::entity seed);

/// fn_007282A0 (pos, icon, status, pu, multiplier): a seed of this icon at pos, linked to it (its creator is the icon);
/// OneOffSpellSeed::CreateSpellIntoHand 0x72A730 uses it for a one-shot seed when the player has an icon of that seed
[[nodiscard]] entt::entity CreateSeed(entt::entity icon, const glm::vec3& position, PlayerNames player, int powerUp,
                                      float multiplier);
/// ValidForPutFullyChargedPowerUpSeedInHand 0x77F950
[[nodiscard]] bool ValidForPutFullyChargedSeedInHand(entt::entity icon, PlayerNames player);
/// PutFullyChargedPowerUpSeedInHand 0x77F8F0: a seed of this icon (fn_007282A0) with the store in it, into the hand,
/// then fn_00729900(1) (it waits delayBeforeSeedActive). 1 when it was put.
int PutFullyChargedSeedInHand(entt::entity icon, PlayerNames player);
/// WorshipSpellIcon::CancelCharge 0x77F9A0: for that player's charge (or a store): stop, and the store back to the site
bool CancelCharge(entt::entity icon, PlayerNames player);
/// fn_0077FA00 StartCharge(status, pu, requireChants): not already charging, and chants to charge from (the site's or the
/// store) when asked; the seed goes in at once when already full
bool StartCharge(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// ValidForStartCharge 0x77FAB0
[[nodiscard]] bool ValidForStartCharge(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// ValidForRequestSpell 0x77FBA0: built, and ValidForPut... or ValidForStartCharge
[[nodiscard]] bool ValidForRequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// fn_0077FB40 (packet 0x25 through GPlayer 0x64BDB0): the seed if full, else start charging
bool RequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// fn_0077FBF0: a seed of this icon is held and the power-up level's magic is enabled for the player
[[nodiscard]] bool PowerUpValid(entt::entity icon, PlayerNames player, int powerUp);
/// fn_0077FC30 (packet 0x6A): charge the held seed to that power-up level
bool SetChargingPowerUp(entt::entity icon, PlayerNames player, int powerUp);

/// WorshipSpellIcon::ActualInterfaceTap 0x77F880: the full seed into the hand, else cancel, else start charging
int ActualInterfaceTap(entt::entity icon, PlayerNames player);
/// SpellIcon::InterfaceValidToTap 0x7263C0 (built; the player's own icon, or any neutral one in the 0x2000 mode)
[[nodiscard]] bool InterfaceValidToTap(entt::entity icon, PlayerNames player);
/// SpellIcon vt 0x914 GetWorshipSpellIcon: a worship icon is its own; a town centre's icon asks its town's site for the
/// icon of its seed (TownSpellIcon::GetWorshipSpellIcon 0x748F30 -> fn_0077C2B0); entt::null
[[nodiscard]] entt::entity WorshipIconOf(entt::entity icon);
/// SpellIcon::InterfaceTap 0x726430 for a worship icon or a town centre icon (it forwards to the site's icon of its
/// seed): ActualInterfaceTap, and for the local player the tap sound (fn_00726490: IN_GAME 42 G_ClickOnSpell_01 at
/// pitch {100, 115, 130, 145, 155, 175}[placement index] %). 1 when an icon took the tap.
int InterfaceTap(entt::entity icon, PlayerNames player);

/// WorshipSpellIcon::MaintainSpell 0x77F6F0: the site pays; an icon without a site pays from its store
float MaintainSpell(entt::entity icon, float amount);

/// fn_0077F780 / fn_0077F7D0: a seed made from the icon joins / leaves its list (and the seed's flag bit 0)
void AddSeed(entt::entity icon, entt::entity seed);
void RemoveSeed(entt::entity icon, entt::entity seed);

/// The per-frame part of SpellIcon::DrawMagicSystem 0x726D20 (TChargingData::Draw 0x7267A0): the pulse ring while
/// charging. phase: the PSys global phase [0xD4EBF8], 0..1 over 3.33 s (Worship/SpellSeedGraphic.h).
void UpdateChargingVisual(entt::entity icon, float phase);
} // namespace openblack::worship::icon
