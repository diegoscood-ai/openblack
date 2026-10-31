/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fourth land's Undead Village as a quest run on the game's own Land 4 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): the land entry's quest= starts the land's setup, its arrival without the arrival's film (which
// also brings the player's creature through the vortex), the undead village with its two sunk totems and the two
// watchers over the cursed town, marks the three Guardian Stones broken as the land's earlier quests would, and then
// the village man's scroll and the creature's Creed scene, in the order the land's control script runs them. The
// timeline drives it as a player would: the scroll is pressed on with the hand, the story scene is let play, one totem
// is pulled up out of the ground and let go with the creature beside it, so that the creature pulls it up again once
// it has sunk back, while the hand holds the other at its full height; the curse's end is let play, and the creature
// takes the Creed from the mountain top. The points are map coordinates, taken from the places the quest's scripts
// name; none has yet been seen in a running game.

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
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 15.0f;
/// How near a totem's spot the pick looks for it: the two stand about 100 apart
constexpr float k_TotemPickRadius = 10.0f;
/// A totem the quest makes is an abode of the African totem's info, for a script's types
constexpr uint32_t k_TotemInfo = static_cast<uint32_t>(AbodeInfo::AfricanTotem);

/// The gold scroll at the edge of the Japanese village, on the village man's spot
constexpr glm::vec2 k_Scroll {2590.67f, 3201.2202f};
/// The undead village's two totems, where the land's setup makes them sunk into the ground
constexpr glm::vec2 k_Totem1 {3016.44f, 3584.36f};
constexpr glm::vec2 k_Totem2 {2922.68f, 3550.94f};
constexpr ThingNear k_Totem1Pick {
    .type = script::ObjectType::Abode, .subtype = k_TotemInfo, .point = k_Totem1, .radius = k_TotemPickRadius};
constexpr ThingNear k_Totem2Pick {
    .type = script::ObjectType::Abode, .subtype = k_TotemInfo, .point = k_Totem2, .radius = k_TotemPickRadius};
/// Where the creature is put beside the first totem: 20 beyond it on the line from the second, so that it stands well
/// within the 50 the totem's watcher asks of it and as far from the second totem as it can. Not a place any script names
constexpr glm::vec2 k_BesideTotem1 {3035.28f, 3591.07f};
/// The Creed's spot on the mountain top, and the hilltop beside it where the Creed scene puts the creature
constexpr glm::vec2 k_Creed {2694.475f, 2594.948f};
constexpr glm::vec2 k_CreedHilltop {2707.42f, 2591.429f};
constexpr ThingNear k_CreedPick {.type = script::ObjectType::MobileObject,
                                 .subtype = static_cast<uint32_t>(MobileObjectInfo::Creed),
                                 .point = k_Creed,
                                 .radius = 10.0f};
/// The pull: with the Action button held on the totem, the mouse goes up the screen. The drag stops at the totem's full
/// height, so the sweep is longer than the pull needs; how far a pixel raises it is not worked out here
constexpr glm::vec2 k_PullSweep {0.0f, -0.4f};
constexpr float k_PullSeconds = 2.0f;
/// The camera on a totem: 20 to its south and 60 above the land there, looking at a point 8 above the totem's ground.
/// The first totem's ground is 55 above the land 60 to its south, with a rise between: from there, 40 up, the pointer's
/// line meets the rise about 20 short of the totem. From just in front of the totem and well above it, the line comes
/// down onto the totem past the rise; the second totem is looked at the same way
constexpr float k_TotemEyeHeight = 60.0f;
constexpr float k_TotemEyeBack = 20.0f;
constexpr float k_TotemLookHeight = 8.0f;

/// The globals and scripts the chapter is read by
constexpr std::string_view k_Globals = "LighteningMeteorBroken DarknessMeteorBroken FireMeteorBroken "
                                       "UndeadVillageCutScenePlayed TotemsBothPulledUp PlayerSavedUndeadTown "
                                       "CreatureHasGotCreed";
constexpr std::string_view k_Scripts = "ManExplainsAboutUndeadVillage ChallengeHighlightNotify LookAfterTotems "
                                       "LookAfterTotem CreatureGetsCreed SetUndeadTownHealth SetUndeadTownBeliefToZero";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// The wait for a script global to hold a value
Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
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

/// The player's creature put at a point of the land, as a script's set position puts it
Command PutCreature(glm::vec2 point)
{
	return {.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = point};
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

/// A totem taken hold of with the hand and pulled up: the pointer on the totem, the Action button pressed once the
/// hand is over it, and the mouse moved up the screen with the button held. The button is left down: the caller lets
/// it go
void GripAndPull(std::vector<Command>& commands, const ThingNear& totem)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = totem});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = totem});
	commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 0.4f});
	commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = k_PullSweep, .amount = k_PullSeconds});
	commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = k_PullSeconds + 0.2f});
}
} // namespace

