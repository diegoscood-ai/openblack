/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 1's creature guide (the second half of "The Creature's Learning") as quest runs, played after the trainer's
// lessons in the same quest run, with the ape the player's creature (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"). The land's control script starts the guide's wandering as soon as the creature is chosen and
// the guide's chapters once the trainer's last lesson is done; these timelines start the same scripts at the same
// points. The texts that wait for a click are clicked on, the scrolls are pressed on with the hand, and the player's
// part that only the hand can give is marked where a command stands in for it. The points are map coordinates, taken
// from the places the guide's scripts name.

#include <cstddef>
#include <cstdint>

#include <algorithm>
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

/// The messenger's spot by the temple, where his gold scroll is laid
constexpr glm::vec2 k_MessengerSpot {1999.41f, 2493.35f};
/// Where the meeting sends the guide once the messenger's scroll is clicked, in the pass
constexpr glm::vec2 k_GuideInThePass {2169.17f, 2621.51f};
/// Where the player's creature is sent to meet it: 22 from the guide, on the side it comes from
constexpr glm::vec2 k_CreatureMeets {2150.0f, 2610.0f};
/// Where the food lesson's own miracles are made, and the village store they are cast on
constexpr glm::vec2 k_MiracleSpot {2458.78f, 2611.89f};
constexpr glm::vec2 k_StoragePit {2487.55f, 2571.21f};
/// The food lesson's gold scroll, 15 east of the place the lesson impresses the village from
constexpr glm::vec2 k_FoodScroll {2436.95f, 2634.93f};

/// The Aztec village, as the guide's chapter after the food lesson picks it, and the belief that wins it for the player
constexpr glm::vec2 k_AztecVillage {2487.55f, 2571.21f};
constexpr float k_VillagePickRadius = 150.0f;
constexpr float k_WinningBelief = 2.0f;
/// Where the guide sleeps between its chapters by the village, made there when its chapter starts it afresh
constexpr glm::vec2 k_GuideMade {2401.23f, 2631.2f};
/// The fight lesson's clearing, its scroll 10 back from the guide on both axes, and where the creature is sent: 19
/// from the guide, beside the spot the lesson walks it to
constexpr glm::vec2 k_FightClearing {2472.35f, 2788.707f};
constexpr glm::vec2 k_FightScroll {2462.35f, 2778.707f};
constexpr glm::vec2 k_CreatureToFight {2490.0f, 2785.0f};
/// The spot the fight lesson walks the creature to, opposite the guide, and the arena's land 10 from it, away from
/// the guide, which a click on steps the creature to
constexpr glm::vec2 k_CreatureFightSpot {2498.965f, 2780.871f};
constexpr glm::vec2 k_ArenaLand {2506.0f, 2773.8f};
/// How long a blow is charged for, as the Ogre's fights charge theirs
constexpr float k_BlowChargeMs = 400.0f;
/// The storm chapter: the guide's nap after the fight, its scroll 10 east of it; the spot below the mountain and the
/// scroll there; where the creature is sent by it, and up the mountain to the top, where the guide waits
constexpr glm::vec2 k_NapScroll {2482.35f, 2788.707f};
constexpr float k_NapScrollPickRadius = 45.0f;
constexpr glm::vec2 k_BelowTheMountainScroll {2280.452f, 2819.082f};
constexpr glm::vec2 k_CreatureBelowTheMountain {2300.0f, 2822.0f};
/// (the top itself is at (2318.29, 2513.058); the creature is sent 15 short of it, on the side it climbs from)
constexpr glm::vec2 k_CreatureOnTheTop {2318.29f, 2528.0f};

/// The globals and scripts the guide's chapters are read by
constexpr std::string_view k_GuideGlobals = "MoveTheGuideState GuideNotImpressing StopPopulating StopStayingTired "
                                            "StopHealingCombatants CreatureDevelopment_Fight OpenVortexL1";
