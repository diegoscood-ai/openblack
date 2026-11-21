/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Quest runs that take a quest's other branches: the same start as the quest's own timeline, up to its choice, then
// the other choice. On Land 4: The Treacherous Path with the blind woman dying four times, or three times and then
// arriving with no potion left; The Heartbroken Man with Keiko killed, or Adam killed; The Totem Puzzle failed twice and
// then solved; The Treacherous Path with Morg fighting the creature, or two deaths then the arrival with a
// potion; Leave Through the Vortex opened by its rumbles; The Defending Ogres' raid and its gremlins' deaths; on
// Land 5, Nemesis's shielded village with a worshipper led away and the creature by a stone; on Land 3, the man who
// wants to be thrown, called on, thrown, set alight and thrown into a pond. The points are map coordinates, taken from
// the places the challenge scripts name.

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
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;
/// The belief in the player that wins a village: more than its belief in the neutral player (0.5)
constexpr float k_WinningBelief = 2.0f;
constexpr float k_VillagePickRadius = 50.0f;

/// The Treacherous Path's silver scroll by the blind woman's hut, raised 10; the Aztec village, which must be the
/// player's before it comes; the wolf pack on her way, picked within this of its spot
constexpr glm::vec2 k_BlindWomanScroll {3703.723f, 3105.303f};
constexpr float k_BlindWomanScrollRaised = 10.0f;
constexpr glm::vec2 k_AztecVillage {3385.63f, 3100.74f};
constexpr glm::vec2 k_WolfPack {3509.908f, 3280.037f};
constexpr float k_WolfPickRadius = 80.0f;
constexpr uint32_t k_WolfSubtype = 2;
constexpr size_t k_PackWolves = 8;
/// The blind woman, picked by her kind of villager near the start of the road she walks: each journey walks her round
/// her fence to it, starts her along the road and only then lets her be hurt. The first journey lets her be hurt about
/// 12 seconds after the wolves are dealt with; each later one as it starts her on the road
constexpr glm::vec2 k_RoadStart {3692.7063f, 3109.028f};
constexpr uint32_t k_BlindWomanSubtype = 14;
constexpr float k_BlindWomanPickRadius = 30.0f;
constexpr float k_FirstHurtSeconds = 20.0f;
constexpr float k_LaterHurtSeconds = 6.0f;
/// How many times she has to die for each ending: three uses up her potions, a fourth fails the quest
constexpr size_t k_PotionDeaths = 3;
/// Seconds after the last line of an ending's scene before the camera is taken off the brother's house, so that the
/// script's wait for the brother (and the woman) to be out of view ends
constexpr float k_LookAwaySeconds = 15.0f;

/// The Heartbroken Man's scroll, by the woman's shack
constexpr glm::vec2 k_NomadScroll {2930.707f, 2875.43f};
/// Keiko, picked by her kind of villager at the edge of her village where she is made, as her slow walk about it
/// begins; Adam, picked by his kind where he is made in his house on the mountain
constexpr glm::vec2 k_KeikoStart {3346.66f, 3094.67f};
constexpr uint32_t k_KeikoSubtype = 14;
constexpr float k_KeikoPickRadius = 3.0f;
constexpr glm::vec2 k_AdamHouse {3051.88f, 2664.84f};
constexpr uint32_t k_AdamSubtype = 75;
constexpr float k_AdamPickRadius = 10.0f;

/// The Totem Puzzle's scroll, just north of the bell towers
constexpr glm::vec2 k_PuzzleScroll {2542.824f, 3109.667f};
/// The Japanese village, where the land's setup finds it
constexpr glm::vec2 k_JapaneseVillage {2590.363f, 3196.893f};
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
/// A tower the first round's sequence doesn't start with, clicked first to fail an attempt; and how many attempts fail
constexpr uint8_t k_WrongTower = 4;
constexpr size_t k_FailedAttempts = 2;

constexpr std::string_view k_PathGlobals = "TimesDied BlindWomanStillGoingStrong";
constexpr std::string_view k_PathScripts = "BlindWomanMain BlindWomanJourney BlindWomanKilled GremlinSniper";
constexpr std::string_view k_NomadGlobals = "StartLand4Nomad";
constexpr std::string_view k_NomadScripts = "Land4Nomad ChallengeHighlightNotify WomanWalkingAroundTown";
constexpr std::string_view k_PuzzleGlobals = "TowerClicked KeepQuiet StartLand4Nomad";
constexpr std::string_view k_PuzzleScripts = "Land4Puzzle ChallengeHighlightNotify SeeWhichTowerClickedOn";

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

[[nodiscard]] ThingNear BlindWoman()
{
	return {.type = script::ObjectType::Villager,
	        .subtype = k_BlindWomanSubtype,
	        .point = k_RoadStart,
	        .radius = k_BlindWomanPickRadius};
}

/// The Treacherous Path's timeline up to her first journey: the village won, the scroll pressed on, her introduction
/// played, and the wolf pack on her way killed once she sets off
[[nodiscard]] std::vector<Command> TreacherousPathStart()
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
	return commands;
}

