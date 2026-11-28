/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Land 1's silver scroll The Explorers played to its end, as a land timeline on the game's own Land 1 with the SkipBox
// answered 3 (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): with the creature choice skipped, the
// land's control script starts the challenge at once, which builds the camp on the beach and puts up its scroll. The
// timeline taps the scroll, lets the first verse play, then gives the sailors what they sing for, as a player does: a
// wood miracle poured beside the ark, then a food miracle, then two animals carried to them in the hand. Then the ark
// sails and the dispenser reward plays. The points are map coordinates, taken from the places the challenge's script
// names; the animals' are where Land 1's script makes them.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <vector>

#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedLandTimeline.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

/// The waits for a task of a script: running
constexpr size_t k_Running = 0;
/// The Action (right) button
constexpr size_t k_ActionButton = 3;

/// The ark, where the scroll is put up 16 above it; the challenge looks within 40 of it for what it is given
constexpr glm::vec2 k_Ark {1881.0833f, 3154.1094f};
/// A point of the beach 12 from the ark, clear of its hull, the sailors' seats and the campfire: where the miracles are
/// poured and the animals let go
constexpr glm::vec2 k_GiftSpot {1872.0f, 3145.0f};
/// A cow of the field west of the village, and a horse of the herd 80 from the ark, where Land 1's script makes them;
/// the scripts' sub-types of the two kinds of animal
constexpr uint32_t k_Cow = 8;
constexpr uint32_t k_Horse = 9;
constexpr glm::vec2 k_Cows {1687.0f, 2493.0f};
constexpr glm::vec2 k_Horses {1934.4f, 3095.0f};
/// A child of the Norse village, by its creche
constexpr glm::vec2 k_Creche {1868.01f, 2730.12f};
/// How long the Action button pours a miracle
constexpr float k_PourSeconds = 3.5f;

/// The globals and scripts every end check logs
constexpr std::string_view k_Globals = "MissionariesInitiated GotWood GotGrain GotMeat ArkSailed SailorDeadCount "
                                       "InVerseSection MissionariesCompletedNow";
constexpr std::string_view k_Scripts = "TheMissionaries TheMissionariesVerseOne TheMissionariesBuildingBoat "
                                       "TheMissionariesVerseTwo TheMissionariesVerseThree TheMissionariesReadyToGo "
                                       "GiveSpellDispenserReward";

Command WaitTask(std::string_view script, size_t state)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = state, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::AtLeast};
}

/// Logs the globals and scripts named, some seconds after the command before it; with a period, again every period
Command LogGlobals(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// The camera put 70 south of a point and 45 up, looking at it
Command CameraSouthOf(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 45.0f, at.y - 70.0f}, .look = {at.x, 5.0f, at.y}};
}

/// The scroll over the ark tapped: the hand pointed at it and the Action button pressed once the hand is over it, as the
/// scroll's click test asks
std::vector<Command> TapTheScroll()
{
	return {CameraSouthOf(k_Ark, 1.0f),
	        {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Ark, .value = 1},
	        {.kind = Kind::PointerPress,
	         .delaySeconds = 0.5f,
	         .waitUntilFree = true,
	         .value = k_ActionButton,
	         .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Ark, .radius = 15.0f}},
	        {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton}};
}

/// A miracle of a seed poured beside the ark: the seed given to the hand as a bubble's would be (the skip path's
/// worship offers none yet), the hand held over the gift spot, the Action button held while the grains fall, then let
/// go
std::vector<Command> Pour(SpellSeedType seed, float delay)
{
	return {{.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(seed)},
	        CameraSouthOf(k_GiftSpot, 1.0f),
	        {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_GiftSpot},
	        {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .waitUntilFree = true, .value = k_ActionButton},
	        {.kind = Kind::PointerRelease, .delaySeconds = k_PourSeconds, .value = k_ActionButton},
	        {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1}};
}

/// A thing of the land carried to the gift spot in the hand and let go there: the hand held over the spot takes the
/// thing as if from where it stands (a shortcut of the player's reach over the land to it), then lets go of it
std::vector<Command> Carry(ThingNear thing, float delay)
{
	return {CameraSouthOf(k_GiftSpot, delay),
	        {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_GiftSpot},
	        {.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = thing},
	        {.kind = Kind::HandDrop, .delaySeconds = 1.0f},
	        {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1}};
}

void Append(std::vector<Command>& commands, const std::vector<Command>& more)
{
	commands.insert(commands.end(), more.begin(), more.end());
}

