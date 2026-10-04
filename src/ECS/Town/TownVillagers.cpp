/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownVillagers.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMourning.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"

// Town.cpp of runblack.exe W120 (TownVillagers.h)

namespace openblack::ecs::town_villagers
{
using namespace components;

namespace
{
const std::vector<entt::entity> k_NoVillagers;
/// g_game +0x205BFC / +0x205C00 (Vagrants())
std::vector<entt::entity> g_Vagrants;
/// ShuffleDue 0x7475C8: the town id x 20 [0x8C7658]
constexpr float k_ShuffleIdFactor = 20.0f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

/// TownStats::Add(Villager) 0x7492E0 / Remove 0x7493C0, the counts V4 reads at once (the rest is recomputed by
/// town_stats::Compute at the start of each Town::Process, V3): adults / children (+0x08 / +0x0C) and the sexes
/// (+0x54 / +0x58, children too)
void CountVillager(TownStats& stats, entt::entity villager, int32_t k)
{
	// (openblack, guard) not below 0: the counts are a cache that town_stats::Compute rebuilds each Town::Process, and a
	// removal before the first one would wrap
	const auto add = [k](uint32_t& count) {
		count = k < 0 && count == 0 ? 0u : static_cast<uint32_t>(static_cast<int32_t>(count) + k);
	};
	add(villager::IsChild(villager) ? stats.children : stats.adults);
	add(villager::InfoOf(villager).sex == SexType::Female ? stats.females : stats.males);
}

/// _shortsort 0x7C7FB8 with ShuffleCompare (town_desire::ShortSort's rule)
void ShortSort(std::vector<ShuffleEntry>& a, int lo, int hi)
{
	while (hi > lo)
	{
		int max = lo;
		for (int p = lo + 1; p <= hi; ++p)
		{
			if (ShuffleCompare(a.at(static_cast<size_t>(p)), a.at(static_cast<size_t>(max))) > 0)
			{
				max = p;
			}
		}
		std::swap(a.at(static_cast<size_t>(max)), a.at(static_cast<size_t>(hi)));
		--hi;
	}
}

/// |m^2 + v^2| as 0x74165B..0x741670 / 0x7417C4..0x7417E8 compute it (fld v; fld m; m x m; v x v; faddp; fabs)
float Magnitude(float m, float v)
{
	const float mm = m * m;
	const float vv = v * v;
	return std::fabs(mm + vv);
}
} // namespace

// ---- the homeless and the vagrants -------------------------------------------------------------------------------

const std::vector<entt::entity>& Homeless(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->homelessVillagers : k_NoVillagers;
}

bool IsVillagerInHomelessList(entt::entity town, entt::entity villager)
{
	// 0x73B580: the walk from +0x768 by +0xE4
	const auto& list = Homeless(town);
	return std::find(list.begin(), list.end(), villager) != list.end();
}

void AddToHomelessList(entt::entity town, entt::entity villager)
{
	if (auto* t = TownComponent(town))
	{
		t->homelessVillagers.insert(t->homelessVillagers.begin(), villager); // the head, ++ +0x76C
	}
}

bool RemoveFromHomelessList(entt::entity town, entt::entity villager)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return false;
	}
	auto& list = t->homelessVillagers;
	const auto it = std::find(list.begin(), list.end(), villager);
	if (it == list.end())
	{
		return false;
	}
	list.erase(it); // -- +0x76C, villager +0xE4 = 0
	return true;
}

const std::vector<entt::entity>& Vagrants()
{
	return g_Vagrants;
}

bool IsVagrant(entt::entity villager)
{
	return std::find(g_Vagrants.begin(), g_Vagrants.end(), villager) != g_Vagrants.end();
}

bool RemoveFromVagrants(entt::entity villager)
{
	const auto it = std::find(g_Vagrants.begin(), g_Vagrants.end(), villager);
	if (it == g_Vagrants.end())
	{
		return false;
	}
	g_Vagrants.erase(it);
	return true;
}

void ClearVagrants()
{
	g_Vagrants.clear();
}

// ---- joining, moving, eating -------------------------------------------------------------------------------------

