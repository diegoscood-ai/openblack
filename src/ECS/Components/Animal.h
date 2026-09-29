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

/// An animal of the scripts (CREATE_ANIMAL / CREATE_NEW_ANIMAL, fn_00419D10). Only the ground animals exist so far;
/// they stand still (no animal AI yet). `humanShadowed` is LH3DObject flag 0x4000000 (SetHumanShadowed): the ground blobs.
struct Animal
{
	AnimalInfo type;
	uint32_t age;
	int32_t flock;
	bool humanShadowed {true};
};

} // namespace openblack::ecs::components
