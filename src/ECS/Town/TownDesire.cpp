/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesire.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/SkyType.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "Game.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// TownDesire.cpp / Town.cpp of runblack.exe W120 (TownDesire.h)

namespace openblack::ecs::town_desire
{
using namespace components;

namespace
{
WarningSink g_WarningSinkForTests;
std::function<uint32_t(size_t, entt::entity)> g_CheckSatisfyForTests;

// The constants of the exe (all floats unless "double"). Every x87 operation rounds to float: fn_007DEE00 sets the
// control word's precision to 24 bits (`and cw, 0xFCFF` at 0x7DEE0D, from GGame::EndTurn), so the chains are float
// steps; a double constant (fadd qword) is added in double then rounded once to float
constexpr float k_Zero = 0.0f;        // [0x8AA398]
constexpr float k_One = 1.0f;         // [0x8AA390]
constexpr float k_Half = 0.5f;        // [0x8AA3B4]
constexpr float k_Milli = 0.001f;     // [0x8AA3B0]
constexpr float k_Tenth = 0.1f;       // [0x8AB22C]
constexpr float k_Tiny = 0.0001f;     // [0x8BF518]
constexpr float k_TinyAbodes = 1e-5f; // [0x99A100]
constexpr double k_TinyPop = 1e-5;    // [0x99A0E8] (double)
constexpr float k_WarnAt = 0.95f;     // [0x8CF000]
constexpr float k_Two = 2.0f;         // [0x8AB478]
constexpr float k_FoodWarnOffset = 0.9f; // [0x8C5844]
constexpr float k_Three = 3.0f;       // [0x8C2C50]
constexpr float k_OneAndHalf = 1.5f;  // [0x8AB24C]
constexpr float k_HomelessShare = 0.2f; // [0x8AB244]
constexpr float k_MinusOne = -1.0f;   // [0x8AB678]
constexpr float k_UnhappyOffset = 0.6f; // [0x8C7BDC]
constexpr float k_ZeroTrigger = 0.001f; // 0x3A83126F (0x74600E)
constexpr uint32_t k_PlaytimeAfterTurn = 0xFA0; // 0x748877
constexpr uint32_t k_AverageEvery = 50;         // 0x745B55

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The clamp of most desire functions: fcomp 0; test ah, 1 (less or unordered) -> 0; fcomp 1; test ah, 0x41 (less,
/// equal or unordered) -> itself; else 1
float Clamp01(float v)
{
	if (v < 0.0f || std::isnan(v))
	{
		return k_Zero;
	}
	return v <= 1.0f ? v : k_One;
}

/// `fld 1; fcomp st(1); test ah, 1; je`: 1 < x (or unordered) -> 1, else x
float MinOne(float x)
{
	return (1.0f < x || std::isnan(x)) ? 1.0f : x;
}

/// __ftol 0x7A1400: truncation towards 0 (the low dword is kept)
uint32_t Ftol(float v)
{
	return static_cast<uint32_t>(static_cast<int64_t>(v));
}

float Raw(const TownDesire& d, size_t i)
{
	// Town::GetRawDesire 0x73E420: +0x19C, +0x108, +0xC4 of Town (fld, fadd, fadd)
	return d.raw.at(i) + d.boost.at(i) + d.boostA.at(i);
}

float Desire(const TownDesire& d, size_t i)
{
	// Town::GetDesire 0x73E400: +0x14C, +0x108, +0xC4 of Town
	return d.desire.at(i) + d.boost.at(i) + d.boostA.at(i);
}

void Warn(const DesireContext& c, Warning warning, float value)
{
	if (c.warn != nullptr && *c.warn)
	{
		(*c.warn)(warning, value);
	}
}

/// fn_747A90: (int) carried food (ftol of stats +0xF8) + the storage pit's food, or the temporary pot's
uint32_t FoodAvailable(const DesireInputs& in)
{
	uint32_t food = Ftol(in.stats.foodCarried); // 0x747A94..0x747A9A
	if (in.storageFood.has_value())             // 0x747AA3 GetStoragePit
	{
		return food + *in.storageFood; // 0x747AB9 GetResource(FOOD) (vt +0x98)
	}
	if (in.potFood.has_value()) // 0x747AC6 +0x600
	{
		food += *in.potFood;
	}
	return food;
}

/// fn_747AF0: info +0xDC foodWantedMultiplier x stats +0xE4 (left on the x87 stack for the callers: float precision,
/// 0x7DEE0D)
float DesiredFood(const DesireContext& c)
{
	return c.town.foodWantedMultiplier * c.in.stats.foodForDinner;
}

/// fn_747B00: ftol(stats +0x100 + +0xFC) (wood at the sites and carried) + the storage pit's wood, or the pot's
uint32_t WoodAvailable(const DesireInputs& in)
{
	uint32_t wood = Ftol(in.stats.woodAtSites + in.stats.woodCarried); // 0x747B03..0x747B10
	if (in.storageWood.has_value())
	{
		return wood + *in.storageWood; // 0x747B2F GetResource(WOOD)
	}
	if (in.potWood.has_value()) // 0x747B3C +0x604
	{
		wood += *in.potWood;
	}
	return wood;
}

/// fn_747BE0 (factor info +0xE0 minimumWoodForDesire) / fn_747B60 (+0xE4 maximumWoodForDesire): k = abodes (fn_747C60
/// +0x758, fild qword) / info +0xE8 numOfBuildingsForDesiredWood; `fcomp 1; test ah, 0x41; jne` -> k <= 1 uses 1
/// (so k = max(k, 1)); returned in st0 (float precision at 24 bits, 0x7DEE0D): Wood stores it, the modification
/// 0x7463C0 does not, which gives the same value
float WoodScale(const DesireContext& c, float factor)
{
	const float k = static_cast<float>(c.in.abodeCount) / c.town.numOfBuildingsForDesiredWood;
	const float scale = (k <= 1.0f || std::isnan(k)) ? 1.0f : k;
	return scale * factor;
}

float WoodMinimum(const DesireContext& c)
{
	return WoodScale(c, c.town.minimumWoodForDesire); // fn_747BE0
}

float WoodMaximum(const DesireContext& c)
{
	return WoodScale(c, c.town.maximumWoodForDesire); // fn_747B60
}

// ---- Amount / Desired (TownDesire +0x1AC / +0x1F0, read only by the debug trace 0x745EC0) -------------------------

uint32_t AmountAbodes(const DesireContext& c)
{
	return c.in.stats.abodesWithPlaces; // 0x747CF0: Town +0x620
}

uint32_t DesiredAbodes(const DesireContext& c)
{
	// 0x747D00: fn_749BE0 = +0x10 ? (adults + children) / +0x10 : 0; then ftol((that + +0x10 + 0.001) / (+0x30 +
	// 0.001)) (fiadd, fild dword: signed)
	const auto& s = c.in.stats;
	const float perAbode = s.abodesWithPlaces != 0 ? static_cast<float>(s.adults + s.children) /
	                                                      static_cast<int32_t>(s.abodesWithPlaces)
	                                                : 0.0f;
	const float num = perAbode + static_cast<int32_t>(s.abodesWithPlaces) + k_Milli;
	const float den = static_cast<float>(static_cast<int32_t>(s.totalPlaces)) + k_Milli;
	return Ftol(num / den);
}

uint32_t AmountCivic(const DesireContext& c)
{
	return c.in.stats.civicBuildings; // 0x747D50: Town +0x62C
}

uint32_t DesiredCivic(const DesireContext& c)
{
	return c.in.stats.civicPlans; // 0x747D60: Town +0x634
}

uint32_t AmountSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	// 0x747D70: storage pit && worship site ? the pit's food : 0. TODO(milagros2): the worship site's part; 0
	return 0;
}

uint32_t DesiredSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	// 0x747DD0 -> fn_73C980 (worship site -> fn_77B920). TODO(milagros2): 0
	return 0;
}

// ---- the table -------------------------------------------------------------------------------------------------

/// GTownDesireFunction 0xDA32C8 + d x 0x68 (crt_xc 0x744BD0; dev\tmp_dis\aldeanos\v3\emu_dtab_all.py)
const std::array<DesireFunctions, k_Count> k_DesireTable = {{
    // 0 Food: +0x10 the thunk 0x747340 (mov eax, [ecx]; jmp [eax + 0x420]) -> Town::CalculateDesireForFood 0x747F00;
    // CheckSatisfyFoodDesire 0x759F30; modification 0x7462A0; +0x64
    {"Food", DesireForFood, nullptr, nullptr, villager::CheckSatisfyFoodDesire, ModificationFood, false, true},
    // 1 Wood 0x747FF0; CheckSatisfyWoodDesire 0x75F4A0; 0x746350; +0x64
    {"Wood", DesireForWood, nullptr, nullptr, villager::CheckSatisfyWoodDesire, ModificationWood, false, true},
    // 2 Playtime 0x7487B0; CheckSatisfyPlaytimeDesire 0x763130; 0x746490; +0x60
    {"Playtime", DesireForPlaytime, nullptr, nullptr, villager::CheckSatisfyPlaytimeDesire, ModificationGeneral, true,
     false},
    // 3 Protection 0x7488A0; no CheckSatisfy (+0x4C -1); 0x746490; +0x60
    {"Protection", DesireForProtection, nullptr, nullptr, nullptr, ModificationGeneral, true, false},
    // 4 Mercy 0x7488B0; 0x746490; +0x60
    {"Mercy", DesireForMercy, nullptr, nullptr, nullptr, ModificationGeneral, true, false},
    // 5 Abodes 0x748210; Amount 0x747CF0, Desired 0x747D00; CheckSatisfyAbodesDesire 0x758E30; 0x746490
    {"Abodes", DesireForAbodes, AmountAbodes, DesiredAbodes, villager::CheckSatisfyAbodesDesire, ModificationGeneral,
     false, false},
    // 6 Civic_Buildings 0x748330; 0x747D50 / 0x747D60; CheckSatisfyCivicBuildings 0x758E90; 0x746490
    {"Civic_Buildings", DesireForCivicBuildings, AmountCivic, DesiredCivic, villager::CheckSatisfyCivicBuildings,
     ModificationGeneral, false, false},
    // 7 Supply_Worship 0x748320; 0x747D70 / 0x747DD0; CheckSatisfySuppyWorship 0x76CC00; 0x746490
    {"Supply_Worship", DesireForSupplyWorship, AmountSupplyWorship, DesiredSupplyWorship,
     villager::CheckSatisfySuppyWorship, ModificationGeneral, false, false},
    // 8 For_Children 0x748430; no CheckSatisfy; 0x746490; +0x64
    {"For_Children", DesireForChildren, nullptr, nullptr, nullptr, ModificationGeneral, false, true},
    // 9 To_Build 0x748640; CheckSatisfyToBuild 0x759330; 0x746400; +0x64
    {"To_Build", DesireToBuild, nullptr, nullptr, villager::CheckSatisfyToBuild, ModificationToBuild, false, true},
    // 10 For_Rain 0x748690; 0x746490
    {"For_Rain", DesireForRain, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    // 11 For_Sun 0x7486A0; 0x746490
    {"For_Sun", DesireForSun, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    // 12 Repair_Town 0x7486B0; CheckSatisfyToRepair 0x759370; 0x746490; +0x64
    {"Repair_Town", DesireToRepair, nullptr, nullptr, villager::CheckSatisfyToRepair, ModificationGeneral, false, true},
    // 13 Suppy_Workshop (sic) 0x748730; CheckSatisfySupplyWorkshop 0x7593A0; 0x746490
    {"Suppy_Workshop", DesireToSupplyWorkshop, nullptr, nullptr, villager::CheckSatisfySupplyWorkshop,
     ModificationGeneral, false, false},
    // 14 For_Wonder 0x748740; 0x746490
    {"For_Wonder", DesireToBuildWonder, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    // 15 Relaxation 0x7488C0; CheckSatisfyRelaxation 0x761460; 0x746490; +0x60
    {"Relaxation", DesireForRelaxation, nullptr, nullptr, villager::CheckSatisfyRelaxation, ModificationGeneral, true,
     false},
    // 16 Sleep 0x748960; CheckSatisfySleep 0x761490 (V2, VillagerDecide); 0x746490; +0x60
    {"Sleep", DesireForSleep, nullptr, nullptr, villager::CheckSatisfySleep, ModificationGeneral, true, false},
}};

/// The comparator 0x746110: fld a+4; fcomp b+4; test ah, 1 (less or unordered) -> 1; test ah, 0x40 (equal) -> 0; -1
int Compare(const DesireSort& a, const DesireSort& b)
{
	if (a.value < b.value || std::isnan(a.value) || std::isnan(b.value))
	{
		return 1;
	}
	if (a.value == b.value)
	{
		return 0;
	}
	return -1;
}

/// _shortsort 0x7C7FB8: while hi > lo, the max of [lo, hi] (comp(p, max) > 0 takes p, so the last of equal maxima
/// stays the first found) goes to hi
void ShortSort(std::array<DesireSort, k_Count>& a, int lo, int hi)
{
	while (hi > lo) // 0x7C7FC2 / 0x7C7FFF (ja)
	{
		int max = lo;
		for (int p = lo + 1; p <= hi; ++p) // 0x7C7FD6 (ja: p > hi ends)
		{
			if (Compare(a.at(static_cast<size_t>(p)), a.at(static_cast<size_t>(max))) > 0) // 0x7C7FE3 (jle)
			{
				max = p;
			}
		}
		std::swap(a.at(static_cast<size_t>(max)), a.at(static_cast<size_t>(hi))); // _swap 0x7C8006 (a == b: nothing)
		--hi;
	}
}

const DesireSort& Entry(const std::array<DesireSort, k_Count>& sorted, size_t k)
{
	return sorted.at(k);
}

Town* TownOf(entt::entity town)
{
	auto& registry = Entities();
	if (town == entt::null || !registry.Valid(town))
	{
		return nullptr;
	}
	return registry.TryGet<Town>(town);
}

const std::array<GTownDesireInfo, 17>& DesireInfo()
{
	return Locator::infoConstants::value().townDesire;
}

/// The guidance call of a town (GGuidance 0x71CC40 / 0x71CA60 / 0x71CAF0 with the town's HelpTown)
WarningSink SinkFor(entt::entity town)
{
	return [town](Warning warning, float value) {
		if (g_WarningSinkForTests)
		{
			g_WarningSinkForTests(warning, value);
			return;
		}
		const auto help = town_queries::HelpTownOf(town);
		switch (warning)
		{
		case Warning::VillagersUnhappy:
			audio::guidance::HelpSpritesVillagerUnhappy(help);
			break;
		case Warning::LowOnFood:
			audio::guidance::HelpSpritesLowOnFood(help, value);
			break;
		case Warning::LowOnWood:
			audio::guidance::HelpSpritesLowOnWood(help, value);
			break;
		}
	};
}

/// The context of a town entity (its inputs are gathered by the caller)
DesireContext ContextFor(const Town& town, const DesireInputs& in, const WarningSink* warn)
{
	const auto& info = Locator::infoConstants::value();
	// g[0xDA92B4] / g[0xDA92B8]: GVillagerInfo[10] (load_variables 0x42C9AF) +0x264 / +0x268
	const auto& farmer = info.villager.at(10);
	return DesireContext {town.desire, in, info.town, info.townDesire, farmer.maxFoodCarried, farmer.maxWoodCarried,
	                      warn};
}

/// OPENBLACK_TOWN_TRACE="1[,<every>][,raw]": the 17 desires of each town every <every> turns (50) and whenever the
/// first of order 1 changes; with ",raw" order 2 too
struct TownTrace
{
	bool on {false};
	uint32_t every {50};
	bool raw {false};
};

const TownTrace& TraceConfig()
{
	static const TownTrace k_Config = [] {
		TownTrace config;
		const char* value = std::getenv("OPENBLACK_TOWN_TRACE");
		if (value == nullptr || *value == '\0' || std::string(value) == "0")
		{
			return config;
		}
		config.on = true;
		std::string text(value);
		size_t start = text.find(',');
		while (start != std::string::npos)
		{
			const size_t next = text.find(',', start + 1);
			const auto part = text.substr(start + 1, next == std::string::npos ? std::string::npos : next - start - 1);
			if (part == "raw")
			{
				config.raw = true;
			}
			else if (!part.empty())
			{
				config.every = std::max(1u, static_cast<uint32_t>(std::strtoul(part.c_str(), nullptr, 10)));
			}
			start = next;
		}
		return config;
	}();
	return k_Config;
}

void TraceTown(const Town& town, const TownDesire& desire, uint32_t turn, std::optional<float> average)
{
	const auto& config = TraceConfig();
	if (!config.on)
	{
		return;
	}
	static std::unordered_map<uint32_t, uint32_t> s_LastFirst;
	const uint32_t first = desire.sorted.at(0).index;
	const auto last = s_LastFirst.find(town.id);
	const bool changed = last == s_LastFirst.end() || last->second != first;
	s_LastFirst[town.id] = first;
	if (!changed && turn % config.every != 0 && !average.has_value())
	{
		return;
	}
	auto logger = spdlog::get("game");
	if (logger == nullptr)
	{
		return;
	}
	std::string line = fmt::format("town {} turn {} pop {}:", town.id, turn, town.stats.adults + town.stats.children);
	for (const auto& e : desire.sorted)
	{
		const auto d = static_cast<size_t>(e.index) < k_Count ? e.index : 0u;
		line += fmt::format(" [{} {} {:.3f}", e.index, k_DesireTable.at(d).name, e.value);
		if (d == static_cast<size_t>(TownDesireInfo::ForSleep) || desire.raw.at(d) != desire.desire.at(d))
		{
			line += fmt::format(" raw {:.3f}", Raw(desire, d));
		}
		line += "]";
	}
	if (average.has_value())
	{
		line += fmt::format(" avg {:.3f}", *average);
	}
	SPDLOG_LOGGER_INFO(logger, "{}", line);
	if (config.raw)
	{
		std::string raw = fmt::format("town {} turn {} raw:", town.id, turn);
		for (const auto& e : desire.sortedRaw)
		{
			const auto d = static_cast<size_t>(e.index) < k_Count ? e.index : 0u;
			raw += fmt::format(" [{} {} {:.3f}]", e.index, k_DesireTable.at(d).name, e.value);
		}
		SPDLOG_LOGGER_INFO(logger, "{}", raw);
	}
}
} // namespace

const std::array<DesireFunctions, k_Count>& Table()
{
	return k_DesireTable;
}

// ---- the desire functions ----------------------------------------------------------------------------------------

float DesireForFood(const DesireContext& c)
{
	// 0x747F08..0x747F25: a = (u64) fn_747A90 + 1e-4, stored
	const auto a = static_cast<float>(FoodAvailable(c.in)) + k_Tiny;
	// 0x747F29..0x747F3E: v = 1 - a / (fn_747AF0 + 1e-4), stored (fst; at 24 bits (0x7DEE0D) the value the comparison
	// below reads is the stored float)
	const float v = 1.0f - a / (DesiredFood(c) + k_Tiny);
	// 0x747F42..0x747FA0: v >= 0.95 (fcomp, test ah, 1), a player, the local one: HelpSpritesLowOnFood(min(v, 2) - 0.9)
	// with the stored v
	if (v >= k_WarnAt && c.in.hasPlayer && c.in.isLocalPlayer)
	{
		const float m = v < k_Two ? v : k_Two;
		Warn(c, Warning::LowOnFood, m - k_FoodWarnOffset);
	}
	return Clamp01(v); // 0x747FA5..0x747FE5
}

float DesireForWood(const DesireContext& c)
{
	const auto& d = c.desire;
	// 0x747FF7..0x74803A: R12 + R9 + R6 + R5, the raws then the boosts then the boosts A, in this order; < 3 or 3
	const float sum = d.raw.at(12) + d.raw.at(9) + d.raw.at(6) + d.raw.at(5) + d.boost.at(12) +
	                   d.boost.at(9) + d.boost.at(6) + d.boostA.at(12) + d.boost.at(5) + d.boostA.at(9) +
	                   d.boostA.at(6) + d.boostA.at(5);
	// 0x748040..0x74809D: fcomp 3; test ah, 1; je -> 3 (so unordered keeps the sum)
	const float s = (sum < k_Three || std::isnan(sum)) ? sum : k_Three;
	// 0x7480BB..0x7480D3: a = (craftsmen (byte +0x6E0) + 0.001) / (adults (fild qword) + 0.001) + S, stored
	const auto craftsmen = c.in.stats.disciples.at(static_cast<size_t>(VillagerDisciple::Craftsman));
	const float a = (static_cast<float>(craftsmen) + k_Milli) / (static_cast<float>(c.in.stats.adults) + k_Milli) + s;
	// 0x7480D9..0x748100: w = (u64) fn_747B00, B = fn_747BE0, C = fn_747B60 (each stored)
	const auto w = static_cast<float>(WoodAvailable(c.in));
	const auto b = WoodMinimum(c);    // fstp [esp + 0x14]
	const auto cMax = WoodMaximum(c); // fstp [esp + 0x18]
	// 0x748104..0x748152: v = (1 - min(w / C, 1)) x (1 - min(w / B, 1) + a) (fcom 1; test ah, 1; jne keeps x < 1)
	float x = w / b;
	x = (x < 1.0f || std::isnan(x)) ? x : 1.0f;
	const float t = 1.0f - x + a;
	float y = w / cMax;
	y = (y < 1.0f || std::isnan(y)) ? y : 1.0f;
	const auto v = (1.0f - y) * t;
	// 0x74815A..0x7481BC: v >= 0.95, a player, the local one: fn_0071CAF0 (LowOnWood)(min(v, 2) - 1)
	if (v >= k_WarnAt && c.in.hasPlayer && c.in.isLocalPlayer)
	{
		const float m = v < k_Two ? v : k_Two;
		Warn(c, Warning::LowOnWood, m - k_One);
	}
	return Clamp01(v); // 0x7481C1..0x748201
}

float DesireForPlaytime(const DesireContext& c)
{
	// 0x7487B5..0x748870: D0, D1, D5, D6, D9 all strictly below their DesireTriggersVillagerAction (+0x18; fcomp, test
	// ah, 1, je -> 0)
	for (const size_t d : {0u, 1u, 5u, 6u, 9u})
	{
		const auto value = Desire(c.desire, d); // fstp [esp + 8]
		if (!(value < c.info.at(d).desireTriggersVillagerAction))
		{
			return k_Zero;
		}
	}
	// 0x748872..0x748881: the game turn > 4000 (cmp 0xFA0; jbe -> 0)
	return c.in.turn > k_PlaytimeAfterTurn ? k_Tenth : k_Zero;
}

float DesireForProtection(const DesireContext& c)
{
	return c.in.protection; // 0x7488A0: fld [ecx + 0xEC0]
}

float DesireForMercy(const DesireContext& c)
{
	return c.in.mercy; // 0x7488B0: fld [ecx + 0xEBC]
}

float DesireForAbodes(const DesireContext& c)
{
	const auto& s = c.in.stats;
	// 0x748216..0x74824B: a = adults (dword) / (adult places +0x644 (fild dword) + 1e-5), < 1.5 or 1.5, stored
	const float a =
	    static_cast<int32_t>(s.adults) / (static_cast<float>(static_cast<int32_t>(s.adultPlaces)) + k_TinyAbodes);
	const auto aStored = (a < k_OneAndHalf || std::isnan(a)) ? a : k_OneAndHalf;
	// 0x748253..0x7482B2: c = children / (child places +0x650 + 1e-5), min(c, 1.5); compared with a (fcomp, test ah,
	// 0x41, jne: c <= a keeps a), else m = min(c, 1.5) stored
	const float cRatio =
	    static_cast<int32_t>(s.children) / (static_cast<float>(static_cast<int32_t>(s.childPlaces)) + k_TinyAbodes);
	const float cMin = (cRatio < k_OneAndHalf || std::isnan(cRatio)) ? cRatio : k_OneAndHalf;
	const float m = (cMin <= aStored || std::isnan(cMin)) ? aStored : cMin;
	// 0x7482B4..0x7482D9: m^4 x (1 - R9), stored
	const float m4 = m * m * m * m;
	const auto p = m4 * (1.0f - Raw(c.desire, 9));
	// 0x7482DD..0x74831B: (1 - D6) x that, clamped to [0, 1]
	return Clamp01((1.0f - Desire(c.desire, 6)) * p);
}

float DesireForCivicBuildings(const DesireContext& c)
{
	const auto& s = c.in.stats;
	const uint32_t pop = s.adults + s.children; // 0x748338..0x748345 (ebp)
	float sum = k_Zero;                          // [esp + 0x10]
	for (size_t i = 0; i < 16; ++i)              // 0x7483F2..0x7483F6 (jb 0x10)
	{
		// 0x748353: the town has none of that abode number (+0x718[i] == 0)
		if (s.abodesByNumber.at(i) != 0)
		{
			continue;
		}
		// 0x748360..0x74837D: GAbodeInfo::Find(tribe +0x5B8, i) +0x1B4 PopulationWhenNeeded > -1 (jle)
		const auto& needed = c.in.populationWhenNeeded.at(i);
		if (!needed.has_value() || *needed <= -1)
		{
			continue;
		}
		const int32_t p = *needed;
		// 0x74837F..0x7483BA: p == 0, or homeless (fild qword) / pop (fidiv dword) < 0.2 (fcomp; test ah, 1: also
		// unordered, so pop 0 with no homeless passes)
		if (p != 0)
		{
			const float share = static_cast<float>(c.in.homeless) / static_cast<int32_t>(pop);
			if (!(share < k_HomelessShare || std::isnan(share)))
			{
				continue;
			}
		}
		// 0x7483BC: p <= pop (signed, jg)
		if (p > static_cast<int32_t>(pop))
		{
			continue;
		}
		// 0x7483C0..0x7483EE: s += 0.5 (pop - p + 0.001) / (p + 0.001) + 0.5, stored
		const float add =
		    (static_cast<float>(static_cast<int32_t>(pop) - p) + k_Milli) / (static_cast<float>(p) + k_Milli);
		sum = add * k_Half + sum + k_Half;
	}
	// 0x7483FC..0x748422: min(s, 1) (fcomp 1; test ah, 1; je -> 1)
	return (sum < k_One || std::isnan(sum)) ? sum : k_One;
}

float DesireForSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	return k_Zero; // 0x748320: fld 0; ret
}

float DesireForChildren(const DesireContext& c)
{
	const auto& s = c.in.stats;
	// 0x748436..0x748478: A = R0 >= 1 ? 0 : (R0 > 0 ? 1 - R0 : 1)
	float a = k_One;
	const float r0 = Raw(c.desire, 0);
	if (!(r0 < 1.0f || std::isnan(r0)))
	{
		a = k_Zero; // 0x7484DB -> 0x74846C: 1 - 1
	}
	else if (!(r0 <= 0.0f || std::isnan(r0)))
	{
		a = 1.0f - r0; // 0x74846C
	}
	// 0x748478..0x748490: B = children < child places (unsigned, jb) ? 1 : 0
	const float b = s.children < s.childPlaces ? k_One : k_Zero;
	// 0x748498..0x7484AE: C = adults < adult places ? 1 : 0.5
	const float cAdults = s.adults < s.adultPlaces ? k_One : k_Half;
	// 0x7484B6..0x74852A: P = D3 > 0 ? D3 : 0 (stored), M = D4 > 0 ? D4 : 0; Q = (1 - M)(1 - P), stored
	const float d3 = Desire(c.desire, 3);
	const float pProtection = (d3 <= 0.0f || std::isnan(d3)) ? k_Zero : d3;
	const float d4 = Desire(c.desire, 4);
	const float mMercy = (d4 <= 0.0f || std::isnan(d4)) ? 0.0f : d4;
	const auto q = (1.0f - pProtection) * (1.0f - mMercy);
	// 0x748530..0x748561: al = GetPlayer() ? 0.5 x GetAlignmentValue()^3 (the loop of two fmul) : 0, stored
	float al = k_Zero;
	if (c.in.hasPlayer)
	{
		const float alignment = c.in.alignment; // fst [esp + 8]
		al = alignment * alignment * alignment * k_Half;
	}
	// 0x748569..0x748597: X = fn_73E5E0(4) (player ? player +0x68[4] : 1); v = X (1 + al) Q C B A, stored
	const float x = c.in.hasPlayer ? c.in.tribalPower4 : 1.0f;
	auto v = x * (1.0f + al) * q * cAdults * b * a;
	// 0x74857C..0x7485B4: no creche (+0x744) or not IsFunctional (vt +0xD4) == 1 -> v x 0.5
	if (!c.in.crecheFunctional)
	{
		v = v * k_Half;
	}
	return Clamp01(v); // 0x7485B8..0x7485F5
}

float DesireToBuild(const DesireContext& c)
{
	// 0x748640..0x748668: sum of BuildingSite::GetDesireForVillagers 0x43BD70 over the sites +0x790 (each stored)
	float sum = k_Zero;
	for (const float site : c.in.siteDesires)
	{
		sum = site + sum;
	}
	// 0x74866A..0x748689: 1 < s -> 1
	return (k_One < sum || std::isnan(sum)) ? k_One : sum;
}

float DesireForRain([[maybe_unused]] const DesireContext& c)
{
	return k_Zero; // 0x748690
}

float DesireForSun([[maybe_unused]] const DesireContext& c)
{
	return k_Zero; // 0x7486A0
}

float AbodeDesireToBeRepaired(const RepairInput& abode, const GTownInfo& town)
{
	// 0x406976..0x406991: GetPercentRepaired (vt +0x884) > town info +0x10C thresholdToStartRepairing (fcomp; test
	// ah, 0x41; je) -> 0
	if (abode.life > town.thresholdToStartRepairing)
	{
		return k_Zero;
	}
	// 0x406993..0x4069A7: a home (AbodeType +0x120 & 2) with nobody (+0xA4 == 0) -> 0
	if (abode.livingQuarters && abode.inhabitants == 0)
	{
		return k_Zero;
	}
	// MultiMapFixed::GetDesireToBeRepaired 0x52ECE0: IsRepaired (Abode 0x4016A0: life >= 1) -> 0
	if (!(abode.life < k_One || std::isnan(abode.life)))
	{
		return k_Zero;
	}
	// 0x52ECF3..0x52ED2A: min((1 - life) x 0.5 + 0.5) x info +0x118 DesireToBeRepaired, 1) (fcom 1; test ah, 1; jne)
	const float v = ((1.0f - abode.life) * k_Half + k_Half) * abode.desireToBeRepaired;
	return (v < 1.0f || std::isnan(v)) ? v : k_One;
}

float DesireToRepair(const DesireContext& c)
{
	// 0x7486B5..0x7486E1: the abodes +0x754 (next +0x9C) vt +0x8D8 GetDesireToBeRepaired, each stored
	float sum = k_Zero;
	for (const auto& abode : c.in.abodes)
	{
		sum = AbodeDesireToBeRepaired(abode, c.town) + sum;
	}
	// 0x7486E3..0x748704: the plans +0x9A8 (next +0x44) vt +0x514 = PlannedMultiMapFixed::GetDesireToBeRepaired
	// 0x648910 (+0x30 ? info +0x118 : 0), each stored
	for (const float plan : c.in.planRepairDesires)
	{
		sum = plan + sum;
	}
	// 0x748706..0x748726: 1 < s -> 1
	return (k_One < sum || std::isnan(sum)) ? k_One : sum;
}

float DesireToSupplyWorkshop([[maybe_unused]] const DesireContext& c)
{
	return k_Zero; // 0x748730
}

float DesireToBuildWonder(const DesireContext& c)
{
	// 0x748743..0x748753: b = GetBeliefInPlayer(GetPlayer()) 0x73BAB0, stored. TODO(milagros2): not ported (0)
	const float belief = c.in.belief;
	// 0x748757..0x7487A4: b x (1 - min(0.5 (D0 + (D1 + D5)), 0.5)) (fcom 0.5; test ah, 1; jne keeps < 0.5)
	const auto d5 = Desire(c.desire, 5);
	const auto d15 = Desire(c.desire, 1) + d5;
	float h = (Desire(c.desire, 0) + d15) * k_Half;
	h = (h < k_Half || std::isnan(h)) ? h : k_Half;
	return (1.0f - h) * belief;
}

float DesireForRelaxation(const DesireContext& c)
{
	// 0x7488C4..0x7488DC: sky = Time2SkyType(GetVisualTime()) (stored)
	const float visual = c.in.visualHour;
	const float sky = sky_type::At(visual);
	// 0x7488E0..0x748907: fn_557AE0(0.5 x info +0xEC relaxationMod, the same) (both stored floats)
	const auto w = c.town.relaxationMod * k_Half;
	const float ramp = sky_type::EveningRamp(visual, w, w);
	// 0x74890C..0x74892C: x = 1 - sky; x > 0 (fcom 0; test ah, 0x41; je keeps) else 0; R = ramp x x
	float x = 1.0f - sky;
	x = (x <= 0.0f || std::isnan(x)) ? 0.0f : x;
	const float r = ramp * x;
	// 0x74892E..0x74895A: R < 0.1 (or unordered) -> 0.1; R > 1 -> 1
	if (r < k_Tenth || std::isnan(r))
	{
		return k_Tenth;
	}
	return r <= 1.0f ? r : k_One;
}

float DesireForSleep(const DesireContext& c)
{
	// 0x748964..0x74897C: sky = Time2SkyType(GetVisualTime()) (stored)
	const float visual = c.in.visualHour;
	const float sky = sky_type::At(visual);
	// 0x748983..0x7489A0: s = fn_557AE0(1, 0) + sky - info +0xD8 bedTimeMod
	float s = sky_type::EveningRamp(visual, 1.0f, 0.0f) + sky - c.town.bedTimeMod;
	// 0x7489A2..0x7489B9: s > 0 (test ah, 0x41; je keeps) else 0; s^2 (not clamped: 6.25 at night)
	s = (s <= 0.0f || std::isnan(s)) ? 0.0f : s;
	return s * s;
}

// ---- GetDesire, the modifications ---------------------------------------------------------------------------------

float GetDesire(const TownDesire& desire, size_t d)
{
	return Desire(desire, d);
}

float GetRawDesire(const TownDesire& desire, size_t d)
{
	return Raw(desire, d);
}

float GetDesireVillagerModification(const DesireContext& c, size_t d)
{
	// 0x746270: GetDesireFunctions(GetInfo(d)) +0x50 / +0x54 on the town, with d
	const auto fn = k_DesireTable.at(d).modification;
	return fn != nullptr ? fn(c, d) : k_One;
}

float ModificationGeneral(const DesireContext& c, size_t d)
{
	// 0x746493..0x7464E2: 1 - min(+0x510[d] (Town) / ((u64) adults + children + 1e-5 (double)), 1)
	const auto pop = static_cast<float>(static_cast<double>(c.in.stats.adults + c.in.stats.children) + k_TinyPop);
	return 1.0f - MinOne(c.desire.doingNow.at(d) / pop);
}

float ModificationFood(const DesireContext& c, size_t d)
{
	// 0x7462A3..0x7462CE: n = (u64) g[0xDA92B4] (150) x +0x510[d] + 1e-4, stored
	auto n = static_cast<float>(c.farmerMaxFood) * c.desire.doingNow.at(d) + k_Tiny;
	// 0x7462D2..0x74630C: + (GetStoragePit ? (u64) its GetResource(FOOD) : 0), stored (the pot is not read)
	const float store = c.in.storageFood.has_value() ? static_cast<float>(*c.in.storageFood) : 0.0f;
	n = store + n;
	// 0x746310..0x74633D: 1 - min(n / (fn_747AF0 + 1e-4), 1)
	return 1.0f - MinOne(n / (DesiredFood(c) + k_Tiny));
}

float ModificationWood(const DesireContext& c, size_t d)
{
	// 0x746353..0x74637E: (u64) g[0xDA92B8] (250) x +0x510[d] + 1e-4, stored
	auto n = static_cast<float>(c.farmerMaxWood) * c.desire.doingNow.at(d) + k_Tiny;
	// 0x746382..0x7463BC: + the storage pit's wood (GetResource(WOOD)), stored
	const float store = c.in.storageWood.has_value() ? static_cast<float>(*c.in.storageWood) : 0.0f;
	n = store + n;
	// 0x7463C0..0x7463ED: 1 - min(n / (fn_747B60 + 1e-4), 1)
	return 1.0f - MinOne(n / (WoodMaximum(c) + k_Tiny));
}

float ModificationToBuild(const DesireContext& c, [[maybe_unused]] size_t d)
{
	// 0x746404..0x746458: a = 1e-4 + sum (u64) site +0x634, b = 1e-4 + sum (dword) fn_43BBD0(site), each stored
	auto a = k_Tiny; // 0x38D1B717
	auto b = k_Tiny;
	for (size_t i = 0; i < c.in.siteBuilders.size(); ++i)
	{
		a = static_cast<float>(c.in.siteBuilders.at(i)) + a;
		const int32_t places = i < c.in.sitePlaces.size() ? c.in.sitePlaces.at(i) : 0;
		b = static_cast<float>(places) + b;
	}
	// 0x74645A..0x746480: 1 - min(a / b, 1) (no sites: 1e-4 / 1e-4, so 0)
	return 1.0f - MinOne(a / b);
}

float GetTemporaryDesireVillagerModification(const TownDesire& desire, uint32_t population, size_t k)
{
	// 0x7464F3..0x74651D: pop = (u64) Town +0x618 + +0x61C + 1e-5 (double)
	const auto pop = static_cast<float>(static_cast<double>(population) + k_TinyPop);
	// 0x746523..0x746548: x = +0x4DC[k] - +0x454[k]; `fld 0; fcomp st(1); test ah, 0x41; jne` keeps x >= 0, else 0
	float x = desire.doingNow.at(k) - desire.doingNowAtStart.at(k);
	x = (x < 0.0f) ? 0.0f : x;
	// 0x746548..0x746561: 1 - min(x / pop, 1)
	return 1.0f - MinOne(x / pop);
}

// ---- Process -----------------------------------------------------------------------------------------------------

float CallDesireFunction(TownDesire& desire, const DesireContext& c, size_t d)
{
	const auto& entry = k_DesireTable.at(d);
	// 0x745D9D..0x745DA6: no function -> 0 (none of the 17)
	if (entry.function == nullptr)
	{
		return k_Zero;
	}
	// 0x745DB8..0x745DBC: f, stored
	const float f = entry.function(c);
	// 0x745DC6..0x745DFF: x GetInfo(d) +0x58[(GetTribe 0x73C840 - 0xDA57A8) / 0x1C] TribeMultiplier -> +0x168[d]
	const auto tribe = static_cast<size_t>(static_cast<int32_t>(c.in.tribe));
	const auto& multipliers = c.info.at(d).tribeMultiplier;
	// (openblack, guard) a tribe out of the 9 (Tribe::NONE) takes 1: the original always has a GTribeInfo here
	const float multiplier = tribe < multipliers.size() ? multipliers.at(tribe) : k_One;
	const auto raw = multiplier * f;
	desire.raw.at(d) = raw;
	// 0x745E06..0x745E4C: raw x GetDesireVillagerModification(d), < -1 (or unordered) -> -1, > 1 -> 1
	const float v = GetDesireVillagerModification(c, d) * raw;
	if (v < k_MinusOne || std::isnan(v))
	{
		return k_MinusOne;
	}
	return v <= 1.0f ? v : k_One;
}

void ProcessDesire(TownDesire& desire, const DesireContext& c, size_t d)
{
	// 0x745CAC..0x745CC0: +0x4DC[d] < 0 (fcomp; test ah, 1: also unordered) -> 0
	auto& doing = desire.doingNow.at(d);
	if (doing < k_Zero || std::isnan(doing))
	{
		doing = k_Zero;
	}
	// 0x745CCB..0x745CE0: the copies +0x454 / +0x498
	desire.doingNowAtStart.at(d) = desire.doingNow.at(d);
	desire.doingNowCountAtStart.at(d) = desire.doingNowCount.at(d);
	const auto& entry = k_DesireTable.at(d);
	// 0x745CF8..0x745D1C / 0x745D23..0x745D47: Amount / Desired (fild qword: unsigned)
	if (entry.amount != nullptr)
	{
		desire.amount.at(d) = static_cast<float>(entry.amount(c));
	}
	if (entry.desired != nullptr)
	{
		desire.desired.at(d) = static_cast<float>(entry.desired(c));
	}
	// 0x745D4E..0x745D56: +0x118[d] = CallDesireFunction(d)
	desire.desire.at(d) = CallDesireFunction(desire, c, d);
	// 0x745D5D..0x745D6E: [0xDA2770] = ftol([0xDA2770] + desire), GNetwork::ResetStateDebug's checksum: not ported
}

void MsvcQsort(std::array<DesireSort, k_Count>& entries)
{
	// _qsort 0x7C7E64 (num 17, width 12; element indices here: the byte test `higuy - 1 - lo >= hi - loguy` of
	// 0x7C7F43..0x7C7F51 is (higuy - lo) > (hi - loguy) in elements)
	constexpr int k_Cutoff = 8; // 0x7C7EAF
	std::array<int, 30> loStack {};
	std::array<int, 30> hiStack {};
	int stack = 0; // [ebp - 4]
	int lo = 0;
	int hi = static_cast<int>(k_Count) - 1;
	for (;;)
	{
		const int size = hi - lo + 1; // 0x7C7EA6..0x7C7EAE
		if (size <= k_Cutoff)
		{
			ShortSort(entries, lo, hi); // 0x7C7EBA
		}
		else
		{
			// 0x7C7EE3..0x7C7EED: the middle (lo + size / 2) swapped to lo as the pivot
			const int mid = lo + size / 2;
			std::swap(entries.at(static_cast<size_t>(mid)), entries.at(static_cast<size_t>(lo)));
			int loGuy = lo;     // [ebp - 8]
			int hiGuy = hi + 1; // edi
			for (;;)
			{
				// 0x7C7EFA..0x7C7F10: do loguy++ while loguy <= hi && comp(loguy, lo) <= 0
				do
				{
					++loGuy;
				} while (loGuy <= hi &&
				         Compare(entries.at(static_cast<size_t>(loGuy)), entries.at(static_cast<size_t>(lo))) <= 0);
				// 0x7C7F12..0x7C7F22: do higuy-- while higuy > lo && comp(higuy, lo) >= 0
				do
				{
					--hiGuy;
				} while (hiGuy > lo &&
				         Compare(entries.at(static_cast<size_t>(hiGuy)), entries.at(static_cast<size_t>(lo))) >= 0);
				// 0x7C7F27..0x7C7F38: higuy < loguy -> break; else swap(loguy, higuy)
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(entries.at(static_cast<size_t>(loGuy)), entries.at(static_cast<size_t>(hiGuy)));
			}
			// 0x7C7F3A: swap(lo, higuy)
			std::swap(entries.at(static_cast<size_t>(lo)), entries.at(static_cast<size_t>(hiGuy)));
			if (hiGuy - lo > hi - loGuy) // 0x7C7F4F (jl -> the else)
			{
				// 0x7C7F53..0x7C7F72: lo + 1 < higuy -> push (lo, higuy - 1)
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stack)) = lo;
					hiStack.at(static_cast<size_t>(stack)) = hiGuy - 1;
					++stack;
				}
				// 0x7C7F74..0x7C7F7E: loguy < hi -> lo = loguy, again
				if (loGuy < hi)
				{
					lo = loGuy;
					continue;
				}
			}
			else
			{
				// 0x7C7F83..0x7C7F9A: loguy < hi -> push (loguy, hi)
				if (loGuy < hi)
				{
					loStack.at(static_cast<size_t>(stack)) = loGuy;
					hiStack.at(static_cast<size_t>(stack)) = hi;
					++stack;
				}
				// 0x7C7F9C..0x7C7FAE: lo + 1 < higuy -> hi = higuy - 1, again
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		// 0x7C7EC2..0x7C7EE1: pop, or done
		--stack;
		if (stack < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stack));
		hi = hiStack.at(static_cast<size_t>(stack));
	}
}

