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

// The records of the world file's first classes, on synthetic values: each record's length and the places of its
// values are the game's, and each reads back as it was written (docs/wiki/engine/saved-games.md#an-objects-record).

using namespace openblack::gamesave;

namespace
{
/// The objects the tests' streams have before the record, so that the small numbers are earlier objects
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0x27000};

template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

std::vector<uint8_t> Bytes(const SavedRecord& record)
{
	StreamWriter writer(k_After100);
	Write(writer, record, nullptr);
	return writer.Take();
}

/// Reads a record written by Bytes, its class's number and its own word first
std::optional<SavedRecord> Read(const std::vector<uint8_t>& bytes)
{
	StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	SavedRecord record;
	if (!ReadRecord(reader, type, extra, record, nullptr) || reader.Position() != bytes.size())
	{
		return std::nullopt;
	}
	return record;
}

PlayerRecord APlayer()
{
	PlayerRecord player;
	player.thing = {.extra = 0, .checksum = 0, .destroyed = 0, .flags = 0};
	player.playerNumber = 0;
	player.alignment = {.classWord = 0x008CC7F0, .destroyed = 0, .value = 0.061467f, .change = 0.0f};
	player.info = 1743;
	player.tribalPower.assign(9, 1.0f);
	player.maxTribalPower.assign(9, 1.0f);
	player.type = 1;
	player.name.assign(32, u'\0');
	player.name[0] = u'J';
	player.userId = 1070189248;
	player.computerPlayer = 3;
	player.totalPopulation = 47;
	player.start = {.position = {.x = 0x00DF71B0, .z = 0x013ACEB2, .altitude = 20.0f}, .first = 1, .second = -1};
	player.magicTypeEnabled.assign(42, 0);
	player.magicTypeEnabled[3] = 1;
	player.magicTypeEverBeenEnabled.assign(42, 0);
	player.gameStats = 13;
	player.citadel = 14;
	player.towns = {20, 21};
	player.influencePower = 2.5f;
	player.averageInfluencePower = 2.0f;
	player.damageFromPlayer.assign(8, 0.0f);
	player.allies.assign(8, 0.0f);
	player.interfaces.assign(18, 0);
	player.interfaces[0] = 30;
	player.lastWord = 7;
	return player;
}

ComputerPlayerRecord AComputerPlayer()
{
	ComputerPlayerRecord computer;
	computer.position = {.position = {.x = 1, .z = 2, .altitude = 3.0f}, .flags = 0x0100};
	computer.player = 2;
	computer.actionState = 4;
	computer.lists[1] = {50};
	computer.words = {1, 2, 3, 4};
	computer.gameWord = 0xABCD;
	computer.table.assign(42, 9);
	computer.tableWord = 5;
	computer.numbers = {-1, 0, 1};
	computer.byte = 1;
	computer.attitudes = {5, 6, 7, 8, 9, 10, 11, 12};
	computer.desires[0] = {.suppression = 1.0f, .personality = 0.25f};
	computer.desires[197] = {.suppression = 0.5f, .personality = 1.0f};
	return computer;
}
} // namespace

TEST(SaveRecords, EveryRecordStartsWithItsClassItsWordAndTheRunningSum)
{
	StreamWriter writer(k_After100);
	WriteThing(writer, k_PlayerType, {.extra = 3, .checksum = 0, .destroyed = 1, .flags = 0x40});
	const auto& bytes = writer.Bytes();
	ASSERT_EQ(bytes.size(), 17u);
	EXPECT_EQ(At<uint32_t>(bytes, 0), k_PlayerType);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 8), 0x27000u + (k_PlayerType + 4) + (3 + 4));
	EXPECT_EQ(At<uint32_t>(bytes, 12), 1u);
	EXPECT_EQ(bytes[16], 0x40);
}