bool AddVillagerToTown(entt::entity town, entt::entity villager)
{
	auto& registry = Entities();
	auto* t = TownComponent(town);
	if (t == nullptr || !registry.AllOf<Villager>(villager))
	{
		return false;
	}
	// 0x73A095..0x73A09D: +0x5F4 (uninhabitable) -> 0
	if (t->uninhabitable)
	{
		return false;
	}
	// 0x73A0A7..0x73A0B6: TownStats::Add(v), SetTown(this)
	CountVillager(t->stats, villager, 1);
	villager::SetTown(villager, town);
	// 0x73A0BB..0x73A0DF: an abode of this town -> on to the end; of another one -> out of it, SetAbode(0)
	const auto abode = registry.Get<const Villager>(villager).abode;
	bool done = false;
	if (abode != entt::null && registry.Valid(abode))
	{
		if (abode_villagers::TownOf(abode) == town)
		{
			done = true;
		}
		else
		{
			abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
			villager::SetAbode(villager, entt::null);
			// SetAbode(0) also clears the town (0x750DE8): the original's +0x12C is 0 now (literal)
		}
	}
	if (!done)
	{
		// 0x73A0E4..0x73A101: FindAbodeWithSpaceInTown(v, 0) -> AddVillagerToAbode, 1 (no worship site check)
		if (const auto found = FindAbodeWithSpaceInTown(town, villager, 0.0f); found != entt::null)
		{
			abode_villagers::AddVillagerToAbode(found, villager);
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager, fmt::format("town: into abode {} (AddVillagerToTown)", static_cast<uint32_t>(found)));
			}
			return true;
		}
		// 0x73A105 MakeHomelessNoStateChange
		villager::MakeHomelessNoStateChange(villager);
	}
	// 0x73A10C..0x73A11F: adults + children == 1 -> Town::CheckAddWorshipSite 0x740BF0 (milagros2)
	if (const auto* again = TownComponent(town); again != nullptr && again->stats.adults + again->stats.children == 1)
	{
		worship::town::CheckAddWorshipSite(town);
	}
	return true;
}

entt::entity FindAbodeWithSpaceInTown(entt::entity town, entt::entity villager, float minimum)
{
	// 0x73B371..0x73B3B6: the list +0x754 (next +0x9C), the newest first; IsFunctional (vt +0xD4) and the score
	// strictly above the best (fcom; test ah, 0x41; jne), the best starting at `minimum`
	entt::entity best = entt::null;
	float bestScore = minimum;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (!abode_queries::IsFunctional(abode))
		{
			continue;
		}
		const float score = abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager);
		if (score > bestScore)
		{
			bestScore = score;
			best = abode;
		}
	}
	return best;
}

void ChildToAdult(entt::entity town, entt::entity villager)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// TownStats::ChildToAdult 0x749490: the places (+0x4C / +0x50, with an abode) are recomputed (V3); 0x7494AE..0x7494B9
	// children - 1, adults + 1
	// (openblack, guard) the children not below 0 (a cache rebuilt each Town::Process)
	t->stats.children = t->stats.children != 0 ? t->stats.children - 1 : 0;
	t->stats.adults = t->stats.adults + 1;
	(void)villager;
}

void UseFood(entt::entity town, uint32_t amount)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// 0x73B5EA..0x73B600: fild qword (u32), fadd +0x6F8, fstp
	t->foodUsed = static_cast<float>(static_cast<double>(amount) + static_cast<double>(t->foodUsed));
	// 0x73B606..0x73B60F: GetPlayer() +0xA44 -> +0xA4 += n. TODO(estadísticas): openblack keeps no player statistics
}

void RemoveVillager(entt::entity town, entt::entity villager)
{
	auto& registry = Entities();
	auto* t = TownComponent(town);
	if (t == nullptr || !registry.AllOf<Villager>(villager))
	{
		return;
	}
	// 0x73E21C FindChildrenAndOrphanThem 0x756BE0 (V12, ecs::villager_mourning): the children whose mother it is go to
	// 131 MORN_DEATH and lose her
	villager_mourning::FindChildrenAndOrphanThem(villager);
	// 0x73E231 TownStats::Remove 0x7493C0
	CountVillager(t->stats, villager, -1);
	// 0x73E238..0x73E24C: an abode -> RemoveAliveVillagerFromAbode, SetAbode(0); else out of the homeless list
	const auto abode = registry.Get<const Villager>(villager).abode;
	if (abode != entt::null && registry.Valid(abode))
	{
		abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
		villager::SetAbode(villager, entt::null);
	}
	else
	{
		RemoveFromHomelessList(town, villager);
	}
	// 0x73E29F RemoveVillagerOnWayToWorshipSite 0x73E360 (milagros2's worship::percentage, the town's on-the-way list)
	worship::percentage::RemoveVillagerOnWay(town, villager);
	// 0x73E2A7..0x73E2B2: flags (+0xE0) & 2 -> RemoveVillagerFromWorshipSite 0x76C440 (it needs the town still set:
	// fn_0073E3F0 through GetTown); it sets no state
	if (villager_worship::IsAtWorshipSite(villager))
	{
		villager_worship::RemoveVillagerFromWorshipSite(villager);
	}
	// 0x73E2B7 SetTown(0)
	villager::SetTown(villager, entt::null);
	// 0x73E2BF..0x73E2CD: adults + children == 0 -> +0xF20 = 50 (TownProcess counts it down; Town::SetTownEmpty
	// 0x741080 at 0 is TODO(towns))
	if (auto* now = TownComponent(town); now != nullptr && now->stats.adults + now->stats.children == 0)
	{
		now->emptyCountdown = 50;
	}
	// 0x73E2D8: mother (+0x100) = 0
	if (auto* v = registry.TryGet<Villager>(villager))
	{
		v->mother = entt::null;
	}
}

