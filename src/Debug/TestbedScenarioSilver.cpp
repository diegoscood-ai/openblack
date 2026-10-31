/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Land 1's silver scrolls as land timelines: each is played on the game's own Land 1, with the SkipBox answered 3,
// through a land entry's timeline= (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): it taps its
// scroll, waits for its introduction's last text, does what it can of the quest and logs the quest's globals. The
// points are map coordinates, taken from the places the challenge scripts name.

#include <cstddef>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

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

Command WaitTask(std::string_view script)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = k_Running, .script = script};
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

Command SetGlobal(std::string_view global, float value)
{
	return {.kind = Kind::SetGlobal, .amount = value, .global = global};
}

/// The camera put 70 south of a point and 45 up, looking at it, then the hand pointed at the scroll there and the Action
/// button pressed once the hand is over it, as the scroll's click test asks (a press on the thing, not a tap)
std::vector<Command> TapScrollAt(glm::vec2 scroll)
{
	return {{.kind = Kind::CameraTo,
	         .delaySeconds = 1.0f,
	         .eye = {scroll.x, 45.0f, scroll.y - 70.0f},
	         .look = {scroll.x, 5.0f, scroll.y}},
	        {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1},
	        {.kind = Kind::PointerPress,
	         .delaySeconds = 0.5f,
	         .waitUntilFree = true,
	         .value = 3,
	         .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = 15.0f}},
	        {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}};
}

/// A thing of the land moved to a point by the scripts' own SET_POSITION, as a script moves it
Command MoveThingTo(ThingNear thing, glm::vec2 to, float delay = 0.5f)
{
	return {.kind = Kind::MoveThing, .delaySeconds = delay, .point = to, .thing = thing};
}

/// The commands of a silver scroll's scene: the ones before the tap, the tap on its scroll, then the rest
std::vector<Command> Quest(std::vector<Command> before, glm::vec2 scroll, std::vector<Command> after)
{
	std::vector<Command> all {ClickTexts()};
	all.insert(all.end(), before.begin(), before.end());
	const auto tap = TapScrollAt(scroll);
	all.insert(all.end(), tap.begin(), tap.end());
	all.insert(all.end(), after.begin(), after.end());
	return all;
}

// The Land 1 silver scrolls' points, from the challenge scripts: each scroll, and where its beats are
constexpr glm::vec2 k_ThrowingScroll {1947.396f, 2360.235f};
constexpr glm::vec2 k_ThrowingTarget {2031.129f, 2327.405f};
/// The quest's pile of stones, where the first and smallest is made; and a point of the land 8 beyond the pillar the
/// boulder sits on (the land's rock pillar, at 2031.32, 2325.22), seen from the pile, so a stone thrown at it meets the
/// pillar's side on its way down
constexpr glm::vec2 k_ThrowingPile {1961.672f, 2388.286f};
constexpr glm::vec2 k_BeyondThePillar {2037.25f, 2319.85f};
/// The Action (right) button
constexpr size_t k_ActionButton = 3;
constexpr glm::vec2 k_HermitHut {1350.32f, 2394.94f};
constexpr glm::vec2 k_HermitSpot {1363.148f, 2399.396f};
constexpr glm::vec2 k_StonesScroll {2545.003f, 2244.578f};
constexpr glm::vec2 k_SaviourScroll {1755.4685f, 2523.733f};
constexpr glm::vec2 k_SaviourShore {1745.4685f, 2513.733f};
constexpr glm::vec2 k_FlockHouse {1864.9866f, 2769.275f};
constexpr glm::vec2 k_ShepherdPos {1860.348f, 2763.137f};
constexpr glm::vec2 k_PiperCreche {1868.01f, 2730.12f};
constexpr glm::vec2 k_OgreScroll {2553.5718f, 3184.6072f};

