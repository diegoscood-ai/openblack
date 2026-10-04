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

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/MapCoords.h"
#include "Enums.h"

// The builders (VillagerCivic.cpp of runblack.exe W120, 0x758340..0x75980F; spec dev\documentacion\aldeanos\V7_spec.md,
// disassembly v7\vbuild_0x758340_0x75980F.txt): the way from 163 to a building site (CheckNeededForBuilding,
// SetupBuildingObject), the wood supply (SetupGetBuildingSupplies, DecideHowToGetWood, the storage pit 39), the ring of
// the site (GotoBuildingSite, 40, 184), the build cycle (41) and the builder book-keeping (EnterBuilding /
// ExitBuilding). The building side (sites, ring, pile, BuildBy) is Edificios' ecs::building_sites / ecs::abodes; the
// villager reaches it through BuildingSiteOps so that the tests can put mocks in. Positions are MapCoords x / z
// (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// Building 0x758C97..0x758CD7: f = 1 - a x 0.2f ([0x8AB244]; fsubr [0x8AA390] 1.0, fst), then fe < 1 (test ah, 1) ->
/// 1; else the stored f > 1.2f ([0x8C6C98]; test ah, 0x41) -> 1.2f (0x3F99999A). The game logic runs with the x87 at 24
/// bits (GUtilsDistance.h), so every step is a float step
[[nodiscard]] float BuildFactor(float alignment);
/// Building 0x758CDA..0x758D18: u = ftol(WoodUsedPerBuildCycle (+0x27C, float) x f) (truncation; a float product at 24
/// bits), and pile < u (unsigned, `jae`) -> u = pile
[[nodiscard]] uint32_t BuildAmount(float woodPerCycle, float factor, uint32_t pile);
/// Building 0x758D27..0x758D51: fild qword u; fstp float; fdivr GetWoodValue; fstp float
[[nodiscard]] float BuildStep(uint32_t amount, float woodValue);
/// ArrivesAtBuildingSite 0x758B12..0x758B25: 0 <= index < 0x80 (the ring's 128 entries)
[[nodiscard]] bool BuildPosOk(int32_t index);
/// ArrivesAtBuildingSite 0x758B85..0x758B9A: at the ring point when GetDistanceInMetres <= 0.2 ([0x8AB244]; test ah,
/// 0x41)
[[nodiscard]] bool NearRingPoint(float distance);
/// DecideHowToGetWood 0x75F5B4..0x75F62F: the store's and the forest's weights. Builder mode (1): the store 1 when its
/// stock (u32) is above the capacity (movsx, compared unsigned: `jbe`), else 0; the forest 0.5. Mode 0: frac =
/// (float)(1 - (capacity + 1e-5) / (MaxWoodCarried + 1e-5)) (DropOffFraction), the store frac and the forest
/// (float)(1 - frac)
struct WoodWeights
{
	float store {0.0f};
	float forest {0.0f};
};
[[nodiscard]] WoodWeights WoodSourceWeights(uint32_t stock, int16_t capacity, uint32_t maxWoodCarried, bool forBuilding);
/// DecideHowToGetWood 0x75F63C..0x75F6EC: store = GDM(d store, D) x storeW (stored); forest = a forest ? GDM(d forest,
/// D) x forestW : 0; store > forest (`test ah, 0x41; jne`) -> 1; forest != 0 with a forest -> 2 (a BigForest) / 3; else
/// 0. `isBigForest` is the forest's +0x38
struct WoodChoice
{
	uint32_t how {0};
	float store {0.0f};
	float forest {0.0f};
};
[[nodiscard]] WoodChoice ChooseWoodSource(WoodWeights weights, float storeDistance, std::optional<float> forestDistance,
                                          bool isBigForest, float maxDistance);

// ---- the decision path -------------------------------------------------------------------------------------------

