/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeVillagers.h"

#include <algorithm>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// Abode.cpp of runblack.exe W120 (AbodeVillagers.h)

namespace openblack::ecs::abode_villagers
{
using namespace components;

namespace
{
const std::vector<entt::entity> k_NoVillagers;

/// 0.001 (0x8AA3B0) and 500 (0x43FA0000, CalculateScoreForAddingVillagerToAbode 0x404C78)
constexpr float k_Thousandth = 0.001f;
constexpr float k_ScoreDistance = 500.0f;
/// Abode::Process 0x4044BC / 0x4044C8: +0xB0 += 0.001 each processed turn; the decay at 1
constexpr float k_EmptyTimerStep = 0.001f;
/// 0x404503..0x40450F: +0xB9 counts up to 200 (0xC8)
constexpr uint8_t k_CounterB9Max = 200;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Abode* AbodeComponent(entt::entity abode)
{
	auto& registry = Entities();
	return abode != entt::null && registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr;
}

/// Villager::IsMaleVillager 0x55CAC0 (vt +0x44C): info +0x1F8 == 0 (no child test); IsFemaleVillager 0x55CAE0: == 1
bool IsMaleVillager(entt::entity villager)
{
	return villager::InfoOf(villager).sex == SexType::Male;
}

bool IsFemaleVillager(entt::entity villager)
{
	return villager::InfoOf(villager).sex == SexType::Female;
}

bool IsInside(entt::entity villager)
{
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && (v->flags & Villager::k_FlagAtHome) != 0;
}

/// The list's removal (0x4043BC..0x404406 / 0x4042AE..0x4042F4): the villager is unlinked if it is there
void Unlink(Abode& abode, entt::entity villager)
{
	auto& list = abode.inhabitants;
	if (const auto it = std::find(list.begin(), list.end(), villager); it != list.end())
	{
		list.erase(it);
	}
}

/// The counts of RemoveAlive 0x40436F..0x4043B6 / RemoveDeleted 0x404263..0x4042A8 (never below 0)
void DropCounts(Abode& abode, entt::entity villager)
{
	if (villager::IsChild(villager))
	{
		if (abode.childCount != 0)
		{
			--abode.childCount;
		}
		return;
	}
	if (abode.adultCount != 0)
	{
		--abode.adultCount;
	}
	if (abode.adultMaleCount != 0)
	{
		abode.adultMaleCount = static_cast<uint8_t>(abode.adultMaleCount - (IsMaleVillager(villager) ? 1 : 0));
	}
}
} // namespace

// ---- reading -----------------------------------------------------------------------------------------------------

const std::vector<entt::entity>& VillagersOf(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return a != nullptr ? a->inhabitants : k_NoVillagers;
}

uint8_t PresentAtHome(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return a != nullptr ? a->presentAtHome : 0;
}

entt::entity TownOf(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return entt::null;
	}
	const auto& towns = Entities().Context().towns;
	const auto it = towns.find(a->townId);
	return it != towns.end() && Entities().Valid(it->second) ? it->second : entt::null;
}

const GAbodeInfo* InfoOf(entt::entity abode)
{
	const auto town = TownOf(abode);
	const auto* tribe = town != entt::null ? Entities().TryGet<const Tribe>(town) : nullptr;
	// (openblack, guard) as GatherInputs: every scripted town has its Tribe
	return town_stats::AbodeInfoOf(abode, tribe != nullptr ? *tribe : Tribe::CELTIC);
}

uint32_t MaxVillagers(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	return info != nullptr ? info->maxVillagersInAbode : 0;
}

uint32_t MaxChildren(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	return info != nullptr ? info->maxChildrenInAbode : 0;
}

int32_t GetRoomLeftForAdults(entt::entity abode)
{
	// 0x404660: info +0x174 - (u8) +0xB4
	const auto* a = AbodeComponent(abode);
	return static_cast<int32_t>(MaxVillagers(abode)) - (a != nullptr ? a->adultCount : 0);
}

int32_t GetRoomLeftForChildren(entt::entity abode)
{
	// 0x404680: info +0x178 - (u8) +0xB7
	const auto* a = AbodeComponent(abode);
	return static_cast<int32_t>(MaxChildren(abode)) - (a != nullptr ? a->childCount : 0);
}

