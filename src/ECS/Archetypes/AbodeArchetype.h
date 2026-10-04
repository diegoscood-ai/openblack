/******************************************************************************
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

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::archetypes
{
class AbodeArchetype
{
public:
	/// underConstruction: the MultiMapFixed ctor 0x52E1E0's flag (PlannedAbode::CreatePlannedNoFixedCheck 0x4057AC
	/// passes 1): +0x58 bit 1, +0x5C = 0, not counted by TownStats, and none of the MakeFunctional parts (storage pit,
	/// creche, graveyard, the town centre's totem and icons) until abodes::Built. The script's abodes are whole (0)
	static entt::entity Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians, float scale,
	                           uint32_t foodAmount, uint32_t woodAmount, bool underConstruction = false);
	/// TownCentre::MakeFunctional 0x743E80's parts: CreateTotemIfNecessary 0x743DA0 (the plinth and its hand, only when
	/// the centre has none yet) and the spell icons (worship::town_centre::MakeFunctional)
	static void MakeTownCentreFunctional(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians,
	                                     float scale);
	AbodeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
