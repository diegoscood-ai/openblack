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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

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
	/// Tree::Draw 0x74AB8B: this frame's bend away from the object carried by the hand, a physics object or a
	/// creature (the entry of table 0xD19A48 its bits 6-9 of +0x5C point at), only the drawn matrix: the angle (0 = not
	/// bent) and the horizontal direction from that object to the tree the crown leans towards
	float bendAngle = 0.0f;
	glm::vec2 bendDirection {0.0f, 1.0f};
	/// bent last frame too: the rubbing sound plays when a bend starts
	bool wasBent = false;
	/// When it went into its map cell's fixed list (Object::InsertMapObject 0x636740 -> Fixed::InsertMapObjectToCell
	/// 0x52DEA0 puts it at the HEAD): MapCell::FindTypeOnMap finds the tree inserted last first. Set at creation and when
	/// it is planted again.
	uint32_t mapInsertion = 0;
};

/// A tree that was thrown or dropped where it cannot be replanted (DeadTree, a Rock subclass in the original):
/// it keeps the tree mesh and the orientation it came to rest with, can be picked up again and gives wood.
struct DeadTree
{
	TreeInfo type;
};

/// A tree a forester felled (FelledTree, a DeadTree whose vtable FelledTree::Create 0x5116A0 swaps in): it falls with
/// physics away from the forester and has no "wood here" reaction when it lands (FelledTree::EndPhysics 0x511970 goes
/// straight to Fixed::EndPhysics). The entity also keeps its DeadTree.
struct FelledTree
{
	entt::entity chopper {entt::null};
};

} // namespace openblack::ecs::components
