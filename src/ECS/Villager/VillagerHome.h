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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// Going home (VillagerHome.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V2_spec.md §6.6, disassembly
// dev\tmp_dis\aldeanos\core\d_home.txt, d_home2.txt, dev\_scratch\mapa\v2_f.txt). V2 has the walk to the door; the
// arrival (37 ARRIVES_HOME), the stay (38 AT_HOME) and the homeless' tent are V4.

namespace openblack::ecs::villager
{
/// Villager::GoHome 0x760270 = DoGoingHome(37 ARRIVES_HOME, 238 SLEEP_IN_TENT). Always 1
uint32_t GoHome(entt::entity villager);
/// State 36 GO_HOME: GoHome
uint32_t GoHomeState(components::LivingAction& action);
/// Villager::DoGoingHome 0x760280 (arrive, tent): with an abode, inside -> 38; already on the way (GetFinalState ==
/// arrive) -> nothing; else SetupMoveToOnFootpath(abode, its door, arrive). TODO(V4): without an abode (the tent with a
/// town, 0x760310..; VAGRANT_START 130 without one): nothing yet. Always 1
uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, VillagerStates tent);
/// Living::SetupMoveToOnFootpath 0x5EDD20 (object, pos, final): standing on the object's arrive point and going
/// elsewhere -> SetupMoveToWithHug(pos, final); else the object's UseFootpathIfNecessary (vt +0x80)
void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final);
} // namespace openblack::ecs::villager
