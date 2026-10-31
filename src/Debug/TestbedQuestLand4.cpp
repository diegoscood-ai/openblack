/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 4's gold scrolls as quest runs, each played after a land entry's quest= has started it: Leave Through the
// Vortex, The Heartbroken Man and The Totem Puzzle. The timelines do what the player would: they click the texts that
// wait for a click, bring the camera to the hidden vortex, press on the scrolls, bring the nomad's girlfriend to him,
// hand the Japanese village to the player and click the bell towers in each round's order. The points are map
// coordinates, taken from the places the challenge scripts name.

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
/// The hidden vortex, where its scroll appears raised 20 above it
constexpr glm::vec2 k_Vortex {2060.18f, 2591.698f};
constexpr float k_ScrollRaised = 20.0f;
/// The camera first held about 400 from the vortex for a rumble or two, within the 500 that starts the rumbles'
/// count, then brought within the 100 that opens it
constexpr float k_FarBack = 380.0f;
constexpr float k_FarAbove = 100.0f;
constexpr float k_RumbleSeconds = 35.0f;
/// The Heartbroken Man's scroll, by the woman's shack
constexpr glm::vec2 k_NomadScroll {2930.707f, 2875.43f};
/// The girlfriend, picked as the villager nearest the first spot of her walk once she is back there: from her start she
/// walks at a tenth of her speed round the three spots, sitting and gesturing at each, and is back at the first, where
/// she sits again, about 195 seconds after her walk begins. A pick of an Aztec housewife within 150 of her start took
/// another villager of the town
constexpr glm::vec2 k_GirlfriendFirstSpot {3343.5378f, 3080.7058f};
constexpr float k_GirlfriendPickRadius = 3.0f;
constexpr float k_GirlfriendBackAtFirstSpotSeconds = 206.0f;
/// Where she is put down: within 50 of the man in his house on the mountain, which brings him out to his doorstep, 10
/// from her, within the 50 the reunion asks of them
constexpr glm::vec2 k_ByTheHouse {3040.0f, 2660.0f};
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

/// The Totem Puzzle's scroll, just north of the bell towers
constexpr glm::vec2 k_PuzzleScroll {2542.824f, 3109.667f};
/// The Japanese village, where the land's setup finds it
constexpr glm::vec2 k_JapaneseVillage {2590.363f, 3196.893f};
constexpr float k_VillagePickRadius = 50.0f;
/// The belief in the player that wins the village: more than its belief in the neutral player (0.5)
constexpr float k_WinningBelief = 2.0f;
/// The five bell towers, in the puzzle's numbering; each is picked within this of its spot, the nearest standing about
/// 10 from another
constexpr std::array<glm::vec2, 5> k_BellTowers {{
    {2538.989f, 3076.031f},
    {2539.545f, 3085.679f},
    {2534.248f, 3096.156f},
    {2523.743f, 3101.693f},
    {2510.944f, 3102.708f},
}};
constexpr float k_TowerPickRadius = 3.0f;
/// The camera over the middle of the towers, high above them and a little to their south, so that none stands in front
/// of another on the screen
constexpr glm::vec2 k_TowersMiddle {2529.5f, 3092.5f};
constexpr float k_TowersEyeAbove = 70.0f;
constexpr float k_TowersEyeBack = 25.0f;
/// Each round's towers, from 1, in the order the round's sequence plays them; 0 ends a shorter round
constexpr std::array<std::array<uint8_t, 9>, 4> k_Rounds {{
    {1, 2, 3},
    {1, 2, 3, 5, 2},
    {1, 2, 3, 5, 2, 3, 1},
    {1, 2, 3, 5, 2, 3, 1, 4, 1},
}};

constexpr std::string_view k_VortexGlobals = "VortexOpen LandNumber";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL4 ChallengeHighlightNotify";
constexpr std::string_view k_NomadGlobals = "StartLand4Nomad";
constexpr std::string_view k_NomadScripts = "Land4Nomad ChallengeHighlightNotify WomanWalkingAroundTown";
constexpr std::string_view k_PuzzleGlobals = "TowerClicked KeepQuiet StartLand4Nomad";
constexpr std::string_view k_PuzzleScripts = "Land4Puzzle ChallengeHighlightNotify SeeWhichTowerClickedOn";

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

/// [shortcut] The girlfriend set down at a point, as the hand would carry her there and put her down
[[nodiscard]] Command PutGirlfriendBy(glm::vec2 point, float delay)
{
	return {.kind = Kind::MoveThing,
	        .delaySeconds = delay,
	        .point = point,
	        .thing = ThingNear {
	            .type = script::ObjectType::Villager, .point = k_GirlfriendFirstSpot, .radius = k_GirlfriendPickRadius}};
}

/// A bell tower clicked with the hand, as the player clicks it: once the puzzle's wait for a click has begun (it clears
/// the last tower clicked, and any click before it is thrown away), the pointer on the tower, the Action button pressed
/// once the hand is over it and let go; then the wait for the puzzle to take it
void ClickTower(std::vector<Command>& commands, uint8_t tower)
{
	const ThingNear pick {.type = script::ObjectType::Abode,
	                      .point = k_BellTowers.at(static_cast<size_t>(tower - 1)),
	                      .radius = k_TowerPickRadius};
	commands.push_back(WaitGlobal("TowerClicked", 0.0f));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 0.2f, .thing = pick});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .waitUntilFree = true, .value = k_ActionButton, .thing = pick});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton});
	commands.push_back(WaitGlobal("TowerClicked", static_cast<float>(tower)));
}

