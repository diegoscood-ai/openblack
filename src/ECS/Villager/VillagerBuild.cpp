/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBuild.h"

#include <string>
#include <utility>

#include <fmt/format.h>

#include "ECS/Abodes.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

// VillagerCivic.cpp of runblack.exe W120 (VillagerBuild.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// VillagerDisciple BUILDER (+0xF2 == 4: 0x758353, 0x75933C, 0x7588C3, 0x758A4C)
constexpr uint8_t k_DiscipleBuilder = 4;
/// [0x8AB244] = 0x3E4CCCCD (0.2): the alignment's weight (Building 0x758C97) and the ring point's reach
/// (ArrivesAtBuildingSite 0x758B85)
constexpr float k_PointTwo = 0.2f;
/// [0x8C6C98] = 0x3F99999A (1.2): the build factor's cap (0x758CC2, 0x758CCF)
constexpr float k_MaxBuildFactor = 1.2f;
/// [0xC23704] = 40.0: beyond it the walk to the ring point takes the footpath (GotoBuildingSite 0x758A9B,
/// ReenterBuildingState 0x759002)
constexpr float k_FootpathDistance = 40.0f;
/// 0x3A83126F (0.001): ReenterBuildingState's IsTouching margin (0x758FCF)
constexpr float k_TouchingMargin = 0.001f;
/// 0x80: the ring's entries (ArrivesAtBuildingSite `cmp eax, 0x80` 0x758B20)
constexpr int32_t k_RingSize = 0x80;
/// LookAtObject's mode (ArrivesAtBuildingSite `push 1` 0x758BBA)
constexpr uint32_t k_LookMode = 1;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

