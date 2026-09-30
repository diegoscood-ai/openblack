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

/// An animal of the scripts (CREATE_ANIMAL / CREATE_NEW_ANIMAL, fn_00419D10). Only the ground animals exist so far;
/// they stand still (no animal AI yet). `humanShadowed` is LH3DObject flag 0x4000000 (SetHumanShadowed): the ground blobs.
struct Animal
{
	AnimalInfo type;
	uint32_t age;
	entt::entity flock {entt::null}; ///< Living::SetFlock: its components::Flock
	bool humanShadowed {true};
	/// +0xE0 (fn_00417C50, which also puts it on the town's list +0x984): only the animals that can be shepherded
	/// (IsOkToBeShepherd, vtable +0xBA4: sheep, goat, tortoise, zebra, cow, horse, pig) keep the script's town
	entt::entity town {entt::null};
	/// the player it belongs to (GetPlayer; the spells' animals), -1 none
	int32_t player {-1};
};

} // namespace openblack::ecs::components