TEST(SaveRecords, AnObjectOfSeveralCellsThatCanMoveHasTheGamesPlaces)
{
	// A static object that can move: the parts of an object, a fixed object, one of several cells, then its own
	const ThingPart thing {.extra = 0, .checksum = 0, .destroyed = 0, .flags = 0};
	const PositionPart position {.position = {.x = 0x10000, .z = 0x20000, .altitude = 1.5f}, .flags = 0x0103};
	const ObjectPart object {.info = 2104,
	                         .coords = {.x = 0x10000, .z = 0x20000, .altitude = 1.5f},
	                         .creationIndex = 29,
	                         .life = 1.0f,
	                         .scale = 0.75f,
	                         .heading = 1.25f,
	                         .fire = 0,
	                         .physics = std::nullopt};
	StreamWriter writer(k_After100);
	WriteThing(writer, 0x54, thing);
	Write(writer, thing, position, object, nullptr);
	Write(writer, FixedPart {.townArtifact = 7}, nullptr);
	Write(writer, MultiMapFixedPart {.fixedFlags = 2, .percentBuilt = 0.5f, .footpathLink = 8, .buildingSite = 0}, nullptr);
	Write(writer, MobileStaticPart {.player = 2, .words = {11, 12}}, nullptr);
	const auto& bytes = writer.Bytes();
	ASSERT_EQ(bytes.size(), 96u);
	EXPECT_EQ(At<uint16_t>(bytes, 29), 0x0103);
	EXPECT_EQ(At<uint32_t>(bytes, 31), 2104u);
	EXPECT_EQ(At<uint32_t>(bytes, 47), 29u);
	EXPECT_EQ(At<float>(bytes, 55), 0.75f);
	EXPECT_EQ(At<uint32_t>(bytes, 67), 7u);
	EXPECT_EQ(bytes[71], 2);
	EXPECT_EQ(At<float>(bytes, 72), 0.5f);
	EXPECT_EQ(At<uint32_t>(bytes, 76), 8u);
	EXPECT_EQ(At<uint32_t>(bytes, 84), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 92), 12u);

	StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	ThingPart readThing;
	PositionPart readPosition;
	ObjectPart readObject;
	FixedPart fixed;
	MultiMapFixedPart multi;
	MobileStaticPart part;
	ASSERT_TRUE(ReadThing(reader, readThing));
	ASSERT_TRUE(Read(reader, readPosition));
	ASSERT_TRUE(Read(reader, readThing, readPosition, readObject, nullptr));
	ASSERT_TRUE(Read(reader, fixed, nullptr));
	ASSERT_TRUE(Read(reader, multi, nullptr));
	ASSERT_TRUE(Read(reader, part, nullptr));
	EXPECT_EQ(reader.Position(), bytes.size());
	EXPECT_EQ(readPosition, position);
	EXPECT_EQ(readObject, object);
	EXPECT_EQ(fixed.townArtifact, 7u);
	EXPECT_EQ(multi.footpathLink, 8u);
	EXPECT_EQ(part.words[1], 12u);
	EXPECT_EQ(reader.SumsRead(), 1u);
	EXPECT_FALSE(reader.FirstWrongSum().has_value());
}

TEST(SaveRecords, AnObjectInThePhysicsWritesItsMatrixAndItsSpeedsAndAMovingObjectItsAngles)
{
	const ThingPart thing {};
	const PositionPart inPhysics {.position = {}, .flags = k_PositionFlagInPhysics};
	ObjectPart object;
	object.physics = PhysicsPart {.velocity = {1.0f, 2.0f, 3.0f}, .spin = {0.0f, 0.5f, 0.0f}};
	StreamWriter writer(k_After100);
	Write(writer, thing, inPhysics, object, nullptr);
	Write(writer, MobilePart {.mobileWord = 0x0102});
	Write(writer, MobileObjectPart {.object = 9, .xAngle = 0.5f, .zAngle = -0.5f, .path = 0}, nullptr);
	// 14 of the position, 36 of the object, 72 of the body, 2 of a moving thing and 16 of a moving object
	ASSERT_EQ(writer.Bytes().size(), 14u + 36 + 72 + 2 + 16);
	EXPECT_EQ(At<float>(writer.Bytes(), 50 + 48), 1.0f);
	EXPECT_EQ(At<uint16_t>(writer.Bytes(), 122), 0x0102);

	// A thing that particles carry writes no body, whatever its position's flags
	const ThingPart carried {.extra = 0, .checksum = 0, .destroyed = 0, .flags = k_ThingFlagCarriedByParticles};
	StreamWriter without(k_After100);
	Write(without, carried, inPhysics, object, nullptr);
	EXPECT_EQ(without.Bytes().size(), 14u + 36);

	StreamReader reader(writer.Bytes(), k_After100);
	PositionPart position;
	ObjectPart read;
	MobilePart mobile;
	MobileObjectPart moving;
	ASSERT_TRUE(Read(reader, position));
	ASSERT_TRUE(Read(reader, thing, position, read, nullptr));
	ASSERT_TRUE(Read(reader, mobile));
	ASSERT_TRUE(Read(reader, moving, nullptr));
	EXPECT_EQ(read, object);
	EXPECT_EQ(moving.zAngle, -0.5f);
}