bool IsTooCrowded(entt::entity abode)
{
	// 0x4046C7..0x404712: MaxVillagers 0 -> 1; fild +0xB4, fidiv MaxVillagers, fcomp percentTooCrowded (+0x1A0):
	// not below (test ah, 1; jne) -> 1
	const auto* info = InfoOf(abode);
	const auto* a = AbodeComponent(abode);
	if (info == nullptr || a == nullptr || info->maxVillagersInAbode == 0)
	{
		return true;
	}
	const float f = static_cast<float>(a->adultCount) / static_cast<float>(info->maxVillagersInAbode);
	return !(f < info->percentTooCrowded);
}

float GetPercentAbodeFullWithAdults(entt::entity abode)
{
	// 0x407057..0x407081: MaxVillagers 0 -> 1; else GetNumAdultsInAbode 0x4070D0 ((float) +0xB4) / MaxVillagers
	const auto max = MaxVillagers(abode);
	const auto* a = AbodeComponent(abode);
	if (max == 0 || a == nullptr)
	{
		return 1.0f;
	}
	return static_cast<float>(a->adultCount) / static_cast<float>(max);
}

float GetPercentAbodeFullWithChildren(entt::entity abode)
{
	// 0x407097..0x4070C2: MaxChildren 0 -> 1; else (u8) +0xB7 / MaxChildren with `div` (an integer), fild
	const auto max = MaxChildren(abode);
	const auto* a = AbodeComponent(abode);
	if (max == 0 || a == nullptr)
	{
		return 1.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(a->childCount) / max);
}

float ScoreForAdding(uint32_t count, uint32_t max, float sameSex, uint32_t listSize, float distance)
{
	// 0x404B64 / 0x404BAC: max 0 -> 0
	if (max == 0)
	{
		return 0.0f;
	}
	// 0x404B8A..0x404BE9: f = count / max (fild, fidiv); not below 1 -> 1
	float f = static_cast<float>(count) / static_cast<float>(max);
	if (!(f < 1.0f))
	{
		f = 1.0f;
	}
	// 0x404BEF..0x404C04: room = 1 - f (fst); room <= 0 (test ah, 0x41; jne) -> room
	const float room = 1.0f - f;
	if (!(room > 0.0f))
	{
		return room;
	}
	// 0x404C42..0x404C74: sex = 1 (0x3F800000); with a list (+0xA4 != 0): ((1 - sameSex / count) + 1) x 0.5
	float sex = 1.0f;
	if (listSize != 0)
	{
		const float share = sameSex / static_cast<float>(listSize);
		const float rest = 1.0f - share;
		const float plusOne = rest + 1.0f;
		sex = plusOne * 0.5f;
	}
	// 0x404C78..0x404CA7: (GetDistanceModifier(d, 500) + 1) x 0.5 x sex x room
	const float modifier = gutils::GetDistanceModifier(distance, k_ScoreDistance);
	const float plusOne = modifier + 1.0f;
	const float half = plusOne * 0.5f;
	const float withSex = half * sex;
	return withSex * room;
}

float CalculateScoreForAddingVillagerToAbode(entt::entity abode, entt::entity villager)
{
	const auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return 0.0f;
	}
	// 0x404B4F IsChild: the children's places (+0x178, +0xB7) or the adults' (+0x174, +0xB4)
	const bool child = villager::IsChild(villager);
	const uint32_t max = child ? MaxChildren(abode) : MaxVillagers(abode);
	const uint32_t count = child ? a->childCount : a->adultCount;
	// 0x404C0A..0x404C41: the villagers of the list (+0xA0, the children too, and the villager itself if it is there)
	// whose info +0x1F8 sex is the villager's, a float sum (fadd 1)
	const auto sex = villager::InfoOf(villager).sex;
	float same = 0.0f;
	for (const auto other : a->inhabitants)
	{
		if (villager::InfoOf(other).sex == sex)
		{
			same = same + 1.0f;
		}
	}
	// 0x404C7F..0x404C86: fn_00605CD0(abode +0x14, villager +0x14)
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(abode), town_queries::PosOf(villager));
	return ScoreForAdding(count, max, same, static_cast<uint32_t>(a->inhabitants.size()), distance);
}