void SortDesires(TownDesire& desire)
{
	// fn_746140 0x74614F..0x74616C: {+0xD4 + +0x90, +0x118 + +0xD4 + +0x90, d} for d in order, then _qsort 0x746182
	for (size_t d = 0; d < k_Count; ++d)
	{
		auto& e = desire.sorted.at(d);
		e.boosts = desire.boost.at(d) + desire.boostA.at(d);
		e.value = GetDesire(desire, d);
		e.index = static_cast<uint32_t>(d);
	}
	MsvcQsort(desire.sorted);
}

void SortRawDesires(TownDesire& desire)
{
	// fn_746190 0x7461A0..0x7461BD: {+0x90 (a dword copy), +0x168 + +0xD4 + +0x90, d}, then _qsort 0x7461D3
	for (size_t d = 0; d < k_Count; ++d)
	{
		auto& e = desire.sortedRaw.at(d);
		e.boosts = desire.boostA.at(d);
		e.value = GetRawDesire(desire, d);
		e.index = static_cast<uint32_t>(d);
	}
	MsvcQsort(desire.sortedRaw);
}

std::optional<float> Process(TownDesire& desire, const DesireContext& c)
{
	// 0x745AE7..0x745B1E: +0x164 = (u64)(adults + children - on the way +0x5CC - worshipping +0x5C4) (u32 arithmetic)
	const uint32_t people = c.in.stats.children - static_cast<uint32_t>(c.in.onWayToWorship) -
	                        static_cast<uint32_t>(c.in.worshipping) + c.in.stats.adults;
	desire.population = static_cast<float>(people);
	// 0x745B24..0x745B30: fn_745CA0(d) for d = 0..16
	for (size_t d = 0; d < k_Count; ++d)
	{
		ProcessDesire(desire, c, d);
	}
	SortDesires(desire);    // 0x745B34 fn_746140
	SortRawDesires(desire); // 0x745B3B fn_746190
	// 0x745B42 fn_7457C0: the debug trace (only with [0xCD3C74] != 0): openblack's is OPENBLACK_TOWN_TRACE
	// 0x745B47..0x745B7D: turn % 50 == 0, the town (+0x160) and its player
	if (c.in.turn % k_AverageEvery != 0 || !c.in.hasPlayer)
	{
		return std::nullopt;
	}
	// 0x745B83..0x745BF5: max(R5, R6) and max(R4, R3) (`fcom [stored]; test ah, 0x41; je`: the first if greater)
	const float r5 = Raw(desire, 5);
	const auto r6 = Raw(desire, 6);
	const float max56 = r5 > r6 ? r5 : r6;
	const float r4 = Raw(desire, 4);
	const auto r3 = Raw(desire, 3);
	const float max34 = r4 > r3 ? r4 : r3;
	// 0x745BF5..0x745C24: (2 R0 + raw1 + boost1 + boostA1 + max34 + max56) / info +0x158 divisorForAverageDesires
	float sum = Raw(desire, 0);
	sum = sum + sum;
	sum = sum + desire.raw.at(1) + desire.boost.at(1) + desire.boostA.at(1);
	sum = sum + max34 + max56;
	const float average = sum / c.town.divisorForAverageDesires;
	// 0x745C2A..0x745C46: player +0xA44 +0x70 += 1, +0x6C += 1 - avg. TODO(estadísticas del jugador): openblack has no
	// GPlayer +0xA44 (P-5)
	// 0x745C4C..0x745C8A: avg > info +0x15C thresholdForAverageDesiresHelpSprites (test ah, 0x41; jne skips), the
	// local player: HelpSpritesVillagerUnhappy(town, avg - 0.6)
	const auto stored = average; // fst [esp + 0xC]
	if (!(average <= c.town.thresholdForAverageDesiresHelpSprites || std::isnan(average)) && c.in.isLocalPlayer)
	{
		Warn(c, Warning::VillagersUnhappy, stored - k_UnhappyOffset);
	}
	return stored;
}

