/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's gold scroll Impress Village as a quest run, played after Worship Site's timeline in the same run
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). Once the worship lesson is done it starts the
// gestures lesson and the influence watcher on the Indian village by the store, as the land's control script and its
// expansion watcher start them, then drives the quest as a player would: the scroll is pressed on with the hand, the
// miracle selection is opened and the food miracle asked for with the hand's gestures, and food is poured on the
// village's store. The village then takes belief in the player, as a gift gives it, standing in for the impressing
// that wins it: the town's own belief fold hands it to the player, which lets the influence lesson start. A town merely
// handed over without belief goes back to the neutral player at its next fold. Three more runs take the branches one
// run can't: the village won first with the practice run out, Khazar gone, and the village wiped out (its buildings
// and its people: a village whose people survive rebuilds). A fifth wins the village by the hand alone, with food
// poured at the edge of the player's influence nearest the village until its belief in the player is the strongest.
// The points are map coordinates, taken from the places the challenge scripts and the land's script name; none of them
// has yet been seen in a running game.

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
using testbed_land_timeline::ScriptArgument;
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// Where the camera is put for a scroll or a building: this far above the land and back from it
constexpr float k_EyeAbove = 25.0f;
constexpr float k_EyeBack = 30.0f;
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 15.0f;

/// The Indian village the quest is about, where the land's script makes it
constexpr glm::vec2 k_Village {2862.58f, 3448.27f};
constexpr float k_VillagePickRadius = 20.0f;
constexpr ThingNear k_VillageThing {.type = script::ObjectType::Town, .point = k_Village, .radius = k_VillagePickRadius};
/// The village's store, where the quest looks for it, which the food is poured on
constexpr glm::vec2 k_Store {2978.64f, 3433.42f};
/// The gold scroll, where Khazar's hand lays it
constexpr glm::vec2 k_Scroll {3127.059f, 3316.982f};
/// The player's village centre, inside the player's influence: the camera looks at it while the gestures are drawn
constexpr glm::vec2 k_VillageCentre {3233.325f, 3398.706f};
/// A gesture takes a second or two to draw; the next one waits for it and the hand's rest after it
constexpr float k_DrawSeconds = 3.0f;
/// How many food miracles are poured on the store, and for how long each: the quest waits for more than 2,800 food in
/// it. The first seed given finds the hand still holding the food seed the S gesture asked for (the hand takes a seed
/// only when empty), so the first pour pours that one; the villagers eat from the store as it fills, and five pours
/// gave 2,816 in one run and 2,728 in another: six leave room for that
constexpr int k_FoodMiracles = 6;
constexpr float k_PourSeconds = 6.0f;
/// The belief in the player the village takes to be won: more than its belief in the neutral player (0.5) and than the
/// most the land's script lets it hold in Khazar (0.75), so its next fold makes it the player's
constexpr float k_WinningBelief = 2.0f;

/// The village won by the hand alone. Every miracle the player has on the land (food, wood and water, and the one-off
/// seeds) impresses villagers the same way, so they all tire the town alike, and a thing thrown impresses only the
/// villagers near the hand; the village lies beyond the player's influence, so the food is poured at the point of the
/// influence nearest its houses, about 52 from the door of the nearest one. First a rest of three minutes, long enough
/// for the town to get over a miracle that tired it wholly (the store's food and Khazar's wood), then a short pour
/// every ten seconds, the time the town takes to get over the tiring of a pour that three villagers see
constexpr glm::vec2 k_EdgeOfInfluence {2949.0f, 3412.0f};
constexpr float k_ByHandRestSeconds = 180.0f;
constexpr int k_ByHandPours = 45;
constexpr float k_ByHandPourSeconds = 2.0f;
constexpr float k_ByHandPourGap = 6.0f;
/// How often the run logs the quest's globals while the hand pours, so that the turn the village becomes the player's
/// is bracketed in the log as well as in the state hash's trace
constexpr size_t k_ByHandLogPeriod = 30;

/// The globals and scripts the quest's end is read by
constexpr std::string_view k_ImpressGlobals = "LearnGesturesCompleted KhazarGone DidLearnGesturesBeforeOwningTown "
                                              "TriggeredGestures KhazarInScript";
constexpr std::string_view k_ImpressScripts = "LearnGestures PlayMiracleHandDemo InfluenceCheck LearnInfluence "
                                              "Land2FinalScroll";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// A script started as a new task with the village as its one value, as its caller starts it