/// A singing stone, from where it lies to its hole in the circle
struct SingingStone
{
	glm::vec2 from;
	glm::vec2 hole;
};
constexpr std::array<SingingStone, 5> k_SingingStones {{
    {.from = {1372.46f, 2587.699f}, .hole = {2479.1653f, 2246.3257f}},
    {.from = {1614.094f, 2488.419f}, .hole = {2484.4153f, 2258.5757f}},
    {.from = {1673.924f, 2760.004f}, .hole = {2496.6653f, 2263.8257f}},
    {.from = {2385.508f, 2334.786f}, .hole = {2514.1653f, 2246.3257f}},
    {.from = {1455.505f, 2353.054f}, .hole = {2508.9153f, 2234.0757f}},
}};
/// The drowning men, where the Saviour's script makes them, the last to drown first
constexpr std::array<glm::vec2, 5> k_DrowningMen {{
    {1780.953f, 2460.015f},
    {1777.621f, 2443.252f},
    {1789.306f, 2460.487f},
    {1792.752f, 2449.02f},
    {1777.861f, 2454.776f},
}};
/// The nine lost sheep, where the Lost Flock's script makes them
constexpr std::array<glm::vec2, 9> k_LostSheep {{
    {1802.643f, 2833.384f},
    {1764.388f, 2687.86f},
    {1649.017f, 2619.968f},
    {1457.754f, 2627.792f},
    {1377.553f, 2560.769f},
    {1588.374f, 2268.972f},
    {1538.6907f, 2223.6208f},
    {1842.9502f, 2199.4866f},
    {1996.912f, 2330.437f},
}};
} // namespace

