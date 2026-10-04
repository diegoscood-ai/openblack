/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingSites.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Objects/MagicPiles.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

// BuildingSite.cpp, PlannedAbode (Abode.cpp) and Town.cpp of runblack.exe W120 (BuildingSites.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

BuildingSite* SiteComponent(entt::entity site)
{
	auto& registry = Entities();
	return site != entt::null && registry.Valid(site) ? registry.TryGet<BuildingSite>(site) : nullptr;
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

/// The CitadelBuildingSite part of a site, null for a StandardBuildingSite
CitadelBuildingSite* CitadelSiteComponent(entt::entity site)
{
	auto& registry = Entities();
	return site != entt::null && registry.Valid(site) ? registry.TryGet<CitadelBuildingSite>(site) : nullptr;
}

/// A PlannedTownCitadelHeart's GCitadelHeartInfo (+0x40); null for any other plan
const GCitadelHeartInfo* HeartInfoOf(entt::entity town, plans::PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size() || !t->plannedAbodes.at(plan).citadelHeart)
	{
		return nullptr;
	}
	return &archetypes::CitadelArchetype::HeartInfo(t->plannedAbodes.at(plan).heartInfo);
}

// ---- constants (W120) ---------------------------------------------------------------------------------------------

/// [0x8AB230] = 1.1 and [0x8AB22C] = 0.1: the repair base 1.1 x life - 0.1 (BuildingSite ctor 0x43B7E0,
/// Abode::ReduceLife 0x405EA7)
constexpr float k_RepairBaseScale = 1.1f;
constexpr float k_RepairBaseOffset = 0.1f;
/// [0x8C6B64] = 0x3D490FDB (2 pi / 128): PosBuilder::Process's angle step (0x43AF5E, 0x43B2A2..)
constexpr float k_RingStep = std::bit_cast<float>(0x3D490FDBu);
/// [0x8C581C] = 2.5: PosBuilder's "low edge" bound, the two lowest vertices' LOCAL y (strict)
constexpr float k_LowEdgeY = 2.5f;
/// GetNearestEdge 0x43CE40's bounds: [0x8C6CB4] = 0xC196CBE4 (-6 pi: below -> index 0, 0x43CE47) and [0x8C6CB0] =
/// 0x4196CBE4 (6 pi: above -> pi, then straight to the multiply 0x43CE71 -> 0x43CEC0, the same result)
constexpr float k_MinusSixPi = std::bit_cast<float>(0xC196CBE4u);
constexpr float k_SixPi = std::bit_cast<float>(0x4196CBE4u);
/// [0x8C6CAC] = 0x3E22F983 (1 / 2 pi): GetNearestEdge's index = ftol(angle x it x 128) & 0x7F (0x43CEC0)
constexpr float k_InvTwoPi = std::bit_cast<float>(0x3E22F983u);
constexpr int32_t k_RingMask = 0x7F;
/// GameFloatRand(`push 0x3FC90FDB`, pi / 2, 0x43CE08; BuildingSite.cpp line 0x37C) - [0x8C6C9C] = 0x3F490FDB (pi / 4,
/// 0x43CE1F): GetRandomBuildPos 0x43CDE0, +-45 deg
constexpr float k_RandomBuildSpread = std::bit_cast<float>(0x3FC90FDBu);
constexpr float k_RandomBuildHalf = std::bit_cast<float>(0x3F490FDBu);
/// GetNextPosFromIndex 0x43CF40: step = 2.0 / (Get2DRadius x 2 pi x 0.0078125)
constexpr float k_NextPosMetres = 2.0f;
constexpr float k_NextPosPerEntry = 0.0078125f;
/// [0x8C6C98] = 1.2: GetClearAreaRadius 0x43BDE0
constexpr float k_ClearAreaFactor = 1.2f;
/// [0x8C6CA4] = 50.0: ShouldIGetWood 0x43C680's "close to the building" distance
constexpr float k_CloseToSite = 50.0f;
/// 0x459C4000 = 5000.0: ShouldIGetWood's GetDistanceModifier maximum
constexpr float k_WoodDistanceMax = 5000.0f;
/// [0x8C6CA0] = 0x3EC90FDB (pi / 8, 0x43C3B8) - GameFloatRand(`push 0x3F490FDB`, pi / 4, 0x43C3AE; BuildingSite.cpp
/// line 0x2B8): the wood pile's +-22.5 deg; + 4.0 m past the door's distance (GetResourcePosAndYAngle 0x43C220)
constexpr float k_PileHalfSpread = std::bit_cast<float>(0x3EC90FDBu);
constexpr float k_PileSpread = std::bit_cast<float>(0x3F490FDBu);
constexpr float k_PileBeyondDoor = 4.0f;
/// [0x8AB478] = 2.0: ToBeDeleted 0x43B960's Get2DRadius + 2 (the builders walk out to it)
constexpr float k_BuilderClearance = 2.0f;
/// 0x47C34F80 = 99999.0: GetBestBuildingSite 0x73CF60's start score
constexpr float k_BestSiteStart = 99999.0f;
/// [0x8C5844] = 0.9 and [0x8AB22C] = 0.1: GetBestBuildingSite's weight GetDesireForVillagers x 0.9 + 0.1
constexpr float k_SiteWeightScale = 0.9f;
constexpr float k_SiteWeightOffset = 0.1f;
/// [0x8C7798] = 7.5: Town::CheckWhenNewBuildingCreated 0x741500
constexpr float k_CongregationClearance = 7.5f;
/// GetDesireToBeBuilt 0x73A1A0: 0.8 [0x8C4A04], -10 [0x8C7670], 0.2 [0x8AB244], 0.6 [0x8C7BDC], 10 [0x8AB414], 0.3
/// [0x8AB23C]; a wonder with 7 scaffolds or more is 1.0 (0x73A446)
constexpr float k_HouseCap = 0.8f;
constexpr float k_HouseShortageFloor = -10.0f;
constexpr float k_HouseShare = 0.2f;
constexpr float k_HouseBase = 0.6f;
constexpr float k_HouseCountFloor = 10.0f;
constexpr float k_ScaffoldPenalty = 0.3f;
constexpr uint32_t k_WonderScaffolds = 7;
/// RequestBestPlanned 0x73A650 mask 4 (ABODE_TYPE bit 2: the civic ones and the fields), RequestANewAbode 0x73B330 mask
/// 2 (the living quarters)
constexpr uint32_t k_CivicMask = 4;
constexpr uint32_t k_AbodeMask = 2;
/// GGame::FootballEnabled [0xD0196C]: 0 in .data (its static initial value, a single-player start); openblack has no
/// football pitch (GetDesireToBeBuilt's case 0x1004 then returns 0)
constexpr bool k_FootballEnabled = false;
/// TownDesire info 14 (ToBuildWonder) for GetDesireToBeBuilt's case 0x100
constexpr auto k_WonderDesire = TownDesireInfo::ToBuildWonder;
/// CitadelBuildingSite::GetResourcePosAndYAngle 0x43D470: GetWorshipSiteAngle(index) - 1.1424 ([0x8C6DEC] =
/// 0x3F923A14), GetPosFromAngle(that, 22.0) (`push 0x41B00000`, 0x43D4B9)
constexpr float k_CitadelPileAngle = std::bit_cast<float>(0x3F923A14u);
constexpr float k_CitadelPileMetres = std::bit_cast<float>(0x41B00000u);
/// Citadel::GetWorshipSiteAngle 0x463610: the heart's Y angle (vt +0x508) + slot x 0.897598 ([0x8C836C] = 0x3F65C8FA,
/// 2 pi / 7)
constexpr float k_WorshipSlotAngle = std::bit_cast<float>(0x3F65C8FAu);
/// StandardBuildingSite::GetResourcePosAndYAngle 0x43C24D..0x43C25D: a worship site's pile at the local point (9, 0,
/// -50) (0x41100000, 0, 0xC2480000)
constexpr glm::vec3 k_WorshipSitePile {9.0f, 0.0f, -50.0f};

/// Town +0x740, the town's totem (read by GetDesireToBeBuilt's cases 0x14 / 0x404). (pending) its writer is not read
/// (inferred: Totem::MakeFunctional); openblack keeps none: null
entt::entity TownTotem(entt::entity)
{
	return entt::null;
}
/// Town +0xEA4, the football pitch (case 0x1004). (not ported) null
entt::entity TownFootball(entt::entity)
{
	return entt::null;
}
/// Town +0x750, the workshops (case 0x2004). (not ported) null
entt::entity TownWorkshops(entt::entity)
{
	return entt::null;
}

/// The plan's position as a MapCoords (+0x14): x and z of the script's point (the altitude is not read by V6)
map_coords::MapCoords PlanCoords(const PlannedAbode& plan)
{
	return map_coords::FromMetres(glm::vec2(plan.position.x, plan.position.z));
}

/// Town::CheckWhenNewBuildingCreated(b) 0x741500 (from PostCreatePlanned 0x648C50): GetDistanceInMetres(b, +0xF10) -
/// Get2DRadius(b) < 7.5 -> the congregation point's cache (0, 0, 0)
void CheckWhenNewBuildingCreated(entt::entity town, entt::entity building)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	const map_coords::MapCoords congregation {t->congregationPos.x, t->congregationPos.y, t->congregationPosY};
	if (gutils::GetDistanceInMetres(object::MapCoordsOf(building), congregation) - object::Get2DRadius(building) <
	    k_CongregationClearance)
	{
		t->congregationPos = {0, 0};
		t->congregationPosY = 0.0f;
	}
}

/// fn_43B5A0(A, B, P) 0x43B5A0: P inside the XZ box of A..B, both ends inclusive; y not tested
bool InsideXZ(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p)
{
	return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) && std::min(a.z, b.z) <= p.z &&
	       p.z <= std::max(a.z, b.z);
}

