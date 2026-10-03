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
};

} // namespace openblack::ecs::components
