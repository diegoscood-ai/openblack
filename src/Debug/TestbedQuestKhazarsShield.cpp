/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Khazar's Shield Challenge as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): the land entry's quest= starts the land's setup and then the Miracle Challenge scroll Khazar
// leaves by the worship site once the worship lesson is over, at the spot the lesson gives it, which starts both of his
// challenges. The timeline drives the shield challenge as a player would: the Miracle Challenge scroll is pressed on
// with the hand, the tour is let play, the scroll on the island hut is pressed on, the intro and the shield hand demo
// are let play, the three practice rocks are thrown at the shielded hut, a shield seed is tapped into the hand and a
// circle drawn round the hut, and Khazar's attack and the reward are let play. The points are map coordinates, taken
// from the places the challenge's, the worship lesson's and the hand demos' scripts name; none has yet been seen in a
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
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 10.0f;

/// The Miracle Challenge scroll's spot, where the worship lesson's end flies Khazar's hand and starts the scroll; the
/// challenge raises the scroll 6 above it
constexpr glm::vec2 k_MiracleScroll {3334.112f, 3165.987f};
constexpr float k_MiracleScrollHeight = 6.0f;
/// The island hut, where the shield challenge's scroll is made at the hut's own height
constexpr glm::vec2 k_Hut {3619.57f, 3457.72f};
/// The three practice rocks, made on the mainland 2 apart: mobile statics of the challenge's rock kind, which the
/// scripts see as rocks, each picked within 1 of its spot (they lie about 2.8 apart)
constexpr float k_RockPickRadius = 1.0f;
constexpr ThingNear k_Rock1 {.type = script::ObjectType::Rock, .point = {3539.0312f, 3432.0298f}, .radius = k_RockPickRadius};
constexpr ThingNear k_Rock2 {.type = script::ObjectType::Rock, .point = {3541.0312f, 3434.0298f}, .radius = k_RockPickRadius};
constexpr ThingNear k_Rock3 {.type = script::ObjectType::Rock, .point = {3537.0312f, 3430.0298f}, .radius = k_RockPickRadius};
/// The second shield seed, the one of the three inside the ring of influence round the hut; a one-shot spell on the
/// land, which the testbed's pick finds as a mobile object
constexpr glm::vec2 k_Seed2Spot {3602.7354f, 3423.8164f};
constexpr ThingNear k_Seed2 {.type = script::ObjectType::MobileObject, .point = k_Seed2Spot, .radius = 1.5f};
/// How long the Action button is held on a rock to pick it up, past the hand's 225 ms grab wait
constexpr float k_GrabHoldSeconds = 0.6f;
/// Seconds left for each rock to fly before the next is taken
constexpr float k_FlightSeconds = 2.0f;

/// The globals and scripts the chapter is read by
constexpr std::string_view k_Globals = "KhazarInScript KhazarGone LandNumber";
constexpr std::string_view k_Scripts = "MiracleChallenges PhysicalShieldChallenge FireBallChallenge "
                                       "CheckForKhazarChallengeHighlightNotify PShield";

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
Command CameraOn(glm::vec2 point, float eyeHeight, float back, float lookHeight, float delay = 1.0f)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
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

/// A rock taken and thrown at the hut: the pointer on the rock, the Action button pressed once the hand is over it and
/// held past the grab's wait, which picks it up (a quick tap would split it instead), and let go, the rock kept in the
/// hand; then the hand's own throw at the hut: the button pressed again, the hand moved towards the hut and the button
/// let go as it moves. The run-up's speed is worked out for a flight under gravity without the air's drag, so the rock
/// lands near the hut; that it hits the shield is unverified
void GrabAndThrow(std::vector<Command>& commands, const ThingNear& rock, float delay = 0.5f)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .thing = rock});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .waitUntilFree = true, .value = k_ActionButton, .thing = rock});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_Hut});
}
} // namespace