uint32_t CheckVillagerNeeded(const TownDesire& desire, const std::array<GTownDesireInfo, 17>& info, uint32_t population,
                             bool child, float trigger, const std::function<uint32_t(size_t d)>& checkSatisfy,
                             const std::function<void(const ShareOutStep&)>& trace)
{
	// 0x745FF3..0x74600E: trigger == 0 (fcomp 0; test ah, 0x40: also unordered) -> 0.001
	if (trigger == k_Zero || std::isnan(trigger))
	{
		trigger = k_ZeroTrigger;
	}
	// (child: the caller's IsChild, vt +0xAF8 0x74601F)
	for (size_t k = 0; k < k_Count; ++k) // 0x7460DB..0x7460E2
	{
		const auto& e = Entry(desire.sorted, k);
		// (openblack) an index out of the table cannot happen (Process writes 0..16); skip it
		if (static_cast<size_t>(e.index) >= k_Count)
		{
			continue;
		}
		const size_t d = e.index;
		const auto& entry = k_DesireTable.at(d);
		// 0x746031..0x74605F: t = trigger + DesireSort::GetInfo 0x746570 +0x18; < 1 or 1 (fcom 1; test ah, 1; je),
		// stored as a float
		const float sum = trigger + info.at(d).desireTriggersVillagerAction;
		const float t = (sum < 1.0f || std::isnan(sum)) ? sum : k_One;
		// 0x74605F..0x746073: a child and no +0x60 -> next (no cut)
		if (child && !entry.children)
		{
			if (trace)
			{
				trace({k, e.index, e.value, 0.0f, t, "skip(child)"});
			}
			continue;
		}
		// 0x746075..0x74609A: no CheckSatisfy (+0x40) -> next (no cut)
		if (entry.checkSatisfy == nullptr)
		{
			if (trace)
			{
				trace({k, e.index, e.value, 0.0f, t, "skip(nocs)"});
			}
			continue;
		}
		// 0x74609C..0x7460B1: GetTemporaryDesireVillagerModification(k) x value <= t (fcomp; test ah, 0x41; jne) ->
		// return 0. Literal oddity (PLAN §0.5): the modification is asked with the loop index k, not with the
		// desire's index d, so position k of the order is corrected with the counters of desire number k
		const float temporary = GetTemporaryDesireVillagerModification(desire, population, k);
		const float m = temporary * e.value;
		if (m <= t || std::isnan(m))
		{
			if (trace)
			{
				trace({k, e.index, e.value, temporary, t, "cut"});
			}
			return 0; // 0x7460EB
		}
		// 0x7460B3..0x7460D9: CheckSatisfy (+0x40 / +0x44 on the villager) == 1 -> 1
		const uint32_t satisfied = checkSatisfy(d);
		if (trace)
		{
			trace({k, e.index, e.value, temporary, t, satisfied == 1 ? "cs=1" : "cs=0"});
		}
		if (satisfied == 1)
		{
			return 1; // 0x7460F7
		}
	}
	return 0;
}

