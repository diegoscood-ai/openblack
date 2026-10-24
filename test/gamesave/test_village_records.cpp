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

// The records of the town centre, a script's reminder, a mobile static, a fish farm, a data path, a standard building
// site and a flock, on synthetic values: each record's length and the places of its values are the game's, and each
// reads back as it was written (docs/wiki/engine/saved-games.md#the-town-centre-the-flocks-and-the-building-sites).

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

TEST(SaveVillageRecords, ATownCentreIsAnAbodeWithItsTotemAndSixIcons)
{
	TownCentreRecord centre;
	centre.totemStatue = {40};
	centre.icons.numbers = {41, 0, 0, 0, 0, 0};
	const auto bytes = RoundTrip(centre);
	ASSERT_EQ(bytes.size(), 17u + 67 + 37 + 4 + 28);
	EXPECT_EQ(At<uint32_t>(bytes, 121), 40u);
	EXPECT_EQ(At<uint32_t>(bytes, 125), 6u);
	EXPECT_EQ(At<uint32_t>(bytes, 129), 41u);
}

TEST(SaveVillageRecords, AScriptsReminderKeepsItsTwoObjectsAndItsValues)
{
	ScriptReminderRecord reminder {.object = {42}, .secondValue = 3, .words = {1, 2, 3, 4, 5}, .other = {43}};
	const auto bytes = RoundTrip(reminder);
	ASSERT_EQ(bytes.size(), 69u);
	EXPECT_EQ(At<uint32_t>(bytes, 17), 42u);
	EXPECT_EQ(At<uint32_t>(bytes, 33), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 45), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 65), 43u);
}

TEST(SaveVillageRecords, AMobileStaticAFishFarmAndADataPathHaveTheGamesLengths)
{
	ASSERT_EQ(RoundTrip(MobileStaticRecord {.mobileStatic = {.player = 2, .words = {7, 8}}}).size(), 96u);
	FishFarmRecord farm;
	farm.villagers.numbers = {44};
	farm.town = {14};
	farm.second = 0.5f;
	const auto farmBytes = RoundTrip(farm);
	ASSERT_EQ(farmBytes.size(), 17u + 67 + 8 + 12);
	EXPECT_EQ(At<uint32_t>(farmBytes, 88), 44u);
	EXPECT_EQ(At<uint32_t>(farmBytes, 92), 14u);
	EXPECT_EQ(At<float>(farmBytes, 100), 0.5f);
	const auto path = RoundTrip(DataPathRecord {.trackNumber = -1, .reverse = 1, .last = 2.0f});
	ASSERT_EQ(path.size(), 41u);
	EXPECT_EQ(At<int32_t>(path, 17), -1);
	EXPECT_EQ(At<float>(path, 37), 2.0f);
}

TEST(SaveVillageRecords, AStandardBuildingSiteWritesItsPositionsAsOneValue)
{
	StandardBuildingSiteRecord site;
	site.rootBuilding = {45};
	site.workers.numbers = {46};
	site.positions[0] = 0x11;
	site.woodPile = {47};
	const auto bytes = RoundTrip(site);
	ASSERT_EQ(bytes.size(), 17u + 4 + 16 + 4 + 0x600 + 16 + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 17), 45u);
	EXPECT_EQ(At<uint32_t>(bytes, 25), 46u);
	EXPECT_EQ(bytes[41], 0x11);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 47u);
	// The positions count once in the sum, by their first byte
	StreamWriter writer;
	writer.Put(site.positions);
	EXPECT_EQ(writer.Checksum(), 0x11u + 0x600);
}

TEST(SaveVillageRecords, AFlockWritesItsMembersAndItsDomain)
{
	FlockRecord flock;
	flock.shepherd = {48};
	flock.members.numbers = {49, 50};
	flock.domainRadius = 300;
	flock.savedDomainCentre = {.x = 1, .z = 2, .altitude = 3.0f};
	flock.lastWord = 9;
	const auto bytes = RoundTrip(flock);
	ASSERT_EQ(bytes.size(), 135u + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 51), 48u);
	EXPECT_EQ(At<uint32_t>(bytes, 63), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 71), 50u);
	EXPECT_EQ(At<uint16_t>(bytes, 83), 300u);
	EXPECT_EQ(At<int32_t>(bytes, 103), 2);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 9u);
}

TEST(SaveVillageRecords, ATotemStatueAMagicWoodAWorkshopAScaffoldAndAReactionHaveTheGamesLengths)
{
	const auto totem = RoundTrip(TotemStatueRecord {.object = {51}, .words = {1, 2, 3}});
	ASSERT_EQ(totem.size(), 17u + 67 + 4 + 12);
	EXPECT_EQ(At<uint32_t>(totem, 84), 51u);
	EXPECT_EQ(At<uint32_t>(totem, 96), 3u);
	MagicWoodRecord wood;
	wood.player = {2};
	const auto woodBytes = RoundTrip(wood);
	ASSERT_EQ(woodBytes.size(), 17u + 68 + 13 + 9 + 48 + 4);
	EXPECT_EQ(At<uint32_t>(woodBytes, woodBytes.size() - 4), 2u);
	WorkshopRecord workshop;
	workshop.bytes.values = {1, 2, 3};
	workshop.objects.numbers = {52};
	const auto workshopBytes = RoundTrip(workshop);
	ASSERT_EQ(workshopBytes.size(), 17u + 67 + 37 + 16 + 7 + 8);
	EXPECT_EQ(At<uint32_t>(workshopBytes, 137), 3u);
	EXPECT_EQ(workshopBytes[143], 3);
	EXPECT_EQ(At<uint32_t>(workshopBytes, 148), 52u);
	ScaffoldRecord scaffold;
	scaffold.objects = {Ref {53}, Ref {}, Ref {}};
	scaffold.half = 7;
	const auto scaffoldBytes = RoundTrip(scaffold);
	ASSERT_EQ(scaffoldBytes.size(), 17u + 68 + 38);
	EXPECT_EQ(At<uint32_t>(scaffoldBytes, 85), 53u);
	EXPECT_EQ(At<uint16_t>(scaffoldBytes, 113), 7u);
	ReactionRecord reaction;
	reaction.livings.numbers = {54, 55};
	reaction.lastWord = 9;
	const auto reactionBytes = RoundTrip(reaction);
	ASSERT_EQ(reactionBytes.size(), 53u + 8);
	EXPECT_EQ(At<uint32_t>(reactionBytes, 21), 2u);
	EXPECT_EQ(At<uint32_t>(reactionBytes, reactionBytes.size() - 4), 9u);
}
