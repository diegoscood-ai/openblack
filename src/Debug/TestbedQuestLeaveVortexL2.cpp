/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Leave through the vortex, Land 2's exit, as a quest run: started directly, the film of Lethys's temple falling and
// the vortex opening plays, then the gold scroll over the vortex is pressed on with the hand, which dives the camera in
// and ends the land's story. The points are map coordinates, taken from the place the challenge script names; not yet
// seen in a running game.
//
// The same exit as the story reaches it, as chapters of a story run on Land 1 (story_chain.sh, the start-of-game
// question answered to skip everything): the story's own top script carries the game from Land 1's skip branch to
// Land 2, and Land 2's control script starts Khazar's death, Lethys's theft of the creature and the exit itself as the
// land's state allows, then loads Land 3. Two things stand in for the player's play: Lethys's three towns are won by
// a gift of belief, and his temple's heart is brought down to a twentieth of its life, as the player's blows would
// once he has no towns to pass them on to.

#include <cstddef>
#include <cstdint>

#include <array>
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
/// The vortex where Lethys's temple stood, and its scroll, raised 20 above it
constexpr glm::vec2 k_Vortex {1061.107f, 3597.965f};
constexpr float k_ScrollRaised = 20.0f;
/// The challenge the scroll is made for
constexpr size_t k_Challenge = 59;
constexpr std::string_view k_VortexGlobals = "VortexOpen LeavingLandTwo";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL2 ChallengeHighlightNotify";

/// Land 1's vortex in the skip branch, and its scroll raised 20 above it; the camera put south of it, looking at it, as
/// the Land 1 quest run puts it
constexpr glm::vec2 k_FirstVortex {1700.2275f, 2520.1829f};
constexpr glm::vec3 k_FirstVortexEye {k_FirstVortex.x, 40.0f, k_FirstVortex.y - 60.0f};
constexpr glm::vec3 k_FirstVortexLook {k_FirstVortex.x, 25.0f, k_FirstVortex.y};
/// Lethys's three towns, where the land makes them
constexpr std::array k_LethysTowns {glm::vec2 {1800.44f, 3024.32f}, glm::vec2 {1258.37f, 3022.56f},
                                    glm::vec2 {1333.36f, 3379.90f}};
/// The belief in the player a town of Lethys's takes to be won: more than the most the land lets it hold in Lethys
/// (three quarters), so its next fold makes it the player's
constexpr float k_WinningBelief = 2.0f;
/// The towns on the way from the player's home to Lethys's, each won in turn by the hand from the influence of those
/// before it: the Indian village by the store (12), the Indian village west of it (10), the Indian village beyond (11),
/// the Celtic village by it (14), the neutral Celtic village by Lethys (8), then Lethys's three, nearest first (4, 5, 3)
constexpr std::array k_TownsOnTheWayToLethys {glm::vec2 {2862.58f, 3448.27f}, glm::vec2 {2761.49f, 3102.94f},
                                              glm::vec2 {2196.11f, 3096.86f}, glm::vec2 {2134.70f, 3010.71f},
                                              glm::vec2 {1929.16f, 3166.76f}, glm::vec2 {1800.44f, 3024.32f},
                                              glm::vec2 {1333.36f, 3379.90f}, glm::vec2 {1258.37f, 3022.56f}};
/// How many food miracles are poured towards each town, how long each, and the pause before the next: the by-hand
/// Impress Village run's short pours, every ten seconds or so
constexpr int k_PoursPerTown = 30;
constexpr float k_ByHandPourSeconds = 2.0f;
constexpr float k_ByHandPourGap = 6.0f;
/// The camera over the hand's pour: back from the point and above it
constexpr float k_PourEyeAbove = 40.0f;
constexpr float k_PourEyeBack = 50.0f;
/// Lethys's temple heart, found as the land's control script finds it, within 10 of its marker
constexpr ThingNear k_LethysHeart {.type = script::ObjectType::Citadel, .point = {1010.74f, 3676.572f}, .radius = 10.0f};
/// The life the heart is brought down to: below the tenth the land's control script waits for
constexpr float k_HeartBroughtDown = 0.05f;
/// The exit's scroll is picked within this of the vortex, closer than Lethys's own scroll by his vortex, 12 away
constexpr float k_ExitScrollPickRadius = 5.0f;
/// The good advisor's last line of the exit's film, which waits for the player's click; the scroll is made after it
constexpr uint32_t k_ExitFilmLastText = 1822;
/// How often the land's state is logged while a chapter waits on the control script
constexpr size_t k_WatchPeriod = 30;
constexpr std::string_view k_LandGlobals =
    "KhazarInScript KhazarGone LethysGone ChanceMissed VortexOpen DoneExitLethysVortex LeavingLandTwo";
