/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// Mod test.miracle-dispensers (openblack only, not in the original; Mods/Builtin/MiracleDispensersMod.cpp sets its
// EngineConfig switches): once a land's script has run and the human player has a citadel, one miracle dispenser
// (the original's SpellDispenser, as the Land 1 challenge script's GiveSpellDispenserReward makes one) per player
// miracle seed on open dry land in a ring around the temple, with a short recharge period. They and their orbs take
// their creation index from object_index's mods range, so the original's objects keep theirs.
// Wiki: docs/bw1-notes/mod-library.md#testmiracle-dispensers

namespace openblack::worship::test_dispensers
{
/// A land is loaded: nothing placed yet
void Reset();
/// Every turn after the land's PostLoadCleanup: places them the first turn the mod is on and the citadel exists
void ProcessTurn(uint32_t turn);
} // namespace openblack::worship::test_dispensers
