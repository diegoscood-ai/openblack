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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/MapCoords.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// What a villager carries, takes from and leaves in the structures (Villager.cpp / VillagerStates.cpp of runblack.exe
// W120; spec dev\tmp_dis\aldeanos\V4_spec.md §5.4..§5.5 and V5_spec.md, disassembly dev\tmp_dis\aldeanos\v4\misc.txt,
// misc3.txt, food.txt, v5\carry.txt, dropoff.txt, dropped.txt, speed.txt, extra.txt). V4 has the food half (eating
// from home and from the storage pit, P-1); V5 the carrying: the pick-ups and drops, the capacities, the carried object,
// the drop-off states 31 / 32, the temporary pots (ecs::town_stores) and the load's speed factors. Positions are
// MapCoords x / z (ecs::town_queries). The pure layer has the arithmetic for the tests.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// Villager::GetResourceHeld 0x751570's rule: food > wood (unsigned 16-bit, `jbe`) -> FOOD and the food; else wood != 0
/// -> WOOD and the wood; else None (-1) and 0 (so equal non-zero loads give WOOD)
struct HeldResource
{
	ResourceType type {ResourceType::None};
	uint16_t amount {0};
};
[[nodiscard]] HeldResource HeldLarger(int16_t food, int16_t wood);
/// Villager::GetWoodCarriedObject 0x7502A0: the tree type of +0xE0 bits 14-15: 1 -> 13 TREE_1, 2 -> 14 TREE_2, 3 -> 15
/// TREE_3, 0 -> 12 WOOD
[[nodiscard]] int32_t WoodCarriedObject(uint16_t flags);
/// The exit of the state's row (0xD091B8 + 0x90 x s, slot +0x20) is Villager::ExitBuilding 0x7597B0
/// (k_OriginalStateFns): FUN_00753140 0x753140 for the final state s
[[nodiscard]] bool IsBuildingExitState(VillagerStates state);
/// What Villager::SetStateCarriedObject 0x7501A0 reads
struct CarriedInput
{
	VillagerStates finalState {VillagerStates::InvalidState}; ///< GetFinalState (vt +0xB04)
	VillagerStates topState {VillagerStates::InvalidState};   ///< +0x8C
	float life {1.0f};                                        ///< GetLife (vt +0x11C)
	int16_t wood {0};                                         ///< +0xF6
	int16_t food {0};                                         ///< +0xF4
	uint16_t flags {0};                                       ///< +0xE0 (the tree type, bits 14-15)
	bool finalIsBuilding {false};                             ///< FUN_00753140 0x753140
	int32_t rowCarriedFinal {0};                              ///< Infos[final] +0xEC (0xDB9F54 + 0x114 x s), 0 = none
	int32_t rowCarriedTop {0};                                ///< Infos[top] +0xEC
	int32_t previous {1};                                     ///< +0xF1 before the call
	float lifeWhenCrawlsWounded {0.15f};                      ///< info +0x380
	uint32_t minWoodToShowGraphic {50};                       ///< info +0x26C
	uint32_t minFoodToShowGraphic {100};                      ///< info +0x270
};
/// Villager::SetStateCarriedObject 0x7501A0: the new +0xF1 (CARRIED_OBJECT)
[[nodiscard]] int32_t CarriedObjectFor(const CarriedInput& in);
/// SetStateSpeed 0x753A06..0x753AF7: the load factors, clamp(1 + SpeedModWhenFullLoad - held / Max, 0.75, 1); the wood
/// one is stored as a float (0x753A81), the food one stays on the x87 stack (approximate: double)
struct LoadFactor
{
	float wood {1.0f};
	double food {1.0};
};
[[nodiscard]] LoadFactor LoadFactors(int16_t wood, int16_t food, const GVillagerInfo& info, bool trader);
/// SetStateSpeed 0x7539AE..0x7539F4: base (+0x36C) + clamp(S / divisor (+0x370), 0, 0.5), S = town_desire::TownNeedsSum
/// (on the x87 stack: double)
[[nodiscard]] double TownNeedsFactor(double townNeedsSum, const GVillagerInfo& info);
/// CheckSatisfyFoodDesire 0x75A010..0x75A07C: frac = (float)(1 - (capacity + 1e-5) / (MaxFoodCarried + 1e-5)) (stored);
/// GetDistanceModifier(distance, 500) x frac (on the x87 stack). `held` is +0xF4 (capacity = (int16)(max - held))
[[nodiscard]] float DropOffFraction(int16_t capacity, uint32_t maxFoodCarried);
[[nodiscard]] double DropOffScore(int16_t held, uint32_t maxFoodCarried, float distance);
/// CreateDroppedResource 0x750940's test and the log's wood multiplier (+0x9C, 0x7509C6..0x7509E7: (float)(wood /
/// log.GetWoodValue)); nullopt when nothing is dropped (c0 <= 1 or >= 16, or wood <= MinWoodToShowGraphic)
struct DroppedLog
{
	int32_t carriedObject {1}; ///< c0 = +0xF1: the mesh is CarriedObject 0xC5E19C[c0] (CarriedProps k_PropMeshes)
	float multiplier {0.0f};
};
[[nodiscard]] std::optional<DroppedLog> DroppedLogFor(int32_t carriedObject, int16_t wood, uint32_t minWoodToShowGraphic,
                                                      float logWoodValue);