constexpr std::string_view k_LandScripts = "LandControlAll LandControl2 KillKhazarMain LethysVortex LeaveThroughVortexL2";

/// A town of Lethys's takes more belief in the player than he can hold in it
[[nodiscard]] Command TownWon(glm::vec2 town, float delay)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = delay,
	        .amount = k_WinningBelief,
	        .thing = ThingNear {.type = script::ObjectType::Town, .point = town, .radius = 10.0f}};
}

/// The land's state, logged now and then every period until the chapter ends
[[nodiscard]] Command WatchLand()
{
	return {.kind = Kind::LogGlobals, .value = k_WatchPeriod, .global = k_LandGlobals, .script = k_LandScripts};
}

/// A gold scroll pressed on with the hand, picked within a radius of a point
void PressScrollWithin(std::vector<Command>& commands, glm::vec2 scroll, float radius)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = radius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// A food miracle given to the hand, as a bubble's would be, and poured for a moment at the edge of the player's
/// influence nearest a town, with the camera over the town: the hand held there, the Action button held, then let go
void PourAtTheEdgeTowards(std::vector<Command>& commands, glm::vec2 town)
{
	commands.push_back(
	    {.kind = Kind::HoldSeed, .delaySeconds = k_ByHandPourGap, .value = static_cast<size_t>(SpellSeedType::Food)});
	commands.push_back(CameraOn(town, k_PourEyeAbove, k_PourEyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = town, .value = 2});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_ByHandPourSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

void AddStoryExitScenarios(std::vector<Scenario>& all)
{
	// Land 1's skip branch opens the vortex and lays its scroll at once; pressing on it starts the dive, after which the
	// story's top script loads Land 2 and starts its control script
	std::vector<Command> firstLand {
	    ClickTexts(),
	    WaitGlobal("VortexOpen", 1.0f),
	    {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_FirstVortexEye, .look = k_FirstVortexLook},
	};
	PressScrollWithin(firstLand, k_FirstVortex, k_ScrollPickRadius);
	firstLand.push_back(WaitTask("LandControl2"));
	firstLand.push_back(LogGlobals("LeaveLandNow", k_LandScripts, 1.0f));
	all.push_back({
	    .id = "story.land1_skip_to_land2",
	    .name = "Story: from Land 1's skip branch to Land 2",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 from a new game whose start-of-game question skips everything. The texts that "
	                   "wait for a click are clicked on from now on. Once the vortex is open, it puts the camera south "
	                   "of it and presses on its scroll with the hand, then waits for Land 2's control script.",
	    .expected = "The press starts the dive and the fade; the story's top script stops the land's scripts, loads "
	                "Land 2 and starts its control script, which the end check logs as running.",
	    .commands = std::move(firstLand),
	    .onLand = true,
	});

	// Once the arrival is over, Lethys's three towns are won: each is a reason for Khazar's death on the control
	// script's next check, which starts it once Khazar is in none of his own scripts
	std::vector<Command> khazar {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    WatchLand(),
	};
	for (const auto town : k_LethysTowns)
	{
		khazar.push_back(TownWon(town, 1.0f));
	}
	khazar.push_back(WaitTask("KillKhazarMain"));
	khazar.push_back(WaitTask("KillKhazarMain", TaskWait::Finished));
	khazar.push_back(LogGlobals(k_LandGlobals, k_LandScripts, 2.0f));
	all.push_back({
	    .id = "story.land2_khazar_dies",
	    .name = "Story: Land 2's control script starts Khazar's death",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 after story.land1_skip_to_land2 in the same story run, with the land's control "
	                   "script running. The texts are clicked on. Once the arrival is over, Lethys's three towns take "
	                   "more belief in the player than he can hold in them (a shortcut for winning them); it then waits "
	                   "for the control script to start Khazar's death and for the film to end, logging the land's "
	                   "state every 30 seconds.",
	    .expected = "The towns become the player's. On its next 9-second check the control script counts the reasons "
	                "(Lethys's towns no longer his, and his town total down to none) and, once Khazar is in none of his "
	                "scripts, starts Khazar's death and waits for it; the land's last gold scroll starts after it. The "
	                "end check logs KhazarGone=1 and KillKhazarMain not running.",
	    .commands = std::move(khazar),
	    .onLand = true,
	});

	// The same, with the towns won by the hand: from the player's home village, one town after another on the way to
	// Lethys's, food is poured at the edge of the player's influence nearest the next town, as the by-hand Impress
	// Village run wins its village, until the towns between and then Lethys's three are the player's
	std::vector<Command> byHand {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    WatchLand(),
	};
	for (const auto town : k_TownsOnTheWayToLethys)
	{
		for (int i = 0; i < k_PoursPerTown; ++i)
		{
			PourAtTheEdgeTowards(byHand, town);
		}
	}
	byHand.push_back(WaitTask("KillKhazarMain"));
	byHand.push_back(WaitTask("KillKhazarMain", TaskWait::Finished));
	byHand.push_back(LogGlobals(k_LandGlobals, k_LandScripts, 2.0f));
	all.push_back({
	    .id = "story.land2_towns_won_by_hand",
	    .name = "Story: Land 2's towns won by the hand on the way to Lethys's, then Khazar's death",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 after story.land1_skip_to_land2 in the same story run, in place of "
	                   "story.land2_khazar_dies. The texts are clicked on. Once the arrival is over, for each town on the "
	                   "way from the player's home to Lethys's (the Indian villages by the store and to the west, the "
	                   "Indian and Celtic villages beyond, the neutral Celtic village by Lethys, then his three towns) "
	                   "the hand is given a food miracle and pours it at the edge of the player's influence nearest that "
	                   "town, again and again, as the by-hand Impress Village run wins its village; then it waits for the "
	                   "control script to start Khazar's death and for the film to end, logging the land's state every "
	                   "30 seconds.",
	    .expected = "Each town's belief in the player grows from the miracles its villagers see until its fold hands it to "
	                "the player, which moves the player's influence on towards the next. The first of Lethys's towns lost "
	                "(or a sixth town held) is a reason for Khazar's death on the control script's next check; the end "
	                "check logs KhazarGone=1. Unverified: whether the influence of each town won reaches far enough "
	                "towards the next for its villagers to see the pours (the gaps from the western Indian village to "
	                "the villages beyond, about 565, and from Lethys's nearest town to his other two, 540 and more, may "
	                "be too wide); each pour logs where the hand was put.",
	    .commands = std::move(byHand),
	    .onLand = true,
	});

	// The control script's next check starts Lethys's theft of the creature; nobody follows, so the chance runs out.
	// With no town left to him, the blows the player would deal his temple land on its heart: the heart is brought
	// down below a tenth of its life, which with no towns of his starts the exit on the next check
	std::vector<Command> temple {
	    ClickTexts(),
	    WatchLand(),
	    WaitTask("LethysVortex"),
	    WaitTask("LethysVortex", TaskWait::Finished),
	};
	for (const auto town : k_LethysTowns)
	{
		temple.push_back(TownWon(town, 1.0f));
	}
	temple.push_back(
	    {.kind = Kind::ScriptSetHealth, .delaySeconds = 2.0f, .amount = k_HeartBroughtDown, .thing = k_LethysHeart});
	temple.push_back(WaitTask("LeaveThroughVortexL2"));
	temple.push_back(LogGlobals(k_LandGlobals, k_LandScripts, 1.0f));
	all.push_back({
	    .id = "story.land2_lethys_temple_falls",
	    .name = "Story: Land 2's control script starts the exit through Lethys's temple",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 after story.land2_khazar_dies in the same story run. The texts are clicked on. "
	                   "It waits for the control script to start Lethys's theft of the creature, lets the chance to "
	                   "follow him run out, gives Lethys's three towns the belief again in case one went back to him, "
	                   "then brings his temple's heart down to a twentieth of its life (a shortcut for the player's "
	                   "blows) and waits for the control script to start the exit.",
	    .expected = "Lethys takes the creature into his vortex; with nobody following the chance is missed "
	                "(ChanceMissed=1). With Lethys holding no towns and his temple at a twentieth, the control script's "
	                "next check marks the exit done and starts LeaveThroughVortexL2, which the end check logs running "
	                "with DoneExitLethysVortex=1.",
	    .commands = std::move(temple),
	    .onLand = true,
	});

	// The exit's film, then the scroll over the vortex pressed on; the land is marked as being left, the control script
	// returns, and the story's top script stops the land's scripts and loads Land 3
	std::vector<Command> leaving {
	    ClickTexts(),
	    WaitText(k_ExitFilmLastText),
	    CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised),
	};
	leaving.back().delaySeconds = 3.0f;
	PressScrollWithin(leaving, k_Vortex, k_ExitScrollPickRadius);
	leaving.push_back(WaitGlobal("LeavingLandTwo", 1.0f));
	leaving.push_back(LogGlobals("VortexOpen LeavingLandTwo DoneExitLethysVortex", k_LandScripts));
	leaving.push_back(WaitTask("LandControl3"));
	leaving.push_back({.kind = Kind::ReplayChallenge, .delaySeconds = 1.0f, .value = k_Challenge});
	leaving.push_back(LogGlobals(k_LandGlobals, "LandControlAll LandControl2 LandControl3 LeaveThroughVortexL2", 2.0f));
	all.push_back({
	    .id = "story.land2_leave_through_the_vortex",
	    .name = "Story: leaving Land 2 through the vortex, into Land 3",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 after story.land2_lethys_temple_falls in the same story run. The texts are "
	                   "clicked on. Once the film's last line is read it puts the camera on the scroll over the vortex, "
	                   "presses on it with the hand (picking it within 5 of the vortex, away from Lethys's scroll), waits "
	                   "for the land's exit to be marked, then for Land 3's control script, and taps the scroll's "
	                   "challenge in the log.",
	    .expected = "The film: Lethys's temple explodes, the vortex opens and the advisors speak; the scroll appears 20 "
	                "above the vortex. The press switches it on and the camera dives into the vortex; the check logs "
	                "VortexOpen=0 and LeavingLandTwo=1. The control script returns, the story's top script stops every "
	                "other script and loads Land 3, whose control script runs; the log has no entry for the scroll.",
	    .commands = std::move(leaving),
	    .onLand = true,
	});
}