float DesireToGainMale(uint32_t townMales, uint32_t townFemales, uint8_t adults, uint8_t adultMales)
{
	// 0x4074C6..0x40750F: fild qword (u32) + 0.001, the women the same, fdivp
	const float men = static_cast<float>(townMales) + k_Thousandth;
	const float women = static_cast<float>(townFemales) + k_Thousandth;
	const float townRatio = men / women;
	// 0x4074F6..0x407531: (u8) +0xB5 + 0.001 over ((u8) +0xB4 - +0xB5) + 0.001 (a signed int sub), then fsubp
	const float houseMen = static_cast<float>(adultMales) + k_Thousandth;
	const float houseWomen = static_cast<float>(static_cast<int32_t>(adults) - static_cast<int32_t>(adultMales)) + k_Thousandth;
	const float houseRatio = houseMen / houseWomen;
	return townRatio - houseRatio;
}

float CalculateDesireToGainMale(entt::entity abode)
{
	// 0x4074A6..0x4074C4: MaxVillagers 0 or no town -> 0
	const auto* a = AbodeComponent(abode);
	const auto town = TownOf(abode);
	if (a == nullptr || MaxVillagers(abode) == 0 || town == entt::null)
	{
		return 0.0f;
	}
	const auto& stats = Entities().Get<const Town>(town).stats;
	// Town +0x664 / +0x668 = TownStats +0x54 / +0x58 (the men / women of the town, TownStats::Add 0x749315)
	return DesireToGainMale(stats.males, stats.females, a->adultCount, a->adultMaleCount);
}

float DesireToGainVillager(uint32_t townAdults, uint32_t townAdultPlaces, float percentAdults)
{
	// 0x407566..0x407594: (fild qword +0x618 + 0.001) / (fild +0x644 + 0.001), stored (fstp)
	const float adults = static_cast<float>(townAdults) + k_Thousandth;
	const float places = static_cast<float>(static_cast<int32_t>(townAdultPlaces)) + k_Thousandth;
	const float ratio = adults / places;
	// 0x407598..0x40759E: - GetPercentAbodeFullWithAdults (fsubr)
	return ratio - percentAdults;
}

float CalculateDesireToGainVillager(entt::entity abode)
{
	// 0x407546..0x407564: MaxVillagers 0 or no town -> 0
	const auto town = TownOf(abode);
	if (MaxVillagers(abode) == 0 || town == entt::null)
	{
		return 0.0f;
	}
	const auto& stats = Entities().Get<const Town>(town).stats;
	// Town +0x618 adults, +0x644 = TownStats +0x34 adultPlaces
	return DesireToGainVillager(stats.adults, stats.adultPlaces, GetPercentAbodeFullWithAdults(abode));
}

// ---- the list ----------------------------------------------------------------------------------------------------

void AddVillagerToAbode(entt::entity abode, entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	if (AbodeComponent(abode) == nullptr || v == nullptr)
	{
		return;
	}
	// 0x40406E: tv = villager.GetTown()
	const auto oldTown = v->town != entt::null && registry.Valid(v->town) ? v->town : entt::null;
	if (oldTown != entt::null && town_villagers::IsVillagerInHomelessList(oldTown, villager))
	{
		// 0x404079..0x404154: out of the town's homeless list (+0x768, --+0x76C)
		town_villagers::RemoveFromHomelessList(oldTown, villager);
	}
	else if (const auto old = v->abode; old != entt::null && registry.Valid(old))
	{
		// 0x4040DE..0x4040F8: out of its old abode
		RemoveAliveVillagerFromAbode(old, villager);
	}
	else
	{
		// 0x4040FA..0x404151: out of the global vagrants (g_game +0x205BFC / +0x205C00) if it is there
		town_villagers::RemoveFromVagrants(villager);
	}
	// 0x40415A..0x404176: at the head, ++count (RemoveAlive may have touched the component: read it again)
	auto& a = registry.Get<Abode>(abode);
	a.inhabitants.insert(a.inhabitants.begin(), villager);
	// 0x40417C SetAbode 0x750DE0 (the villager's town = the abode's, or 0)
	villager::SetAbode(villager, abode);
	// 0x404181..0x4041B6: the abode's town: AddVillagerToTown when it is not tv, then VillagerMoveIntoAbode (the places
	// left, TownStats +0x4C / +0x50 / +0x30). (aproximado, V3) the TownStats are recomputed each Town::Process: only
	// the counts the original adds at once (AddVillagerToTown's) are kept here
	if (const auto town = TownOf(abode); town != entt::null)
	{
		if (town != oldTown)
		{
			town_villagers::AddVillagerToTown(town, villager);
		}
	}
	auto& again = registry.Get<Abode>(abode);
	// 0x4041BB..0x404210: a child ++ChildCount; an adult: MaleFemale[sex] when empty, ++AdultCount, AdultMaleCount +=
	// IsMaleVillager
	if (villager::IsChild(villager))
	{
		again.childCount = static_cast<uint8_t>(again.childCount + 1);
		return;
	}
	const auto sex = static_cast<size_t>(villager::InfoOf(villager).sex == SexType::Female ? 1 : 0);
	if (again.maleFemale.at(sex) == entt::null)
	{
		again.maleFemale.at(sex) = villager;
	}
	again.adultCount = static_cast<uint8_t>(again.adultCount + 1);
	again.adultMaleCount = static_cast<uint8_t>(again.adultMaleCount + (IsMaleVillager(villager) ? 1 : 0));
}