TEST(SaveRecords, APlayerHasTheGamesPlacesAndReadsBack)
{
	const auto player = APlayer();
	const auto bytes = Bytes(player);
	ASSERT_EQ(bytes.size(), 644u + 2 * 4);
	EXPECT_EQ(bytes[17], 0);
	EXPECT_EQ(At<uint32_t>(bytes, 18), 0x008CC7F0u);
	EXPECT_EQ(At<uint32_t>(bytes, 34), 1743u);
	EXPECT_EQ(At<uint32_t>(bytes, 38), 9u);
	EXPECT_EQ(At<uint32_t>(bytes, 122), 32u);
	EXPECT_EQ(At<char16_t>(bytes, 126), u'J');
	EXPECT_EQ(At<uint32_t>(bytes, 190), 1070189248u);
	// The computer player's reference, then the population and the start
	EXPECT_EQ(At<uint32_t>(bytes, 214), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 218), 47u);
	EXPECT_EQ(At<int32_t>(bytes, 238), -1);
	EXPECT_EQ(At<uint32_t>(bytes, 242), 42u);
	EXPECT_EQ(At<int32_t>(bytes, 246 + 3 * 4), 1);
	EXPECT_EQ(At<uint32_t>(bytes, 414), 42u);
	EXPECT_EQ(At<uint32_t>(bytes, 460), 13u);
	EXPECT_EQ(At<uint32_t>(bytes, 464), 14u);
	EXPECT_EQ(At<uint32_t>(bytes, 472), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 476), 20u);
	EXPECT_EQ(At<uint32_t>(bytes, 568), 18u);
	EXPECT_EQ(At<uint32_t>(bytes, 572), 30u);
	EXPECT_EQ(At<uint32_t>(bytes, 648), 7u);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	auto expected = player;
	expected.thing.checksum = At<uint32_t>(bytes, 8);
	EXPECT_EQ(std::get<PlayerRecord>(*read), expected);
	EXPECT_EQ(Bytes(*read), bytes);
}

TEST(SaveRecords, AComputerPlayerWritesEveryNodeOfItsTreeWithNoCount)
{
	const auto computer = AComputerPlayer();
	const auto bytes = Bytes(computer);
	// The fixed parts, one object in its second list, and two values for each of the 198 nodes
	ASSERT_EQ(bytes.size(), 1880u + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 31), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 35), 4u);
	EXPECT_EQ(At<uint32_t>(bytes, 39), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 43), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 47), 50u);
	EXPECT_EQ(At<uint32_t>(bytes, 71), 0xABCDu);
	EXPECT_EQ(At<uint32_t>(bytes, 75), 42u);
	EXPECT_EQ(At<uint32_t>(bytes, 247), 5u);
	EXPECT_EQ(bytes[263], 1);
	EXPECT_EQ(At<uint32_t>(bytes, 264), 8u);
	EXPECT_EQ(At<float>(bytes, 300), 1.0f);
	EXPECT_EQ(At<float>(bytes, 304), 0.25f);
	EXPECT_EQ(At<float>(bytes, bytes.size() - 8), 0.5f);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(Bytes(*read), bytes);
	EXPECT_EQ(std::get<ComputerPlayerRecord>(*read).desires[197].suppression, 0.5f);
}