void testbed_scenarios::AddSilverScrollScenarios(std::vector<Scenario>& all)
{
	// Land 1's silver scrolls, each played on Land 1 with the SkipBox answered 3 (skip the creature's training, keep the
	// old creature), as land:Land1.txt:<frames>:skip=3/30:timeline=story.s1_throwing_stones/...: the land's control
	// script starts them all on that path. Each taps its scroll, waits for its introduction's last text and logs the
	// quest's globals; where a beat needs only things moved, they are moved by the scripts' own SET_POSITION
	all.push_back({
	    .id = "story.s1_throwing_stones",
	    .name = "Land 1: Throwing Stones",
	    .facet = Facet::Story,
	    .description = "Waits for ThrowingStones, taps its scroll by the stone pile, waits for the introduction's last "
	                   "text, then moves the target rock off its place, as a stone thrown at it does.",
	    .expected = "The introduction plays (texts 1345 to 1352); once the target rock is off its place the quest ends "
	                "well (End=1, a toy ball's chest from the sky). The end check logs ThrowingStonesStart=1 and End=1.",
	    .commands =
	        Quest({WaitTask("ThrowingStones")}, k_ThrowingScroll,
	              {WaitText(1352, true),
	               MoveThingTo({.type = script::ObjectType::Rock, .point = k_ThrowingTarget, .radius = 3.0f},
	                           k_ThrowingTarget + glm::vec2(8.0f, 0.0f), 5.0f),
	               WaitGlobal("End", 1.0f), LogGlobals("ThrowingStonesStart End ThrowingStonesUsed", "ThrowingStones", 2.0f)}),
	    .onLand = true,
	});
	// The same quest, with a stone of the pile thrown at the pillar by the player's mouse alone: the pointer on the
	// stone, the Action button held past the grab's wait and let go (the stone in the hand), then the hand's throw
	// beyond the pillar. The physics' rest line of the stone says where it stopped
	const ThingNear pileStone {.type = script::ObjectType::Rock, .point = k_ThrowingPile, .radius = 3.0f};
	all.push_back({
	    .id = "story.s1_throwing_stones_pillar",
	    .name = "Land 1: Throwing Stones, a stone thrown at the pillar",
	    .facet = Facet::Story,
	    .description = "Waits for ThrowingStones, taps its scroll by the stone pile, waits for the introduction and the "
	                   "throwing hand demo to end, then takes the pile's first stone with the hand through the mouse and "
	                   "throws it at the pillar the boulder sits on, logging the quest every 2 seconds.",
	    .expected = "The stone stops against the pillar's side (its rest line west of 2028 or on the pillar's top), "
	                "not beyond it: the quest's hit branch says 1355 (the boulder stays) or 1353 (the boulder knocked "
	                "off), and ThrowingStones' task is at that branch's lines. Unverified: that the throw's run-up "
	                "reaches the pillar rather than falling short.",
	    .commands = Quest({WaitTask("ThrowingStones")}, k_ThrowingScroll,
	                      {WaitText(1352, true),
	                       WaitText(5659, true),
	                       {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = pileStone},
	                       {.kind = Kind::PointerPress,
	                        .delaySeconds = 0.3f,
	                        .waitUntilFree = true,
	                        .value = k_ActionButton,
	                        .thing = pileStone},
	                       {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = k_ActionButton},
	                       {.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_BeyondThePillar},
	                       {.kind = Kind::LogGlobals,
	                        .delaySeconds = 2.0f,
	                        .value = 2,
	                        .global = "ThrowingStonesStart End",
	                        .script = "ThrowingStones"}}),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit",
	    .name = "Land 1: The Hermit",
	    .facet = Facet::Story,
	    .description = "Waits for HermitMain, taps the hermit's hut, waits for the advisors' last text, then makes the "
	                   "creature bigger than 0.4 and brings it to the hermit's soap box.",
	    .expected = "The hermit's talk plays (texts 1363 to 1370); a creature bigger than 0.4 near him and he worships it "
	                "(texts 1379, 1381 to 1383): a water miracle dispenser (WaterMiracleAvailable=1). The end check logs "
	                "HermitFinished, "
	                "HermitSeedFound and WaterMiracleAvailable.",
	    .commands =
	        Quest({WaitTask("HermitMain")}, k_HermitHut,
	              {WaitText(1370, true),
	               {.kind = Kind::SetCreatureSize, .delaySeconds = 1.0f, .amount = 0.5f},
	               // brought back to him every two seconds for half a minute, so that its own mind doesn't walk it off
	               // before the hermit's test, which runs once every so many of his turns, sees it
	               {.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               {.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_HermitSpot},
	               // his worship ends with the water miracle's dispenser
	               WaitGlobal("WaterMiracleAvailable", 1.0f),
	               LogGlobals("HermitFinished HermitSeedFound HermitHunting WaterMiracleAvailable", "HermitMain", 5.0f)}),
	    .onLand = true,
	});
	{
		std::vector<Command> stones {WaitText(1344, true)};
		for (const auto& stone : k_SingingStones)
		{
			stones.push_back(
			    MoveThingTo({.type = script::ObjectType::MobileStatic, .subtype = 5, .point = stone.from, .radius = 3.0f},
			                stone.hole, 1.0f));
		}
		stones.push_back(WaitGlobal("SingingStoneCircleFinished", 1.0f));
		stones.push_back(LogGlobals("SingingStonesStart SingingStonesFilled SingingStonesFormed "
		                            "SingingStoneCircleFinished HippyDead",
		                            "SingingStoneCircle", 2.0f));
		all.push_back({
		    .id = "story.s3_singing_stones",
		    .name = "Land 1: The Singing Stones",
		    .facet = Facet::Story,
		    .description = "Waits for the stones' start, taps the scroll by the circle, waits for the hippy's last text, "
		                   "then sets each missing stone in its own hole, as a hand or creature carrying it there does.",
		    .expected = "The hippy's talk plays (texts 1328 to 1344); each stone set in its hole plays its note, and the "
		                "five make the circle sing (SingingStoneCircleFinished=1, a food miracle dispenser in the middle).",
		    .commands = Quest({WaitGlobal("SingingStonesStart", 1.0f), WaitTask("SingingStoneCircle")}, k_StonesScroll,
		                      std::move(stones)),
		    .onLand = true,
		});
	}
	{
		std::vector<Command> men {WaitText(1496, true)};
		for (const auto& man : k_DrowningMen)
		{
			men.push_back(
			    MoveThingTo({.type = script::ObjectType::Villager, .point = man, .radius = 2.0f}, k_SaviourShore, 1.0f));
		}
		men.push_back(WaitGlobal("CreatureSavingPeopleIsFinished", 1.0f));
		men.push_back(LogGlobals("FoundMe CreatureSavingPeopleIsFinished CreatureSavingPeopleSavedCounter "
		                         "CreatureSavingPeopleDrownedCounter",
		                         "CreatureSavingPeople", 2.0f));
		all.push_back({
		    .id = "story.s4_saviour",
		    .name = "Land 1: The Saviour",
		    .facet = Facet::Story,
		    .description = "Waits for CreatureSavingPeople, taps the scroll by the wife on the shore, waits for the "
		                   "introduction's last text, then brings each drowning man ashore, the last to drown first.",
		    .expected = "The introduction plays (texts 1493 to 1496); the five men brought ashore are saved, and the quest "
		                "ends (CreatureSavingPeopleIsFinished=1) with a strength dispenser on the shore.",
		    .commands = Quest({WaitTask("CreatureSavingPeople")}, k_SaviourScroll, std::move(men)),
		    .onLand = true,
		});
	}
	{
		std::vector<Command> sheep {WaitText(1450, true), WaitGlobal("LostFlockInitiated", 1.0f)};
		for (const auto& at : k_LostSheep)
		{
			// the hand takes the sheep, then puts it down by the shepherd with the Action button, where it lands and joins
			// his flock
			const auto drop = k_ShepherdPos + glm::vec2(4.0f, 4.0f);
			sheep.push_back(
			    {.kind = Kind::HandTake,
			     .delaySeconds = 1.0f,
			     .thing = ThingNear {.type = script::ObjectType::Animal, .subtype = 4, .point = at, .radius = 15.0f}});
			sheep.push_back({.kind = Kind::CameraTo,
			                 .delaySeconds = 0.5f,
			                 .eye = {drop.x, 35.0f, drop.y - 50.0f},
			                 .look = {drop.x, 3.0f, drop.y}});
			sheep.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = drop});
			sheep.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3});
			sheep.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3});
			sheep.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
			// the shepherd's thanks for each sheep are spoken before the next is brought, so his count goes up one by one
			sheep.push_back(LogGlobals("GotRewardedOnce", "TheLostFlock", 12.0f));
		}
		sheep.push_back(LogGlobals("LostFlockWanted LostFlockInitiated TheLostFlockFinished LostFlockCompletedNow "
		                           "GotRewardedOnce",
		                           "TheLostFlock", 20.0f));
		all.push_back({
		    .id = "story.s5_lost_flock",
		    .name = "Land 1: The Lost Flock",
		    .facet = Facet::Story,
		    .description = "Waits for the flock to be wanted, taps the shepherd's house, waits for the advisors' last "
		                   "text and the quest's start, then takes each of the nine lost sheep in the hand and puts it "
		                   "down by the shepherd.",
		    .expected = "The shepherd's talk plays (texts 1445 to 1450, LostFlockInitiated=1); each sheep put down lands "
		                "and joins his flock, and at six in it he rewards the player (GotRewardedOnce=1), at eleven a "
		                "second time. The hand's take is the testbed's; the put-down is the Action button's.",
		    .commands = Quest({WaitGlobal("LostFlockWanted", 1.0f)}, k_FlockHouse, std::move(sheep)),
		    .onLand = true,
		});
	}
	all.push_back({
	    .id = "story.s6_pied_piper",
	    .name = "Land 1: The Pied Piper (its introduction)",
	    .facet = Facet::Story,
	    .description = "Waits for PiedPiper, taps the scroll at the Norse creche and waits for the introduction's last "
	                   "text.",
	    .expected = "The mother's plea plays (texts 1391 to 1395, then 1409 and 1410); PiedPiperInitiated=1 and five "
	                "children are in his cave.",
	    .commands = Quest({WaitTask("PiedPiper")}, k_PiperCreche,
	                      {WaitText(1410, true),
	                       LogGlobals("PiedPiperInitiated PiedPiperChildrenInCave PiedPiperFinished", "PiedPiper", 2.0f)}),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s7_ogre",
	    .name = "Land 1: The Ogre (its introduction)",
	    .facet = Facet::Story,
	    .description = "Raises the flag the guide's fight lesson raises, which the skip path never runs, taps the "
	                   "ogre's scroll and waits for the introduction's last text.",
	    .expected = "Sleg is made at his lair from the player's creature; the scroll's tap plays the introduction (texts "
	                "5688 to 1469) and CreatureGuardianInitiated=1.",
	    .commands = Quest(
	        {WaitTask("CreatureGuardian"), SetGlobal("CreatureDevelopment_Fight", 1.0f)}, k_OgreScroll,
	        {WaitText(1469, true), LogGlobals("CreatureDevelopment_Fight CreatureGuardianInitiated CreatureGuardianFinished "
	                                          "EndSendMyCreatureAwayFromGuardianFlag",
	                                          "CreatureGuardian", 2.0f)}),
	    .onLand = true,
	});
}