/// Villager::GetTown (vt +0x48): a valid town entity with its component, or null
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->town == entt::null || !Entities().Valid(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

bool IsBuilderDisciple(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->discipleType == k_DiscipleBuilder;
}

/// The villager's +0x14 as a MapCoords (x / z, altitude 0)
map_coords::MapCoords MeCoords(entt::entity villager)
{
	const auto me = tq::PosOf(villager);
	return {me.x, me.y, 0.0f};
}

/// The villager's +0x14 as a world point (MapCoords::GetAlignment's, the forest finders')
glm::vec3 PositionOf(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

glm::ivec2 Xz(const map_coords::MapCoords& pos)
{
	return {pos.x, pos.z};
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

std::optional<BuildingSiteOps>& OpsOverride()
{
	static std::optional<BuildingSiteOps> s_Ops;
	return s_Ops;
}

const BuildingSiteOps& Ops()
{
	static const BuildingSiteOps k_Default = DefaultBuildingSiteOps();
	const auto& ops = OpsOverride();
	return ops ? *ops : k_Default;
}

/// The walk to a ring point (GotoBuildingSite 0x758A8E..0x758ACD, ReenterBuildingState 0x758FF5..0x759032): beyond 40 m
/// (fcomp; test ah, 0x41: strictly) SetupMoveToOnFootpath(GetBuilding(site), pos, 40), else SetupMoveToWithHug(pos, 40)
void WalkToRingPoint(entt::entity villager, entt::entity site, glm::ivec2 pos, const char* who)
{
	const float d = tq::GetDistanceInMetres(tq::PosOf(villager), pos);
	const bool footpath = d > k_FootpathDistance;
	if (TraceOn(villager))
	{
		const auto* v = VillagerOf(villager);
		Trace(villager, fmt::format("build {}: site {} ring {} ({}, {}) dist {:.3f} footpath {}", who,
		                            static_cast<uint32_t>(site), v != nullptr ? v->buildPosIndex : -1, pos.x, pos.y, d,
		                            footpath ? 1 : 0));
	}
	if (footpath)
	{
		SetupMoveToOnFootpath(villager, Ops().getBuilding(site), pos, VillagerStates::ArrivesAtBuildingSite);
	}
	else
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
	}
}

/// GetRandomBuildPos(&out, me, &+0x118) (vt +0x128): the index is written into buildPosIndex
glm::ivec2 RandomBuildPos(entt::entity villager, entt::entity site)
{
	auto* v = VillagerOf(villager);
	int32_t index = v != nullptr ? v->buildPosIndex : 0;
	const auto pos = Ops().getRandomBuildPos(site, villager, index);
	v = VillagerOf(villager);
	if (v != nullptr)
	{
		v->buildPosIndex = index;
	}
	return Xz(pos);
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float BuildFactor(float alignment)
{
	// 0x758C97..0x758CA3: fmul 0.2f; fsubr 1.0; fst f (24-bit x87: float steps)
	const float product = alignment * k_PointTwo;
	const float fe = 1.0f - product;
	float f = fe;
	// 0x758CA7..0x758CBC: fe < 1 (test ah, 1) -> 1.0
	if (fe < 1.0f)
	{
		f = 1.0f;
	}
	// 0x758CBE..0x758CD7: the stored f > 1.2f (test ah, 0x41; jne skips when <=) -> 0x3F99999A
	else if (f > k_MaxBuildFactor)
	{
		f = k_MaxBuildFactor;
	}
	return f;
}

uint32_t BuildAmount(float woodPerCycle, float factor, uint32_t pile)
{
	// 0x758CDA..0x758CE3: GetWoodUsedPerBuild 0x758E20 (+0x27C); fmul f; __ftol (truncation)
	const float product = woodPerCycle * factor;
	const auto u = static_cast<uint32_t>(static_cast<int64_t>(product));
	// 0x758CEA..0x758D18: GetResource(WOOD) (vt +0x98) < u (cmp; jae) -> u = it (else the same product again)
	return pile < u ? pile : u;
}

float BuildStep(uint32_t amount, float woodValue)
{
	// 0x758D27..0x758D51: (float)(uint64)u (fild qword; fstp float), fdivr GetWoodValue (float), fstp float
	const auto u = static_cast<float>(static_cast<uint64_t>(amount));
	return u / woodValue;
}

bool BuildPosOk(int32_t index)
{
	// 0x758B12..0x758B25: test eax, eax; jl -> 163; cmp eax, 0x80; jge -> 163
	return index >= 0 && index < k_RingSize;
}

bool NearRingPoint(float distance)
{
	// 0x758B85..0x758B9A: fcomp [0x8AB244] 0.2; test ah, 0x41; jne: the walk only when strictly farther
	return !(distance > k_PointTwo);
}

WoodWeights WoodSourceWeights(uint32_t stock, int16_t capacity, uint32_t maxWoodCarried, bool forBuilding)
{
	if (forBuilding)
	{
		// 0x75F5B8..0x75F5E0: forest 0.5 ([0x8AA3B4]); store = stock > (u32)(int32)(movsx capacity) (`jbe`) ? 1 : 0. A
		// negative capacity (more than MaxWoodCarried held) is huge unsigned: 0 (literal)
		const auto cap = static_cast<uint32_t>(static_cast<int32_t>(capacity));
		return {stock > cap ? 1.0f : 0.0f, 0.5f};
	}
	// 0x75F5E2..0x75F62F: frac = (float)(1 - (capacity + 1e-5) / (MaxWoodCarried (+0x268) + 1e-5)); store frac, forest
	// (float)(1 - frac) (CheckSatisfyWoodDesire's mode, V9)
	const float frac = DropOffFraction(capacity, maxWoodCarried);
	return {frac, 1.0f - frac};
}

WoodChoice ChooseWoodSource(WoodWeights weights, float storeDistance, std::optional<float> forestDistance,
                            bool isBigForest, float maxDistance)
{
	WoodChoice choice;
	// 0x75F63C..0x75F65A: GetDistanceModifier(d store, D) x storeW, stored float
	choice.store = gutils::GetDistanceModifier(storeDistance, maxDistance) * weights.store;
	// 0x75F673..0x75F6A1: a forest ? GetDistanceModifier(d forest, D) x forestW : 0
	choice.forest = forestDistance ? gutils::GetDistanceModifier(*forestDistance, maxDistance) * weights.forest : 0.0f;
	// 0x75F6AC..0x75F6C1: store > forest (fcomp; test ah, 0x41; jne) -> 1
	if (choice.store > choice.forest)
	{
		choice.how = 1;
	}
	// 0x75F6C4..0x75F6FF: forest != 0 and a forest: its +0x38 (BigForest) -> 2, else 3
	else if (choice.forest != 0.0f && forestDistance)
	{
		choice.how = isBigForest ? 2 : 3;
	}
	return choice;
}

// ---- the decision path -------------------------------------------------------------------------------------------

uint32_t CheckNeededForBuilding(entt::entity villager)
{
	// 0x758343 GetTown (vt +0x48). (openblack, guard) the original has no null test (its callers have a town)
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	const auto& ops = Ops();
	// 0x75834A: Town::IsBuildingHappening 0x73E2F0 (+0x794 != 0) -> else 0
	if (!ops.isBuildingHappening(town))
	{
		return 0;
	}
	// 0x758353..0x75836F: GetBestBuildingSite(me +0x14, +0xF2 == 4 (sete al))
	const bool includeFull = IsBuilderDisciple(villager);
	const auto site = ops.getBestBuildingSite(town, MeCoords(villager), includeFull);
	// 0x758375..0x758387: a site and SetupBuildingObject(site) == 1 -> 1
	const uint32_t result = site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: need {} (best of {}, includeFull {}) -> {}", static_cast<uint32_t>(site),
		                            building_sites::SitesOf(town).size(), includeFull ? 1 : 0, result));
	}
	return result;
}

uint32_t SetupBuildingObject(entt::entity villager, entt::entity site)
{
	// 0x7584BA: GetTown -> else 0
	if (TownOf(villager) == entt::null)
	{
		return 0;
	}
	const auto& ops = Ops();
	// 0x7584C4..0x7584CD: GetBuilding 0x43BC70 -> else 0
	const auto building = ops.getBuilding(site);
	if (building == entt::null)
	{
		return 0;
	}
	// 0x7584D3..0x7584EA: IsBuilt (vt +0x890) == 1 and IsRepaired (vt +0x88C) != 0 -> 0
	if (ops.isBuilt(building) && ops.isRepaired(building))
	{
		return 0;
	}
	// 0x7584EE..0x758505: CheckForClearArea(building +0x14, GetClearAreaRadius 0x43BDE0) == 1 -> 1
	const float radius = ops.getClearAreaRadius(site);
	if (CheckForClearArea(villager, tq::PosOf(building), radius) == 1)
	{
		return 1;
	}
	// 0x758510..0x758525: SetupGetBuildingSupplies(site) == 1 ? 1 : 0
	return SetupGetBuildingSupplies(villager, site) == 1 ? 1 : 0;
}

