/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CitadelArchetype.h"

#include <bit>
#include <cmath>

#include <spdlog/spdlog.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownPlacement.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/Citadel.h"
#include "Worship/SpecialPoints.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// fn_882AC0: LH3DMesh::CreateFromHD("Data\Citadel\OutsideMeshes\B_FIRST_TEMPL...", 0) 0x882AC7, the heart's mesh
/// (Game.cpp loads Data\Citadel\OutsideMeshes as "temple/<file>")
constexpr auto k_TempleMesh = entt::hashed_string("temple/b_first_temple_l3d");
/// CitadelHeart::CallVirtualFunctionsForCreation 0x4675A0 skips the creation index of the heart, its CitadelEntrance
/// (Object ctor 0x636520 from 0x4676CD) and its TempleLeash (CreateLeashes 0x464950): openblack makes the entrance
/// without one, and no leash
constexpr uint32_t k_HeartObjects = 3;

/// The land flattening of fn_882730 (LH3DCitadel vt +0x234, from CallVirtualFunctionsForCreation 0x46767F) 0x8827C7..
/// 0x882962: cx = ftol(x x 0.1) (+0xB8), cz = ftol(z x 0.1) (+0xBC); A = the altitude byte (+4) of the land cell (cx,
/// cz), 0 outside 0..0x1FF or without a block; for x = cx - 8..cx + 8 (outer), z = cz - 8..cz + 8: t = fn_8826C0(x -
/// cx, z - cz), a = the cell's altitude (0 outside), fn_800DA0(x, z, ftol(a x t + (1 - t) x A) & 0xFF). It runs when
/// the heart is created: at load for CREATE_CITADEL, at the plan's conversion for a planned citadel
void FlattenLandUnderTemple(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return; // no island loaded (tests)
	}
	auto& island = Locator::terrainSystem::value();
	// fld +0x40 / +0x38 (the 3D object's z / x: its MapCoords x 1/6553.6, SetPosition 0x423140); fmul 0.1 ([0x8AC404]);
	// __ftol 0x7A1400 (the x87 at 24 bits: a float product)
	const float x10 = ecs::map_coords::Quantise(position.x) * 0.1f;
	const float z10 = ecs::map_coords::Quantise(position.z) * 0.1f;
	const auto cx = static_cast<int>(x10);
	const auto cz = static_cast<int>(z10);
	// the original's bounds are 0..0x1FF (cmp 0x1FF; jg); openblack's island its cells per side
	const int last = island.GetCellsPerSide() - 1;
	const auto altitudeAt = [&island, last](int x, int z) {
		if (x < 0 || z < 0 || x > last || z > last)
		{
			return 0;
		}
		return static_cast<int>(island.GetCellAltitude(island.GetCell(glm::u16vec2(x, z))) & 0xFF);
	};
	const int centreAltitude = altitudeAt(cx, cz);
	int changed = 0;
	for (int x = cx - 8; x <= cx + 8; ++x)
	{
		for (int z = cz - 8; z <= cz + 8; ++z)
		{
			const auto value = CitadelArchetype::FlattenedAltitude(x - cx, z - cz, altitudeAt(x, z), centreAltitude);
			// fn_800DA0 writes every cell, those out of the map too. (pending) whether it clamps them: openblack's
			// island has no cell there to write
			if (x < 0 || z < 0 || x > last || z > last)
			{
				continue;
			}
			if (static_cast<int>(value) != altitudeAt(x, z))
			{
				island.SetCellAltitude(glm::u16vec2(x, z), value);
				++changed;
			}
		}
	}
	// (openblack) the land blocks, their physics and the height map follow the new altitudes
	island.RebuildAltitudes();
	// (openblack) RebuildAltitudes remakes the whole height map in this call (a frame hitch inside BUILD_BUILDING; the
	// original rewrites its land in place)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Temple at ({:.0f}, {:.0f}): land flattened to {} ({} cells changed)",
	                   position.x, position.z, centreAltitude, changed);
}

