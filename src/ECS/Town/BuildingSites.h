/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "ECS/MapCoords.h"
#include "Enums.h"

// The building side of plans and building sites (BuildingSite.cpp, Town.cpp, PlannedAbode of runblack.exe W120; spec
// dev\documentacion\edificios\V6_spec.md §1, §2, §4 and V6_pending.md). A site is an entity with
// components::BuildingSite; a plan is an entry of Town::plannedAbodes (oldest first) named by its index, which is valid
// until the list changes. The villager side (builders V7, wood V9) is the Personas session's: it calls the functions
// below and passes its own data (the ring index villager +0x118, the drop-off point) by argument.

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::plans
{
/// An index into Town::plannedAbodes
using PlanIndex = size_t;

/// The town's plan list Town +0x9A8 / +0x9AC: its size
[[nodiscard]] size_t PlansOf(entt::entity town);
/// Town::AddPlanned 0x73D080: at the tail (oldest first), +0x9AC++; the creation turn (0x6487F6, g_game +0x205A40) is
/// written here. TownStats::Add(plan) 0x749A60 (+0x14, civic +0x24, wonder +0x28): town_stats::Compute counts the list.
/// Returns its index
PlanIndex AddPlanned(entt::entity town, components::PlannedAbode plan);
/// Town::RemovePlanned 0x73D0D0 (PlannedAbode::ToBeDeleted 0x4056B0): out of the list (the later indexes move down)
void RemovePlanned(entt::entity town, PlanIndex plan);
/// PlannedAbode::GetAbodeType 0x4061E0 = info +0x120 (PlannedTownCentre the same). TODO(H3): PlannedTownCitadelHeart
/// 0x467E30 = 0x804
[[nodiscard]] AbodeType GetAbodeType(entt::entity town, PlanIndex plan);
/// IsCivic vt +0x50C: PlannedAbode 0x4060C0 = the type in {0x14, 0x24, 0x44, 0x84, 0x100, 0x204, 0x404, 0x1004,
/// 0x2004} (jump table 0x406118 / 0x406120, town_stats::IsCivic's set); PlannedTownCentre 0x55DBE0 = 1. TODO(H3): the
/// citadel heart's 0x467E10 = 0
[[nodiscard]] bool IsCivic(entt::entity town, PlanIndex plan);
/// GetDesireToBeRepaired vt +0x514 0x648910 = +0x30 (wasBuilt) ? info +0x118 desireToBeRepaired : 0
[[nodiscard]] float GetDesireToBeRepaired(entt::entity town, PlanIndex plan);
/// The plan's GAbodeInfo (+0x40); null when the index or the info is out of range
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity town, PlanIndex plan);

/// Town::GetDesireToBeBuilt(info, n) 0x73A1A0: how much the town wants a building of that info, n scaffolds offered (0
/// from GetBestPlanned). The switch on the ABODE_TYPE and its constants: the .cpp
[[nodiscard]] float GetDesireToBeBuilt(entt::entity town, const GAbodeInfo& info, uint32_t scaffolds);
/// Town::GetBestPlanned(float& best, mask) 0x73A140: best = 0; the plans oldest first whose GetAbodeType & mask; the
/// strictly larger GetDesireToBeBuilt(info, 0) (test ah, 0x41 jne) wins, the first on ties. nullopt: none above 0
[[nodiscard]] std::optional<PlanIndex> GetBestPlanned(entt::entity town, float& best, uint32_t mask);
/// Town::GetPlannedAtPos(pos, r, onlyRebuild) 0x73E4C0: best = r; for each plan (skipped: onlyRebuild && !+0x30)
/// v = GetDistanceInMetres(pos, plan) - (scale x max(mesh +0x24, +0x2C) fn_636E30 + r); v <= best -> best = v, it
/// (ties: the LAST one). With r = 1 a plan is found within R + 2 m
[[nodiscard]] std::optional<PlanIndex> GetPlannedAtPos(entt::entity town, const map_coords::MapCoords& pos, float r,
                                                       bool onlyRebuild);
