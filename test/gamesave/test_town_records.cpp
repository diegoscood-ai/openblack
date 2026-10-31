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

// The records of the citadel, the footpaths, the storage pit, the piles and the town, on synthetic values: each
// record's length and the places of its values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town).

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

/// Writes the record, reads it back and writes it again: the same bytes, and the record's own class
std::vector<uint8_t> RoundTrip(const SavedRecord& record)
{
	const auto bytes = Bytes(record);
	const auto read = Read(bytes);
	EXPECT_TRUE(read.has_value());
	if (read.has_value())
	{
		EXPECT_EQ(TypeOf(*read), TypeOf(record));
		EXPECT_EQ(Bytes(*read), bytes);
	}
	return bytes;
}

Counted<float> Seventeen(float value)
{
	return {.values = std::vector<float>(17, value)};
}
} // namespace

TEST(SaveTownRecords, TheCitadelWritesItsHeartItsSixPartsAndFourLists)
{
	CitadelRecord citadel;
	citadel.container = {
	    .position = {.position = {.x = 0x10000, .z = 0x20000, .altitude = 3.0f}, .flags = 0}, .info = 812, .owner = {2}};
	citadel.heart = {15};
	citadel.parts.numbers = {16, 17, 0, 0, 0, 0};
	citadel.thirdList.numbers = {18};
	citadel.thirdWord = -1;
	const auto bytes = RoundTrip(citadel);
	// The container part, the heart, the six parts with their count, four lists and three words
	ASSERT_EQ(bytes.size(), 75u + (6 * 4) + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 31), 812u);
	EXPECT_EQ(At<uint32_t>(bytes, 35), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 39), 15u);
	EXPECT_EQ(At<uint32_t>(bytes, 43), 6u);
	EXPECT_EQ(At<uint32_t>(bytes, 47), 16u);
	EXPECT_EQ(At<uint32_t>(bytes, 79), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 83), 18u);
	EXPECT_EQ(At<int32_t>(bytes, bytes.size() - 4), -1);
}

TEST(SaveTownRecords, TheHeartAddsTheCitadelPiecesPartAndItsOwnValues)
{
	CitadelHeartRecord heart;
	heart.base.multi.footpathLink = 20;
	heart.influence = 0.5f;
	heart.citadel = {14};
	heart.town = {21};
	heart.destructionStarted = 1;
	heart.entrance = {22};
	const auto bytes = RoundTrip(heart);
	// The parts of a building of several cells, the piece's part and the heart's values
	ASSERT_EQ(bytes.size(), 149u);
	EXPECT_EQ(At<uint32_t>(bytes, 76), 20u);
	EXPECT_EQ(At<float>(bytes, 84), 0.5f);
	EXPECT_EQ(At<uint32_t>(bytes, 88), 14u);
	EXPECT_EQ(At<uint32_t>(bytes, 100), 21u);
	EXPECT_EQ(bytes[116], 1);
	EXPECT_EQ(At<uint32_t>(bytes, 145), 22u);
}

TEST(SaveTownRecords, TheFootpathsWriteTheirListsAndTheirNodesTheirPlaces)
{
	ASSERT_EQ(RoundTrip(FootpathLinkRecord {.footpaths = {.numbers = {30, 31}}}).size(), 17u + 4 + 8);
	const auto footpath = RoundTrip(FootpathRecord {.nodes = {.numbers = {32}}, .active = 1});
	ASSERT_EQ(footpath.size(), 17u + 8 + 4);
	EXPECT_EQ(At<uint32_t>(footpath, 25), 1u);
	const auto node = RoundTrip(
	    FootpathNodeRecord {.coords = {.x = 7, .z = 8, .altitude = 1.0f}, .flags = 3, .followers = {.numbers = {40}}});
	ASSERT_EQ(node.size(), 17u + 12 + 1 + 8);
	EXPECT_EQ(At<int32_t>(node, 21), 8);
	EXPECT_EQ(node[29], 3);
	EXPECT_EQ(At<uint32_t>(node, 34), 40u);
	ASSERT_EQ(RoundTrip(FootpathLinkSaveRecord {.coords = {}, .link = {33}}).size(), 17u + 12 + 4);
}

TEST(SaveTownRecords, AStoragePitIsAnAbodeWithItsPiles)
{
	StoragePitRecord pit;
	pit.abode.town = {14};
	pit.abode.resources.values = {120, 45};
	pit.foodPile.numbers = {50};
	pit.otherPiles.numbers = {51, 0, 0, 0, 0};
	const auto bytes = RoundTrip(pit);
	// The parts of a building of several cells, the abode's 37 bytes with two resources, the piles
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 8 + 8 + 24);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 17), 14u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 29), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 33), 120u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 41), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 45), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 84 + 49), 50u);
}