/// CitadelEntrance ctor 0x468EB0 (`new 0x68`, CitadelHeart.cpp line 0x5E0; from CallVirtualFunctionsForCreation
/// 0x46768F..0x467765): Object ctor 0x636520(pos, GObjectInfo 0xD41F68), +0x54..+0x64 = 0, vtable 0x8CA294; its 3D
/// object LH3DObject::Create(type 1) with the Entrance mesh, SetPosition at the heart's point (heart angle, scale 1),
/// vt +0x1E8; entrance +0x54 = heart. Its Draw (vt +0x610, 0x4648B0) is a bare `ret` and nothing puts it in the map
/// cells. (pending) its mesh (vt +0xF4 with the LH3DCitadel's vt +0x204 0x80BBC0, (inferred) Entrance.l3d, [0xFAA7E4]
/// 0x88297B) and how the hand reaches it: openblack keeps no model for it (a Mesh would also print a footprint)
entt::entity CreateEntrance(entt::entity heart, const glm::vec3& position, float yAngle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entrance = registry.Create();
	registry.Assign<Transform>(entrance, position, lh_matrix::AngleY(yAngle), glm::vec3(1.0f));
	registry.Assign<CitadelEntrance>(entrance, heart);
	return entrance;
}
} // namespace

uint16_t CitadelArchetype::FlattenedAltitude(int dx, int dz, int altitude, int centreAltitude)
{
	// fn_8826C0(dx, dz) 0x8826C0: d = sqrt((10 dx)^2 + (10 dz)^2) stored as a float; d > 70 ([0x92B2C8]; fcomp; test
	// ah, 0x41) -> 1; d < 35 ([0x9A3D7C]) -> 0; else (d - 35) x 0.0285714 ([0x9000D4] = 0x3CEA0EA1). The game thread's
	// x87 is at 24 bits (fn_007DEE00, `and 0xFCFF` 0x7DEE0D): every step rounds to a float, so d == 70 gives exactly 1
	const float x = static_cast<float>(dx) * 10.0f;
	const float z = static_cast<float>(dz) * 10.0f;
	const float xx = x * x;
	const float zz = z * z;
	const float d = std::sqrt(xx + zz);
	float t = 0.0f;
	if (d > 70.0f)
	{
		t = 1.0f;
	}
	else if (!(d < 35.0f))
	{
		const float over = d - 35.0f;
		t = over * std::bit_cast<float>(0x3CEA0EA1u);
	}
	// 0x882919..0x88292D: fild a; fmul t; fld 1; fsub t; fmul A (a float, fstp [esp + 0x10]); faddp; __ftol; & 0xFF
	const float at = static_cast<float>(altitude) * t;
	const float rest = 1.0f - t;
	const float centre = rest * static_cast<float>(centreAltitude);
	const float sum = at + centre;
	return static_cast<uint16_t>(static_cast<int32_t>(sum) & 0xFF);
}

const GCitadelHeartInfo& CitadelArchetype::HeartInfo(uint32_t heartInfo)
{
	if (heartInfo != 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Citadel: heart info {} read as 0 (openblack keeps one record)",
		                   heartInfo);
	}
	return Locator::infoConstants::value().citadelHeart;
}

