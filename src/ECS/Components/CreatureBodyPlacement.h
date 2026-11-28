/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// Where a creature's body model is placed: at its position, turned a quarter turn past its body's yaw, at the scale
/// it is drawn at (ecs::creature_pose::PlaceBody). It is placed when the creature is made and again on every frame of
/// the world's view, never in a turn, so a turn's test of the creature on the screen (the scripts' field of view) finds
/// it where the last frame placed it
struct CreatureBodyPlacement
{
	/// The model's matrix (T(p) R S as a glm::mat4, the translation its column 3)
	glm::mat4 cells {1.0f};
	/// Its one scale
	float scale {1.0f};
};

} // namespace openblack::ecs::components
