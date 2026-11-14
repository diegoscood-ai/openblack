/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Beach Temple Puzzle as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"): the quest's script is started right after the land's setup and before the arrival,
// as the land's control script starts it (quest=SetupLand2/HanoiFlood&/EnterLand2). Once the arrival is over the
// camera is put on the scroll by the beach temple, where the good advisor's remark is let come, and the scroll is
// pressed on with the hand; the introduction's texts are clicked on. Then the four rings are moved from the first base
// to the last with the hand, in the fifteen moves that never put a ring on a smaller one, and the solve's scene is let
// play. The points are map coordinates, taken from the places the quest's script names and where the puzzle's half turn
// puts its bases; none has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// The scroll north of the beach temple, raised 2 above the ground
constexpr glm::vec2 k_Scroll {2385.287f, 2649.39f};
/// How long the camera rests on the scroll before the press, for the good advisor's remark on it
constexpr float k_RemarkSeconds = 10.0f;

/// The puzzle's three bases, where the puzzle's half turn puts them: 20 m apart towards -z from the puzzle's spot, the
/// last one up the beach
constexpr std::array<glm::vec2, 3> k_Bases {{
    {2386.968f, 2633.286f},
    {2386.968f, 2613.286f},
    {2386.968f, 2593.286f},
}};
/// The rings' kinds, the smallest first, as the moves name them
constexpr std::array<MobileObjectInfo, 4> k_Rings {MobileObjectInfo::HanoiPuzzlePart4, MobileObjectInfo::HanoiPuzzlePart3,
                                                   MobileObjectInfo::HanoiPuzzlePart2, MobileObjectInfo::HanoiPuzzlePart1};
/// Where the camera watches the moves from: this far above the land and to the west of the middle base, across the
/// row of bases rather than along it
constexpr float k_BasesEyeAbove = 45.0f;
constexpr float k_BasesEyeSide = 40.0f;
/// How near a base the pick looks for the ring it takes
constexpr float k_RingPickRadius = 5.0f;
/// How long the hand holds the Action button down on a ring to take it
constexpr float k_GrabHoldSeconds = 0.6f;
/// How long a move is given after its drop, for the ring to settle and the puzzle to be calm for long enough again
constexpr float k_SettleSeconds = 3.0f;
/// One move: a ring (by its place among k_Rings, the smallest first) from one base to another
struct Move
{
	size_t ring;
	size_t from;
	size_t to;
};
/// The fifteen moves that take the four rings from the first base to the last, the smallest every other move
constexpr std::array<Move, 15> k_Moves {{
    {0, 0, 1},
    {1, 0, 2},
    {0, 1, 2},
    {2, 0, 1},
    {0, 2, 0},
    {1, 2, 1},
    {0, 0, 1},
    {3, 0, 2},
    {0, 1, 2},
    {1, 1, 0},
    {0, 2, 0},
    {2, 1, 2},
    {0, 0, 1},
    {1, 0, 2},
    {0, 1, 2},
}};

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "HanoiIntroTriggered ChallengeHighlightEndVariable";
constexpr std::string_view k_Scripts = "HanoiFlood HanoiPuzzleStart ChallengeHighlightNotifyWithEndVariable DidYouKnow";

/// The camera on the three bases from the side of their row, the middle one in the middle of the screen
[[nodiscard]] Command BasesCamera()
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {k_Bases.at(1).x - k_BasesEyeSide, k_BasesEyeAbove, k_Bases.at(1).y},
	        .look = {k_Bases.at(1).x, 0.0f, k_Bases.at(1).y}};
}