constexpr std::string_view k_GuideScripts = "MoveTheGuideAround CreatureGuideSet MeetTheGuide GuideAsksToMeetYourCreature "
                                            "GuideImpressTownLesson ImpressFood GuideCreateMiracle PopulateTown "
                                            "GuideImpressTownNextLesson GuideStayTired CreatureDevGuideTeachesFight "
                                            "HealCombatants TheStorm LeaveThroughVortexL1 EndScriptsForStorm";

Command WaitTask(std::string_view script, TaskWait wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

Command WaitText(uint32_t text, bool read = true)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command LogGuide(float delay = 0.0f)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .global = k_GuideGlobals, .script = k_GuideScripts};
}

/// The camera put back from a point (south of it, towards smaller map z) and above it, looking at it a little above
/// the land; both heights are above the land there, as the timeline's camera takes them
Command CameraOn(glm::vec2 point, float above, float back, float delay = 1.0f)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
	        .eye = {point.x, above, point.y - back},
	        .look = {point.x, 3.0f, point.y}};
}

/// The camera of the real input runs: 40 over the land and 60 back from the point, looking 3 over the land at it, so
/// the point is at the middle of the screen
Command CameraGate(glm::vec2 point, float delay)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
	        .eye = {point.x, 40.0f, point.y - 60.0f},
	        .look = {point.x, 3.0f, point.y}};
}

/// A gold scroll pressed on with the hand: the pointer on the drawn scroll, then the Action button pressed once the
/// hand is over it (which waits for the scroll to be laid) and let go, as a player clicks it
void PressScroll(std::vector<Command>& commands, glm::vec2 scroll, float radius = k_ScrollPickRadius)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = radius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal,
	        .waitUntilFree = true,
	        .amount = value,
	        .global = global,
	        .compare = testbed_land_timeline::Compare::Equal};
}

/// How near the point the player's creature is walked to
constexpr float k_SentWithin = 5.0f;

/// The player's creature walked to a point under a script's control (a script's MOVE_GAME_THING), where a player leads
/// it with the leash: its own mind can't take it elsewhere on the way
Command SendTo(glm::vec2 point, float delay = 1.0f)
{
	return {.kind = Kind::ScriptMove, .delaySeconds = delay, .point = point, .radius = k_SentWithin};
}

/// A blow (band: 0 high, 1 in the middle, 2 low) the player's creature is told to strike in its fight, as the player's
/// click on the other's body charges and lets it go
Command Blow(size_t band, float delay, bool added = false)
{
	return {.kind = Kind::FightBlow, .delaySeconds = delay, .value = band, .chargeMs = k_BlowChargeMs, .addToQueue = added};
}
/// The guide's miracle at the food lesson's spot tapped with the Action button, then cast with the same button over
/// the village store, through the testbed's pointer
void TapAndCast(std::vector<Command>& commands, float delay)
{
	const ThingNear miracle {.type = script::ObjectType::OneShotSpell, .point = k_MiracleSpot, .radius = 10.0f};
	const ThingNear store {.type = script::ObjectType::Abode, .point = k_StoragePit, .radius = 10.0f};
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .thing = miracle});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = miracle});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = store});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = store});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton});
}

/// A story run's twin of a guide quest run's timeline: the same commands but for the scripts the timeline starts,
/// which the land's control script starts in a story run at the same points, so that the twin only waits and drives
/// the input. A command the twin cannot keep (a shortcut that is not input) is replaced by the commands given
[[nodiscard]] Scenario StoryTwin(const std::vector<Scenario>& all, std::string_view questId, std::string_view id,
                                 std::string_view name, std::string_view description, Command::Kind replaced = Kind::Stop,
                                 const std::vector<Command>& replacement = {})
{
	const auto quest = std::ranges::find(all, questId, &Scenario::id);
	Scenario twin = *quest;
	twin.id = id;
	twin.name = name;
	twin.description = description;
	twin.commands.clear();
	for (const auto& command : quest->commands)
	{
		if (command.kind == Kind::StartScript)
		{
			continue;
		}
		if (command.kind == replaced && replaced != Kind::Stop)
		{
			twin.commands.insert(twin.commands.end(), replacement.begin(), replacement.end());
			continue;
		}
		twin.commands.push_back(command);
	}
	return twin;
}
} // namespace

