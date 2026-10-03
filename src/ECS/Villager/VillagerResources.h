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

#include "Enums.h"

// What a villager carries and takes from the structures (Villager.cpp / VillagerStates.cpp of runblack.exe W120;
// spec dev\tmp_dis\aldeanos\V4_spec.md §5.4..§5.5, disassembly dev\tmp_dis\aldeanos\v4\misc.txt, misc3.txt, food.txt).
// V4 has the food half (eating from home and from the storage pit, P-1); the carrying states 31 / 32, the capacities and
// the temporary pots are V5. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
/// Villager::PickupResource 0x7513F0 (type, n, tree): FOOD: +0xF4 += n (a 16-bit add), Town +0x708 += n; WOOD: +0xF6 +=
/// n, Town +0x70C += n and +0xE0 bits 14-15 = tree & 3. Returns n
int16_t PickupResource(entt::entity villager, ResourceType type, int16_t amount, uint8_t treeType);
/// Villager::PickupFood 0x751490: PickupResource(FOOD, n, 0)
void PickupFood(entt::entity villager, int16_t amount);
/// Villager::DropFood 0x7511E0 (n): n == 0 or n above what it carries (unsigned 16-bit compare) -> all of it;
/// +0xF4 -= n, Town +0x708 -= n. The food is gone (no pot is made). Returns n
uint16_t DropFood(entt::entity villager, uint16_t amount);
/// Villager::GetResourceFrom 0x753390 (object, type, n): c = object.RemoveResource(type, n) (vt +0xA0,
/// object_resources::RemoveResource); c != 0 -> PickupResource(type, c, object.GetCarriedTreeType() (Object 0x402AF0:
/// 0)); object.IsSpeedUp (GameThingWithPos 0x402410: 0) -> SetFoodSpeedup; object.IsPoisoned -> SetPoisoned(1)
/// (ecs::life::TakePoisonedResource). Returns c
uint16_t GetResourceFrom(entt::entity villager, entt::entity object, ResourceType type, int16_t amount);
/// Villager::GetStoragePit 0x751F10: the town's (Town::GetStoragePit 0x73B5B0) or, without one, the villager's abode
[[nodiscard]] entt::entity GetStoragePit(entt::entity villager);
/// Villager::GetResourceDropoffPos 0x753E20 (type): GetStoragePit functional -> its GetArrivePos; else the town's
/// storage pit functional -> its GetArrivePos; else with a town Town::GetTemporaryResourceStorePotOrPos 0x73E900
/// (TODO(V5): the temporary pot; (aproximado hasta V5) the villager's own position, so the callers see "already there");
/// without a town the villager's position
[[nodiscard]] glm::ivec2 GetResourceDropoffPos(entt::entity villager, ResourceType type);
/// MultiMapFixed::GetResourceNearestEdge (vt +0x8D4): StoragePit 0x733400 = its GetArrivePos; MultiMapFixed 0x401590 ->
/// GetResourcePos 0x401560 = its position (+0x14)
[[nodiscard]] glm::ivec2 GetResourceNearestEdge(entt::entity object, ResourceType type, entt::entity villager);
/// Villager::AtStructureRemoveResource 0x76A2F0 (object, type, n): pos = GetResourceNearestEdge; IsCloseToEqual(me, pos,
/// GetRadius) (distance <= my 2D radius) -> c = GetResourceFrom(object, type, n): 0 -> 0, c < n -> 0x24, else 1; not
/// there -> SetupMoveToWithHug(pos, GetFinalState), 0x24
uint32_t AtStructureRemoveResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t amount);
/// Villager::ArrivesAtStoragePitForResource 0x7698D0 (type, n, ok, fail)
uint32_t ArrivesAtStoragePitForResource(entt::entity villager, ResourceType type, uint32_t amount, VillagerStates ok,
                                        VillagerStates fail);
} // namespace openblack::ecs::villager
