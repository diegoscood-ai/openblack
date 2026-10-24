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

// The records of the planned buildings, the spell dispenser, a player's interface and its status, the sub-actions and
// the particles' container, on synthetic values: each record's length and the places of its values are the game's,
// and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-planned-buildings-the-players-interface-and-the-particles).

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

TEST(SavePlannedRecords, APlannedBuildingWritesItsPlanThenItsTown)
{
	PlannedAbodeRecord planned;
	planned.planned.yAngle = 1.5f;
	planned.planned.creationTurn = 7;
	planned.planned.footpathLink = {40};
	planned.planned.info = 3;
	planned.town = {41};
	const auto bytes = RoundTrip(planned);
	ASSERT_EQ(bytes.size(), 17u + 14 + 20 + 4 + 4 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x61u);
	EXPECT_EQ(At<float>(bytes, 31), 1.5f);
	EXPECT_EQ(At<int32_t>(bytes, 47), 7);
	EXPECT_EQ(At<uint32_t>(bytes, 51), 40u);
	EXPECT_EQ(At<uint32_t>(bytes, 55), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 59), 41u);
	EXPECT_EQ(At<uint32_t>(RoundTrip(PlannedCitadelHeartRecord {}), 0), 0x39u);
}

TEST(SavePlannedRecords, ASpellDispenserAnEntranceAndASeedHaveTheGamesLengths)
{
	SpellDispenserRecord dispenser;
	dispenser.object = {42};
	dispenser.words = {1, 2, 3, 4};
	const auto dispenserBytes = RoundTrip(dispenser);
	ASSERT_EQ(dispenserBytes.size(), 17u + 67 + 37 + 4 + 16);
	EXPECT_EQ(At<uint32_t>(dispenserBytes, 121), 42u);
	EXPECT_EQ(At<uint32_t>(dispenserBytes, 137), 4u);
	ASSERT_EQ(RoundTrip(CitadelEntranceRecord {}).size(), 17u + 14 + 36);
	const auto seed = RoundTrip(OneOffSpellSeedRecord {.words = {5, 6, 7}});
	ASSERT_EQ(seed.size(), 17u + 68 + 12);
	EXPECT_EQ(At<uint32_t>(seed, 93), 7u);
}

TEST(SaveInterfaceRecords, AnInterfacesStatusStartsWithItsMarkAndEndsWithItsLeash)
{
	EXPECT_EQ(RoundTrip(PlayerInterfaceRecord {.status = {43}}).size(), 35u);
	InterfaceStatusRecord status;
	status.playerNumber = 1;
	status.hand.held.numbers = {44};
	status.cameraPosition = {1.0f, 2.0f, 3.0f};
	status.guidance = {45};
	status.leashStatus = {46};
	status.lastWord = 9;
	const auto bytes = RoundTrip(status);
	ASSERT_EQ(bytes.size(), 222u);
	EXPECT_EQ(At<uint32_t>(bytes, 31), k_InterfaceStatusMark);
	EXPECT_EQ(bytes[35], 1);
	EXPECT_EQ(At<uint32_t>(bytes, 89), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 93), 44u);
	EXPECT_EQ(At<float>(bytes, 126), 1.0f);
	EXPECT_EQ(At<uint32_t>(bytes, 206), 45u);
	EXPECT_EQ(At<uint32_t>(bytes, 210), 46u);
	EXPECT_EQ(At<uint32_t>(bytes, 218), 9u);
	EXPECT_EQ(RoundTrip(LeashStatusRecord {.object = {47}}).size(), 54u);
}

TEST(SaveInterfaceRecords, ASubActionWritesItsArgumentsAndAnArgumentItsSixtyFourBytes)
{
	EXPECT_EQ(RoundTrip(ComputerSeenRecord {.object = {48}, .word = 2}).size(), 25u);
	const auto action = RoundTrip(PlayerSubActionRecord {.word = 1, .arguments = {.numbers = {49, 50}}});
	ASSERT_EQ(action.size(), 33u);
	EXPECT_EQ(At<uint32_t>(action, 21), 2u);
	EXPECT_EQ(At<uint32_t>(action, 29), 50u);
	PlayerSubActionArgumentRecord argument;
	argument.bytes.values.assign(64, 0);
	argument.bytes.values[0] = 0x12;
	argument.point = {4.0f, 5.0f, 6.0f};
	const auto bytes = RoundTrip(argument);
	ASSERT_EQ(bytes.size(), 117u);
	EXPECT_EQ(At<uint32_t>(bytes, 25), 64u);
	EXPECT_EQ(bytes[29], 0x12);
	EXPECT_EQ(At<float>(bytes, 113), 6.0f);
}

TEST(SaveInterfaceRecords, AParticleContainerAndAParticleInterfaceHaveTheGamesLengths)
{
	const auto container = RoundTrip(ParticleContainerRecord {.byte = 1, .object = {51}, .system = {52}, .lastWord = 3});
	ASSERT_EQ(container.size(), 48u);
	EXPECT_EQ(At<uint32_t>(container, 36), 52u);
	EXPECT_EQ(At<uint32_t>(container, 44), 3u);
	EXPECT_EQ(RoundTrip(ParticleInterfaceRecord {.system = {53}}).size(), 21u);
}
