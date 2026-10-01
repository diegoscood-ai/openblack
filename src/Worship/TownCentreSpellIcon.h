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

#include "Enums.h"

// The town centre's spell icons (TownCentre.cpp 0x743F60..0x744320, TownSpellIcon.cpp 0x748A70..0x748F70): one
// TownCentreSpellIcon per spell seed the town holds, at most 6, on the town centre mesh's special points 0..5.
// Tapping one taps the worship site's icon of the same seed.

namespace openblack::worship::town_centre
{
/// TownCentre::AddSpell 0x744050: nothing if an icon of the seed exists; else the first free slot whose special point
/// exists (fn_00743F60), fn_00748CB0(pos, &spellIcon[1], seed, TC, the point's angle, TC scale, 1, 0). Its ctor
/// fn_00748A70 puts it in the town's list (fn_0073D1C0) and so at the town's worship site. 1 when made.
bool AddSpell(entt::entity townCentre, SpellSeedType seed);
/// fn_007442C0: the seed's icons leave the town centre and go (TownSpellIcon::ToBeDeleted 0x748AE0 -> Town::RemoveSpellIcon
/// 0x73D220 -> the site's fn_0077CAA0)
void RemoveSpell(entt::entity townCentre, SpellSeedType seed);
/// TownCentre::AddPowerUp 0x744010 / fn_00744030: SetPULevel 0x748EB0 (pu, 1 / 0) on the seed's icon
void AddPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
void ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
/// TownCentre::FindSpellIcon 0x743FA0
[[nodiscard]] entt::entity FindSpellIcon(entt::entity townCentre, SpellSeedType seed);
/// fn_00744120: how many icons (TownCentre::CanTownCentreHoldMoreSpells 0x73D5F0: < 6)
[[nodiscard]] int SpellCount(entt::entity townCentre);

/// TownCentre::MakeFunctional 0x743E80 (a town centre is made): an icon for every magic the town already holds
/// (AddSpell + AddPowerUp), then WorshipSite::AddTownSpells
void MakeFunctional(entt::entity townCentre);
} // namespace openblack::worship::town_centre