TEST(SaveRecords, TheActionStateTheAttitudeAndTheStatisticsHaveTheGamesLengths)
{
	ActionStateRecord state;
	state.computerPlayer = 3;
	state.objects = {40, 41};
	state.words[7] = 9;
	state.block[0] = 0x55;
	state.point = {1.0f, 2.0f, 3.0f};
	state.object = 42;
	state.otherPoint = {4.0f, 5.0f, 6.0f};
	const auto stateBytes = Bytes(state);
	ASSERT_EQ(stateBytes.size(), 119u + 2 * 4);
	EXPECT_EQ(At<uint32_t>(stateBytes, 31), 3u);
	EXPECT_EQ(At<uint32_t>(stateBytes, 35), 2u);
	EXPECT_EQ(At<uint32_t>(stateBytes, 75), 9u);
	EXPECT_EQ(stateBytes[79], 0x55);
	EXPECT_EQ(At<float>(stateBytes, 91), 1.0f);
	EXPECT_EQ(At<uint32_t>(stateBytes, 107), 42u);
	EXPECT_EQ(At<float>(stateBytes, 123), 6.0f);
	ASSERT_TRUE(Read(stateBytes).has_value());
	EXPECT_EQ(Bytes(*Read(stateBytes)), stateBytes);

	const auto attitude = Bytes(ComputerAttitudeRecord {.thing = {.extra = 1}, .attitude = 0.5f});
	ASSERT_EQ(attitude.size(), 21u);
	EXPECT_EQ(At<uint32_t>(attitude, 4), 1u);
	EXPECT_EQ(At<float>(attitude, 17), 0.5f);

	GameStatsRecord stats;
	stats.player = 2;
	stats.townStats[9] = 4;
	stats.counts[0] = 12;
	stats.influence.data[0] = 0.5f;
	stats.population.index = 3;
	stats.casts[35] = 6;
	stats.last[2] = 2;
	const auto statsBytes = Bytes(stats);
	// The two graphs are one value each, of 2012 bytes
	ASSERT_EQ(statsBytes.size(), 4421u);
	EXPECT_EQ(At<uint32_t>(statsBytes, 17), 2u);
	EXPECT_EQ(At<uint32_t>(statsBytes, 33 + 36), 4u);
	EXPECT_EQ(At<uint32_t>(statsBytes, 73), 12u);
	EXPECT_EQ(At<float>(statsBytes, 173), 0.5f);
	EXPECT_EQ(At<uint32_t>(statsBytes, 173 + 2012 + 2004), 3u);
	EXPECT_EQ(At<uint32_t>(statsBytes, 4421 - 16), 6u);
	EXPECT_EQ(At<uint32_t>(statsBytes, 4421 - 4), 2u);
	// The sum counts each graph once, by its first byte
	StreamWriter writer(k_After100);
	writer.Put(stats.influence);
	EXPECT_EQ(writer.Checksum(), 0x27000u + 2012);
	ASSERT_TRUE(Read(statsBytes).has_value());
	EXPECT_EQ(Bytes(*Read(statsBytes)), statsBytes);
}

TEST(SaveRecords, ACutRecordOrAClassThatIsNotKnownReadsAsNothing)
{
	const auto bytes = Bytes(APlayer());
	EXPECT_FALSE(Read(std::vector<uint8_t>(bytes.begin(), bytes.end() - 1)).has_value());
	StreamReader reader(bytes, k_After100);
	SavedRecord record;
	// A number that names no class of the game
	EXPECT_FALSE(ReadRecord(reader, 0xFFF0, 0, record, nullptr));
	EXPECT_EQ(reader.UnknownType(), 0xFFF0u);
	EXPECT_TRUE(IsKnownType(k_GameStatsType));
	EXPECT_FALSE(IsKnownType(0xFFF0));
}