std::vector<Command> Explorers()
{
	std::vector<Command> all {{.kind = Kind::ClickTexts, .value = 1}};
	// the camp and the scroll go up once the creature choice is finished, which the skip path's load of the old creature
	// marks; the scroll's hint script starts with the scroll
	all.push_back(WaitTask("PointOutScroll", k_Running));
	Append(all, TapTheScroll());
	// the first verse, then the advisors' two lines after it
	all.push_back(WaitText(1262, true));
	all.push_back(LogGlobals(1.0f, 60));
	// wood: one pour of the wood miracle beside the ark, each pile it leaves counted and built into the ark in a scene
	Append(all, Pour(SpellSeedType::Wood, 2.0f));
	// the flag is set once the second verse is over
	all.push_back(WaitGlobal("GotWood", 1.0f));
	// once the boat is built, a child brought to the ark becomes the galley boy
	Append(all, Carry(ThingNear {.type = script::ObjectType::VillagerChild, .point = k_Creche, .radius = 60.0f}, 2.0f));
	all.push_back(WaitText(1265, true));
	// grain: one pour of the food miracle beside the ark
	Append(all, Pour(SpellSeedType::Food, 3.0f));
	// the flag is set once the third verse is over
	all.push_back(WaitGlobal("GotGrain", 1.0f));
	// meat: a cow, then a horse, each carried to the ark in the hand
	Append(all,
	       Carry(ThingNear {.type = script::ObjectType::Animal, .subtype = k_Cow, .point = k_Cows, .radius = 30.0f}, 2.0f));
	all.push_back(WaitText(1263, true));
	Append(all,
	       Carry(ThingNear {.type = script::ObjectType::Animal, .subtype = k_Horse, .point = k_Horses, .radius = 30.0f}, 4.0f));
	all.push_back(WaitGlobal("GotMeat", 1.0f));
	// the ark sails, then the dispenser reward; the flag is set once the reward is given
	all.push_back(WaitGlobal("ArkSailed", 1.0f));
	all.push_back(LogGlobals(5.0f));
	return all;
}

/// Adults of the Norse village, a forester and a housewife (the land's only kinds of grown villager there),
/// carried to the ark as the people it is offered; and the accordion player among the sailors by the ark
constexpr uint32_t k_NorseForester = 50;
constexpr uint32_t k_NorseHousewife = 49;
constexpr uint32_t k_Accordionist = 83;
constexpr glm::vec2 k_NorseVillage {1865.61f, 2641.24f};
constexpr float k_VillagePick = 150.0f;
/// Seconds after a gift's last line before the scene it plays is over
constexpr float k_AfterTheScene = 15.0f;
constexpr ThingNear k_Man {.type = script::ObjectType::Villager,
                           .subtype = k_NorseForester,
                           .point = k_NorseVillage,
                           .radius = k_VillagePick,
                           .takeableOnly = true};
constexpr ThingNear k_Woman {.type = script::ObjectType::Villager,
                             .subtype = k_NorseHousewife,
                             .point = k_NorseVillage,
                             .radius = k_VillagePick,
                             .takeableOnly = true};
constexpr ThingNear k_AccordionPlayer {
    .type = script::ObjectType::Villager, .subtype = k_Accordionist, .point = k_Ark, .radius = 40.0f};
/// The tree nearest the ark, carried to it as a gift of unprepared wood
constexpr ThingNear k_BeachTree {.type = script::ObjectType::Tree, .point = k_Ark, .radius = 200.0f};

/// The Explorers' people: a man brought before the boat is built, a tree, then the wood; a woman taken aboard; the
/// accordion player killed (a shortcut of the player's blow); a woman turned away and a man taken as the replacement
std::vector<Command> ExplorersPeople()
{
	std::vector<Command> all {{.kind = Kind::ClickTexts, .value = 1}};
	all.push_back(WaitTask("PointOutScroll", k_Running));
	Append(all, TapTheScroll());
	all.push_back(WaitText(1262, true));
	all.push_back(LogGlobals(1.0f, 60));
	// a man before the boat is built: more company, but the boat first
	Append(all, Carry(k_Man, 2.0f));
	all.push_back(WaitText(1283, true));
	// a tree is no good: the sailors want prepared wood (not waited for, as the script looks for its kind of tree only)
	Append(all, Carry(k_BeachTree, 2.0f));
	all.push_back(LogGlobals(8.0f));
	Append(all, Pour(SpellSeedType::Wood, 2.0f));
	all.push_back(WaitGlobal("GotWood", 1.0f));
	// once the boat is built, a woman taken aboard; no man yet, as a man taken aboard is kept to replace the next sailor
	// who dies at once
	Append(all, Carry(k_Woman, 2.0f));
	all.push_back(WaitText(1285, true));
	// [shortcut] the accordion player killed by a script's SET_PROPERTY of his health, in place of the player's blow,
	// once the woman's scene is over: a villager the script holds keeps his health while a scene plays
	all.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = k_AfterTheScene, .amount = 0.0f, .thing = k_AccordionPlayer});
	all.push_back(WaitGlobal("SailorDeadCount", 1.0f));
	// with a sailor dead, a woman is turned away and a man taken in his place
	Append(all, Carry(k_Woman, 3.0f));
	all.push_back(WaitText(1280, true));
	Append(all, Carry(k_Man, 22.0f));
	all.push_back(WaitText(1282, true));
	all.push_back(LogGlobals(10.0f));
	return all;
}

