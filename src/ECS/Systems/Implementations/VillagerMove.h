/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

// The villagers' shared move set-up (used by the miracles' villager states: fire, teleport, the teleport test hook).

namespace openblack::ecs::villager
{
/// Living::SetupMoveToWithHug 0x5F2890 (pos, final): the walk (MobileWallHug::SetupMobileMoveToPos: a LINEAR move to
/// `goal`) and SetCurrentAndDestinationState(MOVE_TO_POS, final) 0x5F2980 -> CallEntryStateFunction(c, d) 0x752440:
/// first the current state (TOP = MOVE_TO_POS, which Villager::SetState 0x753690 does by clearing FINAL), then the
/// destination one (FINAL = final). The move state is GLivingInfo +0x124 (MOVE_TO_POS for the villagers).
void SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final);
} // namespace openblack::ecs::villager