/// PlannedAbode::CreatePlanned(life) 0x405710 (vt +0x500): GAbodeInfo::IsOkToCreateAtPos 0x404B10 (the fixed check,
/// town_placement) or null; else CreatePlannedNoFixedCheck
entt::entity CreatePlanned(entt::entity town, PlanIndex plan, float life);
/// CreatePlannedNoFixedCheck(life) vt +0x504: PlannedAbode 0x405770 (Abode::Create under construction,
/// PostCreatePlanned 0x648C50, +0x30 -> building +0x58 |= 4, the plan deleted) and PlannedTownCentre 0x744550
/// (TownCentre::Create 0x743C90 directly, without the +0x30 step). The life argument is ignored (underConstruction = 1,
/// V6_spec §2.2). Returns the building or null (the plan survives). TODO(H3): PlannedTownCitadelHeart 0x467EF0
/// (citadel_plan_spec.md §2.2)
entt::entity CreatePlannedNoFixedCheck(entt::entity town, PlanIndex plan, float life);
/// PlannedAbode::Create(Abode*) 0x405660 (from Abode::MoveAbodeToPlannedAbodes 0x40453E): a plan where the building
/// stands, PlannedMultiMapFixed(MultiMapFixed*) 0x648820: its position, GetYAngle, GetScale, info, the creation turn
/// and +0x30 = the building's +0x58 bit 3 (a rebuild plan when it was built). Always a PlannedAbode (a town centre's
/// too). The footpath link +0x64 moves to it (TODO(footpaths)); Init(GetTown) 0x4055A0 -> AddPlanned. nullopt only
/// without the building's info or Transform (the original fails only on allocation)
std::optional<PlanIndex> CreateFromBuilding(entt::entity town, entt::entity building);
} // namespace openblack::ecs::plans

