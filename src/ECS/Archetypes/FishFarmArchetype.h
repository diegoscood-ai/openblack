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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs::archetypes
{
class FishFarmArchetype
{
public:
	/// 0x52C7B0(pos, GFishFarmInfo[info], town) -> FishFarm::FishFarm 0x52C360 (the nearest town) and
	/// CallVirtualFunctionsForCreation 0x52CC10: the farm and, if there is sea around it, its shoal of fish
	static entt::entity Create(const glm::vec3& position, uint32_t info);
	FishFarmArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
