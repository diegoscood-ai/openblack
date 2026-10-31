/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Destroy it!, Land 2's last gold scroll, as quest runs: started directly, its scroll on the shore is pressed on with
// the hand, the fly-over of Lethys's three towns plays, and the log entry is then updated every 5 seconds. The other
// runs take the branches the land as it starts can't: Lethys's towns won from him by belief before the press, all
// three or only the two the fly-over visits one by one, and the land's exit begun before the scroll is made or while it
// waits. The points are map coordinates, taken from the place the challenge script names and the towns the land makes;
// not yet seen in a running game.

#include <cstddef>

#include <array>
#include <optional>
#include <span>
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
/// The scroll on the shore west of the Indian town by the bay
constexpr glm::vec2 k_FinalScroll {2571.77f, 3068.29f};
/// The challenge the scroll's log entry is kept under
constexpr size_t k_Challenge = 91;
/// Lethys's three towns, where the land makes them: the near Celtic town, the one on the left and his home town on the
/// right, as the fly-over looks at them
constexpr glm::vec2 k_NearTown {1800.44f, 3024.32f};
constexpr glm::vec2 k_LeftTown {1258.37f, 3022.56f};
constexpr glm::vec2 k_RightTown {1333.36f, 3379.90f};
/// The belief in the player a town of Lethys's takes to be won: more than the most the land lets it hold in Lethys
/// (three quarters), so its next fold makes it the player's
constexpr float k_WinningBelief = 2.0f;
/// How long the towns are left to fold their new belief before the scroll is pressed on
constexpr float k_FoldSeconds = 5.0f;
/// How long after the fly-over the log entry's updates are watched: two of its 5-second recounts
constexpr float k_RecountsWatched = 12.0f;
/// How many times the entry is replayed from the log after the first fly-over: each replay draws the advisors' lines
/// again, so that the evil advisor's butt-in after a good advisor's line has a chance to be seen
constexpr int k_Replays = 3;
/// How long after the temple fly-over starts the log is tapped for the entry: after the evil advisor's line (2 seconds
/// in), before the fade (once the line is read)
constexpr float k_TapDuringFilm = 3.0f;
constexpr std::string_view k_FinalScrollGlobals = "DoneExitLethysVortex LeavingLandTwo";
constexpr std::string_view k_FinalScrollScripts =
    "Land2FinalScroll Land2FinalScrollCinema ChallengeHighlightNotifyL2FinalScroll";
/// Seconds between the quest's recounts of the towns, each of which updates the log entry, and a little more so that
/// each log comes after one
constexpr float k_UpdateSeconds = 5.0f;
constexpr float k_UpdateMargin = 1.0f;
/// Seconds between the logs that watch the chapter for the whole of the first run, whatever the timeline waits for
constexpr size_t k_WatchPeriod = 30;
/// The globals and scripts the first run reads the chapter by: how many of the three towns Lethys still holds, whether
/// the left and the right one are still his, whether the land's exit has begun, and the quest's scripts
constexpr std::string_view k_ChapterGlobals =
    "LethysTownTotal TownOnLeftRequired TownOnRightRequired DoneExitLethysVortex LeavingLandTwo";
constexpr std::string_view k_ChapterScripts =
    "Land2FinalScroll Land2FinalScrollCinema ChallengeHighlightNotifyL2FinalScroll UpdateLethysTownTotal";

/// Logs the globals and scripts named, then again every period until the scenario ends
[[nodiscard]] Command WatchGlobals(std::string_view globals, std::string_view scripts, size_t period)
{
	return {.kind = Kind::LogGlobals, .value = period, .global = globals, .script = scripts};
}

[[nodiscard]] Command TownWon(glm::vec2 town, float delay)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = delay,
	        .amount = k_WinningBelief,
	        .thing = ThingNear {.type = script::ObjectType::Town, .point = town, .radius = 10.0f}};
}

[[nodiscard]] Command Replay(float delay)
{
	return {.kind = Kind::ReplayChallenge, .delaySeconds = delay, .value = k_Challenge};
}

/// The camera on the scroll, then the scroll pressed on, a delay after the command before
void PressFinalScroll(std::vector<Command>& commands, float delay)
{
	auto camera = CameraOn(k_FinalScroll, k_EyeAbove, k_EyeBack, 0.0f);
	camera.delaySeconds = delay;
	commands.push_back(camera);
	PressScroll(commands, k_FinalScroll);
}

