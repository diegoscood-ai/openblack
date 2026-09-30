/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where a villager or animal is drawn this frame (ECS/MobileDrawing.h), instead of its Transform: between its
/// positions at the start and the end of the last turn, turned smoothly, sheared along the slope. Only for drawing.
struct DrawPosition
{
	/// Living +0x2C: the position at the start of the last processed turn (Living::ProcessLiving copies Pos into it)
	glm::vec3 turnStart {0.0f};
	bool started {false};
	/// Villager +0x108: the drawn yaw, turning towards the real one every frame
	float yaw {0.0f};
	bool hasYaw {false};
	/// this frame's result
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// fn_0051B220: the slope shear, the land's rise along the object's x and z axes (row0 += a row1, row2 += b row1)
	float shearX {0.0f};
	float shearZ {0.0f};
};

} // namespace openblack::ecs::components
