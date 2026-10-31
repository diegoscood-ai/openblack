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

#include <string>
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
/// The Lost Brother's places: his house (its scroll), his sister's soap box, and the brother himself at his sick bed
constexpr glm::vec2 k_LostBrotherHouse {1713.83f, 2592.59f};
constexpr glm::vec2 k_SoapBox {1700.36f, 2592.6f};
constexpr ThingNear k_LostBrother {
    .type = script::ObjectType::Villager, .subtype = 52, .point = {1368.27f, 2568.31f}, .radius = 20.0f};
/// The Sculptor's places: his scroll, the sculptor at his work, the quarry's blank rock, and the cow gate stone he
/// carves by him
constexpr glm::vec2 k_SculptorScroll {1760.61f, 2692.3f};
constexpr glm::vec2 k_Sculptor {1768.99f, 2690.71f};
constexpr ThingNear k_QuarryRock {
    .type = script::ObjectType::MobileStatic, .subtype = 50, .point = {1410.94f, 2349.51f}, .radius = 30.0f};
constexpr ThingNear k_CowStone {
    .type = script::ObjectType::MobileStatic, .subtype = 51, .point = {1768.99f, 2690.71f}, .radius = 30.0f};
/// The young ape in the glade beyond the gates
constexpr ThingNear k_GladeApe {.type = script::ObjectType::Creature, .point = {2233.02f, 3152.32f}, .radius = 2.0f};
/// The ape gate stone, by the brother's house
constexpr ThingNear k_ApeStone {
    .type = script::ObjectType::MobileStatic, .subtype = 49, .point = {1713.83f, 2592.59f}, .radius = 20.0f};
/// The globals the tiger stone's end is read by
constexpr std::string_view k_TigerStoneGlobals =
    "GateKey2Present GateKey1Present HighlightGateKey2Active ChooseYourCreatureFinished LostBrotherFinished";
/// The creature's new home in the pen (the first lesson puts it there and makes it its home), and the spot in front
/// of the pen the later lessons walk it to (the second, third and fourth lessons move it there)
constexpr glm::vec2 k_CreatureHome {1896.5f, 2520.06f};
constexpr glm::vec2 k_CreatureSpot {1896.582f, 2526.114f};
/// The player's creature, found about the pen: the nearest creature within 30 of its spot (both points above are
/// within 7 of it)
constexpr ThingNear k_MyCreature {.type = script::ObjectType::Creature, .point = k_CreatureSpot, .radius = 30.0f};
/// The lessons' gold scroll over the pen (the same point for the second to fourth lessons' scroll and the last
/// lesson's second scroll), raised 6. The did-you-know signs nearest it are 13 and 21 away, outside the pick's 5
constexpr glm::vec2 k_PenScroll {1894.8346f, 2514.7295f};
constexpr ThingNear k_PenScrollPick {.type = script::ObjectType::Highlight, .point = k_PenScroll, .radius = 5.0f};
/// The leash post the trainer points at in the leash lesson (at a height of 34.8624, about 5.4 above the pen's ground
/// at 29.48): the post's spot is not yet seen in a running game
constexpr glm::vec2 k_LeashPost {1902.8757f, 2528.1145f};
constexpr float k_LeashPostHeight = 5.4f;
/// The palm tree the leash lessons plant (the one the last lesson ties the creature to), and the trainer's spot by
/// it
constexpr glm::vec2 k_LeashTree {1962.062f, 2523.358f};
constexpr glm::vec2 k_NearTree {1958.873f, 2527.082f};
/// The cow the creature is led to pick up (made again at the same point later in the lesson): an animal of the
/// cow's kind
constexpr glm::vec2 k_CowPoint {1963.887f, 2491.963f};
constexpr ThingNear k_Cow {.type = script::ObjectType::Animal, .subtype = 8, .point = k_CowPoint, .radius = 10.0f};
/// The food given in the eating lesson: a villager of the Norse village by the pen. A testbed choice: the lesson accepts
/// anything held within 50 of the creature and any eating
/// (in the village, away from the trainer, who is a villager too and the script's own)
constexpr ThingNear k_FoodVillager {.type = script::ObjectType::Villager, .point = {1865.61f, 2641.24f}, .radius = 40.0f};
/// The globals the lessons' end is read by
constexpr std::string_view k_LessonGlobals =
    "IsSkippingCreatureGuide ChooseYourCreatureFinished TrainerDeleted CreatureDevInterimReminderFlag LeashTree";
