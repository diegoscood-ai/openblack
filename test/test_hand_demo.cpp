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
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Input/HandDemo.h"

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
