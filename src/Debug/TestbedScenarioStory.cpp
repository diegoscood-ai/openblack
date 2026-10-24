/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The story's chapters as land timelines: each is played on the game's own Land 1 through a land entry's timeline=
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"), drives the chapter as a player would, waits on the
// challenge script's own globals, tasks and texts, and ends by logging the globals that show the chapter is done. The
// points are map coordinates, taken from the places the challenge script names; those not yet seen in a running game
// say so.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include <SDL_scancode.h>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

/// The waits for a task of a script: running, and none running after one was
constexpr size_t k_Running = 0;

Command WaitTask(std::string_view script, size_t wait = k_Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = wait, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
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

/// The globals the opening's end is read by: the SkipBox's answer as the setup script keeps it, the land, the tiger
/// gate stone the welcome dance is held round, and the later steps of the tutorial, all still to come
constexpr std::string_view k_OpeningGlobals = "IsSkippingToCreatureSelect IsSkippingCreatureGuide IsKeepingOldCreature "
                                              "LandNumber GateKey2 ProtectGateKeysFlag PickupDemoPlayed CitadelBuiltL1";

/// Where the family waits for the player at the hand-over, and the end of the walk the player follows them on: the
/// focuses of the camera file's points the tutorial names for them
constexpr glm::vec2 k_StartPath {1494.03f, 2109.63f};
constexpr glm::vec2 k_EndMarker {1633.72f, 2307.98f};
/// The tutorial ends the follow once the camera is within 50 of the end marker; the camera is let go a little sooner,
/// so that the script's next test finds it in
constexpr float k_FollowLetGo = 45.0f;
/// The father, the mother and the son, the nearest villager to where they wait
constexpr ThingNear k_Family {.type = script::ObjectType::Villager, .point = k_StartPath, .radius = 15.0f};
/// The temple the tutorial builds, and the spot its lessons put the camera at
constexpr glm::vec2 k_Citadel {1915.05f, 2508.89f};
constexpr glm::vec2 k_TemplePos {1867.52f, 2539.12f};
/// The globals the tutorial's end is read by
constexpr std::string_view k_TutorialGlobals = "GateKey2 ProtectGateKeysFlag PickupDemoPlayed CitadelBuiltL1 StopTracking";

/// The tiger gate stone where the welcome dance is held, within the radius the challenge script finds it in (the
/// mobile static of the tiger stone's kind)
constexpr ThingNear k_TigerStone {
    .type = script::ObjectType::MobileStatic, .subtype = 52, .point = {1652.15f, 2322.91f}, .radius = 30.0f};
/// The gold scroll of Choose Your Creature, where the challenge script puts it
constexpr glm::vec2 k_ChooseScroll {1779.63f, 2805.83f};
/// The gate stone plinth: the point the challenge script finds it from (not yet seen in a running game)
constexpr glm::vec2 k_Plinth {1774.5f, 2793.3f};
/// The globals the tiger stone's end is read by
constexpr std::string_view k_TigerStoneGlobals =
    "GateKey2Present GateKey1Present HighlightGateKey2Active ChooseYourCreatureFinished LostBrotherFinished";
} // namespace