std::vector<Command> Playthrough()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    LogGlobals(k_Globals, k_Scripts, 1.0f),
	    // [camera] the scroll by the temple, near enough and on the screen for the advisor's remark
	    CameraOn(k_Scroll, k_EyeAbove, k_EyeBack, 2.0f),
	    LogGlobals(k_Globals, k_Scripts, k_RemarkSeconds),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Scroll);
	// the introduction: the man's lines, the signpost and its line, and the camera given back
	commands.push_back(WaitText(5263));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 15.0f));
	// [camera] the three bases from the side of their row, the middle one in the middle of the screen, so that no
	// stack stands between the camera and another base
	commands.push_back(BasesCamera());
	// [real] each ring taken from the top of its stack with the hand and let go of over the base it goes to
	for (const auto& move : k_Moves)
	{
		const ThingNear ring {.type = script::ObjectType::MobileObject,
		                      .subtype = static_cast<uint32_t>(k_Rings.at(move.ring)),
		                      .point = k_Bases.at(move.from),
		                      .radius = k_RingPickRadius};
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = ring});
		commands.push_back(
		    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = ring});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
		commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Bases.at(move.to)});
		commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
		commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
		commands.push_back(LogGlobals(k_Globals, k_Scripts, k_SettleSeconds));
	}
	// the solve: the man at the rebuilt temple, his three lines, the log at 1, the healing and the signpost gone
	commands.push_back(WaitText(5266));
	commands.push_back(WaitTask("HanoiFlood", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	return commands;
}
/// How often the run for the player's own moves logs the quest
constexpr size_t k_ByHandLogSeconds = 30;

/// The camera held on the bases for a run whose ring moves come through the window's own events: the timeline moves no
/// pointer, and the introduction is not played
std::vector<Command> ByHand()
{
	return {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    {.kind = Kind::LogGlobals, .value = k_ByHandLogSeconds, .global = k_Globals, .script = k_Scripts},
	    BasesCamera(),
	    WaitTask("HanoiFlood", TaskWait::Finished),
	    LogGlobals(k_Globals, k_Scripts, 2.0f),
	};
}
} // namespace

void testbed_scenarios::land2_quests::AddBeachTempleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_beach_temple_puzzle",
	    .name = "Land 2 quest run: The Beach Temple Puzzle, the introduction and the solve",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts the quest's script before the arrival "
	                   "(quest=SetupLand2/HanoiFlood&/EnterLand2 on Land2.txt), as the land's control script does. The "
	                   "texts that wait for a click are clicked on, the arrival's too. Once the arrival is over it puts "
	                   "the camera on the scroll by the beach temple for ten seconds and presses on the scroll with the "
	                   "hand; after the introduction it moves the four rings from the first base to the last with the "
	                   "hand, one at a time, in the fifteen moves of the puzzle's solution.",
	    .expected = "The puzzle is made on the beach with its ring of influence and the scroll; with the camera near "
	                "the good advisor says 5276. The press plays the introduction: the man's lines 5259 to 5263, the "
	                "log at 0 with alignment -0.5, the rules signpost (text 4210), and the man fades away; the scroll's "
	                "script then waits for the puzzle. Each ring taken goes onto the base it is let go of over; after "
	                "the fifteenth move the puzzle is solved: the man's lines 5264, 5265 and 5266, the log at 1 with "
	                "alignment 1, the healing at the rebuilt temple and the signpost faded away; the end check logs "
	                "HanoiIntroTriggered=1 and ChallengeHighlightEndVariable=1. Unverified: that the hand reaches each "
	                "ring from this camera and lets it go near enough to the base for it to settle.",
	    .commands = Playthrough(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_beach_temple_puzzle_by_hand",
	    .name = "Land 2 quest run: The Beach Temple Puzzle, the rings moved by the player's own input",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts the quest's script before the arrival "
	                   "(quest=SetupLand2/HanoiFlood&/EnterLand2 on Land2.txt), with an input recording replayed "
	                   "(tools/checks/real_input_tap.sh). The texts that wait for a click are clicked on. Once the arrival "
	                   "is over it holds the camera on the three bases from the side of their row, as the other run's "
	                   "moves are watched, and only waits; it moves no pointer and presses no scroll.",
	    .expected = "The recording's presses take each ring from the top of its stack and let it go over the base it goes "
	                "to; after the fifteenth move the puzzle is solved and the success scene plays, without the "
	                "introduction's thanks (the introduction was not played).",
	    .commands = ByHand(),
	    .onLand = true,
	});
}
