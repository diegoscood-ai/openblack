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

/// One entry of TownDesire's two sorted orders (+0x278 / +0x344, 12 bytes as the original's DesireSort)
struct DesireSort
{
	/// +0: order 1 (fn_746140) +0xD4 + +0x90 (fstp: a float); order 2 (fn_746190 0x7461A0) +0x90 (copied as a dword)
	float boosts {0.0f};
	/// +4: order 1 GetDesire, order 2 GetRawDesire; the only key of the comparator 0x746110
	float value {0.0f};
	/// +8: the TownDesireInfo
	uint32_t index {0};
};

/// TownStats (Town +0x610; TownStats::Add(Villager) 0x7492E0, Add(Abode) 0x7498C0). Only the fields the desires read;
/// recomputed at the start of each Town::Process by ecs::town_stats (the original keeps them incrementally)
struct TownStats
{
	uint32_t adults {0};           ///< +0x08 (Town +0x618)
	uint32_t children {0};         ///< +0x0C (Town +0x61C)
	uint32_t abodesWithPlaces {0}; ///< +0x10 (Town +0x620): abodes with max villagers + max children != 0
	uint32_t civicBuildings {0};   ///< +0x1C (Town +0x62C): IsCivic (vt +0x8C0)
	uint32_t civicPlans {0};       ///< +0x24 (Town +0x634): planned civic buildings. TODO(V6): 0
	uint32_t totalPlaces {0};      ///< +0x30 (Town +0x640): sum of max villagers + max children of every abode
	uint32_t adultPlaces {0};      ///< +0x34 (Town +0x644): sum of MaxVillagers (info +0x174) of the abodes with places
	uint32_t childPlaces {0};      ///< +0x40 (Town +0x650): sum of MaxChildren (info +0x178) of the abodes with places
	std::array<uint8_t, 13> disciples {}; ///< +0xC8 NumDisciples[VillagerDisciple] (CRAFTSMAN 8: +0xD0, Town +0x6E0)
	float foodForDinner {0.0f};    ///< +0xE4 (Town +0x6F4): sum of GVillagerInfo +0x2D8 foodReqiredForDinner
	float foodCarried {0.0f};      ///< +0xF8 (Town +0x708): sum of the villagers' +0xF4 (FOOD carried)
	float woodCarried {0.0f};      ///< +0xFC (Town +0x70C): sum of +0xF6 (WOOD carried)
	float woodAtSites {0.0f};      ///< +0x100 (Town +0x710): the wood at the building sites. TODO(V6): 0
	std::array<uint8_t, 16> abodesByNumber {}; ///< +0x108 (Town +0x718): abodes per AbodeNumber
};