/// The scripts the lessons' end is read by
constexpr std::string_view k_LessonScripts =
    "CreatureDevelopmentSet CreatureDevSeeHome CreatureDevLearnToEat CreatureDevPunishment CreatureDevLeashIntro "
    "CreatureDevLeashAttachToHouse CreatureDevelopmentHighlightNotify CreatureDevelopmentHighlightNotifyTwo "
    "CreatureDevInterimReminder GetRidOfTrainer";
} // namespace

void testbed_scenarios::AddStoryScenarios(std::vector<Scenario>& all)
{
	// the quest runs written in their own files
	AddQuestTempleFinishedScenarios(all);
	AddQuestVortexLand1Scenarios(all);
	AddQuestKhazarsFireballScenarios(all);
	AddQuestKhazarsShieldScenarios(all);
	AddQuestCouldntBearScenarios(all);
	AddQuestLand3Scenarios(all);
	AddQuestUndeadVillageScenarios(all);
	AddQuestDefendingOgresScenarios(all);
	AddQuestLand4Scenarios(all);
	AddQuestLand5Scenarios(all);
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

	// The same quest started directly (a land entry's quest=SetupLand1/ChooseYourCreature): the gold scroll is
	// pressed on with the hand, as the script's click test asks, rather than tapped
	all.push_back({
	    .id = "quest.choose_your_creature",
	    .name = "Land 1 quest run: Choose Your Creature to the tiger stone",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand1/ChooseYourCreature). It waits for the quest to "
	                   "run, puts the camera on its gold scroll, points the hand at the scroll and presses the Action "
	                   "button once the hand is over it, waits for Sable's challenge to end, then takes the tiger gate "
	                   "stone into the hand and presses over the plinth, as story.c1_tiger_stone does.",
	    .expected = "The press starts Sable's cut-scene (the native trace shows the quest's cinema, Sable made and her "
	                "lines); then GateKey2Present becomes 1 and The Lost Brother starts.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("ChooseYourCreature"),
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_ChooseScroll.x, 45.0f, k_ChooseScroll.y - 70.0f},
	             .look = {k_ChooseScroll.x, 10.0f, k_ChooseScroll.y}},
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_ChooseScroll, .value = 1},
	            {.kind = Kind::PointerPress,
	             .delaySeconds = 0.5f,
	             .waitUntilFree = true,
	             .value = 3,
	             .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_ChooseScroll, .radius = 15.0f}},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
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

	// The Lost Brother, as Choose Your Creature's tiger stone starts it, then the ape stone it frees: the scroll by the
	// house pressed on, the cut-scene let play, the sick brother carried from his bed and laid by his sister, and once
	// the quest is over the ape stone taken to the plinth
	all.push_back({
	    .id = "quest.the_lost_brother",
	    .name = "Land 1 quest run: The Lost Brother and the ape stone",
	    .facet = Facet::Story,
	    .description = "Played after quest.choose_your_creature in the same quest run. It presses on The Lost Brother's "
	                   "scroll by the house, lets its cut-scene play, takes the sick brother from his bed into the hand, "
	                   "lays him by his sister at the soap box, waits for the quest to end, then takes the ape gate "
	                   "stone to the plinth.",
	    .expected = "The brother laid by his sister reunites them (LostBrotherFinished set), the ape stone can be picked "
	                "up, and on the plinth GateKey1Present becomes 1 and The Sculptor starts.",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("LostBrother"),
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_LostBrotherHouse.x, 40.0f, k_LostBrotherHouse.y - 60.0f},
	             .look = {k_LostBrotherHouse.x, 10.0f, k_LostBrotherHouse.y}},
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_LostBrotherHouse, .value = 1},
	            {.kind = Kind::PointerPress,
	             .delaySeconds = 0.5f,
	             .waitUntilFree = true,
	             .value = 3,
	             .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_LostBrotherHouse, .radius = 15.0f}},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            // the cut-scene: the sister walks to her soap box and tells of her brother
	            {.kind = Kind::HandTake, .delaySeconds = 45.0f, .thing = k_LostBrother},
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 0.5f,
	             .eye = {k_SoapBox.x, 35.0f, k_SoapBox.y - 50.0f},
	             .look = {k_SoapBox.x, 3.0f, k_SoapBox.y}},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_SoapBox + glm::vec2(3.0f, 0.0f)},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            {.kind = Kind::WaitGlobal,
	             .waitUntilFree = true,
	             .amount = 0.0f,
	             .global = "LostBrotherFinished",
	             .compare = Compare::NotEqual},
	            {.kind = Kind::HandTake, .delaySeconds = 2.0f, .thing = k_ApeStone},
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 0.5f,
	             .eye = {k_Plinth.x, 40.0f, k_Plinth.y - 60.0f},
	             .look = {k_Plinth.x, 3.0f, k_Plinth.y}},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Plinth},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            WaitGlobal("GateKey1Present", 1.0f),
	            LogGlobals(k_TigerStoneGlobals, "ChooseYourCreature LostBrother TheSculptor", 2.0f),
	        },
	    .onLand = true,
	});

	// The Sculptor, as the ape stone starts it, then the cow stone it carves and the glade: the scroll by his house
	// pressed on, the cut-scene let play, the quarry's blank rock carried to him and laid by him, the carved cow stone
	// taken to the plinth once he is done; then, the glade's scene let play, the ape pressed on twice to choose it
	all.push_back({
	    .id = "quest.the_sculptor_and_glade",
	    .name = "Land 1 quest run: The Sculptor, the cow stone and the choice in the glade",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_lost_brother in the same quest run. It presses on The Sculptor's scroll, lets "
	                   "its cut-scene play, carries the quarry's blank rock to the sculptor and lays it by him, waits for "
	                   "the cow stone he carves, takes it to the plinth, lets the gates and the glade's scene play, then "
	                   "presses on the ape twice to choose it.",
	    .expected = "The rock laid by the sculptor is carved (CreatureRetrieveFinished becomes 1); on the plinth "
	                "GateKey3Present becomes 1, the gates open and the three creatures show in the glade; the ape pressed "
	                "on twice is chosen (ChooseYourCreatureFinished becomes 1).",
	    .commands =
	        {
	            ClickTexts(),
	            WaitTask("TheSculptor"),
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_SculptorScroll.x, 40.0f, k_SculptorScroll.y - 60.0f},
	             .look = {k_SculptorScroll.x, 10.0f, k_SculptorScroll.y}},
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_SculptorScroll, .value = 1},
	            {.kind = Kind::PointerPress,
	             .delaySeconds = 0.5f,
	             .waitUntilFree = true,
	             .value = 3,
	             .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_SculptorScroll, .radius = 15.0f}},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            // the cut-scene: the sculptor tells of the stone he could carve
	            {.kind = Kind::HandTake, .delaySeconds = 45.0f, .thing = k_QuarryRock},
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 0.5f,
	             .eye = {k_Sculptor.x, 35.0f, k_Sculptor.y - 50.0f},
	             .look = {k_Sculptor.x, 3.0f, k_Sculptor.y}},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Sculptor + glm::vec2(5.0f, 0.0f)},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            WaitGlobal("CreatureRetrieveFinished", 1.0f),
	            {.kind = Kind::HandTake, .delaySeconds = 2.0f, .thing = k_CowStone},
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 0.5f,
	             .eye = {k_Plinth.x, 40.0f, k_Plinth.y - 60.0f},
	             .look = {k_Plinth.x, 3.0f, k_Plinth.y}},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Plinth},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            WaitGlobal("GateKey3Present", 1.0f),
	            // the gates and the glade's scene, then the choice: the ape pressed on, and again to be sure
	            {.kind = Kind::PointerTo, .delaySeconds = 90.0f, .thing = k_GladeApe},
	            {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            WaitGlobal("ChooseYourCreatureFinished", 1.0f),
	            LogGlobals("GateKey1Present GateKey2Present GateKey3Present CreatureRetrieveFinished "
	                       "ChooseYourCreatureFinished",
	                       "ChooseYourCreature TheSculptor CreaturesInGlade", 2.0f),
	        },
	    .onLand = true,
	});

	// The glade alone, as a new game that skips to the creature's choice reaches it (quest=SetupLand1/CreaturesInGlade):
	// the glade's scene let play, then the ape pressed on twice to choose it
	all.push_back({
	    .id = "quest.glade_choice",
	    .name = "Land 1 quest run: the choice in the glade",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start of the glade's script. It lets the glade's scene play, then presses "
	                   "on the ape twice to choose it.",
	    .expected = "The ape pressed on twice is chosen (ChooseYourCreatureFinished becomes 1).",
	    .commands =
	        {
	            ClickTexts(),
	            // the temple built, as the story has it by the glade: a quest run has only its plan. The creature's
	            // lessons need it, since the Leash of Learning hangs on one of its leash posts by the pen
	            {.kind = Kind::ScriptBuildBuilding, .point = k_Citadel, .amount = 1.0f},
	            {.kind = Kind::TempleBuilt, .built = 1.0f},
	            WaitTask("CreaturesInGlade"),
	            // the scene's last line (the evil advisor's "If rather small at the moment."), after which the hand is the
	            // player's again
	            WaitText(4104, true),
	            // the camera is left on the choosing view by the scene
	            {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .thing = k_GladeApe},
	            {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            WaitGlobal("ChooseYourCreatureFinished", 1.0f),
	        },
	    .onLand = true,
	});

	// The Creature's Learning: the trainer's five lessons, started as CreatureDevelopmentSet runs them (the land's
	// control script runs the same five in the same order). The pen's scroll is pressed on for each lesson after the
	// first; the creature is held and stroked, fed, held and slapped, put on the Leash of Learning and pulled to the
	// tree, sent to the cow, and tied to the tree with a double click
	std::vector<Command> learning {
	    ClickTexts(),
	    {.kind = Kind::StartScript, .script = "CreatureDevelopmentSet"},
	    WaitTask("CreatureDevSeeHome"),
	    LogGlobals(k_LessonGlobals, k_LessonScripts),

	    // ---- Lesson 1, the new home: no scroll to press; the cut-scene plays and its texts are clicked on. Then the
	    // creature is held for over two seconds (a locked hold of it) and stroked within two minutes (a stroke's
	    // strength above 0 gives 3858)
	    // the trainer's line once the cut-scene is over, then the nag starts as the hold is waited for
	    WaitText(6866, true),
	    WaitTask("CreatureDevInterimReminder"),
	    // [camera] the pen from the west, as the lesson's own last view of it looks
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1872.0f, 14.0f, 2532.0f},
	     .look = {k_CreatureHome.x, 3.0f, k_CreatureHome.y}},
	    // [real] the Action button pressed on the creature and held: the hand's locked select of its own creature
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_MyCreature},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_MyCreature},
	    // held past the lesson's two-second test, through its two lines (3857, 3861)
	    WaitText(3861, true),
	    // [real] the held hand moved gently to and fro over the body: strokes, given to the creature's mind as the
	    // hand lets go
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.03f, 0.0f}, .amount = 0.8f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {-0.03f, 0.0f}, .amount = 0.8f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.03f, 0.0f}, .amount = 0.8f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {-0.03f, 0.0f}, .amount = 0.8f},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	    // the stroke's line (3858; a slap would give 3859), then the trainer leaves (3860)
	    WaitText(3860, true),
	    LogGlobals(k_LessonGlobals, "CreatureDevSeeHome CreatureDevLearnToEat"),

	    // ---- Lesson 2, learning to eat: after the 31 s pause the trainer walks out and calls until the scroll or she
	    // is clicked
	    WaitTask("CreatureDevelopmentHighlightNotify"),
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1870.0f, 15.0f, 2525.0f},
	     .look = {k_PenScroll.x, 6.0f, k_PenScroll.y}},
	    // [real] the scroll pressed on with the hand, as the quests before press theirs
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_PenScroll, .value = 1},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_PenScrollPick},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    // the cut-scene's last line (3864), then the advisors' comments on what the hand is over start
	    WaitText(3864, true),
	    WaitTask("CheckForCommentOnFoodItems"),
	    // [shortcut] a villager of the Norse village put straight into the hand (a store's food pile can't be held): the
	    // lesson waits for anything held within 50 of the creature, and for any eating
	    {.kind = Kind::HandTake, .delaySeconds = 3.0f, .thing = k_FoodVillager},
	    // "Give him the food by keeping your hand in front of him" (3865)
	    WaitText(3865, true),
	    // [real] the hand moved in front of the creature: a gift is called off as soon as the hand is too far from
	    // where the creature takes it
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_MyCreature},
	    // [hand] the hand gives the creature what it holds, as the Action button held over it does; the pointer's way
	    // of giving (the press over the creature with something in the hand) is not checked to give rather than take
	    // hold of the creature
	    {.kind = Kind::HandGive, .delaySeconds = 1.0f},
	    // eaten once (the eating actions' counts), then the cut-scene (3867, 3866)
	    WaitText(3867, false),
	    WaitText(3866, true),
	    LogGlobals(k_LessonGlobals, "CreatureDevLearnToEat CreatureDevPunishment CreatureDevelopmentTrainerGoesBack"),

	    // ---- Lesson 3, punishment: after the 61 s pause, the scroll pressed on; the cut-scene has the creature throw
	    // the trainer (and waits for her to fly); then the creature is held and slapped (any change of the hand's
	    // stroke or slap strength after the lesson clears it counts)
	    WaitTask("CreatureDevelopmentHighlightNotify"),
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1870.0f, 15.0f, 2525.0f},
	     .look = {k_PenScroll.x, 6.0f, k_PenScroll.y}},
	    // [real]
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_PenScroll, .value = 1},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_PenScrollPick},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    // the cut-scene's last line (4040), read for four seconds before it is clicked on: clicked on at once, the
	    // trainer is sent back to her spot while she still flies from the throw, and a thing in flight takes no such
	    // order, so she would never get there
	    WaitText(4040, false),
	    {.kind = Kind::ClickTexts, .value = 0},
	    {.kind = Kind::ClickTexts, .delaySeconds = 4.0f, .value = 1},
	    WaitText(4040, true),
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 4.0f,
	     .eye = {1872.0f, 14.0f, 2536.0f},
	     .look = {k_CreatureSpot.x, 3.0f, k_CreatureSpot.y}},
	    // [real] the creature held; the lesson says "Now slap him" once it finds it held (4042), which shows its
	    // wait has begun after the feedback was cleared. The hold waits for the lesson's first "Now slap him" nag
	    // (4041), which comes once the cut-scene is over: a press while the cut-scene still has the camera is lost
	    WaitText(4041, true),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_MyCreature},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_MyCreature},
	    WaitText(4042, true),
	    // [real] quick sweeps from side to side across the middle of the screen: slaps, a second after taking hold;
	    // no slow move, which would stroke
	    {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {-0.3f, 0.0f}, .amount = 0.1f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.6f, 0.0f}, .amount = 0.1f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {-0.3f, 0.0f}, .amount = 0.1f},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	    // the slap's lines (4043, 4046) or, when the sweeps cross the creature lengthwise and read as a stroke, the
	    // stroke's (4044, 4045): whichever was felt, the lesson then sends the trainer back
	    WaitTask("CreatureDevelopmentTrainerGoesBack"),
	    LogGlobals(k_LessonGlobals, "CreatureDevPunishment CreatureDevLeashIntro CreatureDevelopmentTrainerGoesBack"),

	    // ---- Lesson 4, the Leash of Learning: after the 61 s pause, the scroll pressed on; the Leash of Learning
	    // picked at its post (the hand's leash of the learning kind), put on the creature (the creature leashed), the
	    // creature pulled to within 20 of the tree, then sent to the cow until it holds it (what the creature holds)
	    WaitTask("CreatureDevelopmentHighlightNotify"),
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1870.0f, 15.0f, 2525.0f},
	     .look = {k_PenScroll.x, 6.0f, k_PenScroll.y}},
	    // [real]
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_PenScroll, .value = 1},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_PenScrollPick},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    // "Please click on it" (4051)
	    WaitText(4051, true),
	    // [camera] the leash post in the middle of the screen
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1890.0f, 15.0f, 2545.0f},
	     .look = {k_LeashPost.x, k_LeashPostHeight, k_LeashPost.y}},
	    // [real] the post tapped with the pointer in the middle of the screen, as leash.double_click_tie aims at its
	    // pillar; a leash post has no script type for a pick, so the screen's middle is the only aim
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    // "Good! Now click on your Creature." (4049)
	    WaitText(4049, true),
	    // [hand] the picked leash put on the creature, as the Action button's tap on it does; the pointer's press on
	    // the creature with the leash picked starts the hand's hold of it instead (the wait for a tap first is not
	    // ported in the hand)
	    {.kind = Kind::HandTapLeash, .delaySeconds = 1.0f},
	    // the leashed cut-scene, to its last line (4055)
	    WaitText(4055, true),
	    // the cut-scene over: the lesson's reminder starts as it gives the camera back, so the camera below is not
	    // undone by the cut-scene's own last move (from where the leash post's rope stands between the hand and the tree)
	    WaitTask("CreatureDevInterimReminder"),
	    // [camera] the pen and the tree in view, from east of the post
	    {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = {1925.0f, 35.0f, 2560.0f}, .look = {1955.0f, 0.0f, 2525.0f}},
	    // [real] the hand, with the leash, held 10 short of the tree on the creature's side, and the Action button
	    // tapped on the ground there: the creature walks to the point, and a second tap on its way makes it run, as the
	    // trainer advises (4055)
	    {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_LeashTree - glm::vec2(10.0f, 0.0f)},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    // the creature within 20 of the tree and the trainer at her spot by it: the cut-scene's first line (4056), then
	    // its last (4061)
	    WaitText(4056, false),
	    WaitText(4061, true),
	    {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	    // the cut-scene over: the lesson's reminder starts again as it gives the camera back
	    WaitTask("CreatureDevInterimReminder"),
	    // [camera] from south-east of the cow, so that the creature, come from the tree on the north, stands behind it
	    // rather than between the camera and the cow
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1985.0f, 25.0f, 2470.0f},
	     .look = {k_CowPoint.x, 1.0f, k_CowPoint.y}},
	    // [real] the hand with the leash over the cow (the lesson lets it be picked up once the hand is within 10) and
	    // the Action button clicked on it: the leash's order on a thing; the camera has two seconds to arrive
	    {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .thing = k_Cow},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .waitUntilFree = true, .value = 3, .thing = k_Cow},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    // the creature holds the cow: the cut-scene (4062 to 4064)
	    WaitText(4062, false),
	    WaitText(4064, true),
	    LogGlobals(k_LessonGlobals, "CreatureDevLeashIntro CreatureDevLeashAttachToHouse"),

	    // ---- Lesson 5, tying the leash: after the 31 s pause, the trainer by the tree and two scrolls, either pressed
	    // on (a click on either); then the creature tied to the tree with a double click (its leash tied to the tree)
	    WaitTask("CreatureDevLeashAttachToHouse"),
	    // [real] the leash shaken off the hand first, so that the press on the scroll is a plain tap rather than the
	    // leash's order on a thing (the lesson tells of the shake in 4064)
	    {.kind = Kind::LeashShake, .delaySeconds = 1.0f},
	    WaitTask("CreatureDevelopmentHighlightNotifyTwo"),
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1870.0f, 15.0f, 2525.0f},
	     .look = {k_PenScroll.x, 6.0f, k_PenScroll.y}},
	    // [real] the pen's scroll, the lesson's second
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_PenScroll, .value = 1},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_PenScrollPick},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    // "With the Leash attached to your hand, move it over the tree." (4068)
	    WaitText(4068, true),
	    // [hand] the creature leashed again, as in lesson 4
	    {.kind = Kind::HandTapLeash, .delaySeconds = 1.0f},
	    // [camera]
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {1940.0f, 20.0f, 2540.0f},
	     .look = {k_LeashTree.x, 2.0f, k_LeashTree.y}},
	    // [real] the hand over the tree's foot and the Action button clicked, then clicked again as a double click, as
	    // leash.double_click_tie ties its leash to a pillar
	    {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_LeashTree},
	    {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    {.kind = Kind::PointerDoubleClick, .delaySeconds = 0.1f},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	    {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	    // tied: the cut-scene (4070), the farewell and the good advisor's last line (6096)
	    WaitText(4070, false),
	    WaitText(6096, true),
	    // the set is done once the last lesson returns
	    WaitTask("CreatureDevelopmentSet", static_cast<size_t>(testbed_land_timeline::TaskWait::Finished)),
	    LogGlobals(k_LessonGlobals, k_LessonScripts, 2.0f),
	};
	all.push_back({
	    .id = "quest.the_creatures_learning",
	    .name = "Land 1 quest run: The Creature's Learning, the trainer's five lessons",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_sculptor_and_glade in the same quest run, with the ape the player's creature. "
	                   "It starts CreatureDevelopmentSet, clicks the texts, holds the creature and strokes it, presses on "
	                   "the pen's scroll for each later lesson, gives the creature a food pile put in the hand, holds and "
	                   "slaps it, taps the Leash of Learning's post, leashes the creature, pulls it to the tree, sends it "
	                   "to the cow, shakes the leash off, presses on the pen's scroll again, leashes it and double-clicks "
	                   "the tree.",
	    .expected = "Each lesson ends on what it waits for: the stroke (3858, then 3860), the food eaten (3867), the slap "
	                "(4043, 4046), the leash picked (4049), the creature leashed and at the tree (4056), holding the cow "
	                "(4062), tied to the tree (4070), and the farewell (6096). CreatureDevelopmentSet finishes; the end "
	                "check logs TrainerDeleted=0 and the five lessons not running. Unverified: the leash post's spot, "
	                "that the forced throw makes the trainer fly (the punishment lesson waits for it), and that the "
	                "hand's sweeps give a stroke and a slap.",
	    .commands = std::move(learning),
	    .onLand = true,
	});

	// MISSING: none. Every lesson can be driven with existing kinds; the steps not driven by the player's own input are
	// the food (HandTake, a shortcut), the giving (HandGive) and the two leashings (HandTapLeash), marked
	// above. Kinds that would make them real inputs: a pick of the temple's leash posts for PointerTo (a post has no
	// script type, so it is aimed at the screen's middle), and the hand taking food from the village store under a held
	// press (the pointer path, not a command, is what is unchecked there).
	// Land 2's gods with their minds on, traced: each turn of Khazar's and Lethys's minds writes one line to the log, what
	// their hands did, the queued action they forced and what their weighing chose (a debug control: the trace is off
	// in play)
	all.push_back({
	    .id = "story.land2_computer_minds",
	    .name = "Land 2: the gods' minds, traced",
	    .facet = Facet::Story,
	    .description = "Played on Land 2 from its start. The traces of the two computer players' minds are switched on, "
	                   "and the texts are clicked on; nothing else is done, so the land's scripts and the gods' own minds "
	                   "play as they would.",
	    .expected = "A line a turn for each god whose mind is on, free and not held: its hand's steps, the queued action it "
	                "forced, or what its weighing chose; a chosen node whose action openblack lacks is told in a debug line. "
	                "The lines give the actions the gods choose, by how often.",
	    .commands =
	        {
	            ClickTexts(),
	            {.kind = Kind::TraceComputerMind, .player = PlayerNames::PLAYER_TWO, .value = 1},
	            {.kind = Kind::TraceComputerMind, .player = PlayerNames::PLAYER_THREE, .value = 1},
	            LogGlobals("LandNumber", "LandControl2", 1.0f),
	        },
	    .onLand = true,
	});

	// The same traces as the first thing of a land timeline, so that a quest run after it logs every turn of the gods'
	// minds from the land's start (a debug control: the trace is off in play)
	all.push_back({
	    .id = "story.land2_computer_minds_first",
	    .name = "Land 2: the gods' minds traced first",
	    .facet = Facet::Story,
	    .description = "Put first in a land timeline on Land 2: the traces of the two computer players' minds are switched "
	                   "on at once, and the timeline goes on to the next scene; nothing else is done.",
	    .expected = "From then on a line a turn for each god whose mind is on, free and not held, as in "
	                "story.land2_computer_minds: the lines give every action the gods choose on the land, by how often.",
	    .commands =
	        {
	            {.kind = Kind::TraceComputerMind, .player = PlayerNames::PLAYER_TWO, .value = 1},
	            {.kind = Kind::TraceComputerMind, .player = PlayerNames::PLAYER_THREE, .value = 1},
	        },
	    .onLand = true,
	});
}

