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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"

namespace openblack::ecs::components
{

/// The miracle and worship fields of a Town (research sources.md §1.2), kept apart from components::Town. Assigned
/// by TownArchetype; Worship/TownMagic.cpp and Worship/WorshipPercentage.cpp own them.
struct TownMagic
{
	static constexpr size_t k_MagicTypes = 42;

	/// +0xDFC MagicTypesHeld[42]: the town holds the magic (Town::AddMagicTypesHeld 0x73D380)
	std::array<bool, k_MagicTypes> held {};
	/// +0x778 the TownSpellIcon list (the town centre's icons, TownCentreSpellIcon entities; fn_0073D1C0 pushes at the
	/// head, so the newest is first)
	std::vector<entt::entity> spellIcons;
	/// +0x98C the WorshipSite (Town::SetWorshipSite, WorshipSite::AddTown 0x77C800)
	entt::entity worshipSite {entt::null};
	/// +0x5C0 the worship percentage 0..1 the totem drag sets (Town::SetWorshipPercentage 0x73C060)
	float worshipPercentage {0.0f};
	/// +0x5C4 villagers at the site (dancing or hiding), counted by the site (fn_0073E3E0)
	int32_t worshipping {0};
	/// +0x5CC villagers on their way to it (AddVillagerOnWayToWorshipSite 0x73E300), and their list +0xDF4 / +0xDF8
	int32_t onWayToWorship {0};
	std::vector<entt::entity> onWayVillagers;
	/// +0x5F0 "the script forbids a worship site" (= !SET_CAN_BUILD_WORSHIPSITE)
	bool forbidWorshipSite {false};
	/// the deaths with reason 4 (worship), Town::GetDeathsFromWorshipping 0x740D60 (GET_TOWN_WORSHIP_DEATHS)
	int32_t deathsFromWorship {0};
};

/// The worship part of a TotemStatue (on its plinth's entity, next to components::TotemStatue): TotemStatue +0x80 the
/// percentage and +0x9C..+0xC8 the LH3DLib Zoomer that raises it (SetWorshipPercentage 0x738270, Draw 0x738960)
struct TotemWorship
{
	float percentage {0.0f}; ///< +0x80
	Zoomer rise {};          ///< +0x9C: the percentage moving to its new value in |change| x 5200 ms [0x999A98] (in ms)
};

} // namespace openblack::ecs::components
