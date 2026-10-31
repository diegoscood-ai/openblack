/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 3's main gold quest, "So You Couldn't Bear to Be Without Your Creature?", as a quest run on the game's own Land 3
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): the land entry's quest= starts the land's setup,
// then the creature's prison in the background and the arrival, in the order the land's control script starts them.
// The timeline lets the arrival play as a player would watch it (its click-through lines clicked on), then puts the
// camera over the prison on the hill and logs the prison's state while the pillars hold the creature. Then the three
// villages that feed the pillars are wiped out one at a time, their buildings and their people, which the quest counts
// as a village won (a pillar reads a town with no building standing as the player's). A village whose people survive
// rebuilds and reads as standing again, its pillar rising. Handing a town over without the belief that wins it does not
// hold: the town's next belief count gives it back to the strongest believer. Each pillar's fall and its cut scene are
// waited on, the camera over the pillar and then back over its village, before the next village goes. Lethys's creed
// scene, which needs his last town in trouble, is not driven. The points are map coordinates taken from the places the
// quest names.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::TaskWait;

/// The prison spot on the hill, where the quest holds the player's creature between the three pillars
constexpr glm::vec2 k_Prison {2062.2075f, 2259.8464f};
/// Seconds between the logs of the prison's state while the pillars hold the creature: a few of the pillars' 3-second
/// checks each
constexpr float k_PrisonLogSeconds = 10.0f;
/// The three villages whose pillars hold the creature, Japanese, Indian and Egyptian, where the land's setup finds
/// their towns; each is picked within this of its spot
constexpr float k_VillagePickRadius = 20.0f;
constexpr std::array<glm::vec2, 3> k_Villages {{{2829.13f, 2822.8f}, {2831.99f, 2547.2f}, {2250.22f, 2188.63f}}};
/// Where each pillar's cut scene puts the camera to watch the pillar sink, and where it fades back to over the village
/// it was won from (the third pillar's scene goes on to free the creature instead); each camera is waited on within
/// the radius, which takes in its height above the land
constexpr std::array<glm::vec2, 3> k_PillarCameras {{{2073.003f, 2324.33f}, {2121.658f, 2261.63f}, {2020.365f, 2221.719f}}};
constexpr std::array<glm::vec2, 2> k_VillageCameras {{{2867.839f, 2796.466f}, {2827.757f, 2596.52f}}};
constexpr float k_PillarCameraRadius = 30.0f;
constexpr float k_VillageCameraRadius = 60.0f;
/// The good advisor's last line as the creature is freed, said once the third pillar is down
constexpr uint32_t k_FreedLastLine = 6944;

/// The globals and scripts the chapter is read by: how many pillars are down, whether the arrival vortex is still
/// open, the land's number, whether Lethys has been finished off, and whether the monk's quest is done (which the
/// wolves' attack reads)
constexpr std::string_view k_Globals =
    "PrisonStatuesDeactivated VortexOpen LandNumber LethysKilled MonkHasReachedEnd GlobalDebug";
constexpr std::string_view k_Scripts = "GetThroughVortexL2 VortexEntry FreeTheCreature FreezeRayStatue "
                                       "LethysCreatureActing SetupLethysCreature DisableLocalWeather";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// The wait for a text to have been shown and read
Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

/// Logs the globals and scripts named, some seconds after the command before it, and again every period if given
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = globals, .script = scripts};
}

Command ClickTexts()
{
	return {.kind = Kind::ClickTexts, .value = 1};
}

/// The camera put back from a point (to its south) and above it, looking at it; both heights are above the land there,
/// as the timeline's camera takes them
Command CameraOn(glm::vec2 point, float eyeHeight, float back, float lookHeight)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, eyeHeight, point.y - back},
	        .look = {point.x, lookHeight, point.y}};
}
} // namespace

