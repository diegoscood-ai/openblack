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

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "Enums.h"
#include "InfoConstants.h"

// The town's desires (TownDesire.cpp of runblack.exe W120, the class at Town +0x34; spec
// dev\tmp_dis\aldeanos\V3_spec.md, disassembly dev\tmp_dis\aldeanos\town_dis\desire.txt, desire_fns.txt, rep.txt):
// the 17 desire functions of the table 0xDA32C8 (filled by crt_xc 0x744BD0), TownDesire::Process 0x745AE0, the two
// sorted orders (the VC6 CRT _qsort 0x7C7E64, not stable), the share-out CheckVillagerNeededForTownDesire 0x745FF0 and
// the readers the other sessions use (audio, milagros2). Offsets are TownDesire's (Town +0x34) unless "Town +".
//
// Two layers: the pure one (a DesireContext of plain values; what test/test_town_desire.cpp checks) and the entity
// one (GatherInputs from the ECS, then the pure functions). The x87 chains of the original are computed in double and
// stored to float where the original stores (fstp dword) (aproximado: 64-bit instead of 80-bit intermediates).

namespace openblack::ecs::town_desire
{
constexpr size_t k_Count = 17;
using components::DesireSort;
using components::TownStats;

// ---- the pure layer ----------------------------------------------------------------------------------------------

/// GetDesireToBeRepaired's inputs of one abode of the town list (+0x754)
struct RepairInput
{
	float life {1.0f};              ///< GetPercentRepaired vt +0x884 = MultiMapFixed 0x401500 -> GetLife (vt +0x11C)
	bool livingQuarters {false};    ///< abode info AbodeType (+0x120) & 2
	uint32_t inhabitants {0};       ///< +0xA4, the villagers living in it
	float desireToBeRepaired {0.0f}; ///< abode info +0x118 (GMultiMapFixedInfo)
};

/// What the desire functions read of the town, gathered once per TownDesire::Process (GatherInputs)
struct DesireInputs
{
	TownStats stats;                 ///< Town +0x610 (openblack: recomputed per turn, ecs::town_stats)
	int32_t worshipping {0};         ///< Town +0x5C4 (milagros2 TownMagic::worshipping)
	int32_t onWayToWorship {0};      ///< Town +0x5CC (TownMagic::onWayToWorship)
	uint32_t homeless {0};           ///< Town +0x76C
	uint32_t abodeCount {0};         ///< Town +0x758 (fn_747C60)
	/// GetStoragePit 0x73B5B0 ? its GetResource(FOOD / WOOD) (vt +0x98) : nullopt
	std::optional<uint32_t> storageFood;
	std::optional<uint32_t> storageWood;
	/// the temporary pots +0x600 / +0x604 (Town::temporaryPots, ecs::town_stores): their GetResource, nullopt without one
	std::optional<uint32_t> potFood;
	std::optional<uint32_t> potWood;
	/// the building sites +0x790 (V6: none yet): their BuildingSite::GetDesireForVillagers 0x43BD70, site +0x634 and
	/// fn_43BBD0, in list order
	std::vector<float> siteDesires;
	std::vector<uint32_t> siteBuilders;
	std::vector<int32_t> sitePlaces;
	/// the abode list +0x754 (newest first) for Repair_Town; the plans +0x9A8 give 0 (V6/V11)
	std::vector<RepairInput> abodes;
	/// GAbodeInfo::Find(tribe, i) 0x405B30 +0x1B4 PopulationWhenNeeded for the 16 abode numbers; nullopt: no record
	/// (the original would read a null record; openblack skips it)
	std::array<std::optional<int32_t>, 16> populationWhenNeeded {};
	bool hasPlayer {true};           ///< Town::GetPlayer (vt +0x1C): openblack's towns always have one (NEUTRAL)
	bool isLocalPlayer {false};      ///< GPlayer::IsMemberOfThisPlayer 0x64D750 (inferido: owner == PLAYER_ONE)
	float alignment {0.0f};          ///< GPlayer::GetAlignmentValue 0x64D6A0 (ecs::effects::alignment::Get)
	float tribalPower4 {1.0f};       ///< 0x73E5E0(4) = player ? player +0x68[4] : 1 (PlayerMagic::tribalPower)
	bool crecheFunctional {false};   ///< +0x744 && IsFunctional (vt +0xD4)
	float belief {0.0f};             ///< Town::GetBeliefInPlayer 0x73BAB0. TODO(milagros2): not ported, 0
	float protection {0.0f};         ///< Town +0xEC0 (ProcessPlayerInteract 0x73DEC0. TODO(agresiones): 0)
	float mercy {0.0f};              ///< Town +0xEBC (idem)
	float visualHour {12.0f};        ///< GGameInfo::GetVisualTime 0x5575A0 (DayNightClock::GetVisualTime)
	uint32_t turn {0};               ///< g_game +0x205A40
	Tribe tribe {Tribe::CELTIC};     ///< Town +0x5B8 (inferido: TRIBE_TYPE = openblack's Tribe for TribeMultiplier)
};

/// The guidance calls of the desires (audio::guidance): which one and its value
enum class Warning
{
	VillagersUnhappy, ///< TownDesire::Process 0x745C8A HelpSpritesVillagerUnhappy (value avg - 0.6, not used)
	LowOnFood,        ///< Town::CalculateDesireForFood 0x747FA0 HelpSpritesLowOnFood(min(v, 2) - 0.9)
	LowOnWood,        ///< 0x7481BC fn_0071CAF0 (LowOnWood)(min(v, 2) - 1)
};
using WarningSink = std::function<void(Warning, float)>;

/// Everything a desire function reads: the town's TownDesire (the values of this turn for the desires already done,
/// of the last turn for the rest), the inputs and the info
struct DesireContext
{
	const components::TownDesire& desire;
	const DesireInputs& in;
	const GTownInfo& town;                        ///< Town +0x28 (0xDA2780)
	const std::array<GTownDesireInfo, 17>& info;  ///< 0xDA2930 + d x 0x90
	/// g[0xDA92B4] / g[0xDA92B8] = GVillagerInfo[10] ("African Farmer Male", 0xDA6BE8 + 10 x 0x3A4) maxFoodCarried
	/// (+0x264) / maxWoodCarried (+0x268): a fixed row of the exe (literal oddity), 150 / 250 with info.dat
	uint32_t farmerMaxFood {150};
	uint32_t farmerMaxWood {250};
	const WarningSink* warn {nullptr}; ///< null: no guidance (FoodDesireValue, the tests that do not look)
};

using DesireFn = float (*)(const DesireContext&);
using AmountFn = uint32_t (*)(const DesireContext&);
using ModificationFn = float (*)(const DesireContext&, size_t d);
using CheckSatisfyFn = uint32_t (*)(entt::entity villager);

/// One entry of GTownDesireFunction 0xDA32C8 + d x 0x68 (emu_dtab_all.py)
struct DesireFunctions
{
	const char* name;            ///< +0x00
	DesireFn function;           ///< +0x10 / +0x14 (a member of Town)
	AmountFn amount;             ///< +0x20 (debug trace only)
	AmountFn desired;            ///< +0x30 (debug trace only)
	CheckSatisfyFn checkSatisfy; ///< +0x40 / +0x44 (a member of Villager)
	ModificationFn modification; ///< +0x50 / +0x54 (a member of Town, called with d)
	bool children;               ///< +0x60: a child may serve it (CheckVillagerNeededForTownDesire 0x746073)
	bool flag64;                 ///< +0x64: its only reader 0x7466B0 has no callers: unused
};
/// The table (TownDesire.cpp, the order of TownDesireInfo)
[[nodiscard]] const std::array<DesireFunctions, k_Count>& Table();

// The desire functions (this = Town in the original). R(d) = GetRawDesire, D(d) = GetDesire.
/// Town::CalculateDesireForFood 0x747F00 (entry 0's thunk 0x747340, vt +0x420), with the LowOnFood warning
[[nodiscard]] float DesireForFood(const DesireContext& c);
/// 0x747FF0, with the LowOnWood warning
[[nodiscard]] float DesireForWood(const DesireContext& c);
/// 0x7487B0
[[nodiscard]] float DesireForPlaytime(const DesireContext& c);
/// 0x7488A0: Town +0xEC0
[[nodiscard]] float DesireForProtection(const DesireContext& c);
/// 0x7488B0: Town +0xEBC
[[nodiscard]] float DesireForMercy(const DesireContext& c);
/// 0x748210
[[nodiscard]] float DesireForAbodes(const DesireContext& c);
/// 0x748330
[[nodiscard]] float DesireForCivicBuildings(const DesireContext& c);
/// 0x748320 (`fld 0; ret`)
[[nodiscard]] float DesireForSupplyWorship(const DesireContext& c);
/// 0x748430
[[nodiscard]] float DesireForChildren(const DesireContext& c);
/// 0x748640
[[nodiscard]] float DesireToBuild(const DesireContext& c);
/// 0x748690 / 0x7486A0 (`fld 0; ret`)
[[nodiscard]] float DesireForRain(const DesireContext& c);
[[nodiscard]] float DesireForSun(const DesireContext& c);
/// 0x7486B0
[[nodiscard]] float DesireToRepair(const DesireContext& c);
/// 0x748730 (`fld 0; ret`)
[[nodiscard]] float DesireToSupplyWorkshop(const DesireContext& c);
/// 0x748740
[[nodiscard]] float DesireToBuildWonder(const DesireContext& c);
/// 0x7488C0
[[nodiscard]] float DesireForRelaxation(const DesireContext& c);
/// 0x748960
[[nodiscard]] float DesireForSleep(const DesireContext& c);

/// Abode::GetDesireToBeRepaired 0x406970 (vt +0x8D8, with MultiMapFixed's 0x52ECE0)
[[nodiscard]] float AbodeDesireToBeRepaired(const RepairInput& abode, const GTownInfo& town);

/// Town::GetDesire 0x73E400: +0x118 + +0xD4 + +0x90
[[nodiscard]] float GetDesire(const components::TownDesire& desire, size_t d);
/// Town::GetRawDesire 0x73E420: +0x168 + +0xD4 + +0x90
[[nodiscard]] float GetRawDesire(const components::TownDesire& desire, size_t d);

/// TownDesire::GetDesireVillagerModification 0x746270: the entry's +0x50 with d
[[nodiscard]] float GetDesireVillagerModification(const DesireContext& c, size_t d);
/// the general one 0x746490: 1 - min(+0x4DC[d] / (adults + children + 1e-5), 1)
[[nodiscard]] float ModificationGeneral(const DesireContext& c, size_t d);
/// Food 0x7462A0: 1 - min((150 +0x4DC[d] + 1e-4 + store food) / (desired food + 1e-4), 1)
[[nodiscard]] float ModificationFood(const DesireContext& c, size_t d);
/// Wood 0x746350: 1 - min((250 +0x4DC[d] + 1e-4 + store wood) / (fn_747B60 + 1e-4), 1)
[[nodiscard]] float ModificationWood(const DesireContext& c, size_t d);
/// To_Build 0x746400: 1 - min((1e-4 + sum site +0x634) / (1e-4 + sum fn_43BBD0), 1)
[[nodiscard]] float ModificationToBuild(const DesireContext& c, size_t d);
/// TownDesire::GetTemporaryDesireVillagerModification 0x7464F0(k): 1 - min(max(+0x4DC[k] - +0x454[k], 0) / (pop +
/// 1e-5), 1), pop = Town +0x618 + +0x61C
[[nodiscard]] float GetTemporaryDesireVillagerModification(const components::TownDesire& desire, uint32_t population,
                                                           size_t k);

/// TownDesire::CallDesireFunction 0x745D80: raw (+0x168) = f x TribeMultiplier[tribe]; returns clamp(raw x mod, -1, 1)
float CallDesireFunction(components::TownDesire& desire, const DesireContext& c, size_t d);
/// fn_745CA0(d): +0x4DC[d] >= 0, the copies +0x454 / +0x498, Amount / Desired, +0x118 = CallDesireFunction
void ProcessDesire(components::TownDesire& desire, const DesireContext& c, size_t d);
/// The VC6 CRT _qsort 0x7C7E64 (CUTOFF 8 -> _shortsort 0x7C7FB8, else the middle as pivot and _swap 0x7C8006; not
/// stable) with the comparator 0x746110 (value: a < b -> 1, == -> 0, else -1: descending, NaN -> 1)
void MsvcQsort(std::array<DesireSort, k_Count>& entries);
/// fn_746140: order 1 (+0x278): {+0xD4 + +0x90, GetDesire, d} for d = 0..16, then _qsort
void SortDesires(components::TownDesire& desire);
/// fn_746190: order 2 (+0x344): {+0x90, GetRawDesire, d}, then _qsort
void SortRawDesires(components::TownDesire& desire);
/// TownDesire::Process 0x745AE0 (+0x164, the 17 desires in order 0..16, both orders, the average every 50 turns and
/// its HelpSpritesVillagerUnhappy). Returns the average when it was computed (the trace)
std::optional<float> Process(components::TownDesire& desire, const DesireContext& c);

/// One step of the share-out's loop, for the trace (OPENBLACK_VILLAGER_TRACE)
struct ShareOutStep
{
	size_t k;
	uint32_t d;
	float value;
	float temporary;
	float threshold;
	const char* result; ///< "skip(child)", "skip(nocs)", "cut", "cs=0", "cs=1"
};
/// TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 on order 1 (the pure part): `checkSatisfy(d)` stands for
/// the entry's CheckSatisfy on the villager (only called for an entry that has one). 0 or 1 (0x7460EB / 0x7460F7)
uint32_t CheckVillagerNeeded(const components::TownDesire& desire, const std::array<GTownDesireInfo, 17>& info,
                             uint32_t population, bool child, float trigger,
                             const std::function<uint32_t(size_t d)>& checkSatisfy,
                             const std::function<void(const ShareOutStep&)>& trace = {});

/// GetDesireSignificanceToVillager 0x746660: max(D(d) - info[d] +0x18, 0)
[[nodiscard]] float GetDesireSignificanceToVillager(const components::TownDesire& desire, const GTownDesireInfo& info,
                                                    size_t d);
/// TownDesire::GetMostDesired 0x745E50: the strict argmax of +0x118 above 0 (no boosts); -1 when all are <= 0
[[nodiscard]] int GetMostDesired(const components::TownDesire& desire);
/// TownDesire::GetMostSignificantRawDesire 0x745EA0: order2[0].value >= m ? order2[0].index : -1
[[nodiscard]] int GetMostSignificantRawDesire(const components::TownDesire& desire, float minimum);
/// fn_747270: the name's index (_stricmp against the 17 names of the table); -1 when none
[[nodiscard]] int FindDesire(std::string_view name);

// ---- the entity layer (the API of spec §9.2 for audio, milagros2 and the villagers) ------------------------------

/// Town::GetDesire 0x73E400 / Town::GetRawDesire 0x73E420 of a town entity (0 without one)
[[nodiscard]] float GetDesire(entt::entity town, TownDesireInfo d);
[[nodiscard]] float GetRawDesire(entt::entity town, TownDesireInfo d);
/// fn_00747150 (this = TownDesire, Town +0x34; SetStateSpeed 0x7539BD): S = 0.2 x (raw 13 + 12 + 9 + 7 + 6 + 5 + 4 + 3
/// + 1 + 0) (+0x168, no boosts) clamped to [0, 1]; returned on the x87 stack (double). 0 without a town
[[nodiscard]] double TownNeedsSum(const components::TownDesire& desire);
[[nodiscard]] double TownNeedsSum(entt::entity town);
/// +0x278 (GetSortedDesire 0x7465D0 = &order1[k]) and +0x344 (Town +0x378: value +0x37C, type +0x380, the one
/// GGuidance::CheckTownDesiresSFX 0x71B130 reads). An empty (all 0) array without a town
[[nodiscard]] const std::array<DesireSort, k_Count>& GetSortedDesires(entt::entity town);
[[nodiscard]] const std::array<DesireSort, k_Count>& GetSortedRawDesires(entt::entity town);
/// The four arrays of TownDesire (GetResourceDropSample 0x71B5F0 reads Raw + Boost + BoostA of 0, 1 and 10)
enum class Field
{
	BoostA, ///< +0x90 (Town +0xC4): nobody writes it in a new game (inferido: only Load)
	Boost,  ///< +0xD4 (Town +0x108): SET_TOWN_DESIRE_BOOST, TOWN_DESIRE_BOOST
	Desire, ///< +0x118 (Town +0x14C): with the modification, in [-1, 1]
	Raw,    ///< +0x168 (Town +0x19C): function x TribeMultiplier, not clamped
};
[[nodiscard]] float GetField(entt::entity town, TownDesireInfo d, Field field);
/// GetDesireSignificanceToVillager 0x746660 of a town entity
[[nodiscard]] float GetDesireSignificanceToVillager(entt::entity town, TownDesireInfo d);
[[nodiscard]] int GetMostDesired(entt::entity town);
[[nodiscard]] int GetMostSignificantRawDesire(entt::entity town, float minimum);
/// Town::CalculateDesireForFood 0x747F00 now, literal: with HelpSpritesLowOnFood when v >= 0.95 for the local player
/// (Villager::ArrivesAtFoodReaction 0x764A45 calls it too, V9)
float CalculateDesireForFood(entt::entity town);
/// openblack: the same value without the warning (for readers)
[[nodiscard]] float FoodDesireValue(entt::entity town);
/// TownDesire::CallDesireFunction 0x745D80 now, on a town entity (Abode::DoResourceRemoving 0x404FA0 / DoResourceAdding
/// 0x404E22 call it before the resource changes): +0x168[d] (the raw) is rewritten and the function's warning may play
/// (HelpSpritesLowOnFood / LowOnWood); returns clamp(raw x modification, -1, 1). 0 without a town
float CallDesireFunctionNow(entt::entity town, TownDesireInfo d);
/// TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 (town +0x34, the villager, its trigger): 0 or 1
uint32_t CheckVillagerNeededForTownDesire(entt::entity town, entt::entity villager, float trigger);
/// The scripts' boost (+0xD4[d] = boost); `resort`: fn_746140 (order 1 only, as SET_TOWN_DESIRE_BOOST 0x6FE73E)
void SetBoost(entt::entity town, TownDesireInfo d, float boost, bool resort);
/// +0x410 (turns above desireAffectsAlignmentAfter, fn_7466D0): written only by milagros2's alignment by desires
[[nodiscard]] std::array<int32_t, k_Count>& AlignmentTurns(entt::entity town);

/// TownDesire::Process 0x745AE0 for a town entity (from Town::Process 0x7473D7; ecs::town_process)
void Process(entt::entity town);
/// The inputs of the pure layer from the ECS (the town's stats must be this turn's: town_process computes them)
[[nodiscard]] DesireInputs GatherInputs(entt::entity town);

// ---- scripts -----------------------------------------------------------------------------------------------------

/// GScript::SetTownDesireBoost 0x6FE650 after its three POPs (boost, desire, thing): "Thing not valid!" without a
/// town, "Invalid Params" unless desire < 17 (signed) and -1 <= boost <= 1; both right: +0xD4[desire] = boost and
/// fn_746140 (order 1 only). (aproximado) a negative desire, which the original writes out of the array, is not
/// written. Returns whether it wrote; `errors` gets the messages (ScriptErrorMessage 0x6F62B0)
bool ScriptSetTownDesireBoost(entt::entity thing, int32_t desire, float boost, std::vector<std::string>* errors);
/// GScript::GetDesire 0x6FCCA0 after its first POP (d): d out of [0, 17) -> "Invalid desire" and 0 without the second
/// POP (literal: the object stays on the stack); else `popObject()`, no thing -> "Object no longer valid" and 0, not a
/// town -> 0 (no message, 0x6FCD48), else GetRawDesire(d)
float ScriptGetDesire(int32_t d, const std::function<entt::entity()>& popObject, std::vector<std::string>* errors);
/// GSetup::MapCommands TOWN_DESIRE_BOOST 0x7179EC: town and fn_747270(name) != -1 -> +0xD4[d] = value, no re-sort and
/// no range check
void MapTownDesireBoost(entt::entity town, std::string_view name, float value);

// ---- test hooks --------------------------------------------------------------------------------------------------

/// The tests: the guidance calls go here instead of audio::guidance (empty: audio)
void SetWarningSinkForTests(WarningSink sink);
/// The tests: CheckSatisfy(d, villager) instead of the table's (empty: the table's)
void SetCheckSatisfyForTests(std::function<uint32_t(size_t d, entt::entity villager)> checkSatisfy);
} // namespace openblack::ecs::town_desire