entt::entity CitadelArchetype::CreateHeart(const glm::vec3& position, PlayerNames owner, entt::entity citadel,
                                           float yAngle, float scale, float life, bool underConstruction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = HeartInfo(0);
	// (openblack) the CitadelEntrance class's tap handlers, once
	RegisterTapHandlers();
	// 1. `new 0xE8` (CitadelHeart.cpp line 0x1CF) -> ctor 0x4649B0
	ecs::object_index::Skip(k_HeartObjects);
	const auto heart = registry.Create();
	// CallVirtualFunctionsForCreation's SetPosition 0x467648: the point of the MapCoords, the Y angle (vt +0x508) and
	// scale 1.0 (0x46761F; the plan's scale only feeds the influence)
	registry.Assign<Transform>(heart, position, lh_matrix::AngleY(yAngle), glm::vec3(1.0f));
	registry.Assign<Temple>(heart, owner);
	// MultiMapFixed ctor 0x52E1E0(pos, info, angle, scale, percent = life, underConstruction): 1 -> +0x58 bit 1, +0x5C
	// = 0 (bit 3 clear); 0 -> bit 3, +0x5C = life (0x52E20B..0x52E234)
	auto& part = registry.Assign<CitadelPartBuild>(heart);
	if (underConstruction)
	{
		part.buildFlags = CitadelPartBuild::k_UnderConstruction;
		part.percentBuilt = 0.0f;
	}
	else
	{
		part.buildFlags = CitadelPartBuild::k_Built;
		part.percentBuilt = life;
	}
	// CitadelPart ctor 0x4693F0: +0x80 = citadel (0x469443), SetLife(info +0x128 StartLife) (0x469452), +0x7C =
	// info +0x11C, the part at the head of citadel +0x4C. Citadel ctor 0x462B10 when the player has none (from
	// 0x467F3D): openblack keeps the Citadel on this entity (CitadelWorship); the heart ctor 0x4649F9..0x464A4D makes
	// it the citadel's heart (+0x30) when it has none, and +0x6C its influence (InfluenceSources' CitadelInfluence)
	auto& component = registry.Assign<CitadelHeart>(heart);
	component.citadel = citadel != entt::null ? citadel : heart;
	component.scale = scale;
	ecs::life::SetLife(heart, info.startLife);
	if (citadel == entt::null)
	{
		worship::citadel::Initialise(heart, yAngle);
	}
	// 2. CallVirtualFunctionsForCreation vt +0x658 = 0x4675A0 (skipped when the 3D object +0x40 exists: never here)
	//    2.1 +0x40 = LH3DObject::Create(Get3DType 0x464B40 = 8) 0x80B4D0; SetPosition (above)
	//    2.2 +0x40 vt +0x234 = fn_882730(point, GetPercentBuilt (vt +0x880), GetRemapedPlayer): the mesh, +0x9C = 0,
	//        then SetPercent 0x883120 (vt +0x200) with that percent; the land flattening
	registry.Assign<Mesh>(heart, k_TempleMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	worship::citadel::SetHeartDrawPercent(heart, registry.Get<const CitadelPartBuild>(heart).percentBuilt);
	FlattenLandUnderTemple(position);
	//    2.3 InitialiseIsFixedForMapList vt +0x850 (map_cells' own)
	//    2.4 the entrance +0xDC (0x46768F..0x467765)
	registry.Get<CitadelHeart>(heart).entrance = CreateEntrance(heart, position, yAngle);
	//    2.5 InsertMapObject vt +0x544 (0x46776C). (approximate) its collide shape (CreateCollideData 0x468FB0) is not
	//        ported: map_cells takes the mesh's
	ecs::map_cells::InsertMapObject(heart);
	//    2.6 (pending) +0xE0 fn_454960(2, ..., 15.0, ...), the global heart list [0xC5E3D4], +0x90 fn_8499C0,
	//        CreateLeashes 0x464950, the +0xA8 object; fn_468DB0 (0x467856)
	// 3. heart +0x98 = fn_4651D0: the alignment bird flock (Flock 0x52F780, GFlockInfo 0xC5E624). (not ported)
	// 4. life >= 1 (fcomp 1.0; test ah, 1; jne) -> fn_464F50(citadel, 0): CREATE_CITADEL; never a plan's conversion
	if (!(life < 1.0f))
	{
		worship::citadel::OpenWorshipSites(registry.Get<const CitadelHeart>(heart).citadel, 0.0f);
	}
	return heart;
}

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
                                      [[maybe_unused]] const glm::vec3& size)
{
	// Citadel::CreateCitadel 0x463240: a new Citadel and CitadelHeart::Create(pos, info, citadel, angle, scale 1.0,
	// life 1.0, 0)
	return CreateHeart(position, playerOwner, entt::null, worship::YAngleOf(glm::mat3(rotation)), 1.0f, 1.0f, false);
}

void CitadelArchetype::CreatePlan(entt::entity town, const glm::vec3& position, uint32_t heartInfo, float yAngle,
                                  float scale)
{
	// ctor 0x467DD0: PlannedMultiMapFixed ctor 0x648780 (+0x14 pos, +0x28 angle, +0x2C scale, +0x30 = 0, +0x38 = 0,
	// +0x3C the creation turn, +0x40 info, +0x44 = 0), vtable 0x8C9D4C, +0x48 = town; with a town Town::AddPlanned
	// 0x73D080
	if (town == entt::null)
	{
		return;
	}
	PlannedAbode plan {AbodeInfo::None, position, yAngle, scale, false};
	plan.citadelHeart = true;
	plan.heartInfo = heartInfo;
	ecs::plans::AddPlanned(town, plan);
}