/// Lethys's temple, where his influence is the strongest on the land, and the camera put over it for his taunts
constexpr glm::vec2 k_LethysTemple {1010.74f, 3676.572f};
constexpr float k_TauntEyeAbove = 40.0f;
constexpr float k_TauntEyeBack = 50.0f;
/// Seconds the camera stays there: his first taunt, and the next a minute later
constexpr float k_TauntsSeconds = 75.0f;
/// The neutral Indian village by the store (town 12), Khazar's home town (town 1) and Lethys's nearest town (town 4),
/// each watched by the land's town-ownership script, where the land makes them
constexpr glm::vec2 k_NeutralVillage {2862.58f, 3448.27f};
constexpr glm::vec2 k_KhazarHome {2761.43f, 1980.72f};
constexpr glm::vec2 k_LethysNearTown {1800.44f, 3024.32f};
/// Seconds after each change of hands for the gods' visits to the camera and their lines to end
constexpr float k_ReactionSeconds = 40.0f;
constexpr std::string_view k_ReactionScripts = "KhazarAI LethysTauntAI KhazarConfrontPlayer LethysConfrontPlayer "
                                               "Land2TownTakeOver TownOwnershipCheck";

/// A town of the land won by a player, by a gift of belief in that player larger than any other belief it holds: its
/// next fold hands it over, and the town keeps to it. A take-over alone does not last, as the next fold hands the
/// town back to the player it believes in most. Each later change of the same town gives more than the one before
[[nodiscard]] Command TownTo(glm::vec2 town, PlayerNames player, float belief)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = player,
	        .delaySeconds = 1.0f,
	        .amount = belief,
	        .thing = ThingNear {.type = script::ObjectType::Town, .point = town, .radius = 10.0f}};
}

