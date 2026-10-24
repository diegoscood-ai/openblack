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

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Input/HandDemo.h"

using namespace openblack;

namespace
{
/// A record laid out as a .hnd file has it, its fields at their places in the 124 bytes
struct RecordBytes
{
	uint32_t message {0};
	std::array<float, 12> throwBlock {};
	std::array<float, 2> mouse {};
	std::array<float, 3> eye {};
	std::array<float, 3> focus {};
	uint32_t trigger {0};
	uint32_t timeMs {0};
};

template <typename T>
void Put(std::vector<uint8_t>& bytes, size_t offset, const T& value)
{
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Append(std::vector<uint8_t>& bytes, const RecordBytes& record)
{
	const auto start = bytes.size();
	// Bytes the parser does not read are left at a value it would show if it did
	bytes.resize(start + hand_demo::k_RecordSize, 0xAB);
	Put(bytes, start + 0x00, record.message);
	for (size_t i = 0; i < record.throwBlock.size(); ++i)
	{
		Put(bytes, start + 0x04 + i * 4, record.throwBlock.at(i));
	}
	Put(bytes, start + 0x34, record.mouse);
	Put(bytes, start + 0x3C, record.eye);
	Put(bytes, start + 0x48, record.focus);
	Put(bytes, start + 0x5C, record.trigger);
	Put(bytes, start + 0x60, record.timeMs);
}

RecordBytes Sample(uint32_t message, float seed)
{
	RecordBytes record;
	record.message = message;
	for (size_t i = 0; i < record.throwBlock.size(); ++i)
	{
		record.throwBlock.at(i) = seed + static_cast<float>(i);
	}
	record.mouse = {seed * 0.01f, 1.0f - (seed * 0.01f)};
	record.eye = {seed + 1000.0f, seed + 50.0f, seed + 2000.0f};
	record.focus = {seed + 1010.0f, seed + 5.0f, seed + 2020.0f};
	record.trigger = message == 0 ? 0 : 1;
	record.timeMs = 100000u + static_cast<uint32_t>(seed) * 16u;
	return record;
}

void ExpectSame(const hand_demo::Record& parsed, const RecordBytes& written)
{
	EXPECT_EQ(parsed.message, written.message);
	EXPECT_EQ(parsed.throwBlock, written.throwBlock);
	EXPECT_EQ(parsed.mouse.x, written.mouse.at(0));
	EXPECT_EQ(parsed.mouse.y, written.mouse.at(1));
	EXPECT_EQ(parsed.eye.x, written.eye.at(0));
	EXPECT_EQ(parsed.eye.y, written.eye.at(1));
	EXPECT_EQ(parsed.eye.z, written.eye.at(2));
	EXPECT_EQ(parsed.focus.x, written.focus.at(0));
	EXPECT_EQ(parsed.focus.y, written.focus.at(1));
	EXPECT_EQ(parsed.focus.z, written.focus.at(2));
	EXPECT_EQ(parsed.trigger, written.trigger);
	EXPECT_EQ(parsed.timeMs, written.timeMs);
}
} // namespace

TEST(HandDemoRecords, ARecordIs124Bytes)
{
	EXPECT_EQ(hand_demo::k_RecordSize, 124u);
}

TEST(HandDemoRecords, ReadsEveryFieldOfEachRecordInOrder)
{
	const std::array written {Sample(0, 1.0f), Sample(1, 2.0f), Sample(0, 3.0f), Sample(2, 4.0f), Sample(3, 5.0f)};
	std::vector<uint8_t> bytes;
	for (const auto& record : written)
	{
		Append(bytes, record);
	}
	const auto records = hand_demo::ParseRecords(bytes);
	ASSERT_EQ(records.size(), written.size());
	for (size_t i = 0; i < written.size(); ++i)
	{
		SCOPED_TRACE(i);
		ExpectSame(records.at(i), written.at(i));
	}
}

TEST(HandDemoRecords, LeavesOutAShortLastRecord)
{
	std::vector<uint8_t> bytes;
	Append(bytes, Sample(0, 1.0f));
	Append(bytes, Sample(4, 2.0f));
	bytes.resize(bytes.size() - 1);
	const auto records = hand_demo::ParseRecords(bytes);
	ASSERT_EQ(records.size(), 1u);
	ExpectSame(records.front(), Sample(0, 1.0f));
}

TEST(HandDemoRecords, NothingFromNoBytes)
{
	EXPECT_TRUE(hand_demo::ParseRecords({}).empty());
	const std::vector<uint8_t> tooShort(hand_demo::k_RecordSize - 1, 0);
	EXPECT_TRUE(hand_demo::ParseRecords(tooShort).empty());
}