Command StartOnVillage(std::string_view script)
{
	return {.kind = Kind::StartScript,
	        .script = script,
	        .scriptArguments = {ScriptArgument {.kind = ScriptArgument::Kind::Thing,
	                                            .thingType = static_cast<uint8_t>(script::ObjectType::Town),
	                                            .thingPoint = k_Village,
	                                            .thingRadius = k_VillagePickRadius}}};
}

Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}

Command SetGlobal(std::string_view global, float value)
{
	return {.kind = Kind::SetGlobal, .amount = value, .global = global};
}

/// Logs the globals and scripts named, some seconds after the command before it, and again every period if given
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = globals, .script = scripts};
}

/// The camera put back from a point and above it, looking at it; both heights are above the land there, as the
/// timeline's camera takes them
Command CameraOn(glm::vec2 point)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, k_EyeAbove, point.y - k_EyeBack},
	        .look = {point.x, 0.0f, point.y}};
}

/// A gesture drawn with the empty hand across the middle of the screen; a command to the hand waits for the one before
Command Draw(GestureType gesture)
{
	return {.kind = Kind::DrawGesture, .delaySeconds = k_DrawSeconds, .value = static_cast<size_t>(gesture)};
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

/// A food miracle in the hand, some seconds after the command before it, poured over a point for some seconds, with the
/// Action button held
void PourFood(std::vector<Command>& commands, glm::vec2 point, float pourSeconds = k_PourSeconds, float delay = 1.0f)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(SpellSeedType::Food)});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = point});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = pourSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

/// The village takes more belief in the player than in anyone else, as a gift gives it; its next fold hands it over
Command WinVillage()
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 2.0f,
	        .amount = k_WinningBelief,
	        .thing = k_VillageThing};
}

/// Which of the quest's branches a run takes; each run takes some that the others can't
enum class Branch : uint8_t
{
	/// The practice done, Khazar alive, the store filled before the village is won
	Main,
	/// The village won before the scroll is pressed on, and the practice left to run out with only the selection
	/// opened, so that a gesture count matches again once and Khazar's line is said twice
	WonFirstTimedOut,
	/// Khazar gone before the scroll is laid: his lessons are skipped, and the influence lesson is only its info sign
	KhazarGone,
	/// The village wiped out, its buildings and its people, while the store waits for its food
	VillageDestroyed,
	/// As Main, but the village is won by the hand's food alone, with no belief given to it
	ByHand,
};

