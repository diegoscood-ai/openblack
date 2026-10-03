/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownProcess.h"

#include <cstdlib>

#include <optional>
#include <string>

#include <spdlog/spdlog.h>

#include "ECS/Components/Town.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"

// Town.cpp of runblack.exe W120 (TownProcess.h)

namespace openblack::ecs::town_process
{
using namespace components;

namespace
{
constexpr uint32_t k_WorshipEvery = 10; // 0x7473F1 (div 0xA)

/// OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<town id>]": at turn 2, SetBoost(d, boost) with the re-sort of
/// SET_TOWN_DESIRE_BOOST (order 1), on every town or on the one with that id. Not part of the original
void RunTestBoost(uint32_t turn)
{
	if (turn != 2)
	{
		return;
	}
	const char* value = std::getenv("OPENBLACK_TEST_TOWN_DESIRE");
	if (value == nullptr || *value == '\0')
	{
		return;
	}
	const std::string text(value);
	const auto first = text.find(',');
	if (first == std::string::npos)
	{
		return;
	}
	const int desire = std::atoi(text.substr(0, first).c_str());
	const auto second = text.find(',', first + 1);
	const float boost = std::strtof(text.substr(first + 1, second - first - 1).c_str(), nullptr);
	std::optional<uint32_t> only;
	if (second != std::string::npos)
	{
		only = static_cast<uint32_t>(std::strtoul(text.substr(second + 1).c_str(), nullptr, 10));
	}
	if (desire < 0 || desire >= static_cast<int>(town_desire::k_Count))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Town>([&](entt::entity entity, const Town& town) {
		if (!only.has_value() || town.id == *only)
		{
			town_desire::SetBoost(entity, static_cast<TownDesireInfo>(desire), boost, true);
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "OPENBLACK_TEST_TOWN_DESIRE: town {} desire {} boost {:.3f}", town.id,
				                   desire, boost);
			}
		}
	});
}
} // namespace