void RemoveAliveVillagerFromAbode(entt::entity abode, entt::entity villager)
{
	if (AbodeComponent(abode) == nullptr)
	{
		return;
	}
	// 0x404346..0x40435A: inside -> SetTopState(163): ExitAtHome 0x761B40 does the LeaveHome (row 163: +0xC0 = 0)
	if (IsInside(villager))
	{
		villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// 0x404360..0x4043B6 the counts, 0x4043BC..0x404406 the list
	DropCounts(*a, villager);
	Unlink(*a, villager);
	// 0x404406 SetAbode(0)
	villager::SetAbode(villager, entt::null);
	// 0x40440F..0x40442A: with a town, TownStats::VillagerMoveOutOfAbode 0x7494C0 (the places left: recomputed, V3)
}

void RemoveDeletedVillagerFromAbode(entt::entity abode, entt::entity villager)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// 0x404227..0x40424D: MaleFemale[sex] == the villager -> MaleFemale[sex == 0] and MaleFemale[sex] both 0
	const auto sex = static_cast<size_t>(villager::InfoOf(villager).sex == SexType::Female ? 1 : 0);
	if (a->maleFemale.at(sex) == villager)
	{
		a->maleFemale.at(sex == 0 ? 1 : 0) = entt::null;
		a->maleFemale.at(sex) = entt::null;
	}
	// 0x404254..0x4042A8 the counts, 0x4042AE..0x4042F4 the list, 0x4042F4 SetAbode(0)
	DropCounts(*a, villager);
	Unlink(*a, villager);
	villager::SetAbode(villager, entt::null);
	// 0x4042FC..0x404326: with a town, Town::RemoveVillager 0x73E210 and VillagerMoveOutOfAbode (recomputed, V3)
	if (const auto town = TownOf(abode); town != entt::null)
	{
		town_villagers::RemoveVillager(town, villager);
	}
}

void RemoveAllVillagersFromAbode(entt::entity abode)
{
	// 0x404560..0x40457D: from the head, the next (+0xE4) read before HomeDeleted (which takes it out of the list)
	const auto list = VillagersOf(abode);
	for (const auto villager : list)
	{
		if (Entities().Valid(villager))
		{
			villager::HomeDeleted(villager);
		}
	}
}

void ArriveHome(entt::entity abode)
{
	if (auto* a = AbodeComponent(abode))
	{
		a->presentAtHome = static_cast<uint8_t>(a->presentAtHome + 1); // 0x405FA0 inc byte
	}
}

void LeaveHome(entt::entity abode)
{
	if (auto* a = AbodeComponent(abode))
	{
		a->presentAtHome = static_cast<uint8_t>(a->presentAtHome - 1); // 0x405FB0 dec byte
	}
}

void ChildToAdult(entt::entity abode, entt::entity villager)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// 0x404CC3..0x404CFF: --ChildCount when not 0, ++AdultCount, AdultMaleCount += IsMaleVillager
	if (a->childCount != 0)
	{
		--a->childCount;
	}
	a->adultCount = static_cast<uint8_t>(a->adultCount + 1);
	a->adultMaleCount = static_cast<uint8_t>(a->adultMaleCount + (IsMaleVillager(villager) ? 1 : 0));
	// 0x404D05..0x404D1A: with a town, Town::ChildToAdult 0x73AF50 (TownStats::ChildToAdult)
	if (const auto town = TownOf(abode); town != entt::null)
	{
		town_villagers::ChildToAdult(town, villager);
	}
}

