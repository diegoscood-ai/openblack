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

// The records of a creature, a fire, a rock and a spell's seed, on synthetic values: each record's length and the
// places of its values are the game's, and each reads back as it was written
// (docs/wiki/engine/saved-games.md#the-creature).

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

TEST(SaveCreatureRecords, ACreatureWithNoListsOrPlanWritesItsOwn569Bytes)
{
	CreatureRecord creature;
	for (uint32_t i = 0; i < k_CreatureSpellCount; ++i)
	{
		creature.creature.spells.at(i).spell = i;
	}
	creature.creature.index = 0;
	creature.creature.owner = 1;
	creature.creature.size = 1.5f;
	const auto bytes = RoundTrip(creature);
	// The head, the position, the object, the moving thing's word, the walk round obstacles, the living thing
	constexpr size_t k_Own = 17 + 14 + 36 + 2 + 39 + 67;
	ASSERT_EQ(bytes.size(), k_Own + 569);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x69u);
	EXPECT_EQ(At<uint32_t>(bytes, k_Own), k_CreatureVersion);
	EXPECT_EQ(bytes[k_Own + 9], 1);
	EXPECT_EQ(At<uint32_t>(bytes, k_Own + 10 + 24), 1u);
	// The queued spells' count follows the sixteen slots
	EXPECT_EQ(At<uint32_t>(bytes, k_Own + 10 + 384), 0u);
	EXPECT_EQ(At<float>(bytes, k_Own + 569 - 32 - 4 - 20), 1.5f);
}

TEST(SaveCreatureRecords, ACreatureKeepsItsPlansStepsFromTheCurrentOne)
{
	CreatureRecord creature;
	creature.creature.keepsPlan = 1;
	creature.creature.plan.current = 1;
	creature.creature.plan.count = 3;
	creature.creature.plan.steps.resize(2);
	creature.creature.plan.steps[1].orderKind = 7;
	creature.creature.dancePartners.numbers = {40};
	const auto bytes = RoundTrip(creature);
	constexpr size_t k_Own = 17 + 14 + 36 + 2 + 39 + 67;
	ASSERT_EQ(bytes.size(), k_Own + 569 + 4 + 144);
	EXPECT_EQ(At<uint32_t>(bytes, bytes.size() - 32 - 8 - 4), 7u);
}

TEST(SaveCreatureRecords, AnOlderCreatureHasNoMindProcessingWord)
{
	CreatureRecord creature;
	creature.creature.version = k_LastCreatureVersionWithoutProcessing;
	EXPECT_EQ(RoundTrip(creature).size(), 17u + 14 + 36 + 2 + 39 + 67 + 565);
}

TEST(SaveCreatureRecords, AFireARockAndASpellSeedHaveTheGamesLengths)
{
	FireEffectRecord fire;
	fire.firstMark = 1;
	fire.objects[0] = {41};
	fire.burning.numbers = {42, 43};
	fire.lastSecondMark = 2;
	const auto fireBytes = RoundTrip(fire);
	ASSERT_EQ(fireBytes.size(), 17u + 4 + 3 + 8 + 16 + 1 + 8 + 4 + 8 + 2);
	EXPECT_EQ(fireBytes[21], 1);
	EXPECT_EQ(At<uint32_t>(fireBytes, 32), 41u);
	EXPECT_EQ(At<uint32_t>(fireBytes, 57), 2u);
	EXPECT_EQ(fireBytes[fireBytes.size() - 1], 2);
	// a fireball's ball, as the game's save writes it: an object's parts, its particle manager and its rain flag
	const auto ball = RoundTrip(MagicFireBallRecord {.manager = {61}, .affectedByRain = 0});
	ASSERT_EQ(ball.size(), 17u + 14 + 36 + 4 + 1);
	EXPECT_EQ(ball[0], 0x20u);
	const auto rock = RoundTrip(RockRecord {.words = {5, 6}, .object = {44}});
	ASSERT_EQ(rock.size(), 17u + 67 + 12 + 12);
	EXPECT_EQ(At<uint32_t>(rock, 104), 44u);
	SpellSeedRecord seed;
	seed.info = 9;
	seed.lastFlag = 1;
	const auto seedBytes = RoundTrip(seed);
	ASSERT_EQ(seedBytes.size(), 17u + 14 + 36 + 4 + 1 + 4 + 16 + 2 + 28 + 1 + 8 + 1);
	EXPECT_EQ(At<uint32_t>(seedBytes, 72), 9u);
	EXPECT_EQ(seedBytes[seedBytes.size() - 1], 1);
}

// The classes the reader reads as the game writes them: a plain spell icon (a building of several cells, its seed's row,
// a point and a word), an arena's spell icon (a fixed object, an object and three words), a teleport's stone (a mobile
// static and three objects) and a footpath finder (the thing's part and five objects)
TEST(SaveCreatureRecords, TheSpellIconsTheTeleportsStoneAndTheFootpathFinderHaveTheGamesLengths)
{
	EXPECT_TRUE(IsKnownType(0x81));
	EXPECT_TRUE(IsKnownType(0x42));
	SpellIconRecord icon;
	icon.icon.info = 2578;
	icon.icon.word = 3;
	const auto iconBytes = RoundTrip(icon);
	ASSERT_EQ(iconBytes.size(), 17u + 67 + 4 + 12 + 4);
	EXPECT_EQ(At<uint32_t>(iconBytes, 0), 0x81u);
	EXPECT_EQ(At<uint32_t>(iconBytes, 84), 2578u);
	EXPECT_EQ(At<uint32_t>(iconBytes, 100), 3u);
	const auto arena = RoundTrip(ArenaSpellIconRecord {.object = {45}, .words = {1, 2, 3}});
	ASSERT_EQ(arena.size(), 17u + 54 + 4 + 12);
	EXPECT_EQ(At<uint32_t>(arena, 0), 0x42u);
	EXPECT_EQ(At<uint32_t>(arena, 71), 45u);
	EXPECT_EQ(At<uint32_t>(arena, 83), 3u);
	const auto stone = RoundTrip(MagicTeleportRecord {.reaction = {46}, .spell = {47}, .player = {48}});
	ASSERT_EQ(stone.size(), 17u + 67 + 12 + 12);
	EXPECT_EQ(At<uint32_t>(stone, 0), 0x19u);
	EXPECT_EQ(At<uint32_t>(stone, 96), 46u);
	EXPECT_EQ(At<uint32_t>(stone, 104), 48u);
	const auto finder =
	    RoundTrip(FootpathFinderRecord {.from = {49}, .to = {50}, .footpath = {51}, .startNode = {52}, .destNode = {53}});
	ASSERT_EQ(finder.size(), 17u + 20);
	EXPECT_EQ(At<uint32_t>(finder, 0), 0x5u);
	EXPECT_EQ(At<uint32_t>(finder, 17), 49u);
	EXPECT_EQ(At<uint32_t>(finder, 33), 53u);
}
