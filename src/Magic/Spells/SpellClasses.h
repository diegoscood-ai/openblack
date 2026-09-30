/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// One registration per Spell class file (Spells/*.cpp), called in this order by magic::RegisterSpellClasses
// (Core/Spell.cpp). A class nobody registered runs as the plain Spell.

namespace openblack::magic
{
void RegisterGeneralSpell(); ///< SpellGeneral.cpp: the plain Spell (fireball, lightning bolt, beam explosion)
void RegisterHealSpell();    ///< SpellHeal.cpp
void RegisterResourceSpell(); ///< SpellResource.cpp: food and wood
void RegisterForestSpell();   ///< SpellForest.cpp: the forest (NATURE)
void RegisterTeleportSpell(); ///< SpellTeleport.cpp: the teleport stones (M6)
void RegisterShieldSpell();   ///< SpellShield.cpp: the magic and physical shields (M6)
} // namespace openblack::magic