/// DeadTree::GetDefaultResource 0x511330: ftol((qword)woodValue x +0x9C x scale) (x87)
[[nodiscard]] int32_t DroppedLogValue(uint32_t woodValue, float multiplier, float scale);

// ---- carrying ----------------------------------------------------------------------------------------------------

/// Villager::PickupResource 0x7513F0 (type, n, tree): FOOD: +0xF4 += n (a 16-bit add), Town +0x708 += n; any other type
/// (`jne`, also -2): +0xF6 += n, Town +0x70C += n and +0xE0 bits 14-15 = tree & 3. Returns n
int16_t PickupResource(entt::entity villager, ResourceType type, int16_t amount, uint8_t treeType);
/// Villager::PickupFood 0x751490: PickupResource(FOOD, n, 0)
void PickupFood(entt::entity villager, int16_t amount);
/// Villager::PickupWood 0x7514B0: PickupResource(WOOD, n, tree)
void PickupWood(entt::entity villager, int16_t amount, uint8_t treeType);
/// Villager::DropFood 0x7511E0 (n): n == 0 or n above what it carries (unsigned 16-bit compare) -> all of it;
/// +0xF4 -= n, Town +0x708 -= n. The food is gone (no pot is made). Returns n
uint16_t DropFood(entt::entity villager, uint16_t amount);
/// Villager::DropWood 0x751240: DropFood on +0xF6 and Town +0x70C. The tree bits stay (literal). Returns n
uint16_t DropWood(entt::entity villager, uint16_t amount);
/// Villager::DropResource 0x7511B0 (type, n): WOOD -> DropWood, FOOD -> DropFood, else 0
uint16_t DropResource(entt::entity villager, ResourceType type, uint16_t amount);
/// Villager::GetFoodCapacity 0x7514D0 / GetWoodCapacity 0x7514F0: (int16)(info.MaxFoodCarried (+0x264) / MaxWoodCarried
/// (+0x268) - held); negative above the maximum
[[nodiscard]] int16_t GetFoodCapacity(entt::entity villager);
[[nodiscard]] int16_t GetWoodCapacity(entt::entity villager);
/// Villager::GetResourceHeld 0x751570 (&type): HeldLarger of +0xF4 / +0xF6
[[nodiscard]] uint16_t GetResourceHeld(entt::entity villager, ResourceType& type);
/// Villager::AddResource 0x7564D0 (vt +0x9C of a villager, for object_resources::AddResource): FOOD -> PickupFood(n) and,
/// poisoned, SetPoisoned(1) (ecs::life::TakePoisonedResource); WOOD -> PickupWood(n, 0); other types nothing. Returns 0
/// always (literal)
uint32_t AddResourceToVillager(entt::entity villager, ResourceType type, uint32_t amount, bool poisoned);
/// Villager::GetResourceFrom 0x753390 (object, type, n): c = object.RemoveResource(type, n) (vt +0xA0,
/// object_resources::RemoveResource); c != 0 -> PickupResource(type, c, object.GetCarriedTreeType() (Object 0x402AF0:
/// 0)); object.IsSpeedUp (GameThingWithPos 0x402410: 0) -> SetFoodSpeedup; object.IsPoisoned -> SetPoisoned(1)
/// (ecs::life::TakePoisonedResource). Returns c
uint16_t GetResourceFrom(entt::entity villager, entt::entity object, ResourceType type, int16_t amount);

// ---- the carried object ------------------------------------------------------------------------------------------

/// Villager::GetWoodCarriedObject 0x7502A0 of the villager's +0xE0
[[nodiscard]] int32_t GetWoodCarriedObject(entt::entity villager);
/// FUN_00753140 0x753140: GetFinalState's exit is ExitBuilding 0x7597B0
[[nodiscard]] bool FinalStateIsBuilding(entt::entity villager);
/// Villager::SetStateCarriedObject 0x7501A0 (from GetAnimId 0x750133): the CARRIED_OBJECT +0xF1 it would write, from
/// `previous` (the current +0xF1, kept with the final state 4 IN_SCRIPT or TOP 200 SCRIPT_PLAY_ANIM). The caller stores
/// it (SkeletalAnimation::carriedObject)
[[nodiscard]] int32_t SetStateCarriedObject(entt::entity villager, int32_t previous);

// ---- where: the storage pit, the home and the temporary pot ------------------------------------------------------