/// TownDesire (Town +0x34, 0x564 bytes; bw1-decomp TownDesire.h; spec dev\tmp_dis\aldeanos\V3_spec.md §2.1). The
/// town object is zero-filled at creation (Base::operator new 0x4366F0 -> fn_436870 `rep stosd`), so all start at 0.
/// +0x08 / +0x4C / +0x15C / +0x234 have no known use and are left out
struct TownDesire
{
	/// +0x90 (Town +0xC4): boost A. (inferido) nobody writes it in a new game (only TownDesire::Load)
	std::array<float, 17> boostA {};
	/// +0xD4 (Town +0x108): the scripts' boost (SET_TOWN_DESIRE_BOOST 0x6FE737, TOWN_DESIRE_BOOST 0x717A26)
	std::array<float, 17> boost {};
	/// +0x118 (Town +0x14C): the desire, with the villagers' modification, in [-1, 1] (fn_745CA0 0x745D56)
	std::array<float, 17> desire {};
	/// +0x164: adults + children - worshipping - on the way (unsigned, fild qword), a float (0x745B1E). No known reader
	float population {0.0f};
	/// +0x168 (Town +0x19C): the raw desire, function x TribeMultiplier, not clamped (CallDesireFunction 0x745DFF)
	std::array<float, 17> raw {};
	/// +0x1AC / +0x1F0: Amount / Desired (5, 6, 7 only; read only by the debug trace 0x745EC0)
	std::array<float, 17> amount {};
	std::array<float, 17> desired {};
	/// +0x278: order 1 (value GetDesire; fn_746140); GetSortedDesire 0x7465D0 = &sorted[k]
	std::array<DesireSort, 17> sorted {};
	/// +0x344 (Town +0x378): order 2 (value GetRawDesire; fn_746190), what audio reads at Town +0x37C / +0x380
	std::array<DesireSort, 17> sortedRaw {};
	/// +0x410: turns above desireAffectsAlignmentAfter (fn_7466D0, milagros2's alignment by desires)
	std::array<int32_t, 17> alignmentTurns {};
	/// +0x454 / +0x498: doingNow / doingNowCount at the start of this turn's Process (fn_745CA0)
	std::array<float, 17> doingNowAtStart {};
	std::array<float, 17> doingNowCountAtStart {};
	/// +0x4DC (town +0x510): the sum of +-amount (state table file 0x08) of the villagers' final states serving it
	/// (Villager::AdjustTownModifier 0x7535BE); brought back to >= 0 by fn_745CA0
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
	/// +0xEAC / +0xEB0: the player of the last aggression against the town and the game turn of it (Town::UpdateAggressor
	/// 0x73C9B0, 0x73CA82 / 0x73CA98; read by Villager::ReactToMagicShieldPriority 0x765C28). Only the record is ported:
	/// the per-player aggression slots (town + n x 0x80 + 0x9F4, fn_0073E0F0), the 0.9 decay of +0xEB4 / +0xEB8 and the
	/// guidance SFX are not. Written by a physical shield's impacts (Magic/Objects/MapShield); TODO(towns): the other
	/// aggressions (damage, fire, buildings crushed) still do not. 0 = never (as in the original, turn 0 is "none")
	PlayerNames aggressor {PlayerNames::NEUTRAL};
	uint32_t aggressorTurn {0};
	/// +0x30: Town::SetStoragePit 0x73EA60 (from StoragePit::MakeFunctional 0x732F30; the last one wins); read through
	/// town_queries::GetStoragePit 0x73B5B0. (aproximado hasta V6) set when the script creates the storage pit
	entt::entity storagePit {entt::null};
	/// +0x744: Creche::MakeFunctional 0x50AB50 sets it when it is still null (the first one wins). (aproximado hasta
	/// V6) set when the script creates the creche
	entt::entity creche {entt::null};
	/// +0x5E4: "a plan was asked for this turn" (0 at 0x73961B; Town::Process 0x747390 clears it; CheckSatisfyAbodes /
	/// Civic set it, V6)
	bool requestedPlanThisTurn {false};
	/// +0x5E8 / +0x5EC: the building / resource pulse (0 at 0x73C7EA / 0x73C7F0; AddBuildingSite and
	/// StoragePit::AddResource write +0x5E8, V5/V6); Town::Process 0x747528: +0x5EC != 0 -> +0x5E8 = 0; +0x5EC = +0x5E8
	uint32_t buildPulse {0};
	uint32_t buildPulsePrevious {0};
	/// +0xF20: the empty town's countdown (Town::RemoveVillager 0x73E2CD sets 50, TODO(V12)); Town::Process 0x747536
	uint32_t emptyCountdown {0};
	/// +0xEC0 / +0xEBC: Protection / Mercy, written by Town::ProcessPlayerInteract 0x73DEC0 from the per-player
	/// aggression slots. TODO(agresiones): not ported, 0 (a town never attacked)
	float protectionDesire {0.0f};
	float mercyDesire {0.0f};
	/// +0x610 TownStats (this turn's, ecs::town_stats)
	TownStats stats;
};

} // namespace openblack::ecs::components
