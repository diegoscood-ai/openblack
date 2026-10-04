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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack
{
struct GCitadelHeartInfo;
}

// The citadel heart (CitadelHeart.cpp of runblack.exe W120; spec dev\documentacion\edificios\citadel_plan_spec.md): the
// script's finished temple (CREATE_CITADEL), the town's plan of one (CREATE_PLANNED_CITADEL, a PlannedTownCitadelHeart
// in Town::plannedAbodes) and its conversion into a temple under construction (0x467EF0), the temple's 3D side
// (CallVirtualFunctionsForCreation 0x4675A0: the land flattening, the CitadelEntrance) and the entrance's tap.
// The building side (BuildBy / Built, the CitadelBuildingSite) is in ecs::abodes / ecs::building_sites, the per-turn
// part in worship::citadel::Process.

namespace openblack::ecs::archetypes
{
class CitadelArchetype
{
public:
	/// CREATE_CITADEL: Citadel::CreateCitadel 0x463240 -> CitadelHeart::Create(pos, info, citadel, angle, scale 1.0,
	/// life 1.0, 0): a built temple (the script's size is ignored), and fn_464F50 as life >= 1
	static entt::entity Create(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
	                           const glm::vec3& size);
	/// CREATE_PLANNED_CITADEL (handler 0x715E91): PlannedTownCitadelHeart ctor 0x467DD0(pos, heartInfo, town, angle,
	/// scale): only a plan at the tail of the town's list (Town::AddPlanned 0x73D080); not drawn, not in the map cells,
	/// no land flattening, no creation index. `heartInfo` is the script's N3 (0xC5E270 + N3 x 0x158)
	static void CreatePlan(entt::entity town, const glm::vec3& position, uint32_t heartInfo, float yAngle, float scale);
	/// PlannedTownCitadelHeart::CreatePlanned 0x467EA0 (vt +0x500): fn_4695E0 (the heart info's IsOkToCreateAtPos:
	/// MapCoords::IsSuitableForFixed(GetMesh info +0x124, angle, scale) != 0) or null; else CreatePlannedNoFixedCheck
	static entt::entity CreatePlanned(entt::entity town, size_t plan, float life);
	/// PlannedTownCitadelHeart::CreatePlannedNoFixedCheck 0x467EF0 (vt +0x504): the town's player (none: null, the plan
	/// survives); its citadel (made with the heart when it has none, 0x462B10); CitadelHeart::Create(..., life, 1)
	/// under construction at 0 %; heart +0x94 = the plan's town; a rebuild plan's heart +0x58 |= 4; PostCreatePlanned
	/// 0x648C50 (no CheckWhenNewBuildingCreated: the plan's GetTown 0x56FF10 is 0); the plan deleted. Returns the heart
	/// or null
	static entt::entity CreatePlannedNoFixedCheck(entt::entity town, size_t plan, float life);
	/// CitadelHeart::Create 0x464E20(pos, info, citadel, angle, scale, life, underConstruction): the heart (ctor
	/// 0x4649B0 over CitadelPart 0x4693F0 over MultiMapFixed 0x52E1E0), its CallVirtualFunctionsForCreation 0x4675A0
	/// and, at life >= 1, fn_464F50(citadel, 0). `citadel` null: the player's citadel is made with this heart
	/// (openblack keeps the Citadel on its first heart's entity, components::CitadelWorship)
	static entt::entity CreateHeart(const glm::vec3& position, PlayerNames owner, entt::entity citadel, float yAngle,
	                                float scale, float life, bool underConstruction);
	/// fn_882730's blend for one land cell (0x8827C7..0x882962 with fn_8826C0 0x8826C0): the cell (dx, dz) from the
	/// heart's of altitude `altitude`, the heart's cell `centreAltitude`: ftol(a x t + (1 - t) x A) & 0xFF, in float
	[[nodiscard]] static uint16_t FlattenedAltitude(int dx, int dz, int altitude, int centreAltitude);
	/// The heart info of the script's N3: 0xC5E270 + N3 x 0x158. (approximate) openblack keeps the one record (N3 = 0,
	/// every shipped land); another N3 reads past it into the pens' records in the original
	[[nodiscard]] static const GCitadelHeartInfo& HeartInfo(uint32_t heartInfo);
	/// CitadelEntrance::InterfaceValidToTap 0x468F50 / InterfaceTap 0x468EF0 in ecs::hand_tap
	/// (Register<CitadelEntrance>). Once (later calls do nothing); CreateHeart calls it
	static void RegisterTapHandlers();
	CitadelArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
