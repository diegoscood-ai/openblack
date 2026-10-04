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

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/MapCoords.h"
#include "Enums.h"

// FishFarm.cpp of runblack.exe W120 (FishFarm : MultiMapFixed; spec dev\documentacion\edificios\fields_features_spec.md
// §3): the fishermen (+0x80 / +0x84), the food stock's takers and what the villager side (Personas, V8_V9_spec §5)
// calls. The shoal, its fish and FishFarm::Process 0x52D130 are in FishShoals.{h,cpp}. The fish puzzle's shoal-only
// FishFarm entities (FishPuzzle.cpp) have no town: TownFishFarms never gives them.

namespace openblack::ecs::fish_farms
{
/// fn_0052D250 AddFisherman: a null villager -> nothing; else at the head (+0x84++), no duplicate test (EnterFishing
/// 0x75B85E tests first: HasFisherman), and the villager's TargetThing (+0x118) = the farm (0x52D281;
/// TODO(Personas): villager::SetTargetThing). No maximum (maxNoFishermanPerFishFarm is only in Score)
void AddFisherman(entt::entity farm, entt::entity villager);
/// FishFarm::RemoveFisherman 0x52D290: every node of the villager out (+0x84-- each); TargetThing NOT written
void RemoveFisherman(entt::entity farm, entt::entity villager);
/// Is the villager in the list (EnterFishing's test 0x75B85E)
[[nodiscard]] bool HasFisherman(entt::entity farm, entt::entity villager);
/// +0x84 (Fishing 0x75B6CB's GameRand(n))
[[nodiscard]] uint32_t FishermanCount(entt::entity farm);
/// fn_0052D2F0: ftol(fn_0052D240 (1.0) x (1 - min((float)((u64)+0x84 / (int)maxNoFishermanPerFishFarm), 1))): 1 with
/// no fisherman, 0 with 1..4 (the ftol's int)
[[nodiscard]] int32_t Score(entt::entity farm);
/// fn_0052C870 FishingSpot: r = Get2DRadius (5), h = r / 2; x + GameFloatRand(r) - h ("FishFarm.cpp" 0xB8), then
/// z + GameFloatRand(r) - h (0xB9), each axis ftol((pos x 10 / 65536 + d) x 65536 / 10); the altitude copied
[[nodiscard]] map_coords::MapCoords FishingSpot(entt::entity farm);
/// FishFarm::GetArrivePos 0x52C490 (vt +0x104): the position +0x14
[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity farm);
/// fn_0052CED0 RemoveFood(n): (float)n <= food (or unordered) -> food -= n, n; else ftol(food) and food = 0
int32_t RemoveFood(entt::entity farm, int32_t amount);
/// FishFarm::RemoveResource 0x52CF20 (vt +0xA0): FOOD -> RemoveFood(n); any other type 0
int32_t RemoveResource(entt::entity farm, ResourceType type, int32_t amount);
/// FishFarm::GetTown 0x52C450: +0x8C (FishFarm::town), entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity farm);
/// Town +0x788: the town's fish farms, newest first (head insertion in the ctor 0x52C407..0x52C418): the FishFarm
/// entities with that town, by creation index from high to low. A null town gives none
[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town);
/// "in g_game +0x205C0C": a valid entity with a FishFarm
[[nodiscard]] bool IsFishFarm(entt::entity thing);
/// (openblack) disconnects the on_destroy<FishFarm> listener (fields::DisconnectDeletionListeners, before a Reset)
void DisconnectDeletionListener();
/// FishFarm::DeleteDependancys 0x52C6B0 (FishFarm::ToBeDeleted 0x52C693; run by an on_destroy<FishFarm> listener that
/// AddFisherman connects, so ecs::ToBeDeleted / Registry::Destroy reach it): while the list is not empty, the head's
/// SetTopState(163) (vt +0x8E8, fields::ReleaseWorker) relying on ExitFishing 0x75B880 to unlink it
/// (0x52C6B3..0x52C6D7); (openblack guard) a pass that does not shrink the list ends the loop (no hook registered, or
/// the villager side did not unlink it). The town list +0x788 and the global list need nothing (the components);
/// RemoveMapObject is ecs::ToBeDeleted's generic part
void DeleteDependancys(entt::entity farm);
} // namespace openblack::ecs::fish_farms
