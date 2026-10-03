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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
}

// The abode's villagers (Abode.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V4_spec.md §2.2, §6.2..§6.4, §8,
// disassembly dev\tmp_dis\aldeanos\v4\abode.txt, misc2.txt, town.txt): the list +0xA0 / +0xA4 (Abode::inhabitants, the
// head first), the pair +0xA8 / +0xAC (Abode::maleFemale), the counts +0xB4 / +0xB5 / +0xB7, PresentAtHome +0xB6, the
// score of a villager for the abode, Abode::Process (Town::Process step 4) and the two moves of the town's shuffle (step
// 24). The town's side (the homeless list, AddVillagerToTown, FindAbodeWithSpaceInTown) is ecs::town_villagers.

namespace openblack::ecs::abode_villagers
{
// ---- reading -----------------------------------------------------------------------------------------------------

/// +0xA0 (next = villager +0xE4) / +0xA4: the abode's villagers, the head first (AddVillagerToAbode 0x40415A inserts at
/// the head). An empty list for anything that is not an abode
[[nodiscard]] const std::vector<entt::entity>& VillagersOf(entt::entity abode);
/// +0xB6 PresentAtHome: the villagers inside now (only ArriveHome 0x405FA0 / LeaveHome 0x405FB0 change it)
[[nodiscard]] uint8_t PresentAtHome(entt::entity abode);
/// The abode's GAbodeInfo (Object +0x28): town_stats::AbodeInfoOf with its town's tribe; nullptr when none
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity abode);
/// Abode::GetTown (vt +0x48, Abode +0x98): the town of Abode::townId; entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity abode);
/// GAbodeInfo +0x174 MaxVillagersInAbode / +0x178 MaxChildrenInAbode (0 without an info)
[[nodiscard]] uint32_t MaxVillagers(entt::entity abode);
[[nodiscard]] uint32_t MaxChildren(entt::entity abode);
/// Abode::GetRoomLeftForAdults 0x404660 / GetRoomLeftForChildren 0x404680: max - count, signed
[[nodiscard]] int32_t GetRoomLeftForAdults(entt::entity abode);
[[nodiscard]] int32_t GetRoomLeftForChildren(entt::entity abode);
/// Abode::IsTooCrowded 0x4046C0: MaxVillagers 0 -> 1; else AdultCount / MaxVillagers >= percentTooCrowded (+0x1A0)
[[nodiscard]] bool IsTooCrowded(entt::entity abode);
/// Abode::GetPercentAbodeFullWithAdults 0x407050 (vt +0x89C): AdultCount / MaxVillagers (fidiv); MaxVillagers 0 -> 1
[[nodiscard]] float GetPercentAbodeFullWithAdults(entt::entity abode);
/// Abode::GetPercentAbodeFullWithChildren 0x407090 (vt +0x8A0): ChildCount / MaxChildren as an INTEGER division (`div`,
/// 0x4070B3: 0 or 1); MaxChildren 0 -> 1
[[nodiscard]] float GetPercentAbodeFullWithChildren(entt::entity abode);
/// Abode::CalculateScoreForAddingVillagerToAbode 0x404B40 (ScoreForAdding with the abode's counts and the distance)
[[nodiscard]] float CalculateScoreForAddingVillagerToAbode(entt::entity abode, entt::entity villager);
/// Abode::CalculateDesireToGainMale 0x4074A0 (DesireToGainMale with the town's TownStats +0x54 / +0x58)
[[nodiscard]] float CalculateDesireToGainMale(entt::entity abode);
/// Abode::CalculateDesireToGainVillager 0x407540 (DesireToGainVillager with the town's +0x618 / +0x644)
[[nodiscard]] float CalculateDesireToGainVillager(entt::entity abode);

// ---- the list ----------------------------------------------------------------------------------------------------