/// fn_43B630(L, S, u, c, P2, &hit) 0x43B630: the ray segment c -> P2 against the edge L -> S in XZ. fn_43B4F0 0x43B4F0:
/// the line L + t u against the vertical plane through c with normal N = (-D.z k, 0, D.x k); the hit's y is
/// interpolated along the edge (V6_pending §2.3). 1 / sqrt exact (the InverseSquareRoot approximation does not move it)
bool EdgeHit(const glm::vec3& l, const glm::vec3& s, const glm::vec3& u, const glm::vec3& c, const glm::vec3& p2,
             glm::vec3& hit)
{
	const float dx = p2.x - c.x;
	const float dz = p2.z - c.z;
	const float k = 1.0f / std::sqrt(dx * dx + dz * dz);
	const float nx = -dz * k;
	const float nz = dx * k;
	const float den = nz * u.z + nx * u.x;
	if (den == 0.0f)
	{
		return false;
	}
	const float t = ((c.x - l.x) * nx + (c.z - l.z) * nz) / den;
	hit = l + t * u;
	return InsideXZ(l, s, hit) && InsideXZ(c, p2, hit);
}

/// PosBuilder::Process(LH3DObject* o, int circle) 0x43AE10 on openblack's L3D (V6_pending §2.6)
void PosBuilderProcess(std::array<glm::vec3, BuildingSite::k_RingSize>& ring, entt::entity building, bool circle)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(building);
	const auto* meshComponent = registry.TryGet<const Mesh>(building);
	const auto meshId = meshComponent != nullptr ? meshComponent->id : entt::id_type {0};
	// (fn_43CDB0: no 3D object -> no Process; the ring stays as the zero-filled `new` left it)
	if (transform == nullptr || meshId == 0 || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(meshId))
	{
		return;
	}
	const auto& mesh = *Locator::resources::value().GetMeshes().Handle(meshId);
	// 1. every entry = o +0x38 (the translation); the static best-distance array 0xC58CD4[128] = 0
	ring.fill(transform->position);
	std::array<float, BuildingSite::k_RingSize> best {};
	// 2. c = the matrix x the mesh's bounding-box centre (mesh +0x18, (min + max) x 0.5); R = o +0x44 x mesh +0x30
	const glm::mat4 toWorld = lh_matrix::Model(*transform);
	const auto box = mesh.GetBoundingBox();
	const glm::vec3 centre = glm::vec3(toWorld * glm::vec4(box.Center(), 1.0f));
	const float radius = object::GetScale(building) * object::MeshHalfDiagonal(meshId);
	if (circle)
	{
		// 3. 0x43AEE8..0x43AF74: a_i accumulated (fadd), x = sin a x R + c.x, z = cos a x R + c.z (the mirror of the
		// mesh branch, literal), y = GetAltitude(MapCoords(ftol(x x 65536 x 0.1), ftol(z x 65536 x 0.1))); no post-pass
		float a = 0.0f;
		for (auto& entry : ring)
		{
			const float x = std::sin(a) * radius + centre.x;
			const float z = std::cos(a) * radius + centre.z;
			// the two products before the ftol (0x43AF18 / 0x43AF1E), each rounded to float: the FPU runs at 24-bit
			// precision (fn_007DEE00, 0x7DEE0D)
			const map_coords::MapCoords at {map_coords::FtoL(x * 65536.0f * 0.1f), map_coords::FtoL(z * 65536.0f * 0.1f),
			                                0.0f};
			entry = glm::vec3(x, map_coords::ToWorld(at).y, z);
			a = a + k_RingStep;
		}
		return;
	}
	// 4. 0x43AF77..0x43B3EA: every triangle of every primitive of every sub-mesh, in file order
	for (const auto& sub : mesh.GetSubMeshes())
	{
		const auto& positions = sub->GetCollisionPositions();
		const auto& indices = sub->GetCollisionIndices();
		for (const auto& [first, count] : sub->GetCollisionRanges())
		{
			for (uint32_t k = first; k + 2 < first + count && k + 2 < indices.size(); k += 3)
			{
				const std::array<glm::vec3, 3> v {positions.at(indices.at(k)), positions.at(indices.at(k + 1)),
				                                  positions.at(indices.at(k + 2))};
				// the lowest (L) and the second lowest (S) by local y (0x43B014..0x43B0A4)
				size_t lo = 0;
				size_t second = 0;
				if (v[0].y <= v[1].y)
				{
					if (v[2].y <= v[0].y)
					{
						lo = 2;
						second = 0;
					}
					else
					{
						lo = 0;
						second = v[2].y <= v[1].y ? 2 : 1;
					}
				}
				else if (v[2].y <= v[1].y)
				{
					lo = 2;
					second = 1;
				}
				else
				{
					lo = 1;
					second = v[0].y <= v[2].y ? 0 : 2;
				}
				if (!(v[lo].y < k_LowEdgeY && v[second].y < k_LowEdgeY))
				{
					continue;
				}
				// to world (0x43B0D0..0x43B1F2), u = normalize(S - L) in 3D (0x43B1F6..0x43B29C)
				const glm::vec3 l = glm::vec3(toWorld * glm::vec4(v[lo], 1.0f));
				const glm::vec3 s = glm::vec3(toWorld * glm::vec4(v[second], 1.0f));
				const glm::vec3 d = s - l;
				const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
				if (length == 0.0f)
				{
					continue; // (openblack, guard) a degenerate edge: the original's u would be NaN and never hit
				}
				const glm::vec3 u = d / length;
				for (size_t i = 0; i < ring.size(); ++i)
				{
					// 0x43B2A2..0x43B316: a = (float)i x step (fild; fmul), P2 = (c.x + R cos a, c.y, c.z + R sin a)
					const float a = static_cast<float>(i) * k_RingStep;
					const glm::vec3 p2(centre.x + radius * std::cos(a), centre.y, centre.z + radius * std::sin(a));
					glm::vec3 hit;
					if (EdgeHit(l, s, u, centre, p2, hit))
					{
						const float hx = hit.x - centre.x;
						const float hz = hit.z - centre.z;
						const float q = hx * hx + hz * hz;
						if (q > best.at(i)) // test ah, 0x41 jne: ties keep the first
						{
							ring.at(i) = hit;
							best.at(i) = q;
						}
					}
				}
			}
		}
	}
	// 5. 0x43B3F0..0x43B4D2: each entry pushed 1 m outwards along its 3D direction from c (no hit: from the
	// translation)
	for (auto& entry : ring)
	{
		const glm::vec3 d = entry - centre;
		if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
		{
			const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			entry = centre + d * (l + 1.0f) / l;
		}
	}
}

/// fn_43CDB0 0x43CDB0 (both ctors): GetBuilding && its 3D object -> PosBuilder::Process(ring, +0x40, IsFootball vt
/// +0x464)
void ComputeRing(entt::entity site)
{
	auto* s = SiteComponent(site);
	const auto building = building_sites::GetBuilding(site);
	if (s == nullptr || building == entt::null)
	{
		return;
	}
	PosBuilderProcess(s->ring, building, abodes::TypeOf(building) == AbodeType::FootballPitch);
}

/// The ring entry as ArrivesAtBuildingSite / GetNearestEdge make it a MapCoords: (ftol(x x 6553.6), ftol(z x 6553.6),
/// 0)
map_coords::MapCoords RingCoords(const BuildingSite& s, size_t index)
{
	const auto& point = s.ring.at(index);
	return {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), 0.0f};
}

/// A pile's ToBeDeleted (vt +0xC): ecs::ToBeDeleted (out of the physics and the map cells, then the entity)
void DeletePile(entt::entity pile)
{
	ecs::ToBeDeleted(pile);
}
} // namespace

// =====================================================================================================================
// plans
// =====================================================================================================================

size_t plans::PlansOf(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->plannedAbodes.size() : 0;
}

plans::PlanIndex plans::AddPlanned(entt::entity town, PlannedAbode plan)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return 0;
	}
	// PlannedMultiMapFixed ctor 0x6487F6..0x6487FC: +0x3C = g_game +0x205A40; Town::AddPlanned 0x73D080: the tail
	plan.creationTurn = game_clock::Turn();
	t->plannedAbodes.push_back(plan);
	return t->plannedAbodes.size() - 1;
}

void plans::RemovePlanned(entt::entity town, PlanIndex plan)
{
	// Town::RemovePlanned 0x73D0D0: unlink, +0x9AC--. (TownStats remove fn_749B10: town_stats::Compute counts the list)
	if (auto* t = TownComponent(town); t != nullptr && plan < t->plannedAbodes.size())
	{
		t->plannedAbodes.erase(t->plannedAbodes.begin() + static_cast<std::ptrdiff_t>(plan));
	}
}

const GAbodeInfo* plans::InfoOf(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	const auto i = static_cast<size_t>(static_cast<int32_t>(t->plannedAbodes.at(plan).info));
	return i < infos.size() ? &infos.at(i) : nullptr;
}

AbodeType plans::GetAbodeType(entt::entity town, PlanIndex plan)
{
	// PlannedTownCitadelHeart 0x467E30 = 0x804 (bit 2: GetBestPlanned's mask 4 takes it)
	if (HeartInfoOf(town, plan) != nullptr)
	{
		return AbodeType::Citadel;
	}
	// PlannedAbode 0x4061E0 = info +0x120 (PlannedMultiMapFixed's 0x465570 = 1 is never a town's plan here)
	const auto* info = InfoOf(town, plan);
	return info != nullptr ? info->abodeType : AbodeType::General;
}

bool plans::IsCivic(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return false;
	}
	if (t->plannedAbodes.at(plan).citadelHeart)
	{
		return false; // PlannedTownCitadelHeart::IsCivic 0x467E10 (`xor eax, eax`)
	}
	if (t->plannedAbodes.at(plan).townCentre)
	{
		return true; // PlannedTownCentre::IsCivic 0x55DBE0
	}
	return town_stats::IsCivic(GetAbodeType(town, plan)); // PlannedAbode 0x4060C0
}

float plans::GetDesireToBeRepaired(entt::entity town, PlanIndex plan)
{
	// 0x648910: +0x30 ? info +0x118 : 0.0 (a PlannedTownCitadelHeart's info: its GCitadelHeartInfo)
	const auto* t = TownComponent(town);
	if (const auto* heart = HeartInfoOf(town, plan); heart != nullptr)
	{
		return t->plannedAbodes.at(plan).wasBuilt ? heart->desireToBeRepaired : 0.0f;
	}
	const auto* info = InfoOf(town, plan);
	if (t == nullptr || info == nullptr || !t->plannedAbodes.at(plan).wasBuilt)
	{
		return 0.0f;
	}
	return info->desireToBeRepaired;
}

float plans::GetDesireToBeBuilt(entt::entity town, const GAbodeInfo& info, uint32_t scaffolds)
{
	return GetDesireToBeBuilt(town, info, info.abodeType, &info, scaffolds);
}