void AddVortex(std::vector<Scenario>& all)
{
	// the camera within 500 of the vortex for a rumble or two, then within 100, which opens it
	auto near = CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised);
	near.delaySeconds = k_RumbleSeconds;
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("LeaveThroughVortexL4"),
	    LogEvery(k_VortexGlobals, k_VortexScripts),
	    CameraOn(k_Vortex, k_FarAbove, k_FarBack, 0.0f),
	    near,
	    WaitTask("ChallengeHighlightNotify"),
	};
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitTask("LeaveThroughVortexL4", TaskWait::Finished));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_4",
	    .name = "Land 4 quest run: Leave through the vortex",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/LeaveThroughVortexL4 on Land4.txt). The texts "
	                   "that wait for a click are clicked on. It holds the camera about 400 from the hidden vortex for 35 "
	                   "seconds, then brings it over the vortex, within 100 of it; once the scroll's notifier runs it "
	                   "presses on the scroll with the hand and waits for the quest to end.",
	    .expected = "The rumbles every 30 seconds shake the camera, more as it comes near; within 100 the vortex opens "
	                "in its cut scene with its six lines and the advisors' five; the scroll appears 20 above it, and its "
	                "press flies the camera into the vortex and ends the quest.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddHeartbrokenMan(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Land4Nomad"),
	    LogEvery(k_NomadGlobals, k_NomadScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_NomadScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_NomadScroll);
	// the introduction plays; then the girlfriend walks about her town
	commands.push_back(WaitTask("WomanWalkingAroundTown"));
	// [shortcut] once she is back at the first spot of her walk, she is set down by the man's house, which brings him
	// out to his doorstep within 50 of her
	commands.push_back(PutGirlfriendBy(k_ByTheHouse, k_GirlfriendBackAtFirstSpotSeconds));
	commands.push_back(WaitTask("Land4Nomad", TaskWait::Finished));
	commands.push_back(LogGlobals(k_NomadGlobals, k_NomadScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_heartbroken_man",
	    .name = "Land 4 quest run: The Heartbroken Man",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/StartLand4Nomad=1/Land4Nomad on "
	                   "Land4.txt). The texts that wait for a click are clicked on. It presses on the scroll by the "
	                   "woman's shack; once the introduction is over and the girlfriend is back at the first spot "
	                   "of her walk about her town, it sets her down by the man's house, then waits for the quest to end.",
	    .expected = "The introduction: the woman at her shack and the man on the mountain, with their lines; the "
	                "girlfriend walks about the town. Set down by his house, the man comes out to his doorstep; with her "
	                "within 50 of him there, the reunion plays and the stone's scene follows, and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
void AddTotemPuzzle(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Land4Puzzle"),
	    LogEvery(k_PuzzleGlobals, k_PuzzleScripts),
	    // [shortcut] the Japanese village takes more belief in the player than in anyone else, as winning it gives,
	    // and its next fold hands it over
	    // (a plain change of hands goes back at the town's next belief fold, its belief in the player being none)
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    // the puzzle looks every 10 seconds; then its scroll comes
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_PuzzleScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_PuzzleScroll);
	// the elder's introduction plays; once it is over the first round's sequence plays, and the camera is put over the
	// towers while it does (the puzzle keeps the advisors quiet from then)
	commands.push_back(WaitText(5380));
	commands.push_back(WaitGlobal("KeepQuiet", 1.0f));
	commands.push_back(CameraOn(k_TowersMiddle, k_TowersEyeAbove, k_TowersEyeBack, 0.0f));
	for (const auto& round : k_Rounds)
	{
		// the round's first wait for a click: after its sequence, which throws away the clicks made while it plays
		commands.push_back(WaitTask("SeeWhichTowerClickedOn"));
		for (const auto tower : round)
		{
			if (tower == 0)
			{
				break;
			}
			ClickTower(commands, tower);
		}
	}
	commands.push_back(WaitTask("Land4Puzzle", TaskWait::Finished));
	commands.push_back(LogGlobals(k_PuzzleGlobals, k_PuzzleScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_totem_puzzle",
	    .name = "Land 4 quest run: The Totem Puzzle",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/Land4Puzzle on Land4.txt). The "
	                   "texts that wait for a click are clicked on. It hands the Japanese village to the player, presses "
	                   "on the scroll by the bell towers once it comes, lets the elder's introduction play, puts the "
	                   "camera over the towers, and in each of the four rounds, once the puzzle waits for a click, "
	                   "clicks the towers with the hand in the round's order, each once the puzzle has taken the one "
	                   "before; then waits for the quest to end.",
	    .expected = "The scroll comes within 10 seconds of the village changing hands; its press starts the elder's "
	                "introduction with his four lines and the log at 0. Each round's sequence flashes and rings the "
	                "towers; each tower clicked flashes and rings, and each round answered plays the stone chime. After "
	                "the fourth the towers burst and are deleted, the log goes to 1, the shields go, and the stone's "
	                "scene follows: the good advisor's line, the stone raised and exploded, and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand4Scenarios(std::vector<Scenario>& all)
{
	AddVortex(all);
	AddHeartbrokenMan(all);
	AddTotemPuzzle(all);
}