// ---- the shuffle -------------------------------------------------------------------------------------------------

bool ShuffleDue(const Town& town, uint32_t turn)
{
	const auto every = Locator::infoConstants::value().town.shuffleVillagersEvery;
	if (every == 0)
	{
		return false; // (openblack, guard) the original divides by zero
	}
	// 0x7475A3..0x7475D2: fild qword (u32) id; fmul 20 (float); fiadd (int32) turn; __ftol
	const float scaled = static_cast<float>(static_cast<double>(town.id)) * k_ShuffleIdFactor;
	const float sum = scaled + static_cast<float>(static_cast<int32_t>(turn));
	const auto value = static_cast<uint32_t>(map_coords::FtoL(sum));
	// 0x7475DA..0x7475E4: unsigned div by info +0x168
	return value % every == 0;
}

int ShuffleCompare(const ShuffleEntry& a, const ShuffleEntry& b)
{
	// 0x7417C0: |b|^2 < |a|^2 (fcompp; test ah, 1) -> -1, else 1
	return Magnitude(b.male, b.villager) < Magnitude(a.male, a.villager) ? -1 : 1;
}

void SortShuffle(std::vector<ShuffleEntry>& entries)
{
	// _qsort 0x7C7E64 (the VC6 CRT; the same steps as town_desire::MsvcQsort: CUTOFF 8, the middle as the pivot)
	if (entries.size() < 2)
	{
		return;
	}
	constexpr int k_Cutoff = 8;
	std::array<int, 30> loStack {};
	std::array<int, 30> hiStack {};
	int stack = 0;
	int lo = 0;
	int hi = static_cast<int>(entries.size()) - 1;
	const auto at = [&entries](int i) -> ShuffleEntry& { return entries.at(static_cast<size_t>(i)); };
	for (;;)
	{
		const int size = hi - lo + 1;
		if (size <= k_Cutoff)
		{
			ShortSort(entries, lo, hi);
		}
		else
		{
			const int mid = lo + size / 2;
			std::swap(at(mid), at(lo));
			int loGuy = lo;
			int hiGuy = hi + 1;
			for (;;)
			{
				do
				{
					++loGuy;
				} while (loGuy <= hi && ShuffleCompare(at(loGuy), at(lo)) <= 0);
				do
				{
					--hiGuy;
				} while (hiGuy > lo && ShuffleCompare(at(hiGuy), at(lo)) >= 0);
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(at(loGuy), at(hiGuy));
			}
			std::swap(at(lo), at(hiGuy));
			if (hiGuy - lo > hi - loGuy)
			{
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stack)) = lo;
					hiStack.at(static_cast<size_t>(stack)) = hiGuy - 1;
					++stack;
				}
				if (loGuy < hi)
				{
					lo = loGuy;
					continue;
				}
			}
			else
			{
				if (loGuy < hi)
				{
					loStack.at(static_cast<size_t>(stack)) = loGuy;
					hiStack.at(static_cast<size_t>(stack)) = hi;
					++stack;
				}
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		--stack;
		if (stack < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stack));
		hi = hiStack.at(static_cast<size_t>(stack));
	}
}