std::vector<Command> ImpressCommands(Branch branch)
{
	// Once the worship lesson is done, the land's control script starts the influence watcher on the village, and the
	// expansion watcher, once its 20 minutes are over with the home village still the player's, starts the gestures
	// lesson on it and marks that it did; the run starts both at once
	std::vector<Command> impress {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitGlobal("LearnWorshipCompleted", 1.0f),
	};
	if (branch == Branch::KhazarGone)
	{
		// set before the lesson starts, so that the scroll's wait ends as soon as it is laid
		impress.push_back(SetGlobal("KhazarGone", 1.0f));
	}
	impress.push_back(StartOnVillage("InfluenceCheck"));
	impress.push_back(StartOnVillage("LearnGestures"));
	impress.push_back(SetGlobal("TriggeredGestures", 1.0f));
	if (branch == Branch::KhazarGone)
	{
		// the lesson marks itself done at once; once the village is the player's the influence lesson shows its sign
		impress.push_back(WaitGlobal("LearnGesturesCompleted", 1.0f));
		impress.push_back(WinVillage());
		impress.push_back(WaitTask("LearnInfluence"));
		impress.push_back(WaitTask("LearnInfluence", TaskWait::Finished));
		impress.push_back(LogGlobals(k_ImpressGlobals, k_ImpressScripts, 2.0f));
		return impress;
	}
	if (branch == Branch::WonFirstTimedOut)
	{
		impress.push_back(WinVillage());
	}
	// Khazar's hand flies to the scroll's spot while the lesson has him; the scroll is laid before it lets him go
	impress.push_back(WaitGlobal("KhazarInScript", 1.0f));
	impress.push_back(WaitGlobal("KhazarInScript", 0.0f));
	impress.push_back(CameraOn(k_Scroll));
	PressScroll(impress, k_Scroll);
	// Khazar's film and the gestures hand demo, which ends on his line asking the player to try it; the practice then
	// waits for the miracle selection to be opened and a miracle asked for: the spiral, then the food miracle's S. Left
	// at the spiral, it runs out after its 5 minutes
	impress.push_back(WaitText(6076));
	impress.push_back(CameraOn(k_VillageCentre));
	impress.push_back({.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.5f}});
	impress.push_back(Draw(GestureType::Spiral));
	if (branch != Branch::WonFirstTimedOut)
	{
		impress.push_back(Draw(GestureType::SShape));
	}
	// the second film, at the village, ends on Khazar's line about the store
	impress.push_back(WaitText(6926));
	impress.push_back(CameraOn(k_Store));
	if (branch == Branch::VillageDestroyed)
	{
		// the store's wait ends with the village gone, and the lesson marks itself done
		impress.push_back({.kind = Kind::KnockDownTown, .delaySeconds = 2.0f, .thing = k_VillageThing});
		impress.push_back(WaitGlobal("LearnGesturesCompleted", 1.0f));
		impress.push_back(LogGlobals(k_ImpressGlobals, k_ImpressScripts, 10.0f));
		return impress;
	}
	for (int i = 0; i < k_FoodMiracles; ++i)
	{
		PourFood(impress, k_Store);
	}
	if (branch == Branch::WonFirstTimedOut)
	{
		// with the village the player's already, the store's film has only Khazar's first line
		impress.push_back(WaitText(6081));
	}
	else if (branch == Branch::ByHand)
	{
		// the store's film ends on Khazar's second line while the village is not yet the player's; the hand then wins
		// it: the rest, then the pours at the edge of the influence, the globals logged all the while
		impress.push_back(WaitText(6082));
		impress.push_back(LogGlobals(k_ImpressGlobals, k_ImpressScripts, 0.0f, k_ByHandLogPeriod));
		impress.push_back(CameraOn(k_EdgeOfInfluence));
		impress.push_back(LogGlobals(k_ImpressGlobals, k_ImpressScripts, k_ByHandRestSeconds));
		for (int i = 0; i < k_ByHandPours; ++i)
		{
			PourFood(impress, k_EdgeOfInfluence, k_ByHandPourSeconds, k_ByHandPourGap);
		}
	}
	else
	{
		// the store's film ends on Khazar's second line while the village is not yet the player's; the village is
		// then won
		impress.push_back(WaitText(6082));
		impress.push_back(WinVillage());
	}
	// the influence lesson plays to its last line
	impress.push_back(WaitGlobal("LearnGesturesCompleted", 1.0f));
	impress.push_back(WaitTask("LearnInfluence"));
	impress.push_back(WaitText(6090));
	impress.push_back(WaitTask("LearnInfluence", TaskWait::Finished));
	impress.push_back(LogGlobals(k_ImpressGlobals, k_ImpressScripts, 2.0f));
	return impress;
}
} // namespace

