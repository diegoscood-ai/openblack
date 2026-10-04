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
	/// The SuperVillager's second yaw stage (ECS/SuperVillager.h; fn_00825530 0x8255C1..0x8256BF): with followRate > 0
	/// (rad/s, 3.92699 [0x9A392C]) followYaw (+0x14) chases the object's yaw (obj+0x48: the drawn +0x108 then,
	/// Villager::Draw 0x51BA5E, in LH3DObject::SetPosition's convention, `yaw` + 90 degrees) and the body is drawn turned
	/// about its own Y by their difference, followDrawnTurn (a local copy of the sheared object matrix, 0x8255AB rep
	/// movsd: ecs::DrawnBodyModel; `rotation` is not turned). followSnap is +0x30 bit 2, of this stage (0x8255B8: keeps
	/// them equal) and of the cross-fade (0x8257C4: not drawn, ECS/Animations.h); followTurn false (a swimmer, 0x825A13)
	/// moves followYaw but does not turn; followFrozen (fn_00825400's CheckRegionOnScreen 0x82541D failed) moves nothing.
	/// followYaw is set when the SuperVillager is made (fn_00825F20 0x825FBC, hasFollowYaw). Set by ECS/SuperVillager
	float followRate {0.0f};
	bool followSnap {false};
	bool followTurn {true};
	bool followFrozen {false};
	float followYaw {0.0f};
	bool hasFollowYaw {false};
	/// this frame's turn of the SuperVillager's drawn copy (followYaw - Wrap(target)), 0 for none
	float followDrawnTurn {0.0f};
};

} // namespace openblack::ecs::components