/// The quest run once the towns named are the player's, won by belief before the press; the entry may be replayed from
/// the log while the fly-over plays, a delay after it starts
std::vector<Command> TownsWonRun(std::span<const glm::vec2> towns, std::optional<float> tapDuringFilm = std::nullopt)
{
	std::vector<Command> commands {ClickTexts(), WaitTask("ChallengeHighlightNotifyL2FinalScroll")};
	for (const auto town : towns)
	{
		commands.push_back(TownWon(town, 0.5f));
	}
	PressFinalScroll(commands, k_FoldSeconds);
	if (tapDuringFilm.has_value())
	{
		commands.push_back(WaitTask("Land2FinalScrollCinema"));
		commands.push_back(Replay(*tapDuringFilm));
	}
	commands.push_back(WaitTask("Land2FinalScrollCinema", TaskWait::Finished));
	return commands;
}
} // namespace

void testbed_scenarios::land2_quests::AddDestroyItScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the scroll's notifier runs once the quest has laid the scroll
	    WaitTask("ChallengeHighlightNotifyL2FinalScroll"),
	    WatchGlobals(k_ChapterGlobals, k_ChapterScripts, k_WatchPeriod),
	};
	PressFinalScroll(commands, 1.0f);
	// the fly-over: the evil advisor's line on the temple (6937), then the camera over the near town and the good
	// advisor's line (2094), after which the log entry is recorded; while Lethys still holds them, the left and the
	// right town are shown with an advisor's remark each, then the fade to black and the camera put back
	commands.push_back(WaitText(6937));
	commands.push_back(WaitText(2094));
	commands.push_back(WaitTask("Land2FinalScrollCinema", TaskWait::Finished));
	// what the player must do: every 5 seconds the towns are recounted and the log entry's success updated; two
	// updates are let pass, each logged after it
	commands.push_back(LogGlobals(k_ChapterGlobals, k_ChapterScripts, k_UpdateSeconds + k_UpdateMargin));
	commands.push_back(LogGlobals(k_ChapterGlobals, k_ChapterScripts, k_UpdateSeconds));
	for (int i = 0; i < k_Replays; ++i)
	{
		commands.push_back(Replay(1.0f));
		commands.push_back(WaitTask("Land2FinalScrollCinema", TaskWait::Finished));
	}
	commands.push_back(LogGlobals(k_ChapterGlobals, k_ChapterScripts, 1.0f));
	all.push_back({
	    .id = "quest.destroy_it",
	    .name = "Land 2 quest run: Destroy it!",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/Land2FinalScroll on Land2.txt). The texts that "
	                   "wait for a click are clicked on. Once the scroll's notifier runs it puts the camera on the "
	                   "scroll on the shore, presses on it with the hand, lets the fly-over over Lethys's three towns "
	                   "play, lets two of the quest's 5-second updates pass, each logged after it, then replays the "
	                   "entry from the log three times.",
	    .expected = "The press starts the fly-over: the camera looks west across the water, the evil advisor's line on "
	                "the temple, the near town, the good advisor's line and the log entry \"Destroy it!\" recorded at 0 "
	                "(Lethys holds all three towns at the land's start), then the towns on the left and the right with "
	                "an advisor's remark on each, while Lethys holds them; the screen fades and the camera is put back. "
	                "Afterwards the entry is updated every 5 seconds, still at 0; the logs read three towns held, both "
	                "left and right still his, DoneExitLethysVortex=0 and Land2FinalScroll still running. Each replay "
	                "plays the whole fly-over again and records the entry again; after a good advisor's last line the "
	                "evil advisor butts in.",
	    .commands = std::move(commands),
	    .onLand = true,
	});

	// Lethys's three towns won from him before the press: the fly-over takes the temple's branch and records nothing,
	// and the entry is first made by the update that follows it, which a replay during the fly-over and one after it
	// tell apart. The quest's wait between updates counts from its last test, which it passed before the fly-over, so
	// the first update comes as soon as the fly-over ends
	constexpr std::array k_AllTowns {k_NearTown, k_LeftTown, k_RightTown};
	auto gone = TownsWonRun(k_AllTowns, k_TapDuringFilm);
	gone.push_back(Replay(1.0f));
	gone.push_back(WaitTask("Land2FinalScrollCinema", TaskWait::Finished));
	gone.push_back(LogGlobals(k_FinalScrollGlobals, k_FinalScrollScripts, k_RecountsWatched));
	all.push_back({
	    .id = "quest.destroy_it_towns_gone",
	    .name = "Land 2 quest run: Destroy it!, Lethys's towns won first",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/Land2FinalScroll on Land2.txt). The texts that "
	                   "wait for a click are clicked on. Once the scroll's notifier runs, Lethys's three towns take more "
	                   "belief in the player than he can hold in them; after their fold the scroll is pressed on, the "
	                   "entry is replayed from the log while the fly-over plays, and again after it.",
	    .expected = "The towns become the player's. The fly-over goes from the scroll straight to Lethys's temple with "
	                "the evil advisor's line, then the fade and the camera put back; the replay during it finds no "
	                "entry, the update as it ends makes it at full success, and the replay after it plays the temple "
	                "fly-over again.",
	    .commands = std::move(gone),
	    .onLand = true,
	});

	// Only the towns on the left and the right won: Lethys keeps the near town, so the fly-over records the entry but
	// visits neither town, and the butt-in's test reads a line that was never drawn
	constexpr std::array k_SideTowns {k_LeftTown, k_RightTown};
	auto sideTowns = TownsWonRun(k_SideTowns);
	sideTowns.push_back(LogGlobals(k_FinalScrollGlobals, k_FinalScrollScripts, k_RecountsWatched));
	all.push_back({
	    .id = "quest.destroy_it_near_town_only",
	    .name = "Land 2 quest run: Destroy it!, only the near town left to Lethys",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/Land2FinalScroll on Land2.txt). The texts that "
	                   "wait for a click are clicked on. Once the scroll's notifier runs, the towns on the left and the "
	                   "right take more belief in the player than Lethys can hold in them; after their fold the scroll is "
	                   "pressed on.",
	    .expected = "The fly-over shows the near town with both advisors' lines and records the entry at two thirds, "
	                "goes to neither of the other towns, and asks whether the good advisor speaks the empty line, which "
	                "he does not, so the evil advisor does not butt in.",
	    .commands = std::move(sideTowns),
	    .onLand = true,
	});

	// The land's exit begun: before the quest starts (the quest run sets the flag first), then, on a second start,
	// while the scroll waits for its click
	std::vector<Command> exitBegun {
	    LogGlobals(k_FinalScrollGlobals, k_FinalScrollScripts, 1.0f),
	    CameraOn(k_FinalScroll, k_EyeAbove, k_EyeBack, 0.0f),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_FinalScroll, .value = 1},
	    {.kind = Kind::SetGlobal, .amount = 0.0f, .global = "DoneExitLethysVortex"},
	    StartScript("Land2FinalScroll"),
	    WaitTask("ChallengeHighlightNotifyL2FinalScroll"),
	    {.kind = Kind::SetGlobal, .delaySeconds = 1.0f, .amount = 1.0f, .global = "DoneExitLethysVortex"},
	    // the notifier, already seen running, may end in the turn the flag is set, before this wait first looks
	    WaitTask("ChallengeHighlightNotifyL2FinalScroll", TaskWait::NotRunning),
	    LogGlobals(k_FinalScrollGlobals, k_FinalScrollScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.destroy_it_exit_begun",
	    .name = "Land 2 quest run: Destroy it!, the land's exit begun",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start with the exit flag set first "
	                   "(quest=SetupLand2/DoneExitLethysVortex=1/Land2FinalScroll on Land2.txt). It logs the quest's "
	                   "state, points the hand at the scroll's spot, then clears the flag, starts the quest again and "
	                   "sets the flag once the scroll's notifier runs.",
	    .expected = "With the flag set first the quest ends at once and no scroll is made: the hand at its spot is over "
	                "no scroll. Started again, the scroll appears; setting the flag ends its wait, the towns are "
	                "counted, and the quest ends without the fly-over or the log entry.",
	    .commands = std::move(exitBegun),
	    .onLand = true,
	});
}