testbed_scenarios::QuestChapter::QuestChapter(std::vector<testbed_land_timeline::QuestScript> scripts)
    : _scripts(std::move(scripts))
{
	_name = "Quest run:";
	for (const auto& script : _scripts)
	{
		_name += " " + script.name;
	}
	_scenario = {
	    .id = "quest",
	    .name = _name,
	    .facet = Facet::Story,
	    .description = "A land entry's quest=: the land's story script is not started; these are, one after the other.",
	    .onLand = true,
	};
	for (size_t i = 0; i < _scripts.size(); ++i)
	{
		const std::string_view script = _scripts[i].name;
		if (_scripts[i].globalValue.has_value())
		{
			_scenario.commands.push_back({.kind = Kind::SetGlobal, .amount = *_scripts[i].globalValue, .global = script});
			continue;
		}
		_scenario.commands.push_back({.kind = Kind::StartScript, .script = script, .scriptArguments = _scripts[i].arguments});
		// a started script's task is there at once, before the scripts run: the next starts once none of it is left
		// (a script may end within its first turn, before a wait could see it running)
		if (i + 1 < _scripts.size() && !_scripts[i].background)
		{
			_scenario.commands.push_back(WaitTask(script, static_cast<size_t>(testbed_land_timeline::TaskWait::NotRunning)));
		}
	}
}
