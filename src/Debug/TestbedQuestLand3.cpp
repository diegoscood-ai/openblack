/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 3's gold scrolls as quest runs, each played after a land entry's quest= has started it: The Wolves Are
// Possessed, Fire! Fire! I'm on Fire! and Leave Through the Vortex. The timelines do what the player would: they click
// the texts that wait for a click, win a village by belief where the quest waits for the player to have won it, kill
// the wolves, and press on the vortex's scroll. The points are map coordinates, taken from the places the challenge
// scripts name.

#include <cstddef>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// The Indian village the wolves go for, where the quest finds its town; picked within this of its spot
constexpr glm::vec2 k_IndianVillage {2831.99f, 2547.2f};
constexpr float k_VillagePickRadius = 20.0f;
/// The belief in the player the village takes to be won: more than any other player's there, so its next fold makes
/// it the player's and keeps it so
constexpr float k_WinningBelief = 2.0f;
/// The pack's way from where it gathers to the village's edge, and how far from its middle a wolf is picked: the whole
/// way, about 205 each side, but not the land's own wolves, the nearest of which is about 296 from it
constexpr glm::vec2 k_PackWayMiddle {2620.0f, 2320.0f};
constexpr float k_PackPickRadius = 250.0f;
/// The wolf's script sub-type, and how many the pack has once the monk has thinned it
constexpr uint32_t k_WolfSubtype = 2;
constexpr size_t k_PackAfterMonk = 8;
/// The monk's last line before the wolves' cut scene ends
constexpr uint32_t k_MonkLastLine = 5061;
/// Seconds from that line read to the first wolf taken: the cut scene closes about three seconds after it
constexpr float k_FirstKillSeconds = 6.0f;
/// The vortex the creed scene opens, and the scroll over it, raised 20 above it
constexpr glm::vec2 k_Vortex {2274.481f, 2043.0614f};
constexpr float k_ScrollRaised = 20.0f;
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

constexpr std::string_view k_WolfGlobals = "PrisonStatuesDeactivated MonkHasReachedEnd";
constexpr std::string_view k_WolfScripts = "WolfAttack WaitUntilTownTakenOver GetRidOfWolf";
constexpr std::string_view k_FireGlobals = "PrisonStatuesDeactivated MonkHasReachedEnd";
constexpr std::string_view k_FireScripts = "FanaticAttack FanaticOnFire";
constexpr std::string_view k_VortexGlobals = "VortexOpen LandNumber";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL3 ChallengeHighlightNotify";

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

/// The wolves' quest up to the end of its cut scene: the village won, the pack formed and thinned by the monk
[[nodiscard]] std::vector<Command> WolvesUpToTheirWalk()
{
	return {
	    ClickTexts(),
	    WaitTask("WolfAttack"),
	    LogEvery(k_WolfGlobals, k_WolfScripts),
	    // the quest waits 5 minutes, then for the Indian village to be the player's
	    WaitTask("WaitUntilTownTakenOver"),
	    // [shortcut] the village won by belief: its next fold hands it to the player
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_IndianVillage, .radius = k_VillagePickRadius}},
	    // the pack gathers and sets off; the cut scene follows it, and the monk thins it to eight
	    WaitText(k_MonkLastLine),
	};
}

