/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fourth land's "The Defending Ogres" as a quest run on the game's own Land 4 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"): the land entry's quest= starts the land's setup and its arrival with the arrival's
// film (which also brings the player's creature through the vortex and starts the three curses), then, in the order
// the land's control script starts them, the advice on building up the village, Sleg in his lair, the watcher that
// keeps an unleashed creature away from him, and the frightened man's story, which logs the scroll. The timeline drives
// it as a player would: the arrival has played by the time it starts, the camera is turned away from the man's spot by
// the temple and the five minutes before he comes are let pass without exploring, his gold scroll is pressed on with
// the hand and his story is let play; then the creature is put on the leash and on its side of the fighting ground before
// Sleg's lair, Sleg's challenge is let play, the creature fights him by itself, and the stone's breaking is let play. The
// points are map coordinates, taken from the places the quest names; none has yet been seen in a running game.

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

/// The gates by the home village: the camera looks north over them while the man is awaited, which keeps his spot by
/// the temple (to the south-east) out of view, and keeps the camera far from the Japanese village and from Sleg's lair,
/// the two places whose nearness counts as exploring too early
constexpr glm::vec2 k_Gates {2164.2515f, 2889.9802f};
/// The man's gold scroll, beside his spot by the temple, drawn at the land's height
constexpr glm::vec2 k_Scroll {2390.8188f, 2572.488f};
/// The creature's side of the fighting ground before Sleg's lair, where Sleg's challenge puts it: well within the
/// reach of the fighting ground that wakes Sleg
constexpr glm::vec2 k_CreatureFightSide {1535.906f, 2624.728f};

/// Sleg's fight health (a script property) set to none this many times in the fight, this far apart
constexpr size_t k_FightHealthDrops = 5;
constexpr float k_FirstDropSeconds = 20.0f;
constexpr float k_SecondsBetweenDrops = 15.0f;
constexpr size_t k_FightHealthProperty = 33;

/// The globals and scripts the chapter is read by
constexpr std::string_view k_Globals = "IntroFlybyFinished IllegalExplore CreatureVenturedTooFar "
                                       "ManHasExplainedAboutNemesis OgreInFightSequence OgreDefeated "
                                       "LighteningMeteorBroken FireMeteorWaveActive ForceMeteorWaveActive "
                                       "LaunchGremlins VortexOpen LethysKilled";
constexpr std::string_view k_Scripts = "BeginLand4 VortexEntry BuildUpTown Land4Ogre KeepCreatureAwayFromOgre "
                                       "ManExplainsAboutNemesis StopPlayerExploring ChallengeHighlightNotify "
                                       "OgreFamilyControl FightEndCinema Land4MeteoritesControl";

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
} // namespace

