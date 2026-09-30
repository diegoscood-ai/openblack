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

enum class MagicTreeType
{
};

struct Tree
{
	TreeInfo type;
	float maxSize;
	uint32_t forestId = 0;
	/// Tree +0x5E bit 1: the script's own flag (CREATE_NEW_TREE 0x716324); the hand sets it to "inside a town" when it
	/// replants the tree (Tree::EndPhysics 0x74BB5A).
	bool isNonScenic = true;
	/// Tree +0x5E bit 0: still growing. Set in the ctor 0x749E00 when maxSize differs from the size it is created at,
	/// cleared once it reaches maxSize.
	bool growing = false;
	/// Tree +0x60: turns left until the next growth step (info growTurns, randomised at creation)
	uint16_t growCounter = 0;
	/// Tree +0x5C bits 2-5: which of the 16 wind sway slots it uses, round(yAngle x 16 / 2pi) & 15 at creation
	/// (0x74A0E7), so that trees facing the same way sway together
	uint8_t windSlot = 0;
};

/// A tree that was thrown or dropped where it cannot be replanted (DeadTree, a Rock subclass in the original):
/// it keeps the tree mesh and the orientation it came to rest with, can be picked up again and gives wood.
struct DeadTree
{
	TreeInfo type;
};

} // namespace openblack::ecs::components
