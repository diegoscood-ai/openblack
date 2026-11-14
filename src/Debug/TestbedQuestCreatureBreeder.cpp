/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 1's silver scroll The Creature Breeder as a land timeline on the game's own Land 1 with the SkipBox answered 3
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): the land's control script starts the breeder as it
// runs the skip branch. The timeline presses the scroll over the kennels with the hand, lets the breeder's show of his
// creatures play, leaves the offer alone until it runs out, then presses the scroll again when it comes back. The points
// are map coordinates, taken from the places the quest's script names; none has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedLandTimeline.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

/// The wait for a task of a script: running
constexpr size_t k_Running = 0;
/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// The kennels, where the scroll is put up 5 above the house
constexpr glm::vec2 k_Kennels {1899.27f, 3411.21f};
/// The scroll comes 10 seconds after the breeder starts, and 10 seconds after each offer closes
constexpr float k_ScrollSeconds = 12.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "BreederRunning GlobalCurrentCreature GlobalOldCreature NrOfChoosenCreature";
constexpr std::string_view k_Scripts = "CreatureBreeder SpecialStored SwapCreaturesAtBreeder ChallengeHighlightNotify";

Command Log(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}

/// The camera 30 south of the kennels and 20 up, looking at them, once the scroll has come
Command CameraOnTheKennels()
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = k_ScrollSeconds,
	        .eye = {k_Kennels.x, 20.0f, k_Kennels.y - 30.0f},
	        .look = {k_Kennels.x, 5.0f, k_Kennels.y}};
}

/// The scroll over the kennels pressed on with the hand: the pointer on the drawn scroll, the Action button pressed once
/// the hand is over it and let go, as a player clicks it
void PressTheScroll(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Kennels, .value = 1});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Kennels, .radius = 15.0f}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

std::vector<Command> TwoVisits()
{
	std::vector<Command> commands {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::WaitTask, .waitUntilFree = true, .value = k_Running, .script = "CreatureBreeder"},
	    Log(0.0f, 20),
	    // [camera] the kennels, once the scroll is up
	    CameraOnTheKennels(),
	};
	// [real] the scroll pressed on: the show of the creatures, then their offer
	PressTheScroll(commands);
	commands.push_back(WaitGlobal("BreederRunning", 1.0f));
	// the offer is left alone until its time runs out
	commands.push_back(WaitGlobal("BreederRunning", 0.0f));
	commands.push_back(Log(1.0f));
	// [camera] the kennels again; [real] the scroll pressed on when it is back: the show again
	commands.push_back(CameraOnTheKennels());
	PressTheScroll(commands);
	commands.push_back(WaitGlobal("BreederRunning", 1.0f));
	commands.push_back(Log(10.0f));
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestCreatureBreederScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.q_creature_breeder",
	    .name = "Land 1: The Creature Breeder, two visits",
	    .facet = Facet::Story,
	    .description = "On Land 1 with the SkipBox answered 3, where the land's control script starts the breeder: the "
	                   "texts that wait for a click are clicked on. Once the scroll over the kennels is up it is pressed "
	                   "on with the hand; the breeder's show is let play and his offer left alone until it runs out; the "
	                   "scroll is pressed on again when it comes back.",
	    .expected = "The breeder is made at his start, unhurtable and not to be picked up; 10 seconds on the scroll stands "
	                "5 above the kennels. The press brings the cut scene: the breeder at his soap box, the camera on the "
	                "kennels, every special creature but the player's own species made in a row with a sparkle, the "
	                "two-subject camera and text 5097; each creature gets its swap scroll (BreederRunning=1). Once the "
	                "offer runs out (BreederRunning=0) the scroll comes back 10 seconds later and the second press shows "
	                "the creatures again.",
	    .commands = TwoVisits(),
	    .onLand = true,
	});
}
