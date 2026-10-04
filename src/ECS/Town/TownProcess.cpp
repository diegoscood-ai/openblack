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

#include "ECS/Abodes.h"
#include "ECS/Components/Town.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "InfoConstants.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"
#include "Worship/Citadel.h"
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

	// 1 0x747390: +0x5E4 = 0
	t->requestedPlanThisTurn = false;
	// 2 0x747396 fn_43BD00(&+0x790): the building sites' pruning
	building_sites::PruneSites(town);
	t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	// 0 (openblack) the TownStats of the turn (+0x610) from the entities (the original keeps them incrementally), after
	// the pruning (nothing between 0x747390 and TownDesire::Process reads them; the pruned sites are out of them)
	t->stats = town_stats::Compute(town);
	// 3 0x7473A0 +0x5C8 = GetBaseInfluence 0x73FD40; 4 0x7473AD fn_747600: turn % GTownInfo +0x4C processAbodeEvery == 0
	//   (0x747615 unsigned div) -> for each structure +0x754 (newest first) its Process (vt +0x5FC) and, unless +0x5F8,
	//   +0x5C8 += GetInfluence (vt +0x868); 5 0x7473BD with a player +0x5C8 x= g_game +0x250078. (aproximado) the
	//   influence part is milagros2's influence::ProcessTowns (InfluenceSources.cpp), which their influence::ProcessTurn
	//   runs every turn (see ProcessPlayers): here only the Process part, in the same order. Abode::Process 0x404440 for
	//   the classes that do not override it (the Field 0x529020, TownCentre 0x743DF0, Workshop 0x7797F0 and
	//   SpellDispenser 0x722A70 overrides are their owners')
	if (const auto every = Locator::infoConstants::value().town.processAbodeEvery; every != 0 && turn % every == 0)
	{
		for (const auto abode : town_stats::AbodesOf(town))
		{
			// Abode::Process 0x404440 (and TownCentre::Process 0x743DF4, which calls it first) starts with
			// MultiMapFixed::Process 0x52F700: +0x74 -> its Process (vt +0x100, StandardBuildingSite 0x43D8D0).
			// (openblack) here for every abode, before abode_villagers::ProcessAbode's part. (pending) Field 0x529020 /
			// Workshop 0x7797F0 / SpellDispenser 0x722A70 not read
			if (const auto site = abodes::GetBuildingSite(abode); site != entt::null)
			{
				building_sites::Process(site);
			}
			if (abode_villagers::RunsAbodeProcess(abode))
			{
				abode_villagers::ProcessAbode(abode);
			}
		}
	}
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
	// 14 0x74743D Town::ProcessTownRepairs 0x747DE0: at most one new site (a repair site or a rebuild plan's)
	building_sites::ProcessTownRepairs(town);
	// 15 0x747444 Town::ProcessTownEmergency 0x7477A0 (+0xF1C, +0xEC4; the fire through ecs::fire::IsOnFire)
	town_emergency::ProcessTownEmergency(town);
	t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	// 16 0x74744B Town::UpdateAttitudeToCreature 0x7437F0. TODO(criatura)
	// 17 0x747450..0x7474A0: the temporary pots +0x600 / +0x604: available, empty (+0x70 == 0) and a functional
	//    storage pit -> ToBeDeleted, null; not available -> null. TODO(V5): openblack has no temporary pots
	// 18 0x7474A2..0x7474FD: the missionaries +0x99C (MissionaryControl::Process 0x7567E0, the unavailable ones taken
	//    off, --+0x9A0). TODO(milagros2)
	// 19 0x7474FF..0x747506 fn_4383D0(+0x798, town), unconditional: the belief (GBelief: +0xC8 x Town +0x5DC folded
	//    into +0x8 and +0x88, the boredom, the desires' cost, the neutral pin, the conversion; ecs::town_belief). The
	//    conversion ends the old owner's walk (ProcessPlayers)
	town_belief::Fold(town);
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
			// TODO(V12): Town::SetTownEmpty 0x741080 (its belief part is town_belief::SetTownEmpty)
		}
		else if (t->stats.adults + t->stats.children != 0)
		{
			t->emptyCountdown = 0;
		}
	}
	// 23 0x747574..0x74759E: not neutral: fn_555240(+0x5C8, +0xF24): |a - b| > 0.01 -> g_game +0x250174 = 1 (the
	//    drawn influence). (inferido) milagros2's influence drawing recomputes it: nothing to call
	// 24 0x7475A3..0x7475E8: ftol(20 x +0x5B4 (the town id) + turn) % info +0x168 shuffleVillagersEvery == 0 ->
	//    ShuffleVillagersAroundAbodes 0x741540 (one move a call)
	if (auto* again = registry.TryGet<Town>(town); again != nullptr && town_villagers::ShuffleDue(*again, turn))
	{
		town_villagers::ShuffleVillagersAroundAbodes(town);
	}
}

void ProcessPlayers()
{
	const uint32_t turn = villager::CurrentTurn();
	RunTestBoost(turn);
	// (openblack) the GameThing deletion pass of the building sites deleted last turn (BuildingSite::ToBeDeleted
	// 0x43B960 marks them; IsAvailable answered false meanwhile)
	building_sites::FlushDeleted();
	// Town::Process steps 3-5 (+0x5C8: GetBaseInfluence 0x7473A0, fn_747600 0x7473AD, x g_game +0x250078 0x7473BD) are
	// not called here: openblack runs them for every town in milagros2's own turn hook (influence::ProcessTurn ->
	// influence::ProcessTowns, InfluenceSources.cpp, inside magic::ProcessTurn). (aproximado) so the influence of the
	// turn is computed after the desires instead of just before each town's; the desires do not read it
	// GPlayer::ProcessPlayers 0x649A20 -> GPlayer::Process 0x6494E0: Citadel::Process (+0xA48, 0x649525), then each
	// town of +0xA50 (next +0x75C): Town::Process 0x649551. The player's alignment (0x6496C5) comes after its towns
	// (For_Children reads the last turn's): ecs::effects::alignment::ProcessPlayers in magic::ProcessTurn
	// GetNextPlayerAndNeutral 0x550980: the slots 0..7. A town taken over in its fold (step 19, TakeOverTown
	// fn_00649810 -> fn_0064C090 puts it at the tail of the new owner's list with next = 0, 0x64C0C6) ends the old
	// owner's walk (0x649545 mov edi, [edi + 0x75C]): its later towns wait for the next turn; the taken town comes
	// again under the new owner when that slot is later (always for the neutral one). (approximate) TownsOf orders by
	// Town::id, so the taken town is not necessarily the new owner's last
	auto& registry = Locator::entitiesRegistry::value();
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		// GPlayer::Process 0x6494E0: the player's Citadel::Process 0x462D70 (+0xA48, 0x649525) before its towns
		worship::citadel::Process(worship::citadel::Of(player));
		for (const auto town : map_cells::TownsOf(player))
		{
			ProcessTown(town);
			if (const auto* t = registry.TryGet<Town>(town); t != nullptr && t->owner != player)
			{
				break;
			}
		}
	}
}
} // namespace openblack::ecs::town_process
