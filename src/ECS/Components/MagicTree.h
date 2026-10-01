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

namespace openblack::ecs::components
{

/// MagicTree (a Tree, 0x74 bytes, vtable 0x92CA30; ctor 0x5FCF50): a tree the forest miracle made. The entity also has
/// the Tree, Transform, Fixed and Mesh of a normal tree. Magic/Objects/MagicTree.
struct MagicTree
{
	PlayerNames player {PlayerNames::NEUTRAL}; ///< +0x6C GetPlayerNumber of the spell's player (GetPlayer 0x5FD060)
	float woodValueMultiplier {1.0f};          ///< +0x70 woodValueMultiplier x the spell's tribal power (vt 0x868)
};

} // namespace openblack::ecs::components