/// Abode::AddVillagerToAbode 0x404060: out of the town's homeless list, out of its old abode, or out of the vagrants;
/// at the head of the list; SetAbode (its town = the abode's); AddVillagerToTown if that town is not the villager's;
/// a child ++ChildCount, an adult MaleFemale[sex] (if empty), ++AdultCount, AdultMaleCount += IsMale
void AddVillagerToAbode(entt::entity abode, entt::entity villager);
/// Abode::RemoveAliveVillagerFromAbode 0x404340: inside -> SetTopState(163) (its exit does the LeaveHome); the counts
/// (not below 0); out of the list; SetAbode(0). MaleFemale is not touched (literal)
void RemoveAliveVillagerFromAbode(entt::entity abode, entt::entity villager);
/// Abode::RemoveDeletedVillagerFromAbode 0x404220 (the villager is being deleted): MaleFemale[sex] == it -> BOTH to 0;
/// the counts; the list; SetAbode(0); Town::RemoveVillager (town_villagers, the part V4 has)
void RemoveDeletedVillagerFromAbode(entt::entity abode, entt::entity villager);
/// Abode::RemoveAllVillagersFromAbode 0x404560 (the abode is destroyed): Villager::HomeDeleted 0x7611F0 of each
void RemoveAllVillagersFromAbode(entt::entity abode);
/// Abode::ArriveHome 0x405FA0 (`inc byte +0xB6`) / LeaveHome 0x405FB0 (`dec byte +0xB6`), wrapping like the bytes
void ArriveHome(entt::entity abode);
void LeaveHome(entt::entity abode);
/// Abode::ChildToAdult 0x404CC0: --ChildCount (not below 0), ++AdultCount, AdultMaleCount += IsMale, Town::ChildToAdult
void ChildToAdult(entt::entity abode, entt::entity villager);
/// Abode::SwapMaleForFemaleFrom(y) 0x407620 on x: the first man of y's list and the first woman of x's that are not
/// inside: ForceMoveVillagerToAbode(man -> x), (woman -> y); 1. No such pair: 0
uint32_t SwapMaleForFemaleFrom(entt::entity x, entt::entity y);
/// Abode::TakeVillagerFrom(y, male) 0x4075B0 on x: the first of y's list of that sex (IsMaleVillager / IsFemaleVillager)
/// that is not inside -> ForceMoveVillagerToAbode(-> x); 1. None: 0
uint32_t TakeVillagerFrom(entt::entity x, entt::entity y, bool male);

// ---- the turn ----------------------------------------------------------------------------------------------------

/// Abode::Process 0x404440 (vt +0x5FC, Town::Process step 4 for the abodes of the classes that do not override it)
void ProcessAbode(entt::entity abode);
/// Whether the abode's class runs Abode::Process 0x404440 as its vt +0x5FC (Abode, StoragePit, Creche, Wonder,
/// Graveyard); Field 0x529020, TownCentre 0x743DF0, Workshop 0x7797F0 and SpellDispenser 0x722A70 override it
[[nodiscard]] bool RunsAbodeProcess(entt::entity abode);

// ---- the pure layer (tests) --------------------------------------------------------------------------------------

/// 0x404B4F..0x404CA7: f = count / max (fidiv), min(f, 1); room = 1 - f, room <= 0 -> room; sex = listSize == 0 ? 1 :
/// ((1 - sameSex / listSize) + 1) x 0.5; (GetDistanceModifier(distance, 500) + 1) x 0.5 x sex x room; max 0 -> 0
[[nodiscard]] float ScoreForAdding(uint32_t count, uint32_t max, float sameSex, uint32_t listSize, float distance);
/// 0x4074BB..0x407531: (males + 0.001) / (females + 0.001) - (adultMales + 0.001) / ((adults - adultMales) + 0.001)
[[nodiscard]] float DesireToGainMale(uint32_t townMales, uint32_t townFemales, uint8_t adults, uint8_t adultMales);
/// 0x407566..0x40759E: (townAdults + 0.001) / (townAdultPlaces + 0.001) - percentAdults
[[nodiscard]] float DesireToGainVillager(uint32_t townAdults, uint32_t townAdultPlaces, float percentAdults);
} // namespace openblack::ecs::abode_villagers