bool PlanShuffle(const std::vector<ShuffleEntry>& sorted, size_t i, float (*percentAdults)(entt::entity),
                 ShufflePlan& plan)
{
	if (i + 1 >= sorted.size())
	{
		return false;
	}
	const auto& a = sorted.at(i);
	// 0x74165B..0x741670: best = |a.m^2 + a.v^2|
	float best = Magnitude(a.male, a.villager);
	// 0x741685..0x7416B9: the c after a: |(c.m + a.m)^2 + (c.v + a.v)^2| strictly below the best (fcom; test ah, 1)
	std::optional<size_t> found;
	for (size_t j = i + 1; j < sorted.size(); ++j)
	{
		const auto& c = sorted.at(j);
		const float m = c.male + a.male;
		const float v = c.villager + a.villager;
		const float d = Magnitude(m, v);
		if (d < best)
		{
			best = d;
			found = j;
		}
	}
	if (!found)
	{
		return false;
	}
	const auto& b = sorted.at(*found);
	// 0x7416C3..0x7416D9: masV = a.v > b.v (fcomp; test ah, 0x41)
	const bool moreVillager = a.villager > b.villager;
	// 0x7416DB..0x741723: a.v x b.v < 0 (opposite signs): the winner (masV ? a : b) not below 100% full of adults (vt
	// +0x89C < 1, test ah, 1) keeps the swap; below it, no swap
	bool swap = true;
	if (a.villager * b.villager < 0.0f)
	{
		const auto winner = moreVillager ? a.abode : b.abode;
		if (percentAdults != nullptr && percentAdults(winner) < 1.0f)
		{
			swap = false;
		}
	}
	// 0x741725..0x74173A: masM = a.m > b.m (test ah, 0x41)
	const bool moreMale = a.male > b.male;
	plan.a = i;
	plan.b = *found;
	plan.swap = swap;
	if (swap)
	{
		// 0x74173E..0x74175A: masM ? a.SwapMaleForFemaleFrom(b) : b.SwapMaleForFemaleFrom(a)
		plan.firstIsA = moreMale;
		plan.male = false;
	}
	else
	{
		// 0x74175C..0x741777: masV ? a.TakeVillagerFrom(b, masM) : b.TakeVillagerFrom(a, !masM)
		plan.firstIsA = moreVillager;
		plan.male = moreVillager ? moreMale : !moreMale;
	}
	return true;
}

void ShuffleVillagersAroundAbodes(entt::entity town)
{
	// 0x741549..0x741629: the structures +0x754 (newest first) that are functional with MaxVillagers + MaxChildren != 0
	std::vector<ShuffleEntry> entries;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (!abode_queries::IsFunctional(abode) ||
		    abode_villagers::MaxVillagers(abode) + abode_villagers::MaxChildren(abode) == 0)
		{
			continue;
		}
		// 0x741604..0x74161E: {abode, CalculateDesireToGainMale, 0.5 [0x8AA3B4] x CalculateDesireToGainVillager}
		const float male = abode_villagers::CalculateDesireToGainMale(abode);
		const float villager = abode_villagers::CalculateDesireToGainVillager(abode) * 0.5f;
		entries.push_back({abode, male, villager});
	}
	// 0x74158B: n <= 0 -> nothing
	if (entries.empty())
	{
		return;
	}
	// 0x741638 _qsort(list, n, 12, 0x7417C0)
	SortShuffle(entries);
	// 0x74163D..0x741798: i = 0 .. n - 2; one move a call (the first that returns 1 ends it)
	for (size_t i = 0; i + 1 < entries.size(); ++i)
	{
		ShufflePlan plan;
		if (!PlanShuffle(entries, i, &abode_villagers::GetPercentAbodeFullWithAdults, plan))
		{
			continue;
		}
		const auto a = entries.at(plan.a).abode;
		const auto b = entries.at(plan.b).abode;
		const auto first = plan.firstIsA ? a : b;
		const auto second = plan.firstIsA ? b : a;
		const uint32_t moved = plan.swap ? abode_villagers::SwapMaleForFemaleFrom(first, second)
		                                 : abode_villagers::TakeVillagerFrom(first, second, plan.male);
		if (std::getenv("OPENBLACK_TOWN_TRACE") != nullptr)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Town trace: shuffle: {} -> {} ({}{}) = {}", static_cast<uint32_t>(second),
				                   static_cast<uint32_t>(first), plan.swap ? "swap" : "take",
				                   plan.swap ? "" : (plan.male ? " male" : " female"), moved);
			}
		}
		if (moved != 0)
		{
			break; // 0x74177C jne 0x74179E
		}
	}
}

// ---- towns near a point ------------------------------------------------------------------------------------------

entt::entity GetNearestTown(glm::ivec2 pos, float radius)
{
	return map_cells::GetNearestTown(map_coords::MapCoords {pos.x, pos.y, 0.0f}, radius);
}
} // namespace openblack::ecs::town_villagers
