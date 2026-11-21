/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 4's two Theseus mazes as a quest run, played after a land entry's quest= has started them. The timeline only
// holds the camera high over the grid and logs the quest's state: the player's clicks on the grid's squares come from
// the real input path (tools/checks/real_input_tap.sh), never the testbed's pointer. The points are map coordinates,
// taken from the place the challenge script names.

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
/// The middle of the second maze's grid (8 squares of 4 metres from the place the script names), and the camera
/// straight over it, this high above the land and a metre to its south: the clicks' pixels are worked out for this
/// view (the spec tools/checks/story/quests/theseus_maze.txt gives them)
constexpr glm::vec2 k_GridMiddle {1917.4681f, 2467.2109f};
constexpr float k_EyeHigh = 55.0f;
constexpr float k_EyeSouth = 1.0f;
/// Seconds between the logs of the quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

constexpr std::string_view k_Globals = "";
constexpr std::string_view k_Scripts = "ThesiusPuzzleLand4 DidYouKnow rewardfromsky";

void AddTheseusMaze(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    WaitTask("ThesiusPuzzleLand4"),
	    {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_Globals, .script = k_Scripts},
	    CameraOn(k_GridMiddle, k_EyeHigh, k_EyeSouth, 0.0f),
	    WaitTask("ThesiusPuzzleLand4", TaskWait::Finished),
	    LogGlobals(k_Globals, k_Scripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.theseus_maze",
	    .name = "Land 4 quest run: the Theseus mazes, clicked by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (land:Land4.txt:<frames>:quest=SetupLand4/BeginLand4(0)/"
	                   "ThesiusPuzzleLand4:timeline=quest.theseus_maze), through tools/checks/real_input_tap.sh: it puts "
	                   "the camera 55 above the second maze's middle, looking straight down, and waits for the quest to "
	                   "end. The clicks on the squares that take each villager out are the real input's.",
	    .expected = "The first maze is made with its villager and wolf; each click moves the villager a square, a marker "
	                "shows the square, and the wolf takes up to two steps after him. Out of the first maze it goes and the "
	                "second is made; out of that the did-you-know signpost goes and the prize falls on the home town.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestTheseusMazeScenarios(std::vector<Scenario>& all)
{
	AddTheseusMaze(all);
}