float GetDesireSignificanceToVillager(const TownDesire& desire, const GTownDesireInfo& info, size_t d)
{
	// 0x746664..0x74669F: D(d) (stored) - info +0x18; > 0 (fcom 0; test ah, 0x41; je keeps) else 0
	const auto value = Desire(desire, d);
	const float v = value - info.desireTriggersVillagerAction;
	return (v <= 0.0f || std::isnan(v)) ? k_Zero : v;
}

int GetMostDesired(const TownDesire& desire)
{
	// 0x745E50..0x745E7F: best = 0, -1; desire[d] > best (fcom; test ah, 1) takes it
	float best = k_Zero;
	int index = -1;
	for (size_t d = 0; d < k_Count; ++d)
	{
		if (best < desire.desire.at(d))
		{
			best = desire.desire.at(d);
			index = static_cast<int>(d);
		}
	}
	return index;
}

int GetMostSignificantRawDesire(const TownDesire& desire, float minimum)
{
	// 0x745EA0..0x745EBD: order2[0].value (+0x348) < m (fcomp; test ah, 1: also unordered) -> -1, else its index
	const auto& first = desire.sortedRaw.at(0);
	if (first.value < minimum || std::isnan(first.value) || std::isnan(minimum))
	{
		return -1;
	}
	return static_cast<int>(first.index);
}