void testbed_scenarios::AddImpressVillageQuest(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.impress_village",
	    .name = "Land 2 quest run: Impress Village",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.impress_village). The texts that wait for a click are clicked "
	                   "on. Once the worship lesson is done it starts the influence watcher and the gestures lesson on "
	                   "the Indian village by the store, as the land's control script and its expansion watcher do, "
	                   "presses on Khazar's scroll with the hand, draws the spiral and the food miracle's S after the "
	                   "gestures hand demo, pours six food miracles on the village's store after the second film, gives "
	                   "the village belief in the player after the store's film, so that it becomes the player's, and "
	                   "waits for the influence lesson to end.",
	    .expected = "Khazar's hand lays the scroll; his film, the gestures hand demo and the log at 0; the selection "
	                "and the request end the practice with his praise; the store is emptied, the second film and the "
	                "log at 0.5; with more than 2,800 food in the store his film with both lines, the log at 1, and once "
	                "the village is the player's the influence lesson (its camera path, the Influence hand demo, the "
	                "info sign); the end check logs LearnGesturesCompleted=1 and DidLearnGesturesBeforeOwningTown=1. "
	                "Unverified: that the gestures work under the lesson's interface limit, that the store is inside "
	                "the player's influence for the hand to pour on it, and that six pours give 2,800 food.",
	    .commands = ImpressCommands(Branch::Main),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.impress_village_won_first",
	    .name = "Land 2 quest run: Impress Village, the village won first and the practice run out",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.impress_village_won_first). As quest.impress_village, but the "
	                   "village is given belief in the player before the scroll is laid, so that it is the player's "
	                   "first, and only the spiral is drawn after the gestures hand demo, so that the practice runs "
	                   "out.",
	    .expected = "After 5 minutes Khazar's line that it doesn't matter, twice (the selection's count matches again "
	                "once), and no praise; the store must still be filled; with more than 2,800 food only his first "
	                "line, as a one-line dialogue; the influence lesson without the camera path; the end check logs "
	                "LearnGesturesCompleted=1 and DidLearnGesturesBeforeOwningTown=0.",
	    .commands = ImpressCommands(Branch::WonFirstTimedOut),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.impress_village_khazar_gone",
	    .name = "Land 2 quest run: Impress Village with Khazar gone",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.impress_village_khazar_gone). KhazarGone is set to 1 before "
	                   "the gestures lesson and the influence watcher start, standing in for Khazar's death; once the "
	                   "lesson is done the village is given belief in the player, and the run waits for the influence "
	                   "lesson to end.",
	    .expected = "The scroll is laid and its wait ends at once; no film, demo or store step; the lesson marks itself "
	                "done; once the village is the player's the influence lesson shows only its info sign.",
	    .commands = ImpressCommands(Branch::KhazarGone),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.impress_village_destroyed",
	    .name = "Land 2 quest run: Impress Village with the village knocked down",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.impress_village_destroyed). As quest.impress_village up to the "
	                   "second film; then the village's buildings and villagers are deleted while the store waits for "
	                   "food.",
	    .expected = "The store step ends without its film, the log at 1, and the lesson marks itself done; the end "
	                "check logs whether the influence lesson started.",
	    .commands = ImpressCommands(Branch::VillageDestroyed),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.impress_village_by_hand",
	    .name = "Land 2 quest run: Impress Village, the village won by the hand's food alone",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.impress_village_by_hand). As quest.impress_village up to the "
	                   "end of the store's film, but no belief is given to the village. After Khazar's second line the "
	                   "quest's globals are logged every 30 seconds, the camera looks at the edge of the player's "
	                   "influence nearest the village, and after a rest of three minutes the hand pours food there for "
	                   "two seconds every ten seconds, 45 times, through the pointer and the Action button; the run then "
	                   "waits for the influence lesson to end.",
	    .expected = "Every miracle the player has on the land (food, wood, water and the one-off seeds) impresses through "
	                "the same reaction, looking at a nice miracle, so there is no other kind of miracle to turn to; the "
	                "rest lets that reaction's tiring, which the store's food and Khazar's wood may have used up, come "
	                "back (+0.00056 a turn, 1786 turns from 0 to 1). The village's houses lie beyond the influence: from "
	                "its edge a pour reaches only the three villagers of the nearest house (52.4 from its door, within "
	                "the reaction's reach of 60), at a distance share of 0.7356. Each villager impressed adds its share "
	                "of the town (30.001 / 36.001 = 0.83334) x the food's worth (2 x 0.0033) x 0.7356 x the tiring (1, "
	                "0.98333, 0.96667 for the three) = 0.011935 a pour, and tires the town by 0.05 a pour, which ten "
	                "seconds give back. 42 pours pass the neutral player's 0.5 (0.50112; 41 give 0.48919): with the "
	                "three villagers at home at every pour, the village becomes the player's at the fold after the 42nd "
	                "pour, about 600 seconds (6,000 turns) after Khazar's second line, when the script's check of the "
	                "town's player returns 1 and LearnGesturesCompleted turns 1; villagers who come by the store add "
	                "to it and make it sooner. The town's belief in each player shows each turn in the state hash's "
	                "town belief part. Unverified: where the villagers are while the hand pours, that the point is "
	                "inside the influence for the hand to pour there, how many times a short pour impresses a "
	                "villager, how much Khazar's wood took and gave him, and that pours left over after the win don't "
	                "disturb the influence lesson. Run on joint294 (2026-10-30): the village was the player's at turn "
	                "7483, about 30 seconds after the rest, after two pours at the edge (every other seed is refused, "
	                "the hand still holding the last one), so the six pours on the store gave most of its belief.",
	    .commands = ImpressCommands(Branch::ByHand),
	    .onLand = true,
	});
}
