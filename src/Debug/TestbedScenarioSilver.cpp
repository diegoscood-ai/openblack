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
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedLand2Quests.h"
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
/// The spot on the hill the fisherman walks to and watches the throws from; and a point of the slope below it, on the
/// way to the pillar but 20 short of it, for a throw that misses: a stone held low over him is thrown only downhill
constexpr glm::vec2 k_FishermanSpot {1944.409f, 2365.232f};
constexpr glm::vec2 k_ShortOfThePillar {2012.8f, 2335.4f};
/// The Action (right) button
constexpr size_t k_ActionButton = 3;
constexpr glm::vec2 k_HermitHut {1350.32f, 2394.94f};
constexpr glm::vec2 k_HermitSpot {1363.148f, 2399.396f};
/// The spot the hermit wanders round, where his script sends him back to; the rock the quest puts on the hill above the
/// hut; a point of the land about 90 from his spot, on his way to the Norse village, beyond the 75 his script lets him
/// stray and the 50 within which the creature counts as near him; and the Norse village's store he takes his revenge on
constexpr glm::vec2 k_HermitSoapBox {1354.4812f, 2406.957f};
constexpr glm::vec2 k_HermitRock {1378.6321f, 2288.384f};
constexpr glm::vec2 k_HermitAway {1435.0f, 2448.0f};
constexpr glm::vec2 k_NorseStore {1819.83f, 2645.601f};
/// The hermit, a villager of his own kind as his script makes him, wherever he stands within about 100 of his spot
constexpr uint32_t k_HermitKind = 66;
constexpr ThingNear k_Hermit {
    .type = script::ObjectType::Villager, .subtype = k_HermitKind, .point = k_HermitSoapBox, .radius = 100.0f};
/// His hut, the rock and the reward chest that falls beside his spot. The script makes the rock as a mobile static of
/// the second kind, a rock's: a thing of the rock class, which the scripts then see as a rock of that kind
constexpr ThingNear k_HermitHutThing {.type = script::ObjectType::Abode, .point = k_HermitHut, .radius = 5.0f};
constexpr ThingNear k_HermitRockThing {.type = script::ObjectType::Rock, .subtype = 2, .point = k_HermitRock, .radius = 3.0f};
constexpr ThingNear k_HermitReward {.type = script::ObjectType::Reward, .point = k_HermitSoapBox, .radius = 10.0f};
/// The globals and scripts the quest is read by
constexpr std::string_view k_HermitGlobals = "HermitFinished HermitSeedFound HermitHunting WaterMiracleAvailable";
constexpr std::string_view k_HermitScripts = "HermitMain HermitLastSeen HermitSeedHidden HermitWorshipping "
                                             "HermitWardPact HermitKilled";

Command HermitLog(float delay)
{
	return LogGlobals(k_HermitGlobals, k_HermitScripts, delay);
}

/// [shortcut] The creature, or the hermit, put at a point by a script's SET_POSITION every two seconds for a while, in
/// place of the player walking the creature there or carrying the hermit, so that the creature's own mind doesn't walk
/// it off before the hermit's test, which runs once every so many of his turns, sees it
void KeepAt(std::vector<Command>& commands, glm::vec2 point, size_t times, std::optional<ThingNear> thing = std::nullopt)
{
	for (size_t i = 0; i < times; ++i)
	{
		commands.push_back({.kind = Kind::MoveThing, .delaySeconds = i == 0 ? 1.0f : 2.0f, .point = point, .thing = thing});
	}
}

/// [shortcut] The creature made bigger than 0.4 and brought to the hermit, in place of the player growing it and walking
/// it there: he worships it, and his worship ends with the water miracle's dispenser
void ImpressTheHermit(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::SetCreatureSize, .delaySeconds = 1.0f, .amount = 0.5f});
	KeepAt(commands, k_HermitSpot, 16);
	commands.push_back(WaitGlobal("WaterMiracleAvailable", 1.0f));
}

/// [shortcut] The rock moved off its place on the hill by a script's SET_POSITION, in place of the creature picking it
/// up: the seed under it appears
Command MoveTheRock(float delay)
{
	return MoveThingTo(k_HermitRockThing, k_HermitRock + glm::vec2(8.0f, 0.0f), delay);
}

/// [shortcut] The hermit killed by a script's SET_PROPERTY of his health, in place of the player's or the creature's
/// blow
Command KillTheHermit(float delay)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = 0.0f, .thing = k_Hermit};
}

/// [shortcut] The hut damaged by a script's SET_PROPERTY of its health, in place of the player's throw or miracle
Command DamageTheHut(float delay)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = 0.5f, .thing = k_HermitHutThing};
}

/// The hermit's scroll tapped and his introduction let play, up to the advisors' last line, then the rest
std::vector<Command> HermitIntroduction(const std::vector<Command>& after)
{
	std::vector<Command> commands {WaitText(1370, true), HermitLog(1.0f)};
	commands.insert(commands.end(), after.begin(), after.end());
	return Quest({WaitTask("HermitMain")}, k_HermitHut, commands);
}

/// The scroll pressed again once the introduction has taken the log entry's snapshot, for its reminder (2492, the good
/// advisor's); then impressed, and the rock moved while he stands at his spot: the seed's scene with him and his last
/// walk into his hut
std::vector<Command> HermitImpressed()
{
	std::vector<Command> commands;
	land2_quests::ReplayReminder(commands, k_HermitHut, 2492);
	ImpressTheHermit(commands);
	commands.push_back(HermitLog(5.0f));
	// once the dispenser's scene is over, the seed: its scene waits for the screen to be free
	commands.push_back(MoveTheRock(20.0f));
	commands.push_back(WaitText(1386, true));
	commands.push_back(WaitGlobal("HermitFinished", 2.0f));
	commands.push_back(HermitLog(2.0f));
	return HermitIntroduction(commands);
}

/// The rock moved before he is impressed, while he stands at his spot: the seed's scene with him; then impressed, the
/// record goes straight to done and he walks into his hut
std::vector<Command> HermitSeedFirst()
{
	std::vector<Command> commands {MoveTheRock(3.0f), WaitText(1386, true), HermitLog(5.0f)};
	ImpressTheHermit(commands);
	commands.push_back(WaitGlobal("HermitFinished", 2.0f));
	commands.push_back(HermitLog(2.0f));
	return HermitIntroduction(commands);
}

/// The hermit put far from his spot, so he walks back; then killed before he is impressed or offended: the death scene;
/// then the rock moved with him dead: the seed's scene without him
std::vector<Command> HermitKilledFirst()
{
	std::vector<Command> commands;
	KeepAt(commands, k_HermitAway, 1, k_Hermit);
	// his walk back, about 90, and his wandering at his spot again
	commands.push_back(HermitLog(90.0f));
	commands.push_back(KillTheHermit(1.0f));
	commands.push_back(WaitText(1378, true));
	commands.push_back(HermitLog(12.0f));
	commands.push_back(MoveTheRock(1.0f));
	commands.push_back(WaitText(1389, true));
	commands.push_back(HermitLog(5.0f));
	return HermitIntroduction(commands);
}

/// The hand's press on a thing: the pointer on it, then the Action button pressed once the hand is over it and let go
void PressOn(std::vector<Command>& commands, const ThingNear& thing)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = thing});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = thing});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// The camera put back from a point and above it, looking at it
Command CameraOver(glm::vec2 point, float eyeHeight, float back)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, eyeHeight, point.y - back},
	        .look = {point.x, 5.0f, point.y}};
}

/// The hut damaged: his despair, the reward from the sky, the advisors; the reward's chest tapped; his walk to the Norse
/// village's store with the camera there, the fire and his dance, and his fade at his hut once out of sight
std::vector<Command> HermitHutDamaged()
{
	std::vector<Command> commands {DamageTheHut(3.0f), WaitText(1373, true), WaitText(1375, true), HermitLog(1.0f)};
	// [camera] his spot; [real] the reward's chest beside it pressed on with the hand
	commands.push_back(CameraOver(k_HermitSoapBox, 25.0f, 30.0f));
	PressOn(commands, k_HermitReward);
	commands.push_back(HermitLog(5.0f));
	// [camera] the store, within the 100 his script waits for, on the screen; his scene there
	commands.push_back(CameraOver(k_NorseStore, 45.0f, 70.0f));
	commands.push_back(WaitText(1376, true));
	commands.push_back(WaitText(1377, true));
	commands.push_back(WaitText(1390, true));
	commands.push_back(HermitLog(5.0f));
	// his walk home, out of the camera's sight, and his fade there
	commands.push_back(WaitGlobal("HermitFinished", 3.0f));
	commands.push_back(HermitLog(2.0f));
	return HermitIntroduction(commands);
}

/// The hut damaged, then the hermit killed on his way to his revenge: the death scene in its place
std::vector<Command> HermitHutDamagedThenKilled()
{
	return HermitIntroduction({DamageTheHut(3.0f), WaitText(1375, true), HermitLog(1.0f), KillTheHermit(1.0f),
	                           WaitText(1378, true), HermitLog(12.0f)});
}

/// A small creature brought to him, smaller than 0.2 and then between 0.2 and 0.4: each size taunted once he tests it,
/// then taken more than 50 away for a few of his tests and brought back, for the second taunt. His tests come every
/// three seconds or so and a taunt's text is on the screen only a few seconds, so the run keeps to its times rather than
/// waiting on the texts; the log has them
std::vector<Command> HermitTaunts()
{
	constexpr std::array<float, 2> k_Sizes {0.15f, 0.3f};
	std::vector<Command> commands;
	for (const float size : k_Sizes)
	{
		commands.push_back({.kind = Kind::SetCreatureSize, .delaySeconds = 1.0f, .amount = size});
		KeepAt(commands, k_HermitSpot, 16);
		commands.push_back(HermitLog(1.0f));
		KeepAt(commands, k_HermitAway, 8);
		KeepAt(commands, k_HermitSpot, 16);
		commands.push_back(HermitLog(5.0f));
	}
	return HermitIntroduction(commands);
}
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
/// The circle's holes the stones are carried between, a place inside the circle away from every hole, and the first bad
/// stone, where the circle's script makes it
constexpr glm::vec2 k_Hole3 {2479.1653f, 2246.3257f};
constexpr glm::vec2 k_Hole4 {2484.4153f, 2258.5757f};
constexpr glm::vec2 k_Hole8 {2508.9153f, 2234.0757f};
constexpr glm::vec2 k_CircleMiddle {2496.6653f, 2246.3257f};
constexpr glm::vec2 k_CircleAside {2497.0f, 2252.0f};
constexpr glm::vec2 k_BadStone1 {2219.089f, 2304.378f};
/// The carries, in order: the third and fourth stones each into the other's hole, the fifth and seventh into their own,
/// the first bad stone into the eighth hole; that bad stone out to the middle and the eighth stone into its hole; then
/// the third and fourth stones swapped into their own holes
constexpr std::array<std::pair<glm::vec2, glm::vec2>, 10> k_ByHandCarries {{
    {k_SingingStones[0].from, k_Hole4},
    {k_SingingStones[1].from, k_Hole3},
    {k_SingingStones[2].from, k_SingingStones[2].hole},
    {k_SingingStones[3].from, k_SingingStones[3].hole},
    {k_BadStone1, k_Hole8},
    {k_Hole8, k_CircleMiddle},
    {k_SingingStones[4].from, k_Hole8},
    {k_Hole4, k_CircleAside},
    {k_Hole3, k_Hole4},
    {k_CircleAside, k_Hole3},
}};
/// The camera put 6 south of a point and 30 up, looking 1 above the land there, so that what stands there is in the
/// middle of the screen, seen from above past the trees and stones round it
Command StoneCameraOn(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 30.0f, at.y - 6.0f}, .look = {at.x, 1.0f, at.y}};
}
/// [camera] Each carry: the stone where it lies, then 3 seconds later the place it is let go, 8 seconds a carry, and 15
/// more after the fifth and the seventh, which fill every hole, for the circle's check and its scene
void CarryTheStones(std::vector<Command>& commands, float firstDelay)
{
	for (size_t carry = 0; carry < k_ByHandCarries.size(); ++carry)
	{
		const auto& [from, to] = k_ByHandCarries.at(carry);
		commands.push_back(StoneCameraOn(from, carry == 0 ? firstDelay : 2.0f));
		commands.push_back(StoneCameraOn(to, 3.0f));
		commands.push_back(LogGlobals("SingingStonesFilled SingingStonesFilledWithBad SingingStonesFormed HippyDead",
		                              "SingingStoneCircle", carry == 4 || carry == 6 ? 18.0f : 3.0f));
	}
	commands.push_back(WaitGlobal("SingingStoneCircleFinished", 1.0f));
	commands.push_back(LogGlobals("SingingStonesStart SingingStonesFilled SingingStonesFormed "
	                              "SingingStoneCircleFinished HippyDead",
	                              "SingingStoneCircle", 2.0f));
}
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
/// The Lost Flock's field, where the shepherd keeps his flock and the sheep creature is made; the spot by his house the
/// sheep are put down at; another in the field, for a dead one; and where his reward is put
constexpr glm::vec2 k_FlockField {1852.9114f, 2780.7761f};
constexpr glm::vec2 k_FlockDrop {k_ShepherdPos.x + 4.0f, k_ShepherdPos.y + 4.0f};
constexpr glm::vec2 k_FlockDeadDrop {1846.9114f, 2770.7761f};
constexpr glm::vec2 k_FlockReward {1848.5687f, 2757.708f};
/// The shepherd's lines for the fifth to the eighth sheep found, in turn: one for the first after his reward, then the
/// three he cycles through
constexpr std::array<uint32_t, 4> k_FlockThanks {1456, 1457, 1458, 1459};
/// A lost sheep, of the scripts' sheep sub-type, standing where its script made it; a living sheep of the flock in the
/// field
constexpr uint32_t k_SheepSubtype = 4;
ThingNear LostSheep(glm::vec2 at)
{
	return {.type = script::ObjectType::Animal, .subtype = k_SheepSubtype, .point = at, .radius = 15.0f};
}
constexpr ThingNear k_FieldSheep {
    .type = script::ObjectType::Animal, .subtype = k_SheepSubtype, .point = k_FlockField, .radius = 25.0f, .aliveOnly = true};
/// The sheep creature, of the scripts' sheep species, made in the field
constexpr ThingNear k_SheepCreature {
    .type = script::ObjectType::Creature, .subtype = 11, .point = k_FlockField, .radius = 25.0f};
constexpr ThingNear k_FlockRewardChest {.type = script::ObjectType::Reward, .point = k_FlockReward, .radius = 10.0f};

void Append(std::vector<Command>& commands, const std::vector<Command>& more)
{
	commands.insert(commands.end(), more.begin(), more.end());
}

/// The camera put 50 south of a point and 35 up, looking at it
Command FlockCameraOn(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 35.0f, at.y - 50.0f}, .look = {at.x, 3.0f, at.y}};
}

/// What the hand holds put down at a point with the Action button, where it lands
void PutDownHeld(std::vector<Command>& commands, glm::vec2 drop)
{
	commands.push_back(FlockCameraOn(drop, 0.5f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = drop});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

/// A thing taken into the hand through the player's mouse: the camera on it, the pointer on it, the Action button held
/// past the grab's wait and let go, the thing then in the hand
void TakeByMouse(std::vector<Command>& commands, const ThingNear& thing)
{
	commands.push_back(FlockCameraOn(thing.point, 1.0f));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = thing});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .waitUntilFree = true, .value = k_ActionButton, .thing = thing});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = k_ActionButton});
}

/// A lost sheep taken by the player's mouse and put down by the shepherd, where it lands and joins his flock
void CarrySheepByMouse(std::vector<Command>& commands, glm::vec2 at)
{
	TakeByMouse(commands, LostSheep(at));
	PutDownHeld(commands, k_FlockDrop);
}

/// [shortcut] A sheep of the flock killed in the field by a script's SET_PROPERTY of its health, in place of the player's
/// blow or miracle
Command KillFieldSheep(float delay)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = 0.0f, .thing = k_FieldSheep};
}

/// The Lost Flock up to the end of its introduction: the flock wanted, the shepherd's house tapped, the advisors' last
/// text and the quest's start, then the commands after
std::vector<Command> LostFlockStarted(const std::vector<Command>& after)
{
	std::vector<Command> started {WaitText(1450, true), WaitGlobal("LostFlockInitiated", 1.0f)};
	Append(started, after);
	return Quest({WaitGlobal("LostFlockWanted", 1.0f)}, k_FlockHouse, std::move(started));
}

/// The sheep creature's offer taken up: its scroll tapped with the player's creature far off; the creature put by it
/// and the scroll tapped again, so it walks up and the good advisor asks; the scroll tapped once more and the sheep
/// clicked once it is asked, which swaps them. (Not played: clicking the player's own creature to call the swap off; a
/// land timeline's pick has no way yet to tell the player's creature from the sheep)
std::vector<Command> SwapToTheSheep()
{
	std::vector<Command> swap = TapScrollAt(k_FlockField);
	swap.push_back(WaitText(5104, true));
	// [shortcut] the player's creature put 20 from the sheep creature, as the leash would lead it there
	swap.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_FlockField + glm::vec2(-20.0f, 0.0f)});
	Append(swap, TapScrollAt(k_FlockField));
	swap.push_back(WaitText(2420, false));
	// the scroll made again over the sheep; the sheep clicked once the creatures have met and the question is asked
	swap.push_back(LogGlobals("TheLostFlockFinished GlobalCurrentCreature", "SwapCreatures", 4.0f));
	Append(swap, TapScrollAt(k_FlockField));
	swap.push_back({.kind = Kind::PointerTo, .delaySeconds = 10.0f, .thing = k_SheepCreature});
	swap.push_back({.kind = Kind::PointerPress,
	                .delaySeconds = 0.5f,
	                .waitUntilFree = true,
	                .value = k_ActionButton,
	                .thing = k_SheepCreature});
	swap.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	swap.push_back(WaitText(5105, true));
	swap.push_back(LogGlobals("TheLostFlockFinished GlobalCurrentCreature", "SwapCreatures", 5.0f));
	return swap;
}

/// Any sheep of the scripts' sheep sub-type flying near the shepherd's field: the one the hand has just thrown
constexpr ThingNear k_ThrownSheep {
    .type = script::ObjectType::Animal, .subtype = k_SheepSubtype, .point = k_FlockField, .radius = 300.0f, .flyingOnly = true};

/// How long the camera stays behind a thing in the real input path's Lost Flock, for its take with the hand
constexpr float k_FollowSeconds = 6.0f;
/// How long it stays behind a thrown sheep, through its flight and its fall, until its take
constexpr float k_ThrownFollowSeconds = 12.0f;
/// How many times a sheep of the flock is thrown before its body is put down in the field
constexpr size_t k_ThrowsToKill = 3;

/// A round of the real input path's Lost Flock: the camera behind a lost sheep for its take (the follow shot), then
/// over the spot by the shepherd's house for its put-down; 15 seconds in all, 8 after the round before
void BringByFollow(std::vector<Command>& commands, glm::vec2 sheep, float delay = 8.0f)
{
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = delay, .amount = 1.0f, .thing = LostSheep(sheep)});
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_FollowSeconds, .value = 1});
	commands.push_back(FlockCameraOn(k_FlockDrop, 1.0f));
}

/// The same round with the camera 20 to the north-east of a lost sheep, 10 up, looking at its middle, for one a south
/// follow shot finds behind a rock or among other animals
void BringByLook(std::vector<Command>& commands, glm::vec2 sheep)
{
	commands.push_back({.kind = Kind::CameraTo,
	                    .delaySeconds = 8.0f,
	                    .eye = {sheep.x + 12.0f, 10.0f, sheep.y + 16.0f},
	                    .look = {sheep.x, 0.5f, sheep.y}});
	commands.push_back(FlockCameraOn(k_FlockDrop, k_FollowSeconds + 1.0f));
}

/// A round on a sheep of the shepherd's flock: the camera behind it for its take; let go, so that it stays there for
/// its throw; then behind the thrown sheep through its flight and fall for its take and the next throw, until the last,
/// after which its body is put down in the field (the camera over the spot)
void KillByThrows(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 8.0f, .amount = 1.0f, .thing = k_FieldSheep});
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_FollowSeconds, .value = 1});
	for (size_t i = 0; i < k_ThrowsToKill; ++i)
	{
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 2.0f, .amount = 1.0f, .thing = k_ThrownSheep});
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_ThrownFollowSeconds, .value = 1});
	}
	commands.push_back(FlockCameraOn(k_FlockDeadDrop, 1.0f));
	commands.push_back(LogGlobals("LostFlockCompletedNow TheLostFlockFinished", "TheLostFlock", 2.0f));
}

/// The Lost Flock played through the real input path (tools/checks/realinput/the_lost_flock.txt, replayed through
/// tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The timeline clicks
/// the texts and places the camera: over the shepherd's house for the scroll; behind or beside each lost sheep in turn
/// for its take, then over the spot by his house for its put-down; behind sheep of his flock for their takes and throws,
/// and over the field for their bodies' put-downs. Four sheep brought, a pause for his reward's scene, one more, one of
/// his flock thrown, the other four tried, then four more of his flock tried. The presses are the recording's; what each
/// round takes is in the recording's notes
std::vector<Command> LostFlockRealInput()
{
	std::vector<Command> all {ClickTexts(), WaitGlobal("LostFlockWanted", 1.0f)};
	all.push_back(TapScrollAt(k_FlockHouse).front());
	all.push_back(WaitText(1450, true));
	all.push_back(WaitGlobal("LostFlockInitiated", 1.0f));
	BringByFollow(all, k_LostSheep.at(0), 2.0f);
	BringByFollow(all, k_LostSheep.at(3));
	BringByFollow(all, k_LostSheep.at(4));
	BringByFollow(all, k_LostSheep.at(5));
	// his reward's scene and the advisors' lines after it
	BringByFollow(all, k_LostSheep.at(6), 48.0f);
	KillByThrows(all);
	BringByFollow(all, k_LostSheep.at(7));
	BringByLook(all, k_LostSheep.at(1));
	BringByLook(all, k_LostSheep.at(2));
	BringByLook(all, k_LostSheep.at(8));
	for (size_t i = 0; i < 4; ++i)
	{
		KillByThrows(all);
	}
	all.push_back(LogGlobals("LostFlockInitiated TheLostFlockFinished LostFlockCompletedNow", "TheLostFlock", 20.0f));
	return all;
}

/// Where Throwing Stones' pile stones lie for the real input path's takes: the four the introduction makes that the
/// throwing hand demo leaves where they settled, then the five spots the pile's watchers put a stone back at once it has
/// come to rest far away (the spots they are made at)
constexpr std::array<glm::vec2, 9> k_PileStoneSpots {{
    {1962.53f, 2387.85f},
    {1965.57f, 2381.31f},
    {1969.39f, 2385.80f},
    {1969.51f, 2388.65f},
    {1960.672f, 2384.286f},
    {1961.672f, 2388.286f},
    {1963.672f, 2382.286f},
    {1966.672f, 2388.286f},
    {1966.672f, 2393.286f},
}};
/// Where Throwing Stones' reward falls
constexpr glm::vec2 k_ThrowingReward {2023.378f, 2326.985f};
/// How many rounds of a take and a throw the real input path's Throwing Stones plays
constexpr size_t k_ThrowingRounds = 16;
/// The stones that lie where the introduction left them, taken in the first rounds
constexpr size_t k_SettledStones = 4;

/// The camera 25 above a point and 4 to the south, looking down at it, so that a press at the screen's middle meets
/// the thing lying there
Command CameraAbove(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 25.0f, at.y - 4.0f}, .look = {at.x, 0.5f, at.y}};
}

/// Throwing Stones played through the real input path (tools/checks/realinput/throwing_stones.txt, replayed through
/// tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The timeline
/// clicks the texts and places the camera: over the scroll; then, from the throwing hand demo's last line, rounds of
/// 15 seconds: above a stone of the pile for its take (4 seconds), behind the pile looking down the slope to the
/// pillar, with the fisherman's spot in view, for its throw (8 seconds), and above the reward's spot (3 seconds). The
/// presses are the recording's
std::vector<Command> ThrowingStonesRealInput()
{
	std::vector<Command> all {ClickTexts(), WaitTask("ThrowingStones")};
	all.push_back(TapScrollAt(k_ThrowingScroll).front());
	all.push_back(WaitText(1352, true));
	all.push_back(WaitText(5659, true));
	const size_t putBackSpots = k_PileStoneSpots.size() - k_SettledStones;
	for (size_t round = 0; round < k_ThrowingRounds; ++round)
	{
		const size_t spot = round < k_SettledStones ? round : k_SettledStones + ((round - k_SettledStones) % putBackSpots);
		all.push_back(CameraAbove(k_PileStoneSpots.at(spot), 2.0f));
		all.push_back(
		    {.kind = Kind::CameraTo, .delaySeconds = 4.0f, .eye = {1925.0f, 60.0f, 2425.0f}, .look = {1970.0f, 0.0f, 2370.0f}});
		all.push_back(CameraAbove(k_ThrowingReward, 8.0f));
		all.push_back(LogGlobals("ThrowingStonesStart End ThrowRewardCount", "ThrowingStones", 1.0f));
	}
	return all;
}

/// The Saviour's wife, the villager the quest's script holds as the fisherman's wife, wherever she has walked (the
/// introduction walks her along the shore); a drowning man where the script makes him; and a man just brought out,
/// near the shore where he is put down (runs have found him there and 11 from it, before the script lets him walk off
/// to the village), with the wife 40 away
constexpr ThingNear k_SaviourWife {
    .type = script::ObjectType::Villager, .point = {1775.0f, 2483.0f}, .radius = 6.0f, .global = "FisherWife"};
constexpr ThingNear k_SavedMan {.type = script::ObjectType::Villager, .point = k_SaviourShore, .radius = 15.0f};
[[nodiscard]] constexpr ThingNear DrowningMan(glm::vec2 man)
{
	return {.type = script::ObjectType::Villager, .point = man, .radius = 2.0f};
}

/// A blow that leaves a villager no life
constexpr float k_DeadlyBlow = 10.0f;
/// The men brought out before the wife is killed, in the branch that kills her
constexpr size_t k_WifeKilledAfterMen = 3;

/// [shortcut] A villager killed by an effect's blow through the game's own damage, where the player's miracle or the
/// creature would strike him (a script's setting of the health kills nobody a script holds); where he stands is logged
/// first
void KillVillager(std::vector<Command>& commands, const ThingNear& villager, float delay = 1.0f)
{
	commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = delay, .thing = villager});
	commands.push_back({.kind = Kind::EffectHit,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = 0.0f,
	                    .amount = k_DeadlyBlow,
	                    .thing = villager});
}

/// The Saviour's other endings: from the introduction's last text, the men brought out (the first `saved` of them,
/// to the shore, and the first `killedSaved` of those killed there, each before the next is brought), the next
/// `killedInWater` killed where they swim, the rest left to drown; the wife killed when `wifeKilled`, once the film is
/// over
struct SaviourBranch
{
	std::string_view id;
	std::string_view name;
	std::string_view description;
	std::string_view expected;
	size_t saved;
	size_t killedSaved;
	size_t killedInWater;
	bool wifeKilled;
};

constexpr std::array<SaviourBranch, 5> k_SaviourBranches {{
    {.id = "story.s4_saviour_drowned",
     .name = "Land 1: The Saviour, every man left to drown",
     .description = "Waits for CreatureSavingPeople, taps the scroll by the wife on the shore, waits for the "
                    "introduction's last text, then leaves the five men in the water.",
     .expected = "Each man drowns as the time runs out; the closing scene has no one saved and no one killed (text "
                 "1500 and the advisors' lines after it).",
     .saved = 0,
     .killedSaved = 0,
     .killedInWater = 0,
     .wifeKilled = false},
    {.id = "story.s4_saviour_all_killed",
     .name = "Land 1: The Saviour, every man killed in the water",
     .description = "As story.s4_saviour_drowned, but kills each of the five men where he swims (a shortcut of the "
                    "player's blows).",
     .expected = "The evil advisor's line for each man who leaves the water dead without drowning; the closing scene's "
                 "murder ending (text 1505, both advisors) and a dispenser.",
     .saved = 0,
     .killedSaved = 0,
     .killedInWater = 5,
     .wifeKilled = false},
    {.id = "story.s4_saviour_some_saved",
     .name = "Land 1: The Saviour, two saved, one of them killed, one killed in the water",
     .description = "As story.s4_saviour_drowned, but brings two men out to the shore and kills the first of them "
                    "there, kills a third where he swims (shortcuts of the player's hand and blows), and leaves two "
                    "to drown.",
     .expected = "The evil advisor's line for the saved man killed and for the man killed in the water; the closing "
                 "scene's some-saved ending (text 1506) with its killed lines (1507, 1508).",
     .saved = 2,
     .killedSaved = 1,
     .killedInWater = 1,
     .wifeKilled = false},
    {.id = "story.s4_saviour_none_saved_some_killed",
     .name = "Land 1: The Saviour, two killed in the water, the rest drowned",
     .description = "As story.s4_saviour_drowned, but kills two of the men where they swim (a shortcut of the player's "
                    "blows), and leaves three to drown.",
     .expected = "The closing scene's none-saved ending with murders (texts 1507 and 1508).",
     .saved = 0,
     .killedSaved = 0,
     .killedInWater = 2,
     .wifeKilled = false},
    {.id = "story.s4_saviour_wife_killed",
     .name = "Land 1: The Saviour, the wife killed, every man saved",
     .description = "As story.s4_saviour, but brings the five men out to the shore, killing the wife there once the "
                    "introduction's film is over, after the third (a shortcut of the player's blows).",
     .expected = "The closing scene makes a man (her brother) and a witness on the shore; the brother's line (text "
                 "1509), then the all-saved-but-the-wife ending (1506).",
     .saved = 5,
     .killedSaved = 0,
     .killedInWater = 0,
     .wifeKilled = true},
}};

[[nodiscard]] std::vector<Command> SaviourBranchCommands(const SaviourBranch& branch)
{
	std::vector<Command> men {WaitText(1496, true)};
	size_t next = 0;
	for (; next < branch.saved; ++next)
	{
		// the wife can be killed only once the introduction's film is over, a couple of seconds after its last text is
		// read, while the first men are being brought out
		if (branch.wifeKilled && next == k_WifeKilledAfterMen)
		{
			KillVillager(men, k_SaviourWife, 3.0f);
		}
		men.push_back(MoveThingTo(DrowningMan(k_DrowningMen.at(next)), k_SaviourShore, 1.0f));
		// killed on the shore once his script has seen him out of the water, before he walks off
		if (next < branch.killedSaved)
		{
			KillVillager(men, k_SavedMan, 2.0f);
		}
	}
	for (size_t i = 0; i < branch.killedInWater; ++i, ++next)
	{
		KillVillager(men, DrowningMan(k_DrowningMen.at(next)));
	}
	men.push_back(WaitGlobal("CreatureSavingPeopleIsFinished", 1.0f));
	// when the last man to die drowned, the closing scene waits for the scroll to be tapped again
	if (branch.saved + branch.killedInWater < k_DrowningMen.size())
	{
		Append(men, TapScrollAt(k_SaviourScroll));
	}
	men.push_back(LogGlobals("FoundMe CreatureSavingPeopleIsFinished CreatureSavingPeopleSavedCounter "
	                         "CreatureSavingPeopleDrownedCounter",
	                         "CreatureSavingPeople", 30.0f));
	return men;
}

void AddSaviourBranches(std::vector<Scenario>& all)
{
	for (const auto& branch : k_SaviourBranches)
	{
		all.push_back({
		    .id = branch.id,
		    .name = branch.name,
		    .facet = Facet::Story,
		    .description = branch.description,
		    .expected = branch.expected,
		    .commands = Quest({WaitTask("CreatureSavingPeople")}, k_SaviourScroll, SaviourBranchCommands(branch)),
		    .onLand = true,
		});
	}
}
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
	// The same quest, with the fisherman's watch: a stone of the pile taken by the player's mouse and held over the
	// fisherman, then thrown onto the hill far from the pillar; the boulder is then moved off its place as in the first
	all.push_back({
	    .id = "story.s1_throwing_stones_fisherman",
	    .name = "Land 1: Throwing Stones, the fisherman's watch",
	    .facet = Facet::Story,
	    .description = "Waits for ThrowingStones, taps its scroll by the stone pile, waits for the introduction and the "
	                   "throwing hand demo to end, then takes the pile's first stone with the hand through the mouse, holds "
	                   "it over the fisherman's spot for 4 seconds and throws it down the slope towards the pillar, "
	                   "landing 20 short of it; then moves the target rock off its place, as a stone thrown at it "
	                   "does.",
	    .expected = "The fisherman watches the stone in the hand and ducks while it is within 10 of him (his 270), "
	                "follows its flight and, as it lands more than 12 from the pillar, despairs (his 386) and, with the "
	                "dialogue free, says 1359. Once the target rock is off its place the quest ends well (End=1). "
	                "Unverified: that the hand over his spot brings the stone within 10 of him, and that the throw "
	                "lands on the hill.",
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
	                       {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_FishermanSpot},
	                       {.kind = Kind::HandOver, .delaySeconds = 4.0f, .value = 1},
	                       {.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_ShortOfThePillar},
	                       LogGlobals("ThrowingStonesStart End", "ThrowingStones", 8.0f),
	                       MoveThingTo({.type = script::ObjectType::Rock, .point = k_ThrowingTarget, .radius = 3.0f},
	                                   k_ThrowingTarget + glm::vec2(8.0f, 0.0f), 5.0f),
	                       WaitGlobal("End", 1.0f),
	                       LogGlobals("ThrowingStonesStart End ThrowingStonesUsed", "ThrowingStones", 2.0f)}),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit",
	    .name = "Land 1: The Hermit",
	    .facet = Facet::Story,
	    .description = "Waits for HermitMain, taps the hermit's hut, waits for the advisors' last text, presses the "
	                   "scroll at the hut again for its reminder, then makes the creature bigger than 0.4 and brings it to "
	                   "the hermit's soap box; once he has worshipped it, moves the rock off its place on the hill.",
	    .expected = "The hermit's talk plays (texts 1363 to 1370); a creature bigger than 0.4 near him and he worships it "
	                "(texts 1379, 1381 to 1383): a water miracle dispenser (WaterMiracleAvailable=1). The rock moved, "
	                "the strong creature seed appears and his scene about the fireflies plays (texts 1384 to 1386); the "
	                "record is updated to done and he walks into his hut and is gone (HermitFinished=2, "
	                "HermitSeedFound=1). The scroll's second press, while the quest runs, speaks the reminder (text 2492).",
	    .commands = HermitImpressed(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit_seed_first",
	    .name = "Land 1: The Hermit, the seed found before he is impressed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.s2_hermit does, moves the rock off its place while the hermit is "
	                   "at his spot, then makes the creature bigger than 0.4 and brings it to him.",
	    .expected = "The seed and his firefly scene (texts 1384 to 1386, HermitSeedFound=1); his worship (text 1379) "
	                "then skips the rock's lines: the record goes straight to done with alignment 0, the dispenser is "
	                "given, and he walks into his hut and is gone (HermitFinished=2).",
	    .commands = HermitSeedFirst(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit_killed",
	    .name = "Land 1: The Hermit, put far away, then killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.s2_hermit does, puts the hermit about 90 from his spot by a "
	                   "script's SET_POSITION, lets him walk back, kills him by a script's SET_PROPERTY of his health, "
	                   "then moves the rock off its place.",
	    .expected = "Far from his spot he is sent walking back to it and wanders there again; his death plays the death "
	                "scene (text 1378), closes the record with alignment -0.6 (HermitFinished=1); the rock moved with "
	                "him dead, the seed's scene without him (texts 1387 to 1389).",
	    .commands = HermitKilledFirst(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit_hut",
	    .name = "Land 1: The Hermit, his hut damaged",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.s2_hermit does, damages the hut by a script's SET_PROPERTY of its "
	                   "health, presses on the reward chest that falls by his spot with the hand, then puts the camera "
	                   "over the Norse village's store.",
	    .expected = "His despair (text 1373), the water miracle reward from the sky, the record at 0.99 with alignment "
	                "-0.4, the advisors (1374, 1375), HermitHunting=1; the chest's press speaks its help. He walks to "
	                "the store; with the camera there his scene plays (1376, 1377), the store is set on fire and he "
	                "dances (1390); back at his hut, out of sight, he fades away (HermitFinished=3). Unverified: that "
	                "the hand's press reaches the chest outside the player's influence.",
	    .commands = HermitHutDamaged(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit_hut_killed",
	    .name = "Land 1: The Hermit, his hut damaged, then killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.s2_hermit does, damages the hut by a script's SET_PROPERTY of its "
	                   "health, then kills the hermit by a script's SET_PROPERTY of his health as he sets off.",
	    .expected = "His despair and the advisors (texts 1373 to 1375); his death on the way stops his revenge and plays "
	                "the death scene (text 1378), which leaves HermitFinished=1.",
	    .commands = HermitHutDamagedThenKilled(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.s2_hermit_taunts",
	    .name = "Land 1: The Hermit, a small creature",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.s2_hermit does, then makes the creature 0.15 and keeps it by the "
	                   "hermit, takes it about 90 away and back; then the same at 0.3.",
	    .expected = "At 0.15 his first taunt (text 1371), then after the creature's absence the second (1380); at 0.3 "
	                "his first taunt (1372), then after the absence the second (1380). He is not impressed "
	                "(HermitFinished=0).",
	    .commands = HermitTaunts(),
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
		// The wrong-stone and wrong-order branches and the circle's completion for the real input path
		// (tools/checks/realinput/singing_stones_by_hand.plan): no pointer commands, so the replayed mouse alone drives
		// the hand. The timeline clicks the texts and only places the camera, at fixed times after the hippy's last line
		// of the introduction: over the scroll, then over each stone where it lies and over the place it is carried to.
		// The recording presses on the scroll, then takes and lets go of each stone with the Action button
		std::vector<Command> commands {ClickTexts(), WaitGlobal("SingingStonesStart", 1.0f), WaitTask("SingingStoneCircle")};
		// [camera] the scroll, 70 south of it and 45 up, as TapScrollAt places it
		commands.push_back(TapScrollAt(k_StonesScroll).front());
		commands.push_back(WaitText(1344, true));
		CarryTheStones(commands, 8.0f);
		all.push_back({
		    .id = "story.s3_singing_stones_by_hand",
		    .name = "Land 1: The Singing Stones, the stones carried by hand",
		    .facet = Facet::Story,
		    .description = "With the player's mouse from a recording: the timeline only clicks the texts and places the "
		                   "camera, over the scroll, then over each stone and the place it is carried to. The scroll's "
		                   "press and every stone's take and let-go are the recording's: the third and fourth stones each "
		                   "into the other's hole, the fifth and seventh into their own, a wrong stone into the eighth; "
		                   "the wrong stone out and the eighth in; then the third and fourth swapped into their own.",
		    .expected = "The wrong stone among the five filled holes plays the hippy's scene (texts 1338, 1339); the five "
		                "right stones in a wrong order his other scene (1336, 1343); once each is in its own hole the "
		                "circle sings (1342, 1331, 1332) and a food miracle dispenser is given in its middle.",
		    .commands = std::move(commands),
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
	AddSaviourBranches(all);
	{
		// The wife-killed ending for the real input path (tools/checks/realinput/saviour_real.txt, replayed through
		// tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The
		// timeline clicks the texts, places the camera over the scroll and then over the wife, and waits; the scroll's
		// press, her take with the hand and her throw inland are the recording's. The men are still brought out by the
		// scripts' own move, standing in for the creature carrying them
		std::vector<Command> commands {ClickTexts(), WaitTask("CreatureSavingPeople")};
		// [camera] the scroll, 70 south of it and 45 up, as TapScrollAt places it
		commands.push_back(TapScrollAt(k_SaviourScroll).front());
		commands.push_back(WaitText(1496, true));
		for (size_t next = 0; next < k_WifeKilledAfterMen; ++next)
		{
			commands.push_back(MoveThingTo(DrowningMan(k_DrowningMen.at(next)), k_SaviourShore, 1.0f));
		}
		// [camera] over the wife once the film is over, 50 south of her and 35 up
		commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = 3.0f, .thing = k_SaviourWife});
		commands.push_back(FlockCameraOn(k_SaviourWife.point, 0.0f));
		commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = 6.0f, .thing = k_SaviourWife});
		for (size_t next = k_WifeKilledAfterMen; next < k_DrowningMen.size(); ++next)
		{
			commands.push_back(MoveThingTo(DrowningMan(k_DrowningMen.at(next)), k_SaviourShore, 1.0f));
		}
		commands.push_back(WaitGlobal("CreatureSavingPeopleIsFinished", 1.0f));
		commands.push_back(LogGlobals("FoundMe CreatureSavingPeopleIsFinished CreatureSavingPeopleSavedCounter "
		                              "CreatureSavingPeopleDrownedCounter",
		                              "CreatureSavingPeople", 30.0f));
		all.push_back({
		    .id = "story.s4_saviour_wife_killed_real",
		    .name = "Land 1: The Saviour, the wife thrown to her death by hand, every man saved",
		    .facet = Facet::Story,
		    .description = "As story.s4_saviour_wife_killed, with the player's mouse from a recording: the timeline only "
		                   "places the camera, over the scroll and then over the wife; the scroll's press, her take with "
		                   "the hand and her throw inland are the recording's.",
		    .expected = "Thrown hard, she dies where she lands; the closing scene makes her brother and a witness on "
		                "the shore, the brother's line (text 1509), then the all-saved-but-the-wife ending (1506).",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		// The some-saved ending for the real input path (tools/checks/realinput/saviour_some_saved_real.txt): no pointer
		// commands. The timeline clicks the texts and places the camera over the scroll, then over the first man to
		// drown, then over the shore where a man brought out is put down, and logs where the first man and a man on the
		// shore are; the leash key, the taps that send the creature to the man and to the shore, and the take and the
		// throw of a man brought out are the recording's
		std::vector<Command> commands {ClickTexts(), WaitTask("CreatureSavingPeople")};
		// [camera] the scroll, 70 south of it and 45 up, as TapScrollAt places it
		commands.push_back(TapScrollAt(k_SaviourScroll).front());
		commands.push_back(WaitText(1496, true));
		// [camera] over the first man to drown, 50 south of him and 35 up
		commands.push_back(FlockCameraOn(k_DrowningMen.front(), 0.5f));
		for (size_t i = 0; i < 8; ++i)
		{
			commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = 5.0f, .thing = DrowningMan(k_DrowningMen.front())});
		}
		// [camera] over the shore where the men are put down
		commands.push_back(FlockCameraOn(k_SaviourShore, 0.0f));
		for (size_t i = 0; i < 6; ++i)
		{
			commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = 5.0f, .thing = k_SavedMan});
		}
		commands.push_back(WaitGlobal("CreatureSavingPeopleIsFinished", 1.0f));
		commands.push_back(LogGlobals("FoundMe CreatureSavingPeopleIsFinished CreatureSavingPeopleSavedCounter "
		                              "CreatureSavingPeopleDrownedCounter",
		                              "CreatureSavingPeople", 30.0f));
		all.push_back({
		    .id = "story.s4_saviour_some_saved_real",
		    .name = "Land 1: The Saviour, a man brought out by the leashed creature and thrown to his death by hand",
		    .facet = Facet::Story,
		    .description = "With the player's mouse and keys from a recording: the timeline only places the camera, over "
		                   "the scroll, over the first man to drown and then over the shore; the scroll's press, the "
		                   "leash key, the taps that send the creature to the man and to the shore, and the take and the "
		                   "throw of the man brought out are the recording's.",
		    .expected = "The creature sent to him carries him out; once he is out of the water, taken and thrown hard he "
		                "dies, and the evil advisor says his line for a saved man killed (text 1504).",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		// the active scroll tapped again: the good advisor says its reminder, before the first sheep is brought
		std::vector<Command> sheep = TapScrollAt(k_FlockHouse);
		sheep.push_back(LogGlobals("LostFlockInitiated", "TheLostFlock StandardReminder", 8.0f));
		for (size_t i = 0; i < k_LostSheep.size(); ++i)
		{
			// the hand takes the sheep, then puts it down by the shepherd with the Action button, where it lands and joins
			// his flock
			sheep.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = LostSheep(k_LostSheep.at(i))});
			PutDownHeld(sheep, k_FlockDrop);
			if (i < 3)
			{
				// the shepherd's thanks for each sheep are spoken before the next is brought, so his count goes up one by
				// one
				sheep.push_back(LogGlobals("GotRewardedOnce", "TheLostFlock", 12.0f));
			}
			else if (i == 3)
			{
				// the fourth brings his reward: once its scene is over and the advisors have said it is the first reward,
				// the player clicks it, for its help line
				sheep.push_back(WaitGlobal("LostFlockCompletedNow", 1.0f));
				sheep.push_back(WaitText(5251, true));
				sheep.push_back(FlockCameraOn(k_FlockReward, 1.0f));
				sheep.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_FlockRewardChest});
				sheep.push_back({.kind = Kind::PointerPress,
				                 .delaySeconds = 0.5f,
				                 .waitUntilFree = true,
				                 .value = k_ActionButton,
				                 .thing = k_FlockRewardChest});
				sheep.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
				sheep.push_back(LogGlobals("LostFlockCompletedNow", "TheLostFlock FirstReward", 6.0f));
			}
			else if (i < 8)
			{
				// each later one brought once his line for the one before has been said, so that no count is skipped
				sheep.push_back(WaitText(k_FlockThanks.at(i - 4), true));
			}
		}
		sheep.push_back(WaitGlobal("TheLostFlockFinished", 1.0f));
		sheep.push_back(
		    LogGlobals("LostFlockWanted LostFlockInitiated TheLostFlockFinished LostFlockCompletedNow", "TheLostFlock", 2.0f));
		Append(sheep, SwapToTheSheep());
		all.push_back({
		    .id = "story.s5_lost_flock",
		    .name = "Land 1: The Lost Flock",
		    .facet = Facet::Story,
		    .description = "Waits for the flock to be wanted, taps the shepherd's house, waits for the advisors' last "
		                   "text and the quest's start, taps the scroll again for its reminder, then takes each of the "
		                   "nine lost sheep in the hand and puts it down by the shepherd, clicking his reward once it is "
		                   "given, and bringing each sheep after it once his line for the one before is said. Then takes "
		                   "up the sheep creature's offer: its scroll tapped with the creature far off, again with the "
		                   "creature put by it, the creature clicked to call it off, and the sheep clicked to swap.",
		    .expected = "The shepherd's talk plays (texts 1445 to 1450, LostFlockInitiated=1) and the reminder 2494; each "
		                "sheep put down lands and joins his flock: three thanks (1462), his reward at six in it (1460, "
		                "1461, LostFlockCompletedNow=1), the reward's help line once clicked, 1456 to 1459 for the next "
		                "four and the sheep creature at eleven (TheLostFlockFinished=1). The swap: 5104 with the creature "
		                "far off, 2420 once it has walked up, the scroll back after it is clicked, and the swap with 5105. "
		                "The hand's take is the testbed's; the put-down and the clicks are the mouse's.",
		    .commands = LostFlockStarted(sheep),
		    .onLand = true,
		});
	}
	{
		// the shepherd's one sheep taken from his flock by the player's mouse and put back
		std::vector<Command> dead;
		TakeByMouse(dead, k_FieldSheep);
		dead.push_back(LogGlobals("LostFlockInitiated", "TheLostFlock", 3.0f));
		PutDownHeld(dead, k_FlockDrop);
		dead.push_back(LogGlobals("LostFlockInitiated", "TheLostFlock", 8.0f));
		// four lost sheep brought by the mouse, one at a time: three thanks, then his reward
		for (size_t i = 0; i < 4; ++i)
		{
			CarrySheepByMouse(dead, k_LostSheep.at(i));
			dead.push_back(LogGlobals("TheLostFlockFinished", "TheLostFlock", 12.0f));
		}
		dead.push_back(WaitGlobal("LostFlockCompletedNow", 1.0f));
		// a fifth: his first line after the reward
		CarrySheepByMouse(dead, k_LostSheep.at(4));
		dead.push_back(WaitText(k_FlockThanks.at(0), true));
		// [shortcut] the sixth killed where it stands by a script's SET_PROPERTY of its health, in place of the player's
		// blow or miracle; once it has died the mouse carries it into the field: the first dead sheep, never one of his
		// flock
		const ThingNear sixth {.type = script::ObjectType::Animal,
		                       .subtype = k_SheepSubtype,
		                       .point = k_LostSheep.at(5),
		                       .radius = 15.0f,
		                       .aliveOnly = true};
		dead.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 1.0f, .amount = 0.0f, .thing = sixth});
		dead.push_back(LogGlobals("TheLostFlockFinished", "TheLostFlock", 5.0f));
		TakeByMouse(dead, LostSheep(k_LostSheep.at(5)));
		PutDownHeld(dead, k_FlockDeadDrop);
		dead.push_back(WaitText(1463, true));
		// one of his flock killed: the second dead sheep, which leaves his flock one smaller
		dead.push_back(KillFieldSheep(2.0f));
		dead.push_back(WaitText(1466, true));
		// the seventh brings the flock back to its biggest since the reward, which earns no line; the eighth takes it
		// past that, which earns the next
		CarrySheepByMouse(dead, k_LostSheep.at(6));
		dead.push_back(LogGlobals("TheLostFlockFinished", "TheLostFlock", 15.0f));
		CarrySheepByMouse(dead, k_LostSheep.at(7));
		dead.push_back(WaitText(k_FlockThanks.at(1), true));
		// three more of his flock killed, a line for each: the third and fourth dead, then the fifth, which ends the quest
		dead.push_back(KillFieldSheep(2.0f));
		dead.push_back(KillFieldSheep(10.0f));
		dead.push_back(KillFieldSheep(10.0f));
		dead.push_back(WaitGlobal("TheLostFlockFinished", 3.0f));
		dead.push_back(LogGlobals("LostFlockInitiated TheLostFlockFinished LostFlockCompletedNow", "TheLostFlock", 2.0f));
		all.push_back({
		    .id = "story.s5_lost_flock_dead_sheep",
		    .name = "Land 1: The Lost Flock, five sheep dead",
		    .facet = Facet::Story,
		    .description = "The Lost Flock's start as story.s5_lost_flock, then, all through the mouse: the shepherd's "
		                   "own sheep taken from his flock and put back; five lost sheep brought; the sixth killed where "
		                   "it stands and carried dead into the field; one of the flock killed; two more brought; then "
		                   "three more of the flock killed.",
		    .expected = "With his flock only him, the script looks for a sheep within 50. Three thanks (1462), his "
		                "reward at six, 1456 for the fifth; the carried dead sheep is found and counted (1463), the "
		                "killed one too (1466); the seventh sheep earns no line and the eighth 1457; the third and "
		                "fourth dead 1466, the fifth 1464 and 1465, closing the quest (TheLostFlockFinished=3). The "
		                "deaths are the scripts' SET_PROPERTY; the takes, put-downs and clicks are the mouse's.",
		    .commands = LostFlockStarted(dead),
		    .onLand = true,
		});
	}
	{
		// [shortcut] the shepherd killed in his field by a script's SET_PROPERTY of his health, in place of the player's
		// blow or miracle, once he has walked there
		const std::vector<Command> killed {
		    LogGlobals("LostFlockInitiated", "TheLostFlock", 15.0f),
		    {.kind = Kind::ScriptSetHealth,
		     .delaySeconds = 1.0f,
		     .amount = 0.0f,
		     .thing = ThingNear {.type = script::ObjectType::Villager, .point = k_FlockField, .radius = 20.0f}},
		    WaitGlobal("TheLostFlockFinished", 2.0f),
		    LogGlobals("LostFlockInitiated TheLostFlockFinished LostFlockCompletedNow",
		               "TheLostFlock TheLostFlockKilledShepherd", 2.0f),
		};
		all.push_back({
		    .id = "story.s5_lost_flock_shepherd_killed",
		    .name = "Land 1: The Lost Flock, the shepherd killed",
		    .facet = Facet::Story,
		    .description = "The Lost Flock's start as story.s5_lost_flock, then the shepherd killed in his field.",
		    .expected = "The advisors' lines over his death (1453 to 1455), the record closed, and the quest over "
		                "(TheLostFlockFinished=2). The death is the scripts' SET_PROPERTY.",
		    .commands = LostFlockStarted(killed),
		    .onLand = true,
		});
	}
	all.push_back({
	    .id = "realinput.the_lost_flock",
	    .name = "Land 1: The Lost Flock, the sheep brought and killed by hand",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, with the player's mouse from a recording: the "
	                   "timeline only clicks the texts and places the camera: over the shepherd's house, behind or beside "
	                   "each lost sheep and over the spot by his house, behind sheep of his flock through their throws "
	                   "and over the field; the scroll's press, each take with the hand, each put-down and each throw "
	                   "are the recording's.",
	    .expected = "Four lost sheep put down by the shepherd join his flock: three thanks (1462) and his reward at "
	                "six; two more brought, 1456 for the flock at seven. A sheep of his flock thrown against the field's "
	                "north side dies there and is found and counted (1463); the next take, in front of a sheep, is the "
	                "shepherd's, and his death ends the quest (TheLostFlockFinished=2). Takes behind rocks, among the "
	                "pigs and of thrown sheep at rest miss (tools/checks/realinput/the_lost_flock.txt).",
	    .commands = LostFlockRealInput(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.throwing_stones",
	    .name = "Land 1: Throwing Stones, stones taken and thrown by hand",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, with the player's mouse from a recording: the "
	                   "timeline only clicks the texts and places the camera: over the scroll, then in rounds above a "
	                   "stone of the pile, behind the pile looking down to the pillar and above the reward's spot; the "
	                   "scroll's press, each take with the hand, each throw and each press on the reward are the "
	                   "recording's.",
	    .expected = "Stones thrown from the hill at the boulder on the pillar, watched by the fisherman, who cheers or "
	                "despairs at each; the boulder knocked off ends the quest well (End=1) and drops the reward's chest, "
	                "and the practice rounds put the boulder back (tools/checks/realinput/throwing_stones.txt).",
	    .commands = ThrowingStonesRealInput(),
	    .onLand = true,
	});
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