int FindDesire(std::string_view name)
{
	// fn_747270: _stricmp against the 17 names; -1
	for (size_t d = 0; d < k_Count; ++d)
	{
		const std::string_view entry(k_DesireTable.at(d).name);
		if (entry.size() == name.size() &&
		    std::equal(entry.begin(), entry.end(), name.begin(), [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		    }))
		{
			return static_cast<int>(d);
		}
	}
	return -1;
}

// ---- the entity layer --------------------------------------------------------------------------------------------

namespace
{
const std::array<DesireSort, k_Count> k_EmptyOrder {};

size_t Index(TownDesireInfo d)
{
	return static_cast<size_t>(static_cast<int>(d));
}

bool ValidDesire(TownDesireInfo d)
{
	return static_cast<int>(d) >= 0 && Index(d) < k_Count;
}
} // namespace

float GetDesire(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	return t != nullptr && ValidDesire(d) ? GetDesire(t->desire, Index(d)) : k_Zero;
}

float GetRawDesire(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	return t != nullptr && ValidDesire(d) ? GetRawDesire(t->desire, Index(d)) : k_Zero;
}

float TownNeedsSum(const TownDesire& desire)
{
	// 0x747150..0x747186: fld +0x19C (13); fadd +0x198 (12), +0x18C (9), +0x184 (7), +0x180 (6), +0x17C (5), +0x178 (4),
	// +0x174 (3), +0x16C (1), +0x168 (0): the raw desires (+0x168, no boosts), in that order on the x87 stack, each fadd
	// rounded to float (24-bit x87 control word, 0x7DEE0D)
	const auto& raw = desire.raw;
	float sum = raw.at(13);
	for (const size_t d : {12u, 9u, 7u, 6u, 5u, 4u, 3u, 1u, 0u})
	{
		sum += raw.at(d);
	}
	// 0x74718C: x 0.2 ([0x8AA3AC], a float); 0x747192..0x7471B7: < 0 -> 0, > 1 -> 1
	sum *= 0.2f;
	return sum < 0.0f ? 0.0f : sum > 1.0f ? 1.0f : sum;
}