namespace openblack::ecs::building_sites
{
// ---- lifetime ------------------------------------------------------------------------------------------------------

/// MultiMapFixed::CreateBuildingSite 0x52F590 (`new 0x648`, Fixed.cpp line 0x50C) -> BuildingSite(MultiMapFixed*)
/// 0x43B7E0: +0x638 = building +0x58 bit 2; root; building +0x74 = it (fn_52E3F0); +0x640 = GetLife, or 1.1 x life -
/// 0.1 ([0x8AB230], [0x8AB22C]) when the building has no DestructionMesh (+0x90) and IsBuilt; the ring (fn_43CDB0 ->
/// PosBuilder::Process 0x43AE10). Not put in a town: InsertBuildingSite does that
entt::entity Create(entt::entity building);
/// BuildingSite::ToBeDeleted 0x43B960 (vt +0xC), in the original's order (the .cpp). The builders are sent to 163
/// DECIDE_WHAT_TO_DO (SetupMoveToWithHug out of the building, or SetTopState); the pile is released
void ToBeDeleted(entt::entity site);
/// Destroys the entities of the sites deleted since the last call (the GameThing deletion pass). Called once per turn
/// by town_process::ProcessPlayers before the towns
void FlushDeleted();
/// GameThing::IsAvailable 0x401810 of a site: a valid entity with the component and +0xA bit 0 clear
[[nodiscard]] bool IsAvailable(entt::entity site);
/// StandardBuildingSite::Process 0x43D8D0 (vt +0x100, from MultiMapFixed::Process 0x52F700): +0x644 && its IsAvailable
/// != 1 -> +0x644 = 0. TODO(H3): CitadelBuildingSite::Process 0x43D660
void Process(entt::entity site);

// ---- the town list +0x790 / +0x794 -------------------------------------------------------------------------------

/// The town's sites, head first (the newest first)
[[nodiscard]] const std::vector<entt::entity>& SitesOf(entt::entity town);
/// Town::IsBuildingHappening 0x73E2F0: +0x794 != 0 (caller Villager::CheckNeededForBuilding 0x75834A)
[[nodiscard]] bool IsBuildingHappening(entt::entity town);
/// Town::AddBuildingSite(BuildingSite*) 0x73B910: already in the list -> nothing (no pulse); else (TownStats::Add(site)
/// 0x749AA0 when GetTown == this: recomputed by town_stats) a node at the head and the pulse +0x5E8 = 1, +0x5EC = 0
/// (0x73B96D..0x73B977)
void InsertBuildingSite(entt::entity town, entt::entity site);
/// fn_73B990 0x73B990: every node of the site out (TownStats remove fn_749B50: recomputed)
void RemoveBuildingSiteFromList(entt::entity town, entt::entity site);
/// Town::AddBuildingSite(MultiMapFixed*) 0x73B8E0: Create(building), InsertBuildingSite. A site on an existing
/// building (also Abode::ReduceLife 0x405E7A, ProcessTownRepairs 0x747E75, Villager::SetupBuildingObject 0x758565);
/// +0x638 is the building's +0x58 bit 2: a repair site only after ProcessTownRepairs 0x747E6E or a rebuild plan's
/// conversion 0x4057CC (a rock-damaged house's is an ordinary one, repair_spec §5.4)
entt::entity AddBuildingSite(entt::entity town, entt::entity building);
/// Town::AddBuildingSite(PlannedMultiMapFixed*) 0x73B860: plans::CreatePlanned(0.0) (WITH the fixed check), Create,
/// InsertBuildingSite; null when the plan could not be built
entt::entity AddBuildingSiteFromPlan(entt::entity town, plans::PlanIndex plan);
/// Town::AddBuildingSiteNoFixedCheck 0x73B8A0: the same with plans::CreatePlannedNoFixedCheck(0.0)
entt::entity AddBuildingSiteNoFixedCheck(entt::entity town, plans::PlanIndex plan);
/// Town::RemoveBuildingSite(MultiMapFixed*) 0x73BA20: the first site whose GetBuilding is the building ->
/// ToBeDeleted(0), true; else false
bool RemoveBuildingSite(entt::entity town, entt::entity building);
/// Town::GetBuildingSiteInList 0x73CE40: the site whose GetBuilding is the building, or null
[[nodiscard]] entt::entity GetBuildingSiteInList(entt::entity town, entt::entity building);
/// Town::IsBuildingSiteValid 0x73CF00: in the list, GetBuilding != 0 and !(IsBuilt && IsRepaired) (vt +0x890 / +0x88C)
[[nodiscard]] bool IsBuildingSiteValid(entt::entity town, entt::entity site);
/// Town::GetBestBuildingSite(pos, includeFull) 0x73CF60: the head first; score GetDistanceInMetres(pos,
/// building's GetNearestEdgeToPos(pos)) x (GetDesireForVillagers x 0.9 + 0.1) below 99999 (strict); the minimum wins
/// (literal: it favours the sites that need FEWER builders). includeFull is `villager +0xF2 == 4` (BUILDER)
[[nodiscard]] entt::entity GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos, bool includeFull);
/// Town::GetBestRepairBuildingSite 0x747EA0: among the sites with +0x638, the strictly largest GetDesireToBeRepaired
/// above 0 (first on ties). (V11's caller CheckSatisfyToRepair 0x75937E)
[[nodiscard]] entt::entity GetBestRepairBuildingSite(entt::entity town);
/// What Town::ProcessTownRepairs 0x747DE0 picks (0x747DE6..0x747E68): the abode (it wins over a plan) or the plan; both
/// empty: nothing
struct TownRepairChoice
{
	entt::entity abode {entt::null};
	std::optional<plans::PlanIndex> plan;
};
/// The two loops of ProcessTownRepairs sharing one best (0 to start, the strictly larger wins, the first on ties): the
/// plans with +0x30 by GetDesireToBeRepaired (vt +0x514, 0x648910), oldest first; then the abodes of +0x754 without a
/// site (+0x74) and without +0x58 bit 2 by Abode::GetDesireToBeRepaired (vt +0x8D8): an abode must beat the best plan
[[nodiscard]] TownRepairChoice ChooseTownRepair(entt::entity town);
/// Town::ProcessTownRepairs 0x747DE0 (Town::Process step 14, 0x74743D): ChooseTownRepair; an abode -> +0x58 |= 4 and
/// AddBuildingSite(MultiMapFixed*) 0x73B8E0 (a repair site, +0x638 = 1); else a plan -> AddBuildingSiteFromPlan
/// 0x73B860 (with the fixed check). At most one site a town turn
void ProcessTownRepairs(entt::entity town);
/// Town::RequestBestPlanned 0x73A650: GetBestPlanned(mask 4) -> AddBuildingSiteNoFixedCheck; true when a site was made
/// (caller CheckSatisfyCivicBuildings 0x758ECA). The +0x5E4 flag is the caller's (Town::requestedPlanThisTurn)
bool RequestBestPlanned(entt::entity town);
/// Town::RequestANewAbode(type) 0x73B330: GetBestPlanned(mask 2) -> AddBuildingSiteFromPlan (with the fixed check); the
/// type is not used (caller CheckSatisfyAbodesDesire 0x758E6C)
bool RequestANewAbode(entt::entity town, AbodeType unused);
/// fn_73B620 0x73B620 (caller Villager::Building 0x758D71): TownStats +0xEC += u (float; kept in
/// Town::woodUsedForBuilding, as the stats are recomputed). The player's GameStats (player +0xA44 +0xA8) are not ported
void AddWoodUsedForBuilding(entt::entity town, uint32_t wood);
/// Town::ForceBuildingOfPlannedAtPos(pos, v) 0x73E560 (static): every town of every player (GetNextPlayerAndNeutral
/// 0x550980): GetPlannedAtPos(pos, 1.0, 0) -> AddBuildingSiteNoFixedCheck -> site +0x63C = v (CHL BUILD_BUILDING)
void ForceBuildingOfPlannedAtPos(const map_coords::MapCoords& pos, float desire);
/// The building-site pruning fn_43BD00(&town +0x790) 0x43BD00 (Town::Process step 2, 0x747396): each node (next read
/// first): root null, not available, or built and repaired -> the site's ToBeDeleted when it is available
void PruneSites(entt::entity town);