/// Villager::GetStoragePit 0x751F10: the town's (Town::GetStoragePit 0x73B5B0) or, without one, the villager's abode
[[nodiscard]] entt::entity GetStoragePit(entt::entity villager);
/// Villager::GetResourceDropoffPos 0x753E20 (type): GetStoragePit functional -> its GetArrivePos; else the town's
/// storage pit functional -> its GetArrivePos; else with a town Town::GetTemporaryResourceStorePotOrPos 0x73E900's
/// point (the town's temporary pot of that type is made if it has none: a side effect, literal); without a town the
/// villager's position
[[nodiscard]] glm::ivec2 GetResourceDropoffPos(entt::entity villager, ResourceType type);
/// MultiMapFixed::GetResourceNearestEdge (vt +0x8D4): StoragePit 0x733400 = its GetArrivePos; MultiMapFixed 0x401590 ->
/// GetResourcePos 0x401560 = its position (+0x14)
[[nodiscard]] glm::ivec2 GetResourceNearestEdge(entt::entity object, ResourceType type, entt::entity villager);
/// Villager::AtStructureRemoveResource 0x76A2F0 (object, type, n): pos = GetResourceNearestEdge; IsCloseToEqual(me, pos,
/// GetRadius) (distance <= my 2D radius) -> c = GetResourceFrom(object, type, n): 0 -> 0, c < n -> 0x24, else 1; not
/// there -> SetupMoveToWithHug(pos, GetFinalState), 0x24
uint32_t AtStructureRemoveResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t amount);
/// Villager::AtStructureAddResource 0x76A3B0 (object, type, &n, _): edge = GetResourceNearestEdge; IsCloseToEqual(me,
/// edge, GetSpeedInMetres) -> added = object.AddResource(type, n) (object_resources::AddResource): 0 -> 0, else
/// DropResource(type, added), n = added, 1; not there -> SetupMoveToWithHug(edge, GetFinalState), n = 0, 0x24
uint32_t AtStructureAddResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t& amount);
/// Villager::ArrivesAtStoragePitForResource 0x7698D0 (type, n, ok, fail)
uint32_t ArrivesAtStoragePitForResource(entt::entity villager, ResourceType type, uint32_t amount, VillagerStates ok,
                                        VillagerStates fail);

// ---- dropping off (states 31 / 32) -------------------------------------------------------------------------------

/// Villager::GotoStoragePitForDropOff 0x769620 (also called as a function by CheckSatisfyFoodDesire 0x75A094): the pit
/// (or home) functional -> SetupMoveToOnFootpath(pit, arrive, 32), 1; holding nothing -> SetTopState(163), 0; else
/// SetupMoveToWithHug(GetResourceDropoffPos(type), 32), 1
uint32_t GotoStoragePitForDropOff(entt::entity villager);
/// 31 GOTO_STORAGE_PIT_FOR_DROP_OFF (the state table's adapter of GotoStoragePitForDropOff)
uint32_t GotoStoragePitForDropOffState(components::LivingAction& action);
/// 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF: Villager::ArrivesAtStoragePitForDropOff 0x7696D0 (clip 347 P_PUT_DOWN_BAG)
uint32_t ArrivesAtStoragePitForDropOff(components::LivingAction& action);

// ---- dropped resources -------------------------------------------------------------------------------------------

/// Villager::CreateDroppedResource 0x750940 (a, b, c): the carried wood falls as a DeadTree log (Pine's GTreeInfo
/// 0xDA49D8, the carried object's mesh, the multiplier wood / GetWoodValue) put into physics with the velocity a (none:
/// 0, and b ignored), the angular b and the +0x90 vector c; then DropWood(0). TODO(arboles/Fisicas): the log is not
/// made (no DeadTree::Create 0x510BB0 / InitialisePhysics API yet): (approximate) the wood is lost
void CreateDroppedResource(entt::entity villager, std::optional<glm::vec3> velocity, std::optional<glm::vec3> angular,
                           std::optional<glm::vec3> extra);

// ---- test hooks --------------------------------------------------------------------------------------------------

using TemporaryStoreFn =
    std::function<town_stores::TemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)>;
/// The tests: Town::GetTemporaryResourceStorePotOrPos instead of ecs::town_stores' (empty: town_stores', which makes a
/// pile with PotArchetype)
void SetTemporaryStoreForTests(TemporaryStoreFn store);
/// The tests: what CreateDroppedResource does with the log (position, mesh, multiplier, velocity, angular, +0x90)
/// instead of ecs::CreateDroppedLog + PhysicsObjects::AddDroppedObject (empty: those)
using DroppedLogFn = std::function<void(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity,
                                        glm::vec3 angular, std::optional<glm::vec3> momentum)>;
void SetDroppedLogForTests(DroppedLogFn make);
} // namespace openblack::ecs::villager