float TownNeedsSum(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? TownNeedsSum(t->desire) : 0.0f;
}

const std::array<DesireSort, k_Count>& GetSortedDesires(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? t->desire.sorted : k_EmptyOrder;
}

const std::array<DesireSort, k_Count>& GetSortedRawDesires(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? t->desire.sortedRaw : k_EmptyOrder;
}

float GetField(entt::entity town, TownDesireInfo d, Field field)
{
	const auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	const auto i = Index(d);
	switch (field)
	{
	case Field::BoostA:
		return t->desire.boostA.at(i);
	case Field::Boost:
		return t->desire.boost.at(i);
	case Field::Desire:
		return t->desire.desire.at(i);
	case Field::Raw:
		return t->desire.raw.at(i);
	}
	return k_Zero;
}

float GetDesireSignificanceToVillager(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	return GetDesireSignificanceToVillager(t->desire, DesireInfo().at(Index(d)), Index(d));
}

int GetMostDesired(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetMostDesired(t->desire) : -1;
}

int GetMostSignificantRawDesire(entt::entity town, float minimum)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetMostSignificantRawDesire(t->desire, minimum) : -1;
}

DesireInputs GatherInputs(entt::entity town)
{
	DesireInputs in;
	auto& registry = Entities();
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return in;
	}
	in.stats = t->stats;
	if (const auto* magic = registry.TryGet<const TownMagic>(town); magic != nullptr)
	{
		in.worshipping = magic->worshipping;      // +0x5C4 (milagros2, read only)
		in.onWayToWorship = magic->onWayToWorship; // +0x5CC
	}
	in.homeless = static_cast<uint32_t>(t->homelessVillagers.size()); // +0x76C
	const auto* tribe = registry.TryGet<const Tribe>(town);
	in.tribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) every scripted town has its Tribe
	// +0x754 / +0x758 (the fields are abodes too)
	const auto abodes = town_stats::AbodesOf(town);
	in.abodeCount = static_cast<uint32_t>(abodes.size());
	const auto livingQuarters = static_cast<uint32_t>(AbodeType::LivingQuarters);
	for (const auto abode : abodes)
	{
		RepairInput repair;
		repair.life = life::LifeOf(abode);
		const auto* info = town_stats::AbodeInfoOf(abode, in.tribe);
		repair.livingQuarters = info != nullptr && (static_cast<uint32_t>(info->abodeType) & livingQuarters) != 0;
		repair.inhabitants = static_cast<uint32_t>(registry.Get<Abode>(abode).inhabitants.size());
		repair.desireToBeRepaired = info != nullptr ? info->desireToBeRepaired : 0.0f;
		in.abodes.push_back(repair);
	}
	// GetStoragePit 0x73B5B0 and its GetResource (PotStructure 0x66EF00: StoragePitStore)
	if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
	{
		in.storageFood = StoragePitStore::GetResource(pit, ResourceType::Food);
		in.storageWood = StoragePitStore::GetResource(pit, ResourceType::Wood);
	}
	// +0x600 / +0x604 the temporary pots (ecs::town_stores): fn_747A90 0x747AC6 / fn_747B00 0x747B3C test the slot only
	// (no IsAvailable), then GetResource (vt +0x98: PotStructure 0x66EF00, object_resources). (approximate) a slot whose
	// entity is gone counts as empty (until TownProcess step 17 clears it, Edificios, a recycled entity could be read)
	if (const auto pot = t->temporaryPots.at(0); pot != entt::null && registry.Valid(pot))
	{
		in.potFood = object_resources::GetResource(pot, ResourceType::Food);
	}
	if (const auto pot = t->temporaryPots.at(1); pot != entt::null && registry.Valid(pot))
	{
		in.potWood = object_resources::GetResource(pot, ResourceType::Wood);
	}
	// +0x790 the building sites (head first) and +0x9A8 the plans (oldest first): Edificios' DesireInputsOf, in the
	// list orders the float sums keep
	auto sites = building_sites::DesireInputsOf(town);
	in.siteDesires = std::move(sites.siteDesires);
	in.siteBuilders = std::move(sites.siteBuilders);
	in.sitePlaces = std::move(sites.sitePlaces);
	in.planRepairDesires = std::move(sites.planRepairDesires);
	for (size_t i = 0; i < in.populationWhenNeeded.size(); ++i)
	{
		if (const auto* info = town_stats::FindAbodeInfo(in.tribe, static_cast<AbodeNumber>(i)); info != nullptr)
		{
			in.populationWhenNeeded.at(i) = info->populationWhenNeeded;
		}
	}
	// Town::GetPlayer (+0x2C): always one in openblack (NEUTRAL when the script gave none)
	in.hasPlayer = true;
	// GPlayer::IsMemberOfThisPlayer(MyInterfaceStatus) 0x64D750. (inferido) the local player is PLAYER_ONE
	in.isLocalPlayer = t->owner == PlayerNames::PLAYER_ONE;
	in.alignment = effects::alignment::Get(t->owner);                          // GetAlignmentValue 0x64D6A0
	in.tribalPower4 = magic::players::MagicOf(t->owner).tribalPower.at(4);     // 0x73E5E0(4): player +0x68[4]
	const auto creche = town_queries::GetCreche(town);                          // +0x744
	in.crecheFunctional = creche != entt::null && abode_queries::IsFunctional(creche);
	// TODO(milagros2): Town::GetBeliefInPlayer 0x73BAB0 is not ported (For_Wonder 0)
	in.belief = 0.0f;
	in.protection = t->protectionDesire; // +0xEC0
	in.mercy = t->mercyDesire;           // +0xEBC
	// GGameInfo::GetVisualTime 0x5575A0 = [0xBF3380]: the Game's DayNightClock (the single source)
	// (openblack, guard) without a Game (the tests) noon
	in.visualHour = Game::Instance() != nullptr ? Game::Instance()->GetDayNightClock().GetVisualTime() : 12.0f;
	in.turn = villager::CurrentTurn();
	return in;
}