uint32_t SwapMaleForFemaleFrom(entt::entity x, entt::entity y)
{
	// 0x407628..0x40765F: the first of y's list that IsMaleVillager and is not inside (+0xE0 & 4)
	entt::entity man = entt::null;
	for (const auto v : VillagersOf(y))
	{
		if (IsMaleVillager(v) && !IsInside(v))
		{
			man = v;
			break;
		}
	}
	if (man == entt::null)
	{
		return 0;
	}
	// 0x407663..0x407696: the first of x's list that IsFemaleVillager and is not inside
	entt::entity woman = entt::null;
	for (const auto v : VillagersOf(x))
	{
		if (IsFemaleVillager(v) && !IsInside(v))
		{
			woman = v;
			break;
		}
	}
	if (woman == entt::null)
	{
		return 0;
	}
	// 0x407696..0x4076A5: ForceMoveVillagerToAbode(man -> x), then (woman -> y)
	villager::ForceMoveVillagerToAbode(man, x);
	villager::ForceMoveVillagerToAbode(woman, y);
	return 1;
}

uint32_t TakeVillagerFrom(entt::entity x, entt::entity y, bool male)
{
	// 0x4075B6..0x407600: y's list from the head: male ? IsMaleVillager : IsFemaleVillager, and not inside
	for (const auto v : VillagersOf(y))
	{
		const bool sex = male ? IsMaleVillager(v) : IsFemaleVillager(v);
		if (sex && !IsInside(v))
		{
			villager::ForceMoveVillagerToAbode(v, x); // 0x407603
			return 1;
		}
	}
	return 0;
}

// ---- the turn ----------------------------------------------------------------------------------------------------

bool RunsAbodeProcess(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	if (info == nullptr)
	{
		return true;
	}
	switch (info->abodeType)
	{
	case AbodeType::Field:          // Field::Process 0x529020 (ecs::Fields)
	case AbodeType::TownCentre:     // TownCentre::Process 0x743DF0
	case AbodeType::Workshop:       // Workshop::Process 0x7797F0
	case AbodeType::SpellDispenser: // SpellDispenser::Process 0x722A70
	case AbodeType::FootballPitch:  // (inferido) its own class, not read
		return false;
	default:
		return true;
	}
}

void ProcessAbode(entt::entity abode)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// 0x404443 MultiMapFixed::Process 0x52F700: with +0x74 (a building site), its Process (vt +0x100). Done by
	// TownProcess step 3 (Edificios' building_sites::Process) for every abode with a site, just before this
	// 0x404448..0x4044B4: empty of adults and of children (== 0, test ah, 0x40), built (vt +0x890), not in a script
	// (GameThingWithPos::IsInScript 0x402280: +0x24 & 0x200; (inferido) no openblack abode has it) and, with a town,
	// one that is not uninhabitable (+0x5F4)
	const auto town = TownOf(abode);
	const bool uninhabitable = town != entt::null && Entities().Get<const Town>(town).uninhabitable;
	if (GetPercentAbodeFullWithAdults(abode) == 0.0f && GetPercentAbodeFullWithChildren(abode) == 0.0f &&
	    abode_queries::IsBuilt(abode) && !uninhabitable)
	{
		// 0x4044B6..0x404503: +0xB0 += 0.001 (fst); at 1 (not below: test ah, 1): +0x7C |= 0x40, ReduceLife(info
		// +0x1B0 emptyAbodeLifeReducer, no player), +0x7C &= ~0x40, +0xB0 = 0
		a->emptyTimer = a->emptyTimer + k_EmptyTimerStep;
		if (!(a->emptyTimer < 1.0f))
		{
			const auto* info = InfoOf(abode);
			const float reducer = info != nullptr ? info->emptyAbodeLifeReducer : 0.0f;
			// (aproximado) Abode::ReduceLife 0x405D90 (vt +0x5B8: the repair site's baseline, StopBeingFunctional)
			// has no entry point in openblack (the physics' Buildings.cpp does its own at a hit): Object::ReduceLife
			// 0x637810's part, ecs::life. The +0x7C bit 0x40 only lives during the call: not kept
			life::ReduceLife(abode, reducer);
			a = AbodeComponent(abode);
			if (a == nullptr)
			{
				return;
			}
			a->emptyTimer = 0.0f;
		}
	}
	// 0x404503..0x40450F: +0xB9 < 200 -> ++ (no reader found, P-8)
	if (a->field0xB9 < k_CounterB9Max)
	{
		++a->field0xB9;
	}
}
} // namespace openblack::ecs::abode_villagers
