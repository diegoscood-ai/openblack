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

#include "Enums.h"

namespace openblack::ecs::components
{

enum class MagicTreeType
{
};

struct Tree
{
	TreeInfo type;
	/// +0x64 the scale it grows to (the Tree ctor 0x749E00's maxScale; a magic tree's target scale). The scale itself is
	/// the Transform's.
	float maxSize;
	uint32_t forestId = 0;
	/// Trees planted near a town are scenic: foresters leave them alone (Tree +0x5e bit 2 in the original).
	bool isNonScenic = true;
	/// +0x5E bit 0: made with a scale other than maxSize, so it grows (Tree ctor 0x749E00). ECS/TreeGrowth.
	bool growing = false;
	/// +0x60 (int16): the turns to its next growth step, GameRand(growsAfterNumGameTurns) at creation (Tree::Process
	/// 0x74A290 counts it down)
	int16_t growCountdown = 0;
	/// +0x68 the Forest container it is in (ECS/Forests; only the forest miracle makes them yet), entt::null none
	entt::entity forest {entt::null};
};

/// A tree that was thrown or dropped where it cannot be replanted (DeadTree, a Rock subclass in the original):
/// it keeps the tree mesh and the orientation it came to rest with, can be picked up again and gives wood.
struct DeadTree
{
	TreeInfo type;
};

} // namespace openblack::ecs::components