/// What TownDesire reads of the sites and plans (V6_spec §4.3), in the list orders (+0x790 head first, +0x9A8 oldest
/// first): GetDesireForVillagers 0x43BD70 (To_Build 0x748640), +0x634 and fn_43BBD0 (ModificationToBuild 0x746400), the
/// plans' GetDesireToBeRepaired 0x648910 (Repair 0x7486B0)
struct DesireInputs
{
	std::vector<float> siteDesires;
	std::vector<uint32_t> siteBuilders; ///< +0x634 as TownDesire.h keeps it (uint32; the counter is signed here)
	std::vector<int32_t> sitePlaces;
	std::vector<float> planRepairDesires;
};
[[nodiscard]] DesireInputs DesireInputsOf(entt::entity town);

// ---- one site
// --------------------------------------------------------------------------------------------------------

[[nodiscard]] entt::entity GetRootBuilding(entt::entity site); ///< 0x43BCA0 = +0x14
/// BuildingSite::GetBuilding 0x43BC70: +0x14 && its IsAvailable (vt +0x2C) ? GetBuildingObject (vt +0x8BC,
/// MultiMapFixed 0x401520 = this) : 0
[[nodiscard]] entt::entity GetBuilding(entt::entity site);
[[nodiscard]] entt::entity GetTown(entt::entity site); ///< 0x43C0B0 (vt +0x48): root ? root->GetTown() : 0
/// BuildingSite::GetBuildersNeeded 0x43BC00 (signed, may be negative)
[[nodiscard]] int32_t GetBuildersNeeded(entt::entity site);
[[nodiscard]] bool NeedsBuilders(entt::entity site);   ///< fn_43BC60: GetBuildersNeeded > 0
[[nodiscard]] int32_t GetBuilderCount(entt::entity site); ///< +0x634
/// fn_43BBD0 0x43BBD0: GetBuilding ? its info +0x110 MaxVillagerNeededToBuild : 0
[[nodiscard]] int32_t GetMaxBuilders(entt::entity site);
[[nodiscard]] bool IsBuilder(entt::entity site, entt::entity villager); ///< the villager is in +0x18
[[nodiscard]] float GetDesireForVillagers(entt::entity site); ///< 0x43BD70
[[nodiscard]] float GetDesireToBeRepaired(entt::entity site); ///< 0x43BE00
[[nodiscard]] float GetClearAreaRadius(entt::entity site);    ///< 0x43BDE0 (caller SetupBuildingObject 0x7584EE)
[[nodiscard]] float GetPercentBuilt(entt::entity site);       ///< 0x43BCB0
[[nodiscard]] float GetRadius(entt::entity site);             ///< 0x43D050 (vt +0x60), caller 0x759688
[[nodiscard]] float GetWoodValue(entt::entity site);          ///< 0x43C0C0 (caller Building 0x758D3D)
[[nodiscard]] float GetWoodNeededToBuild(entt::entity site);  ///< 0x43C5F0 (caller IsInterestedInWoodObject 0x76501C)
[[nodiscard]] bool IsRepairSite(entt::entity site);           ///< +0x638
[[nodiscard]] float GetRepairBase(entt::entity site);         ///< +0x640
void SetRepairBase(entt::entity site, float base);            ///< +0x640 (Abode::ReduceLife 0x405EA7)
/// BuildingSite::ShouldIGetWood(villager) 0x43C680 (callers 0x75871A, 0x758F94). `resourceDropoffPos` is the
/// villager's GetResourceDropoffPos(WOOD) 0x753E20 (Personas'), asked only where the original asks it (0x43C78E)
[[nodiscard]] bool ShouldIGetWood(entt::entity site, entt::entity villager,
                                  const std::function<map_coords::MapCoords()>& resourceDropoffPos);