void Process(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	const auto context = ContextFor(*t, in, &sink);
	const auto average = Process(t->desire, context);
	TraceTown(*t, t->desire, in.turn, average);
}

float CalculateDesireForFood(entt::entity town)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	return DesireForFood(ContextFor(*t, in, &sink));
}

float FoodDesireValue(entt::entity town)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	return DesireForFood(ContextFor(*t, in, nullptr));
}

float CallDesireFunctionNow(entt::entity town, TownDesireInfo d)
{
	auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	return CallDesireFunction(t->desire, ContextFor(*t, in, &sink), Index(d));
}

uint32_t CheckVillagerNeededForTownDesire(entt::entity town, entt::entity villager, float trigger)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return 0;
	}
	const auto check = [villager](size_t d) -> uint32_t {
		if (g_CheckSatisfyForTests)
		{
			return g_CheckSatisfyForTests(d, villager);
		}
		const auto fn = k_DesireTable.at(d).checkSatisfy;
		return fn != nullptr ? fn(villager) : 0;
	};
	std::function<void(const ShareOutStep&)> trace;
	if (villager::TraceOn(villager))
	{
		trace = [villager](const ShareOutStep& s) {
			villager::Trace(villager, fmt::format("civic: t={:.4f} k={} d={} v={:.4f} tmp={:.4f} -> {}", s.threshold,
			                                      s.k, s.d, s.value, s.temporary, s.result));
		};
	}
	// pop of GetTemporaryDesireVillagerModification: Town +0x618 + +0x61C
	const uint32_t population = t->stats.adults + t->stats.children;
	return CheckVillagerNeeded(t->desire, DesireInfo(), population, villager::IsChild(villager), trigger, check, trace);
}

