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
/// objects are invisible (PlannedMultiMapFixed::Draw 0x648930 is a bare `ret`). ecs::plans converts them into abodes
/// under construction (CreatePlannedNoFixedCheck 0x405770 / 0x744550). +0x34 (no known use) and +0x38 GFootpathLink*
/// (footpaths not ported) are left out
struct PlannedAbode
{
	AbodeInfo info;      ///< +0x40
	glm::vec3 position;  ///< +0x14
	float yAngleRadians; ///< +0x28, the script's N4 * 0.001
	float scale;         ///< +0x2C, N5 * 0.001
	bool townCentre;     ///< PlannedTownCentre
	/// +0x30: "was a built building" (a rebuild plan: PlannedMultiMapFixed(MultiMapFixed*) 0x6488B4, the building's
	/// +0x58 bit 3); 0 for the script's (0x648803). Read by GetDesireToBeRepaired 0x648910 and
	/// CreatePlannedNoFixedCheck 0x4057C5
	bool wasBuilt {false};
	/// +0x3C: the creation turn (g_game +0x205A40, 0x6487F6); its age 0x6488E0 = turn - it. plans::AddPlanned writes it
	uint32_t creationTurn {0};
	/// a PlannedTownCitadelHeart (vtable 0x8C9D4C, ctor 0x467DD0; CitadelArchetype::CreatePlan): `info` is None and its
	/// info is the GCitadelHeartInfo of `heartInfo` (+0x40 = 0xC5E270 + N3 x 0x158). GetAbodeType 0x467E30 = 0x804,
	/// IsCivic 0x467E10 = 0, GetTown 0x56FF10 = 0
	bool citadelHeart {false};
	uint32_t heartInfo {0};
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
	uint32_t civicPlans {0};       ///< +0x24 (Town +0x634): the plans whose IsCivic (vt +0x50C, plans::IsCivic)
	uint32_t totalPlaces {0};      ///< +0x30 (Town +0x640): sum of max villagers + max children of every abode
	uint32_t adultPlaces {0};      ///< +0x34 (Town +0x644): sum of MaxVillagers (info +0x174) of the abodes with places
	uint32_t childPlaces {0};      ///< +0x40 (Town +0x650): sum of MaxChildren (info +0x178) of the abodes with places
	int32_t freeAdultPlaces {0};   ///< +0x4C (Town +0x65C): MaxVillagers of the counted abodes - their adults (V6)
	uint32_t males {0};            ///< +0x54 (Town +0x664): +0x54[info +0x1F8 sex]++ for every villager, children too
	uint32_t females {0};          ///< +0x58 (Town +0x668) (TownStats::Add 0x749315; ShuffleVillagersAroundAbodes reads them)
	std::array<uint8_t, 13> disciples {}; ///< +0xC8 NumDisciples[VillagerDisciple] (CRAFTSMAN 8: +0xD0, Town +0x6E0)
	float foodForDinner {0.0f};    ///< +0xE4 (Town +0x6F4): sum of GVillagerInfo +0x2D8 foodReqiredForDinner
	float foodCarried {0.0f};      ///< +0xF8 (Town +0x708): sum of the villagers' +0xF4 (FOOD carried)
	float woodCarried {0.0f};      ///< +0xFC (Town +0x70C): sum of +0xF6 (WOOD carried)
	float woodAtSites {0.0f};      ///< +0x100 (Town +0x710): GetWoodForStats of the sites whose GetTown is the town
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

/// std::array<float, N> with every element `value` (the default member initialisers of TownBelief)
template <size_t N>
constexpr std::array<float, N> FilledArray(float value)
{
	std::array<float, N> result {};
	result.fill(value);
	return result;
}

/// Town +0x798 GBelief (Belief.h, 0x1D0 bytes; GBelief : Base, its vtable +0x0 and Base +0x4 left out), every row by
/// the player number (PlayerNames, NEUTRAL = 7); changed by ecs::town_belief and ecs::town_stores::AddToBelief. The
/// defaults are the zero-filled allocation (Base::operator new 0x4366F0 -> fn_00436870, rep stosd) with Init
/// 0x437DD0's cap and boredom, so that a town made without Init still caps at 10; Init also sets the desire
/// thresholds. Town +0x5D8 and +0x5DC, read only by the belief, are kept here too.
/// Spec dev\documentacion\edificios\belief_spec.md
struct TownBelief
{
	/// +0x8 [player] BeliefInPlayer (GetBeliefInPlayer 0x437E70): SetBelief 0x4387D0 (capped), ReduceBelief 0x437FD0
	/// (not clamped: it can go below 0); 0 in Init but the neutral slot (= +0x5D8)
	std::array<float, 8> belief {};
	/// +0x28 [player], float (bw1-decomp says uint32): += f (0x437ED5); only decays (x GPlayerInfo +0x48 0.997 a turn,
	/// fold 0x438644..0x43864C) and is read only by the computer player (fn_00438A40). Not reset by Init
	std::array<float, 8> recent {};
	std::array<uint32_t, 8> lastAddedTurn {}; ///< +0x48 [player], the turn of the last f != 0 (0x437EF5); no reader
	/// +0x68 [player] BeliefInPlayerMax: 10.0 (Init 0x437DE1); SET_TOWN_BELIEF_CAP (SetBeliefInPlayerCap 0x438A00)
	std::array<float, 8> cap {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
	/// +0x88 [player]: += the folded amount (fold 0x438424); 0 every 10 turns (ProcessOncePerTurn 0x438158). Not reset
	/// by Init
	std::array<float, 8> addedThisPeriod {};
	/// +0xA8 [player]: ReduceBelief's accumulator for its (never shown) draw (0x437FFD, 0x438041); 0 in Init
	std::array<float, 8> reduceAccumulator {};
	/// +0xC8 [player]: += f (0x437EC7), what was added since the last fold; fn_004383D0 folds it x +0x5DC (beliefScale)
	/// into +0x8 (SetBelief 0x4387D0) and +0x88, then 0 (0x4384A6)
	std::array<float, 8> pending {};
	/// +0xE8 [REACTION] BoredomMultiplier: 1.0 (Init 0x437E40); the fold adds ReactionInfo +0x4C below 1, Villager::
	/// UpdateHowImpressed 0x7635B9 AddToBoredomMultiplier 0x438790; read by GetBoredomMultiplier 0x56FE70
	std::array<float, 41> boredom = FilledArray<41>(1.0f);
	/// +0x18C [TOWN_DESIRE]: the desire above which the owner loses belief (fold 0x438565); GTownDesireInfo +0x3C in
	/// Init and on every fold with something pending (fn_00437E50), then -= +0x48 a fold while above GBeliefInfo +0x1C
	/// (0.25)
	std::array<float, 17> desireThreshold {};
	/// Town +0x5D8 BeliefInNeutralPlayer (fn_0073E4B0): GTownInfo +0xB8 (0.5) after Init in the Town ctor (0x73966C);
	/// SET_TOWN_BELIEF of the neutral player (Town::SetBeliefInPlayer 0x73BA87). The fold pins belief[NEUTRAL] to it
	float beliefInNeutralPlayer {0.0f};
	/// Town +0x5DC: the scale of the pending belief (1.0, Town ctor 0x739672); SET_TOWN_BALANCE_BELIEF_SCALE (case 98,
	/// 0x717BBA), LHVM SET_OBJECT_BELIEF_SCALE (GScript::SetObjectBeliefScale 0x6FF8B2, not ported)
	float beliefScale {1.0f};
};

struct Town
{
	uint32_t id;
	/// (openblack) the order the Town ctor ran in (TownArchetype::Create, 1 up; 0 for a town made elsewhere). The
	/// global town list g_game +0x205C84 is filled at the head (0x73964D..0x739656): the higher, the newer. Town::id is
	/// the script's CREATE_TOWN argument, not that order (maps create towns out of id order, and one repeats id 0)
	uint32_t creationStamp {0};
	/// +0x2C, Town::GetPlayer: the player given to CREATE_TOWN (the neutral player when none, Town ctor 0x739545).
	/// Planned citadels belong to it, not to the player named in CREATE_PLANNED_CITADEL (0x467EF0).
	PlayerNames owner {PlayerNames::NEUTRAL};
	bool uninhabitable = false; ///< +0x5F4, SET_TOWN_UNINHABITABLE (0x715542)
	/// +0x768 / +0x76C: the town's homeless, the head first (MakeHomelessNoStateChange 0x7612F9 inserts at the head, next
	/// = villager +0xE4). Changed only by ecs::town_villagers
	std::vector<entt::entity> homelessVillagers;
	/// +0x9A4: the first town centre made for it (CREATE_TOWN_CENTRE 0x71577C sets it only while empty)
	entt::entity centre {entt::null};
	/// +0x600 / +0x604 [RESOURCE_TYPE]: the temporary pots (FOOD, WOOD) Town::GetTemporaryResourceStorePotOrPos 0x73E900
	/// makes and keeps (0x73EA11); ecs::town_stores
	std::array<entt::entity, 2> temporaryPots {entt::null, entt::null};
	/// +0xEC8 [player][RESOURCE_TYPE]: the turn each player last took FOOD / WOOD from this town's abodes or storage pit
	/// through an interface (Town::SetGameTurnResourceLastRemoved 0x7400D0); 0 = never. ecs::town_stores
	std::array<std::array<uint32_t, 2>, 8> resourceLastRemovedTurn {};
	/// +0x798: the town's GBelief (ecs::town_belief; AddToBelief is ecs::town_stores')
	TownBelief belief;
	/// +0x5C0, Town::SetWorshipPercentage 0x73C060 (CREATE_TOWN_CENTRE's N5 * 0.001; all the shipped lands pass 0).
	/// The original keeps it only if the town has a worship site (otherwise 0) and passes it on to the totem statue;
	/// openblack has no worship sites yet and stores the script's value.
	float worshipPercentage {0.0f};
	std::vector<PlannedAbode> plannedAbodes; ///< +0x9A8 / +0x9AC, oldest first (Town::AddPlanned 0x73D080, ecs::plans)
	/// +0x790 / +0x794: the building sites, the head first (Town::AddBuildingSite 0x73B910 inserts at the head);
	/// entities with components::BuildingSite. Changed only by ecs::building_sites
	std::vector<entt::entity> buildingSites;
	/// +0x748: the graveyard (ecs::graveyard: Graveyard::MakeFunctional 0x595E00 / DeleteDependancys 0x595CE0 through
	/// SetGraveyard fn_73D690); read by GetDesireToBeBuilt 0x73A1A0 (0x204, 0x2004)
	entt::entity graveyard {entt::null};
	/// +0x728 / +0x72C and +0x734 / +0x738: the town rectangle, MapCoords x / z (min, max; the cells are the high words
	/// +0x72A / +0x72E / +0x736 / +0x73A). Town::SetTownArea 0x73AAF0 (town_placement); min 0x7FFFFFFF, max 0 = empty
	glm::ivec2 areaMin {0x7FFFFFFF, 0x7FFFFFFF};
	glm::ivec2 areaMax {0, 0};
	/// +0x5FC: the town has had a centre, a storage pit and a house (fn_404960 0x404960, set once). (not ported)
	/// readers
	bool hasCentrePitAndHouse {false};
	/// TownStats +0xEC (Town +0x6FC): the wood used building (fn_73B620 0x73B620), kept (the stats are recomputed)
	float woodUsedForBuilding {0.0f};
	/// +0xF08/+0xF0C: CREATE_FLOCK's flocks for this town; fn_00419D10 takes a flock off when an animal that can't be
	/// shepherded joins it
	std::vector<entt::entity> flocks;
	TownDesire desire; ///< +0x34
	/// +0xF10: Town::GetCongregationPos 0x7408B0's cache, MapCoords x / z (6553.6 per metre) and y; (0, 0, 0) = not
	/// computed yet. Zeroed by the constructor (0x739501, fn_0073C710 0x73C81D); written by GetCongregationPos, by
	/// SET_TOWN_CONGREGATION_POS (MapCommandProcess case 6, 0x7155A3..0x7155B9) and cleared by
	/// CheckWhenNewBuildingCreated 0x741500 (a building made within 7.5 m, from PostCreatePlanned 0x648C50: ecs::plans)
	glm::ivec2 congregationPos {0, 0};
	float congregationPosY {0.0f};
	/// +0xF1C: the turn the town's emergency started (Town::IsInStateOfEmergency 0x747970 reads it; 0 = none).
	/// Written by Town::SetInStateOfEmergency 0x7479A0, cleared by ProcessTownEmergency 0x747873 (ecs::town_emergency)
	uint32_t emergencyStartTurn {0};
	/// +0xEC4: the worship percentage (+0x5C0) saved while the emergency lasts (ProcessTownEmergency 0x7477EA), given
	/// back after it (0x747864) and then 0 (0x747869). 0 at creation (the zero-filled town)
	float savedWorshipPercentage {0.0f};
	/// +0xEAC / +0xEB0: the player of the last aggression against the town and the game turn of it (Town::UpdateAggressor
	/// 0x73C9B0, 0x73CA82 / 0x73CA98; read by Villager::ReactToMagicShieldPriority 0x765C28). Only the record is ported:
	/// the per-player aggression slots (town + n x 0x80 + 0x9F4, fn_0073E0F0), the 0.9 decay of +0xEB4 / +0xEB8 and the
	/// guidance SFX are not. Written by a physical shield's impacts (Magic/Objects/MapShield); TODO(towns): the other
	/// aggressions (damage, fire, buildings crushed) still do not. 0 = never (as in the original, turn 0 is "none")
	PlayerNames aggressor {PlayerNames::NEUTRAL};
	uint32_t aggressorTurn {0};
	/// +0x30: Town::SetStoragePit 0x73EA60 (from StoragePit::MakeFunctional 0x732F30; the last one wins); read through
	/// town_queries::GetStoragePit 0x73B5B0. A whole (script) one at its creation, a plan's when built (MakeFunctional,
	/// town_stores::SetStoragePit)
	entt::entity storagePit {entt::null};
	/// +0x744: Creche::MakeFunctional 0x50AB50 sets it when it is still null (the first one wins): a whole (script) one
	/// at its creation, a plan's when built (abodes::MakeFunctional)
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
	/// +0x6F8 (TownStats +0xE8): the food the villagers have eaten (Town::UseFood 0x73B5E0), kept: it cannot be recomputed
	float foodUsed {0.0f};
};

} // namespace openblack::ecs::components
