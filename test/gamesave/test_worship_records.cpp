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

// The records of the worship site, its needs and totem, a town's artifact and the dances, on synthetic values: each
// record's length and the places of its values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-worship-sites-and-the-dances).

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

TEST(SaveWorshipRecords, AWorshipSiteAndItsTotemHaveTheGamesLengths)
{
	WorshipSiteRecord site;
	site.tribeInfo = 3;
	site.towns.numbers = {40};
	site.resourcesOnTheWay.values = {1, 2};
	site.overloadPulse = 0.5f;
	const auto bytes = RoundTrip(site);
	ASSERT_EQ(bytes.size(), 207u + 4 + 8);
	EXPECT_EQ(At<uint32_t>(bytes, 96), 3u);
	EXPECT_EQ(At<uint32_t>(bytes, 108), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 112), 40u);
	EXPECT_EQ(At<float>(bytes, bytes.size() - 4), 0.5f);
	const auto totem = RoundTrip(WorshipTotemRecord {.specularColour = 7, .site = {41}});
	ASSERT_EQ(totem.size(), 104u);
	EXPECT_EQ(At<uint32_t>(totem, 100), 41u);
}

TEST(SaveWorshipRecords, TheNeedsTheirVisualsAndATownsArtifactHaveTheGamesLengths)
{
	const auto needs = RoundTrip(ShowNeedsRecord {.object = {42}, .visuals = {.numbers = {43, 0, 0}}});
	ASSERT_EQ(needs.size(), 37u);
	EXPECT_EQ(At<uint32_t>(needs, 21), 3u);
	ASSERT_EQ(RoundTrip(ShowNeedsVisualsRecord {.other = {44}}).size(), 83u);
	const auto artifact = RoundTrip(TownArtifactRecord {.word = 5, .words = {6, 7}});
	ASSERT_EQ(artifact.size(), 49u);
	EXPECT_EQ(At<uint32_t>(artifact, 29), 5u);
	EXPECT_EQ(At<uint32_t>(artifact, 45), 7u);
}

TEST(SaveWorshipRecords, ADanceItsKeyFramesItsActionsAndItsGroupsHaveTheGamesLengths)
{
	DanceRecord dance;
	dance.group.keyFrames.numbers = {45};
	dance.footpathLink = {46};
	const auto bytes = RoundTrip(dance);
	ASSERT_EQ(bytes.size(), 192u + 4);
	EXPECT_EQ(At<uint32_t>(bytes, 47), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 4), 46u);
	ASSERT_EQ(RoundTrip(DanceKeyFrameRecord {.actions = {.numbers = {47}}}).size(), 33u);
	const auto action = RoundTrip(DanceKeyActionRecord {.words = {.values = {8, 9}}, .word = 1});
	ASSERT_EQ(action.size(), 89u);
	EXPECT_EQ(At<uint32_t>(action, 17), 2u);
	EXPECT_EQ(At<uint32_t>(action, 29), 1u);
	DanceGroupRecord group;
	group.text[0] = 'a';
	group.villagerType = 2;
	const auto groupBytes = RoundTrip(group);
	ASSERT_EQ(groupBytes.size(), 222u);
	EXPECT_EQ(groupBytes[138], 'a');
	EXPECT_EQ(At<uint32_t>(groupBytes, 218), 2u);
}