/// Villager::CheckNeededForBuilding 0x758340: IsBuildingHappening; GetBestBuildingSite(me, disciple == BUILDER) ->
/// SetupBuildingObject == 1 -> 1; else 0
uint32_t CheckNeededForBuilding(entt::entity villager);
/// Villager::SetupBuildingObject(BuildingSite*) 0x7584B0: the site's building, not built-and-repaired;
/// CheckForClearArea (0 in W120) else SetupGetBuildingSupplies. Also Milagros' CheckNeededForWorshipSiteBuilding
/// 0x76C98A
uint32_t SetupBuildingObject(entt::entity villager, entt::entity site);
/// Villager::SetupBuildingObject(MultiMapFixed*) 0x758530 (ArrivesHome 0x760AB3, CheckInteractWithAbode 0x7574C1, ...):
/// the building's site (GetBuildingSiteInList, else AddBuildingSite 0x73B8E0: a repair site) and
/// SetupBuildingObject(site)
uint32_t SetupBuildingObjectForBuilding(entt::entity villager, entt::entity building);
/// Villager::CheckForClearArea(pos, radius) 0x7590A0: the pushable objects in the building's clear area. Always 0 in
/// W120 (every IsPushable vt +0x814 is Object::IsPushable 0x402AE0 = 0; V7_spec §1.2)
uint32_t CheckForClearArea(entt::entity villager, glm::ivec2 pos, float radius);

// ---- the wood supply ---------------------------------------------------------------------------------------------

/// Villager::SetupGetBuildingSupplies(site) 0x7586E0: ShouldIGetWood (Edificios) false -> GotoBuildingSite; else
/// DecideHowToGetWood(1): 1 -> GotoStoragePitForBuildingMaterials; 2 / 3 (the forests) TODO(V9): 0
uint32_t SetupGetBuildingSupplies(entt::entity villager, entt::entity site);
/// Villager::DecideHowToGetWood(mode, &bigForest, &forest) 0x75F510: 0 nothing, 1 the store (storage pit, home or the
/// town's temporary wood pot, made if missing), 2 a BigForest, 3 a forest
struct WoodSource
{
	uint32_t how {0};
	std::optional<uint32_t> forest;    ///< the forest (ecs::Trees id) of 3
	entt::entity bigForest {entt::null}; ///< the BigForest of 2
	float store {0.0f};                ///< the scores (trace)
	float forestScore {0.0f};
};
WoodSource DecideHowToGetWood(entt::entity villager, bool forBuilding);
/// Villager::GotoStoragePitForBuildingMaterials(site) 0x7587D0: full -> GotoBuildingSite; the pit's wood edge (or
/// GetResourceDropoffPos(WOOD)); not a builder: no room and not a BUILDER disciple -> 0, else +0xFC = site unless the
/// TOP's exit is ExitBuilding; the walk with FINAL 39 (footpath with a pit / home, else wall-hug)
uint32_t GotoStoragePitForBuildingMaterials(entt::entity villager, entt::entity site);

// ---- the site ----------------------------------------------------------------------------------------------------

/// Villager::GotoBuildingSite(site) 0x758A00: SetTopState(163), +0xFC = site, GetRandomBuildPos, the walk with FINAL 40
/// (footpath beyond 40 m)
uint32_t GotoBuildingSite(entt::entity villager, entt::entity site);