float plans::GetDesireToBeBuilt(entt::entity town, const GMultiMapFixedInfo& info, AbodeType abodeType,
                                const GAbodeInfo* abode, uint32_t scaffolds)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return 0.0f;
	}
	// 0x73A1AC b = info +0x114; 0x73A1BF type = info->GetAbodeType() (vt +0x40: a GAbodeInfo's +0x120, the citadel
	// heart's 0x464380 = 0x804)
	float b = info.desireToBeBuilt;
	const auto type = static_cast<uint32_t>(abodeType);
	// 0x73A1C2..0x73A1EC: s = the sites (+0x790) whose building's GetAbodeType (vt +0x8C4) is the type
	uint32_t s = 0;
	for (const auto site : t->buildingSites)
	{
		const auto building = building_sites::GetBuilding(site);
		if (building == entt::null)
		{
			continue; // (openblack, guard) the original would call through a NULL building
		}
		// vt +0x8C4
		if (const auto other = abodes::TypeOf(building); other.has_value() && static_cast<uint32_t>(*other) == type)
		{
			++s;
		}
	}
	// 0x73A1EE..0x73A1FD: b == 0 -> b (fcom 0; test ah, 0x40)
	if (b == 0.0f)
	{
		return b;
	}
	const bool neutral = t->owner == PlayerNames::NEUTRAL; // GPlayer::IsNeutral 0x64AC00
	float r = 0.0f;
	bool divide = true; // the "skip /s" exits (to 0x73A56D) only skip the division
	switch (type)
	{
	case 0x2: // LIVING_QUARTERS 0x73A2EF
	{
		if (abode == nullptr)
		{
			break; // (openblack, guard) only a GAbodeInfo has this type
		}
		// freePlaces = stats +0x4C (town +0x65C) - town +0x76C (the homeless), signed
		const int32_t freePlaces = t->stats.freeAdultPlaces - static_cast<int32_t>(t->homelessVillagers.size());
		// u = (adults + children) / 10 + 1, unsigned (div by 0xCCCCCCCD >> 3)
		const uint32_t u = (t->stats.adults + t->stats.children) / 10 + 1;
		// a signed compare (jle 0x73A320)
		if (freePlaces > static_cast<int32_t>(u) && scaffolds == 0)
		{
			b = 0.0f;
		}
		else if (freePlaces < 0)
		{
			const float v = std::min(b - static_cast<float>(freePlaces) / static_cast<float>(u), k_HouseCap);
			const auto q = static_cast<uint32_t>(
			    map_coords::FtoL(-std::max(static_cast<float>(freePlaces), k_HouseShortageFloor)));
			const uint32_t maxVillagers = abode->maxVillagersInAbode; // +0x174
			// unsigned compare (jbe)
			const auto qf = static_cast<float>(q);
			const auto mf = static_cast<float>(maxVillagers);
			const float share = q > maxVillagers ? mf / qf * k_HouseShare : qf / mf * k_HouseShare + k_HouseShare;
			b = (share + k_HouseBase) * v;
		}
		// 0x73A3F4..0x73A43D: m = byte stats +0x108[GetAbodeNumber vt +0x44]; b -= b / max(m + 1, 10) x m
		const auto number = static_cast<size_t>(static_cast<int32_t>(abode->abodeNumber));
		const float m =
		    number < t->stats.abodesByNumber.size() ? static_cast<float>(t->stats.abodesByNumber.at(number)) : 0.0f;
		b = b - b / std::max(m + 1.0f, k_HouseCountFloor) * m;
		r = b;
		break;
	}
	case 0x14: // TOTEM 0x73A279
		if (neutral || TownTotem(town) != entt::null || s != 0)
		{
			r = 0.0f;
		}
		else if (t->centre == entt::null)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x24: // STORAGE_PIT 0x73A2BE: GetStoragePit 0x73B5B0
		if (town_queries::GetStoragePit(town) == entt::null && s == 0)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x44: // CRECHE 0x73A2E4: town +0x744
		if (t->creche == entt::null && s == 0)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x84: // WORKSHOP 0x73A23C: byte town +0x721 = stats +0x111 = abodes of number 9 (Workshop)
	{
		const float v = neutral ? 0.0f : b;
		if (t->stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Workshop)) > 0 || s != 0)
		{
			r = v * 0.5f;
		}
		else
		{
			r = v;
			divide = false;
		}
		break;
	}
	case 0x100: // WONDER 0x73A446
		if (scaffolds >= k_WonderScaffolds)
		{
			r = 1.0f;
		}
		else
		{
			// GetDesire(14) 0x73E400 against TownDesire::GetInfo(14) +0x18 DesireTriggersVillagerAction
			const float d = town_desire::GetDesire(town, k_WonderDesire);
			const auto& desireInfo = Locator::infoConstants::value().townDesire.at(static_cast<size_t>(k_WonderDesire));
			r = d < desireInfo.desireTriggersVillagerAction ? 0.0f : b * d;
		}
		break;
	case 0x204: // GRAVEYARD: town +0x748 (Town::GetGraveyard)
		r = graveyard::GetGraveyard(town) != entt::null ? 0.0f : b;
		break;
	case 0x404: // TOWN_CENTRE: +0x9A4, then the totem +0x740
		if (t->centre == entt::null && s == 0 && TownTotem(town) == entt::null)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x1004: // FOOTBALL: 0x73A502 returns 0 at once (no n-correction) when the football is off
		if constexpr (k_FootballEnabled)
		{
			if (TownFootball(town) == entt::null && s == 0)
			{
				r = b;
				divide = false;
			}
			break;
		}
		else
		{
			return 0.0f;
		}
	case 0x2004: // SPELL_DISPENSER: centre, storage pit, graveyard, creche and workshops
		r = (t->centre != entt::null && town_queries::GetStoragePit(town) != entt::null &&
		     graveyard::GetGraveyard(town) != entt::null && t->creche != entt::null &&
		     TownWorkshops(town) != entt::null)
		        ? b
		        : 0.0f;
		break;
	default: // 0x804 citadel, 0x4004 field, 0xA windmill, ...
		r = b;
		break;
	}
	// common 0x73A557: r / s
	if (divide && s != 0)
	{
		r = r / static_cast<float>(s);
	}
	// 0x73A571: c = (float)(uint64)(uint32)(n - info +0x10C) x 0.3 x r; r -= min(c, r). n below ScaffoldsRequired wraps
	// to a huge value and r becomes 0 (literal)
	if (scaffolds != 0)
	{
		const float c =
		    static_cast<float>(static_cast<uint32_t>(scaffolds - info.scaffoldsRequired)) * k_ScaffoldPenalty * r;
		r = r - std::min(c, r);
	}
	return r;
}

std::optional<plans::PlanIndex> plans::GetBestPlanned(entt::entity town, float& best, uint32_t mask)
{
	best = 0.0f;
	std::optional<PlanIndex> result;
	for (PlanIndex i = 0; i < PlansOf(town); ++i)
	{
		if ((static_cast<uint32_t>(GetAbodeType(town, i)) & mask) == 0) // vt +0x510
		{
			continue;
		}
		// GetDesireToBeBuilt(+0x40, 0): a PlannedTownCitadelHeart's info is its GCitadelHeartInfo (type 0x804)
		float d = 0.0f;
		if (const auto* heart = HeartInfoOf(town, i); heart != nullptr)
		{
			d = GetDesireToBeBuilt(town, *heart, AbodeType::Citadel, nullptr, 0);
		}
		else if (const auto* info = InfoOf(town, i); info != nullptr)
		{
			d = GetDesireToBeBuilt(town, *info, 0);
		}
		else
		{
			continue;
		}
		if (d > best) // test ah, 0x41 jne: strict, the first on ties
		{
			best = d;
			result = i;
		}
	}
	return result;
}

std::optional<plans::PlanIndex> plans::GetPlannedAtPos(entt::entity town, const map_coords::MapCoords& pos, float r,
                                                       bool onlyRebuild)
{
	const auto* t = TownComponent(town);
	// 0x73E4C6: best = r; +0x9AC == 0 -> 0
	float best = r;
	std::optional<PlanIndex> result;
	if (t == nullptr)
	{
		return result;
	}
	for (PlanIndex i = 0; i < t->plannedAbodes.size(); ++i)
	{
		const auto& plan = t->plannedAbodes.at(i);
		if (onlyRebuild && !plan.wasBuilt)
		{
			continue;
		}
		const auto* info = InfoOf(town, i);
		const auto* heart = HeartInfoOf(town, i);
		// fn_636E30(info, scale): scale x max(mesh +0x24, mesh +0x2C) of the info's mesh (vt +0x2C, MeshPack 0xE9FE34;
		// the citadel heart's GetMesh 0x464370 = info +0x124)
		float radius = 0.0f;
		if (heart != nullptr)
		{
			radius = object::MeshRadius2D(resources::HashIdentifier(heart->meshType), plan.scale);
		}
		else if (info != nullptr)
		{
			radius = object::MeshRadius2D(resources::HashIdentifier(info->meshId), plan.scale);
		}
		const float v = gutils::GetDistanceInMetres(pos, PlanCoords(plan)) - (radius + r);
		if (v <= best) // test ah, 0x41; je skips only v > best: the LAST one on ties
		{
			best = v;
			result = i;
		}
	}
	return result;
}

entt::entity plans::CreatePlanned(entt::entity town, PlanIndex plan, float life)
{
	// PlannedTownCitadelHeart::CreatePlanned 0x467EA0
	if (HeartInfoOf(town, plan) != nullptr)
	{
		return archetypes::CitadelArchetype::CreatePlanned(town, plan, life);
	}
	const auto* t = TownComponent(town);
	const auto* info = InfoOf(town, plan);
	if (t == nullptr || info == nullptr)
	{
		return entt::null;
	}
	// 0x405710: GAbodeInfo::IsOkToCreateAtPos(info, &+0x14, +0x28 angle, GetScale vt +0x120, +0x48 town) 0x404B10
	const auto& p = t->plannedAbodes.at(plan);
	if (!town_placement::IsOkToCreateAtPos(*info, PlanCoords(p), p.yAngleRadians, p.scale, town))
	{
		return entt::null;
	}
	return CreatePlannedNoFixedCheck(town, plan, life); // vt +0x504
}

