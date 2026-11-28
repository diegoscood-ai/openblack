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

// A puzzle totem writes an abode's record, then the place the hand last pulled it to and the place it was made at, 12
// bytes each, then its highest step and its height
TEST(SaveBuildingRecords, APuzzleTotemAddsItsTwoPlacesItsHighestStepAndItsHeight)
{
	const auto bytes = RoundTrip(PuzzleTotemRecord {.pulledTo = {.x = 0x10000, .z = 0x20000, .altitude = 12.5f},
	                                                .madeAt = {.x = 0x30000, .z = 0x40000, .altitude = 20.0f},
	                                                .maxHeight = -1,
	                                                .height = 3});
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 12 + 12 + 4 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x31u);
	const size_t totem = 17 + 67 + 37;
	EXPECT_EQ(At<int32_t>(bytes, totem), 0x10000);
	EXPECT_EQ(At<int32_t>(bytes, totem + 4), 0x20000);
	EXPECT_EQ(At<float>(bytes, totem + 8), 12.5f);
	EXPECT_EQ(At<int32_t>(bytes, totem + 12), 0x30000);
	EXPECT_EQ(At<float>(bytes, totem + 20), 20.0f);
	EXPECT_EQ(At<int32_t>(bytes, totem + 24), -1);
	EXPECT_EQ(At<int32_t>(bytes, totem + 28), 3);
}

// A fragment writes a rock's record, then its turns left, its mesh index, its building, and its mesh with no flag in
// front: the count of its pieces, three words, then each piece's three words, its triangles' count and 88 bytes each
TEST(SaveBuildingRecords, AFragmentAddsItsTurnsItsMeshIndexItsBuildingAndItsMesh)
{
	const auto rock = RoundTrip(RockRecord {});
	FragmentRecord fragment;
	fragment.turnsLeft = 200;
	fragment.meshIndex = 37;
	fragment.building = {12};
	fragment.mesh.words = {2, 0, 0};
	DestructionMeshPiece piece;
	piece.words = {1, 1, 3};
	piece.triangles.push_back({});
	piece.triangles.back().at(0) = 5;
	fragment.mesh.pieces.push_back(piece);
	const auto bytes = RoundTrip(fragment);
	ASSERT_EQ(bytes.size(), rock.size() + 12 + (4 + 12) + (12 + 4 + 88));
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0xF7u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size()), 200u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size() + 4), 37u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size() + 8), 12u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size() + 12), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size() + 16), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, rock.size() + 40), 1u);
	EXPECT_EQ(bytes[rock.size() + 44], 5);
}

TEST(SaveBuildingRecords, AFootballPitchAddsItsCupFinalItsMatchItsSidesItsWatchersAndItsBall)
{
	FootballPitchRecord pitch;
	pitch.functional = 1;
	pitch.playState = 2;
	pitch.homeTally = 3;
	pitch.homeTeam.numbers = {50, 51};
	pitch.places.at(39) = {60};
	pitch.ballHeading = {1.0f, 0.0f, 0.0f};
	pitch.ball = {70};
	const auto bytes = RoundTrip(pitch);
	// the abode, then the cup final's 32 slots, the twelve 12-byte values, the start's values, the match's clocks
	// and tallies, the referee, the four lists (the home side's two), the nearest four, the fetcher, the 40 places,
	// the heading and the ball
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 128 + 144 + 23 + 20 + 2 + 8 + 4 + (16 + 8) + 16 + 4 + 160 + 12 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x45u);
	EXPECT_EQ(At<uint32_t>(bytes, 121 + 128 + 144), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 70u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 20), 60u);
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
