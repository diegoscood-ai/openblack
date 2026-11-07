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

// The records of the particle manager and its atoms, the spell icons, the wonder, a script's highlight, the magic food,
// a workshop's building site and the dead trees, on synthetic values: each record's length and the places of its
// values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-particles-the-spell-icons-and-the-destruction-meshes).

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

TEST(SaveParticleRecords, TheParticleManagerEndsWithItsSpellsTargets)
{
	ParticleManagerRecord manager;
	manager.systems.numbers = {40};
	manager.persistent = {.first = 7, .second = 8};
	manager.targets.targets.elements = {{.word = 3, .object = {41}}};
	manager.targets.points.values = {Point {1.0f, 2.0f, 3.0f}};
	manager.targets.word = 9;
	const auto bytes = RoundTrip(manager);
	ASSERT_EQ(bytes.size(), 184u + 4 + 8 + 12);
	EXPECT_EQ(At<uint32_t>(bytes, 25), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 29), 40u);
	EXPECT_EQ(At<uint32_t>(bytes, 168), 7u);
	EXPECT_EQ(At<uint32_t>(bytes, 176), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 184), 41u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 9u);
	AtomCollectionRecord atoms;
	atoms.persistents.elements = {{.first = 1, .second = 2}};
	const auto atomBytes = RoundTrip(atoms);
	ASSERT_EQ(atomBytes.size(), 55u + 8);
	EXPECT_EQ(At<uint32_t>(atomBytes, 25), 2u);
}

TEST(SaveParticleRecords, TheSpellIconsTheWonderAndTheHighlightHaveTheGamesLengths)
{
	ASSERT_EQ(RoundTrip(TownSpellIconRecord {.town = {42}}).size(), 108u);
	const auto centre = RoundTrip(TownCentreSpellIconRecord {.words = {.values = {1, 2, 3}}});
	ASSERT_EQ(centre.size(), 116u + 12);
	EXPECT_EQ(At<uint32_t>(centre, 112), 3u);
	WorshipSpellIconRecord worship;
	worship.objects.numbers = {43};
	const auto worshipBytes = RoundTrip(worship);
	ASSERT_EQ(worshipBytes.size(), 137u + 4);
	EXPECT_EQ(At<uint32_t>(worshipBytes, 137), 43u);
	ASSERT_EQ(RoundTrip(WonderRecord {.word = 5}).size(), 125u);
	ASSERT_EQ(RoundTrip(ScriptHighlightRecord {}).size(), 95u);
}

TEST(SaveParticleRecords, TheMagicFoodTheWorkshopsSiteAndTheDeadTreesHaveTheGamesLengths)
{
	ASSERT_EQ(RoundTrip(MagicFoodRecord {.player = {44}}).size(), 163u);
	ASSERT_EQ(RoundTrip(WorkshopBuildingSiteRecord {}).size(), 17u + 4 + 12 + 4 + 0x600 + 16);
	const auto felled = RoundTrip(FelledTreeRecord {.info = 6, .word = 7});
	ASSERT_EQ(felled.size(), 116u);
	EXPECT_EQ(At<uint32_t>(felled, 0), 0x71u);
	EXPECT_EQ(At<uint32_t>(felled, 108), 6u);
	EXPECT_EQ(At<uint32_t>(RoundTrip(DeadTreeRecord {}), 0), 0x70u);
}