/// The middle of the three seats
constexpr glm::vec2 k_Seats {1859.2f, 3160.7f};
/// The script globals holding the three sailors, the accordion player last
constexpr std::array<std::string_view, 3> k_SailorGlobals {"GSailor1", "GSailor2", "GSailor3"};
/// How many times each sailor is taken and thrown: a throw along the beach leaves him hurt, the next one kills him
constexpr size_t k_ThrowRounds = 3;

/// The Explorers' sailors killed for the real input path (tools/checks/realinput/explorers_sailors_killed.txt, replayed
/// through tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The
/// timeline clicks the texts and places the camera: over the scroll; once the first verse is over, behind each sailor in
/// turn (the follow shot, wherever he has walked), for his take; then west of the seats in the sea looking east at
/// them, so that the beach runs across the screen, for his throw. The scroll's press, each take with the hand and each
/// throw along the beach are the recording's
std::vector<Command> ExplorersSailorsKilled()
{
	std::vector<Command> all {{.kind = Kind::ClickTexts, .value = 1}};
	all.push_back(WaitTask("PointOutScroll", k_Running));
	all.push_back(CameraSouthOf(k_Ark, 1.0f));
	all.push_back(WaitText(1262, true));
	all.push_back(LogGlobals(1.0f, 30));
	for (size_t round = 0; round < k_ThrowRounds; ++round)
	{
		for (size_t sailor = 0; sailor < k_SailorGlobals.size(); ++sailor)
		{
			// behind him 2 seconds after the start, else 8 seconds after the last throw's camera
			const float delay = round == 0 && sailor == 0 ? 2.0f : 8.0f;
			all.push_back({.kind = Kind::CameraFollow,
			               .delaySeconds = delay,
			               .amount = 1.0f,
			               .thing = ThingNear {.type = script::ObjectType::Villager,
			                                   .point = k_Seats,
			                                   .radius = 300.0f,
			                                   .global = k_SailorGlobals.at(sailor)}});
			all.push_back({.kind = Kind::CameraFollow, .delaySeconds = 5.0f, .value = 1});
			all.push_back({.kind = Kind::CameraTo,
			               .delaySeconds = 1.0f,
			               .eye = {k_Seats.x - 50.0f, 35.0f, k_Seats.y},
			               .look = {k_Seats.x, 3.0f, k_Seats.y}});
		}
	}
	all.push_back(WaitGlobal("SailorDeadCount", 3.0f));
	all.push_back(LogGlobals(20.0f));
	return all;
}

/// The beech nearest the ark, 45 from it, where Land 1's script plants it, the camera high over it looking down at its
/// crown, and the player's influence round it and the ark
constexpr glm::vec2 k_TreeByTheArk {1926.02f, 3153.24f};
constexpr glm::vec2 k_TreeAndArk {1903.5f, 3150.0f};
constexpr float k_TreeAndArkRadius = 60.0f;
constexpr float k_OverTreeHeight = 30.0f;
constexpr float k_OverTreeBack = 20.0f;
constexpr float k_TreeCrownHeight = 6.0f;
/// Seconds over the tree for its pull, then over the beach by the ark for its put-down
constexpr float k_TreePullSeconds = 12.0f;