entt::entity plans::CreatePlannedNoFixedCheck(entt::entity town, PlanIndex plan, [[maybe_unused]] float life)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return entt::null;
	}
	// PlannedTownCitadelHeart::CreatePlannedNoFixedCheck 0x467EF0 (the life passed on to CitadelHeart::Create)
	if (t->plannedAbodes.at(plan).citadelHeart)
	{
		return archetypes::CitadelArchetype::CreatePlannedNoFixedCheck(town, plan, life);
	}
	const PlannedAbode p = t->plannedAbodes.at(plan);
	const uint32_t townId = t->id;
	// 1. PlannedAbode
	// 0x4057AC: Abode::Create(&+0x14, info, town, angle, GetScale, food 0, wood 0, life, 1, 1) 0x402E20 -> the class
	// ctor -> MultiMapFixed ctor 0x52E1E0(..., percent = life, underConstruction = 1), Abode::Init (food and wood 0; no
	// MakeFunctional as it is not built) and CreateAbodeSurroundingObjects 0x403E00. PlannedTownCentre 0x744550:
	// TownCentre::Create(pos, info, town, angle, scale, life, 1) 0x743C90 directly. (approximate) both are
	// AbodeArchetype::Create under construction (it has no Init / surrounding-objects split)
	const auto building =
	    archetypes::AbodeArchetype::Create(townId, p.position, p.info, p.yAngleRadians, p.scale, 0, 0, true);
	// 2. none -> 0 (the plan survives)
	if (building == entt::null)
	{
		return entt::null;
	}
	// 3. PostCreatePlanned 0x648C50: the footpath link moves to the building (TODO(footpaths): not ported); with the
	//    plan's town, Town::CheckWhenNewBuildingCreated 0x741500
	CheckWhenNewBuildingCreated(town, building);
	// 4. 0x4057C5..0x4057CC (PlannedAbode only, not PlannedTownCentre): +0x30 -> building +0x58 |= 4
	if (!p.townCentre && p.wasBuilt)
	{
		if (auto* a = Entities().TryGet<Abode>(building); a != nullptr)
		{
			a->buildFlags |= Abode::k_NotRepaired;
		}
	}
	// 5. plan->ToBeDeleted(0) (vt +0xC) -> Town::RemovePlanned
	RemovePlanned(town, plan);
	return building;
}

std::optional<plans::PlanIndex> plans::CreateFromBuilding(entt::entity town, entt::entity building)
{
	auto& registry = Entities();
	const auto* a =
	    building != entt::null && registry.Valid(building) ? registry.TryGet<const Abode>(building) : nullptr;
	const auto* info = a != nullptr ? abodes::InfoOf(building) : nullptr;
	const auto* transform = a != nullptr ? registry.TryGet<const Transform>(building) : nullptr;
	// the record's number in info.dat (abodes::InfoOf answers a record of this table)
	const auto& infos = Locator::infoConstants::value().abode;
	std::optional<size_t> index;
	for (size_t i = 0; info != nullptr && i < infos.size(); ++i)
	{
		if (&infos.at(i) == info)
		{
			index = i;
			break;
		}
	}
	// (openblack, guard) the original only fails on allocation (`new 0x4C`, Abode.cpp line 0x560, 0x405662)
	if (!index.has_value() || transform == nullptr)
	{
		return std::nullopt;
	}
	// PlannedAbode(Abode*) 0x405580 -> PlannedMultiMapFixed(MultiMapFixed*) 0x648820 (+0x48 = 0, then Init's town)
	PlannedAbode plan {};
	plan.info = static_cast<AbodeInfo>(*index);              // +0x40 = b +0x28 (0x64889F)
	plan.position = transform->position;                     // +0x14 = b +0x14 (0x648875..0x648882)
	plan.yAngleRadians = map_cells::detail::YAngleOf(transform->rotation); // +0x28 = GetYAngle (vt +0x508, 0x648889)
	plan.scale = object::GetScale(building);                              // +0x2C = GetScale (vt +0x120, 0x648896)
	// a PlannedAbode, not a PlannedTownCentre, whatever the building's class (0x405681)
	plan.townCentre = false;
	// 0x6488B4..0x6488CB: +0x30 = (b +0x58 & 8) ? 1 : 0, the built bit
	plan.wasBuilt = (a->buildFlags & Abode::k_Built) != 0;
	// 0x40568C..0x405696: +0x38 = b +0x64; b +0x64 = 0 (the footpath link). TODO(footpaths): not ported
	// 0x40569D..0x4056A3: PlannedAbode::Init(b->GetTown()) 0x4055A0: +0x48 = town; town -> Town::AddPlanned 0x73D080
	// (the creation turn +0x3C, 0x6488AB, is written there)
	return AddPlanned(town, plan);
}

// =====================================================================================================================
// building sites
// =====================================================================================================================

entt::entity building_sites::Create(entt::entity building)
{
	auto& registry = Entities();
	if (building == entt::null || !registry.Valid(building))
	{
		return entt::null;
	}
	// 0x43B7E0: GameThing ctor; +0x14..+0x2C, +0x30, +0x634, +0x63C, +0x640 = 0 (the component's defaults)
	const auto site = registry.Create();
	auto& s = registry.Assign<BuildingSite>(site);
	auto* a = registry.TryGet<Abode>(building);
	// +0x638 = (b +0x58 >> 2) & 1; +0x14 = b; b +0x74 = this (fn_52E3F0)
	s.isRepairSite = a != nullptr && (a->buildFlags & Abode::k_NotRepaired) != 0;
	s.root = building;
	if (a != nullptr)
	{
		a->buildingSite = site;
	}
	// a CitadelPart (the citadel heart, a worship site) keeps its +0x58 / +0x74 in components::CitadelPartBuild
	if (auto* part = registry.TryGet<CitadelPartBuild>(building); part != nullptr)
	{
		s.isRepairSite = (part->buildFlags & CitadelPartBuild::k_NotRepaired) != 0;
		part->buildingSite = site;
	}
	// +0x640 = GetLife (vt +0x11C); no DestructionMesh (vt +0x8B4) and IsBuilt -> 1.1 x life - 0.1
	const float life = life::LifeOf(building);
	s.repairBase = life;
	if (!abodes::HasDestructionMesh(building) && abodes::IsBuilt(building))
	{
		s.repairBase = k_RepairBaseScale * life - k_RepairBaseOffset;
	}
	// the ring (fn_43CDB0)
	ComputeRing(site);
	// (openblack) IsDrawBuilding holds from now on: the partly built model
	abodes::RedrawConstruction(building);
	// CitadelHeart::CreateBuildingSite 0x468DC0 (`new 0x65C`, CitadelHeart.cpp line 0x922): CitadelBuildingSite
	// (MultiMapFixed*) 0x43D1E0 = this ctor, then CreatePilesOfWood 0x43D2A0. Any other building (a worship site too:
	// MultiMapFixed::CreateBuildingSite 0x52F590) gets a StandardBuildingSite
	if (registry.AllOf<CitadelHeart>(building))
	{
		registry.Assign<CitadelBuildingSite>(site);
		CreatePilesOfWood(site);
	}
	return site;
}

void building_sites::ToBeDeleted(entt::entity site)
{
	auto& registry = Entities();
	// CitadelBuildingSite::ToBeDeleted 0x43D220 first, for each of the six slots: SetMultiMapFixed(0) (vt +0x868); with
	// wood (JustGetResource(WOOD) vt +0x94) and !(g_game +0x14 & 0x8000) Pot::SetupReaction 0x66D660, else its
	// ToBeDeleted; SetPileWood(0) (vt +0x10C, the no-op 0x43D180: the slots keep the piles, literal). Then
	// BuildingSite::ToBeDeleted 0x43B960 (below), whose pile step finds GetPileWood(0) = 0
	if (auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		citadelSite->pilesUnlinked = true;
		const auto piles = citadelSite->piles;
		for (const auto pile : piles)
		{
			if (pile == entt::null || !registry.Valid(pile))
			{
				continue; // (openblack, guard) a pile already gone
			}
			if (object_resources::GetResource(pile, ResourceType::Wood) != 0)
			{
				animal_ai::SetupPotReaction(pile);
			}
			else
			{
				DeletePile(pile);
			}
		}
	}
	auto* s = SiteComponent(site);
	// 1. +0xA & 1 -> return. (approximate) the bit is set first here (GameThing::ToBeDeleted sets it at step 9): the
	//    villagers' exit functions run below and may come back to this site
	if (s == nullptr || s->beingDeleted)
	{
		return;
	}
	s->beingDeleted = true;
	const auto root = s->root;
	// 2. out of g_game +0x205CAC (openblack keeps no global list)
	// 3. every town of g_game +0x205C84 that lists it: fn_73B990
	map_cells::ForEachTown([site](entt::entity town) {
		if (const auto* t = TownComponent(town);
		    t != nullptr && std::find(t->buildingSites.begin(), t->buildingSites.end(), site) != t->buildingSites.end())
		{
			RemoveBuildingSiteFromList(town, site);
		}
		return true;
	});
	// 4. each available builder, in list order: inside Get2DRadius + 2 -> SetupMoveToWithHug(out to that circle, 163)
	// 0x5F2890; else SetTopState(163) (vt +0x8E8). The villager's own +0xFC is its business (IsBuildingSiteValid fails)
	const auto builders = s->builders; // a copy: the villagers' exit functions may call RemoveBuilder
	for (const auto builder : builders)
	{
		// IsAvailable 0x43BA04 (Villager::IsAvailable 0x751D50: not being deleted, not DYING)
		if (!registry.Valid(builder) || !villager::IsAvailable(builder))
		{
			continue;
		}
		const auto rootPos = object::MapCoordsOf(root);
		const auto villagerPos = object::MapCoordsOf(builder);
		const float d = object::Get2DRadius(root) + k_BuilderClearance;
		// (openblack, guard) a root already destroyed has no position: SetTopState
		if (registry.Valid(root) && gutils::GetDistanceInMetres(villagerPos, rootPos) < d)
		{
			const auto p = rootPos + gutils::GetPosFromAngle(gutils::Get3DAngleFromXZ(rootPos, villagerPos), d);
			villager::SetupMoveToWithHug(builder, map_coords::ToMetres(p), VillagerStates::DecideWhatToDo);
		}
		else
		{
			villager::SetTopState(builder, VillagerStates::DecideWhatToDo);
		}
	}
	s = SiteComponent(site);
	if (s == nullptr)
	{
		return;
	}
	// 5. the list emptied (LHLinkedList::Remove 0x43DBB0 until empty); +0x634 is NOT reset
	s->builders.clear();
	// 6. every scaffold of +0x20 -> ToBeDeleted(0). (not ported) scaffolds
	// 7. the pile: not a workshop's (vt +0x310; not ported, never) -> SetMultiMapFixed(0) (vt +0x868), then with wood
	//    and !(g_game +0x14 & 0x8000) Pot::SetupReaction 0x66D660 (the villagers carry it away), else its ToBeDeleted;
	//    SetPileWood(0) in any case. (pending) the meaning of g_game +0x14 bit 0x8000: taken as clear
	if (const auto pile = s->woodPile; pile != entt::null)
	{
		if (registry.Valid(pile) && object_resources::GetResource(pile, ResourceType::Wood) != 0)
		{
			animal_ai::SetupPotReaction(pile);
		}
		else
		{
			DeletePile(pile);
		}
		if ((s = SiteComponent(site)) != nullptr)
		{
			s->woodPile = entt::null;
		}
	}
	// 8. root: +0x74 = 0 (fn_52E3F0), +0x14 = 0
	if (s != nullptr && root != entt::null)
	{
		if (auto* a = registry.Valid(root) ? registry.TryGet<Abode>(root) : nullptr; a != nullptr)
		{
			a->buildingSite = entt::null;
		}
		if (auto* part = registry.Valid(root) ? registry.TryGet<CitadelPartBuild>(root) : nullptr; part != nullptr)
		{
			part->buildingSite = entt::null;
		}
		s->root = entt::null;
		abodes::RedrawConstruction(root); // (openblack) IsDrawBuilding no longer holds
	}
	// 9. GameThing::ToBeDeleted 0x56FB70: FlushDeleted
}