void AddWolves(std::vector<Scenario>& all)
{
	auto commands = WolvesUpToTheirWalk();
	// [shortcut] each wolf of the pack taken out of the game by a script's delete once the cut scene is over, before any
	// reaches the village, as the player's blows would leave the pack: a wolf killed leaves its pack once it lies dead.
	// A script's health of 0 is not used, as it leaves the wolf walking on in its pack. A wolf already dying is passed
	// over, so each pick takes a living one
	for (size_t i = 0; i < k_PackAfterMonk; ++i)
	{
		commands.push_back({.kind = Kind::ScriptDelete,
		                    .delaySeconds = i == 0 ? k_FirstKillSeconds : 0.5f,
		                    .thing = ThingNear {.type = script::ObjectType::Animal,
		                                        .subtype = k_WolfSubtype,
		                                        .point = k_PackWayMiddle,
		                                        .radius = k_PackPickRadius,
		                                        .aliveOnly = true}});
	}
	commands.push_back(WaitTask("WolfAttack", TaskWait::Finished));
	commands.push_back(LogGlobals(k_WolfGlobals, k_WolfScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_wolves_are_possessed",
	    .name = "Land 3 quest run: The Wolves Are Possessed",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/MonkHasReachedEnd=1/WolfAttack on Land3.txt). "
	                   "The texts that wait for a click are clicked on. Once the quest waits for the Indian village it "
	                   "gives the village the belief that wins it for the player; after the cut scene's last line it "
	                   "takes the pack's eight wolves out one by one with a script's delete, and waits for the quest to "
	                   "end, logging its state every 30 seconds.",
	    .expected = "Five minutes in, the quest waits for the village; once it is the player's, the pack of twenty forms "
	                "about 510 from it and sets off at half speed, the cut scene follows it with the monk thinning it to "
	                "eight, and the story-log entry starts at nothing done. With every wolf gone before any is within 25 "
	                "of the village's edge, the entry is updated to complete and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
	// The same quest with no wolf killed: the pack reaches the village, which loses its faith in the player
	auto lost = WolvesUpToTheirWalk();
	lost.push_back(WaitTask("WolfAttack", TaskWait::Finished));
	lost.push_back(LogGlobals(k_WolfGlobals, k_WolfScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_wolves_are_possessed_lost",
	    .name = "Land 3 quest run: The Wolves Are Possessed, the village lost",
	    .facet = Facet::Story,
	    .description = "As the wolves' quest run, with the same start, but no wolf is killed: it waits for the quest to "
	                   "end, logging its state every 30 seconds.",
	    .expected = "The pack of eight walks on; once a wolf is within 25 of the village's edge the pack is let loose, a "
	                "cut scene flies the camera to the village, the entry is marked complete, the good advisor says the "
	                "wolves have reached the village and that the villagers are switching their faith; the village "
	                "believes in Lethys its most belief plus one, and in the player 0.3 of what it believed in its owner, "
	                "so that its next fold hands it to Lethys; the quest ends.",
	    .commands = std::move(lost),
	    .onLand = true,
	});
}

void AddFire(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FanaticAttack"),
	    LogEvery(k_FireGlobals, k_FireScripts),
	    WaitTask("FanaticAttack", TaskWait::Finished),
	    LogGlobals(k_FireGlobals, k_FireScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.fire_fire_im_on_fire",
	    .name = "Land 3 quest run: Fire! Fire! I'm on Fire!",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/MonkHasReachedEnd=1/FanaticAttack(0) on "
	                   "Land3.txt). The texts that wait for a click are clicked on; nothing puts the fishermen out. It "
	                   "waits for the quest to end, logging its state every 30 seconds.",
	    .expected = "A minute in, the cut scene: Lethys's lines, his three fireballs setting the sixteen fishermen "
	                "alight, the advisors, and the monk's water miracles; then the fishermen run for the village. With "
	                "none put out, each burns to death on the path, and after the last the music stops, the weather "
	                "comes back and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddVortex(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised),
	};
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitTask("LeaveThroughVortexL3", TaskWait::Finished));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_3",
	    .name = "Land 3 quest run: Leave through the vortex",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/LeaveThroughVortexL3(marker@2274.481;29.8098;"
	                   "2043.0614) on Land3.txt). The texts that wait for a click are clicked on. Once the scroll's "
	                   "notifier runs, it puts the camera on the scroll over the vortex, presses on it with the hand and "
	                   "waits for the quest to end.",
	    .expected = "The scroll appears 20 above the vortex; the press switches it on and the camera flies high above "
	                "the vortex, looking down into it, and dives in; the quest ends with the land's exit.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand3Scenarios(std::vector<Scenario>& all)
{
	AddWolves(all);
	AddFire(all);
	AddVortex(all);
}
