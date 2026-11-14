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

// The town centre's spell icons: one TownCentreSpellIcon per spell seed the town holds, at most 6, on the town
// centre mesh's special points 0..5.
// Tapping one taps the worship site's icon of the same seed.

namespace openblack::worship::town_centre
{
/// Nothing if an icon of the seed exists; else in the first free slot whose special point exists, a town spell icon
/// at the point with its angle and the town centre's scale. It goes in the town's list and so at the town's worship
/// site. True when made.
bool AddSpell(entt::entity townCentre, SpellSeedType seed);
/// The seed's icons leave the town centre and go (out of the town's list, and the site's icon if no town holds the
/// seed any more)
void RemoveSpell(entt::entity townCentre, SpellSeedType seed);
/// The power-up level set / cleared on the seed's icon
void AddPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
void ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
/// The town centre's icon of that seed, or entt::null
[[nodiscard]] entt::entity FindSpellIcon(entt::entity townCentre, SpellSeedType seed);
/// How many icons (the town centre can hold more while < 6)
[[nodiscard]] int SpellCount(entt::entity townCentre);

/// The icons part of a town centre's removal: each of the 6 slots is emptied, the icon forgets its town centre and is
/// deleted unless a flag of it is set ((pending) the flag is not identified; openblack's icons go at once). A slot
/// never holds a destroyed icon: OnToBeDeleted, and the registry's destruction of the icon on any other path, empty it
void DeleteDependants(entt::entity townCentre);

/// The icon's own part of its deletion, however it goes (a town's removal of the spell, its centre's removal, a fire
/// burning it down, an explosion, a tornado, ecs::ToBeDeleted): it leaves its town centre's slot, then its town's list
/// (the town's worship site loses its icon of the seed when no town holds the seed any more), then its seed graphic
/// is deleted. Nothing for an entity that is not a town centre's icon
void OnToBeDeleted(entt::entity icon);
/// The registry's destruction of a town centre's icon, on any path, empties its centre's slot and takes it out of its
/// town's list. Connected when an icon is made (once per registry)
void ConnectDeletionListener();

/// A town centre is made: an icon for every magic the town already holds
/// (AddSpell + AddPowerUp), then site::AddTownSpells
void MakeFunctional(entt::entity townCentre);

/// The town changes hands: every icon of the 6 slots is the new owner's, and so are its seed graphic and the
/// graphic's holder effect (seed_graphic::SetPlayer). An icon belongs to its town, so its own player is the town's
void SetPlayer(entt::entity townCentre, PlayerNames player);
/// The player each icon's seed graphic is drawn for, slot by slot (empty slots left out): a debug readout
[[nodiscard]] std::vector<PlayerNames> GraphicPlayers(entt::entity townCentre);
} // namespace openblack::worship::town_centre