void ProcessTown(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	const uint32_t turn = villager::CurrentTurn();

	// 0 (openblack) the TownStats of the turn (+0x610) from the entities (the original keeps them incrementally)
	t->stats = town_stats::Compute(town);
	// 1 0x747390: +0x5E4 = 0
	t->requestedPlanThisTurn = false;
	// 2 0x747396 fn_43BD00(&+0x790): the building sites' pruning. TODO(V6)
	// 3 0x7473A0 +0x5C8 = GetBaseInfluence 0x73FD40; 4 0x7473AD fn_747600: every processAbodeEvery (+0x4C) turns,
	//   Abode::Process (vt +0x5FC) of each abode +0x754 (TODO(V4, R6)) and, unless +0x5F8, +0x5C8 += GetInfluence
	//   (vt +0x868); 5 0x7473BD with a player +0x5C8 x= g_game +0x250078. The influence part is milagros2's
	//   influence::ProcessTowns (InfluenceSources.cpp), which their influence::ProcessTurn runs every turn: not called
	//   here (see ProcessPlayers)
	// 6 0x7473D7: TownDesire::Process 0x745AE0
	town_desire::Process(town);
	// 7 0x7473DE fn_747780: TownArtifact::Process 0x425FB0 of the artifacts +0x994 (next +0x20). TODO(artefactos)
	// 8 0x7473E3..0x7473FE: turn % 10 == 0 -> fn_7489F0: n = GetWorshipersNeeded(1, 0, null) 0x73C860; n > 0 ->
	//   AdjustWorshipersWorshipping(n, 1, 0) 0x73C0F0 (milagros2, Worship/WorshipPercentage)
	if (turn % k_WorshipEvery == 0)
	{
		const int needed = worship::percentage::GetWorshipersNeeded(town, true, false, nullptr);
		if (needed > 0) // 0x7489FE jle
		{
			worship::percentage::AdjustWorshipersWorshipping(town, needed, true, false);
		}
	}
	// 9 0x747405 fn_747660: the list +0x770 (CallState == 5 -> take off and delete; not available -> take off).
	//   (inferido) only the ctor, fn_73C710 and Town::Load write it: empty in a new game, not ported
	// 10 0x74740A..0x74741E: the football +0xEA4 when FootballEnabled (vt +0x5FC): out of the plan
	// 11 0x747428 fn_747750: the town's spell icons +0x778 (next +0x110) vt +0x5FC. TODO(milagros2): confirm whether
	//    their icons' step covers it
	// 12 0x74742F fn_73D850: each desire flag +0x9B0[i] available: Process and +0x58 = R(+0x5C); else null.
	//    TODO(banderas): openblack does not make the 7 TownDesireFlags (CREATE_TOWN only skips their index)
	// 13 0x747436 Town::ProcessPlayerInteract 0x73DEC0 (Protection +0xEC0 / Mercy +0xEBC from the aggression slots).
	//    TODO(agresiones): they stay 0
	// 14 0x74743D Town::ProcessTownRepairs 0x747DE0. TODO(V11)
	// 15 0x747444 Town::ProcessTownEmergency 0x7477A0 (+0xF1C). TODO(Milagros, M-7): the fire
	// 16 0x74744B Town::UpdateAttitudeToCreature 0x7437F0. TODO(criatura)
	// 17 0x747450..0x7474A0: the temporary pots +0x600 / +0x604: available, empty (+0x70 == 0) and a functional
	//    storage pit -> ToBeDeleted, null; not available -> null. TODO(V5): openblack has no temporary pots
	// 18 0x7474A2..0x7474FD: the missionaries +0x99C (MissionaryControl::Process 0x7567E0, the unavailable ones taken
	//    off, --+0x9A0). TODO(milagros2)
	// 19 0x7474FF..0x747506 fn_4383D0(+0x798, town): the belief (GBelief). TODO(milagros2): no function yet
	// 20 0x74750B..0x747523: with a player, fn_4141F0(player +0x60, town): the alignment by desires (TownDesire
	//    fn_7466D0 with GetDesire, info +0x4C / +0x50 / +0x54 and AlignmentTurns +0x410; fn_414660; CAlignmentHistory::
	//    Add 0x414D40). TODO(milagros2): no function yet
	// 21 0x747528..0x747544: the pulse: +0x5EC != 0 -> +0x5E8 = 0; +0x5EC = +0x5E8 (the writers come in V5/V6)
	if (t->buildPulsePrevious != 0)
	{
		t->buildPulse = 0;
	}
	t->buildPulsePrevious = t->buildPulse;
	// 22 0x747536..0x747574: +0xF20 != 0 -> --; at 0 SetTownEmpty 0x741080 (TODO(V12)); else with people -> 0. Who
	//    sets 50 (Town::RemoveVillager 0x73E2CD) is TODO(V12) too
	if (t->emptyCountdown != 0)
	{
		--t->emptyCountdown;
		if (t->emptyCountdown == 0)
		{
			// TODO(V12): Town::SetTownEmpty 0x741080
		}
		else if (t->stats.adults + t->stats.children != 0)
		{
			t->emptyCountdown = 0;
		}
	}
	// 23 0x747574..0x74759E: not neutral: fn_555240(+0x5C8, +0xF24): |a - b| > 0.01 -> g_game +0x250174 = 1 (the
	//    drawn influence). (inferido) milagros2's influence drawing recomputes it: nothing to call
	// 24 0x7475A3..0x7475E8: ftol(20 x +0x5B4 + turn) % info +0x168 shuffleVillagersEvery == 0 ->
	//    ShuffleVillagersAroundAbodes 0x741540. TODO(V4, R6)
}

void ProcessPlayers()
{
	const uint32_t turn = villager::CurrentTurn();
	RunTestBoost(turn);
	// Town::Process steps 3-5 (+0x5C8: GetBaseInfluence 0x7473A0, fn_747600 0x7473AD, x g_game +0x250078 0x7473BD) are
	// not called here: openblack runs them for every town in milagros2's own turn hook (influence::ProcessTurn ->
	// influence::ProcessTowns, InfluenceSources.cpp, inside magic::ProcessTurn). (aproximado) so the influence of the
	// turn is computed after the desires instead of just before each town's; the desires do not read it
	// GPlayer::ProcessPlayers 0x649A20 -> GPlayer::Process 0x6494E0: Citadel::Process (+0xA48, not ours), then each
	// town of +0xA50 (next +0x75C): Town::Process 0x649551. The player's alignment (0x6496C5) comes after its towns
	// (For_Children reads the last turn's): ecs::effects::alignment::ProcessPlayers in magic::ProcessTurn
	map_cells::ForEachTown([](entt::entity town) {
		ProcessTown(town);
		return true;
	});
}
} // namespace openblack::ecs::town_process
