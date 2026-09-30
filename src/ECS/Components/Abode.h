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
#include <set>

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
	/// Villager
	std::set<entt::entity> inhabitants;
	/// PresentAtHome (+0xB6): villagers inside now; only Abode::ArriveHome 0x405FA0 (++) and Abode::LeaveHome 0x405FB0 (--)
	/// change it (from Villager::ArrivesHome 0x760A93 and Villager::ExitAtHome 0x761B5F). Lights the chimney smoke
	/// (Abode::Draw 0x516288) and the night windows. Nothing sets it yet: the villagers never go home in openblack
	uint8_t presentAtHome {0};
};

} // namespace openblack::ecs::components