std::vector<Command> ExplorersTreeGift()
{
	std::vector<Command> all {{.kind = Kind::ClickTexts, .value = 1}};
	all.push_back(WaitTask("PointOutScroll", k_Running));
	all.push_back(CameraSouthOf(k_Ark, 1.0f));
	all.push_back(WaitText(1262, true));
	all.push_back(LogGlobals(1.0f, 30));
	// [shortcut] the player's influence round the tree and the ark, as the land's later towns would give it
	all.push_back({.kind = Kind::ScriptInfluence, .delaySeconds = 0.5f, .point = k_TreeAndArk, .radius = k_TreeAndArkRadius});
	all.push_back({.kind = Kind::CameraTo,
	               .delaySeconds = 1.0f,
	               .eye = {k_TreeByTheArk.x, k_OverTreeHeight, k_TreeByTheArk.y - k_OverTreeBack},
	               .look = {k_TreeByTheArk.x, k_TreeCrownHeight, k_TreeByTheArk.y}});
	all.push_back({.kind = Kind::CameraTo,
	               .delaySeconds = k_TreePullSeconds,
	               .eye = {k_GiftSpot.x, k_OverTreeHeight, k_GiftSpot.y - k_OverTreeBack},
	               .look = {k_GiftSpot.x, 0.0f, k_GiftSpot.y}});
	all.push_back(LogGlobals(20.0f));
	return all;
}
} // namespace

void testbed_scenarios::AddQuestExplorersScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.q_explorers",
	    .name = "Land 1: The Explorers, given wood, grain and meat",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, as land:Land1.txt:<frames>:skip=3/30:"
	                   "timeline=story.q_explorers. Taps the scroll over the ark and lets the first verse play; pours a "
	                   "wood miracle beside the ark; once the boat is built carries a child there; pours a food miracle "
	                   "there; carries a cow, then a horse, there in the hand (each taken from where it stands, a "
	                   "shortcut of the player's reach); then waits for the ark to sail and the dispenser reward.",
	    .expected = "The scroll's tap brings the three sailors out and plays the first verse (texts 4762 to 4773, then "
	                "1261 and 1262). The wood is built into the ark (1273), GotWood=1 and the second verse plays; the "
	                "child becomes the galley boy (1265); the grain is taken (1274), GotGrain=1 and the third verse "
	                "plays; the cow (1263, then 1288) and the horse make GotMeat=1 (1270); the ark sails (1267, 1268, "
	                "1269, ArkSailed=1) and a Water dispenser is built on the beach.",
	    .commands = Explorers(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.q_explorers_people",
	    .name = "Land 1: The Explorers, offered people, a tree, and a sailor killed",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, as story.q_explorers. Taps the scroll over the ark "
	                   "and lets the first verse play; carries a man of the Norse village there, then the nearest tree; "
	                   "pours a wood miracle; carries a woman; kills the accordion player (a shortcut of the "
	                   "player's blow); then carries a woman, and a man (each taken from where it stands, a shortcut of "
	                   "the player's reach).",
	    .expected = "The man before the boat: more company, but the boat first (1283); the tree: the good advisor's "
	                "prepared-wood line (1275); after the wood a woman (1284, 1285) is taken aboard; the "
	                "accordion player's death is recorded and the tune moves to a sailor swapped in; the woman is turned "
	                "away (1280) and the man replaces the dead sailor (1281, 1282), a new sailor walking out of the door.",
	    .commands = ExplorersPeople(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.explorers_tree_gift",
	    .name = "Land 1: The Explorers, a tree pulled up and set down by the ark by hand",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, with the player's mouse from a recording: the "
	                   "timeline only clicks the texts, gives the player influence round the ark and the beech nearest "
	                   "it, and places the camera: over the scroll, then once the first verse is read high over the "
	                   "beech, then over the beach by the ark. The scroll's press, the tree's pull and its put-down are "
	                   "the recording's.",
	    .expected = "The tree set down within 40 of the ark before any wood is given brings the good advisor out once "
	                "with his line (1275).",
	    .commands = ExplorersTreeGift(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.explorers_sailors_killed",
	    .name = "Land 1: The Explorers, the three sailors thrown to their deaths by hand",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, with the player's mouse from a recording: the "
	                   "timeline only clicks the texts and places the camera: over the scroll, then three times round "
	                   "behind each sailor and west of their seats looking east; the scroll's press, each sailor's take "
	                   "with the hand and his throw along the beach are the recording's.",
	    .expected = "Each sailor thrown twice along the beach dies; the first two deaths bring the good advisor's line "
	                "(1266 or 1277) and the third the evil advisor's two (1278, 1279), the record at success 1, "
	                "alignment -1, and the challenge ends with no reward.",
	    .commands = ExplorersSailorsKilled(),
	    .onLand = true,
	});
}