void testbed_scenarios::AddQuestCouldntBearScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    // the lines said with a click to go on (the advisors' look at the creature, the statues and the villages) are
	    // clicked on
	    ClickTexts(),
	    // the arrival runs once the land is set up; the prison was started just before it
	    WaitTask("GetThroughVortexL2"),
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // the arrival's film: the vortex opens over the home town and the advisors step out (5012, 5013); the creature's
	    // moan and the camera swung to the prison (5063, clicked on); closer, the evil advisor points at it, the
	    // torture, and the good advisor's line on the statues (5018, clicked on), after which the temple and the home
	    // town's buildings are set down and the log entry is recorded; the villages (5020, clicked on), the camera back
	    // over the home town and the film's end
	    WaitText(5012),
	    WaitText(5063),
	    WaitText(5018),
	    WaitText(5020),
	    // half a minute after the film the advisors speak again (5703, 5704, 5705); the arrival then waits for the
	    // vortex to have closed and ends
	    WaitText(5705),
	    WaitTask("GetThroughVortexL2", TaskWait::Finished),
	    LogGlobals(k_Globals, k_Scripts),
	    // [camera] the prison on the hill: the creature held between the pillars, their freeze beams every 3 seconds and
	    // Lethys's creature tormenting it
	    CameraOn(k_Prison, 40.0f, 60.0f, 10.0f),
	    LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds),
	    LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds),
	};
	// [shortcut] each village wiped out, its buildings and its people, which the quest takes as won (a shortcut of the
	// miracles that destroy it); the pillar it feeds falls at its next 3-second check and its cut scene plays, which is
	// waited on by the count of pillars down, then by the camera over the sinking pillar and back over the village. With
	// all three down the creature is freed
	for (size_t i = 0; i < k_Villages.size(); ++i)
	{
		commands.push_back({.kind = Kind::KnockDownTown,
		                    .delaySeconds = 2.0f,
		                    .thing = testbed_land_timeline::ThingNear {
		                        .type = script::ObjectType::Town, .point = k_Villages.at(i), .radius = k_VillagePickRadius}});
		commands.push_back({.kind = Kind::WaitGlobal,
		                    .waitUntilFree = true,
		                    .amount = static_cast<float>(i + 1),
		                    .global = "PrisonStatuesDeactivated",
		                    .compare = testbed_land_timeline::Compare::AtLeast});
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
		commands.push_back(
		    {.kind = Kind::WaitCamera, .waitUntilFree = true, .point = k_PillarCameras.at(i), .radius = k_PillarCameraRadius});
		if (i < k_VillageCameras.size())
		{
			commands.push_back({.kind = Kind::WaitCamera,
			                    .waitUntilFree = true,
			                    .point = k_VillageCameras.at(i),
			                    .radius = k_VillageCameraRadius});
		}
	}
	// the third pillar's scene: Lethys's call to his creature, the camera over the prison and the good advisor's lines,
	// the freeze lifted and the creature let go
	commands.push_back(WaitText(k_FreedLastLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds));

	all.push_back({
	    .id = "quest.so_you_couldnt_bear",
	    .name = "Land 3 quest run: So You Couldn't Bear to Be Without Your Creature?",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/FreeTheCreature&/GetThroughVortexL2 on "
	                   "Land3.txt). The texts that wait for a click are clicked on. It lets the arrival's film play, "
	                   "then the advisors' remarks half a minute later, waits for the arrival to end, puts the camera "
	                   "over the prison on the hill and logs the prison's state twice, 10 seconds apart; then wipes out "
	                   "the Japanese, Indian and Egyptian villages (buildings and villagers) one at a time, each "
	                   "waited on until its pillar has fallen and its cut scene has come back over the village, and "
	                   "waits for the creature's release.",
	    .expected = "The land's setup loads the player's creature at the prison from the profile's mind file, bars it "
	                "from the temple, finds the four towns, stills the weather round the home town for 10 minutes, "
	                "makes Lethys's wolf at 1.5 times its size and sets Lethys's personality. The prison makes its "
	                "three pillars indestructible and puts the creature on its spot, leash off, indestructible, at a "
	                "tenth of its health. The arrival's film opens the vortex with Nemesis's music, the advisors' "
	                "lines in order, the temple, worship site, storage pit and workshop scaffold, the log entry at 0 "
	                "with its reminder, and ends with the music stopped; the vortex fades and the log reads it closed. At "
	                "the prison every pillar fires its beam and freeze at the creature every 3 seconds and keeps it "
	                "from tiring, and Lethys's creature torments it in turn; the log reads no pillar down. Each village "
	                "destroyed brings its pillar down at its next check, with its cut scene, and the count of "
	                "pillars down rises to 3; the third frees the creature. With the villages destroyed rather than "
	                "won, neither the fanatics' fire attack nor the wolves' attack starts (both need their village "
	                "standing and the player's), and no pillar rises again. "
	                "Unverified: the camera's heights over the hill, and that each cut scene's camera comes within "
	                "the radius waited on.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