/// [shortcut] The blind woman killed on the road, as the player's blows would kill her: her health brought to nothing
/// once the journey lets her be hurt. After the first, each waits for her death's scene to have made her again and her
/// next journey to have started her on the road
void KillBlindWoman(std::vector<Command>& commands, size_t death)
{
	if (death > 0)
	{
		commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 2.0f));
		commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 0.0f));
	}
	commands.push_back({.kind = Kind::ScriptSetHealth,
	                    .delaySeconds = death == 0 ? k_FirstHurtSeconds : k_LaterHurtSeconds,
	                    .amount = 0.0f,
	                    .thing = BlindWoman()});
}

/// The camera taken back to the woman's hut, far from the brother's house, some seconds after an ending's last line
[[nodiscard]] Command LookAwayFromTheBrother()
{
	auto away = CameraOn(k_BlindWomanScroll, k_EyeAbove + k_BlindWomanScrollRaised, k_EyeBack, k_BlindWomanScrollRaised);
	away.delaySeconds = k_LookAwaySeconds;
	return away;
}

void AddTreacherousPathFourDeaths(std::vector<Scenario>& all)
{
	auto commands = TreacherousPathStart();
	for (size_t death = 0; death < k_PotionDeaths; ++death)
	{
		KillBlindWoman(commands, death);
	}
	// [shortcut] the fourth time she is taken out of the world, as the player's hand or creature could leave her
	commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 2.0f));
	commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 0.0f));
	commands.push_back({.kind = Kind::ScriptDelete, .delaySeconds = k_LaterHurtSeconds, .thing = BlindWoman()});
	// the failure's scene: the evil advisor's two lines, the brother crawling out to die and the advisors' two
	commands.push_back(WaitText(5323));
	commands.push_back(LookAwayFromTheBrother());
	commands.push_back(WaitTask("BlindWomanMain", TaskWait::Finished));
	commands.push_back(LogGlobals(k_PathGlobals, k_PathScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_treacherous_path_four_deaths",
	    .name = "Land 4 quest run: The Treacherous Path, the woman dying four times",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/BlindWomanMain on Land4.txt). "
	                   "As quest.the_treacherous_path up to the wolves; then it kills the woman on the road three times, "
	                   "each once her journey lets her be hurt, and the fourth time takes her out of the world. Once the "
	                   "failure's last line is read it takes the camera back to her hut and waits for the quest to end.",
	    .expected = "Each of the first three deaths plays its scene at her hut with its lines, and she sets off again. The "
	                "fourth ends the quest in failure: the evil advisor's lines over where she was, the brother crawling "
	                "out of his house to die, the log at 1, the advisors' two lines; his body is removed once out of view.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddTreacherousPathNoPotion(std::vector<Scenario>& all)
{
	auto commands = TreacherousPathStart();
	for (size_t death = 0; death < k_PotionDeaths; ++death)
	{
		KillBlindWoman(commands, death);
	}
	// then she walks the whole road again and reaches her brother with no potion left
	commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 1.0f));
	commands.push_back(WaitText(5319));
	commands.push_back(LookAwayFromTheBrother());
	commands.push_back(WaitTask("BlindWomanMain", TaskWait::Finished));
	commands.push_back(LogGlobals(k_PathGlobals, k_PathScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_treacherous_path_no_potion",
	    .name = "Land 4 quest run: The Treacherous Path, three deaths then the arrival with no potion",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/BlindWomanMain on Land4.txt). "
	                   "As quest.the_treacherous_path up to the wolves; then it kills the woman on the road three times, "
	                   "each once her journey lets her be hurt, and lets her walk the road a fourth time. Once the "
	                   "arrival's last line is read it takes the camera back to her hut and waits for the quest to end.",
	    .expected = "Each death plays its scene at her hut with its lines, and she sets off again. She reaches the "
	                "brother's house with no potion: he crawls out and dies, her two lines and the advisors' four, the log "
	                "at 1; she walks into the house and joins the village, and his body is removed once out of view.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The Heartbroken Man's timeline up to Keiko's walk: the scroll pressed on and the introduction played
[[nodiscard]] std::vector<Command> HeartbrokenManStart()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Land4Nomad"),
	    LogEvery(k_NomadGlobals, k_NomadScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_NomadScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_NomadScroll);
	// the introduction plays; then Keiko walks about her town
	commands.push_back(WaitTask("WomanWalkingAroundTown"));
	return commands;
}