/// Villager::EnterBuilding(final, next) 0x759750 (entry of 39, 40, 41, 184): +0xFC not a valid site -> 0 (refused);
/// another entry function before -> AddBuilder; 1
uint32_t EnterBuilding(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Villager::ExitBuilding(next) 0x7597B0: the same exit -> 1; else RemoveBuilder (a valid site) and +0xFC = 0; 1
uint32_t ExitBuilding(components::LivingAction& action, VillagerStates next);

/// State 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS: Villager::ArrivesAtStoragePitForBuildingMaterials 0x758990
/// (clip 340 P_PICK_UP_STICKS)
uint32_t ArrivesAtStoragePitForBuildingMaterials(components::LivingAction& action);
/// State 40 ARRIVES_AT_BUILDING_SITE: Villager::ArrivesAtBuildingSite 0x758AF0 (clip 348 P_PUT_DOWN_STICKS)
uint32_t ArrivesAtBuildingSite(components::LivingAction& action);
/// State 41 BUILDING: Villager::Building 0x758C40 (clip BuildingAnimation 0x423E20)
uint32_t BuildingState(components::LivingAction& action);
/// State 184 REENTER_BUILDING_STATE: Villager::ReenterBuildingState 0x758F60
uint32_t ReenterBuildingState(components::LivingAction& action);

// ---- the building side (Edificios) and the test hooks ------------------------------------------------------------

/// What the builders call of the building side, one function each: by default Edificios' ecs::building_sites /
/// ecs::abodes (and ecs::object::IsTouching, ecs::effects::alignment::LandAlignmentAt), or a test's mocks
struct BuildingSiteOps
{
	std::function<bool(entt::entity town)> isBuildingHappening;                                  ///< 0x73E2F0
	std::function<entt::entity(entt::entity town, const map_coords::MapCoords& pos, bool includeFull)>
	    getBestBuildingSite;                                                                      ///< 0x73CF60
	std::function<entt::entity(entt::entity town)> getBestRepairBuildingSite;                    ///< 0x747EA0
	std::function<bool(entt::entity town, entt::entity site)> isBuildingSiteValid;               ///< 0x73CF00
	std::function<entt::entity(entt::entity town, entt::entity building)> getBuildingSiteInList; ///< 0x73CE40
	std::function<entt::entity(entt::entity town, entt::entity building)> addBuildingSite;       ///< 0x73B8E0
	std::function<bool(entt::entity town)> requestBestPlanned;                                   ///< 0x73A650
	std::function<bool(entt::entity town)> requestANewAbode;                                     ///< 0x73B330 (push 2)
	std::function<void(entt::entity town, uint32_t wood)> addWoodUsedForBuilding;                ///< fn_0073B620
	std::function<entt::entity(entt::entity site)> getBuilding;                                  ///< 0x43BC70
	std::function<bool(entt::entity site)> needsBuilders;                                        ///< fn_0043BC60
	std::function<bool(entt::entity site, entt::entity villager)> isBuilder;                     ///< the +0x18 walk
	std::function<int32_t(entt::entity site)> getBuilderCount;                                   ///< +0x634 (trace)
	std::function<float(entt::entity site)> getClearAreaRadius;                                  ///< 0x43BDE0
	std::function<float(entt::entity site)> getWoodValue;                                        ///< 0x43C0C0
	std::function<bool(entt::entity site, entt::entity villager,
	                   const std::function<map_coords::MapCoords()>& resourceDropoffPos)>
	    shouldIGetWood;                                                                           ///< 0x43C680
	std::function<uint32_t(entt::entity site, ResourceType type)> getResource;                   ///< vt +0x98
	std::function<uint32_t(entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos)>
	    addResource;                                                                              ///< vt +0x9C
	std::function<uint32_t(entt::entity site, ResourceType type, uint32_t amount)> removeResource; ///< vt +0xA0
	std::function<void(entt::entity site, float amount)> buildBy;                                ///< fn_0043D080
	std::function<bool(entt::entity site)> isAvailable;                                          ///< vt +0x2C 0x401810
	std::function<map_coords::MapCoords(entt::entity site, entt::entity villager, int32_t& index)>
	    getRandomBuildPos;                                                                        ///< vt +0x128
	std::function<map_coords::MapCoords(entt::entity site, int32_t& index)> getNextPosFromIndex;  ///< vt +0x124
	std::function<std::optional<map_coords::MapCoords>(entt::entity site, int32_t index)> getBuildPos; ///< +0x34 ring
	std::function<void(entt::entity site, entt::entity villager)> addBuilder;                    ///< 0x43BE40
	std::function<void(entt::entity site, entt::entity villager)> removeBuilder;                 ///< 0x43BE90
	std::function<bool(entt::entity building)> isBuilt;                                          ///< vt +0x890
	std::function<bool(entt::entity building)> isRepaired;                                       ///< vt +0x88C
	std::function<bool(entt::entity villager, entt::entity building, float margin)> isTouching;   ///< vt +0x6B8
	std::function<float(const glm::vec3& position)> landAlignmentAt;                             ///< 0x6057B0
};
/// The defaults (Edificios' functions)
[[nodiscard]] BuildingSiteOps DefaultBuildingSiteOps();
/// The tests: mocks instead of the defaults (nullopt: back to them)
void SetBuildingSiteOpsForTests(std::optional<BuildingSiteOps> ops);
} // namespace openblack::ecs::villager