/// vt +0x98 GetResource (Standard 0x43C5B0): the pile's JustGetResource(type) (vt +0x94), 0 without one
[[nodiscard]] uint32_t GetResource(entt::entity site, ResourceType type);
[[nodiscard]] uint32_t GetWoodForStats(entt::entity site); ///< vt +0x104 0x43C5E0 = GetResource(WOOD)
/// vt +0x9C AddResource (Standard 0x43C490): WOOD only: the pile (made when missing, CreatePileWood) JustAddResource
/// (vt +0x8C). `pos` is the villager's position (ArrivesAtBuildingSite 0x758BEB); the Standard site ignores it.
/// Returns what was added. TODO(H3): CitadelBuildingSite::AddResource 0x43D360 (pos NULL adds nothing)
uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos,
                     bool poisoned = false);
/// vt +0xA0 RemoveResource (Standard 0x43C530): WOOD only: the pile's JustRemoveResource (vt +0x90). Returns what was
/// removed (Building 0x758D55 passes status 0)
uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount);
/// fn_43D080 0x43D080: GetBuilding()->BuildBy(x) (vt +0x900, abodes::BuildBy 0x52ED40). Caller Building 0x758D62 (x =
/// u / GetWoodValue, the wood value read BEFORE RemoveResource)
void BuildBy(entt::entity site, float amount);
/// GetNearestEdge(out, angle, -, int* idx) 0x43CE40 (vt +0x120): the ring entry for the angle, idx written
[[nodiscard]] map_coords::MapCoords GetNearestEdge(entt::entity site, float angle, int32_t& index);
/// GetRandomBuildPos(out, Object* o, int* idx) 0x43CDE0 (vt +0x128; 1 GameFloatRand); callers GotoBuildingSite
/// 0x758A7F, ReenterBuildingState 0x758FB3
[[nodiscard]] map_coords::MapCoords GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index);
/// GetNextPosFromIndex(out, int* idx) 0x43CF40 (vt +0x124; GameFloatRand then GameRand(2)); caller Building 0x758DC0
[[nodiscard]] map_coords::MapCoords GetNextPosFromIndex(entt::entity site, int32_t& index);
/// The ring entry ArrivesAtBuildingSite 0x758B2B..0x758B6A reads directly: (ftol(x x 6553.6), ftol(z x 6553.6), 0);
/// only for 0 <= index < 128 (0x758B12..0x758B25), else nullopt
[[nodiscard]] std::optional<map_coords::MapCoords> GetBuildPos(entt::entity site, int32_t index);
/// AddBuilder 0x43BE40 (caller EnterBuilding 0x759794): the duplicate search is dead (literal): a node at the head,
/// +0x634++
void AddBuilder(entt::entity site, entt::entity villager);
/// RemoveBuilder 0x43BE90 (caller ExitBuilding 0x7597F7): every node of the villager out, then +0x634-- once (also
/// when it was not there)
void RemoveBuilder(entt::entity site, entt::entity villager);
/// vt +0x108 GetPileWood(pos) (Standard 0x43D6E0 = +0x644, the position ignored)
[[nodiscard]] entt::entity GetPileWood(entt::entity site, const map_coords::MapCoords* pos);
/// The site whose pile this is (PotStructure's IsLinkedToThisBuildingSite 0x43D830 through the pile's structure +0x74,
/// 0x66EF12 / 0x66EE1E / 0x66EDB9), or null
[[nodiscard]] entt::entity SiteOfPile(entt::entity pile);
/// CreatePileWood (Standard) 0x43D760: a "Magic Wood" pot (GPotInfo 0xD4D1C4 = 9) at GetResourcePosAndYAngle(WOOD, -1)
/// when the site is available and has none
void CreatePileWood(entt::entity site);
/// GetResourcePosAndYAngle(out, type, index, float* angle) 0x43C220 (vt +0x114, Standard); `angle` may be null
[[nodiscard]] map_coords::MapCoords GetResourcePosAndYAngle(entt::entity site, ResourceType type, int32_t index,
                                                           float* angle);
} // namespace openblack::ecs::building_sites
