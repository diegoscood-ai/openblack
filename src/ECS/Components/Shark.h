/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
/// A shark: the class Whale of the original (Whale.cpp, 0x74 bytes, vtable 0x8FEBEC; GMobileObjectInfo[24]), kept in
/// the list g_game+0x205D04. It has no AI of its own: the script's WALK_PATH moves it (ECS/MobileWalkPaths.h).
struct Shark
{
	/// +0x2C: the position at the start of the turn (Whale::Process 0x775280 copies Pos into it every turn)
	glm::vec3 turnStart {0.0f};
	/// +0x6C: the drawn heading (LH3DMath::GetYAngle of the turn's move, kept while it stands still). The constructor
	/// writes the creation angle there, then CallVirtualFunctionsForCreation 0x774CA0 clears it to 0.
	float heading {0.0f};
};

} // namespace openblack::ecs::components
