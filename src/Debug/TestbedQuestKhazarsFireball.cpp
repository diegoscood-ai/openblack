/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Khazar's Fireball Challenge as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): the land entry's quest= starts the land's setup and then the Miracle Challenge scroll Khazar
// leaves by the worship site once the worship lesson is over, at the spot the lesson gives it. The timeline drives it
// as a player would: the scroll is pressed on with the hand, the tour is let play, the scroll on the target huts is
// pressed on, the intro and the fireball hand demo are let play, and each of the three fireball globes is tapped into
// the hand and thrown at the middle hut. The points are map coordinates, taken from the places the
// challenge's, the worship lesson's and the hand demos' scripts name; none has yet been seen in a running game.

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
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 15.0f;
/// How near a globe's own landing spot the tap looks for it: the three lie about 3.5 apart, and the nearest is taken
constexpr float k_GlobePickRadius = 3.0f;

/// The Miracle Challenge scroll's spot, where the worship lesson's end flies Khazar's hand and starts the scroll; the
/// challenge raises the scroll 6 above it
constexpr glm::vec2 k_MiracleScroll {3334.112f, 3165.987f};
constexpr float k_MiracleScrollHeight = 6.0f;
/// The middle target hut, where the fireball challenge's scroll is made
constexpr glm::vec2 k_HutScroll {3428.18f, 2975.42f};
/// The three fireball globes, by the spots the challenge watches them at once they have settled; each is a one-shot
/// spell on the land, which the testbed's pick finds as a mobile object
constexpr ThingNear k_Globe1 {
    .type = script::ObjectType::MobileObject, .point = {3246.5798f, 3165.6497f}, .radius = k_GlobePickRadius};
constexpr ThingNear k_Globe2 {
    .type = script::ObjectType::MobileObject, .point = {3244.058f, 3163.129f}, .radius = k_GlobePickRadius};
constexpr ThingNear k_Globe3 {
    .type = script::ObjectType::MobileObject, .point = {3249.1016f, 3168.1802f}, .radius = k_GlobePickRadius};
/// Each globe is thrown at the middle hut through the hand's own throw: its run-up's speed is worked out for a flight
/// under the physics' gravity, while a fireball flies by its own rules, so where it lands is unverified
constexpr glm::vec2 k_ThrowTarget = k_HutScroll;
/// Seconds left for each fireball to fly before the next globe is tapped
constexpr float k_FlightSeconds = 4.0f;

/// The globals and scripts the chapter is read by
constexpr std::string_view k_Globals = "KhazarInScript KhazarGone LandNumber";
constexpr std::string_view k_Scripts = "MiracleChallenges FireBallChallenge PhysicalShieldChallenge "
                                       "CheckForKhazarChallengeHighlightNotify Fireball";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// The wait for a text to have been shown and read
/// A text shown and said to its end, as the scripts' own wait until read takes it: a press made while a tip is still
/// being said lands in its dialogue
Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true, .voice = true};
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

/// A gold scroll pressed on with the hand: the pointer on the drawn scroll, then the Action button pressed once the
/// hand is over it and let go, as a player clicks it
void PressScroll(std::vector<Command>& commands, glm::vec2 scroll)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// A fireball globe tapped into the hand, then thrown: the pointer on the globe and the Action button clicked once the
/// hand is over it, which puts its fire seed in the hand; then, from where the globe was, the button held, the hand
/// moved towards the hut and the button let go as it moves, as the hand demo shows a fireball thrown
void TapAndThrow(std::vector<Command>& commands, const ThingNear& globe, float delay = 1.0f)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .thing = globe});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = globe});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = k_ThrowTarget});
}
} // namespace

void testbed_scenarios::AddQuestKhazarsFireballScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the scroll's notifier runs once the challenge has made the scroll
	    WaitTask("CheckForKhazarChallengeHighlightNotify"),
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // [camera] the scroll, raised over its spot
	    CameraOn(k_MiracleScroll, 25.0f, 30.0f, k_MiracleScrollHeight),
	};
	// [real] the Miracle Challenge scroll pressed on (the notifier's click test, in the land's setup)
	PressScroll(commands, k_MiracleScroll);
	// the tour: both challenges are started and Khazar shows their places; its last line (4488), then the film's end and
	// Khazar's release
	commands.push_back(WaitText(4488));
	commands.push_back(WaitTask("MiracleChallenges", TaskWait::NotRunning));
	// [camera] the middle hut, the fireball challenge's scroll on it
	commands.push_back(CameraOn(k_HutScroll, 25.0f, 30.0f, 3.0f));
	// [real] the hut's scroll pressed on (the same notifier's click test)
	PressScroll(commands, k_HutScroll);
	// the intro: Khazar's lines on the huts (4531) and the globes (4546), then the fireball hand demo's lines (5123 to
	// 5126); the film ends and the good advisor's tip is said at once (4532). The camera is left where the demo put it,
	// behind the globes looking out to the huts
	commands.push_back(WaitText(4531));
	commands.push_back(WaitText(4546));
	commands.push_back(WaitText(5126));
	commands.push_back(WaitText(4532));
	// [real] the three globes tapped and thrown one at a time: a hand holding a seed takes no other. The first wait ends only
	// once all three globes are gone: a seed tapped from a globe is not the plain fire seed it looks for
	TapAndThrow(commands, k_Globe1);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_FlightSeconds));
	TapAndThrow(commands, k_Globe2);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_FlightSeconds));
	TapAndThrow(commands, k_Globe3);
	// the round is counted once the globes are gone and the hand is empty, 7 seconds on. A hit shows the huts and ends
	// on the fire-spreads line (4537), then the reward's dialogue ends on the evil advisor's line (6921). After a miss
	// (4540) new globes come instead, which this chapter does not
	// throw: the run then stops at this wait, which the round's log shows
	commands.push_back(WaitText(4537));
	commands.push_back(WaitText(6921));
	commands.push_back(WaitTask("FireBallChallenge", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));

	all.push_back({
	    .id = "quest.khazars_fireball_challenge",
	    .name = "Land 2 quest run: Khazar's Fireball Challenge",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/MiracleChallenges(marker@3334.112;76.487;"
	                   "3165.987) on Land2.txt). The texts that wait for a click are clicked on. It presses on the "
	                   "Miracle Challenge scroll with the hand, lets Khazar's tour play, presses on the scroll on the "
	                   "middle target hut, lets the intro and the fireball hand demo play, then taps each of the three "
	                   "fireball globes into the hand and throws it at the middle hut.",
	    .expected = "The tour starts both challenges and shows their places; the hut's scroll starts the intro: the "
	                "huts mended, the log at 0, three globes, the hand demo and the tap tip. A hut hit by a fireball "
	                "ends the round with the evil advisor's result and the fire-spreads line, then the Fireball miracle "
	                "is given to the home town, Khazar's three lines and the evil advisor's, and FireBallChallenge "
	                "finishes; the end check logs KhazarInScript=0. Unverified: that the throw reaches the huts "
	                "(a miss leaves the run at the wait for 4537, with new globes laid out), that the hand reaches the "
	                "scrolls (the hut's lies inside the anti-influence ring round the first hut), and that the hand "
	                "demo leaves the hand empty.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
