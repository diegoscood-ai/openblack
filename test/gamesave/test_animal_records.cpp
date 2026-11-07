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

// The records of the animals, the birds and a town's desire flags, on synthetic values: each record's length and the
// places of its values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-animals-and-the-towns-desire-flags).

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

TEST(SaveAnimalRecords, AnAnimalWritesALivingThingsPartsThenItsOwnValues)
{
	PigRecord pig;
	pig.living.flock = {40};
	pig.animal.object = {41};
	pig.animal.hunger = -3;
	pig.animal.foodPosition = {5, 6};
	pig.animal.secondObject = {42};
	pig.animal.deathReason = 7;
	pig.animal.angleZoomer[0] = 0x21;
	const auto bytes = RoundTrip(pig);
	// The head, the position, the object, the moving thing's word, the walk round obstacles, the living thing, the
	// animal
	ASSERT_EQ(bytes.size(), 17u + 14 + 36 + 2 + 39 + 67 + 85);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x87u);
	EXPECT_EQ(At<uint32_t>(bytes, 175), 41u);
	EXPECT_EQ(At<int16_t>(bytes, 179), -3);
	EXPECT_EQ(At<int32_t>(bytes, 191), 6);
	EXPECT_EQ(At<uint32_t>(bytes, 195), 42u);
	EXPECT_EQ(bytes[211], 7);
	EXPECT_EQ(bytes[212], 0x21);
}

TEST(SaveAnimalRecords, ABirdKeepsItsOwnClass)
{
	DoveRecord dove;
	dove.animal.tiredness = 9;
	const auto bytes = RoundTrip(dove);
	ASSERT_EQ(bytes.size(), 260u);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x8Eu);
	EXPECT_EQ(At<int16_t>(bytes, 181), 9);
}

TEST(SaveAnimalRecords, ATownsDesireFlagsWriteItsTownThenFourValues)
{
	const auto bytes = RoundTrip(TownDesireFlagsRecord {.town = {43}, .slot = 2, .desire = 0.25f, .desireType = 4, .mesh = 5});
	ASSERT_EQ(bytes.size(), 17u + 14 + 36 + 4 + 16);
	EXPECT_EQ(At<uint32_t>(bytes, 67), 43u);
	EXPECT_EQ(At<int32_t>(bytes, 71), 2);
	EXPECT_EQ(At<float>(bytes, 75), 0.25f);
	EXPECT_EQ(At<uint32_t>(bytes, 83), 5u);
}

TEST(SaveAnimalRecords, TheBirdsAndTheWolfASpellMakesAddTheirSpellsValues)
{
	SpellDoveRecord dove;
	dove.spell[0] = 0x31;
	const auto doveBytes = RoundTrip(dove);
	ASSERT_EQ(doveBytes.size(), 260u + 48);
	EXPECT_EQ(At<uint32_t>(doveBytes, 0), 0x90u);
	EXPECT_EQ(doveBytes[260], 0x31);
	EXPECT_EQ(At<uint32_t>(RoundTrip(SpellBatRecord {}), 0), 0x95u);
	SpellWolfRecord wolf;
	wolf.point = {1.0f, 2.0f, 3.0f};
	wolf.word = 4;
	wolf.other = {44};
	const auto wolfBytes = RoundTrip(wolf);
	ASSERT_EQ(wolfBytes.size(), 260u + 12 + 16 + 4 + 48 + 4);
	EXPECT_EQ(At<float>(wolfBytes, 260), 1.0f);
	EXPECT_EQ(At<uint32_t>(wolfBytes, 288), 4u);
	EXPECT_EQ(At<uint32_t>(wolfBytes, 340), 44u);
}