uint32_t SetupBuildingObjectForBuilding(entt::entity villager, entt::entity building)
{
	// 0x75853B: GetTown -> else 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	const auto& ops = Ops();
	// 0x758552: Town::GetBuildingSiteInList 0x73CE40
	auto site = ops.getBuildingSiteInList(town, building);
	// 0x75855B..0x75857F: while none: AddBuildingSite(MultiMapFixed*) 0x73B8E0 (0 -> 0) and the list again. (openblack,
	// guard) one retry only: the original loops for ever when the added site has no GetBuilding (V7_spec §3.3)
	if (site == entt::null)
	{
		if (ops.addBuildingSite(town, building) == entt::null)
		{
			TraceIf(villager, fmt::format("build: building {} AddBuildingSite failed -> 0", static_cast<uint32_t>(building)));
			return 0;
		}
		site = ops.getBuildingSiteInList(town, building);
		if (site == entt::null)
		{
			return 0;
		}
	}
	// 0x758584..0x758591: SetupBuildingObject(site) == 1 (dec, neg, sbb, inc)
	return SetupBuildingObject(villager, site) == 1 ? 1 : 0;
}

uint32_t CheckForClearArea([[maybe_unused]] entt::entity villager, [[maybe_unused]] glm::ivec2 pos,
                           [[maybe_unused]] float radius)
{
	// 0x7590A0: fn_0074E4C0(point, radius, ClearAreaPoint::ProcessPoint 0x7591E0, 10.0, 1) keeps the nearest object
	// with IsPushable (vt +0x814) and CalculateForceAppliedBy > 0.05. (inferred, equivalent) every Object vtable points
	// +0x814 at Object::IsPushable 0x402AE0 (`xor eax, eax; ret`; pushable.py, 135 vtables), so nothing is found and
	// the visit has no other side effect (no GameRand): 0. State 185 ARRIVE_AT_PUSH_OBJECT is never entered (not
	// ported)
	return 0;
}

// ---- the town desires (VillagerSatisfy.h) ------------------------------------------------------------------------

uint32_t CheckSatisfyAbodesDesire(entt::entity villager)
{
	// 0x758E33: CheckNeededForBuilding == 1 -> 1 (any site, not only an abode's: literal)
	if (CheckNeededForBuilding(villager) == 1)
	{
		return 1;
	}
	// (openblack, guard) GetTown (vt +0x48) is not tested for null here
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// 0x758E46..0x758E57: Town +0x5E4 == 0 -> +0x5E4 = 1, before the request (also when it fails)
	auto& t = Entities().Get<Town>(town);
	if (t.requestedPlanThisTurn)
	{
		return 0;
	}
	t.requestedPlanThisTurn = true;
	// 0x758E61..0x758E80: Town::RequestANewAbode(2) 0x73B330 == 1 && CheckNeededForBuilding == 1 -> 1
	const bool requested = Ops().requestANewAbode(town);
	TraceIf(villager, fmt::format("build: abodes desire: RequestANewAbode -> {}", requested ? 1 : 0));
	return requested && CheckNeededForBuilding(villager) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyCivicBuildings(entt::entity villager)
{
	// 0x758E93: CheckNeededForBuilding == 1 -> 1
	if (CheckNeededForBuilding(villager) == 1)
	{
		return 1;
	}
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// 0x758EA6..0x758EB7: Town +0x5E4 == 0 -> +0x5E4 = 1 first
	auto& t = Entities().Get<Town>(town);
	if (t.requestedPlanThisTurn)
	{
		return 0;
	}
	t.requestedPlanThisTurn = true;
	// 0x758EC1..0x758EDE: Town::RequestBestPlanned 0x73A650 (0x758ECA) == 1 && CheckNeededForBuilding == 1 -> 1
	const bool requested = Ops().requestBestPlanned(town);
	TraceIf(villager, fmt::format("build: civic desire: RequestBestPlanned -> {}", requested ? 1 : 0));
	return requested && CheckNeededForBuilding(villager) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyToBuild(entt::entity villager)
{
	// 0x759333: GetTown -> else 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// 0x75933C..0x759351: GetBestBuildingSite(me +0x14, +0xF2 == 4); a site and SetupBuildingObject == 1 -> 1
	const auto site = Ops().getBestBuildingSite(town, MeCoords(villager), IsBuilderDisciple(villager));
	return site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyToRepair(entt::entity villager)
{
	// 0x759373: GetTown -> else 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// 0x75937E: Town::GetBestRepairBuildingSite 0x747EA0; a site and SetupBuildingObject == 1 -> 1
	const auto site = Ops().getBestRepairBuildingSite(town);
	return site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
}

// ---- the wood supply ---------------------------------------------------------------------------------------------

uint32_t SetupGetBuildingSupplies(entt::entity villager, entt::entity site)
{
	// 0x7586EB: GetTown -> else 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	const auto& ops = Ops();
	// 0x758706: IsBuildingSiteValid -> else 0
	if (!ops.isBuildingSiteValid(town, site))
	{
		return 0;
	}
	// 0x75871A..0x758726: BuildingSite::ShouldIGetWood(me) 0x43C680 (it asks for GetResourceDropoffPos(WOOD) 0x753E20
	// itself, only where it needs it) == 0 -> GotoBuildingSite(site)
	const auto dropoff = [villager]() {
		const auto p = GetResourceDropoffPos(villager, ResourceType::Wood);
		return map_coords::MapCoords {p.x, p.y, 0.0f};
	};
	if (!ops.shouldIGetWood(site, villager, dropoff))
	{
		TraceIf(villager, fmt::format("build: supplies site {} shouldGet 0 -> goto", static_cast<uint32_t>(site)));
		return GotoBuildingSite(villager, site);
	}
	// 0x758733..0x758744: g_game +0x14 & 0x40000 -> SetupWaitForWood 0x7585A0 (and on). (inferred) the bit is never set
	// in W120 (the only reference in .text; GGame init writes +0x14 = 0x20): SetupWaitForWood / 232 WAIT_FOR_WOOD are
	// not ported
	// 0x758757: DecideHowToGetWood(1, &bigForest, &forest)
	const auto source = DecideHowToGetWood(villager, true);
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: supplies site {} shouldGet 1 how {} (store {:.9f} forest {:.9f})",
		                            static_cast<uint32_t>(site), source.how, source.store, source.forestScore));
	}
	switch (source.how)
	{
	case 1:
		// 0x7587BF: GotoStoragePitForBuildingMaterials(site)
		return GotoStoragePitForBuildingMaterials(villager, site);
	case 2:
		// 0x75878B..0x7587B9: +0xFC = site; SetupMoveToOnFootpath(bigForest, BigForest::GetArrivePos(me) 0x439360, 53
		// ARRIVES_AT_BIG_FOREST); 1. TODO(V9): the forest walks are not ported: neutral 0, +0xFC untouched
		return 0;
	case 3:
		// 0x75876F..0x758788: +0xFC = site; VillagerGotoForest(forest, 49 FORESTER_ARRIVES_AT_FOREST) 0x75F720.
		// TODO(V9): neutral 0, +0xFC untouched
		return 0;
	default:
		return 0;
	}
}

