/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 5's gold scroll I Have a Surprise for You as a quest run, played after a land entry's quest= has started the
// land's control script: the land set up, the opening film, the curse and the loop on the three Wonder villages. The
// timeline does what the player would: it clicks the texts that wait for a click, lets the first night's curse strike,
// then hands the three Wonder villages to the player one at a time, a film each, which lifts the curse. The points are
// map coordinates, taken from the places the control script names; not yet seen in a running game. Also the land's
// control script on its own, quest.land_control_5: the background quests the control script starts let run, and two
// villages handed to the player so that the control script starts the quests their winning starts.

#include <cstddef>

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
/// The three Wonder villages, Aztec, Greek and Tibetan, where the control script finds their towns
constexpr std::array<glm::vec2, 3> k_WonderVillages {{
    {2637.484f, 3747.7695f},
    {2220.5083f, 2633.8025f},
    {1678.1516f, 2322.4634f},
}};
/// The control script finds each within 100 of its point, so the pick looks as far
constexpr float k_VillagePickRadius = 100.0f;
/// More belief in the player than in anyone else, as winning a village gives
constexpr float k_WinningBelief = 2.0f;
/// The land starts at 13:00 and the curse first strikes after 18:00, about ten minutes of the game's day in; the first
/// village is handed over a minute after that strike, the others two minutes apart, each film's length
constexpr float k_FirstNightSeconds = 660.0f;
constexpr float k_FilmSeconds = 120.0f;
/// Seconds between the logs of the quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

constexpr std::string_view k_Globals = "LandNumber";
constexpr std::string_view k_Scripts = "LandControl5 CreatureCurse SetupLand5 BeginLand5";

/// The neutral Japanese village, where the control script finds it, and the Greek Wonder village
constexpr glm::vec2 k_JapaneseVillage {2257.9912f, 3315.3586f};
constexpr glm::vec2 k_GreekVillage = k_WonderVillages.at(1);
/// The control script's waits, and the scripts it starts, in the background or once a village is won
constexpr std::string_view k_ControlGlobals = "LandNumber ArkSailed NemesisStillFighting";
constexpr std::string_view k_ControlScripts =
    "LandControl5 SetupLand5 BeginLand5 LionPuzzle CreatureCurse JapaneseTraitor ThrowThroughShield Crusaders "
    "NemesisDefendHisLastTown SwapToBrownBear FireOnHighMain";
/// The same with the missionaries' quest, logged only at the run's end: a run without the boat having sailed in the
/// first land closes its quest's window there
constexpr std::string_view k_EndScripts =
    "LandControl5 LionPuzzle CreatureCurse JapaneseTraitor ThrowThroughShield Crusaders NemesisDefendHisLastTown "
    "SwapToBrownBear FireOnHighMain MissionariesReturned";
/// How long the land runs with every background quest before the first village is handed over, so that each of them
/// has begun, and the gap before the second, so that the first's start shows apart
constexpr float k_BackgroundSeconds = 120.0f;
constexpr float k_VillageGapSeconds = 60.0f;
/// How long the land runs once the second village's quest has begun, past the five minutes it waits before its attack
constexpr float k_AfterSeconds = 330.0f;

