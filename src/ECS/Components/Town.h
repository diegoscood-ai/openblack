/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A building the town plans to build (CREATE_PLANNED_ABODE 0x715629: PlannedTownCentre::Create 0x7444D0 when the
/// abode's type is 0x404 (TownCentre), else PlannedAbode::Create 0x405600; Town::AddPlanned). Only data: the planned
/// objects are invisible (PlannedMultiMapFixed::Draw 0x648930 is a bare `ret`) and nobody builds them yet.
struct PlannedAbode
{
	AbodeInfo info;
	glm::vec3 position;
	float yAngleRadians; ///< the script's N4 * 0.001
	float scale;         ///< N5 * 0.001
	bool townCentre;     ///< PlannedTownCentre
};

struct Town
{
	uint32_t id;
	std::unordered_map<std::string, float> beliefs;
	bool uninhabitable = false; ///< +0x5F4, SET_TOWN_UNINHABITABLE (0x715542)
	std::set<entt::entity> homelessVillagers;
	/// +0x9A4: the first town centre made for it (CREATE_TOWN_CENTRE 0x71577C sets it only while empty)
	entt::entity centre {entt::null};
	/// +0x5C0, Town::SetWorshipPercentage 0x73C060 (CREATE_TOWN_CENTRE's N5 * 0.001; all the shipped lands pass 0).
	/// The original keeps it only if the town has a worship site (otherwise 0) and passes it on to the totem statue;
	/// openblack has no worship sites yet and stores the script's value.
	float worshipPercentage {0.0f};
	std::vector<PlannedAbode> plannedAbodes; ///< Town::AddPlanned
	/// +0xF08/+0xF0C: CREATE_FLOCK's flocks for this town; fn_00419D10 takes a flock off when an animal that can't be
	/// shepherded joins it
	std::vector<entt::entity> flocks;
};

} // namespace openblack::ecs::components