WoodSource DecideHowToGetWood(entt::entity villager, bool forBuilding)
{
	WoodSource result;
	if (VillagerOf(villager) == nullptr)
	{
		return result;
	}
	const auto town = TownOf(villager);
	// 0x75F51D..0x75F538: pos = (0, 0, 0); D = GTownInfo maxDistanceForTownForest ([0xDA28E4], 250)
	glm::ivec2 pos {0, 0};
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	uint32_t stock = 0;
	// 0x75F53E..0x75F57C: GetStoragePit 0x751F10 (the town's or the home) and its IsFunctional (vt +0xD4) -> its
	// GetArrivePos (vt +0x104) and GetResource(WOOD) (vt +0x98)
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		pos = abode_queries::GetArrivePos(pit);
		stock = object_resources::GetResource(pit, ResourceType::Wood);
	}
	else if (town != entt::null)
	{
		// 0x75F57E..0x75F5A6: Town::GetTemporaryResourceStorePotOrPos(me, &pos, WOOD) 0x73E900 (the pot is made if the
		// town has none: a side effect, literal) and the pot's GetResource(WOOD). (openblack, guard) no pot -> 0 (the
		// original has no null test)
		const auto store = GetTemporaryStore(town, MeCoords(villager), ResourceType::Wood);
		pos = {store.pos.x, store.pos.z};
		if (store.pot != entt::null && Entities().Valid(store.pot))
		{
			stock = object_resources::GetResource(store.pot, ResourceType::Wood);
		}
	}
	// (openblack, guard) without a town the original dereferences null at 0x75F596: pos (0, 0), stock 0
	// 0x75F5AB: GetWoodCapacity (its result unused); 0x75F5B4..0x75F62F: the weights (GetWoodCapacity again)
	const auto weights = WoodSourceWeights(stock, GetWoodCapacity(villager), InfoOf(villager).maxWoodCarried, forBuilding);
	// 0x75F63C: GetDistanceInMetres(pos, me) fn_00605CD0
	const auto me = tq::PosOf(villager);
	const float storeDistance = tq::GetDistanceInMetres(pos, me);
	// 0x75F660..0x75F693: with a town Town::FindNearestForestToPos(me) 0x73EC10; none -> FindForest(me, D, 0)
	// fn_0053A1A0. (approximate) the town's forest list (+0x608, Town::AsssignTownFeature 0x73EAC0 on load) is not
	// filled yet, so the global FindForest decides (V7_spec P-2)
	const auto at = PositionOf(villager);
	std::optional<uint32_t> forest;
	if (town != entt::null)
	{
		forest = FindNearestForestToPos(Entities().Get<const Town>(town).id, at);
	}
	if (!forest)
	{
		forest = FindForest(at, maxDistance, false);
	}
	// 0x75F673..0x75F68A: GetDistanceInMetres(forest +0x14, me); +0x38 the BigForest
	std::optional<float> forestDistance;
	entt::entity bigForest = entt::null;
	if (forest)
	{
		const auto centre = ForestCentre(*forest);
		// (approximate) the forest's MapCoords +0x14 (0x75F675) from its centre in metres (a round trip: may be one fixed
		// unit off); openblack's Trees keeps no MapCoords for a forest
		forestDistance = tq::GetDistanceInMetres(tq::ToMapCoords({centre.x, centre.z}), me);
		bigForest = ForestBigForest(*forest);
	}
	const auto choice = ChooseWoodSource(weights, storeDistance, forestDistance, bigForest != entt::null, maxDistance);
	result.how = choice.how;
	result.store = choice.store;
	result.forestScore = choice.forest;
	if (choice.how == 2)
	{
		result.bigForest = bigForest; // 0x75F6E2: *bigForest = +0x38
	}
	else if (choice.how == 3)
	{
		result.forest = forest; // 0x75F6F5: *forest = it
	}
	return result;
}