void building_sites::FlushDeleted()
{
	auto& registry = Entities();
	std::vector<entt::entity> gone;
	registry.Each<const BuildingSite>([&gone](entt::entity site, const BuildingSite& s) {
		if (s.beingDeleted)
		{
			gone.push_back(site);
		}
	});
	for (const auto site : gone)
	{
		registry.Destroy(site);
	}
}

bool building_sites::IsAvailable(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && !s->beingDeleted;
}

void building_sites::Process(entt::entity site)
{
	// CitadelBuildingSite::Process 0x43D660: each slot whose pile's IsAvailable (vt +0x2C) != 1 -> 0. (inferred) not
	// reached in the original: CitadelHeart::Process 0x4665A0 does not call MultiMapFixed::Process 0x52F700, the only
	// caller of a site's Process found
	if (auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		for (auto& pile : citadelSite->piles)
		{
			if (pile != entt::null && !Entities().Valid(pile))
			{
				pile = entt::null;
			}
		}
		return;
	}
	// StandardBuildingSite::Process 0x43D8D0: +0x644 && its IsAvailable != 1 -> +0x644 = 0
	if (auto* s = SiteComponent(site); s != nullptr && s->woodPile != entt::null && !Entities().Valid(s->woodPile))
	{
		s->woodPile = entt::null;
	}
}

const std::vector<entt::entity>& building_sites::SitesOf(entt::entity town)
{
	static const std::vector<entt::entity> k_None;
	const auto* t = TownComponent(town);
	return t != nullptr ? t->buildingSites : k_None;
}

bool building_sites::IsBuildingHappening(entt::entity town)
{
	return !SitesOf(town).empty(); // 0x73E2F0: +0x794 != 0
}

void building_sites::InsertBuildingSite(entt::entity town, entt::entity site)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// 0x73B910: already in +0x790 -> return (no pulse)
	if (std::find(t->buildingSites.begin(), t->buildingSites.end(), site) != t->buildingSites.end())
	{
		return;
	}
	// GetTown (vt +0x48) == this -> TownStats::Add(site) 0x749AA0 (+0x18, civic +0x20, wonder +0x2C, +0x100 +=
	// GetWoodForStats): town_stats::Compute counts the list. if (site): a node at the head, +0x794++
	if (site != entt::null)
	{
		t->buildingSites.insert(t->buildingSites.begin(), site);
	}
	// 0x73B96D..0x73B977: +0x5E8 = 1, +0x5EC = 0
	t->buildPulse = 1;
	t->buildPulsePrevious = 0;
}

void building_sites::RemoveBuildingSiteFromList(entt::entity town, entt::entity site)
{
	// fn_73B990: (TownStats remove fn_749B50 when GetTown == this: recomputed) every node of the site, +0x794-- each
	if (auto* t = TownComponent(town); t != nullptr)
	{
		auto& sites = t->buildingSites;
		sites.erase(std::remove(sites.begin(), sites.end(), site), sites.end());
	}
}

entt::entity building_sites::AddBuildingSite(entt::entity town, entt::entity building)
{
	// 0x73B8E0: building->CreateBuildingSite() (vt +0x4D4, Abode = MultiMapFixed 0x52F590), then the list insert
	const auto site = Create(building);
	InsertBuildingSite(town, site);
	return site;
}

entt::entity building_sites::AddBuildingSiteFromPlan(entt::entity town, plans::PlanIndex plan)
{
	// 0x73B860: b = plan->CreatePlanned(0.0) (vt +0x500); site = b->CreateBuildingSite(); AddBuildingSite(site)
	const auto building = plans::CreatePlanned(town, plan, 0.0f);
	if (building == entt::null)
	{
		return entt::null;
	}
	return AddBuildingSite(town, building);
}

entt::entity building_sites::AddBuildingSiteNoFixedCheck(entt::entity town, plans::PlanIndex plan)
{
	// 0x73B8A0: the same with CreatePlannedNoFixedCheck(0.0) (vt +0x504)
	const auto building = plans::CreatePlannedNoFixedCheck(town, plan, 0.0f);
	if (building == entt::null)
	{
		return entt::null;
	}
	return AddBuildingSite(town, building);
}

bool building_sites::RemoveBuildingSite(entt::entity town, entt::entity building)
{
	// 0x73BA20: the first node whose site's GetBuilding is b -> its ToBeDeleted(0) (vt +0xC)
	const auto site = GetBuildingSiteInList(town, building);
	if (site == entt::null)
	{
		return false;
	}
	ToBeDeleted(site);
	return true;
}

entt::entity building_sites::GetBuildingSiteInList(entt::entity town, entt::entity building)
{
	for (const auto site : SitesOf(town))
	{
		if (GetBuilding(site) == building)
		{
			return site;
		}
	}
	return entt::null;
}

bool building_sites::IsBuildingSiteValid(entt::entity town, entt::entity site)
{
	const auto& sites = SitesOf(town);
	if (std::find(sites.begin(), sites.end(), site) == sites.end())
	{
		return false;
	}
	const auto building = GetBuilding(site);
	return building != entt::null && !(abodes::IsBuilt(building) && abodes::IsRepaired(building));
}

entt::entity building_sites::GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos, bool includeFull)
{
	entt::entity best = entt::null;
	float score = k_BestSiteStart;
	const auto sites = SitesOf(town);
	for (const auto site : sites)
	{
		if (GetBuilding(site) == entt::null)
		{
			continue;
		}
		if (!NeedsBuilders(site) && !includeFull)
		{
			continue;
		}
		// 0x73CFB6: GetBuilding read again; 0 -> return 0 (abort)
		const auto building = GetBuilding(site);
		if (building == entt::null)
		{
			return entt::null;
		}
		const float w = GetDesireForVillagers(site) * k_SiteWeightScale + k_SiteWeightOffset;
		// b->GetNearestEdgeToPos(pos) vt +0x83C = Object 0x636DA0
		const auto edge = object::GetNearestEdgeToPos(building, pos);
		const float s = gutils::GetDistanceInMetres(pos, edge) * w;
		if (s < score)
		{
			score = s;
			best = site;
		}
	}
	return best;
}

entt::entity building_sites::GetBestRepairBuildingSite(entt::entity town)
{
	entt::entity best = entt::null;
	float bestDesire = 0.0f;
	for (const auto site : SitesOf(town))
	{
		const auto* s = SiteComponent(site);
		if (s == nullptr || !s->isRepairSite)
		{
			continue;
		}
		const float d = GetDesireToBeRepaired(site);
		if (d > bestDesire)
		{
			bestDesire = d;
			best = site;
		}
	}
	return best;
}

building_sites::TownRepairChoice building_sites::ChooseTownRepair(entt::entity town)
{
	TownRepairChoice choice;
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return choice;
	}
	// 0x747DE6..0x747DF3: best = 0
	float best = 0.0f;
	// 0x747DFD..0x747E28: each plan of +0x9A8 (next +0x44) with +0x30: vt +0x514 (0x648910) > best (fcom; test ah,
	// 0x41; jne: strictly, a NaN never wins)
	for (plans::PlanIndex i = 0; i < t->plannedAbodes.size(); ++i)
	{
		if (!t->plannedAbodes.at(i).wasBuilt)
		{
			continue;
		}
		const float v = plans::GetDesireToBeRepaired(town, i);
		if (v > best)
		{
			best = v;
			choice.plan = i;
		}
	}
	// 0x747E2A..0x747E68: each structure of +0x754 (next +0x9C) with no site (+0x74, 0x747E39) and no +0x58 bit 2
	// (0x747E3F): vt +0x8D8 (Abode::GetDesireToBeRepaired 0x406970) > the SAME best
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* a = Entities().TryGet<const Abode>(abode);
		if (a == nullptr || a->buildingSite != entt::null || (a->buildFlags & Abode::k_NotRepaired) != 0)
		{
			continue;
		}
		const float v = abodes::GetDesireToBeRepaired(abode);
		if (v > best)
		{
			best = v;
			choice.abode = abode;
		}
	}
	return choice;
}

void building_sites::ProcessTownRepairs(entt::entity town)
{
	const auto choice = ChooseTownRepair(town);
	// 0x747E6A..0x747E7F: an abode -> +0x58 |= 4 (0x747E6E), AddBuildingSite(MultiMapFixed*) 0x73B8E0 (its site copies
	// the bit: +0x638 = 1, a repair site for GetBestRepairBuildingSite) and return, also when a plan was better before
	if (choice.abode != entt::null)
	{
		if (auto* a = Entities().TryGet<Abode>(choice.abode); a != nullptr)
		{
			a->buildFlags |= Abode::k_NotRepaired;
		}
		AddBuildingSite(town, choice.abode);
		return;
	}
	// 0x747E80..0x747E87: else a plan -> AddBuildingSite(PlannedMultiMapFixed*) 0x73B860 (CreatePlanned WITH the fixed
	// check; a rebuild plan's building gets bit 2 at 0x4057CC, so its site is a repair site too)
	if (choice.plan.has_value())
	{
		AddBuildingSiteFromPlan(town, *choice.plan);
	}
}