void AddHeartbrokenManKilled(std::vector<Scenario>& all, bool keiko)
{
	auto commands = HeartbrokenManStart();
	// [shortcut] Keiko, or Adam, killed as the player's blows would kill them: the health brought to nothing
	commands.push_back({.kind = Kind::ScriptSetHealth,
	                    .delaySeconds = 1.0f,
	                    .amount = 0.0f,
	                    .thing = keiko ? ThingNear {.type = script::ObjectType::Villager,
	                                                .subtype = k_KeikoSubtype,
	                                                .point = k_KeikoStart,
	                                                .radius = k_KeikoPickRadius}
	                                   : ThingNear {.type = script::ObjectType::Villager,
	                                                .subtype = k_AdamSubtype,
	                                                .point = k_AdamHouse,
	                                                .radius = k_AdamPickRadius}});
	commands.push_back(WaitTask("Land4Nomad", TaskWait::Finished));
	commands.push_back(LogGlobals(k_NomadGlobals, k_NomadScripts, 1.0f));
	all.push_back({
	    .id = keiko ? "quest.the_heartbroken_man_keiko_killed" : "quest.the_heartbroken_man_adam_killed",
	    .name = keiko ? "Land 4 quest run: The Heartbroken Man, Keiko killed"
	                  : "Land 4 quest run: The Heartbroken Man, Adam killed",
	    .facet = Facet::Story,
	    .description = keiko ? "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/StartLand4Nomad=1/"
	                           "Land4Nomad on Land4.txt). As quest.the_heartbroken_man up to Keiko's walk; then it kills "
	                           "Keiko as her walk begins, and waits for the quest to end."
	                         : "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/StartLand4Nomad=1/"
	                           "Land4Nomad on Land4.txt). As quest.the_heartbroken_man up to Keiko's walk; then it kills "
	                           "Adam in his house on the mountain, and waits for the quest to end.",
	    .expected = keiko ? "Keiko's walk stops and the screen fades: Adam by his door with his three lines, the log at "
	                        "-0.8, the fade to black; then the stone's scene with the evil advisor's two lines, and the "
	                        "quest ends."
	                      : "Keiko's walk stops and the screen fades: the woman by her shack with her lines and the evil "
	                        "advisor's, the log at 0, an influence of 200 round her and her release; then the stone's "
	                        "scene with the evil advisor's two lines, and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The player's influence given round Keiko's walk or Adam's house, as owning her village or his would give it: a ring
/// over each, wide enough for the hand to take them there
constexpr glm::vec2 k_KeikoWalkMiddle {3332.0f, 3086.0f};
constexpr float k_KeikoInfluenceRadius = 60.0f;
constexpr float k_AdamInfluenceRadius = 40.0f;
/// The take's camera: high over the first spot of Keiko's walk, where she sits from about 14 to 36 seconds after her
/// walk begins, or over Adam where he is made, looking steeply down, so that no house stands in front; a throw up the
/// screen sends them north, off her village's plateau or down his mountain
constexpr glm::vec2 k_KeikoFirstSpot {3343.5378f, 3080.7058f};
constexpr float k_TakeEyeAbove = 40.0f;
constexpr float k_TakeEyeBack = 20.0f;
/// Adam's first throw leaves him hurt on the mountain's shoulder north of his house, where he lies still; the second
/// take is there, with the player's influence given there too, this long after the first take's camera
constexpr glm::vec2 k_AdamFirstLanding {3051.3113f, 2780.0247f};
constexpr float k_SecondTakeSeconds = 25.0f;

/// The Heartbroken Man with Keiko, or Adam, killed for the real input path (tools/checks/realinput/
/// heartbroken_man_killed.txt, replayed through tools/checks/real_input_tap.sh): no pointer commands, so the replayed
/// mouse alone drives the hand. The timeline clicks the texts and places the camera: over the scroll, then, once Keiko's
/// walk begins, high over where the one to kill will be, and for Adam then over where the first throw leaves him. The
/// scroll's press, each take with the hand and each throw are the recording's
void AddHeartbrokenManKilledByHand(std::vector<Scenario>& all, bool keiko)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Land4Nomad"),
	    LogEvery(k_NomadGlobals, k_NomadScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_NomadScroll, k_EyeAbove, k_EyeBack, 0.0f),
	    // the introduction plays; then Keiko walks about her town
	    WaitTask("WomanWalkingAroundTown"),
	    // [shortcut] the player's influence there, as owning the village would give it
	    {.kind = Kind::ScriptInfluence,
	     .delaySeconds = 0.5f,
	     .point = keiko ? k_KeikoWalkMiddle : k_AdamHouse,
	     .radius = keiko ? k_KeikoInfluenceRadius : k_AdamInfluenceRadius},
	    CameraOn(keiko ? k_KeikoFirstSpot : k_AdamHouse, k_TakeEyeAbove, k_TakeEyeBack, 0.0f),
	};
	if (!keiko)
	{
		// [shortcut] the player's influence where the first throw left him, as owning the land there would give it
		commands.push_back({.kind = Kind::ScriptInfluence,
		                    .delaySeconds = k_SecondTakeSeconds,
		                    .point = k_AdamFirstLanding,
		                    .radius = k_AdamInfluenceRadius});
		commands.push_back(CameraOn(k_AdamFirstLanding, k_TakeEyeAbove, k_TakeEyeBack, 0.0f));
	}
	commands.push_back(WaitTask("Land4Nomad", TaskWait::Finished));
	commands.push_back(LogGlobals(k_NomadGlobals, k_NomadScripts, 1.0f));
	all.push_back({
	    .id = keiko ? "realinput.the_heartbroken_man_keiko_killed" : "realinput.the_heartbroken_man_adam_killed",
	    .name = keiko ? "Land 4 quest run: The Heartbroken Man, Keiko thrown to her death by hand"
	                  : "Land 4 quest run: The Heartbroken Man, Adam thrown to his death by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/StartLand4Nomad=1/Land4Nomad on "
	                   "Land4.txt), with the player's mouse from a recording: the timeline only clicks the texts and "
	                   "places the camera: over the scroll; once Keiko's walk begins it gives the player influence there "
	                   "and puts the camera high over the first spot of her walk, or over Adam and, 25 seconds later, "
	                   "with influence there too, over where his first throw leaves him. The scroll's press, each take "
	                   "with the hand and each throw are the recording's.",
	    .expected = keiko ? "Keiko thrown dies: her walk stops and the evil ending plays (Adam's three lines, the log at "
	                        "-0.8), then the stone's scene, and the quest ends."
	                      : "Adam thrown dies: the neighbour's ending plays (her lines and the evil advisor's, the log "
	                        "at 0), then the stone's scene, and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
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

void AddTotemPuzzleTwoFailures(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Land4Puzzle"),
	    LogEvery(k_PuzzleGlobals, k_PuzzleScripts),
	    // [shortcut] the Japanese village takes more belief in the player than in anyone else, as winning it gives,
	    // and its next fold hands it over
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_PuzzleScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_PuzzleScroll);
	// the elder's introduction plays; once it is over the first round's sequence plays, and the camera is put over the
	// towers while it does
	commands.push_back(WaitText(5380));
	commands.push_back(WaitGlobal("KeepQuiet", 1.0f));
	commands.push_back(CameraOn(k_TowersMiddle, k_TowersEyeAbove, k_TowersEyeBack, 0.0f));
	for (size_t attempt = 0; attempt < k_FailedAttempts; ++attempt)
	{
		// a wrong tower clicked first ends the attempt; its scene plays (the elder's the first time, the evil
		// advisor's after), the advisors may speak again, and a new scroll comes at the same spot
		commands.push_back(WaitTask("SeeWhichTowerClickedOn"));
		ClickTower(commands, k_WrongTower);
		commands.push_back(WaitGlobal("KeepQuiet", 0.0f));
		commands.push_back(WaitTask("ChallengeHighlightNotify"));
		commands.push_back(CameraOn(k_PuzzleScroll, k_EyeAbove, k_EyeBack, 0.0f));
		PressScroll(commands, k_PuzzleScroll);
		// the short scene of a later attempt, then the first round's sequence again
		commands.push_back(WaitGlobal("KeepQuiet", 1.0f));
		commands.push_back(CameraOn(k_TowersMiddle, k_TowersEyeAbove, k_TowersEyeBack, 0.0f));
	}
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
	    .id = "quest.the_totem_puzzle_two_failures",
	    .name = "Land 4 quest run: The Totem Puzzle, two failed attempts then solved",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/Land4Puzzle on Land4.txt). As "
	                   "quest.the_totem_puzzle up to the first round; then twice it clicks a wrong tower first, lets the "
	                   "failure's scene play and presses on the new scroll; then it answers the four rounds as "
	                   "quest.the_totem_puzzle does and waits for the quest to end.",
	    .expected = "Each wrong tower ends the attempt with the bad-stone sound: the first with the elder's scene and his "
	                "three lines, the second with the evil advisor's line. A new scroll comes at the same spot each time, "
	                "and its press plays the short scene with the evil advisor's line before the sequence. The third "
	                "attempt is answered and the quest ends as on quest.the_totem_puzzle.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// Morg's ridge, where he stands; the creature put down beside him, within the 50 of him that makes him fight it; the
/// camera held over him, within the 75 that makes him mutter, while he does
constexpr glm::vec2 k_MorgRidge {3209.364f, 3277.383f};
constexpr glm::vec2 k_BesideMorg {3205.0f, 3280.0f};
constexpr float k_MorgWatchSeconds = 30.0f;
/// Left alone it walks off home at once, and his script looks for it only every 10 to 35 seconds, between his
/// throws and his walks back to the ridge: it is put back beside him every second for a minute
constexpr size_t k_PutsBesideMorg = 60;
constexpr float k_SecondsBetweenPuts = 1.0f;
/// The fight's health of Morg set to none this many times, this far apart, while the creature fights by itself
constexpr size_t k_MorgFightTurns = 30;
constexpr float k_MorgFightSeconds = 10.0f;
constexpr size_t k_FightHealthProperty = 33;
/// How many times she dies before the arrival that still has a potion left for the brother
constexpr size_t k_DeathsWithAPotionLeft = 2;

/// Leave Through the Vortex's hidden vortex, and where its scroll comes, raised 20 above it; the camera held 200 back
/// and 100 above it, within the 500 that hastens the rumbles and near enough that each is strong (above 0.8), but not
/// within the 100 that opens it, so that the rumbles open it by themselves
constexpr glm::vec2 k_Vortex {2060.18f, 2591.698f};
constexpr float k_VortexScrollRaised = 20.0f;
constexpr float k_RumblesAbove = 100.0f;
constexpr float k_RumblesBack = 200.0f;
constexpr std::string_view k_VortexGlobals = "VortexOpen LandNumber";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL4 ChallengeHighlightNotify";

void AddTreacherousPathMorgFight(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("BlindWomanMain"),
	    LogEvery(k_PathGlobals, k_PathScripts),
	    // the camera over Morg's ridge, as a player looking about would, for his mutter
	    CameraOn(k_MorgRidge, k_EyeAbove, k_EyeBack, 0.0f),
	};
	// [shortcut] the player's creature put down beside him, as the leash would lead it there and hold it
	for (size_t put = 0; put < k_PutsBesideMorg; ++put)
	{
		commands.push_back({.kind = Kind::MoveThing,
		                    .delaySeconds = put == 0 ? k_MorgWatchSeconds : k_SecondsBetweenPuts,
		                    .point = k_BesideMorg});
	}
	// once his script has made them fight, the creature fights by itself, as its switch in a fight lets it; [shortcut]
	// Morg's fight health is set to none again and again, as the player's blows would wear it down
	for (size_t turn = 0; turn < k_MorgFightTurns; ++turn)
	{
		commands.push_back({.kind = Kind::FightAuto, .delaySeconds = k_MorgFightSeconds, .value = 1});
		commands.push_back(
		    {.kind = Kind::ScriptOpponentProperty, .delaySeconds = 0.5f, .value = k_FightHealthProperty, .amount = 0.0f});
	}
	commands.push_back(LogGlobals(k_PathGlobals, k_PathScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_treacherous_path_morg_fight",
	    .name = "Land 4 quest run: The Treacherous Path, Morg against the creature",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/BlindWomanMain on Land4.txt). "
	                   "The texts that wait for a click are clicked on. It holds the camera over Morg's ridge, puts the "
	                   "player's creature beside him every second for a minute from 30 seconds on, and for five minutes lets "
	                   "it fight by itself while "
	                   "Morg's fight health is set to none every 10 seconds; then it logs.",
	    .expected = "Morg mutters while the camera is near. With the creature within 50 he grows to its size, the two "
	                "are made to face and fight; the script waits for the fight to start and to end. Whoever wins, the "
	                "creature is released: Morg driven out if his fight health ends below the creature's, else back on "
	                "his ridge to heal.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddTreacherousPathTwoDeaths(std::vector<Scenario>& all)
{
	auto commands = TreacherousPathStart();
	for (size_t death = 0; death < k_DeathsWithAPotionLeft; ++death)
	{
		KillBlindWoman(commands, death);
	}
	// then she walks the whole road again and reaches her brother with a potion left
	commands.push_back(WaitGlobal("BlindWomanStillGoingStrong", 1.0f));
	commands.push_back(WaitTask("BlindWomanMain", TaskWait::Finished));
	commands.push_back(LogGlobals(k_PathGlobals, k_PathScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_treacherous_path_two_deaths",
	    .name = "Land 4 quest run: The Treacherous Path, two deaths then the arrival with a potion",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/BlindWomanMain on Land4.txt). "
	                   "As quest.the_treacherous_path up to the wolves; then it kills the woman on the road twice, each "
	                   "once her journey lets her be hurt, lets her walk the road a third time and waits for the quest to "
	                   "end.",
	    .expected = "Each death plays its scene at her hut with its lines, and she sets off again. She reaches the "
	                "brother's house with a potion left: he limps out, she heals him, the log at 1, the hug, the wolf "
	                "and its scroll, and both walk into the house.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddVortexRumbles(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("LeaveThroughVortexL4"),
	    LogEvery(k_VortexGlobals, k_VortexScripts),
	    CameraOn(k_Vortex, k_RumblesAbove, k_RumblesBack, 0.0f),
	    // the rumbles come a second sooner each time until five at 3 seconds apart open the vortex; its scene plays and
	    // its scroll comes
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Vortex, k_EyeAbove + k_VortexScrollRaised, k_EyeBack, k_VortexScrollRaised),
	};
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitTask("LeaveThroughVortexL4", TaskWait::Finished));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_4_rumbles",
	    .name = "Land 4 quest run: Leave through the vortex, opened by its rumbles",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/LeaveThroughVortexL4 on Land4.txt). The texts "
	                   "that wait for a click are clicked on. It holds the camera about 220 from the hidden vortex, never "
	                   "within the 100 that opens it, until the rumbles have opened it; then it presses on the scroll and "
	                   "waits for the quest to end.",
	    .expected = "The rumbles start 30 seconds apart and come a second sooner each time, down to 3; the advisors "
	                "speak at 25 or 20, 15 or 10 and 5 seconds apart; five rumbles at 3 seconds apart open the vortex "
	                "in its cut scene, the scroll appears, and its press ends the quest.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The Defending Ogres: the gates by the home village, which the camera looks north over while the man is awaited (his
/// spot by the temple behind it, and far from the Japanese village and Sleg's lair, whose nearness counts as exploring
/// too early); the man's gold scroll by the temple
constexpr glm::vec2 k_OgreGates {2164.2515f, 2889.9802f};
constexpr glm::vec2 k_OgreManScroll {2390.8188f, 2572.488f};
/// The man's story's lines that wait for a click, in order, the last before he sets off home and the gremlins are made
constexpr std::array<uint32_t, 8> k_OgreManLines {5347, 5352, 5358, 5363, 5366, 5370, 5372, 5375};
/// Where the gremlins idle between raids, by Sleg's lair; each is picked within this of it (Sleg stands about 57
/// away), once the raid's carriers have thrown their villagers to Sleg and all are back there
constexpr glm::vec2 k_GremlinChillSpot {1680.4583f, 2841.1338f};
constexpr float k_GremlinPickRadius = 35.0f;
constexpr float k_BackByTheLairSeconds = 150.0f;
constexpr size_t k_Gremlins = 4;
/// A gremlin's health is brought to this, below the 0.3 that kills it
constexpr float k_GremlinHurt = 0.2f;
/// The creature's home, which the camera watches during the raid
constexpr glm::vec2 k_CreatureHomeL4 {2235.0f, 2695.0f};
constexpr std::string_view k_OgreGlobals = "ManHasExplainedAboutNemesis OgreInFightSequence LaunchGremlins "
                                           "FireMeteorWaveActive GremlinsKilled IllegalExplore";
constexpr std::string_view k_OgreScripts = "Land4Ogre OgreFamilyControl ControlGremlin GremlinsAllGone "
                                           "GremlinAttackWarning ManExplainsAboutNemesis";

/// Nemesis's shielded village: within 300 of the shield's centre, where the creature's coming starts the shield's
/// scene; the scene's last line; the first worshipper and his stone's other worshippers, each made by his stone
constexpr glm::vec2 k_NearTheShieldL5 {1700.0f, 2100.0f};
constexpr uint32_t k_ShieldSceneLastLineL5 = 5502;
struct ShieldWorshipper
{
	uint32_t subtype;
	glm::vec2 at;
};
constexpr std::array<ShieldWorshipper, 3> k_ShieldWorshippers {{
    {.subtype = 36, .at = {2089.572f, 2068.963f}},
    {.subtype = 41, .at = {1568.99f, 2103.337f}},
    {.subtype = 35, .at = {1393.386f, 2998.66f}},
}};
constexpr float k_ShieldWorshipperPickRadius = 10.0f;
/// The first worshipper led 10 away from his spot, and back to it
constexpr glm::vec2 k_AwayFromHisStone {2099.572f, 2068.963f};
/// Within 100 of the second stone, where a working stone strikes the creature with lightning
constexpr glm::vec2 k_BySecondStone {1571.99f, 2160.0f};
constexpr size_t k_PutsByTheStone = 4;
constexpr std::string_view k_ShieldGlobalsL5 = "ShieldFinished NumberOfActiveStones";
constexpr std::string_view k_ShieldScriptsL5 = "ThrowThroughShield LookAfterShieldStone NemesisDefendHisLastTown";

void AddDefendingOgresRaids(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    LogEvery(k_OgreGlobals, k_OgreScripts),
	    // the camera over the gates, looking north, so that the man comes and nobody explores too early
	    CameraOn(k_OgreGates, 40.0f, 30.0f, 5.0f),
	    WaitText(5293),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_OgreManScroll, 25.0f, 30.0f, 3.0f),
	};
	PressScroll(commands, k_OgreManScroll);
	// the man's story; he sets off home and the gremlins are made
	for (const auto line : k_OgreManLines)
	{
		commands.push_back(WaitText(line));
	}
	commands.push_back(WaitTask("ManExplainsAboutNemesis", TaskWait::Finished));
	// the creature is never brought to Sleg, so no fight starts: once ten minutes have passed since the gremlins'
	// script began and no fireball wave is falling, a raid on the home village starts, and ends 150 seconds later
	commands.push_back(WaitGlobal("LaunchGremlins", 1.0f));
	commands.push_back(CameraOn(k_CreatureHomeL4, 60.0f, 60.0f, 0.0f));
	commands.push_back(WaitGlobal("LaunchGremlins", 0.0f));
	// [shortcut] each gremlin hurt below 0.3, as the player's blows would hurt it, once they are back by the lair
	for (size_t gremlin = 0; gremlin < k_Gremlins; ++gremlin)
	{
		commands.push_back(
		    {.kind = Kind::ScriptSetHealth,
		     .delaySeconds = gremlin == 0 ? k_BackByTheLairSeconds : 3.0f,
		     .amount = k_GremlinHurt,
		     .thing =
		         ThingNear {.type = script::ObjectType::Creature, .point = k_GremlinChillSpot, .radius = k_GremlinPickRadius}});
	}
	commands.push_back(LogGlobals(k_OgreGlobals, k_OgreScripts, 20.0f));
	all.push_back({
	    .id = "quest.the_defending_ogres_raids",
	    .name = "Land 4 quest run: The Defending Ogres, the gremlins' raid and their deaths",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(1)/BuildUpTown&/Land4Ogre&/"
	                   "KeepCreatureAwayFromOgre&/ManExplainsAboutNemesis on Land4.txt). As quest.the_defending_ogres up "
	                   "to the man's story; then, never bringing the creature to Sleg, it waits for a raid on the home "
	                   "village to start and end, then once the gremlins are back by the lair hurts each below 0.3.",
	    .expected = "Ten minutes after the gremlins' script began, with no fireball wave falling, the four gremlins walk "
	                "by the two road points to the home village, the good advisor warns of the attack, each picks up a "
	                "villager, carries him to Sleg and throws him, and Sleg eats one; after 150 seconds the raid "
	                "ends. Each hurt gremlin turns into a dead villager in five sparkles with the good "
	                "advisor's line, and once all four are dead the good advisor says so.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddShieldStonesAndCreature(std::vector<Scenario>& all)
{
	const auto worshipper = [](const ShieldWorshipper& who, glm::vec2 at) {
		return ThingNear {
		    .type = script::ObjectType::Villager, .subtype = who.subtype, .point = at, .radius = k_ShieldWorshipperPickRadius};
	};
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ThrowThroughShield"),
	    LogEvery(k_ShieldGlobalsL5, k_ShieldScriptsL5),
	    // [shortcut] the creature within 300 of the shield, as the leash would lead it there, which starts its scene
	    {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_NearTheShieldL5},
	    {.kind = Kind::WaitText, .waitUntilFree = true, .value = k_ShieldSceneLastLineL5, .read = true, .voice = true},
	};
	// [shortcut] the creature put within 100 of the second stone, as the leash would lead it there, for its lightning
	for (size_t put = 0; put < k_PutsByTheStone; ++put)
	{
		commands.push_back({.kind = Kind::MoveThing, .delaySeconds = put == 0 ? 3.0f : 1.0f, .point = k_BySecondStone});
	}
	// [shortcut] the first worshipper led away from his spot by his stone, as the hand would carry him, and back:
	// his stone's chant and beam stop, then start again
	const auto& first = k_ShieldWorshippers.front();
	commands.push_back(
	    {.kind = Kind::MoveThing, .delaySeconds = 6.0f, .point = k_AwayFromHisStone, .thing = worshipper(first, first.at)});
	commands.push_back(
	    {.kind = Kind::MoveThing, .delaySeconds = 8.0f, .point = first.at, .thing = worshipper(first, k_AwayFromHisStone)});
	// [shortcut] then each worshipper killed, as the player's blow would kill him
	for (const auto& who : k_ShieldWorshippers)
	{
		commands.push_back({.kind = Kind::ScriptSetHealth,
		                    .delaySeconds = &who == &first ? 8.0f : 2.0f,
		                    .amount = 0.0f,
		                    .thing = worshipper(who, who.at)});
	}
	commands.push_back(WaitTask("ThrowThroughShield", TaskWait::Finished));
	commands.push_back(LogGlobals(k_ShieldGlobalsL5, k_ShieldScriptsL5, 1.0f));
	all.push_back({
	    .id = "quest.nemesis_shielded_village_stones",
	    .name = "Land 5 quest run: Nemesis's shielded village, a worshipper led away and the creature struck",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/ThrowThroughShield&/"
	                   "NemesisDefendHisLastTown on Land5.txt). As quest.nemesis_shielded_village up to the shield's "
	                   "scene; then it puts the creature within 100 of the second stone, leads the first worshipper 10 "
	                   "away from his spot and back, and kills the three worshippers.",
	    .expected = "Within 100 of a working stone the creature is struck by lightning from it every 3 seconds. With "
	                "the first worshipper away his stone's chant and beam stop and the shield shrinks; back in place they "
	                "start again. Each worshipper killed takes his ray off for good, and with none left the shield goes.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// Land 3's man who wants to be thrown: his spot by the campfire, picked by his kind of villager; a spot on the land
/// 30 north of it for his first flight, and the pond 60 to its south for his second
constexpr glm::vec2 k_NutterSpot {2796.2798f, 3146.4014f};
constexpr uint32_t k_NutterSubtype = 2;
constexpr float k_NutterPickRadius = 40.0f;
constexpr glm::vec2 k_NutterLandThrow {2796.0f, 3176.0f};
constexpr glm::vec2 k_NutterPondThrow {2780.0f, 3082.0f};
/// The camera 40 south of him and 25 up, within the 100 his calls need; and a view of the land to his north, which
/// leaves him out of sight
constexpr glm::vec3 k_NutterEye {2796.0f, 25.0f, 3106.0f};
constexpr glm::vec3 k_NutterLook {2796.0f, 2.0f, 3146.0f};
constexpr glm::vec3 k_AwayEye {2796.0f, 40.0f, 3200.0f};
constexpr glm::vec3 k_AwayLook {2796.0f, 0.0f, 3300.0f};
/// Seconds the camera stays on him for his calls, which come every 20 seconds while it is near, and for each flight
/// with its slowed time, his landing and his walk back
constexpr float k_NutterCallsSeconds = 45.0f;
constexpr float k_NutterFlightSeconds = 40.0f;
/// How long the by-hand run's timeline waits, for the recording's grab and throw and his walk back
constexpr float k_NutterByHandSeconds = 240.0f;
constexpr std::string_view k_NutterGlobals = "NutterOnFire NutterDrenched";
constexpr std::string_view k_NutterScripts = "ThrowBlokeMain MakeNutterDo EndOfFlyingNutter CreateNewNutter";

[[nodiscard]] ThingNear Nutter()
{
	return {
	    .type = script::ObjectType::Villager, .subtype = k_NutterSubtype, .point = k_NutterSpot, .radius = k_NutterPickRadius};
}

/// The man taken by the hand and thrown at a point of the land
void ThrowNutter(std::vector<Command>& commands, glm::vec2 at)
{
	// the camera put back on his spot first: his flight's scene leaves it following him
	commands.push_back({.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_NutterEye, .look = k_NutterLook});
	// [shortcut] the hand takes him, as the player's grab would, and throws him through the testbed's pointer
	commands.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = Nutter()});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 3.0f, .point = at});
	commands.push_back(LogGlobals(k_NutterGlobals, k_NutterScripts, k_NutterFlightSeconds));
}

void AddThrowBloke(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_NutterEye, .look = k_NutterLook},
	    WaitTask("ThrowBlokeMain"),
	    LogEvery(k_NutterGlobals, k_NutterScripts),
	    LogGlobals(k_NutterGlobals, k_NutterScripts, k_NutterCallsSeconds),
	};
	ThrowNutter(commands, k_NutterLandThrow);
	// [shortcut] back at his spot he is set alight, as a fire the player started would, and the camera stays on his
	// calls
	commands.push_back({.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_NutterEye, .look = k_NutterLook});
	commands.push_back({.kind = Kind::ScriptSetOnFire, .delaySeconds = 5.0f, .value = 1, .amount = 1.0f, .thing = Nutter()});
	commands.push_back(LogGlobals(k_NutterGlobals, k_NutterScripts, k_NutterCallsSeconds));
	ThrowNutter(commands, k_NutterPondThrow);
	// out of sight in the pond he is made again at his spot, then the camera comes back to him
	commands.push_back({.kind = Kind::CameraTo, .eye = k_AwayEye, .look = k_AwayLook});
	commands.push_back(
	    {.kind = Kind::CameraTo, .delaySeconds = k_NutterCallsSeconds, .eye = k_NutterEye, .look = k_NutterLook});
	commands.push_back(LogGlobals(k_NutterGlobals, k_NutterScripts, k_NutterFlightSeconds));
	all.push_back({
	    .id = "quest.throw_bloke",
	    .name = "Land 3 quest run: the man who wants to be thrown",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/ThrowBlokeMain on Land3.txt). The camera "
	                   "looks at the man by his campfire from 40 away for 45 seconds; then the hand takes him and "
	                   "throws him 30 north onto the land; back at his spot he is set alight; then the hand throws "
	                   "him into the pond 60 south of his spot, and the camera looks away and back.",
	    .expected = "While the camera is near he calls to be picked up, every 20 seconds. Each throw slows the game "
	                "while he flies, the camera following him; landed, he walks back to his spot and boasts. Out of "
	                "sight in the pond he is made again at his spot. Alight, he begs for the fire to be put out.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The same camera on the man and nothing else, for a run whose grab and throw come through the window's own events:
/// the timeline only waits while the quest runs
void AddThrowBlokeByHand(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_NutterEye, .look = k_NutterLook},
	    WaitTask("ThrowBlokeMain"),
	    LogEvery(k_NutterGlobals, k_NutterScripts),
	    LogGlobals(k_NutterGlobals, k_NutterScripts, k_NutterByHandSeconds),
	};
	all.push_back({
	    .id = "quest.throw_bloke_by_hand",
	    .name = "Land 3 quest run: the man who wants to be thrown, for the player's own grab and throw",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/ThrowBlokeMain on Land3.txt), with an input "
	                   "recording replayed (tools/checks/real_input_tap.sh). The camera looks at the man by his campfire "
	                   "from 40 away and the timeline only waits; it moves no pointer.",
	    .expected = "The recording's grab picks him up and its throw sends him flying: the game slows while he flies, "
	                "the camera following him, and landed he walks back to his spot and boasts.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

} // namespace

void testbed_scenarios::AddQuestBranchScenarios(std::vector<Scenario>& all)
{
	AddTreacherousPathFourDeaths(all);
	AddTreacherousPathNoPotion(all);
	AddHeartbrokenManKilled(all, true);
	AddTotemPuzzleTwoFailures(all);
	AddHeartbrokenManKilled(all, false);
	AddTreacherousPathMorgFight(all);
	AddTreacherousPathTwoDeaths(all);
	AddVortexRumbles(all);
	AddDefendingOgresRaids(all);
	AddShieldStonesAndCreature(all);
	AddThrowBloke(all);
	AddThrowBlokeByHand(all);
	AddHeartbrokenManKilledByHand(all, true);
	AddHeartbrokenManKilledByHand(all, false);
}