uint32_t GotoStoragePitForBuildingMaterials(entt::entity villager, entt::entity site)
{
	// 0x7587DD / 0x7587F8: GetTown and IsBuildingSiteValid -> else 0
	const auto town = TownOf(villager);
	const auto& ops = Ops();
	if (town == entt::null || !ops.isBuildingSiteValid(town, site))
	{
		return 0;
	}
	// 0x758824..0x758831: GetWoodCapacity <= 0 (test ax, ax; jg) -> GotoBuildingSite(site)
	if (GetWoodCapacity(villager) <= 0)
	{
		TraceIf(villager, "build: pit for materials: full -> goto");
		return GotoBuildingSite(villager, site);
	}
	// 0x758841: GetStoragePit 0x751F10 (kept even when it is not functional)
	const auto pit = GetStoragePit(villager);
	glm::ivec2 pos {0, 0};
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		// 0x75885A..0x758882: pit.GetResourceNearestEdge(WOOD, me, 0) (vt +0x8D4)
		pos = GetResourceNearestEdge(pit, ResourceType::Wood, villager);
	}
	else
	{
		// 0x75888D: GetResourceDropoffPos(WOOD) 0x753E20 (may make the town's temporary wood pot)
		pos = GetResourceDropoffPos(villager, ResourceType::Wood);
	}
	// 0x7588A6..0x7588B6: me in the site's builder list (+0x18)?
	if (!ops.isBuilder(site, villager))
	{
		// 0x7588BA..0x7588D4: !NeedsBuilders fn_0043BC60 && +0xF2 != 4 -> 0
		if (!ops.needsBuilders(site) && !IsBuilderDisciple(villager))
		{
			TraceIf(villager, fmt::format("build: pit for materials: site {} full -> 0", static_cast<uint32_t>(site)));
			return 0;
		}
		// 0x7588D7..0x758912: the exit of the raw TOP's row (+0x8C) is not ExitBuilding 0x7597B0 -> +0xFC = site
		if (!IsBuildingExitState(GetState(villager, Index::Top)))
		{
			if (auto* v = VillagerOf(villager); v != nullptr)
			{
				v->buildingSite = site;
			}
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: pit for materials: {} ({}, {}) -> 39", static_cast<uint32_t>(pit), pos.x, pos.y));
	}
	// 0x75891E..0x758940: a pit (or home), functional or not -> SetupMoveToOnFootpath(pit, pos, 39) (literal); else
	// SetupMoveToWithHug(pos, 39); 1
	if (pit != entt::null)
	{
		SetupMoveToOnFootpath(villager, pit, pos, VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	else
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	return 1;
}

// ---- the site ----------------------------------------------------------------------------------------------------

uint32_t GotoBuildingSite(entt::entity villager, entt::entity site)
{
	// 0x758A0E / 0x758A22: GetTown and IsBuildingSiteValid -> else 0
	const auto town = TownOf(villager);
	const auto& ops = Ops();
	if (town == entt::null || !ops.isBuildingSiteValid(town, site))
	{
		return 0;
	}
	// 0x758A2F..0x758A53: not in the builder list, !NeedsBuilders and +0xF2 != 4 -> 0
	if (!ops.isBuilder(site, villager) && !ops.needsBuilders(site) && !IsBuilderDisciple(villager))
	{
		TraceIf(villager, fmt::format("build goto: site {} full -> 0", static_cast<uint32_t>(site)));
		return 0;
	}
	// 0x758A62: SetTopState(163): a building state leaves through ExitBuilding (RemoveBuilder, +0xFC = 0)
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// 0x758A74: +0xFC = site, after the SetTopState
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->buildingSite = site;
	}
	// 0x758A7F: GetRandomBuildPos(&pos, me, &+0x118) (vt +0x128: one GameFloatRand, Edificios)
	const auto pos = RandomBuildPos(villager, site);
	// 0x758A8E..0x758ACD: the walk with FINAL 40
	WalkToRingPoint(villager, site, pos, "goto");
	return 1;
}

uint32_t EnterBuilding(LivingAction& action, VillagerStates final, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto town = TownOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto& ops = Ops();
	// 0x759762..0x75976D: IsBuildingSiteValid(GetTown, +0xFC) -> else 0 (refused: the Villager wrapper enters 163).
	// (openblack, guard) no town -> refused: the original calls IsBuildingSiteValid on GetTown's null (0x75975D)
	if (town == entt::null || !ops.isBuildingSiteValid(town, site))
	{
		TraceIf(villager, fmt::format("build enter {} site {} refused", static_cast<uint32_t>(next),
		                              static_cast<uint32_t>(site)));
		return 0;
	}
	// 0x759788..0x759794: !IsStateEntryFunctionSameAs(final, next) 0x7524D0 -> AddBuilder(+0xFC, me) 0x43BE40
	if (!IsStateEntryFunctionSameAs(final, next))
	{
		ops.addBuilder(site, villager);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("build enter {} site {} builders {}", static_cast<uint32_t>(next),
			                            static_cast<uint32_t>(site), ops.getBuilderCount(site)));
		}
	}
	return 1;
}