void SetBoost(entt::entity town, TownDesireInfo d, float boost, bool resort)
{
	auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return;
	}
	t->desire.boost.at(Index(d)) = boost;
	if (resort)
	{
		SortDesires(t->desire); // fn_746140 only
	}
}

std::array<int32_t, k_Count>& AlignmentTurns(entt::entity town)
{
	static std::array<int32_t, k_Count> s_None {};
	auto* t = TownOf(town);
	return t != nullptr ? t->desire.alignmentTurns : s_None;
}

// ---- scripts -----------------------------------------------------------------------------------------------------

bool ScriptSetTownDesireBoost(entt::entity thing, int32_t desire, float boost, std::vector<std::string>* errors)
{
	auto* t = TownOf(thing);
	// 0x6FE696..0x6FE6B2: no thing, or not IsTown (vt +0x1B8) -> "Thing not valid!" (0xC0DA54)
	if (t == nullptr && errors != nullptr)
	{
		errors->emplace_back("Thing not valid!");
	}
	// 0x6FE6C3..0x6FE6EF: desire >= 17 (signed, jge), boost < -1 (or unordered), boost > 1 -> "Invalid Params"
	const bool rangeOk = desire < static_cast<int32_t>(k_Count) && !(boost < k_MinusOne || std::isnan(boost)) &&
	                     boost <= k_One;
	if (!rangeOk && errors != nullptr)
	{
		errors->emplace_back("Invalid Params");
	}
	// 0x6FE6F7..0x6FE73E: both right -> +0xD4[desire] = boost (Town +0x108) and fn_746140 (order 1 only).
	// (aproximado) desire < 0, which the original writes out of the array, is not written
	if (!rangeOk || t == nullptr || desire < 0)
	{
		return false;
	}
	t->desire.boost.at(static_cast<size_t>(desire)) = boost;
	SortDesires(t->desire);
	return true;
}

float ScriptGetDesire(int32_t d, const std::function<entt::entity()>& popObject, std::vector<std::string>* errors)
{
	// 0x6FCCB5..0x6FCCF1: d out of [0, 17) -> "Invalid desire" (0xC0CFC0) and PUSH 0 without the second POP
	if (d < 0 || d >= static_cast<int32_t>(k_Count))
	{
		if (errors != nullptr)
		{
			errors->emplace_back("Invalid desire");
		}
		return k_Zero;
	}
	// 0x6FCCF2..0x6FCD3B: POP the object, GetScriptGameThing 0x70D220; none -> "Object no longer valid" (0xC0D428)
	const auto thing = popObject();
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing))
	{
		if (errors != nullptr)
		{
			errors->emplace_back("Object no longer valid");
		}
		return k_Zero;
	}
	// 0x6FCD3C..0x6FCD48: not IsTown (vt +0x1B8) -> PUSH 0 (no message: je 0x6FCD1E is after it)
	const auto* t = registry.TryGet<const Town>(thing);
	if (t == nullptr)
	{
		return k_Zero;
	}
	// 0x6FCD4A..0x6FCD5C: PUSH GetRawDesire(d) (a float)
	return GetRawDesire(t->desire, static_cast<size_t>(d));
}

void MapTownDesireBoost(entt::entity town, std::string_view name, float value)
{
	// 0x7179EC: FindTownWithID 0x552FA0 (the caller), fn_747270(name); both -> +0xD4[d] = value (0x717A26), no
	// re-sort, no range check
	auto* t = TownOf(town);
	const int d = FindDesire(name);
	if (t == nullptr || d == -1)
	{
		return;
	}
	t->desire.boost.at(static_cast<size_t>(d)) = value;
}

void SetWarningSinkForTests(WarningSink sink)
{
	g_WarningSinkForTests = std::move(sink);
}

void SetCheckSatisfyForTests(std::function<uint32_t(size_t d, entt::entity villager)> checkSatisfy)
{
	g_CheckSatisfyForTests = std::move(checkSatisfy);
}
} // namespace openblack::ecs::town_desire
