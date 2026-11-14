/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's "Lethys has taken our Creature!" as quest runs: played after a land entry's quest= has run the land's setup
// and started the arrival, which loads the player's creature from their profile and makes it theirs; once the
// arrival's film is over the kidnapping is started, as the land's control script starts it
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). Two runs, one for each way the quest
// ends: the player follows the creature by pressing on the scroll over the vortex in time, or lets the chance go. The
// points are map coordinates, taken from the places the challenge script names; none of them has yet been seen in a
// running game.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// The leash key's place among the leash shortcuts (L)
constexpr size_t k_LeashKey = 0;
/// Where the camera is put for the scroll: this far above the land and back from it, looking at the scroll's height
constexpr float k_EyeAbove = 25.0f;
constexpr float k_EyeBack = 30.0f;
/// The scroll over the vortex is raised this far above the land
constexpr float k_ScrollAbove = 20.0f;
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 15.0f;
/// The vortex Lethys opens in front of his temple, where the follow-now scroll stands too
constexpr glm::vec2 k_Vortex {1073.1617f, 3597.9407f};
/// The texts the timeline waits on: the good advisor's last line of the first film, which ends it and hands control
/// back while the creature walks; and the evil advisor's last line before the chance to follow runs out
constexpr uint32_t k_FirstFilmLastText = 1802;
constexpr uint32_t k_LastLineBeforeTheChance = 1811;
/// How often the quest's state is logged while it runs, whatever the timeline waits for
constexpr size_t k_LogPeriodSeconds = 10;
/// The globals and scripts the quest's state is read by
constexpr std::string_view k_Globals = "LethysGone VortexOpen LeavingLandTwo ChanceMissed";
constexpr std::string_view k_Scripts = "LethysVortex DecidedToGoThrough LethysTauntAI LethysConfrontPlayer";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}

/// Logs the quest's globals and scripts, some seconds after the command before it; with a period, again every period
/// until the run ends
Command LogState(float delay, size_t periodSeconds = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = periodSeconds, .global = k_Globals, .script = k_Scripts};
}

/// A script started as a new task, as the land's control script starts it
Command StartScript(std::string_view script)
{
	return {.kind = Kind::StartScript, .script = script};
}

Command ClickTexts()
{
	return {.kind = Kind::ClickTexts, .value = 1};
}

/// The start both runs share: the texts that wait for a click are clicked on, the arrival's too; once the arrival is
/// over the kidnapping is started; once it has begun, its state is logged every period; then the first film is
/// watched to its end
std::vector<Command> UntilTheWalk()
{
	return {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("LethysVortex"),
	    WaitTask("LethysVortex"),
	    WaitGlobal("LethysGone", 1.0f),
	    LogState(0.0f, k_LogPeriodSeconds),
	    WaitText(k_FirstFilmLastText),
	};
}

/// The scroll over the vortex pressed on with the hand, as a player clicks it: the camera brought to it, the pointer on
/// the drawn scroll, then the Action button pressed once the hand is over it and let go
void PressTheScroll(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::CameraTo,
	                    .delaySeconds = 0.5f,
	                    .eye = {k_Vortex.x, k_EyeAbove, k_Vortex.y - k_EyeBack},
	                    .look = {k_Vortex.x, k_ScrollAbove, k_Vortex.y}});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Vortex, .value = 1});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Vortex, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}
} // namespace

void testbed_scenarios::AddLethysTakesCreatureQuest(std::vector<Scenario>& all)
{
	// Following: the creature is left to walk into the vortex; once the advisors' last line is read, the scroll over
	// the vortex is pressed on with the hand within the ten seconds the quest gives
	auto follow = UntilTheWalk();
	follow.push_back(WaitText(k_LastLineBeforeTheChance));
	PressTheScroll(follow);
	follow.push_back(WaitTask("LethysVortex", TaskWait::Finished));
	follow.push_back(LogState(2.0f));
	all.push_back({
	    .id = "quest.lethys_takes_creature",
	    .name = "Land 2 quest run: Lethys has taken our Creature!, followed",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt): the "
	                   "arrival loads the player's creature from their profile. The texts that wait for a click are "
	                   "clicked on, the arrival's too. Once the arrival is over it starts the kidnapping, as the land's "
	                   "control script does. The quest's globals and scripts are logged every ten seconds once Lethys "
	                   "is gone. The creature is left to walk to the vortex; once the advisors' last line is read, "
	                   "the camera is brought to the vortex and the scroll over it is pressed on with the hand. It ends "
	                   "once the quest is over.",
	    .expected = "Lethys's two films: his hand over his temple, the vortex opening, the beam to the creature and its "
	                "walk into the vortex, the quest's log entry, the advisors' lines; the scroll 20 above the vortex, "
	                "and once it is pressed the land marked as being left (LeavingLandTwo=1), the camera's flight along "
	                "the exit path and the fade to black. Unverified: that the creature can walk all the way up to the "
	                "vortex, that the scroll is in view from the camera put there, and that the arrival's creature is "
	                "loaded from the game copy's profile.",
	    .commands = std::move(follow),
	    .onLand = true,
	});

	// Missing the chance: once the film hands control back, the player tries to lead the creature on the leash (the L
	// key), which the quest answers with the advisor's remark; the scroll is never pressed
	auto missed = UntilTheWalk();
	missed.push_back({.kind = Kind::LeashKey, .delaySeconds = 2.0f, .value = k_LeashKey});
	missed.push_back({.kind = Kind::LeashState, .delaySeconds = 1.0f});
	missed.push_back(WaitText(k_LastLineBeforeTheChance));
	missed.push_back(WaitTask("LethysVortex", TaskWait::Finished));
	missed.push_back(LogState(2.0f));
	all.push_back({
	    .id = "quest.lethys_takes_creature_missed",
	    .name = "Land 2 quest run: Lethys has taken our Creature!, the chance missed",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt), as "
	                   "quest.lethys_takes_creature is, but once the first film hands control back the leash key is "
	                   "pressed to lead the walking creature, and the scroll over the vortex is never pressed. It ends "
	                   "once the quest is over.",
	    .expected = "As quest.lethys_takes_creature until the scroll; on the leash the good advisor's remark that it "
	                "isn't working, once. Then the ten seconds run out: the chance marked missed (ChanceMissed=1), the "
	                "vortex fading out (VortexOpen=0), game time back on and the evil advisor's line about destroying "
	                "Lethys's temple. Unverified: whether the leash takes while the quest has switched it off, and that "
	                "the creature can walk all the way up to the vortex.",
	    .commands = std::move(missed),
	    .onLand = true,
	});
}