bool building_sites::RequestBestPlanned(entt::entity town)
{
	// 0x73A650..0x73A684: GetBestPlanned(mask 4) -> AddBuildingSiteNoFixedCheck
	float best = 0.0f;
	const auto plan = plans::GetBestPlanned(town, best, k_CivicMask);
	return plan.has_value() && AddBuildingSiteNoFixedCheck(town, *plan) != entt::null;
}

bool building_sites::RequestANewAbode(entt::entity town, [[maybe_unused]] AbodeType unused)
{
	// 0x73B330: GetBestPlanned(mask 2) -> AddBuildingSite(plan) 0x73B860 (with the fixed check)
	float best = 0.0f;
	const auto plan = plans::GetBestPlanned(town, best, k_AbodeMask);
	return plan.has_value() && AddBuildingSiteFromPlan(town, *plan) != entt::null;
}

void building_sites::AddWoodUsedForBuilding(entt::entity town, uint32_t wood)
{
	if (auto* t = TownComponent(town); t != nullptr)
	{
		t->woodUsedForBuilding = t->woodUsedForBuilding + static_cast<float>(wood);
	}
}

void building_sites::ForceBuildingOfPlannedAtPos(const map_coords::MapCoords& pos, float desire)
{
	// 0x73E560: for each player (GetNextPlayerAndNeutral 0x550980) and each of its towns (+0xA50, next +0x75C)
	map_cells::ForEachTown([&pos, desire](entt::entity town) {
		if (const auto plan = plans::GetPlannedAtPos(town, pos, 1.0f, false); plan.has_value())
		{
			if (const auto site = AddBuildingSiteNoFixedCheck(town, *plan); site != entt::null)
			{
				SetDesireBoost(site, desire);
			}
		}
		return true;
	});
}

void building_sites::PruneSites(entt::entity town)
{
	// fn_43BD00 0x43BD00: each node, the next read first (a copy here)
	const auto sites = SitesOf(town);
	for (const auto site : sites)
	{
		const auto* s = SiteComponent(site);
		const auto root = s != nullptr ? s->root : entt::null;
		if (root == entt::null || !abode_queries::IsAvailable(root) ||
		    (abodes::IsBuilt(root) && abodes::IsRepaired(root)))
		{
			if (IsAvailable(site))
			{
				ToBeDeleted(site);
			}
		}
	}
}

building_sites::DesireInputs building_sites::DesireInputsOf(entt::entity town)
{
	DesireInputs in;
	for (const auto site : SitesOf(town))
	{
		in.siteDesires.push_back(GetDesireForVillagers(site));
		in.siteBuilders.push_back(static_cast<uint32_t>(GetBuilderCount(site)));
		in.sitePlaces.push_back(GetMaxBuilders(site));
	}
	for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
	{
		in.planRepairDesires.push_back(plans::GetDesireToBeRepaired(town, i));
	}
	return in;
}

// ---- one site
// --------------------------------------------------------------------------------------------------------

entt::entity building_sites::GetRootBuilding(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->root : entt::null;
}

entt::entity building_sites::GetBuilding(entt::entity site)
{
	// 0x43BC70: +0x14 && its IsAvailable (vt +0x2C) ? GetBuildingObject (vt +0x8BC = this) : 0
	const auto root = GetRootBuilding(site);
	return root != entt::null && abode_queries::IsAvailable(root) ? root : entt::null;
}

entt::entity building_sites::GetTown(entt::entity site)
{
	// 0x43C0B0: root ? root->GetTown() : 0. The citadel heart's and a worship site's GetTown are MultiMapFixed's
	// 0x4220A0 = 0: TownOf answers null for anything that is not an abode
	const auto root = GetRootBuilding(site);
	return root != entt::null && Entities().Valid(root) ? abode_villagers::TownOf(root) : entt::null;
}

int32_t building_sites::GetBuilderCount(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->builderCount : 0;
}

int32_t building_sites::GetMaxBuilders(entt::entity site)
{
	// fn_43BBD0: GetBuilding ? info +0x110 : 0
	const auto building = GetBuilding(site);
	const auto* info = building != entt::null ? abodes::MultiMapFixedInfoOf(building) : nullptr;
	return info != nullptr ? static_cast<int32_t>(info->maxVillagerNeededToBuild) : 0;
}

int32_t building_sites::GetBuildersNeeded(entt::entity site)
{
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return 0;
	}
	const auto root = GetRootBuilding(site);
	if (abodes::IsBuilt(root))
	{
		if (abodes::IsRepaired(root))
		{
			return 0;
		}
		if (abodes::GetDesireToBeRepaired(building) == 0.0f) // vt +0x8D8
		{
			return 0;
		}
	}
	return GetMaxBuilders(site) - GetBuilderCount(site);
}

bool building_sites::NeedsBuilders(entt::entity site)
{
	return GetBuildersNeeded(site) > 0; // fn_43BC60, signed
}

bool building_sites::IsBuilder(entt::entity site, entt::entity villager)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && std::find(s->builders.begin(), s->builders.end(), villager) != s->builders.end();
}

float building_sites::GetDesireForVillagers(entt::entity site)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr)
	{
		return 0.0f;
	}
	// 0x43BD70: clamp((float)GetBuildersNeeded / (float)fn_43BBD0 + +0x63C, 0, 1); a 0 maximum divides by 0 (literal)
	float r = static_cast<float>(GetBuildersNeeded(site)) / static_cast<float>(GetMaxBuilders(site)) + s->desireBoost;
	if (r < 0.0f)
	{
		r = 0.0f;
	}
	if (r > 1.0f)
	{
		r = 1.0f;
	}
	return r;
}

float building_sites::GetDesireToBeRepaired(entt::entity site)
{
	// 0x43BE00: GetBuilding ? GetDesireForVillagers x the ROOT's GetDesireToBeRepaired (vt +0x8D8) : 0
	if (GetBuilding(site) == entt::null)
	{
		return 0.0f;
	}
	return GetDesireForVillagers(site) * abodes::GetDesireToBeRepaired(GetRootBuilding(site));
}

float building_sites::GetClearAreaRadius(entt::entity site)
{
	// 0x43BDE0: GetBuilding()->Get2DRadius (vt +0x64) x 1.2. (openblack, guard) 0 without a building
	const auto building = GetBuilding(site);
	return building != entt::null ? object::Get2DRadius(building) * k_ClearAreaFactor : 0.0f;
}

float building_sites::GetPercentBuilt(entt::entity site)
{
	// 0x43BCB0: GetPercentRepaired (vt +0x884) x GetPercentBuilt (vt +0x880)
	const auto building = GetBuilding(site);
	return building != entt::null ? abodes::GetPercentRepaired(building) * abodes::GetPercentBuilt(building) : 0.0f;
}

float building_sites::GetRadius(entt::entity site)
{
	const auto building = GetBuilding(site);
	return building != entt::null ? object::GetRadius(building) : 0.0f; // 0x43D050: vt +0x60
}

float building_sites::GetWoodValue(entt::entity site)
{
	// 0x43C0C0: (float)(uint32) info +0x6C x GetScale (vt +0x120) / GetPlayer (vt +0x1C) +0x7C (TribalPower[5])
	const auto building = GetBuilding(site);
	const auto* info = building != entt::null ? abodes::MultiMapFixedInfoOf(building) : nullptr;
	if (info == nullptr)
	{
		return 0.0f;
	}
	// the site's GetPlayer (vt +0x1C, 0x43C0FE) is GameThing::GetPlayer 0x570130 = g_game +0x18 + 0xA60 x byte g_game
	// +0x205A5B: the neutral player, for every site
	const float power = magic::players::MagicOf(PlayerNames::NEUTRAL).tribalPower.at(5);
	return static_cast<float>(info->woodValue) * object::GetScale(building) / power;
}

float building_sites::GetWoodNeededToBuild(entt::entity site)
{
	// 0x43C5F0: (root IsBuilt ? 1 - GetBuilding()->GetLife : 1 - GetBuilding()->GetPercentBuilt) x GetWoodValue -
	// (float)(uint) GetResource(WOOD) (vt +0x98)
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return 0.0f; // (openblack, guard)
	}
	const float part = abodes::IsBuilt(GetRootBuilding(site)) ? 1.0f - life::LifeOf(building)
	                                                         : 1.0f - abodes::GetPercentBuilt(building);
	return part * GetWoodValue(site) - static_cast<float>(GetResource(site, ResourceType::Wood));
}

bool building_sites::IsRepairSite(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && s->isRepairSite;
}

float building_sites::GetRepairBase(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->repairBase : 0.0f;
}

void building_sites::SetRepairBase(entt::entity site, float base)
{
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->repairBase = base;
	}
}

