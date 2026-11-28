/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// What a script last gave a villager for the script states it puts it in: a place and a number. Each state reads them
/// its own way: wandering round a place takes the place, and the number as how far from it to wander. The pair of whole
/// numbers the scripts also give is the villager's ScriptAnimation
struct ScriptStateArguments
{
	/// On the ground, in metres
	glm::vec2 place {0.0f};
	float number {0.0f};
};

} // namespace openblack::ecs::components