void AddLandControl(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the last script the land's control script starts in the background
	    WaitTask("NemesisDefendHisLastTown"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_ControlGlobals, .script = k_ControlScripts},
	    // [shortcut] the neutral Japanese village takes more belief in the player than in anyone else, as winning it
	    // gives, and its next fold hands it over; the control script then starts the brown bear's quest
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = k_BackgroundSeconds,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    WaitTask("SwapToBrownBear"),
	    // [shortcut] the Greek Wonder village handed over the same way; the control script plays its film and then
	    // starts the heavenly fire's quest
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = k_VillageGapSeconds,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_GreekVillage, .radius = k_VillagePickRadius}},
	    WaitTask("FireOnHighMain"),
	    LogGlobals(k_ControlGlobals, k_EndScripts, k_AfterSeconds),
	};
	all.push_back({
	    .id = "quest.land_control_5",
	    .name = "Land 5 quest run: the land's control script",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts only the land's control script (quest=LandControl5 "
	                   "on Land5.txt, or with ArkSailed=1 before it for a boat that sailed in the first land). The texts "
	                   "that wait for a click are clicked on. Once the control script has started its background quests "
	                   "it lets them run for two minutes, then hands the neutral Japanese village to the player and, a "
	                   "minute after its quest has begun, the Greek Wonder village, and lets the land run five and a half "
	                   "minutes more, logging the quests' state every 30 seconds.",
	    .expected = "The opening film and the curse, then every background quest begins: the lion puzzle, the curse, the "
	                "Japanese traitor, the shield, the crusaders, Nemesis's last town's defences, and the missionaries' "
	                "return only when ArkSailed is 1. The Japanese village won starts Swap to Brown Bear; the Greek "
	                "village won plays its film and starts The Heavenly Fire.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The control script's own record of each village it watches, read from its task: whether it counts the Japanese
/// village and the Greek Wonder village as the player's, whether it has started the brown bear's quest and played the
/// Greek film, and whether its loop on the Wonder villages is over
constexpr std::string_view k_TakenGlobals =
    "LandNumber NemesisStillFighting LandControl5.NeutralTown1Taken LandControl5.RunBrownBear LandControl5.Town3Taken "
    "LandControl5.Town3FirstTaken LandControl5.WondersActive";
/// Nemesis's gift is more than the player's belief once the village is won (its winner's belief grows by half), and
/// the player's gift back more than Nemesis's then: both kept below the town's cap on belief (10), at which a tie goes to
/// Nemesis
constexpr float k_RetakingBelief = 4.0f;
constexpr float k_WinningBackBelief = 8.0f;
/// The Greek film lasts over a minute: the Japanese village is won this long into it
constexpr float k_IntoFilmSeconds = 15.0f;
/// The gap between a gift and the readout of the town's owner, past the town's next fold, and between the readout and
/// the next gift
constexpr float k_FoldSeconds = 20.0f;
constexpr float k_StepSeconds = 40.0f;
/// After the third Wonder village, its film and the scene that ends the loop on the Wonder villages
constexpr float k_LoopEndSeconds = 180.0f;
/// How long the land runs after the Japanese village is won once the loop is over
constexpr float k_NeverOfferedSeconds = 60.0f;

[[nodiscard]] Command GiveVillage(PlayerNames player, glm::vec2 village, float delay, float amount)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = player,
	        .delaySeconds = delay,
	        .amount = amount,
	        .thing = ThingNear {.type = script::ObjectType::Town, .point = village, .radius = k_VillagePickRadius}};
}

/// The town's owner logged, and its belief in that owner and in the player named: a readout, with no change of hands
[[nodiscard]] Command ReadOwner(PlayerNames player, glm::vec2 village)
{
	return {.kind = Kind::TownOwner,
	        .player = player,
	        .delaySeconds = k_FoldSeconds,
	        .value = k_TownOwnerReadOnly,
	        .thing = ThingNear {.type = script::ObjectType::Town, .point = village, .radius = k_VillagePickRadius}};
}