entt::entity CitadelArchetype::CreatePlanned(entt::entity town, size_t plan, float life)
{
	const auto* t = Locator::entitiesRegistry::value().TryGet<const Town>(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return entt::null;
	}
	// 0x467EA0: fn_4695E0(info, &+0x14, +0x28 angle, GetScale vt +0x120) 0x467EC8 = MapCoords::IsSuitableForFixed(info
	// vt +0x2C GetMesh 0x464370 (= info +0x124), angle, scale) 0x6038B0 != 0 (neg; sbb; neg)
	const auto& p = t->plannedAbodes.at(plan);
	const auto& info = HeartInfo(p.heartInfo);
	const auto at = ecs::map_coords::FromMetres(glm::vec2(p.position.x, p.position.z));
	const auto mesh = resources::HashIdentifier(info.meshType);
	if (!ecs::town_placement::IsSuitableForFixed(at, mesh, p.yAngleRadians, p.scale))
	{
		return entt::null;
	}
	return CreatePlannedNoFixedCheck(town, plan, life); // vt +0x504 (0x467EDA)
}

entt::entity CitadelArchetype::CreatePlannedNoFixedCheck(entt::entity town, size_t plan, float life)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr || plan >= t->plannedAbodes.size() || !t->plannedAbodes.at(plan).citadelHeart)
	{
		return entt::null;
	}
	const PlannedAbode p = t->plannedAbodes.at(plan);
	// 1. player = +0x48->GetPlayer() (town vt +0x1C, 0x467EFB; +0x48 used without a NULL check, 0x467EF5); none -> 0
	//    (0x467F02). openblack's towns always have an owner (the neutral player too)
	const auto player = t->owner;
	// 2. citadel = player +0xA48 (0x467F08); none -> `new 0x80` (CitadelHeart.cpp line 0x714) Citadel ctor
	//    0x462B10(&+0x14, GCitadelInfo 0xC5E1E8, player): made with the heart below
	const auto citadel = worship::citadel::Of(player);
	// 3. CitadelHeart::Create(&+0x14, +0x40, citadel, +0x28 angle, GetScale, life, 1) 0x464E20 (0x467F72); none -> 0
	const auto heart = CreateHeart(p.position, player, citadel, p.yAngleRadians, p.scale, life, true);
	if (heart == entt::null)
	{
		return entt::null;
	}
	// 4. heart +0x94 = +0x48 (0x467F83)
	registry.Get<CitadelHeart>(heart).town = town;
	// 5. +0x30 -> heart +0x58 |= 4 (0x467F90: a rebuild plan's site is a repair site)
	if (p.wasBuilt)
	{
		registry.Get<CitadelPartBuild>(heart).buildFlags |= CitadelPartBuild::k_NotRepaired;
	}
	// 6. PostCreatePlanned 0x648C50 (0x467F99): heart +0x64 = plan +0x38 (footpaths: not ported); plan->GetTown()
	//    (GameThing 0x56FF10 = 0): no Town::CheckWhenNewBuildingCreated
	// 7. plan->ToBeDeleted(0) (vt +0xC 0x467E80 -> Town::RemovePlanned, 0x467FA5)
	ecs::plans::RemovePlanned(town, plan);
	// 8. the heart
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Citadel: the plan of town {} is a temple under construction ({})", t->id,
	                   static_cast<uint32_t>(heart));
	return heart;
}

void CitadelArchetype::RegisterTapHandlers()
{
	// once: hand_tap::Register appends without checking
	static bool s_Registered = false;
	if (s_Registered)
	{
		return;
	}
	s_Registered = true;
	// CitadelEntrance (vtable 0x8CA294): vt +0x740 0x468F50, vt +0x744 0x468EF0
	ecs::hand_tap::Register<CitadelEntrance>(
	    [](entt::entity entrance, const ecs::pot_resource::Dropper&) {
		    return worship::citadel::EntranceValidToTap(entrance);
	    },
	    [](entt::entity entrance, const ecs::pot_resource::Dropper& is, glm::vec3) {
		    return worship::citadel::EntranceTap(entrance, is.isMyInterface);
	    });
}