void testbed_scenarios::AddQuestCreatureGuideScenarios(std::vector<Scenario>& all)
{
	// The guide's wandering, started as the land's control script starts it, in the background, once the creature is
	// chosen and before the trainer's first lesson
	all.push_back({
	    .id = "quest.guide_wander",
	    .name = "Land 1 quest run: the creature guide made and wandering the valley",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_sculptor_and_glade in the same quest run, before "
	                   "quest.the_creatures_learning. It starts MoveTheGuideAround, which the land's control script starts "
	                   "in the background once the creature is chosen, and logs its state.",
	    .expected = "The guide is made by the canyon (the bear, for the ape) and walks to the head of the canyon; its "
	                "state rises from 0.",
	    .commands =
	        {
	            {.kind = Kind::StartScript, .script = "MoveTheGuideAround"},
	            WaitTask("MoveTheGuideAround", TaskWait::Running),
	            LogGuide(2.0f),
	        },
	    .onLand = true,
	});

	// The meeting, the guide's request and the making of friends: CreatureGuideSet started once the trainer's lessons
	// are done, as the land's control script runs its first three guide chapters then. The messenger's scroll is
	// pressed on, the camera put on the guide in the pass for its first sight, and the creature sent to it
	std::vector<Command> meeting {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::StartScript, .script = "CreatureGuideSet"},
	    WaitTask("MeetTheGuide", TaskWait::Running),
	    LogGuide(1.0f),
	    // the meeting waits two minutes (the creature is in its pen, far from the guide), then the messenger walks up
	    // to his spot and his scroll is laid there; the press waits for it
	    CameraOn(k_MessengerSpot, 25.0f, 30.0f),
	};
	PressScroll(meeting, k_MessengerSpot);
	// the messenger's scene, to the good advisor's line, and the camera pulled back over the temple
	meeting.push_back(WaitText(5687));
	meeting.push_back(LogGuide(0.0f));
	// [camera] on the guide in the pass, from 50 back and 30 up: on screen and within 90 of it, which starts its first
	// sight once the scene's own camera move is over
	meeting.push_back(CameraOn(k_GuideInThePass, 30.0f, 50.0f, 6.0f));
	meeting.push_back(WaitText(3952));
	meeting.push_back(WaitTask("GuideAsksToMeetYourCreature", TaskWait::Running));
	meeting.push_back(LogGuide(1.0f));
	// [shortcut] the creature freed from the tree the trainer's last lesson tied it to, and sent to the guide, where a
	// player leads it with the leash: the request only asks that it comes within 45
	meeting.push_back({.kind = Kind::TakeOffLeash, .delaySeconds = 2.0f});
	meeting.push_back(SendTo(k_CreatureMeets));
	// the making of friends, to the guide's pointing at the pass
	meeting.push_back(WaitText(3931));
	meeting.push_back(WaitText(3945));
	meeting.push_back(LogGuide(0.0f));
	// the guide walks to the Aztec village: the chapter ends there and the food lesson begins
	meeting.push_back(WaitTask("GuideAsksToMeetYourCreature", TaskWait::Finished));
	meeting.push_back(LogGuide(1.0f));
	all.push_back({
	    .id = "quest.guide_meet",
	    .name = "Land 1 quest run: meeting the creature guide and making friends",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_creatures_learning in the same quest run. It starts CreatureGuideSet, clicks "
	                   "the texts, presses on the messenger's gold scroll by the temple, puts the camera on the guide in "
	                   "the pass, then takes the leash off the creature and sends it running to the guide.",
	    .expected = "The messenger's scene (3946, 3947, 5687), the guide's first sight (3949 to 3952), the making of "
	                "friends (3931, 3933 to 3945) and the guide's walk to the village; GuideAsksToMeetYourCreature "
	                "finishes. Unverified: the guide has not yet been met in a game.",
	    .commands = std::move(meeting),
	    .onLand = true,
	});

	// The player's own try of the food lesson, after its film: the guide's miracle tapped with the Action button, then
	// cast on the village store with the same button. Through the testbed's pointer, so the rows on the player's part
	// close only on a real-input run
	std::vector<Command> foodTry {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitText(4007),
	    // [camera] the miracle and the store both in view, from the south-west
	    {.kind = Kind::CameraTo, .delaySeconds = 6.0f, .eye = {2440.0f, 45.0f, 2560.0f}, .look = {2470.0f, 3.0f, 2595.0f}},
	    // [pointer] the miracle tapped: pressed and let go at once
	    {.kind = Kind::PointerTo,
	     .delaySeconds = 1.0f,
	     .thing = ThingNear {.type = script::ObjectType::OneShotSpell, .point = k_MiracleSpot, .radius = 10.0f}},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::OneShotSpell, .point = k_MiracleSpot, .radius = 10.0f}},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton},
	    // "Well done. Now click the Action Button over the Village Store." (4009)
	    WaitText(4009),
	    // [pointer] the Action button clicked over the store
	    {.kind = Kind::PointerTo,
	     .delaySeconds = 1.0f,
	     .thing = ThingNear {.type = script::ObjectType::Abode, .point = k_StoragePit, .radius = 10.0f}},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Abode, .point = k_StoragePit, .radius = 10.0f}},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton},
	    // passed: the dance taught and the guide's farewell for its nap (4011 to 4013)
	    WaitText(4011, false),
	    WaitText(4013),
	    WaitTask("GuideImpressTownLesson", TaskWait::Finished),
	    LogGuide(1.0f),
	};
	all.push_back({
	    .id = "quest.guide_food_try",
	    .name = "Land 1 quest run: the player's try at the creature guide's food lesson",
	    .facet = Facet::Story,
	    .description = "Played after quest.guide_impress_town in the same quest run. Once the film's last line (4007) is "
	                   "read, it puts the camera over the guide's miracle and the village store, taps the miracle with the "
	                   "Action button and clicks the same button over the store.",
	    .expected = "4009 once the tapped miracle is held; food in the store passes the lesson: six more miracles, the "
	                "creature taught the dance, and 4011 to 4013; GuideImpressTownLesson finishes. Unverified: the "
	                "lesson has not yet been run in a game.",
	    .commands = std::move(foodTry),
	    .onLand = true,
	});
	// The chapter after the food lesson, started on its own after the creature is chosen: the guide made by the
	// village asleep under its scroll, the village won, the guide's scene, then its scroll pressed on while it naps,
	// and its five-minute nap waited out. The food lesson that ends the village's belief reset before it is the quest
	// run's GuideNotImpressing=1
	std::vector<Command> naps {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::StartScript, .script = "GuideImpressTownNextLesson"},
	    WaitTask("GuideImpressTownNextLesson", TaskWait::Running),
	    LogGuide(1.0f),
	    CameraOn(k_GuideMade, 40.0f, 60.0f, 2.0f),
	    // [shortcut] the Aztec village takes more belief in the player than in anyone else, as winning it gives, and its
	    // next fold hands it over: the chapter waits, with no time limit, for the village to be the player's
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 10.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_AztecVillage, .radius = k_VillagePickRadius}},
	    // the won village's scene, to the evil advisor's line and the good one's suggestion
	    WaitText(1487),
	    WaitText(1489, false),
	    LogGuide(10.0f),
	    // [camera] back on the sleeping guide
	    CameraOn(k_GuideMade, 40.0f, 60.0f, 2.0f),
	};
	// the guide's scroll pressed on as it sleeps, with the village won
	PressScroll(naps, k_GuideMade, 30.0f);
	naps.push_back(WaitText(1488));
	naps.push_back(LogGuide(10.0f));
	// its nap of five minutes ends the chapter
	naps.push_back(WaitTask("GuideImpressTownNextLesson", TaskWait::Finished));
	naps.push_back(LogGuide(1.0f));
	all.push_back({
	    .id = "quest.guide_naps",
	    .name = "Land 1 quest run: the creature guide's naps while the Aztec village is won",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_sculptor_and_glade in a quest run with GuideNotImpressing=1. It starts "
	                   "GuideImpressTownNextLesson, clicks the texts, gives the Aztec village the belief that wins it "
	                   "for the player, and once the won village's scene is over presses on the sleeping guide's scroll; "
	                   "then it waits out the guide's nap.",
	    .expected = "The guide asleep under a scroll by the village; the won village's scene (1487) and the good "
	                "advisor's suggestion (1489, the Missionaries first); the click on the scroll gives 1488 and the "
	                "suggestion again; five minutes later the chapter finishes. Unverified: the chapter has not yet "
	                "been run in a game.",
	    .commands = std::move(naps),
	    .onLand = true,
	});

	// The fight lesson, started on its own after the creature is chosen: the guide walks to the clearing; its scroll
	// pressed on; the creature sent to it; then the drills, each given as the player's clicks give them in a fight
	std::vector<Command> fight {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::StartScript, .script = "CreatureDevGuideTeachesFight"},
	    WaitTask("CreatureDevGuideTeachesFight", TaskWait::Running),
	    LogGuide(1.0f),
	    // [camera] over the clearing's scroll, within 100 of it: the good advisor's notice while it waits for a click
	    CameraOn(k_FightScroll, 30.0f, 45.0f, 2.0f),
	    // "I'm refreshed and ready to start teaching your little Creature more." (5660)
	    WaitText(5660),
	    // the notice has its first chance at once, and the beckoning goes on meanwhile; the creature points the scroll out
	    // once 40 seconds have passed with the camera this near it, so the press waits past that
	    LogGuide(45.0f),
	};
	PressScroll(fight, k_FightScroll);
	// the creature is far: "Bring our Creature to the big guy. I smell combat!" (3968)
	fight.push_back(WaitText(3968));
	// [shortcut] the creature sent to the guide, where a player leads it with the leash
	fight.push_back(SendTo(k_CreatureToFight));
	// the lesson's scene: the shrinking, the two-shot and the taunts, to the creature looking at the camera
	fight.push_back(WaitText(5663));
	fight.push_back(WaitText(6918));
	// the fight started by the guide; then the attack drill's lines
	fight.push_back(WaitText(5665));
	fight.push_back(LogGuide(0.0f));
	// [hand] attack: the creature first steps up to the guide (the lesson puts the two apart, and a blow is struck only
	// within reach), then blows in the middle, a few seconds apart, until the guide reels from one
	for (size_t step = 0; step < 3; ++step)
	{
		fight.push_back({.kind = Kind::FightStep, .delaySeconds = 2.0f, .value = 0});
	}
	for (size_t blow = 0; blow < 10; ++blow)
	{
		fight.push_back(Blow(1, 3.0f));
	}
	// queuing: "A good hit. You can queue up multiple attacks as well." (5667) comes during the attack blows, so the
	// timeline waits for its repeated "Try queuing up several attacks on me now." (5668), then four blows queued at once
	fight.push_back(WaitText(5668, false));
	fight.push_back(Blow(0, 1.0f));
	fight.push_back(Blow(1, 0.1f, true));
	fight.push_back(Blow(2, 0.1f, true));
	fight.push_back(Blow(1, 0.1f, true));
	// blocking: "Click the Action Button on your Creature to make him block." (5670), then blocks
	fight.push_back(WaitText(5670, false));
	for (size_t block = 0; block < 3; ++block)
	{
		fight.push_back({.kind = Kind::FightBlock, .delaySeconds = 2.0f});
	}
	// the guide's blow and its comment, then dodging once 5675 is said: steps to the right and the left
	fight.push_back(WaitText(5675, false));
	for (size_t step = 0; step < 3; ++step)
	{
		fight.push_back({.kind = Kind::FightStep, .delaySeconds = 3.0f, .value = 2 + step % 2});
	}
	// the friendly bout, then the lesson's end: the guide's lines and its healing, to its farewell (4728)
	fight.push_back(WaitText(5679, false));
	fight.push_back(LogGuide(5.0f));
	fight.push_back(WaitText(4026, false));
	fight.push_back(LogGuide(0.0f));
	fight.push_back(WaitText(4728));
	fight.push_back(WaitTask("CreatureDevGuideTeachesFight", TaskWait::Finished));
	fight.push_back(LogGuide(1.0f));
	all.push_back({
	    .id = "quest.guide_fight",
	    .name = "Land 1 quest run: the creature guide's fight lesson",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_sculptor_and_glade in the same quest run. It starts "
	                   "CreatureDevGuideTeachesFight, clicks the texts, presses on the clearing's scroll, sends the "
	                   "creature to the guide, then gives the drills as the player's fight clicks give them: blows in the "
	                   "middle, four blows queued at once, blocks and steps.",
	    .expected = "The lesson's lines (5660, 3968, 6916, 5662, 5663, 6917, 6918), the four drills passed in turn (5667, "
	                "5669, 5671 or 5672, 5678), the bout, the guide grown back to twice its size, its lines after the "
	                "bout (4026, 4029), the healing miracle taught and cast (4030, 4723) and its farewell (4728). "
	                "Unverified: the lesson has not yet been run in a game.",
	    .commands = std::move(fight),
	    .onLand = true,
	});

	// The storm, started on its own after the creature is chosen with the exit vortex's script before it, as the
	// land's control script starts them: the guide's nap after the fight (its scroll pressed on), its walk to below
	// the mountain (that scroll pressed on), the creature brought to it and up the mountain, then the storm, the
	// guide's death and the storm on the player's village, to the way out opening
	std::vector<Command> storm {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::StartScript, .script = "LeaveThroughVortexL1"},
	    {.kind = Kind::StartScript, .delaySeconds = 0.5f, .script = "TheStorm"},
	    WaitTask("TheStorm", TaskWait::Running),
	    LogGuide(1.0f),
	    CameraOn(k_NapScroll, 30.0f, 45.0f, 2.0f),
	};
	// the guide's nap by the clearing: its scroll pressed on once laid (it is laid 10 east of wherever the guide stopped)
	PressScroll(storm, k_NapScroll, k_NapScrollPickRadius);
	storm.push_back(WaitText(4729));
	storm.push_back(WaitText(4728));
	storm.push_back(LogGuide(1.0f));
	// after its five minutes the guide walks to below the mountain and its scroll is put there, 8 north and 8 west of
	// it: the camera within 100 of it from the north-west, so that the guide, twice the size of the creature, stands
	// behind the scroll rather than in front of it, and the scroll pressed on once it is there: the scroll is the nap's,
	// moved there once the guide arrives, as the script starts to watch it for the press
	storm.push_back(WaitTask("CreatureDevelopmentHighlightNotify", TaskWait::Running));
	storm.push_back({.kind = Kind::CameraTo,
	                 .delaySeconds = 2.0f,
	                 .eye = {k_BelowTheMountainScroll.x - 35.0f, 30.0f, k_BelowTheMountainScroll.y + 35.0f},
	                 .look = {k_BelowTheMountainScroll.x, 3.0f, k_BelowTheMountainScroll.y}});
	PressScroll(storm, k_BelowTheMountainScroll);
	// the creature is far: "Please. Use the Leash to bring your Creature to me." (4713)
	storm.push_back(WaitText(4713));
	// [shortcut] the creature sent to the guide
	storm.push_back(SendTo(k_CreatureBelowTheMountain));
	// the guide's summons, to "Follow me to the mountain. I will tell you everything there." (4704)
	storm.push_back(WaitText(4704));
	storm.push_back(LogGuide(5.0f));
	// [shortcut] the creature sent up the mountain, where a player leads it with the leash; once the storm's first
	// scene is over, sent the last way to within 40 of the top
	storm.push_back(SendTo(k_CreatureOnTheTop, 3.0f));
	storm.push_back(WaitText(4715));
	storm.push_back(SendTo(k_CreatureOnTheTop, 6.0f));
	// the storm on the guide, to "Leave! You can't help me!" (4712), then its death
	storm.push_back(WaitText(4712));
	storm.push_back(LogGuide(0.0f));
	storm.push_back(WaitText(5511));
	storm.push_back(LogGuide(1.0f));
	// the storm on the player's village, then the way out
	storm.push_back(WaitText(4720));
	storm.push_back(WaitText(6097));
	storm.push_back(WaitGlobal("OpenVortexL1", 1.0f));
	storm.push_back(LogGuide(10.0f));
	all.push_back({
	    .id = "quest.guide_storm",
	    .name = "Land 1 quest run: the creature guide's last lesson, the storm and its death",
	    .facet = Facet::Story,
	    .description = "Played after quest.the_sculptor_and_glade in the same quest run. It starts LeaveThroughVortexL1 "
	                   "and TheStorm, clicks the texts, presses on the guide's scroll as it naps and again below the "
	                   "mountain, sends the creature to the guide there and up the mountain top, and waits for the way "
	                   "out to open.",
	    .expected = "The nap's lines (4729, 4728), the guide's summons below the mountain (4713, 4700 to 4704), the "
	                "storm made and its first scene (4714, 4705, 4706, 4715), the storm on the guide (4716 to 4712), "
	                "the lightning, the explosion and the guide dead for good (5235, 5511), the storm on the village "
	                "(4720, 4722, 6098, 6097) and OpenVortexL1 set. Unverified: the chapter has not yet been run in a "
	                "game.",
	    .commands = std::move(storm),
	    .onLand = true,
	});
	// The story run's twins, chained after the trainer's lessons in a natural Land 1 run (no quest= script, no global
	// set): the land's control script starts the guide's wandering as soon as the creature is chosen and each chapter
	// when the one before ends, so the twins only wait for them. The food lesson's film and the player's try
	// (quest.guide_impress_town, quest.guide_food_try) start no script and serve as they are
	all.push_back(StoryTwin(all, "quest.guide_meet", "story.guide_meet",
	                        "Land 1 story: meeting the creature guide and making friends",
	                        "As quest.guide_meet, in a natural run once the trainer's last lesson is done: it waits for the "
	                        "land's control script to start the meeting rather than starting it."));
	// the village won as the player wins it in play: the six practice miracles the food lesson leaves cast on its store
	std::vector<Command> practice;
	for (size_t miracle = 0; miracle < 6; ++miracle)
	{
		TapAndCast(practice, 2.0f);
	}
	all.push_back(StoryTwin(all, "quest.guide_naps", "story.guide_naps",
	                        "Land 1 story: the creature guide's naps while the Aztec village is won",
	                        "As quest.guide_naps, in a natural run after the food lesson: it waits for the chapter, and in "
	                        "place of the belief given to the village casts the food lesson's six practice miracles on its "
	                        "store with the hand; if the village is not won by them the chapter waits on, as in play.",
	                        Kind::TownGainsBelief, practice));
	all.push_back(StoryTwin(all, "quest.guide_fight", "story.guide_fight", "Land 1 story: the creature guide's fight lesson",
	                        "As quest.guide_fight, in a natural run after the naps: it waits for the land's control script to "
	                        "start the lesson rather than starting it."));
	all.push_back(StoryTwin(all, "quest.guide_storm", "story.guide_storm",
	                        "Land 1 story: the creature guide's last lesson, the storm and its death",
	                        "As quest.guide_storm, in a natural run after the fight lesson: it waits for the land's control "
	                        "script to start the storm and the exit vortex's script rather than starting them."));

	// The guide's chapters played through the real input path (tools/checks/realinput/guide_meet_food.txt and
	// guide_fight.txt, through tools/checks/real_input_tap.sh): the timelines only click the texts, wait for the
	// chapters' tasks and texts and put the camera where each of the player's acts is aimed, with no pointer commands,
	// so the replayed mouse and keys are the hand's. Each camera is 40 over the land and 60 back from its point,
	// looking 3 over the land at it, so the point is at the middle of the screen and the land under it at (512, 410)
	// of the 1024 x 768 window

	// The meeting, the creature led to the guide with the leash, and the food lesson with the player's own try
	all.push_back({
	    .id = "realinput.guide_meet_food",
	    .name = "Land 1 real input: the creature guide's meeting and food lesson",
	    .facet = Facet::Story,
	    .description = "Played after the glade's choice in a quest run (quest=SetupLand1/CreaturesInGlade/"
	                   "MoveTheGuideAround&/CreatureGuideSet), the choice and every press and key given by the "
	                   "replayed input. It clicks the texts and puts the camera over the messenger's scroll, beside the "
	                   "guide in the pass (the land where the creature is led to at the middle, the guide on screen and "
	                   "within 90), over the food lesson's scroll, over the guide's miracle and over the village store.",
	    .expected = "The replayed presses click both scrolls, the leash key and the presses on the land lead the "
	                "creature to the guide, the press on the miracle taps it (4009) and the press over the store casts "
	                "it, which passes the lesson (4011 to 4013).",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            WaitTask("MeetTheGuide", TaskWait::Running),
	            // [shortcut] the creature knows the learning leash, as the trainer's lessons the quest run skips teach it
	            {.kind = Kind::KnowLeash, .leash = LeashType::Rope},
	            CameraGate(k_MessengerSpot, 1.0f),
	            WaitText(5687),
	            LogGuide(0.0f),
	            CameraGate(k_CreatureMeets, 6.0f),
	            WaitText(3952),
	            LogGuide(0.0f),
	            WaitText(3931, false),
	            LogGuide(0.0f),
	            WaitTask("GuideImpressTownLesson", TaskWait::Running),
	            CameraGate(k_FoodScroll, 1.0f),
	            WaitText(4007),
	            CameraGate(k_MiracleSpot, 5.0f),
	            WaitText(4009, false),
	            CameraGate(k_StoragePit, 0.5f),
	            WaitText(4011, false),
	            LogGuide(0.0f),
	            WaitTask("GuideImpressTownLesson", TaskWait::Finished),
	            LogGuide(1.0f),
	        },
	    .onLand = true,
	});

	// The fight lesson: its scroll clicked, the creature led to the guide with the leash, then the drills: a click on
	// the guide's body strikes, several queue blows, a click on the creature blocks, a click on the arena's land steps
	all.push_back({
	    .id = "realinput.guide_fight",
	    .name = "Land 1 real input: the creature guide's fight lesson",
	    .facet = Facet::Story,
	    .description = "Played after the glade's choice in a quest run (quest=SetupLand1/CreaturesInGlade/"
	                   "CreatureDevGuideTeachesFight), the choice and every press and key given by the replayed input. "
	                   "It clicks the texts and puts the camera over the clearing's scroll, over the land the creature "
	                   "is led to, then for the drills over the guide, over the creature and over the arena's land "
	                   "beside it.",
	    .expected = "The replayed presses click the scroll, lead the creature to the guide, strike the guide (5667), "
	                "queue three blows (5669), block (5671 or 5672) and step (5678); the bout and the lesson's end "
	                "follow (4026 to 4728).",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            WaitTask("CreatureDevGuideTeachesFight", TaskWait::Running),
	            CameraGate(k_FightScroll, 1.0f),
	            WaitText(3968, false),
	            CameraGate(k_CreatureToFight, 1.0f),
	            WaitText(5665, false),
	            LogGuide(0.0f),
	            CameraGate(k_FightClearing, 0.5f),
	            WaitText(5669, false),
	            CameraGate(k_CreatureFightSpot, 0.5f),
	            WaitText(5674, false),
	            CameraGate(k_ArenaLand, 0.5f),
	            WaitText(5678, false),
	            LogGuide(0.0f),
	            WaitTask("CreatureDevGuideTeachesFight", TaskWait::Finished),
	            LogGuide(1.0f),
	        },
	    .onLand = true,
	});
}
