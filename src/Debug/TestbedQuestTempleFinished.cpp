/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Temple Is Finished as a quest run's chapter: the scene that follows the opening walk once the temple is built.
// The player lets the flight to the temple play, waits for the advisor's reminder to go in, strays from the door to be
// pulled back to it, goes in through the entrance with the Action button, stays inside for a while, leaves with Escape,
// and lets the gold scroll's showing play to its end. The points are map coordinates, taken from the places the scene's
// script names; those not yet seen in a running game say so.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <vector>

#include <SDL_scancode.h>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The waits for a task of a script: running, and none running
constexpr size_t k_Running = 0;
constexpr size_t k_NoneRunning = 1;

Command WaitTask(std::string_view script, size_t wait = k_Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = wait, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

/// Logs the globals and scripts named, some seconds after the command before it
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .global = globals, .script = scripts};
}

Command ClickTexts()
{
	return {.kind = Kind::ClickTexts, .value = 1};
}

Command KeyDown(SDL_Scancode key, float delay = 0.0f)
{
	return {.kind = Kind::KeyDown, .delaySeconds = delay, .value = static_cast<size_t>(key)};
}

Command KeyUp(SDL_Scancode key, float delay = 0.0f)
{
	return {.kind = Kind::KeyUp, .delaySeconds = delay, .value = static_cast<size_t>(key)};
}

/// The middle of the temple's doorway on the screen, as fractions of its width and height from the top left, from the
/// view the scene pulls the camera back to. The scene's door point is on the ground at the doorway's sill, so a press
/// aimed at it hits the land in front of the doorway; the doorway's own faces, which a press must hit to go in, stand
/// above it on the screen, about 6 high and 6 deep behind the sill
constexpr glm::vec2 k_DoorwayOnScreen {0.50f, 0.35f};
/// The temple's plan, where the land's control script orders the temple built
constexpr glm::vec2 k_TemplePlan {1915.05f, 2508.89f};
/// A spot over the village, well over 100 from the door, where the player looks away before going in
constexpr glm::vec2 k_AwayFromDoor {1700.0f, 2700.0f};
/// The scene's first reminder to go in comes 20 seconds after its cinema ends; the player looks away a little after it
constexpr float k_AfterReminder = 25.0f;
/// How long the player stays inside before pressing Escape. The scene's reminder to leave waits on a minute's timer that
/// counts game turns, and inside the temple the world is paused, so the reminder never comes: the player leaves after a
/// fixed time instead of waiting for it
constexpr float k_TimeInside = 10.0f;
/// The scene's pull back to the door takes 3 seconds; the pointer is put on the doorway once it is done
constexpr float k_AfterPullBack = 5.0f;
/// The globals and scripts the scene's end is read by
constexpr std::string_view k_TempleGlobals = "CitadelBuiltL1 IsSkippingToCreatureSelect";
constexpr std::string_view k_TempleScripts = "CitadelGuide CitadelGuideCinema DidYouKnow";
} // namespace

void openblack::testbed_scenarios::AddQuestTempleFinishedScenarios(std::vector<Scenario>& all)
{
	// The Temple Is Finished, started directly (a land entry's quest=SetupLand1/CitadelGuide), or after
	// story.t5_temple_built in the story's own timeline: the way in is the pointer on the doorway and the Action
	// button, the way out the Escape key, as the player gives them
	all.push_back({
	    .id = "quest.the_temple_is_finished",
	    .name = "Land 1 quest run: The Temple Is Finished",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 once the temple is built (after story.t5_temple_built, or a quest run's start of "
	                   "SetupLand1/CitadelGuide with the temple built at once). It lets the flight to the temple play, "
	                   "waits for the reminder to go in, looks away over the village to be pulled back to the door, "
	                   "puts the pointer on the doorway and presses the Action button, stays inside for 10 seconds, "
	                   "presses Escape, and lets the gold scroll's showing play to its end.",
	    .expected = "Texts 1998 and 2000 on the flight; 2000 again as the reminder 20 s later; looking away brings the "
	                "camera back to the door with text 1322; the press takes the player inside; Escape 10 s later leaves "
	                "(the reminder 2009 is not waited for: its timer stands still while the world is paused inside); "
	                "then 5212, 5213, 5199, 5206, 5207 and 5232, the stand-in gold scroll deleted and CitadelGuide "
	                "returns.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("CitadelGuideCinema"),
	            // a script's order to build the building at the temple's plan with desire 1, as the land's control script
	            // gives it when the tutorial is skipped: a quest run has only the plan, which the town's builders never
	            // start on their own. After the story's own chapters the temple is already built and it starts nothing
	            {.kind = Kind::ScriptBuildBuilding, .point = k_TemplePlan, .amount = 1.0f},
	            // the scene waits for a built temple, which the builders would take much longer than the scene's start to
	            // finish: it stands for its built percentage set to 1
	            {.kind = Kind::TempleBuilt, .built = 1.0f},
	            // the flight: the entrance's line is read, then the cinema ends once the camera arrives
	            WaitText(1998, true),
	            WaitText(2000, true),
	            // the reminder to go in comes; then the player looks away over the village, and the scene pulls the camera
	            // back to the door
	            {.kind = Kind::CameraTo,
	             .delaySeconds = k_AfterReminder,
	             .eye = {k_AwayFromDoor.x, 60.0f, k_AwayFromDoor.y - 60.0f},
	             .look = {k_AwayFromDoor.x, 10.0f, k_AwayFromDoor.y}},
	            WaitText(1322, true),
	            // going in: the pointer on the doorway, the Action button pressed and let go
	            {.kind = Kind::PointerTo, .delaySeconds = k_AfterPullBack, .point = k_DoorwayOnScreen},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            // inside for a while, then Escape. The reminder to leave is not waited for: its timer counts game turns,
	            // which stand still while the world is paused inside
	            KeyDown(SDL_SCANCODE_ESCAPE, k_TimeInside),
	            KeyUp(SDL_SCANCODE_ESCAPE, 0.2f),
	            // the way out: the gold scroll shown, to the scene's last line
	            WaitText(5212, false),
	            WaitText(5232, true),
	            WaitTask("CitadelGuide", k_NoneRunning),
	            LogGlobals(k_TempleGlobals, k_TempleScripts, 1.0f),
	        },
	    // the temple's room pauses the game: the time inside is counted on the testbed's clock
	    .clockWhilePaused = true,
	    .onLand = true,
	});
}
