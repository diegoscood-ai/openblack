/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's gold scroll "Nemesis. No!" (Khazar's death) as a quest run, played after a land entry's quest= has started
// the arrival on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). The quest is a
// film that takes no input from the player: the land's control script starts it once Khazar is in none of his own
// scripts, and its texts wait for the player's click. The timeline does what the control script and the player would.

#include <cstddef>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::TaskWait;

/// The film's script
constexpr std::string_view k_FilmScript = "KillKhazarMain";
/// The globals and scripts the film's progress and end are read by: Khazar is gone once it starts, and his own
/// behaviour script is stopped by it
constexpr std::string_view k_FilmGlobals = "KhazarGone KhazarInScript KillKhazarActorsLeft VortexOpen";
constexpr std::string_view k_FilmScripts = "KillKhazarMain KillKhazarImmortality KhazarAI";
/// How often the film's state is logged while it plays, in seconds, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

Command WaitTask(std::string_view script, TaskWait wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}
} // namespace

void testbed_scenarios::AddQuestNemesisNoScenarios(std::vector<Scenario>& all)
{
	// Once the arrival is over (its end loads the rival gods' creatures and starts Khazar's behaviour script), the film
	// is started as the land's control script starts it: only while Khazar is in none of his own scripts
	std::vector<Command> film {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitTask("EnterLand2", TaskWait::Finished),
	    WaitGlobal("KhazarInScript", 0.0f),
	    {.kind = Kind::StartScript, .script = k_FilmScript},
	    WaitTask(k_FilmScript, TaskWait::Running),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_FilmGlobals, .script = k_FilmScripts},
	    WaitTask(k_FilmScript, TaskWait::Finished),
	    {.kind = Kind::LogGlobals, .delaySeconds = 2.0f, .global = k_FilmGlobals, .script = k_FilmScripts},
	};
	all.push_back({
	    .id = "quest.nemesis_no",
	    .name = "Land 2 quest run: Nemesis. No!",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over and Khazar is in none of "
	                   "his own scripts, it starts Khazar's death film, as the land's control script does, and waits for "
	                   "the film to end, logging its state every 30 seconds while it plays.",
	    .expected = "Khazar's behaviour script stopped, the vortex opened by his town, the fireballs and the burning "
	                "town, his temple blown up, his creature killed by the storm cloud and the Creed's glow taken by "
	                "Lethys's creature into the vortex, then the advisors' talk; the end check logs KhazarGone=1 and "
	                "KillKhazarMain not running. Unverified: the film's untimed waits (the storm cloud reaching each of "
	                "its three targets, the camera arriving) have not been run to their end.",
	    .commands = std::move(film),
	    .onLand = true,
	});
}
