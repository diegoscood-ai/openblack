/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 4's gold scrolls as quest runs, each played after a land entry's quest= has started it: Leave Through the
// Vortex, The Heartbroken Man and The Totem Puzzle, and its silver scroll The Treacherous Path. The timelines do what the
// player would: they click the texts that wait for a click, bring the camera to the hidden vortex, press on the scrolls, bring
// the nomad's girlfriend to him, hand the Japanese village to the player and click the bell towers in each round's order. The
// points are map coordinates, taken from the places the challenge scripts name.

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

/// The Treacherous Path's silver scroll by the blind woman's hut, raised 10; the Aztec village, which must be the
/// player's before it comes; the wolf pack on her way, picked within this of its spot
constexpr glm::vec2 k_BlindWomanScroll {3703.723f, 3105.303f};
constexpr float k_BlindWomanScrollRaised = 10.0f;
constexpr glm::vec2 k_AztecVillage {3385.63f, 3100.74f};
constexpr glm::vec2 k_WolfPack {3509.908f, 3280.037f};
constexpr float k_WolfPickRadius = 80.0f;
constexpr uint32_t k_WolfSubtype = 2;
constexpr size_t k_PackWolves = 8;

constexpr std::string_view k_VortexGlobals = "VortexOpen LandNumber";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL4 ChallengeHighlightNotify";
constexpr std::string_view k_NomadGlobals = "StartLand4Nomad";
constexpr std::string_view k_NomadScripts = "Land4Nomad ChallengeHighlightNotify WomanWalkingAroundTown";
constexpr std::string_view k_PuzzleGlobals = "TowerClicked KeepQuiet StartLand4Nomad";
constexpr std::string_view k_PuzzleScripts = "Land4Puzzle ChallengeHighlightNotify SeeWhichTowerClickedOn";
constexpr std::string_view k_PathGlobals = "TimesDied BlindWomanStillGoingStrong";
constexpr std::string_view k_PathScripts = "BlindWomanMain BlindWomanJourney GremlinSniper ChallengeHighlightNotify";

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

/// Leave Through the Vortex from its start: the camera held near the hidden vortex, then over it, the scroll pressed on
/// once its notifier runs, and the wait for the quest to end
void AppendVortex(std::vector<Command>& commands)
{
	// the camera within 500 of the vortex for a rumble or two, then within 100, which opens it
	auto near = CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised);
	near.delaySeconds = k_RumbleSeconds;
	commands.push_back(WaitTask("LeaveThroughVortexL4"));
	commands.push_back(LogEvery(k_VortexGlobals, k_VortexScripts));
	commands.push_back(CameraOn(k_Vortex, k_FarAbove, k_FarBack, 0.0f));
	commands.push_back(near);
	commands.push_back(WaitTask("ChallengeHighlightNotify"));
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitTask("LeaveThroughVortexL4", TaskWait::Finished));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
}

