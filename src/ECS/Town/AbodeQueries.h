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

// The abode's queries the idle villagers use (Abode.cpp / MultiMapFixed.cpp of runblack.exe W120; spec
// dev\tmp_dis\aldeanos\V2_spec.md §4.1, disassembly dev\_scratch\mapa\v2_c.txt, v2_d.txt, v2i_door.txt). Positions are
// MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::abode_queries
{
/// GameThing::IsAvailable 0x401810: !(+0xA & 1). (inferido) openblack has no such bit for buildings: a valid entity
[[nodiscard]] bool IsAvailable(entt::entity abode);
/// Abode::IsBuilt 0x4016C0: !(+0x58 & 2) && GetPercentBuilt (+0x5C) >= 1 (abodes::IsBuilt)
[[nodiscard]] bool IsBuilt(entt::entity abode);
/// Abode::IsFunctional 0x406200 (vt +0xD4): MultiMapFixed::IsFunctional 0x52EF70 (IsAvailable, IsBuilt (vt +0x890),
/// GetPercentRepairedForNonFunctional 0x407290 = abode info +0x1B8 thresholdForStopBeingFunctional < GetPercentRepaired
/// 0x401500 = GetLife: fcomp, test ah, 1) == 1 and IsBuilt
[[nodiscard]] bool IsFunctional(entt::entity abode);
/// Abode::GetArrivePos 0x401770 = MultiMapFixed::GetDoorPos 0x52E370: Game3DObject::GetDoorPosition 0x63AFE0 (the world
/// door point x 6553.6, ftol; y 0); when there is none, or its x or its z is 0, the abode's position (+0x14).
/// (aproximado, P-5) the door point of the L3D (extra point 0, HasDoorPosition) through the abode's Transform: the
/// engine's transform (LH3D vt +0x1C4) is not read
[[nodiscard]] glm::ivec2 GetArrivePos(entt::entity abode);
/// Abode::GetPosOutside(p1, p2, p3) 0x4072E0: door + GetPosFromAngle(Get3DAngleFromXZ(pos, door) + GameFloatRand(2 pi
/// / p1) - pi / p1, GameFloatRand(p3) + p2) (Abode.cpp 0x94A then 0x94B)
[[nodiscard]] glm::ivec2 GetPosOutside(entt::entity abode, float p1, float p2, float p3);
} // namespace openblack::ecs::abode_queries
