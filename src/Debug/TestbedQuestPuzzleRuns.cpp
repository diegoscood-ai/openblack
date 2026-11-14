/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The puzzles of Lands 2 to 4 as quest runs played by hand: each timeline only puts the camera straight over what the
// player clicks, at the moments the quest reaches it, and logs the quest's state. The clicks themselves come from the
// real input path (tools/checks/real_input_tap.sh, a crafted input recording), never the testbed's pointer, so the
// pixels each run's steps press are worked out for these views. The points are map coordinates, taken from the places
// the challenge scripts name.

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
/// A camera straight over a point of the land (a few centimetres to its south, for the view's up), this high above
/// the land there: the point is the screen's middle and the land round it lies at its true distances
[[nodiscard]] Command StraightOver(glm::vec2 point, float high)
{
	return CameraOn(point, high, 0.05f, 0.0f);
}

/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

// ---- The fish puzzle (Land 4) ----

/// The fish puzzle's scroll, on the shore, and the net's middle
constexpr glm::vec2 k_FishScroll {2436.942f, 3598.528f};
constexpr glm::vec2 k_FishNet {2497.891f, 3628.35f};
/// The camera over the scroll, and over the water behind the two shoals (both start 12 north of the net, 12 to its
/// east and to its west): the grips that scare them come from two metres beyond each shoal, away from the net
constexpr float k_ScrollHigh = 30.0f;
constexpr glm::vec2 k_BehindTheShoals {k_FishNet.x, k_FishNet.y + 13.414f};
constexpr float k_ShoalsHigh = 55.0f;
/// The boy's last line before the herding
constexpr uint32_t k_FishIntroductionLastLine = 4689;

constexpr std::string_view k_FishScripts = "FishPuzzle ChallengeHighlightNotify SwapCreatures";

void AddFishPuzzle(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FishPuzzle"),
	    LogEvery("", k_FishScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    StraightOver(k_FishScroll, k_ScrollHigh),
	    WaitText(k_FishIntroductionLastLine),
	    StraightOver(k_BehindTheShoals, k_ShoalsHigh),
	    WaitTask("FishPuzzle", TaskWait::Finished),
	    LogGlobals("", k_FishScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.fish_puzzle_by_hand",
	    .name = "Land 4 quest run: the fish puzzle, herded by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (land:Land4.txt:<frames>:quest=SetupLand4/BeginLand4(0)/FishPuzzle:"
	                   "timeline=quest.fish_puzzle_by_hand), through tools/checks/real_input_tap.sh. The texts that wait for "
	                   "a click are clicked on. It puts the camera 30 straight over the scroll once its notifier runs, and "
	                   "after the boy's last line 55 straight over the water behind the two shoals; the presses on the "
	                   "scroll and the grips on the water are the real input's.",
	    .expected = "The scroll's press starts the boy's introduction and the puzzle. Each grip on the water splashes it and "
	                "scares the fish within 8 of it away; herded from behind, 30 fish come into the net together, it "
	                "closes after half a second and the reward scene follows.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
// ---- The Japanese totem puzzle (Land 4) ----

/// The middle of the totems' arc (20 round the puzzle's place, from its north to its south-east), and the camera 55
/// straight over it: every totem shows, and the hand's press on its base takes hold of it
constexpr glm::vec2 k_TotemArcMiddle {2296.4447f, 3589.4393f};
constexpr float k_TotemsHigh = 55.0f;

constexpr std::string_view k_TotemScripts = "JapaneseTotemPuzzle DidYouKnow GiveSpellDispenserReward";

void AddTotemPuzzle(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("JapaneseTotemPuzzle"),
	    LogEvery("", k_TotemScripts),
	    StraightOver(k_TotemArcMiddle, k_TotemsHigh),
	    WaitTask("JapaneseTotemPuzzle", TaskWait::Finished),
	    LogGlobals("", k_TotemScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.totem_puzzle_by_hand",
	    .name = "Land 4 quest run: the Japanese totem puzzle, dragged by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (land:Land4.txt:<frames>:quest=SetupLand4/BeginLand4(0)/"
	                   "JapaneseTotemPuzzle:timeline=quest.totem_puzzle_by_hand), through tools/checks/real_input_tap.sh. "
	                   "It holds the camera 55 straight over the totems' arc; the hand's presses on the totems and its "
	                   "drags up the screen are the real input's.",
	    .expected = "Each totem dragged to its highest is a move: the others move by the puzzle's table and blend to their "
	                "heights, and a sparkle stands over each at its highest. After the eighth every totem stands at its "
	                "highest, the sparkles of three turns show and the spiritual shield dispenser is given.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// ---- The tree puzzles (Lands 2 and 3) ----

/// The grids' middle spots (spots 10 apart; Land 2's grid turned 30 degrees), and the camera 40 straight over them
constexpr glm::vec2 k_TreeOneMiddle {2294.0440f, 3185.3424f};
constexpr glm::vec2 k_TreeTwoMiddle {2699.8920f, 3141.6589f};
constexpr float k_TreesHigh = 40.0f;
/// Land 3's puzzle comes once the Indian town is the player's
constexpr glm::vec2 k_IndianTown {2805.024f, 2499.5864f};
constexpr float k_TownPickRadius = 100.0f;

void AddTreePuzzle(std::vector<Scenario>& all, std::string_view id, std::string_view name, std::string_view script,
                   glm::vec2 middle, bool takesTheIndianTown)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask(script),
	    LogEvery("", script),
	};
	if (takesTheIndianTown)
	{
		// [shortcut] the Indian town handed to the player, as winning it would
		commands.push_back(
		    {.kind = Kind::TownOwner,
		     .player = PlayerNames::PLAYER_ONE,
		     .delaySeconds = 1.0f,
		     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_IndianTown, .radius = k_TownPickRadius}});
	}
	commands.push_back(StraightOver(middle, k_TreesHigh));
	commands.push_back(WaitTask(script, TaskWait::Finished));
	commands.push_back(LogGlobals("", script, 1.0f));
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start, through tools/checks/real_input_tap.sh in two runs: the first logs "
	                   "the grid's trees as the puzzle makes them, the second, its presses worked out from them, pulls the "
	                   "trees. It holds the camera 40 straight over the grid's middle spot; the hand's presses on the trees "
	                   "are the real input's.",
	    .expected = "Each tree pulled off its spot changes it and the four spots beside it, each planted again with a small "
	                "tree of its new kind that grows a twentieth a turn; once all are grown and in one state the trees burn, "
	                "the mist goes and the bats die, and the prize follows.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestPuzzleRunScenarios(std::vector<Scenario>& all)
{
	AddFishPuzzle(all);
	AddTotemPuzzle(all);
	AddTreePuzzle(all, "quest.tree_puzzle_one_by_hand", "Land 2 quest run: the tree puzzle, pulled by hand", "TreePuzzleOne",
	              k_TreeOneMiddle, false);
	AddTreePuzzle(all, "quest.tree_puzzle_two_by_hand", "Land 3 quest run: the tree puzzle, pulled by hand", "TreePuzzleTwo",
	              k_TreeTwoMiddle, true);
}
