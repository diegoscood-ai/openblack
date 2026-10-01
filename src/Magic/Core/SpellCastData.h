/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The cast arguments of Spell::InitWithPos 0x71FE50 (PSysProcessInfo and SpellEventInfo are in PSys/SpellLink.h).

namespace openblack::magic
{

/// SpellCastData (16 bytes, init fn_0071FA10)
struct SpellCastData
{
	/// +0 the gesture packet's size (+0x14) for hand casts (1 for a FIRE seed, DoPreCastThings 0x72950B), the script's radius
	float magnitude {0.0f};
	float chants {0.0f};        ///< +4 effect.initialChants x the seed's multiplier
	float duration {-1.0f};     ///< +8 seconds: timerWhenPlayerCasting x the multiplier, or the script's time
	int maxObjectsToCreate {-1}; ///< +0xC the seed's stored count, or -1
};

} // namespace openblack::magic
