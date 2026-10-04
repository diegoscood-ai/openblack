/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Abode
{
	AbodeNumber type;
	uint32_t townId;
	// If a village does not have a ABODE_STORAGE_PIT then other abodes are used
	// by the villagers
	uint32_t foodAmount;
	uint32_t woodAmount;
	/// +0xA0 / +0xA4: the villagers of the abode, the head first (Abode::AddVillagerToAbode 0x40415A inserts at the head,
	/// next = villager +0xE4). Its order decides who moves in the shuffle (SwapMaleForFemaleFrom 0x407620,
	/// TakeVillagerFrom 0x4075B0) and who is the pair at bed time (CheckWhenGoingToBed 0x760BE4). Changed only by
	/// ecs::abode_villagers
	std::vector<entt::entity> inhabitants;
	/// +0xA8 / +0xAC MaleFemaleVillagers[sex]: the first adult of each sex that moved in (AddVillagerToAbode 0x4041E4);
	/// RemoveDeletedVillagerFromAbode 0x404246 clears both, RemoveAliveVillagerFromAbode none (literal)
	std::array<entt::entity, 2> maleFemale {entt::null, entt::null};
	/// +0xB0: the empty abode's clock (Abode::Process 0x4044B6: +0.001 a processed turn, ReduceLife at 1)
	float emptyTimer {0.0f};
	/// +0xB4 AdultCount / +0xB5 AdultMaleCount / +0xB7 ChildCount (AddVillagerToAbode, Remove*, ChildToAdult)
	uint8_t adultCount {0};
	uint8_t adultMaleCount {0};
	/// PresentAtHome (+0xB6): villagers inside now; only Abode::ArriveHome 0x405FA0 (++) and Abode::LeaveHome 0x405FB0 (--)
	/// change it (from Villager::ArriveHome 0x751FBC, ExitAtHome 0x761B5F / LeaveHome 0x751FFE). Lights the chimney
	/// smoke (Abode::Draw 0x516288) and the night windows (0x515F78)
	uint8_t presentAtHome {0};
	uint8_t childCount {0};
	/// +0xB9: counts up to 200 in each Abode::Process (0x404503..0x40450F); no reader found (P-8)
	uint8_t field0xB9 {0};

	// ---- the construction state (MultiMapFixed +0x58 / +0x5C / +0x74, Abode +0x7C, +0x28; V6_spec §3.1) ---------
	/// +0x58 bit 1: under construction (the ctor 0x52E1E0: (underConstruction & 1) << 1)
	static constexpr uint32_t k_UnderConstruction = 0x2;
	/// +0x58 bit 2: not repaired / a repair asked for (PlannedAbode::CreatePlannedNoFixedCheck 0x4057CC,
	/// ProcessTownRepairs 0x747E6E, MakeFunctional 0x404812 = !IsRepaired; cleared by MultiMapFixed::Repaired 0x52EC8D)
	static constexpr uint32_t k_NotRepaired = 0x4;
	/// +0x58 bit 3: built (the ctor without underConstruction, MultiMapFixed::Built 0x52EC3A)
	static constexpr uint32_t k_Built = 0x8;
	/// +0x58. (openblack) built by default: every abode made outside the plans (CREATE_ABODE, CREATE_TOWN_CENTRE, the
	/// fields, the spell dispenser) is whole; AbodeArchetype::Create(underConstruction) gives a plan's one bit 1
	uint32_t buildFlags {k_Built};
	/// +0x5C PercentBuilt (GetPercentBuilt 0x4014F0): 0 under construction, else the ctor's percent ((inferred) 1 for
	/// the whole ones); BuildBy 0x52ED40 / fn_52EDD0 move it, Built 0x52EBB0 sets 1
	float percentBuilt {1.0f};
	/// +0x74 the building site (fn_52E3F0 from the BuildingSite ctor 0x43B7E0; 0 in BuildingSite::ToBeDeleted
	/// 0x43B960): an entity with components::BuildingSite. IsDrawBuilding 0x52F0C0 = != 0
	entt::entity buildingSite {entt::null};
	/// +0x7C bit 1: TownStats::Add(Abode) 0x7498C0 has counted it (Abode::MakeFunctional 0x404818..0x40483C, once).
	/// (openblack) true by default for the whole abodes (Abode::Init 0x403130 -> MakeFunctional when IsBuilt); false
	/// for a plan's until its MakeFunctional. town_stats::Compute counts only the abodes that have it
	bool addedToTownStats {true};
	/// +0x28 the GAbodeInfo it was made with (AbodeArchetype::Create); None for the abodes made elsewhere (their record
	/// is looked up by number and mesh, town_stats::AbodeInfoOf)
	AbodeInfo info {AbodeInfo::None};
};

/// abodes::RedrawConstruction's state while the abode has a building site and no DestructionMesh (MultiMapFixed::Draw
/// 0x518090 -> DrawBuilding 0x517F90): the GetPercentForDrawBuilding its components::DrawMesh / NotDrawn was made for
struct AbodeConstructionDraw
{
	float percent {-1.0f};
};

} // namespace openblack::ecs::components