void AddLandControlRetaken(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("NemesisDefendHisLastTown"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_TakenGlobals, .script = k_ControlScripts},
	    // [shortcut] the Greek Wonder village won by belief, then the Japanese village while the Greek film plays: the
	    // control script's town checks wait for the film, so the brown bear's quest starts after it
	    GiveVillage(PlayerNames::PLAYER_ONE, k_GreekVillage, k_BackgroundSeconds, k_WinningBelief),
	    GiveVillage(PlayerNames::PLAYER_ONE, k_JapaneseVillage, k_IntoFilmSeconds, k_WinningBelief),
	    WaitTask("FireOnHighMain"),
	    WaitTask("SwapToBrownBear"),
	};
	// [shortcut] Nemesis retakes each village by belief and the player wins it back, the Japanese village first, each
	// change of hands read back and the control script's record logged
	for (const auto village : {k_JapaneseVillage, k_GreekVillage})
	{
		for (const auto& [player, amount] :
		     {std::pair {PlayerNames::PLAYER_TWO, k_RetakingBelief}, std::pair {PlayerNames::PLAYER_ONE, k_WinningBackBelief}})
		{
			commands.push_back(GiveVillage(player, village, k_StepSeconds, amount));
			commands.push_back(ReadOwner(player, village));
			commands.push_back(LogGlobals(k_TakenGlobals, k_ControlScripts));
		}
	}
	commands.push_back(LogGlobals(k_TakenGlobals, k_EndScripts, k_AfterSeconds));
	all.push_back({
	    .id = "quest.land_control_5_retaken",
	    .name = "Land 5 quest run: the land's control script, villages retaken by Nemesis",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts only the land's control script (quest=LandControl5 "
	                   "on Land5.txt). The texts that wait for a click are clicked on. Two minutes after the background "
	                   "quests have begun the Greek Wonder village is handed to the player, and the Japanese village 15 "
	                   "seconds into its film. Once both quests have begun, Nemesis retakes the Japanese village and the "
	                   "player wins it back, then the same with the Greek village, each by belief, 40 seconds apart, each "
	                   "owner read back and the control script's record of the villages logged; the land runs five and a "
	                   "half minutes more.",
	    .expected = "The Greek film plays and The Heavenly Fire begins; Swap to Brown Bear begins only after the film. "
	                "Losing the Japanese village makes the control script count it as lost and winning it back as won, "
	                "with no second Swap to Brown Bear; losing and winning back the Greek village starts no second "
	                "Heavenly Fire.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddLandControlWondersFirst(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("NemesisDefendHisLastTown"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_TakenGlobals, .script = k_ControlScripts},
	};
	// [shortcut] the three Wonder villages won by belief, a film apart, before the Japanese village
	for (size_t i = 0; i < k_WonderVillages.size(); ++i)
	{
		commands.push_back(GiveVillage(PlayerNames::PLAYER_ONE, k_WonderVillages.at(i),
		                               i == 0 ? k_BackgroundSeconds : k_FilmSeconds, k_WinningBelief));
	}
	// [shortcut] then the Japanese village, once the loop on the Wonder villages is over
	commands.push_back(GiveVillage(PlayerNames::PLAYER_ONE, k_JapaneseVillage, k_LoopEndSeconds, k_WinningBelief));
	commands.push_back(ReadOwner(PlayerNames::PLAYER_ONE, k_JapaneseVillage));
	commands.push_back(LogGlobals(k_TakenGlobals, k_EndScripts, k_NeverOfferedSeconds));
	all.push_back({
	    .id = "quest.land_control_5_wonders_first",
	    .name = "Land 5 quest run: the land's control script, the Wonder villages won first",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts only the land's control script (quest=LandControl5 "
	                   "on Land5.txt). The texts that wait for a click are clicked on. Two minutes after the background "
	                   "quests have begun the Aztec, Greek and Tibetan Wonder villages are handed to the player by belief, "
	                   "two minutes apart, then three minutes later the Japanese village; its owner is read back and the "
	                   "land runs a minute more.",
	    .expected = "Each Wonder village plays its film; with the three won the loop on them ends. The Japanese village "
	                "won after that starts no Swap to Brown Bear.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// Nemesis's home town, where the control script finds it, and the camera put over it so that the town is in view
constexpr glm::vec2 k_NemesisHomeTown {3123.7798f, 2625.75f};
constexpr float k_OverHomeTownHeight = 60.0f;
constexpr float k_OverHomeTownBack = 120.0f;
/// A little belief in the player, above the tenth the land's first phase allows there
constexpr float k_SomeBelief = 0.5f;
/// Long enough for the good advisor's two lines on why the home town can't be won yet
constexpr float k_HomeTownLinesSeconds = 30.0f;
/// Belief in the player for the home town: more than Nemesis's there, kept below the town's cap on belief (10)
constexpr float k_HomeTownBelief = 9.0f;
/// The control script's record of whether the home town can be won, and the big fight's scripts
constexpr std::string_view k_HomeGlobals = "LandNumber NemesisStillFighting LandControl5.NemesisTownNowConquerable "
                                           "LandControl5.WondersActive";
constexpr std::string_view k_FightScripts = "LandControl5 TheBigFightMain EndOfGameCreatureSequenceGoToVolcano "
                                            "NemesisCitadelDestroyed VillageWavingSequence";
constexpr float k_FightSeconds = 150.0f;

void AddLandControlHomeTown(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("NemesisDefendHisLastTown"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_HomeGlobals, .script = k_ControlScripts},
	    // the camera over Nemesis's home town; [shortcut] a little belief in the player there, as worship would win it
	    CameraOn(k_NemesisHomeTown, k_OverHomeTownHeight, k_OverHomeTownBack, 0.0f),
	    GiveVillage(PlayerNames::PLAYER_ONE, k_NemesisHomeTown, k_VillageGapSeconds, k_SomeBelief),
	    LogGlobals(k_HomeGlobals, k_ControlScripts, k_HomeTownLinesSeconds),
	    ReadOwner(PlayerNames::PLAYER_ONE, k_NemesisHomeTown),
	};
	// [shortcut] the three Wonder villages and then the Japanese village won by belief, as in the run with the Wonder
	// villages won first
	for (size_t i = 0; i < k_WonderVillages.size(); ++i)
	{
		commands.push_back(GiveVillage(PlayerNames::PLAYER_ONE, k_WonderVillages.at(i),
		                               i == 0 ? k_VillageGapSeconds : k_FilmSeconds, k_WinningBelief));
	}
	commands.push_back(GiveVillage(PlayerNames::PLAYER_ONE, k_JapaneseVillage, k_LoopEndSeconds, k_WinningBelief));
	commands.push_back(ReadOwner(PlayerNames::PLAYER_ONE, k_JapaneseVillage));
	commands.push_back(LogGlobals(k_HomeGlobals, k_ControlScripts));
	// [shortcut] Nemesis retakes the Japanese village and the player wins it back, each by belief and read back
	for (const auto& [player, amount] :
	     {std::pair {PlayerNames::PLAYER_TWO, k_RetakingBelief}, std::pair {PlayerNames::PLAYER_ONE, k_WinningBackBelief}})
	{
		commands.push_back(GiveVillage(player, k_JapaneseVillage, k_StepSeconds, amount));
		commands.push_back(ReadOwner(player, k_JapaneseVillage));
		commands.push_back(LogGlobals(k_HomeGlobals, k_ControlScripts));
	}
	// [shortcut] then his home town won by belief
	commands.push_back(GiveVillage(PlayerNames::PLAYER_ONE, k_NemesisHomeTown, k_StepSeconds, k_HomeTownBelief));
	commands.push_back(ReadOwner(PlayerNames::PLAYER_ONE, k_NemesisHomeTown));
	commands.push_back(LogGlobals(k_HomeGlobals, k_FightScripts));
	commands.push_back(LogGlobals(k_HomeGlobals, k_FightScripts, k_FightSeconds));
	all.push_back({
	    .id = "quest.land_control_5_home_town",
	    .name = "Land 5 quest run: the land's control script, Nemesis's home town won last",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts only the land's control script (quest=LandControl5 "
	                   "on Land5.txt). The texts that wait for a click are clicked on. Once the background quests have "
	                   "begun the camera is put over Nemesis's home town and the town is given a little belief in the "
	                   "player; after the good advisor's lines the Wonder villages and the Japanese village are won by "
	                   "belief as in quest.land_control_5_wonders_first. Then Nemesis retakes the Japanese village and the "
	                   "player wins it back, and the home town is won by belief, each owner read back; the land runs two "
	                   "and a half minutes more.",
	    .expected = "In the first phase the home town's belief in the player is set back to nothing and the good advisor "
	                "explains why. After the Wonder villages, while Nemesis holds the Japanese village the home town's "
	                "belief is held at nothing with the two lines that say so; once he holds none, the line that it can "
	                "be won. Winning it ends the loop: every other script stops and the big fight begins.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand5SurpriseScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("CreatureCurse"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_Globals, .script = k_Scripts},
	};
	// [shortcut] each Wonder village takes more belief in the player than in anyone else, as winning it gives, and its
	// next fold hands it over: the first after the first night's strike, the others once the film before has played
	for (size_t i = 0; i < k_WonderVillages.size(); ++i)
	{
		commands.push_back(
		    {.kind = Kind::TownGainsBelief,
		     .player = PlayerNames::PLAYER_ONE,
		     .delaySeconds = i == 0 ? k_FirstNightSeconds : k_FilmSeconds,
		     .amount = k_WinningBelief,
		     .thing =
		         ThingNear {.type = script::ObjectType::Town, .point = k_WonderVillages.at(i), .radius = k_VillagePickRadius}});
	}
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_FilmSeconds));
	all.push_back({
	    .id = "quest.i_have_a_surprise_for_you",
	    .name = "Land 5 quest run: I have a surprise for you",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl5 on Land5.txt). The texts that wait for a "
	                   "click are clicked on. It lets the land's first night come and the curse strike, then hands the "
	                   "Aztec, Greek and Tibetan Wonder villages to the player, the first eleven minutes in and the others "
	                   "two minutes apart, and logs the quest's state every 30 seconds.",
	    .expected = "The opening film and the curse: after 18:00 it strikes, the creature weakened, shrunk and turned, the "
	                "advisors' line, the creature looking at the hand. Each village won plays its film: Nemesis's hand, "
	                "the Wonder blasted, the step logged. With the three won the curse is lifted in the healing scene. "
	                "Unverified: that each film is over within two minutes.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
	AddLandControl(all);
	AddLandControlRetaken(all);
	AddLandControlWondersFirst(all);
	AddLandControlHomeTown(all);
}