void AddGodsReactToTownsScenario(std::vector<Scenario>& all)
{
	// Once the arrival is over: the camera over Lethys's temple for his taunts, then towns changing hands as the land's
	// town-ownership script watches them, each followed by the gods' lines
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    WatchLand(),
	    CameraOn(k_LethysTemple, k_TauntEyeAbove, k_TauntEyeBack, 0.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_TauntsSeconds),
	    CameraOn(k_NeutralVillage, k_TauntEyeAbove, k_TauntEyeBack, 0.0f),
	    // [shortcut] the neutral village won by the player, then lost to Lethys
	    TownTo(k_NeutralVillage, PlayerNames::PLAYER_ONE, 2.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	    TownTo(k_NeutralVillage, PlayerNames::PLAYER_THREE, 4.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	    // [shortcut] Khazar's home town lost to Lethys, given back to Khazar, then won by the player
	    TownTo(k_KhazarHome, PlayerNames::PLAYER_THREE, 4.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	    TownTo(k_KhazarHome, PlayerNames::PLAYER_TWO, 8.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	    TownTo(k_KhazarHome, PlayerNames::PLAYER_ONE, 16.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	    // [shortcut] last, as it is a reason for Khazar's death: Lethys's nearest town won by the player
	    TownTo(k_LethysNearTown, PlayerNames::PLAYER_ONE, 2.0f),
	    LogGlobals(k_LandGlobals, k_ReactionScripts, k_ReactionSeconds),
	};
	all.push_back({
	    .id = "story.land2_gods_react_to_towns",
	    .name = "Story: Land 2's gods taunt the player and speak as towns change hands",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 after story.land1_skip_to_land2 in the same story run. The texts are clicked "
	                   "on. Once the arrival is over the camera is put over Lethys's temple for 75 seconds. Then, 40 "
	                   "seconds apart, towns change hands by gifts of belief larger than any they hold (shortcuts "
	                   "for winning and losing them): the neutral Indian village by the store to the player, then to "
	                   "Lethys; Khazar's home town to Lethys, back to Khazar, then to the player; and last Lethys's "
	                   "nearest town to the player.",
	    .expected = "Over his temple Lethys's influence is the stronger, so his hand comes to the camera and he taunts in "
	                "his own voice, and again a minute later. Khazar congratulates the player on the neutral village; "
	                "Lethys gloats when he takes it; Khazar laments his town lost to Lethys, says nothing when it comes "
	                "back, and complains when the player takes it; for Lethys's town Khazar praises the player and six "
	                "seconds later Lethys rages, and the town is a reason for Khazar's death on the control script's next "
	                "check. Unverified: whether the dialogue is free for each line (a line is said only then).",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::land2_quests::AddLeaveVortexL2Scenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised),
	};
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitGlobal("LeavingLandTwo", 1.0f));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
	// the scroll's challenge tapped in the log, which has no entry for it
	commands.push_back({.kind = Kind::ReplayChallenge, .delaySeconds = 1.0f, .value = k_Challenge});
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_2",
	    .name = "Land 2 quest run: Leave through the vortex",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/LeaveThroughVortexL2 on Land2.txt). The texts "
	                   "that wait for a click are clicked on. Once the film is over and the scroll's notifier runs, it "
	                   "puts the camera on the scroll over the vortex, presses on it with the hand and waits for the "
	                   "land's exit to be marked, then taps the scroll's challenge in the log.",
	    .expected = "The film: Lethys's temple explodes, the vortex opens and the advisors speak; the scroll appears 20 "
	                "above the vortex. The press switches it on, the camera flies the recorded dive into the vortex and "
	                "the screen fades to black; the end check logs VortexOpen=0 and LeavingLandTwo=1, and the log has no "
	                "entry for the scroll.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
	AddStoryExitScenarios(all);
	AddGodsReactToTownsScenario(all);
}
