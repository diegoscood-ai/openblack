/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <vector>

#include <SavedRecords.h>
#include <gtest/gtest.h>

// The game object's tail and the records it meets, on synthetic values: each one's length and the places of its values
// are the game's, and each reads back as it was written (docs/wiki/engine/saved-games.md#the-end-of-the-game-object).

using namespace openblack::gamesave;

namespace
{
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0x27000};

template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

std::vector<uint8_t> RoundTrip(const SavedRecord& record)
{
	StreamWriter writer(k_After100);
	Write(writer, record, nullptr);
	const auto bytes = writer.Take();
	StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	SavedRecord read;
	EXPECT_TRUE(ReadRecord(reader, type, extra, read, nullptr));
	EXPECT_EQ(reader.Position(), bytes.size());
	EXPECT_EQ(TypeOf(read), TypeOf(record));
	StreamWriter again(k_After100);
	Write(again, read, nullptr);
	EXPECT_EQ(again.Bytes(), bytes);
	return bytes;
}
} // namespace

TEST(SaveGameTail, TheTailHasTheGamesLength)
{
	GameTail tail;
	tail.helpEntries.values.resize(1024);
	tail.scriptObjects[510].byte = 7;
	tail.exclusions.resize(1);
	tail.exclusions[0].values.assign(40, 0);
	tail.exclusionNameSize = 40;
	tail.creatureCurse[0] = 0x42;
	StreamWriter writer(k_After100);
	WritePart(writer, tail, nullptr);
	const auto bytes = writer.Take();
	ASSERT_EQ(bytes.size(), 5642u + 1024 * 16 + 4 + 40);
	EXPECT_EQ(bytes[166 + 1024 * 16 + 510 * 9 + 8], 7);
	EXPECT_EQ(bytes[bytes.size() - 12], 0x42);
	StreamReader reader(bytes, k_After100);
	GameTail read;
	ASSERT_TRUE(ReadPart(reader, read, nullptr));
	EXPECT_EQ(reader.Position(), bytes.size());
	EXPECT_EQ(read, tail);
}

TEST(SaveGameTail, ACameraWritesItsModesValues)
{
	ASSERT_EQ(RoundTrip(CameraRecord {}).size(), 408u);
	CameraRecord camera;
	camera.mode.mode = 0x46;
	camera.mode.object = {40};
	camera.mode.lastWord = 9;
	const auto bytes = RoundTrip(camera);
	ASSERT_EQ(bytes.size(), 408u + 24 + 24 + 4 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 404), 0x46u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 8), 40u);
	CameraRecord free;
	free.mode.mode = 0x65;
	EXPECT_EQ(RoundTrip(free).size(), 408u + 16);
}

TEST(SaveGameTail, TheRecordsTheTailMeetsHaveTheGamesLengths)
{
	EXPECT_EQ(RoundTrip(ScriptTimerRecord {}).size(), 17u + 14 + 8);
	EXPECT_EQ(RoundTrip(BaseOnlyRecord {}).size(), 17u + 14 + 36);
	EXPECT_EQ(RoundTrip(ThingMusicInfoRecord {.object = {41}}).size(), 17u + 4 + 4 + 16 + 12);
	EXPECT_EQ(RoundTrip(WillowWispCollectionRecord {}).size(), 17u + 8 + 12 + 8 + 1);
	EXPECT_EQ(RoundTrip(FadeOutAtomDataRecord {}).size(), 17u + 8 + 1 + 12);
}
