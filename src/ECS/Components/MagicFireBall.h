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

#include "Enums.h"

namespace openblack::ecs::components
{

/// MagicFireBall (Object, 0x60 bytes, vtable 0x93595C): the invisible game object each fireball atom carries. It burns
/// through its FireEffect; the PSys draws it. Not in the map cells (InsertMapObject is empty). Magic/Objects/MagicFireBall.
struct MagicFireBall
{
	int infoRow {0};           ///< GMagicFireBallInfo[0..2] (0xD4E3B0, 0x10C each)
	bool affectedByRain {true}; ///< +0x58: not cast by a script
	uint32_t effect {0};       ///< the psys::manager effect of its atom (+0x5C AtomCore*)
	uint32_t atomKey {0};      ///< which atom of that effect (AttatchFireBallToAtom's atom data +0x28)
	bool hasPlayer {false};    ///< GetPlayer: the atom manager's player (vt 0x1C) or the current player
	PlayerNames player {PlayerNames::NEUTRAL};
	bool seen {true};          ///< refreshed by its atom this turn (the atom data +0x24 keeps the turn)
};

} // namespace openblack::ecs::components