TEST(SaveTownRecords, ABuildingWritesItsDestructionMeshAfterItsFlag)
{
	StoragePitRecord pit;
	auto& mesh = pit.abode.destructionMesh;
	mesh.present = 1;
	mesh.words = {1, 2, 3};
	mesh.pieces.resize(2);
	mesh.pieces[1].words = {4, 5, 6};
	mesh.pieces[1].triangles.resize(2);
	mesh.pieces[1].triangles[1][0] = 0x2A;
	const auto bytes = Bytes(pit);
	const auto plain = Bytes(StoragePitRecord {});
	// The count of pieces and three words, an empty piece, then a piece of two triangles
	ASSERT_EQ(bytes.size(), plain.size() + 16 + 16 + 16 + 2 * 88);
	EXPECT_EQ(At<uint32_t>(bytes, plain.size() - 8), 2u);
	EXPECT_EQ(bytes[bytes.size() - 8 - 88], 0x2A);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(std::get<StoragePitRecord>(*read).abode.destructionMesh, mesh);
}

TEST(SaveTownRecords, ThePilesAreMovingObjectsWithAPotsValuesAndTheirHeight)
{
	WoodPileRecord wood;
	wood.base.mobileObject.object = 60;
	wood.pile.resourceType = 1;
	wood.pile.resourceAmount = 250;
	wood.pile.building = {61};
	wood.pile.altitudeZoomer[0] = 0x3F;
	const auto bytes = RoundTrip(wood);
	// The moving object's 68 bytes, the pot's 13, the building's pot's 9 and the height's 48
	ASSERT_EQ(bytes.size(), 17u + 68 + 13 + 9 + 48);
	EXPECT_EQ(At<uint32_t>(bytes, 69), 60u);
	EXPECT_EQ(At<uint32_t>(bytes, 85), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 93), 250u);
	EXPECT_EQ(At<uint32_t>(bytes, 98), 61u);
	EXPECT_EQ(bytes[107], 0x3F);
	FoodPileRecord food;
	food.store = {62};
	const auto foodBytes = RoundTrip(food);
	ASSERT_EQ(foodBytes.size(), bytes.size() + 4);
	EXPECT_EQ(At<uint32_t>(foodBytes, foodBytes.size() - 4), 62u);
}

TEST(SaveTownRecords, ATownWritesItsDesiresItsListsAndItsTables)
{
	TownRecord town;
	town.container.owner = {2};
	town.mainStoragePit = {16};
	for (auto* desires :
	     {&town.desires.base, &town.desires.modifier, &town.desires.cheat, &town.desires.boost, &town.desires.desire,
	      &town.desires.raw, &town.desires.functionMinimum, &town.desires.functionMaximum, &town.desires.change,
	      &town.desires.previousVillagerStateAmount, &town.desires.previousVillagerStateCount,
	      &town.desires.villagerStateAmount, &town.desires.villagerStateCount})
	{
		*desires = Seventeen(0.25f);
	}
	town.desires.sorts.values.assign(17, DesireSort {});
	town.desires.sortsAgain.values.assign(17, DesireSort {});
	town.desires.alignmentChanges.values.assign(17, 0);
	town.playerNumber = 1;
	town.temporaryStores.numbers = {0, 0};
	town.resourceLastRemovedTurn = {.rows = 8, .columns = 2, .values = std::vector<uint32_t>(16, 7)};
	town.desireFlags.numbers.assign(17, 0);
	town.emptyTownTimer = 9;
	const auto bytes = RoundTrip(town);
	// An empty town has 2308 bytes; the game's desires add 17 values to each of their 16 arrays, the stores two
	// references, the table 16 values and the desire flags 17 references
	ASSERT_EQ(bytes.size(), 2308u + (13 * 68) + (2 * 204) + 68 + 8 + 64 + 68);
	EXPECT_EQ(At<uint32_t>(bytes, 39), 16u);
	EXPECT_EQ(At<uint32_t>(bytes, 43), 17u);
	EXPECT_EQ(At<float>(bytes, 47), 0.25f);
	// The desires end 1436 bytes after they start, then the camera, the identity, the tribe and the player
	EXPECT_EQ(bytes[43 + 1436 + 32], 1);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 9u);
	// From the end: the timer, the belief scale, the missionaries, the 17 desire flags, the flocks, then the table's 16
	// values, its columns and its rows
	constexpr size_t k_TableRowsFromEnd = 4 + 4 + 4 + (4 + (17 * 4)) + 4 + (16 * 4) + 4 + 4;
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - k_TableRowsFromEnd), 8u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - k_TableRowsFromEnd + 4), 2u);
}