bool building_sites::ShouldIGetWood(entt::entity site, entt::entity villager,
                                    const std::function<map_coords::MapCoords()>& resourceDropoffPos)
{
	auto& registry = Entities();
	const auto* s = SiteComponent(site);
	if (s == nullptr || !registry.Valid(villager))
	{
		return false;
	}
	// 0x43C68A: m = (float)(uint) GetResource(WOOD)
	float m = static_cast<float>(GetResource(site, ResourceType::Wood));
	const auto villagerPos = object::MapCoordsOf(villager);
	const auto rootPos = object::MapCoordsOf(s->root);
	// GetDistanceInMetres 0x74CD50 < 50 -> m == 0
	if (gutils::GetDistanceInMetres(villagerPos, rootPos) < k_CloseToSite)
	{
		return m == 0.0f;
	}
	// + each builder's carried wood (+0xF6, movsx)
	for (const auto builder : s->builders)
	{
		if (const auto* v = registry.Valid(builder) ? registry.TryGet<const Villager>(builder) : nullptr; v != nullptr)
		{
			m = m + static_cast<float>(static_cast<int32_t>(v->resourceHeld.at(1)));
		}
	}
	// GetWoodNeededToBuild <= m -> 0 (test ah, 0x41 jne)
	if (GetWoodNeededToBuild(site) <= m)
	{
		return false;
	}
	// r = m / (float)(uint64)((+0x1C + 1) x info +0x384) + (float)(int16) v +0xF6 / (float)(uint64) info +0x268; min 1.
	// Note +0x1C, the list count, not +0x634
	const auto& info = villager::InfoOf(villager);
	const auto* self = registry.TryGet<const Villager>(villager);
	const auto held = self != nullptr ? static_cast<float>(static_cast<int32_t>(self->resourceHeld.at(1))) : 0.0f;
	const auto wanted = static_cast<uint32_t>((s->builders.size() + 1) * info.amountOfWoodPerBuilderWanted);
	float r = m / static_cast<float>(static_cast<uint64_t>(wanted)) +
	          held / static_cast<float>(static_cast<uint64_t>(info.maxWoodCarried));
	r = r < 1.0f ? r : 1.0f;
	// 0x753E20 GetResourceDropoffPos(WOOD), only now. (openblack, tests only) no callback: the villager's own position
	const auto dropoff = resourceDropoffPos ? resourceDropoffPos() : villagerPos;
	// GetDistanceModifier 0x74F290 (5000)
	const float a =
	    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(rootPos, villagerPos), k_WoodDistanceMax) * r;
	const float b =
	    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(dropoff, villagerPos), k_WoodDistanceMax) * (1.0f - r);
	return b > a; // test ah, 0x41 jne -> 0
}

uint32_t building_sites::GetResource(entt::entity site, ResourceType type)
{
	// CitadelBuildingSite::GetResource 0x43D320: the sum of the six slots' JustGetResource(type, 0, 0) (vt +0x94; no
	// IsAvailable test: openblack skips a pile already gone)
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		uint32_t sum = 0;
		for (const auto pile : citadelSite->piles)
		{
			if (pile != entt::null && Entities().Valid(pile))
			{
				sum += object_resources::GetResource(pile, type);
			}
		}
		return sum;
	}
	// 0x43C5B0: pile ? pile->JustGetResource(type, 0, 0) (vt +0x94) : 0. A site pile answers its own amount through
	// object_resources::GetResource (PotStructure::GetResource 0x66EF47)
	const auto pile = GetPileWood(site, nullptr);
	return pile != entt::null ? object_resources::GetResource(pile, type) : 0;
}

uint32_t building_sites::GetWoodForStats(entt::entity site)
{
	return GetResource(site, ResourceType::Wood); // 0x43C5E0
}

uint32_t building_sites::AddResource(entt::entity site, ResourceType type, uint32_t amount,
                                     [[maybe_unused]] const map_coords::MapCoords* pos, bool poisoned)
{
	// CitadelBuildingSite::AddResource 0x43D360: pos NULL -> nothing (0x43D370); else the nearest slot
	// (GetPileWood(pos) vt +0x108), none -> CreatePilesOfWood and once more; JustAddResource(type, n, poisoned) (vt
	// +0x8C, any type); then GetTown (0 for the heart's site) +0x710 += it
	if (CitadelSiteComponent(site) != nullptr)
	{
		uint32_t taken = 0;
		if (pos != nullptr)
		{
			auto pile = GetPileWood(site, pos);
			if (pile == entt::null)
			{
				CreatePilesOfWood(site);
				pile = GetPileWood(site, pos);
			}
			if (pile != entt::null && Entities().Valid(pile))
			{
				taken = pot_resource::JustAddResource(pile, type, amount, poisoned);
			}
		}
		return taken;
	}
	// 0x43C490: added = 0; WOOD: no pile -> CreatePileWood; an available pile -> JustAddResource (vt +0x8C)
	uint32_t added = 0;
	if (type == ResourceType::Wood)
	{
		if (GetPileWood(site, nullptr) == entt::null)
		{
			CreatePileWood(site);
		}
		const auto pile = GetPileWood(site, nullptr);
		if (pile != entt::null && Entities().Valid(pile))
		{
			added = pot_resource::JustAddResource(pile, ResourceType::Wood, amount, poisoned);
		}
	}
	// with a town: town +0x710 (stats +0x100) += (float) added: town_stats::Compute sums the site piles (woodAtSites)
	return added;
}

uint32_t building_sites::RemoveResource(entt::entity site, ResourceType type, uint32_t amount,
                                        const map_coords::MapCoords* interfacePos)
{
	// CitadelBuildingSite::RemoveResource 0x43D3F0: with an interface (0x43D3F6) BuildingSite::RemoveResource 0x43C530
	// (WOOD: GetPileWood(IS->GetPos()), the nearest slot, JustRemoveResource); without one the slots in order, each
	// available pile JustRemoveResource(type, what is left, 0) (vt +0x90, any type) while something is left (0x43D422)
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		uint32_t removed = 0;
		if (interfacePos != nullptr)
		{
			if (const auto pile = GetPileWood(site, interfacePos);
			    type == ResourceType::Wood && pile != entt::null && Entities().Valid(pile))
			{
				removed = object_resources::JustRemoveFromPot(pile, amount);
			}
			return removed;
		}
		const auto piles = citadelSite->piles;
		uint32_t left = amount;
		for (const auto pile : piles)
		{
			if (left == 0)
			{
				break; // test edi, edi; jbe
			}
			if (pile != entt::null && Entities().Valid(pile))
			{
				const uint32_t n = object_resources::JustRemoveFromPot(pile, left);
				removed += n;
				left -= n;
			}
		}
		return removed;
	}
	// 0x43C530: WOOD: pile = GetPileWood(status ? status->vt +0x100 : 0) -> JustRemoveResource(WOOD, n, 0) (vt +0x90);
	// the stats' -= (float) removed: recomputed
	uint32_t removed = 0;
	if (type == ResourceType::Wood)
	{
		if (const auto pile = GetPileWood(site, nullptr); pile != entt::null)
		{
			removed = object_resources::JustRemoveFromPot(pile, amount);
		}
	}
	return removed;
}

void building_sites::BuildBy(entt::entity site, float amount)
{
	// fn_43D080: GetBuilding()->vt +0x900(x)
	if (const auto building = GetBuilding(site); building != entt::null)
	{
		abodes::BuildBy(building, amount);
	}
}

map_coords::MapCoords building_sites::GetNearestEdge(entt::entity site, float angle, int32_t& index)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr)
	{
		return {};
	}
	int32_t i = 0;
	// 0x43CE40: below -6 pi -> index 0
	if (!(angle < k_MinusSixPi))
	{
		if (angle > k_SixPi)
		{
			angle = glm::pi<float>();
		}
		else
		{
			// 0x43CE73..0x43CE91: + 2 pi while below 0; 0x43CE93..0x43CEB1: - 2 pi while above 2 pi (test ah, 0x41 jne:
			// 2 pi itself stays)
			while (angle < 0.0f)
			{
				angle += glm::two_pi<float>();
			}
			while (angle > glm::two_pi<float>())
			{
				angle -= glm::two_pi<float>();
			}
		}
		// angle == 0 -> 0; else ftol(angle x 1 / 2 pi x 128) & 0x7F (the cmp 0x80 / jge after the mask are dead).
		// Each step rounded to float, as the 24-bit FPU (fn_007DEE00) does through the 2 pi loops and both products
		// (0x43CE80..0x43CEC6)
		if (angle != 0.0f)
		{
			i = map_coords::FtoL(angle * k_InvTwoPi * static_cast<float>(BuildingSite::k_RingSize)) & k_RingMask;
		}
	}
	index = i;
	return RingCoords(*s, static_cast<size_t>(i));
}

map_coords::MapCoords building_sites::GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index)
{
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return {}; // (openblack, guard) the original reads GetBuilding()+0x14 unchecked
	}
	// 0x43CDEB: a = Get3DAngleFromXZ(building, villager) (building -> villager); 0x43CDF5: a + (GameFloatRand(pi / 2)
	// - pi / 4) (fsub [0x8C6C9C] then fadd a)
	const float a = gutils::Get3DAngleFromXZ(object::MapCoordsOf(building), object::MapCoordsOf(villager));
	// fsub then fadd, each rounded to float by the 24-bit FPU (fn_007DEE00), pushed as the float argument
	// (0x43CE1F..0x43CE2F)
	const float spread = game_random::GameFloatRand(k_RandomBuildSpread) - k_RandomBuildHalf;
	return GetNearestEdge(site, spread + a, index);
}

map_coords::MapCoords building_sites::GetNextPosFromIndex(entt::entity site, int32_t& index)
{
	const auto* s = SiteComponent(site);
	const auto building = GetBuilding(site);
	if (s == nullptr || building == entt::null)
	{
		return {}; // 0x43CF40: no building -> (0, 0, 0)
	}
	// step = 2.0 / (Get2DRadius x 2 pi x 0.0078125); k = ftol(GameFloatRand(step x 0.5) + step) (line 0x3B7).
	// the product and the fdivr each rounded to float (24-bit FPU, fn_007DEE00; fstp 0x43CF9D)
	const float step = k_NextPosMetres / (object::Get2DRadius(building) * glm::two_pi<float>() * k_NextPosPerEntry);
	const int32_t k = map_coords::FtoL(game_random::GameFloatRand(step * 0.5f) + step);
	// sgn = GameRand(2) ? +1 : -1 (line 0x3B8; neg / sbb / and 2 / dec)
	const int32_t sign = game_random::GameRand(2) != 0 ? 1 : -1;
	// i = *idx + sgn x k, one wrap
	int32_t i = index + sign * k;
	const auto size = static_cast<int32_t>(BuildingSite::k_RingSize);
	if (i >= size)
	{
		i -= size;
	}
	else if (i < 0)
	{
		i += size;
	}
	index = i;
	// (openblack, guard) a k of 128 or more leaves i outside the ring after the one wrap: the original reads past it
	const auto read = static_cast<size_t>(((i % size) + size) % size);
	return RingCoords(*s, read);
}

std::optional<map_coords::MapCoords> building_sites::GetBuildPos(entt::entity site, int32_t index)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr || index < 0 || index >= static_cast<int32_t>(BuildingSite::k_RingSize))
	{
		return std::nullopt;
	}
	return RingCoords(*s, static_cast<size_t>(index));
}