void AddVortex(std::vector<Scenario>& all)
{
	std::vector<Command> commands {ClickTexts()};
	AppendVortex(commands);
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
/// Morg's ridge: the camera brought within the 75 of him that makes him mutter, for long enough for one mutter, then
/// put back on the blind woman's scroll
constexpr glm::vec2 k_MorgRidge {3209.364f, 3277.383f};
constexpr float k_MorgWatchSeconds = 30.0f;

void AddTreacherousPath(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("BlindWomanMain"),
	    LogEvery(k_PathGlobals, k_PathScripts),
	    // [shortcut] the Aztec village takes more belief in the player than in anyone else, as winning it gives, and its
	    // next fold hands it over: the scroll comes once the village is the player's
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_AztecVillage, .radius = k_VillagePickRadius}},
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_BlindWomanScroll, k_EyeAbove + k_BlindWomanScrollRaised, k_EyeBack, k_BlindWomanScrollRaised),
	};
	PressScroll(commands, k_BlindWomanScroll);
	// her introduction plays; then she sets off along the road
	commands.push_back(WaitTask("BlindWomanJourney"));
	// [shortcut] the wolf pack on her way killed, as the player's hand or creature would kill it before she comes
	for (size_t i = 0; i < k_PackWolves; ++i)
	{
		commands.push_back({.kind = Kind::ScriptSetHealth,
		                    .delaySeconds = i == 0 ? 2.0f : 0.5f,
		                    .amount = 0.0f,
		                    .thing = ThingNear {.type = script::ObjectType::Animal,
		                                        .subtype = k_WolfSubtype,
		                                        .point = k_WolfPack,
		                                        .radius = k_WolfPickRadius}});
	}
	// the camera over Morg's ridge for a while, as a player looking about would, for his mutter; then back
	commands.push_back(CameraOn(k_MorgRidge, k_EyeAbove, k_EyeBack, 0.0f));
	auto back = CameraOn(k_BlindWomanScroll, k_EyeAbove + k_BlindWomanScrollRaised, k_EyeBack, k_BlindWomanScrollRaised);
	back.delaySeconds = k_MorgWatchSeconds;
	commands.push_back(back);
	commands.push_back(WaitTask("BlindWomanMain", TaskWait::Finished));
	commands.push_back(LogGlobals(k_PathGlobals, k_PathScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_treacherous_path",
	    .name = "Land 4 quest run: The Treacherous Path",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/BlindWomanMain on Land4.txt). "
	                   "The texts that wait for a click are clicked on. It gives the Aztec village the belief that wins "
	                   "it, presses on the silver scroll by the blind woman's hut once it comes, lets her introduction "
	                   "play, kills the wolf pack on her way once she sets off, holds the camera over Morg's ridge for 30 "
	                   "seconds, and waits for the quest to end.",
	    .expected = "The scroll comes once the village is the player's; its press starts her introduction with her "
	                "three lines and the log at 0. She walks the road past the burning birches and Morg's sheep patch "
	                "with the advisors' remarks, reaches her brother's hut, and the reunion, the log at 1 and the "
	                "wolf's scroll follow. Unverified: that she lives through Morg's boulders and the fire without "
	                "the player's help.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The land's control script's waits: the three Guardian Stones, and the scripts it starts
constexpr std::string_view k_ControlGlobals = "LighteningMeteorBroken DarknessMeteorBroken FireMeteorBroken "
                                              "StartLand4Nomad PlayerSavedUndeadTown CreatureHasGotCreed VortexOpen";
constexpr std::string_view k_ControlScripts =
    "LandControl4 SetupLand4 BeginLand4 SetupUndeadVillage SetUndeadTownBeliefToZero SetUndeadTownHealth "
    "ThesiusPuzzleLand4 JapaneseTotemPuzzle FishPuzzle RunCreatureBreederLand BuildUpTown Land4Ogre Land4Nomad "
    "Land4Puzzle KeepCreatureAwayFromOgre ManExplainsAboutNemesis BlindWomanMain ManExplainsAboutUndeadVillage "
    "CreatureGetsCreed LeaveThroughVortexL4";
/// How long the land runs with every quest in the background before the stones are marked broken, so that each
/// background quest has begun
constexpr float k_BackgroundSeconds = 60.0f;
/// The seconds between one stone marked broken and the next, so that what each stone's watchers do shows apart
constexpr float k_StoneGapSeconds = 30.0f;

void AddLandControl(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the last script the land's control script starts in the background
	    WaitTask("BlindWomanMain"),
	    LogEvery(k_ControlGlobals, k_ControlScripts),
	    LogGlobals(k_ControlGlobals, k_ControlScripts, k_BackgroundSeconds),
	    // [shortcut] the three Guardian Stones marked broken one by one, as The Heartbroken Man, The Totem Puzzle and
	    // the fight with Sleg would
	    {.kind = Kind::SetGlobal, .amount = 1.0f, .global = "DarknessMeteorBroken"},
	    {.kind = Kind::SetGlobal, .delaySeconds = k_StoneGapSeconds, .amount = 1.0f, .global = "FireMeteorBroken"},
	    {.kind = Kind::SetGlobal, .delaySeconds = k_StoneGapSeconds, .amount = 1.0f, .global = "LighteningMeteorBroken"},
	    LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f),
	    // the land's control script then runs the man's explanation of the Undead Village
	    WaitTask("ManExplainsAboutUndeadVillage"),
	};
	AppendUndeadVillageChapter(commands, true);
	AppendVortex(commands);
	// the control script ends with the exit quest, on the same turn: a wait that began after it would never see it run
	commands.push_back(WaitTask("LandControl4", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	all.push_back({
	    .id = "quest.land_control_4",
	    .name = "Land 4 quest run: the land's control script",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl4 on Land4.txt). The texts that wait for a "
	                   "click are clicked on. Once the land's control script has started its background quests it lets "
	                   "them run for a minute, then marks the darkness, fire and lightning Guardian Stones broken 30 "
	                   "seconds apart; it plays Undead Village from its scroll as quest.undead_village does, without "
	                   "starting the Creed's watcher and with both totems marked as standing at their full height once "
	                   "the second is held up, and Leave Through the Vortex as "
	                   "quest.leave_through_the_vortex_land_4 does, and waits for the control script to end.",
	    .expected = "Every background quest runs; with the three stones broken the control script runs the man's "
	                "explanation of the Undead Village, the Creed scene and the exit vortex quest in turn, and ends "
	                "with it. Without being fed the creature, hungry by then, walked off the first totem and off the "
	                "Creed to eat in the home village (runs of 2026-11-05).",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The Totem Puzzle's scroll comes within 20 seconds of the Japanese village changing hands (the quest run of the
/// puzzle alone); the camera goes to it after this, as other quests' scroll notifiers run in the land's own run
constexpr float k_PuzzleScrollSeconds = 30.0f;
/// Keiko as her walk begins, picked by her kind of villager where she is made; her watcher's spot, the second of her
/// walk, where it puts her back; and a point on the road east of her village, 150 from that spot and far out of the
/// camera's view while it is on the Heartbroken Man's scroll
constexpr glm::vec2 k_KeikoStart {3346.66f, 3094.67f};
constexpr uint32_t k_KeikoSubtype = 14;
constexpr float k_KeikoPickRadius = 3.0f;
constexpr glm::vec2 k_KeikoResetSpot {3324.67f, 3087.922f};
constexpr glm::vec2 k_FarFromHerVillage {3474.67f, 3087.922f};
/// The seconds left for her watcher to put her back before she is picked at its spot, and the radius of that pick
constexpr float k_PutBackSeconds = 5.0f;
constexpr float k_ResetPickRadius = 5.0f;

void AddLandControlStones(std::vector<Scenario>& all)
{
	const ThingNear keikoAtStart {
	    .type = script::ObjectType::Villager, .subtype = k_KeikoSubtype, .point = k_KeikoStart, .radius = k_KeikoPickRadius};
	const ThingNear keikoPutBack {.type = script::ObjectType::Villager,
	                              .subtype = k_KeikoSubtype,
	                              .point = k_KeikoResetSpot,
	                              .radius = k_ResetPickRadius};
	auto puzzleScroll = CameraOn(k_PuzzleScroll, k_EyeAbove, k_EyeBack, 0.0f);
	puzzleScroll.delaySeconds = k_PuzzleScrollSeconds;
	std::vector<Command> commands {
	    ClickTexts(),
	    // the last script the land's control script starts in the background
	    WaitTask("BlindWomanMain"),
	    LogEvery(k_ControlGlobals, k_ControlScripts),
	    WaitTask("Land4Puzzle"),
	    // [shortcut] the Japanese village takes more belief in the player than in anyone else, as winning it gives,
	    // and its next fold hands it over
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    puzzleScroll,
	};
	PressScroll(commands, k_PuzzleScroll);
	// the elder's introduction, whose end lets the Heartbroken Man go on; the man who explains Nemesis's curse has his
	// scene once the camera is by the Japanese village, and it is let end
	commands.push_back(WaitText(5380));
	commands.push_back(WaitTask("ManExplainsAboutNemesis", TaskWait::NotRunning));
	// the puzzle's first round, begun as the introduction ended, runs out unplayed during that scene; the elder's scene
	// for a lost round then holds the camera with the scrolls hidden, and the puzzle is quiet again once it is over
	commands.push_back(WaitGlobal("KeepQuiet", 1.0f));
	commands.push_back(WaitGlobal("KeepQuiet", 0.0f));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	commands.push_back(CameraOn(k_NomadScroll, k_EyeAbove, k_EyeBack, 0.0f));
	PressScroll(commands, k_NomadScroll);
	commands.push_back(WaitTask("WomanWalkingAroundTown"));
	// [shortcut] Keiko set down far from her village and out of view, as the hand would carry her off and drop her
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_FarFromHerVillage, .thing = keikoAtStart});
	// [shortcut] once her watcher has put her back, she is set down by the man's house, as the hand would carry her
	commands.push_back(
	    {.kind = Kind::MoveThing, .delaySeconds = k_PutBackSeconds, .point = k_ByTheHouse, .thing = keikoPutBack});
	commands.push_back(WaitTask("Land4Nomad", TaskWait::Finished));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	// [shortcut] the fire and lightning Guardian Stones marked broken, as the Totem Puzzle and the fight with Sleg would
	commands.push_back({.kind = Kind::SetGlobal, .delaySeconds = 1.0f, .amount = 1.0f, .global = "FireMeteorBroken"});
	commands.push_back({.kind = Kind::SetGlobal, .delaySeconds = 1.0f, .amount = 1.0f, .global = "LighteningMeteorBroken"});
	commands.push_back(WaitTask("ManExplainsAboutUndeadVillage"));
	commands.push_back(WaitTask("ManExplainsAboutUndeadVillage", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	all.push_back({
	    .id = "quest.land_control_4_stones",
	    .name = "Land 4 quest run: the land's control script, with the Heartbroken Man played",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl4 on Land4.txt). The texts that wait for a "
	                   "click are clicked on. Once the land's control script has started its background quests it hands "
	                   "the Japanese village to the player and presses on the Totem Puzzle's scroll; once the elder's "
	                   "introduction, the scene of the man who explains Nemesis's curse and the elder's scene for the "
	                   "puzzle's lost first round are over it presses on the Heartbroken Man's scroll, sets Keiko down "
	                   "far from her village out of view as her walk begins, and once she has been put back sets her "
	                   "down by the man's house; then marks the fire and lightning Guardian Stones broken and waits for "
	                   "the man's explanation of the Undead Village to end.",
	    .expected = "The elder's introduction lets the Heartbroken Man go on: its scroll is made. Keiko, out of view and "
	                "more than 100 from her watcher's spot, is put back there; by the man's house the reunion plays and "
	                "the darkness stone breaks. With the other two stones marked broken, the control script runs the "
	                "man's explanation of the Undead Village. The scrolls are hidden while a scene holds the camera: a "
	                "press during the elder's scene for the lost round found no scroll (runs of 2026-11-06).",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

namespace
{
void AddFishPuzzle(std::vector<Scenario>& all);
void AddFishPuzzleBoyDies(std::vector<Scenario>& all);
void AddFishPuzzleBoyThrown(std::vector<Scenario>& all);
} // namespace

void testbed_scenarios::AddQuestLand4Scenarios(std::vector<Scenario>& all)
{
	AddVortex(all);
	AddLandControl(all);
	AddLandControlStones(all);
	AddHeartbrokenMan(all);
	AddTotemPuzzle(all);
	AddTreacherousPath(all);
	AddFishPuzzle(all);
	AddFishPuzzleBoyDies(all);
	AddFishPuzzleBoyThrown(all);
}

namespace
{
/// The Fish Puzzle's scroll on the shore, the middle of its net out in the bay, and where the boy's turtle is made
constexpr glm::vec2 k_FishScroll {2436.942f, 3598.528f};
constexpr glm::vec2 k_FishNet {2497.891f, 3628.35f};
constexpr glm::vec2 k_Turtle {2413.198f, 3630.057f};
/// The camera high over the net while the fish are herded, so every tap's point is on the screen
constexpr float k_OverTheNet = 70.0f;
constexpr float k_BackFromTheNet = 10.0f;
/// The taps on the water, one every half second for a minute: each is just beyond the fish farthest from the net
constexpr size_t k_FishTaps = 120;
constexpr float k_SecondsBetweenTaps = 0.5f;
/// The boy's last line before the herding, and the good advisor's last after the reward
constexpr uint32_t k_HerdingLine = 4689;
constexpr uint32_t k_AfterTheRewardLine = 4694;
/// The evil advisor's lines when the turtle's scroll is seen and when it is pressed with the creature too far, the good
/// advisor's question once the creature is by it, and the evil advisor's line after the swap
constexpr uint32_t k_TurtleSeenLine = 5103;
constexpr uint32_t k_CreatureTooFarLine = 5104;
constexpr uint32_t k_SwapQuestionLine = 2420;
constexpr uint32_t k_SwappedLine = 5105;
/// Where the player's creature is put, within the 50 of the turtle the swap needs
constexpr glm::vec2 k_ByTheTurtle {2390.0f, 3625.0f};
/// The turtle, picked where it was made
constexpr ThingNear k_TurtleThing {.type = script::ObjectType::Creature, .point = k_Turtle, .radius = 6.0f};

constexpr std::string_view k_FishScripts = "FishPuzzle ChallengeHighlightNotify SwapCreatures";

/// The quest's start: the texts clicked on, the camera on the scroll by the boy once it is up
[[nodiscard]] std::vector<Command> FishPuzzleStart()
{
	return {
	    ClickTexts(),
	    WaitTask("FishPuzzle"),
	    LogEvery("", k_FishScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_FishScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
}

/// Once the introduction's cinema is over, the camera high over the net and the water tapped behind the stray fish;
/// with `waitForTheLine`, after the wait for the boy's herding line, else at once
void HerdTheFish(std::vector<Command>& commands, size_t taps, bool waitForTheLine = true)
{
	if (waitForTheLine)
	{
		commands.push_back(WaitText(k_HerdingLine));
	}
	auto overTheNet = CameraOn(k_FishNet, k_OverTheNet, k_BackFromTheNet, 0.0f);
	overTheNet.delaySeconds = 2.0f;
	commands.push_back(overTheNet);
	for (size_t tap = 0; tap < taps; ++tap)
	{
		commands.push_back({.kind = Kind::TapStrayFish, .delaySeconds = tap == 0 ? 1.0f : k_SecondsBetweenTaps});
	}
}

/// The turtle's swap: its scroll pressed first with the creature far away, then with it by the turtle; once the good
/// advisor asks, the turtle pressed on confirms
void SwapForTheTurtle(std::vector<Command>& commands)
{
	commands.push_back(CameraOn(k_Turtle, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back(WaitText(k_TurtleSeenLine));
	PressScroll(commands, k_Turtle);
	commands.push_back(WaitText(k_CreatureTooFarLine));
	// [shortcut] the player's creature put by the turtle, as the leash would lead it there
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_ByTheTurtle});
	PressScroll(commands, k_Turtle);
	// once the creature has come to it, the good advisor asks; the turtle pressed on confirms the swap
	commands.push_back(WaitText(k_SwapQuestionLine));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_TurtleThing});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = k_TurtleThing});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(WaitText(k_SwappedLine));
}

void AddFishPuzzle(std::vector<Scenario>& all)
{
	auto commands = FishPuzzleStart();
	PressScroll(commands, k_FishScroll);
	// the introduction plays; once its cinema is over the camera is put high over the net and the water is tapped
	// behind the stray fish until all are in the net
	HerdTheFish(commands, k_FishTaps);
	// the reward: the turtle and its scroll; pressed first with the creature far away, then with it by the turtle
	commands.push_back(WaitTask("SwapCreatures"));
	commands.push_back(WaitText(k_AfterTheRewardLine));
	SwapForTheTurtle(commands);
	commands.push_back(WaitTask("FishPuzzle", TaskWait::Finished));
	commands.push_back(LogGlobals("", k_FishScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_fish_puzzle",
	    .name = "Land 4 quest run: The Fish Puzzle",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/FishPuzzle on Land4.txt). The "
	                   "texts that wait for a click are clicked on. It presses on the scroll by the fisherman's boy; once "
	                   "the introduction is over it puts the camera high over the net and taps the water just beyond the "
	                   "fish farthest from the net every half second for a minute. Once the turtle is given it presses on "
	                   "the turtle's scroll with the creature far away, puts the creature by the turtle and presses it "
	                   "again, then presses the turtle when the good advisor asks, and waits for the quest to end.",
	    .expected = "The boy's four lines; the fish flee each tap and are driven into the net, which closes once all 30 "
	                "have been in it for half a second. The reward scene with the boy's lines and the turtle, the "
	                "advisors' three lines and the boy's walk home; the turtle's scroll pressed from afar gives the evil "
	                "advisor's line, then with the creature by it the creatures swap in their scene. Unverified: how "
	                "many taps the herding takes.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The boy, picked by his kind of villager where he stands for the quest; and, once the minute of taps is over, on his
/// walk home: within 120 of the point 170 along the way from his spot towards his village, which takes in the stretch
/// from 50 to 290 along (he walks some 6 a second from the reward's scene on, and reaches home 426 along) and none of
/// his village
constexpr glm::vec2 k_BoysSpot {2427.0f, 3612.758f};
constexpr glm::vec2 k_BoyWalkingHome {2497.2f, 3457.9f};
constexpr uint32_t k_BoySubtype = 24;
constexpr float k_BoyPickRadius = 20.0f;
constexpr float k_WalkingBoyPickRadius = 120.0f;
/// The seconds after the herding line is read before the boy is killed while the puzzle is played, past the end of the
/// introduction's cinema some four and a half seconds later: a villager the script holds keeps his health while its
/// scene plays (a kill three seconds after the line, inside the cinema, left him at full health)
constexpr float k_AfterTheIntroSeconds = 8.0f;

/// [shortcut] The boy killed by a script's SET_PROPERTY of his health, in place of the player's blow; the age
/// the quest gives him makes him a child to the scripts
[[nodiscard]] Command KillTheBoy(glm::vec2 near, float radius, float delay)
{
	return {.kind = Kind::ScriptSetHealth,
	        .delaySeconds = delay,
	        .amount = 0.0f,
	        .thing = ThingNear {.type = script::ObjectType::VillagerChild,
	                            .subtype = k_BoySubtype,
	                            .point = near,
	                            .radius = radius,
	                            .aliveOnly = true}};
}

void AddFishPuzzleBoyDies(std::vector<Scenario>& all)
{
	// before the scroll is pressed
	{
		auto commands = FishPuzzleStart();
		commands.push_back(KillTheBoy(k_BoysSpot, k_BoyPickRadius, 2.0f));
		commands.push_back(WaitTask("FishPuzzle", TaskWait::Finished));
		commands.push_back(LogGlobals("", k_FishScripts, 1.0f));
		all.push_back({
		    .id = "quest.the_fish_puzzle_boy_dead_first",
		    .name = "Land 4 quest run: The Fish Puzzle, the boy dead before the scroll is pressed",
		    .facet = Facet::Story,
		    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/FishPuzzle on Land4.txt). "
		                   "The texts that wait for a click are clicked on. Once the scroll by the boy is up the camera "
		                   "is put on it and the boy is killed (a shortcut for the player's blow); the scroll is never "
		                   "pressed, and the timeline waits for the quest to end.",
		    .expected = "The quest ends quietly: the scroll goes and nothing is said.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	// while the puzzle is played, before the fish are herded
	{
		auto commands = FishPuzzleStart();
		PressScroll(commands, k_FishScroll);
		commands.push_back(WaitText(k_HerdingLine));
		commands.push_back(KillTheBoy(k_BoysSpot, k_BoyPickRadius, k_AfterTheIntroSeconds));
		// the herding line was waited for before the kill and has gone since
		HerdTheFish(commands, k_FishTaps, false);
		commands.push_back(WaitTask("FishPuzzle", TaskWait::Finished));
		commands.push_back(LogGlobals("", k_FishScripts, 1.0f));
		all.push_back({
		    .id = "quest.the_fish_puzzle_boy_dead_playing",
		    .name = "Land 4 quest run: The Fish Puzzle, the boy dead while the puzzle is played",
		    .facet = Facet::Story,
		    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/FishPuzzle on Land4.txt). "
		                   "As quest.the_fish_puzzle up to the boy's herding line; once the introduction's cinema is over "
		                   "the boy is killed (a shortcut for the player's blow), then the fish are herded into the net as "
		                   "in quest.the_fish_puzzle, and the timeline waits for the quest to end.",
		    .expected = "The good advisor's line on the boy's death, once; the puzzle is still played to its end and the "
		                "record is done, then the scroll goes and the evil advisor's three lines on the dead boy follow. "
		                "No turtle.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	// on his walk home, with the turtle's swap taken
	{
		auto commands = FishPuzzleStart();
		PressScroll(commands, k_FishScroll);
		HerdTheFish(commands, k_FishTaps);
		// the swap's script starts with the reward's dialogue; once the taps are over the boy is on his way
		commands.push_back(WaitTask("SwapCreatures"));
		commands.push_back(KillTheBoy(k_BoyWalkingHome, k_WalkingBoyPickRadius, 0.5f));
		SwapForTheTurtle(commands);
		commands.push_back(WaitTask("FishPuzzle", TaskWait::Finished));
		commands.push_back(LogGlobals("", k_FishScripts, 20.0f));
		all.push_back({
		    .id = "quest.the_fish_puzzle_boy_dead_walking",
		    .name = "Land 4 quest run: The Fish Puzzle, the boy dead on his walk home, the turtle swapped",
		    .facet = Facet::Story,
		    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/FishPuzzle on Land4.txt). "
		                   "As quest.the_fish_puzzle to the minute of taps; once the taps are over and the turtle's "
		                   "swap has begun, the boy, walking home, is killed (a shortcut for the player's blow). Then the "
		                   "turtle's scroll is pressed with the creature far away, the "
		                   "creature is put by the turtle and the scroll pressed again, the turtle is pressed when the "
		                   "good advisor asks, and the timeline waits for the quest to end.",
		    .expected = "Once the advisors' lines are over, the evil advisor's line on the boy killed on his way home, "
		                "once, and the quest ends. The turtle's swap runs on its own: the evil advisor's line when its "
		                "scroll is seen, his line when it is pressed from afar, the creature walked over to it and the "
		                "good advisor's question, then the swap's scene, the evil advisor's line after it and the old "
		                "creature left in the turtle's place.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
}

/// The camera high over the boy's spot, looking steeply down, so that he stands at the middle of the screen, and the
/// player's influence round it
constexpr float k_TakeEyeAbove = 40.0f;
constexpr float k_TakeEyeBack = 20.0f;
constexpr float k_BoyInfluenceRadius = 60.0f;

void AddFishPuzzleBoyThrown(std::vector<Scenario>& all)
{
	auto commands = FishPuzzleStart();
	// [shortcut] the player's influence round the boy, as owning the village would give it
	commands.push_back(
	    {.kind = Kind::ScriptInfluence, .delaySeconds = 0.5f, .point = k_BoysSpot, .radius = k_BoyInfluenceRadius});
	commands.push_back(CameraOn(k_BoysSpot, k_TakeEyeAbove, k_TakeEyeBack, 0.0f));
	commands.push_back(WaitTask("FishPuzzle", TaskWait::Finished));
	commands.push_back(LogGlobals("", k_FishScripts, 1.0f));
	all.push_back({
	    .id = "realinput.the_fish_puzzle_boy_thrown_first",
	    .name = "Land 4 quest run: The Fish Puzzle, the boy thrown to his death by hand before the scroll is pressed",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/FishPuzzle on Land4.txt), with the "
	                   "player's mouse from a recording: the timeline only clicks the texts, gives the player influence "
	                   "round the boy and puts the camera high over his spot. The take with the hand and the throw are the "
	                   "recording's; the scroll is never pressed.",
	    .expected = "The boy thrown dies, and the quest ends quietly: the scroll goes and nothing is said.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace
