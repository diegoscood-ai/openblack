/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/ScriptTypes.h"
#include "Input/HandDemo.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;
using namespace openblack::hand_demo;

namespace
{
/// Records every 10 ms from a time, the triggers at the given ones
std::vector<Record> Records(size_t count, uint32_t from, const std::vector<size_t>& triggers = {})
{
	std::vector<Record> records(count);
	for (size_t i = 0; i < count; ++i)
	{
		records[i].timeMs = from + static_cast<uint32_t>(i * 10);
		records[i].trigger = std::ranges::find(triggers, i) != triggers.end() ? 1 : 0;
	}
	return records;
}

/// A playback of fake records that notes each record it plays
struct Playback
{
	explicit Playback(const std::vector<Record>& recording, uint32_t nowMs)
	    : records(recording)
	{
		Step(nowMs, true, false);
	}

	StepResult Step(uint32_t nowMs, bool first, bool waitTrigger)
	{
		return StepRecords(records, head, nowMs, first, waitTrigger, trigger,
		                   [this](const Record& record) { played.push_back(&record); });
	}
	StepResult Advance(uint32_t nowMs, bool waitTrigger) { return Step(nowMs, false, waitTrigger); }

	const std::vector<Record>& records;
	Playhead head;
	bool trigger {false};
	std::vector<const Record*> played;
};
} // namespace

TEST(HandDemo, TheCursorIsDownThePictureBetweenTheBars)
{
	const glm::vec2 cursor {0.5f, 0.25f};
	// No bars: the whole screen
	EXPECT_EQ(CursorPixel(cursor, {1280, 960}, 0.0f), glm::ivec2(640, 240));
	// Bars all the way in: 1280 by 720 between bars of 120
	EXPECT_EQ(CursorPixel(cursor, {1280, 960}, 1.0f), glm::ivec2(640, 300));
	const glm::vec2 corner {0.0f, 1.0f};
	EXPECT_EQ(CursorPixel(corner, {1280, 960}, 1.0f), glm::ivec2(0, 840));
}

TEST(HandDemo, AtTenTwentyFourBySevenSixtyEightTheBarsAreNinetySix)
{
	// The bars snapped in by a demo (fraction 1): (768 - 576) / 2 = 96 each, the picture 1024 by 576
	EXPECT_EQ(CursorPixel({0.5f, 0.25f}, {1024, 768}, 1.0f), glm::ivec2(512, 240));
	EXPECT_EQ(CursorPixel({0.0f, 0.0f}, {1024, 768}, 1.0f), glm::ivec2(0, 96));
	EXPECT_EQ(CursorPixel({1.0f, 1.0f}, {1024, 768}, 1.0f), glm::ivec2(1024, 672));
	// Halfway in: bars of 48
	EXPECT_EQ(CursorPixel({0.0f, 0.0f}, {1024, 768}, 0.5f), glm::ivec2(0, 48));
}

TEST(HandDemo, OnAScreenWiderThanTheBarsTheyAreNotClamped)
{
	// 1920 by 1000: (1000 - 1080) / 2 = -40, so the picture's top is above the screen's
	EXPECT_EQ(CursorPixel({0.0f, 0.0f}, {1920, 1000}, 1.0f), glm::ivec2(0, -39));
	EXPECT_EQ(CursorPixel({0.0f, 1.0f}, {1920, 1000}, 1.0f), glm::ivec2(0, 1040));
}

TEST(HandDemo, TheFirstRecordPlaysAtOnceAndTheRestByTheirTimes)
{
	const auto records = Records(5, 5000);
	Playback playback(records, 100);
	ASSERT_EQ(playback.played.size(), 1u);
	EXPECT_EQ(playback.played[0], &records[0]);
	EXPECT_EQ(playback.head.firstTimeMs, 5000u);
	EXPECT_EQ(playback.head.startMs, 100u);
	playback.played.clear();
	// 25 ms later: the records of 10 and 20
	EXPECT_FALSE(playback.Advance(125, false).ended);
	ASSERT_EQ(playback.played.size(), 2u);
	EXPECT_EQ(playback.played[1], &records[2]);
	playback.played.clear();
	// A slow frame catches up, and the last record ends it
	EXPECT_TRUE(playback.Advance(500, false).ended);
	EXPECT_EQ(playback.played.size(), 2u);
}

TEST(HandDemo, AMarkHoldsTheRestUntilTheScriptReadsIt)
{
	const auto records = Records(5, 0, {1});
	Playback playback(records, 0);
	playback.played.clear();
	playback.Advance(10, true);
	EXPECT_TRUE(playback.trigger);
	ASSERT_EQ(playback.played.size(), 1u);
	playback.played.clear();
	// Held while the trigger is pending, however long
	playback.Advance(1000, true);
	EXPECT_TRUE(playback.played.empty());
	// Read: the rest keeps its pace from there
	playback.trigger = false;
	playback.Advance(1000, true);
	ASSERT_EQ(playback.played.size(), 1u);
	EXPECT_EQ(playback.played[0], &records[2]);
	playback.played.clear();
	playback.Advance(1009, true);
	EXPECT_TRUE(playback.played.empty());
	playback.Advance(1010, true);
	EXPECT_EQ(playback.played.size(), 1u);
}

TEST(HandDemo, WithoutPauseAMarkIsOnlyNoted)
{
	const auto records = Records(4, 0, {1});
	Playback playback(records, 0);
	playback.played.clear();
	playback.Advance(30, false);
	EXPECT_TRUE(playback.trigger);
	EXPECT_EQ(playback.played.size(), 3u);
}

TEST(HandDemo, AClockGoneBackPlaysEverythingAtOnce)
{
	const auto records = Records(5, 5000);
	Playback playback(records, 1000);
	playback.played.clear();
	// now - start wraps round, unsigned, so every record is due
	const auto result = playback.Advance(900, false);
	EXPECT_EQ(playback.played.size(), 4u);
	EXPECT_TRUE(result.ended);
}

namespace
{
/// A record's bytes as the game writes them
std::array<uint8_t, k_RecordSize> RecordBytes(uint32_t message, float mouseX, float mouseY, uint32_t trigger, uint32_t time)
{
	std::array<uint8_t, k_RecordSize> bytes {};
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(0x00, message);
	put(0x04, 1.5f);
	put(0x34, mouseX);
	put(0x38, mouseY);
	put(0x3C, 1867.5f);
	put(0x40, 54.5f);
	put(0x44, 2539.0f);
	put(0x48, 1914.0f);
	put(0x4C, 29.5f);
	put(0x50, 2509.0f);
	put(0x54, 0x202u);
	put(0x58, 1.25f);
	put(0x5C, trigger);
	put(0x60, time);
	put(0x64, 3.5f);
	put(0x68, int32_t {0x10000});
	put(0x6C, int32_t {-0x20000});
	put(0x70, 7.0f);
	put(0x74, 16u);
	put(0x78, 5u);
	return bytes;
}
} // namespace

TEST(HandDemoRecordFields, ReadsEveryField)
{
	const auto first = RecordBytes(0, 0.25f, 0.75f, 0, 1000);
	const auto second = RecordBytes(3, 0.5f, 0.5f, 1, 1030);
	std::vector<uint8_t> bytes(first.begin(), first.end());
	bytes.insert(bytes.end(), second.begin(), second.end());
	const auto records = ParseRecords(bytes);
	ASSERT_EQ(records.size(), 2u);
	const auto& move = records[0];
	EXPECT_EQ(move.message, 0u);
	EXPECT_FLOAT_EQ(move.throwBlock[0], 1.5f);
	EXPECT_FLOAT_EQ(move.mouse.x, 0.25f);
	EXPECT_FLOAT_EQ(move.mouse.y, 0.75f);
	EXPECT_FLOAT_EQ(move.eye.y, 54.5f);
	EXPECT_FLOAT_EQ(move.focus.z, 2509.0f);
	EXPECT_EQ(move.hints, 0x202u);
	EXPECT_FLOAT_EQ(move.hintValue, 1.25f);
	EXPECT_EQ(move.trigger, 0u);
	EXPECT_EQ(move.timeMs, 1000u);
	const auto& press = records[1];
	EXPECT_EQ(press.message, 3u);
	EXPECT_EQ(press.trigger, 1u);
	EXPECT_FLOAT_EQ(press.objectDistance, 3.5f);
	EXPECT_EQ(press.objectPlace.x, 0x10000);
	EXPECT_EQ(press.objectPlace.y, -0x20000);
	EXPECT_FLOAT_EQ(press.objectAltitude, 7.0f);
	EXPECT_EQ(press.objectType, 16u);
	EXPECT_EQ(press.objectSubtype, 5u);
}

TEST(HandDemoHints, NoneNearEitherEnd)
{
	auto records = Records(60, 0);
	for (auto& record : records)
	{
		record.hints = 7;
	}
	Playhead head;
	bool trigger = false;
	size_t played = 0;
	const auto play = [&played](const Record&) { ++played; };
	EXPECT_EQ(StepRecords(records, head, 0, true, false, trigger, play).hints, 0u);
	// One step up to the 30th record: counted at the step's first read, the second, all are near the start
	played = 0;
	EXPECT_EQ(StepRecords(records, head, 290, false, false, trigger, play).hints, 0u);
	EXPECT_EQ(played, 29u);
	played = 0;
	EXPECT_EQ(StepRecords(records, head, 300, false, false, trigger, play).hints, 7u);
	EXPECT_EQ(played, 1u);
	EXPECT_EQ(StepRecords(records, head, 400, false, false, trigger, play).hints, 7u);
	// The last twenty show none
	EXPECT_EQ(StepRecords(records, head, 410, false, false, trigger, play).hints, 0u);
	// A step that plays nothing leaves the hints as they were
	EXPECT_EQ(StepRecords(records, head, 410, false, false, trigger, play).hints, std::nullopt);
}

TEST(HandDemoHints, AsTheHandsCameraHints)
{
	EXPECT_EQ(HintsAsTricons(0), 0u);
	EXPECT_EQ(HintsAsTricons(1), 0x02u);
	EXPECT_EQ(HintsAsTricons(2), 0x01u);
	EXPECT_EQ(HintsAsTricons(4), 0x08u);
	EXPECT_EQ(HintsAsTricons(0x202), 0x01u);
}

// A button's record gives the hand back the throw it was ready to make when recorded; a mouse move gives it nothing
TEST(HandDemoThrow, AButtonsRecordCarriesTheThrowTheHandWasReadyToMake)
{
	const std::array<float, 12> block {73.9941f, 5.2013f, -52.1485f, 0.0f, 0.0f, 0.0f,
	                                   1966.6f,  17.5f,   2364.8f,   0.3f, 1.1f, -0.1f};
	std::vector<std::array<float, 12>> given;
	const auto setThrowBlock = [&given](const std::array<float, 12>& thrown) { given.push_back(thrown); };

	Frame frame;
	Record move {.message = 0, .throwBlock = block, .mouse = {0.25f, 0.75f}};
	ApplyRecord(frame, move, setThrowBlock);
	EXPECT_TRUE(given.empty());
	EXPECT_FLOAT_EQ(frame.mouse.x, 0.25f);
	EXPECT_FLOAT_EQ(frame.mouse.y, 0.75f);

	// A release: the block is given whole, the mouse stays where the last move left it
	Record release {.message = 4, .throwBlock = block, .mouse = {0.9f, 0.9f}};
	ApplyRecord(frame, release, setThrowBlock);
	ASSERT_EQ(given.size(), 1u);
	EXPECT_EQ(given.front(), block);
	EXPECT_FLOAT_EQ(frame.mouse.x, 0.25f);
	EXPECT_FALSE(frame.action);

	// A press too, and it holds the action button
	Record press {.message = 3, .throwBlock = block};
	ApplyRecord(frame, press, setThrowBlock);
	EXPECT_EQ(given.size(), 2u);
	EXPECT_TRUE(frame.action);
}

// Every recorded mouse move is kept in order among the frame's moves, each a sample of the gestures; a button's record
// adds none
TEST(HandDemoGestures, EveryRecordedMouseMoveIsKeptInOrderAsAGestureSample)
{
	const auto setThrowBlock = [](const std::array<float, 12>&) {};
	Frame frame;
	ApplyRecord(frame, Record {.message = 0, .mouse = {0.25f, 0.75f}}, setThrowBlock);
	ApplyRecord(frame, Record {.message = 3, .mouse = {0.9f, 0.9f}}, setThrowBlock);
	ApplyRecord(frame, Record {.message = 0, .mouse = {0.3f, 0.7f}}, setThrowBlock);
	ApplyRecord(frame, Record {.message = 0, .mouse = {0.3f, 0.7f}}, setThrowBlock);
	ASSERT_EQ(frame.moves.size(), 3u);
	EXPECT_FLOAT_EQ(frame.moves[0].x, 0.25f);
	EXPECT_FLOAT_EQ(frame.moves[1].y, 0.7f);
	// a move to where the mouse already is still counts, as the recording sent it
	EXPECT_EQ(frame.moves[1], frame.moves[2]);
	EXPECT_FLOAT_EQ(frame.mouse.x, 0.3f);
}

namespace
{
/// A recorded press on the storage pit, as the wood lesson's recording has one: type 16 (a store), subtype 5, a little
/// over 1830 by 2642 metres
Record PressOnStore()
{
	return {.message = 3,
	        .objectDistance = 40.0f,
	        .objectPlace = {map_coords::ToFixed(1830.0f), map_coords::ToFixed(2642.0f)},
	        .objectAltitude = 0.0f,
	        .objectType = static_cast<uint32_t>(script::ObjectType::Store),
	        .objectSubtype = 5};
}

map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres({x, z});
}

ThingUnderHand Store(uint32_t subtype, map_coords::MapCoords at)
{
	return {.type = script::ObjectType::Store, .subtype = subtype, .at = at};
}
} // namespace

TEST(HandDemoClick, OnlyTheActionButtonAndTheUnusedMessageRetarget)
{
	EXPECT_TRUE(ClickCanRetarget(3));
	EXPECT_TRUE(ClickCanRetarget(4));
	EXPECT_TRUE(ClickCanRetarget(6));
	EXPECT_FALSE(ClickCanRetarget(0));
	EXPECT_FALSE(ClickCanRetarget(1));
	EXPECT_FALSE(ClickCanRetarget(2));
	EXPECT_FALSE(ClickCanRetarget(5));
}

TEST(HandDemoClick, AClickRecordedOnNothingLandsOnNothing)
{
	const Record record {.message = 3, .objectType = 0, .objectSubtype = ecs::script_type::k_NoSubtype};
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1830.0f, 2642.0f))), ClickTarget::NoThing);
	EXPECT_EQ(TargetOfClick(record, std::nullopt), ClickTarget::NoThing);
}

TEST(HandDemoClick, AClickStaysOnTheRecordedKindUnderTheHandNearTheRecordedPlace)
{
	const auto record = PressOnStore();
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1832.0f, 2642.0f))), ClickTarget::UnderHand);
}

// While the press grabs a thing the click is compared with it, not with the frame's pick, which never finds a thing
// drawn in the hand; with no grab, with the pick
TEST(HandDemoClick, AClickDuringAGrabIsComparedWithTheGrabbedThing)
{
	EXPECT_EQ(ClickComparedThing<int>(std::nullopt, 7), std::optional(7));
	EXPECT_EQ(ClickComparedThing<int>(3, 7), std::optional(7));
	EXPECT_EQ(ClickComparedThing<int>(3, std::nullopt), std::optional(3));
	EXPECT_EQ(ClickComparedThing<int>(std::nullopt, std::nullopt), std::nullopt);
}

TEST(HandDemoClick, AClickLooksForTheRecordedKindWhenTheHandIsOverSomethingElse)
{
	const auto record = PressOnStore();
	EXPECT_EQ(TargetOfClick(record, std::nullopt), ClickTarget::NearestOfKind);
	// Another subtype, another type, or the right kind 3 metres or more away
	EXPECT_EQ(TargetOfClick(record, Store(2, At(1830.0f, 2642.0f))), ClickTarget::NearestOfKind);
	EXPECT_EQ(
	    TargetOfClick(record, ThingUnderHand {.type = script::ObjectType::Rock, .subtype = 5, .at = At(1830.0f, 2642.0f)}),
	    ClickTarget::NearestOfKind);
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1833.1f, 2642.0f))), ClickTarget::NearestOfKind);
	// The thing under the hand is compared exactly: a recorded "any" subtype is no match for it
	auto any = record;
	any.objectSubtype = ecs::script_type::k_AnySubtype;
	EXPECT_EQ(TargetOfClick(any, Store(5, At(1830.0f, 2642.0f))), ClickTarget::NearestOfKind);
}

TEST(HandDemoClick, AThingWithNoSubtypeUnderTheHandMatchesARecordedNone)
{
	auto record = PressOnStore();
	record.objectSubtype = ecs::script_type::k_NoSubtype;
	EXPECT_EQ(TargetOfClick(record, Store(ecs::script_type::k_NoSubtype, At(1830.0f, 2642.0f))), ClickTarget::UnderHand);
}

TEST(HandDemoClick, TheClickMovesToAThingOfTheKindWithinThreeMetres)
{
	const auto record = PressOnStore();
	EXPECT_TRUE(TakesRecordedClick(record, Store(5, At(1832.0f, 2642.0f))));
	EXPECT_TRUE(TakesRecordedClick(record, Store(5, At(1832.9f, 2642.0f))));
	EXPECT_FALSE(TakesRecordedClick(record, Store(5, At(1833.5f, 2642.0f))));
	EXPECT_FALSE(TakesRecordedClick(record, Store(2, At(1830.0f, 2642.0f))));
	// A thing with no subtype is never one the search asks for, unless any will do
	EXPECT_FALSE(TakesRecordedClick(record, Store(ecs::script_type::k_NoSubtype, At(1830.0f, 2642.0f))));
	auto any = record;
	any.objectSubtype = ecs::script_type::k_AnySubtype;
	EXPECT_TRUE(TakesRecordedClick(any, Store(2, At(1830.0f, 2642.0f))));
	EXPECT_TRUE(TakesRecordedClick(any, Store(ecs::script_type::k_NoSubtype, At(1830.0f, 2642.0f))));
}

// The scripts' type and subtype filter on values: the type, then the subtype or any; never a thing with no subtype
// unless any will do
TEST(ScriptKind, MatchesTheTypeAndTheSubtypeOrAny)
{
	using script::ObjectType;
	using namespace ecs::script_type;
	EXPECT_TRUE(KindMatches(ObjectType::Store, 5, ObjectType::Store, 5));
	EXPECT_TRUE(KindMatches(ObjectType::Store, 5, ObjectType::Store, k_AnySubtype));
	EXPECT_FALSE(KindMatches(ObjectType::Store, 5, ObjectType::Store, 2));
	EXPECT_FALSE(KindMatches(ObjectType::Store, 5, ObjectType::Rock, 5));
	EXPECT_FALSE(KindMatches(ObjectType::Store, k_NoSubtype, ObjectType::Store, k_NoSubtype));
	EXPECT_TRUE(KindMatches(ObjectType::Store, k_NoSubtype, ObjectType::Store, k_AnySubtype));
}

// The keep test compares the subtype exactly: only the very subtype keeps the click, a recorded "any" never does
TEST(HandDemoClick, TheThingUnderTheHandKeepsTheClickOnlyForItsExactSubtype)
{
	auto record = PressOnStore();
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1830.0f, 2642.0f))), ClickTarget::UnderHand);
	record.objectSubtype = 6;
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1830.0f, 2642.0f))), ClickTarget::NearestOfKind);
	record.objectSubtype = ecs::script_type::k_AnySubtype;
	EXPECT_EQ(TargetOfClick(record, Store(5, At(1830.0f, 2642.0f))), ClickTarget::NearestOfKind);
	EXPECT_EQ(TargetOfClick(record, Store(ecs::script_type::k_AnySubtype, At(1830.0f, 2642.0f))), ClickTarget::UnderHand);
}

// A search that finds nothing, or finds a thing that is not an object, leaves the click as it was
TEST(HandDemoClick, AFindThatIsNotAnObjectLeavesTheClickAsItWas)
{
	EXPECT_EQ(OutcomeOfFind(false, false, false), FoundClick::Unchanged);
	EXPECT_EQ(OutcomeOfFind(true, false, false), FoundClick::Unchanged);
	EXPECT_EQ(OutcomeOfFind(true, false, true), FoundClick::Unchanged);
}

// An object found that the hand can act on takes the click; one it cannot act on (a building with nothing built yet,
// a thing going away) makes the click land on no thing
TEST(HandDemoClick, AnObjectFoundTakesTheClickOnlyWhenTheHandCanActOnIt)
{
	EXPECT_EQ(OutcomeOfFind(true, true, true), FoundClick::Thing);
	EXPECT_EQ(OutcomeOfFind(true, true, false), FoundClick::NoThing);
}

// Every object found clears the click's point before the click is made again from it, the hand able to act on it or
// not; a find that changes nothing keeps it
TEST(HandDemoClick, AnObjectFoundClearsTheClicksPoint)
{
	EXPECT_TRUE(ClearsClickPoint(FoundClick::Thing));
	EXPECT_TRUE(ClearsClickPoint(FoundClick::NoThing));
	EXPECT_FALSE(ClearsClickPoint(FoundClick::Unchanged));
}

namespace
{
/// The hand's action press pass after pass, as HandSystem::Update (each frame) and HandSystem::ProcessTurn (each turn)
/// take it: a new press, or a demo's pending press taken again while the button is held; each press then takes
/// something or nothing
struct PressFrames
{
	/// One frame; true when the press is taken in it. While paused the idle hand takes no press
	bool Frame(bool demoPlaying, bool actionHeld, bool pressTakesNothing, bool paused = false)
	{
		const bool pressed = !paused && actionHeld && (!wasHeld || PressTakenAgain(demoPlaying, pending, actionHeld));
		pending = PressPendingAfterPass(demoPlaying, pending, !paused, actionHeld, pressed, pressed && pressTakesNothing);
		wasHeld = actionHeld;
		presses += pressed ? 1 : 0;
		return pressed;
	}

	/// The turn's interface pass: no new press, only a demo's pending press taken again, with the button as the last
	/// frame left it; true when the press is taken in it
	bool Turn(bool demoPlaying, bool pressTakesNothing, bool paused = false)
	{
		const bool pressed = !paused && PressTakenAgain(demoPlaying, pending, wasHeld);
		if (pressed)
		{
			pending = PressStaysPending(demoPlaying, true, pressTakesNothing);
			++presses;
		}
		return pressed;
	}

	bool wasHeld {false};
	bool pending {false};
	int presses {0};
};
} // namespace

TEST(HandDemoPress, APressOnTheLandIsTakenAgainWhileHeld)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	// the button stays down: the press that took nothing is taken again every frame
	EXPECT_TRUE(frames.Frame(true, true, true));
	EXPECT_TRUE(frames.Frame(true, true, true));
	EXPECT_EQ(frames.presses, 3);
}

TEST(HandDemoPress, ATapOrACreatureClickEndsThePendingPress)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	// the press taken again finds a thing to tap (or sends the creature to the point): it takes something
	EXPECT_TRUE(frames.Frame(true, true, false));
	EXPECT_FALSE(frames.Frame(true, true, true));
	EXPECT_FALSE(frames.Frame(true, true, true));
	EXPECT_EQ(frames.presses, 2);
}

TEST(HandDemoPress, OutsideADemoAPressIsTakenOnce)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(false, true, true));
	EXPECT_FALSE(frames.Frame(false, true, true));
	EXPECT_FALSE(frames.Frame(false, true, true));
	EXPECT_FALSE(frames.pending);
	EXPECT_EQ(frames.presses, 1);
	// a press that takes something is taken once too
	PressFrames taking;
	EXPECT_TRUE(taking.Frame(false, true, false));
	EXPECT_FALSE(taking.Frame(false, true, false));
	EXPECT_EQ(taking.presses, 1);
}

TEST(HandDemoPress, TheReleaseEndsThePendingPress)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	EXPECT_FALSE(frames.Frame(true, false, true));
	EXPECT_FALSE(frames.pending);
	// the next press down is a new one, taken once it is pressed
	EXPECT_TRUE(frames.Frame(true, true, false));
	EXPECT_FALSE(frames.Frame(true, true, false));
	EXPECT_EQ(frames.presses, 2);
}

TEST(HandDemoPress, APendingPressIsTakenAgainAtTheNextPassFrameOrTurn)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	// a turn comes before the next frame: the press is taken again there, and takes the thing under the hand
	EXPECT_TRUE(frames.Turn(true, false));
	EXPECT_FALSE(frames.Frame(true, true, true));
	EXPECT_EQ(frames.presses, 2);
	// the next frame comes first: taken again there, then at the turn after it while it still takes nothing
	PressFrames frameFirst;
	EXPECT_TRUE(frameFirst.Frame(true, true, true));
	EXPECT_TRUE(frameFirst.Frame(true, true, true));
	EXPECT_TRUE(frameFirst.Turn(true, true));
	EXPECT_TRUE(frameFirst.Frame(true, true, false));
	EXPECT_FALSE(frameFirst.Turn(true, true));
	EXPECT_EQ(frameFirst.presses, 4);
}

TEST(HandDemoPress, OutsideADemoATurnTakesNoPress)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(false, true, true));
	EXPECT_FALSE(frames.Turn(false, true));
	EXPECT_FALSE(frames.Frame(false, true, true));
	EXPECT_EQ(frames.presses, 1);
}

TEST(HandDemoPress, APauseKeepsThePendingPressForAfterIt)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	// paused: no press is taken, at a frame or a turn, and the pending press is kept
	EXPECT_FALSE(frames.Frame(true, true, true, true));
	EXPECT_FALSE(frames.Turn(true, true, true));
	EXPECT_TRUE(frames.pending);
	// after the pause it is taken again
	EXPECT_TRUE(frames.Frame(true, true, false));
	EXPECT_EQ(frames.presses, 2);
}

TEST(HandDemoPress, AReleaseDuringThePauseEndsThePendingPress)
{
	PressFrames frames;
	EXPECT_TRUE(frames.Frame(true, true, true));
	EXPECT_FALSE(frames.Frame(true, false, true, true));
	EXPECT_FALSE(frames.pending);
	EXPECT_FALSE(frames.Frame(true, false, true));
	EXPECT_EQ(frames.presses, 1);
}

static_assert(!PressPendingAfterPass(false, true, false, true, false, false));
static_assert(!PressTakenAgain(false, true, true));
static_assert(!PressTakenAgain(true, true, false));
static_assert(!PressStaysPending(true, false, true));