void testbed_scenarios::AddStoryScenarios(std::vector<Scenario>& all)
{
	// The opening, up to the hand-over: the SkipBox answered 0 (play everything) by the land entry; the opening
	// cinematic, the welcome and the Drag hand demo play by themselves, and the texts that wait are clicked on
	all.push_back({
	    .id = "story.t1_opening",
	    .name = "Land 1: the opening to the hand-over",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 0, as land:Land1.txt:<frames>:skip=0/30:"
	                   "timeline=story.t1_opening. The texts that wait for a click are clicked on as soon as they take it. "
	                   "It waits for the tutorial's script to run, logs the setup's globals, then waits for the welcome's "
	                   "last text and for the hand-over's text to be read.",
	    .expected = "The log shows FollowUs running from the start, IsSkippingToCreatureSelect=0, LandNumber=1 and GateKey2 "
	                "a thing's id; then texts 5192 and 5219 read, the hand given to the player (the cinema bars go), and "
	                "the end check: FollowUs still running, the tutorial's later steps (ProtectGateKeysFlag, "
	                "PickupDemoPlayed, CitadelBuiltL1) still 0, ChooseYourCreature not running. About four minutes of "
	                "play.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("FollowUs"),
	            LogGlobals(k_OpeningGlobals, "LandControl1 FollowUs"),
	            // the welcome's last text that waits for a click, before the Drag hand demo
	            WaitText(5192, true),
	            // the hand-over: once it is read, the tutorial gives the player the camera and the hand
	            WaitText(5219, true),
	            LogGlobals(k_OpeningGlobals, "LandControl1 FollowUs ChooseYourCreature", 2.0f),
	        },
	    .onLand = true,
	});

	// Following the family: the camera follows them as the player keeps them in view, along their walk to the welcome,
	// and is let go once it is near the walk's end
	all.push_back({
	    .id = "story.t2_follow",
	    .name = "Land 1: following the family",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 after the opening, in the same timeline. The camera follows the family from where "
	                   "they wait for the player, a follow shot behind them, as they walk; once it is within 45 of the "
	                   "walk's end (the tutorial ends the follow at 50) it is let go.",
	    .expected = "The family walks at its full speed with the camera close behind; the follow ends near the welcome "
	                "dance and the second cinematic starts with text 5193. FollowUs still running, ProtectGateKeysFlag 0.",
	    .commands =
	        {
	            ClickTexts(),
	            {.kind = Kind::CameraFollow, .delaySeconds = 1.0f, .amount = 1.0f, .thing = k_Family},
	            {.kind = Kind::WaitCamera, .waitUntilFree = true, .point = k_EndMarker, .radius = k_FollowLetGo},
	            {.kind = Kind::CameraFollow, .value = 1},
	            WaitText(5193, false),
	            LogGlobals(k_TutorialGlobals, "FollowUs WeAreWayAhead LostTheMother"),
	        },
	    .onLand = true,
	});

	// The second cinematic and the way to the temple: the player brings the camera to the temple the people start to
	// build, then the cinema there
	all.push_back({
	    .id = "story.t3_tunnel",
	    .name = "Land 1: the second cinematic and the way to the temple",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 after the family's follow. The cinematic's texts are clicked on; once the tutorial "
	                   "waits for the camera to reach the temple, the camera is put at the spot its lessons use, looking "
	                   "at it, and the texts of the scene there are clicked on.",
	    .expected = "The builders gather at the temple; the tutorial's wait for the camera ends at once (the spot is about "
	                "60 from the temple, within its 75). The crowd leaves, the family and the leader join the town, "
	                "ProtectGateKeysFlag becomes 1 and ProtectGateKeys runs; texts 5209, 5210, 5195 and 5196 play.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("TunnelOfUltimateDoom"),
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_TemplePos.x, 25.0f, k_TemplePos.y},
	             .look = {k_Citadel.x, 5.0f, k_Citadel.y}},
	            // the camera put there can end the wait in the scripts' same turn, before a wait for its end sees it run
	            WaitTask("TunnelOfUltimateDoom", 1),
	            WaitText(5196, true),
	            LogGlobals(k_TutorialGlobals, "FollowUs ProtectGateKeys"),
	        },
	    .onLand = true,
	});

	// The camera lessons: turning, tilting and zooming as each lesson asks, then the pick-up demo
	std::vector<Command> lessons {
	    ClickTexts(),
	    // turning: the turn key held until the lesson has counted enough turns
	    WaitTask("TeachRotate"),
	    KeyDown(SDL_SCANCODE_Z, 1.0f),
	    WaitTask("TeachRotate", 2),
	    KeyUp(SDL_SCANCODE_Z),
	    // tilting: the tilt key held once the lesson's demo is over, until it says the tilt is done
	    WaitTask("TeachPitch"),
	    WaitTask("Pitching", 2),
	    KeyDown(SDL_SCANCODE_A, 1.0f),
	    WaitText(5225, false),
	    KeyUp(SDL_SCANCODE_A),
	    // zooming: the zoom key held with the keys that move forwards and back, in and out again, until the lesson has
	    // counted more than fifteen turns of zooming. Keys rather than the wheel, which would take the pointer from the
	    // player for the rest of the timeline
	    WaitTask("TeachZoom"),
	    KeyDown(SDL_SCANCODE_RCTRL, 1.0f),
	    KeyDown(SDL_SCANCODE_UP, 0.1f),
	    KeyUp(SDL_SCANCODE_UP, 1.5f),
	    KeyDown(SDL_SCANCODE_DOWN),
	    WaitText(5234, false),
	    KeyUp(SDL_SCANCODE_DOWN),
	    KeyUp(SDL_SCANCODE_RCTRL),
	    WaitGlobal("PickupDemoPlayed", 1.0f),
	    LogGlobals(k_TutorialGlobals, "FollowUs TeachZoom TrackZoomUsage PreventCitadelCompletion", 1.0f),
	};
	all.push_back({
	    .id = "story.t4_lessons",
	    .name = "Land 1: the camera lessons and the pick-up demo",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 after the way to the temple. The demos play by themselves and their texts are "
	                   "clicked on. The turn key (Z) is held through the turning lesson, the tilt key (A) through the "
	                   "tilting one, and the zoom key with the forward and back keys through the zooming one.",
	    .expected = "Each lesson ends on its count of game turns with the camera moving (more than 60 turning, 25 tilting, "
	                "15 zooming) and no nagging text; text 5234, then the pick-up demo; PickupDemoPlayed becomes 1 and "
	                "PreventCitadelCompletion stops.",
	    .commands = std::move(lessons),
	    .onLand = true,
	});

	// The temple built: the town's builders raise it once the tutorial sends them back to work, and the tutorial ends
	all.push_back({
	    .id = "story.t5_temple_built",
	    .name = "Land 1: the temple built",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 after the camera lessons. Nothing is done: the tutorial disbands the builders, "
	                   "who go back to the town and finish the temple, and its check finds it built.",
	    .expected = "The builders finish the temple in about a minute and a half of play; at the tutorial's next check "
	                "(every 10 s) CitadelBuiltL1 becomes 1 and FollowUs returns: the temple's gold scroll scene "
	                "(CitadelGuide) starts.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("CheckCitadel"),
	            WaitGlobal("CitadelBuiltL1", 1.0f),
	            // the tutorial returns in the scripts' same turn as its check finds the temple built
	            WaitTask("FollowUs", 1),
	            LogGlobals(k_TutorialGlobals, "FollowUs CitadelGuide CitadelGuideCinema", 1.0f),
	        },
	    .onLand = true,
	});

	// The Temple Is Finished: the flight to the temple, the player taps its entrance, goes in and comes out again, and
	// the advisors show the first gold scroll. The way in and out is the player's own mouse and keys, recorded with
	// --record-input and played back with --replay-input: the timeline only clicks the texts and waits
	all.push_back({
	    .id = "story.t6_temple_finished",
	    .name = "Land 1: The Temple Is Finished",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 after the temple is built. The texts are clicked on; going in through the "
	                   "entrance and out again with Escape are the player's, from a recording of their mouse and keys "
	                   "(--record-input, then --replay-input), since the timeline takes no part in them.",
	    .expected = "Texts 1998 and 2000, the player goes into the temple and out again, then 5212, 5213, 5199, 5206, "
	                "5207 and 5232; the scroll's bodge highlight is deleted and CitadelGuide returns; ChooseYourCreature "
	                "starts.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("CitadelGuideCinema"),
	            WaitText(2000, false),
	            WaitText(5212, false),
	            WaitText(5232, true),
	            WaitTask("CitadelGuide", 1),
	            LogGlobals(k_TutorialGlobals, "LandControl1 CitadelGuide ChooseYourCreature", 1.0f),
	        },
	    .onLand = true,
	});

	// Choose Your Creature's first stone. It follows the temple's chapter in a game played from the start (SkipBox
	// answer 0): the SkipBox's other answers delete the gate stones and go straight to the glade, so it can only be
	// played after the chapters before it, in the same timeline
	all.push_back({
	    .id = "story.c1_tiger_stone",
	    .name = "Land 1: Choose Your Creature's tiger stone",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 once the temple's chapter is done, after it in the same timeline. It waits for "
	                   "Choose Your Creature to run, puts the camera on its gold scroll and taps it, waits for Sable's "
	                   "challenge to end, takes the tiger gate stone from the welcome's dance site into the hand, holds the "
	                   "hand over the plinth and presses the Action button there.",
	    .expected = "The scroll's tap starts Sable's cut-scene; once it is over the tiger stone is taken into the hand. "
	                "Pressed over the plinth, the stone leaves the hand and the plinth takes it (worth 2 to the script): "
	                "GateKey2Present becomes 1, Sable's next texts play and The Lost Brother starts. The end check logs "
	                "GateKey2Present=1, GateKey1Present=0, ChooseYourCreatureFinished=0, LostBrotherFinished=0, and "
	                "ChooseYourCreature and LostBrother running. Unverified: that the plinth's point is inside the "
	                "player's influence, which the hand's laying needs.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("ChooseYourCreature"),
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_ChooseScroll.x, 45.0f, k_ChooseScroll.y - 70.0f},
	             .look = {k_ChooseScroll.x, 10.0f, k_ChooseScroll.y}},
	            {.kind = Kind::TapHighlight, .delaySeconds = 1.0f, .point = k_ChooseScroll},
	            // the challenge's cut-scene is over once the advisor starts watching for the stone to be taken
	            WaitTask("AdvisorSpotsGateKey2"),
	            {.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = k_TigerStone},
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 0.5f,
	             .eye = {k_Plinth.x, 40.0f, k_Plinth.y - 60.0f},
	             .look = {k_Plinth.x, 3.0f, k_Plinth.y}},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Plinth},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            WaitGlobal("GateKey2Present", 1.0f),
	            WaitTask("LostBrother"),
	            LogGlobals(k_TigerStoneGlobals, "ChooseYourCreature LostBrother AdvisorSpotsGateKey2", 2.0f),
	        },
	    .onLand = true,
	});
}
