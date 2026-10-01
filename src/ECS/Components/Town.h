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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
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

/// TownDesire (Town +0x34). V1 has only what Villager::AdjustTownModifier 0x753560 writes: per town desire
/// (TownDesireInfo), the villagers serving it now
struct TownDesire
{
	/// +0x4DC (town +0x510): the sum of +-amount (state table file 0x08) of the villagers' final states serving it
	std::array<float, 17> doingNow {};
	/// +0x520 (town +0x554): +-1 per state, a float as in the original (fadd)
	std::array<float, 17> doingNowCount {};
};

struct Town
{
	uint32_t id;
	/// +0x2C, Town::GetPlayer: the player given to CREATE_TOWN (the neutral player when none, Town ctor 0x739545).
	/// Planned citadels belong to it, not to the player named in CREATE_PLANNED_CITADEL (0x467EF0).
	PlayerNames owner {PlayerNames::NEUTRAL};
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
	TownDesire desire; ///< +0x34
	/// +0xF10: Town::GetCongregationPos 0x7408B0's cache, MapCoords x / z (6553.6 per metre) and y; (0, 0, 0) = not
	/// computed yet. Zeroed by the constructor (0x739501, fn_0073C710 0x73C81D); written by GetCongregationPos, by
	/// SET_TOWN_CONGREGATION_POS (MapCommandProcess case 6, 0x7155A3..0x7155B9) and cleared by
	/// CheckWhenNewBuildingCreated 0x741500 (a building made within 7.5 m; only from PostCreatePlanned: TODO(V6))
	glm::ivec2 congregationPos {0, 0};
	float congregationPosY {0.0f};
	/// +0xF1C: the turn the town's emergency started (Town::IsInStateOfEmergency 0x747970 reads it; 0 = none).
	/// TODO(Milagros): written by ProcessTownEmergency; nobody writes it in openblack yet, so 242 is never reached
	uint32_t emergencyStartTurn {0};
};

} // namespace openblack::ecs::components