void testbed_scenarios::AddQuestDefendingOgresScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    // the lines said with a click to go on are clicked on
	    ClickTexts(),
	    // the quest run's start has played the arrival's film to its end before this timeline starts (the land's
	    // control script waits for the arrival before it starts the rest), so the arrival is over here and the man's
	    // five minutes have begun
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // [camera] over the gates, looking north: the man's spot by the temple is behind the camera, and the camera is
	    // far from the Japanese village and from Sleg's lair, so the player is not exploring too early
	    CameraOn(k_Gates, 40.0f, 30.0f, 5.0f),
	    // half a minute into the land the advisors talk about building up the village
	    WaitText(5293),
	    // five minutes after the arrival, once his spot is out of view, the man appears by the temple, kneels to pray
	    // and his gold scroll is put up beside him, with its notifier
	    WaitTask("ChallengeHighlightNotify"),
	    LogGlobals(k_Globals, k_Scripts),
	    // [camera] the man's scroll by the temple
	    CameraOn(k_Scroll, 25.0f, 30.0f, 3.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Scroll);
	// the man's story, with the fireballs forced on: his plea and his village, the land as it was and the tree blown
	// up, the curses over the land, the road and the mountains, the three Guardian Stones (the scroll is logged at 0
	// over Sleg's lair), and his last lines; then he stands, the scene ends, he sets off home and the gremlins are made
	commands.push_back(WaitText(5347));
	commands.push_back(WaitText(5352));
	commands.push_back(WaitText(5358));
	commands.push_back(WaitText(5363));
	commands.push_back(WaitText(5366));
	commands.push_back(WaitText(5370));
	commands.push_back(WaitText(5372));
	commands.push_back(WaitText(5375));
	commands.push_back(WaitTask("ManExplainsAboutNemesis", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [shortcut] the creature put on the leash, as the player would to lead it to Sleg
	commands.push_back({.kind = Kind::PutOnLeash, .delaySeconds = 1.0f, .leash = LeashType::Rope});
	// [shortcut] the creature put on its side of the fighting ground before Sleg's lair, where the player would lead
	// it on the leash
	commands.push_back(PutCreature(k_CreatureFightSide));
	// MISSING: the creature led on the leash along the road to the fighting ground (the hand holding the leash and
	// pulling the creature towards a point of the land), so that its arrival is the player's
	// Sleg's challenge: the leash is taken off, the camera swings to the fighting ground, Sleg walks out to his side
	// and, the first time, gives his name, his stone and his challenge in his own voice; then the leash is switched
	// off and he starts the fight
	commands.push_back(WaitText(5424));
	commands.push_back(WaitText(5427));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	// [real] the player leaves the fight to the creature, which fights by itself once no order has come for a while
	// [shortcut] Sleg's fight health set to none five times, 15 seconds apart, from 20 seconds after his challenge, as
	// a script's SET_PROPERTY does, so that the creature's next blow knocks him out: left to itself the creature, about
	// three quarters of his strength, loses (a creature gap, docs/wiki/creature/fights.md)
	for (size_t drop = 0; drop < k_FightHealthDrops; ++drop)
	{
		commands.push_back({.kind = Kind::ScriptOpponentProperty,
		                    .delaySeconds = drop == 0 ? k_FirstDropSeconds : k_SecondsBetweenDrops,
		                    .value = k_FightHealthProperty,
		                    .amount = 0.0f});
	}
	// Sleg beaten: the log at 1, the evil advisor's taunt, the stone shown, risen and burst, the lightning ending and
	// both advisors' lines; then the camera is given back and the smoke over the lair is removed
	commands.push_back(WaitText(5429));
	commands.push_back(WaitText(5430));
	commands.push_back(WaitText(5432));
	commands.push_back(WaitTask("Land4Ogre", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));

	all.push_back({
	    .id = "quest.the_defending_ogres",
	    .name = "Land 4 quest run: The Defending Ogres",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand4/BeginLand4(1)/BuildUpTown&/Land4Ogre&/"
	                   "KeepCreatureAwayFromOgre&/ManExplainsAboutNemesis on Land4.txt). The texts that wait for a "
	                   "click are clicked on. The quest run's start has played the arrival's film to its end; the "
	                   "timeline turns the camera north over the gates "
	                   "and waits there, without exploring, for the frightened man to come to the temple; presses on "
	                   "his gold scroll with the hand and lets his story play; then puts the creature on the leash and "
	                   "on its side of the fighting ground before Sleg's lair, lets Sleg's challenge play, leaves the "
	                   "fight to the creature, sets Sleg's fight health to none five times in it, and lets the stone's "
	                   "breaking play. The fight starts about ten minutes in.",
	    .expected = "The setup opens the gates and lowers the home village's needs; the arrival opens the vortex, "
	                "loads the creature beside it, starts the five storms, the darkened sky and the five fireball "
	                "throwers, builds the temple, town centre, workshop and two houses part way and half builds the "
	                "shield dispenser, and plays the film with the advisors' lines in order. Sleg is made in his lair "
	                "with his stone and its smoke from the start. Half a minute in, the building advice; about five "
	                "minutes after the arrival the man appears by the temple, praying, with his scroll; pressed, it "
	                "starts his story with every line in order, the scroll logged at 0, and the man let go home; the "
	                "four gremlins are made by the lair. On the fighting ground the leash comes off, Sleg's four lines "
	                "are said, the leash is switched off and the fight starts; beaten, the log goes to 1, the stone "
	                "rises and bursts, the storms end and Land4Ogre finishes with OgreDefeated=1 and "
	                "LighteningMeteorBroken=1. Unverified: that the creature wanders nowhere near Sleg in the first "
	                "five minutes (which would skip the scroll), that the scroll is drawn where the hand's press "
	                "looks for it, and that Sleg is made at a size the fight works with (his size comes from a global "
	                "the challenge code never sets).",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