uint32_t ExitBuilding(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// 0x7597BB: IsStateExitFunctionSameAs(next) (vt +0x96C 0x752530) -> 1: the builder stays one
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	const auto site = v->buildingSite;
	const auto town = TownOf(villager);
	const auto& ops = Ops();
	// 0x7597CC..0x7597F7: GetTown && IsBuildingSiteValid(+0xFC) -> RemoveBuilder(+0xFC, me) 0x43BE90
	if (town != entt::null && ops.isBuildingSiteValid(town, site))
	{
		ops.removeBuilder(site, villager);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("build exit {} site {} builders {}", static_cast<uint32_t>(next),
			                            static_cast<uint32_t>(site), ops.getBuilderCount(site)));
		}
	}
	// 0x7597FC: +0xFC = 0; 1
	if (auto* still = VillagerOf(villager); still != nullptr)
	{
		still->buildingSite = entt::null;
	}
	return 1;
}

uint32_t ArrivesAtStoragePitForBuildingMaterials(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	// 0x75899C / 0x7589A6: +0xFC null or not a valid site -> 0x7589E4 SetTopState(163); 1. (openblack, guard) no town
	// -> the same: the original calls IsBuildingSiteValid on GetTown's null (0x7589A1)
	if (site == entt::null || town == entt::null || !Ops().isBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 39: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x7589B1: cap = (int)(movsx GetWoodCapacity)
	const int32_t cap = GetWoodCapacity(villager);
	// 0x7589BD..0x7589C8: cap == 0 -> GotoBuildingSite(site) == 1 ? 1 : SetTopState(163), 1
	if (cap == 0)
	{
		TraceIf(villager, "build 39: cap 0 -> goto");
		if (GotoBuildingSite(villager, site) == 1)
		{
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x7589CD..0x7589DC: ArrivesAtStoragePitForResource(WOOD, cap (pushed as is: a negative cap is a huge unsigned n,
	// literal), 184, 163)
	const auto held = v->resourceHeld.at(1);
	const auto r = ArrivesAtStoragePitForResource(villager, ResourceType::Wood, static_cast<uint32_t>(cap),
	                                              VillagerStates::ReenterBuildingState, VillagerStates::DecideWhatToDo);
	if (TraceOn(villager))
	{
		const auto* after = VillagerOf(villager);
		Trace(villager, fmt::format("build 39: cap {} -> +{} wood (pit {}) = {:#x}", cap,
		                            after != nullptr ? after->resourceHeld.at(1) - held : 0,
		                            static_cast<uint32_t>(GetStoragePit(villager)), r));
	}
	return r;
}

uint32_t ArrivesAtBuildingSite(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	const auto& ops = Ops();
	// 0x758B05: !IsBuildingSiteValid(GetTown, +0xFC) -> 0x758C25 SetTopState(163); 1 (+0xFC kept: the exit clears it)
	if (v == nullptr || town == entt::null || !ops.isBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 40: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x758B12..0x758B25: +0x118 outside 0..0x7F -> 163
	const int32_t index = v->buildPosIndex;
	const auto ring = BuildPosOk(index) ? ops.getBuildPos(site, index) : std::nullopt;
	if (!ring)
	{
		TraceIf(villager, fmt::format("build 40: ring {} -> 163", index));
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x758B2B..0x758B7D: pos = (ftol(ring[i].x x 6553.6), ftol(ring[i].z x 6553.6), 0)
	const auto pos = Xz(*ring);
	// 0x758B85..0x758BAF: GetDistanceInMetres(me, pos) fn_00605CD0 > 0.2 -> SetupMoveToWithHug(pos, 40); 1
	const float d = tq::GetDistanceInMetres(tq::PosOf(villager), pos);
	if (!NearRingPoint(d))
	{
		TraceIf(villager, fmt::format("build 40: ring {} dist {:.3f} -> walk", index, d));
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// 0x758BB8..0x758BC8: LookAtObject(GetBuilding(site), 1) 0x5EC520 (null -> 1; else LookAtPos(its +0x14, 1)) != 1 ->
	// 1 (still turning)
	const auto building = ops.getBuilding(site);
	const uint32_t look = building != entt::null ? LookAtPos(villager, tq::PosOf(building), k_LookMode) : 1;
	if (look != 1)
	{
		return 1;
	}
	// 0x758BCA..0x758BFC: wood carried (+0xF6) -> site.AddResource(WOOD, (int16)+0xF6, 0, 0, &me +0x14, 0) (vt +0x9C,
	// its result ignored); DropWood(0) (all of it, whatever was added: literal); SetStateCarriedObject
	const auto wood = v->resourceHeld.at(1);
	uint32_t added = 0;
	if (wood != 0)
	{
		const auto me = MeCoords(villager);
		added = ops.addResource(site, ResourceType::Wood, static_cast<uint32_t>(static_cast<int32_t>(wood)), &me);
		DropWood(villager, 0);
		if (auto* animation = Entities().TryGet<SkeletalAnimation>(villager);
		    animation != nullptr && !animation->carriedLocked)
		{
			animation->carriedObject = SetStateCarriedObject(villager, animation->carriedObject);
		}
	}
	TraceIf(villager, fmt::format("build 40: ring {} dist {:.3f} look {} drop {} (added {})", index, d, look, wood, added));
	// 0x758C01..0x758C13: t = +0x90; PlayAnimThenSetState(41) 0x5ECAC0 (TOP 23 WAIT_FOR_ANIMATION, FINAL 41); +0x90 = t
	// (so 23 ends when the 348 clip begun on entering 40 has played once)
	const uint16_t turns = action.turnsSinceStateChange;
	PlayAnimThenSetState(villager, VillagerStates::Building);
	if (auto* still = ActionOf(villager); still != nullptr)
	{
		still->turnsSinceStateChange = turns;
	}
	return 1;
}

uint32_t BuildingState(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// 0x758C55: GetTown -> else 0
	const auto town = TownOf(villager);
	if (v == nullptr || town == entt::null)
	{
		return 0;
	}
	const auto site = v->buildingSite;
	// 0x758C62..0x758C69: IsReadyForNewAnimation(1) 0x5EC960 (the BuildingAnimation clip played once) -> else 1
	if (!VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		return 1;
	}
	// 0x758C6F: +0x90 = 0 (one build cycle per clip)
	action.turnsSinceStateChange = 0;
	const auto& ops = Ops();
	// 0x758DEF..0x758E02: +0xFC = 0 FIRST (so the exit skips RemoveBuilder), then SetTopState(163); 1
	const auto release = [villager](const char* why) {
		if (auto* still = VillagerOf(villager); still != nullptr)
		{
			still->buildingSite = entt::null;
		}
		TraceIf(villager, fmt::format("build 41: release ({})", why));
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1u;
	};
	// 0x758C82: !IsBuildingSiteValid -> release
	if (!ops.isBuildingSiteValid(town, site))
	{
		return release("invalid");
	}
	// 0x758C8F..0x758C92: MapCoords::GetAlignment 0x6057B0 on +0x14 (Milagros' LandAlignmentAt)
	const float a = ops.landAlignmentAt(PositionOf(villager));
	const float f = BuildFactor(a);
	// 0x758CDA..0x758D18: u = min(ftol(WoodUsedPerBuildCycle x f), the pile)
	const float perCycle = InfoOf(villager).woodUsedPerBuildCycle;
	const uint32_t pile = ops.getResource(site, ResourceType::Wood);
	const uint32_t u = BuildAmount(perCycle, f, pile);
	// 0x758D1C: GetBuilding, kept for the IsBuilt test below
	const auto building = ops.getBuilding(site);
	float value = 0.0f;
	float x = 0.0f;
	// 0x758D21..0x758D71: u != 0
	if (u != 0)
	{
		// 0x758D27..0x758D51: x = (float)u / GetWoodValue 0x43C0C0, the value read BEFORE the removal
		value = ops.getWoodValue(site);
		x = BuildStep(u, value);
		// 0x758D55: site.RemoveResource(WOOD, u, 0, 0) (vt +0xA0), its result ignored
		static_cast<void>(ops.removeResource(site, ResourceType::Wood, u));
		// 0x758D62: fn_0043D080 -> GetBuilding()->BuildBy(x) (vt +0x900; may Build and delete the site)
		ops.buildBy(site, x);
		// 0x758D71: fn_0073B620(GetTown, u): TownStats +0xEC. TODO(stats): the player's GameStats (+0xA44 +0xA8) are not
		// ported
		ops.addWoodUsedForBuilding(town, u);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build 41: a {:.6f} f {:.9f} u {} pile {} value {:.1f} x {:.9f} built {}", a, f, u, pile,
		                            value, x, building != entt::null ? abodes::GetPercentBuilt(building) : 0.0f));
	}
	// 0x758D7A: !site.IsAvailable (vt +0x2C GameThing 0x401810) -> release (the site was deleted by Built: the walk out
	// ToBeDeleted gave this villager is replaced by the 163 below, literal)
	if (!ops.isAvailable(site))
	{
		return release("not available");
	}
	// 0x758D87..0x758D9E: IsBuilt && IsRepaired -> release (without RemoveBuilder: literal)
	if (building != entt::null && ops.isBuilt(building) && ops.isRepaired(building))
	{
		return release("built");
	}
	// 0x758DA6..0x758DCF: wood left in the pile -> GetNextPosFromIndex(&pos, &+0x118) (vt +0x124: GameFloatRand then
	// GameRand(2)); SetupMoveToWithHug(pos, 40); 1
	if (ops.getResource(site, ResourceType::Wood) != 0)
	{
		auto* still = VillagerOf(villager);
		int32_t index = still != nullptr ? still->buildPosIndex : 0;
		const auto pos = Xz(ops.getNextPosFromIndex(site, index));
		still = VillagerOf(villager);
		if (still != nullptr)
		{
			still->buildPosIndex = index;
		}
		TraceIf(villager, fmt::format("build 41: -> next {}", index));
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// 0x758DE3: SetupGetBuildingSupplies(site): 0 leaves the builder in 41, hammering an empty site (literal)
	const auto r = SetupGetBuildingSupplies(villager, site);
	TraceIf(villager, fmt::format("build 41: -> supplies {}", r));
	return r;
}

uint32_t ReenterBuildingState(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	const auto& ops = Ops();
	// 0x758F74 / 0x758F8B: GetTown and IsBuildingSiteValid -> else 0x75904F SetTopState(163); 1
	if (town == entt::null || !ops.isBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 184: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x758F94, 0x759042..0x759058: ShouldIGetWood -> SetupGetBuildingSupplies == 1 ? 1 : SetTopState(163), 1
	const auto dropoff = [villager]() {
		const auto p = GetResourceDropoffPos(villager, ResourceType::Wood);
		return map_coords::MapCoords {p.x, p.y, 0.0f};
	};
	if (ops.shouldIGetWood(site, villager, dropoff))
	{
		TraceIf(villager, "build 184: shouldGet 1");
		if (SetupGetBuildingSupplies(villager, site) == 1)
		{
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x758FB3: GetRandomBuildPos(&pos, me, &+0x118) (one GameFloatRand, also when touching)
	const auto pos = RandomBuildPos(villager, site);
	// 0x758FCA..0x758FEB: IsTouching(GetBuilding(site), 0.001) (vt +0x6B8 Villager 0x55C9A0 -> Object 0x637E00) ->
	// SetTopState(40); 1
	const auto building = ops.getBuilding(site);
	const bool touching = building != entt::null && ops.isTouching(villager, building, k_TouchingMargin);
	TraceIf(villager, fmt::format("build 184: shouldGet 0 touching {}", touching ? 1 : 0));
	if (touching)
	{
		SetTopState(villager, VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// 0x758FF5..0x759032: the walk with FINAL 40
	WalkToRingPoint(villager, site, pos, "184");
	return 1;
}

// ---- the building side -------------------------------------------------------------------------------------------

BuildingSiteOps DefaultBuildingSiteOps()
{
	namespace bs = building_sites;
	BuildingSiteOps ops;
	ops.isBuildingHappening = [](entt::entity town) { return bs::IsBuildingHappening(town); };
	ops.getBestBuildingSite = [](entt::entity town, const map_coords::MapCoords& pos, bool includeFull) {
		return bs::GetBestBuildingSite(town, pos, includeFull);
	};
	ops.getBestRepairBuildingSite = [](entt::entity town) { return bs::GetBestRepairBuildingSite(town); };
	ops.isBuildingSiteValid = [](entt::entity town, entt::entity site) {
		return site != entt::null && bs::IsBuildingSiteValid(town, site);
	};
	ops.getBuildingSiteInList = [](entt::entity town, entt::entity building) {
		return bs::GetBuildingSiteInList(town, building);
	};
	ops.addBuildingSite = [](entt::entity town, entt::entity building) { return bs::AddBuildingSite(town, building); };
	ops.requestBestPlanned = [](entt::entity town) { return bs::RequestBestPlanned(town); };
	// Town::RequestANewAbode(2) 0x73B330 (CheckSatisfyAbodesDesire 0x758E63 `push 2`; the type is not used)
	ops.requestANewAbode = [](entt::entity town) { return bs::RequestANewAbode(town, AbodeType::LivingQuarters); };
	ops.addWoodUsedForBuilding = [](entt::entity town, uint32_t wood) { bs::AddWoodUsedForBuilding(town, wood); };
	ops.getBuilding = [](entt::entity site) { return bs::GetBuilding(site); };
	ops.needsBuilders = [](entt::entity site) { return bs::NeedsBuilders(site); };
	ops.isBuilder = [](entt::entity site, entt::entity villager) { return bs::IsBuilder(site, villager); };
	ops.getBuilderCount = [](entt::entity site) { return bs::GetBuilderCount(site); };
	ops.getClearAreaRadius = [](entt::entity site) { return bs::GetClearAreaRadius(site); };
	ops.getWoodValue = [](entt::entity site) { return bs::GetWoodValue(site); };
	ops.shouldIGetWood = [](entt::entity site, entt::entity villager,
	                        const std::function<map_coords::MapCoords()>& resourceDropoffPos) {
		return bs::ShouldIGetWood(site, villager, resourceDropoffPos);
	};
	ops.getResource = [](entt::entity site, ResourceType type) { return bs::GetResource(site, type); };
	ops.addResource = [](entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos) {
		return bs::AddResource(site, type, amount, pos);
	};
	ops.removeResource = [](entt::entity site, ResourceType type, uint32_t amount) {
		return bs::RemoveResource(site, type, amount);
	};
	ops.buildBy = [](entt::entity site, float amount) { bs::BuildBy(site, amount); };
	ops.isAvailable = [](entt::entity site) { return bs::IsAvailable(site); };
	ops.getRandomBuildPos = [](entt::entity site, entt::entity villager, int32_t& index) {
		return bs::GetRandomBuildPos(site, villager, index);
	};
	ops.getNextPosFromIndex = [](entt::entity site, int32_t& index) { return bs::GetNextPosFromIndex(site, index); };
	ops.getBuildPos = [](entt::entity site, int32_t index) { return bs::GetBuildPos(site, index); };
	ops.addBuilder = [](entt::entity site, entt::entity villager) { bs::AddBuilder(site, villager); };
	ops.removeBuilder = [](entt::entity site, entt::entity villager) { bs::RemoveBuilder(site, villager); };
	ops.isBuilt = [](entt::entity building) { return abodes::IsBuilt(building); };
	ops.isRepaired = [](entt::entity building) { return abodes::IsRepaired(building); };
	ops.isTouching = [](entt::entity villager, entt::entity building, float margin) {
		return object::IsTouching(villager, building, margin);
	};
	ops.landAlignmentAt = [](const glm::vec3& position) { return effects::alignment::LandAlignmentAt(position); };
	return ops;
}

void SetBuildingSiteOpsForTests(std::optional<BuildingSiteOps> ops)
{
	OpsOverride() = std::move(ops);
}
} // namespace openblack::ecs::villager
