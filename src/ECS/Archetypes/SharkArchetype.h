/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs::archetypes
{
class SharkArchetype
{
public:
	/// Whale::Create 0x774C50 (CHL CREATE of type Whale, GScript fn_006F11A0 0x6F1747) and its
	/// CallVirtualFunctionsForCreation 0x774CA0: at twice the scale, mesh 31 MSH_SHARK_BONED playing the looping clip
	/// 129 ANM_SHARK_BONED_SWIM, drawn cut by the water (components::CutByPlane, both parts). `position` is on the
	/// ground (GetAltitude, 0 over the sea).
	static entt::entity Create(const glm::vec3& position, float yAngleRadians, float scale);
	SharkArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