void building_sites::AddBuilder(entt::entity site, entt::entity villager)
{
	// 0x43BE40: the duplicate search uses nothing it finds (dead loop): a node at the head, +0x1C++, +0x634++
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->builders.insert(s->builders.begin(), villager);
		++s->builderCount;
	}
}

void building_sites::RemoveBuilder(entt::entity site, entt::entity villager)
{
	// 0x43BE90: every node of the villager (+0x1C-- each), then +0x634-- once (also when not found / empty)
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->builders.erase(std::remove(s->builders.begin(), s->builders.end(), villager), s->builders.end());
		--s->builderCount;
	}
}

entt::entity building_sites::GetPileWood(entt::entity site, const map_coords::MapCoords* pos)
{
	// CitadelBuildingSite::GetPileWood 0x43D500: pos NULL -> 0; else the slot nearest to pos, GetDistance(pos, pile
	// +0x14) (fn_74CCE0) as (float)(uint32) strictly below the best (FLT_MAX [0x8C6B50]: the first on ties; no
	// IsAvailable test)
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		if (pos == nullptr)
		{
			return entt::null;
		}
		entt::entity nearest = entt::null;
		float best = std::numeric_limits<float>::max();
		for (const auto pile : citadelSite->piles)
		{
			if (pile == entt::null || !Entities().Valid(pile))
			{
				continue; // (openblack, guard) a pile already gone
			}
			const auto whole = static_cast<uint32_t>(gutils::GetDistance(*pos, object::MapCoordsOf(pile)));
			const auto d = static_cast<float>(whole);
			if (d < best)
			{
				best = d;
				nearest = pile;
			}
		}
		return nearest;
	}
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->woodPile : entt::null; // 0x43D6E0: +0x644
}

entt::entity building_sites::SiteOfPile(entt::entity pile)
{
	if (pile == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	Entities().Each<const BuildingSite>([pile, &found](entt::entity site, const BuildingSite& s) {
		if (s.woodPile == pile)
		{
			found = site;
		}
	});
	// CitadelBuildingSite::IsLinkedToThisBuildingSite 0x43D580: one of the six slots, while the piles are still linked
	// to the heart (ToBeDeleted 0x43D220's SetMultiMapFixed(0) unlinks them)
	if (found == entt::null)
	{
		Entities().Each<const CitadelBuildingSite>([pile, &found](entt::entity site, const CitadelBuildingSite& s) {
			if (!s.pilesUnlinked && std::find(s.piles.begin(), s.piles.end(), pile) != s.piles.end())
			{
				found = site;
			}
		});
	}
	return found;
}

void building_sites::CreatePileWood(entt::entity site)
{
	auto* s = SiteComponent(site);
	// 0x43D760: a StandardBuildingSite (dynamic_cast), no pile (GetPileWood(0)) and IsAvailable
	if (s == nullptr || CitadelSiteComponent(site) != nullptr || s->woodPile != entt::null || !IsAvailable(site))
	{
		return;
	}
	// GetResourcePosAndYAngle(&pos, WOOD, -1, &angle) (vt +0x114; angle starts at 0)
	float angle = 0.0f;
	const auto pos = GetResourcePosAndYAngle(site, ResourceType::Wood, -1, &angle);
	// Pot::Create 0x66CF10(pos, GPotInfo 0xD4D1C4 (9 "Magic Wood"), 0, GetBuilding(), town 0, 0, angle, 1.0, 1): its
	// MagicWood branch (0x66CF87..0x66CF97) passes only (pos, player 0, amount, building, flag) to the MagicWood ctor
	// 0x600E20, which makes the PileResource with angle 0 and scale 1 (0x600E30..0x600E48): the Y angle is computed
	// (and its random draw consumed) and dropped
	const auto pile = magic::objects::CreateMagicWood(map_coords::ToWorld(pos), std::nullopt, 0, true);
	if ((s = SiteComponent(site)) != nullptr)
	{
		s->woodPile = pile; // SetPileWood (vt +0x10C)
	}
}

map_coords::MapCoords building_sites::GetResourcePosAndYAngle(entt::entity site, ResourceType type,
                                                              [[maybe_unused]] int32_t index, float* angle)
{
	auto& registry = Entities();
	const auto root = GetRootBuilding(site);
	const auto rootPos = object::MapCoordsOf(root);
	// CitadelBuildingSite::GetResourcePosAndYAngle 0x43D470 (any type): root->GetCitadel() (vt +0x114) ->
	// GetWorshipSiteAngle(index) 0x463610 - 1.1424, the root's MapCoords + GetPosFromAngle(that, 22.0) 0x74D580; the
	// angle out-parameter is not written
	if (CitadelSiteComponent(site) != nullptr)
	{
		// GetWorshipSiteAngle: the citadel's heart (+0x30) Y angle (vt +0x508) + (fild, unsigned) index x 2 pi / 7;
		// fsub 1.1424, fstp (0x43D491..0x43D499). The game thread's x87 is at 24 bits (fn_007DEE00 0x7DEE0D): float
		// steps
		float heartAngle = 0.0f;
		const auto* heart = registry.TryGet<const CitadelHeart>(root);
		if (heart != nullptr && registry.Valid(heart->citadel))
		{
			if (const auto* worship = registry.TryGet<const CitadelWorship>(heart->citadel); worship != nullptr)
			{
				heartAngle = worship->heartYAngle;
			}
		}
		const auto slot = static_cast<float>(static_cast<uint32_t>(index));
		const float step = slot * k_WorshipSlotAngle;
		const float sum = heartAngle + step;
		const float a = sum - k_CitadelPileAngle;
		return rootPos + gutils::GetPosFromAngle(a, k_CitadelPileMetres);
	}
	// 0x43C220: not WOOD -> angle 0, the root's position
	if (type != ResourceType::Wood)
	{
		if (angle != nullptr)
		{
			*angle = 0.0f;
		}
		return rootPos;
	}
	// WOOD and root->IsWorshipSite() (vt +0x304, WorshipSite 0x55DCA0): the local point (9, 0, -50) through the root's
	// 3D object matrix (0x43C26A..0x43C2CF) as a MapCoords (0x603160); the angle (when asked) its GetYAngle (vt +0x508)
	if (const auto* worship = registry.TryGet<const WorshipSite>(root); worship != nullptr)
	{
		if (angle != nullptr)
		{
			*angle = worship->yAngle;
		}
		const auto* transform = registry.TryGet<const Transform>(root);
		const glm::vec3 point = transform != nullptr
		                            ? glm::vec3(lh_matrix::Model(*transform) * glm::vec4(k_WorshipSitePile, 1.0f))
		                            : glm::vec3(0.0f);
		return map_coords::FromWorld(point);
	}
	// WOOD with a pile: its position and Y angle
	if (const auto pile = GetPileWood(site, nullptr); pile != entt::null && registry.Valid(pile))
	{
		if (angle != nullptr)
		{
			const auto* transform = registry.TryGet<const Transform>(pile);
			*angle = transform != nullptr ? map_cells::detail::YAngleOf(transform->rotation) : 0.0f;
		}
		return object::MapCoordsOf(pile);
	}
	// WOOD without a pile (0x43C361..0x43C426; V6_pending §10): a = Get3DAngleFromXZ(door, p) (door -> centre), +
	// 0.392699
	// - GameFloatRand(0.785398) (after the angle); d = GetDistanceInMetres(p, door again) + 4; p + GetPosFromAngle(a,
	//   d); the Y angle a - pi ([0x8C36A0]). The pile is on the far side of the building from the door
	const auto doorAt = abode_queries::GetArrivePos(root); // MultiMapFixed::GetDoorPos 0x52E370 (vt +0x864)
	const map_coords::MapCoords door {doorAt.x, doorAt.y, 0.0f};
	float a = gutils::Get3DAngleFromXZ(door, rootPos);
	a = a + (k_PileHalfSpread - game_random::GameFloatRand(k_PileSpread));
	const auto doorAgain = abode_queries::GetArrivePos(root);
	const float d =
	    gutils::GetDistanceInMetres(rootPos, map_coords::MapCoords {doorAgain.x, doorAgain.y, 0.0f}) + k_PileBeyondDoor;
	if (angle != nullptr)
	{
		*angle = a - glm::pi<float>();
	}
	return rootPos + gutils::GetPosFromAngle(a, d);
}

bool building_sites::IsLinkedToThisBuildingSite(entt::entity site, entt::entity pot)
{
	if (pot == entt::null)
	{
		return false;
	}
	// CitadelBuildingSite 0x43D580: one of +0x644[0..5]
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		return std::find(citadelSite->piles.begin(), citadelSite->piles.end(), pot) != citadelSite->piles.end();
	}
	// StandardBuildingSite 0x43D830: +0x644; BuildingSite 0x43D0A0: 0
	const auto* s = SiteComponent(site);
	return s != nullptr && s->woodPile == pot;
}

void building_sites::CreatePilesOfWood(entt::entity site)
{
	if (CitadelSiteComponent(site) == nullptr)
	{
		return;
	}
	for (int32_t i = 0; i < static_cast<int32_t>(CitadelBuildingSite::k_Piles); ++i)
	{
		// GetResourcePosAndYAngle(&pos, WOOD, i, &angle = 0) (vt +0x114, 0x43D2C9), every slot
		float angle = 0.0f;
		const auto pos = GetResourcePosAndYAngle(site, ResourceType::Wood, i, &angle);
		const auto* citadelSite = CitadelSiteComponent(site);
		const auto old = citadelSite->piles.at(static_cast<size_t>(i));
		// a slot with an available pile (vt +0x2C == 1) keeps it
		if (old != entt::null && Entities().Valid(old))
		{
			continue;
		}
		// Pot::Create 0x66CF10(pos, GPotInfo 0xD4D1C4 (9 "Magic Wood"), 0, GetBuilding(), town 0, 0, angle, 1.0, 1)
		// (0x43D303): the MagicWood ctor 0x600E20 keeps neither the angle nor the scale (see CreatePileWood)
		const auto pile = magic::objects::CreateMagicWood(map_coords::ToWorld(pos), std::nullopt, 0, true);
		if (auto* again = CitadelSiteComponent(site); again != nullptr)
		{
			again->piles.at(static_cast<size_t>(i)) = pile;
		}
	}
}

void building_sites::SetDesireBoost(entt::entity site, float boost)
{
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->desireBoost = boost; // +0x63C (raw float)
	}
}
} // namespace openblack::ecs
