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

#include "ECS/Components/WalkPath.h"

namespace openblack::ecs::components
{

/// A villager walking a track (ECS/LivingWalkPath.h): the walk's step from its speed, the walk's speed that speed. A type
/// of its own, so that the walk path system's turn does not move it
struct LivingWalkPath
{
	WalkPath path;
	int32_t trackNumber {0};
};

} // namespace openblack::ecs::components