void testbed_scenarios::AddQuestUndeadVillageScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // the scroll's notifier runs once the quest has made the scroll
	    WaitTask("ChallengeHighlightNotify"),
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // [camera] the scroll at the edge of the Japanese village
	    CameraOn(k_Scroll, 25.0f, 30.0f, 3.0f),
	};
	// MISSING: the player's influence over the Japanese village and the undead village, as the land's towns won before
	// the quest would give it (the hand's reach there)
	// [real] the scroll pressed on
	PressScroll(commands, k_Scroll);
	// the story scene: the village man's lines, the village as it was and its curse, the totems shown sinking (the
	// watcher over the totems starts here), and the man's last line; then the camera is given back and the scene ends
	commands.push_back(WaitText(5342));
	commands.push_back(WaitTask("ManExplainsAboutUndeadVillage", TaskWait::Finished));
	// the land's control script runs the Creed's watcher once the scene's script has returned: the quest run's start
	// ends with the scene's script, which waits for the scroll this timeline presses, so the watcher is started here
	commands.push_back({.kind = Kind::StartScript, .script = "CreatureGetsCreed"});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 0.5f});
	// [shortcut] the creature put beside the first totem, where a player would lead it
	commands.push_back(PutCreature(k_BesideTotem1));
	// [camera] the first totem
	commands.push_back(CameraOn(k_Totem1, k_TotemEyeHeight, k_TotemEyeBack, k_TotemLookHeight));
	// [real] the first totem pulled up to its full height and let go: with the creature beside it, it has been raised
	// high enough for the creature to raise it again once it has sunk back into the ground
	GripAndPull(commands, k_Totem1Pick);
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 1.0f});
	// [camera] the second totem
	commands.push_back(CameraOn(k_Totem2, k_TotemEyeHeight, k_TotemEyeBack, k_TotemLookHeight));
	// [real] the second totem pulled up and held at its full height, where a held totem does not sink, until the first
	// has sunk back, the creature has raised it again and both stand at their full height together
	GripAndPull(commands, k_Totem2Pick);
	commands.push_back(WaitGlobal("TotemsBothPulledUp", 1.0f));
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = k_ActionButton});
	// the curse lifted: the freed man's three lines, the Creed made on the mountain top, the flight to it, the log at
	// full and the creature let go
	commands.push_back(WaitText(5410));
	commands.push_back(WaitTask("LookAfterTotems", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [shortcut] the creature put on the hilltop beside the Creed, where a player would lead it
	commands.push_back(PutCreature(k_CreedHilltop));
	// [shortcut] the creature walks up to the Creed and picks it up, as a player would have it do
	commands.push_back({.kind = Kind::PickUp, .delaySeconds = 1.0f, .waitUntilFree = true, .thing = k_CreedPick});
	// the Creed scene: the creature on the hilltop with the glow over its hand, the advisors' lines, the rumble and the
	// good advisor's question; then the scene ends and the creature is let go
	commands.push_back(WaitText(5709));
	commands.push_back(WaitTask("CreatureGetsCreed", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));

	all.push_back({
	    .id = "quest.undead_village",
	    .name = "Land 4 quest run: Undead Village",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(0)/SetupUndeadVillage/"
	                   "SetUndeadTownBeliefToZero&/SetUndeadTownHealth&/LighteningMeteorBroken=1/"
	                   "DarknessMeteorBroken=1/FireMeteorBroken=1/ManExplainsAboutUndeadVillage on Land4.txt). The "
	                   "texts that wait for a click are clicked on. It presses on the gold scroll at the Japanese "
	                   "village with the hand and lets the story scene play, then starts CreatureGetsCreed as the land's "
	                   "control script does once the scene's script returns; puts the creature beside the first "
	                   "totem, pulls that totem up with the hand and lets it go; pulls the second up and holds it until "
	                   "both stand at their full height; lets the curse's end play; then puts the creature beside the "
	                   "Creed and has it pick the Creed up.",
	    .expected = "The scroll starts the scene: the totems raised, the man's three lines, the village alive and "
	                "cursed, the log at 0, the man's two last lines, and the camera given back. Let go, the first totem "
	                "sinks back step by step, and the creature beside it raises it again; with the second held at its "
	                "full height the curse breaks: the freed man's three lines, the Creed made, the flight along the "
	                "camera path and the log at 1. The creature's pick of the Creed starts the Creed scene, which ends "
	                "on the good advisor's question, and CreatureGetsCreed finishes; the end check logs "
	                "PlayerSavedUndeadTown=1 and CreatureHasGotCreed=1. Unverified: that the hand reaches the scroll "
	                "and the totems without the influence the land's won towns would give, that the sweep pulls a "
	                "totem to its full height, that the creature stays within reach of the first totem until it has "
	                "sunk back, and that the creature reaches the Creed on the mountain top.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
