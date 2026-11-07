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

#include <algorithm>
#include <vector>

#include <SavedRecords.h>
#include <gtest/gtest.h>

// The records of the creche, the graveyard, the abodes and the fields, on synthetic values: each record's length and
// the places of its values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-towns-buildings).

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

TEST(SaveBuildingRecords, AnAbodeAndACrecheWriteTheSameValuesUnderTheirOwnClasses)
{
	AbodeRecord abode;
	abode.abode.villagers.numbers = {30, 31, 32};
	abode.abode.adults = 2;
	const auto bytes = RoundTrip(abode);
	// The parts of a building of several cells, then the abode's 37 bytes and its three villagers
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 12);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x7u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 21), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 33), 32u);
	EXPECT_EQ(bytes[84 + 37], 2);
	CrecheRecord creche;
	creche.abode = abode.abode;
	const auto crecheBytes = RoundTrip(creche);
	EXPECT_EQ(At<uint32_t>(crecheBytes, 0), 0x50u);
	EXPECT_TRUE(std::equal(crecheBytes.begin() + 4, crecheBytes.begin() + 8, bytes.begin() + 4));
	EXPECT_EQ(crecheBytes.size(), bytes.size());
}

TEST(SaveBuildingRecords, AGraveyardAddsItsGraves)
{
	const auto bytes = RoundTrip(GraveyardRecord {.graves = 5});
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 5u);
}

TEST(SaveBuildingRecords, AFieldAddsItsKindItsTownItsFarmersAndOneByteOfAWord)
{
	FieldRecord field;
	field.typeInfo = 2400;
	field.town = {14};
	field.firstByte = 1;
	field.farmers.numbers = {40, 41};
	field.lastByte = 3;
	const auto bytes = RoundTrip(field);
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 30 + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 121), 2400u);
	EXPECT_EQ(At<uint32_t>(bytes, 133), 14u);
	EXPECT_EQ(bytes[141], 1);
	EXPECT_EQ(At<uint32_t>(bytes, 146), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 154), 41u);
	EXPECT_EQ(bytes[158], 3);
}
