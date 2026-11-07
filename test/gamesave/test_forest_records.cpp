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

#include <optional>
#include <vector>

#include <SavedRecords.h>
#include <gtest/gtest.h>

// The records of the forests and the trees, on synthetic values: each record's length and the places of its values
// are the game's, and each reads back as it was written (docs/wiki/engine/saved-games.md#the-forests-and-the-trees).

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

TEST(SaveForestRecords, AForestWritesItsTwoListsOfTreesAndTwoWordsOfTheGame)
{
	ForestRecord forest;
	forest.container.info = 3;
	forest.isScenic = 1;
	forest.footpathLink = {20};
	forest.id = 7;
	forest.trees.numbers = {30, 31};
	forest.bigForest = {32};
	forest.secondGameWord = 0x55;
	const auto bytes = RoundTrip(forest);
	// The container part, a word, the link, the identity, the two lists, the big forest and two words
	ASSERT_EQ(bytes.size(), 71u + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 39), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 43), 20u);
	EXPECT_EQ(At<uint32_t>(bytes, 47), 7u);
	EXPECT_EQ(At<uint32_t>(bytes, 51), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 59), 31u);
	EXPECT_EQ(At<uint32_t>(bytes, 63), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 67), 32u);
	EXPECT_EQ(At<uint32_t>(bytes, 75), 0x55u);
}

TEST(SaveForestRecords, ABigForestIsABuildingOfSeveralCellsWithItsForest)
{
	BigForestRecord big;
	big.forest = {40};
	big.word = 9;
	const auto bytes = RoundTrip(big);
	ASSERT_EQ(bytes.size(), 17u + 67 + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 84), 40u);
	EXPECT_EQ(At<uint32_t>(bytes, 88), 9u);
}

TEST(SaveForestRecords, ATreeWritesTwoBytesTheFloatItsForestAndTwoBytesMore)
{
	TreeRecord tree;
	tree.base.object.info = 2104;
	tree.firstValue = -2;
	tree.secondValue = 0.5f;
	tree.forest = {41};
	tree.thirdValue = 0x0102;
	const auto bytes = RoundTrip(tree);
	// The fixed object's parts, then two bytes, a float, the forest and two bytes
	ASSERT_EQ(bytes.size(), 83u);
	EXPECT_EQ(At<uint32_t>(bytes, 31), 2104u);
	EXPECT_EQ(At<int16_t>(bytes, 71), -2);
	EXPECT_EQ(At<float>(bytes, 73), 0.5f);
	EXPECT_EQ(At<uint32_t>(bytes, 77), 41u);
	EXPECT_EQ(At<uint16_t>(bytes, 81), 0x0102);
	// Each of the tree's values counts once in the sum
	StreamWriter writer;
	writer.Put(tree.firstValue);
	writer.Put(tree.secondValue);
	EXPECT_EQ(writer.Checksum(), (0xFEu + 2) + (0x00u + 4));
}
