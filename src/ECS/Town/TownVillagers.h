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
#include <glm/vec2.hpp>

// The town's villagers (Town.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V4_spec.md §2.3, §6, §8.2, disassembly
// dev\tmp_dis\aldeanos\v4\town.txt, homeless.txt, misc2.txt): the homeless list +0x768 / +0x76C (Town::homelessVillagers,
// the head first), the global vagrants (g_game +0x205BFC / +0x205C00), AddVillagerToTown, FindAbodeWithSpaceInTown, the
// eaten food +0x6F8 and ShuffleVillagersAroundAbodes (Town::Process step 24). The abode's side is ecs::abode_villagers.

namespace openblack::ecs::components
{
struct Town;
}

namespace openblack::ecs::town_villagers
{
// ---- the homeless and the vagrants -------------------------------------------------------------------------------

/// +0x768 (next = villager +0xE4) / +0x76C: the town's homeless, the head first (MakeHomelessNoStateChange 0x7612F9
/// inserts at the head). Empty without a town
[[nodiscard]] const std::vector<entt::entity>& Homeless(entt::entity town);
/// Town::IsVillagerInHomelessList 0x73B580
[[nodiscard]] bool IsVillagerInHomelessList(entt::entity town, entt::entity villager);
/// At the head of +0x768, ++ +0x76C (MakeHomelessNoStateChange 0x7612F9..0x761312)
void AddToHomelessList(entt::entity town, entt::entity villager);
/// The unlink every writer inlines (AddVillagerToAbode 0x404085, CheckHomelessMoveIntoAbode 0x76137D, RemoveVillager
/// 0x73E259): out of the list if it is there, -- +0x76C. Returns whether it was there
bool RemoveFromHomelessList(entt::entity town, entt::entity villager);
/// g_game +0x205BFC / +0x205C00: the villagers without a town. Its writers are ChildBorn 0x762396, TownDeleted
/// 0x750B58 and ReleaseFromScript 0x75329C: TODO(V12/V14), none in V4, so (inferido) it is always empty in a new game
[[nodiscard]] const std::vector<entt::entity>& Vagrants();
/// Villager::IsVagrant 0x7531A0
[[nodiscard]] bool IsVagrant(entt::entity villager);
/// The unlink of AddVillagerToAbode 0x4040FA / MakeHomelessNoStateChange 0x761290 / DeleteDependancys 0x74FE4B
bool RemoveFromVagrants(entt::entity villager);
/// A new map / the tests: no vagrants
void ClearVagrants();
/// The deletion of a villager (Villager::DeleteDependancys 0x74FD60's list part): out of its town's homeless list and
/// out of the vagrants
void ForgetVillager(entt::entity villager);

// ---- joining, moving, eating -------------------------------------------------------------------------------------

/// Town::AddVillagerToTown 0x73A090: uninhabitable (+0x5F4) -> 0; TownStats::Add; SetTown; an abode of this town ->
/// done; another one -> RemoveAliveVillagerFromAbode, SetAbode(0); FindAbodeWithSpaceInTown(v, 0) ->
/// AddVillagerToAbode, 1; none -> MakeHomelessNoStateChange; then adults + children == 1 -> CheckAddWorshipSite
/// 0x740BF0 (milagros2's worship::town::CheckAddWorshipSite). 1
bool AddVillagerToTown(entt::entity town, entt::entity villager);
/// Town::FindAbodeWithSpaceInTown 0x73B370: the structures +0x754 (newest first), IsFunctional, the strictly best
/// CalculateScoreForAddingVillagerToAbode above `minimum`; entt::null when none
[[nodiscard]] entt::entity FindAbodeWithSpaceInTown(entt::entity town, entt::entity villager, float minimum);
/// Town::ChildToAdult 0x73AF50 -> TownStats::ChildToAdult 0x749490: with an abode +0x4C - 1, +0x50 + 1; children - 1,
/// adults + 1 (the places are recomputed each Town::Process, V3; the adults / children are kept at once)
void ChildToAdult(entt::entity town, entt::entity villager);
/// Town::UseFood 0x73B5E0: +0x6F8 (Town::foodUsed) += n (fild qword, fadd); the player's statistics (+0xA44 -> +0xA4)
/// TODO(estadísticas)
void UseFood(entt::entity town, uint32_t amount);
/// Town::RemoveVillager 0x73E210, the part V4 has: TownStats::Remove (the counts), an abode -> RemoveAliveVillager and
/// SetAbode(0), else out of the homeless list; SetTown(0); mother = 0. TODO(V12/V14/milagros2): FindChildrenAndOrphanThem
/// 0x756BE0, RemoveVillagerOnWayToWorshipSite 0x73E360, RemoveVillagerFromWorshipSite 0x76C440 and the empty town's
/// countdown +0xF20 = 50 (0x73E2CD)
void RemoveVillager(entt::entity town, entt::entity villager);

// ---- the shuffle (Town::Process step 24) ------------------------------------------------------------------------

/// 0x7475A3..0x7475E4: ftol((u64) town id (+0x5B4) x 20 [0x8C7658] + turn (fiadd)) % GTownInfo +0x168
/// shuffleVillagersEvery (unsigned div) == 0
[[nodiscard]] bool ShuffleDue(const components::Town& town, uint32_t turn);
/// Town::ShuffleVillagersAroundAbodes 0x741540
void ShuffleVillagersAroundAbodes(entt::entity town);

/// One entry of the shuffle's list (12 bytes: the abode, CalculateDesireToGainMale, 0.5 x CalculateDesireToGainVillager)
struct ShuffleEntry
{
	entt::entity abode {entt::null};
	float male {0.0f};
	float villager {0.0f};
};
/// The comparator 0x7417C0: |b.m^2 + b.v^2| < |a.m^2 + a.v^2| -> -1, else 1 (never 0: the larger first)
[[nodiscard]] int ShuffleCompare(const ShuffleEntry& a, const ShuffleEntry& b);
/// _qsort 0x7C7E64 (VC6, as town_desire::MsvcQsort) of the list with ShuffleCompare (0x741638)
void SortShuffle(std::vector<ShuffleEntry>& entries);
/// The pair loop 0x74163D..0x74179E on a sorted list, without the moves: which pair it tries first and how. For the
/// tests (`swap` = SwapMaleForFemaleFrom, else TakeVillagerFrom; `first` the one whose function runs; `male` its arg)
struct ShufflePlan
{
	size_t a {0};
	size_t b {0};
	bool swap {false};
	bool firstIsA {true};
	bool male {false};
};
/// The plan for entry i of a sorted list: the j > i whose |(a + c)^2| is strictly the smallest below |a^2|; none ->
/// false. `percentAdults(abode)` is GetPercentAbodeFullWithAdults (the signs-opposite rule)
[[nodiscard]] bool PlanShuffle(const std::vector<ShuffleEntry>& sorted, size_t i, float (*percentAdults)(entt::entity),
                               ShufflePlan& plan);

// ---- towns near a point ------------------------------------------------------------------------------------------

/// MapCoords::GetNearestTown(r) 0x6020E0 (map_cells::GetNearestTown): every player's towns, GetDistanceInMetres < best,
/// best = r (strict)
[[nodiscard]] entt::entity GetNearestTown(glm::ivec2 pos, float radius);
} // namespace openblack::ecs::town_villagers