void testbed_scenarios::AddQuestKhazarsShieldScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the Miracle Challenge scroll's notifier runs once the challenge has made the scroll
	    WaitTask("CheckForKhazarChallengeHighlightNotify"),
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // [camera] the Miracle Challenge scroll, raised over its spot
	    CameraOn(k_MiracleScroll, 25.0f, 30.0f, k_MiracleScrollHeight),
	};
	// [real] the Miracle Challenge scroll pressed on (the notifier's click test, in the land's setup): it starts both
	// challenges and the tour
	PressScroll(commands, k_MiracleScroll);
	// the tour's last line (4488), then the film's end and Khazar's release
	commands.push_back(WaitText(4488));
	commands.push_back(WaitTask("MiracleChallenges", TaskWait::NotRunning));
	// [camera] the island hut, the shield challenge's scroll on it
	commands.push_back(CameraOn(k_Hut, 25.0f, 30.0f, 3.0f));
	// [real] the hut's scroll pressed on (the same notifier's click test)
	PressScroll(commands, k_Hut);
	// the intro: Khazar's lines (4565, 4566, 4567), the shield hand demo's (5119, 5120, 5122), then the line after the
	// film (4568). The camera is left where the intro put it, behind the rocks looking towards the hut, with the hand's
	// interaction only
	commands.push_back(WaitText(4565));
	commands.push_back(WaitText(4567));
	commands.push_back(WaitText(5122));
	commands.push_back(WaitText(4568));
	// [real] the three rocks taken and thrown one at a time at the hut, within the 20 seconds the challenge watches
	// Khazar's shield for a hit, which start once the throw line has been said (the challenge clears every hit seen
	// before)
	GrabAndThrow(commands, k_Rock1);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_FlightSeconds));
	GrabAndThrow(commands, k_Rock2);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_FlightSeconds));
	GrabAndThrow(commands, k_Rock3);
	// the result: a hit or a broken shield (4569) or 20 seconds without (4572), both ending on the player's turn
	// (4570); then the seeds, and the good advisor's tap tip at once (4550)
	commands.push_back(WaitText(4570));
	commands.push_back(WaitText(4550));
	// [camera] the second seed, the one inside the hut's ring of influence, once the tap tip has been said
	commands.push_back(CameraOn(k_Seed2Spot, 20.0f, 20.0f, 0.0f));
	// [real] the seed tapped into the hand: the pointer on it and the Action button clicked once the hand is over it,
	// which puts its shield seed in the hand
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_Seed2});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = k_Seed2});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	// [camera] the hut in view, where the circle is drawn
	commands.push_back(CameraOn(k_Hut, 40.0f, 30.0f, 0.0f));
	// [real] a circle drawn round the hut, centred on where it is on the screen, with the Action button held, which
	// casts the shield seed in the hand as a shield there
	commands.push_back(
	    {.kind = Kind::DrawGesture, .delaySeconds = 1.0f, .point = k_Hut, .value = static_cast<size_t>(GestureType::Circle)});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 3.0f));
	// the drawing tip once the seed is held or a shield stands (4552); with a shield over the hut the good advisor's
	// praise (4555); Khazar's attack (4553); the result (4556 or 4558) and the reward's last line (4554); then
	// Khazar's release
	commands.push_back(WaitText(4552));
	commands.push_back(WaitText(4553));
	commands.push_back(WaitText(4554));
	commands.push_back(WaitTask("PhysicalShieldChallenge", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));

	all.push_back({
	    .id = "quest.khazars_shield_challenge",
	    .name = "Land 2 quest run: Khazar's Shield Challenge",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/MiracleChallenges(marker@3334.112;76.487;"
	                   "3165.987) on Land2.txt). The texts that wait for a click are clicked on. It presses on the "
	                   "Miracle Challenge scroll with the hand, lets Khazar's tour play, presses on the scroll on the "
	                   "island hut, lets the intro and the shield hand demo play, takes each of the three rocks and "
	                   "throws it at the hut, then taps the second shield seed into the hand and "
	                   "draws a circle round the hut, and lets Khazar's attack and the reward play.",
	    .expected = "The tour starts both challenges; the hut's scroll starts the intro: the ring of influence, the hut "
	                "mended, the log at 0, three rocks, the hand demo's shield over the hut and the throw line. The "
	                "result says the hut is protected (or, without a hit in 20 seconds, that the shield deflects), "
	                "three seeds come, the log moves to 0.3 and the tap tip is said; the drawing tip, the good "
	                "advisor's praise if the shield stands within 5 of the hut, Khazar's three boulders, the result, "
	                "the Physical Shield miracle given to the home town and Khazar's two lines; PhysicalShieldChallenge "
	                "finishes and the end check logs KhazarInScript=0. Unverified: that the throws reach the "
	                "shield, that the rocks are in the hand's reach, that the circle drawn round the hut on the screen "
	                "puts the shield within 5 of the hut (without it there is no praise, and the boulders hit the hut), "
	                "and that the hand demo leaves the hand empty.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
