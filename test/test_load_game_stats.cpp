/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>

#include <SavedRecords.h>
#include <gtest/gtest.h>

#include "ECS/Components/PlayerMagic.h"
#include "ECS/SavedWorld/LoadGameStats.h"
#include "Game/GameStats.h"

using namespace openblack;

namespace
{
/// A record whose every word is told apart from the others
gamesave::GameStatsRecord NumberedRecord()
{
	gamesave::GameStatsRecord record;
	record.playersThatLeft = 1;
	record.position = 2;
	record.lossReason = 3;
	for (uint32_t i = 0; i < record.counts.size(); ++i)
	{
		record.counts.at(i) = 100 + i;
	}
	record.counts[9] = std::bit_cast<uint32_t>(12.5f);
	record.influence.data[0] = 0.25f;
	record.influence.size = 400;
	record.influence.index = 7;
	record.influence.samplesPerSlot = 100;
	record.influence.samplesInSlot = 9;
	record.population.index = 11;
	for (uint32_t i = 0; i < record.results.size(); ++i)
	{
		record.results.at(i) = 200 + i;
	}
	record.results[1] = std::bit_cast<uint32_t>(0.75f);
	record.results[3] = std::bit_cast<uint32_t>(3000.0f);
	record.results[4] = std::bit_cast<uint32_t>(10.0f);
	record.results[5] = std::bit_cast<uint32_t>(1.5f);
	record.results[6] = std::bit_cast<uint32_t>(0.5f);
	record.run = {5000, std::bit_cast<uint32_t>(60.0f), std::bit_cast<uint32_t>(0.0f), 123456, 1731600000, 113};
	for (uint32_t i = 0; i < record.casts.size(); ++i)
	{
		record.casts.at(i) = 300 + i;
	}
	record.last = {std::bit_cast<uint32_t>(4.0f), 6, 3};
	return record;
}
} // namespace

// A player's statistics come back from its record in the game's order: the counts, both histories where they stood,
// the results, the whole run's figures, and the spells cast and chants used into the player's magic
TEST(LoadGameStats, APlayersRecordPutsBackEachFigureInTheGamesOrder)
{
	const auto record = NumberedRecord();
	game_stats::Stats stats;
	game_stats::Statics statics;
	ecs::components::PlayerMagic magic;
	ecs::load_game_stats::PutBack(record, stats, statics, magic);

	EXPECT_EQ(statics.playersOut, 1u);
	EXPECT_EQ(stats.finishPlace, 2u);
	EXPECT_EQ(stats.finishValue, 3u);
	EXPECT_EQ(stats.births, 100u);
	EXPECT_EQ(stats.deaths, 101u);
	EXPECT_EQ(stats.minFemales, 107u);
	EXPECT_EQ(stats.townTakenOverPopulation, 108u);
	EXPECT_EQ(stats.desireSum, 12.5f);
	EXPECT_EQ(stats.desireCount, 110u);
	EXPECT_EQ(stats.buildingsBuilt, 111u);
	EXPECT_EQ(stats.buildingsBuiltByKind.at(2), 114u);
	EXPECT_EQ(stats.disciples.at(0), 115u);
	EXPECT_EQ(stats.disciples.at(7), 122u);
	EXPECT_EQ(stats.foodEaten, 123u);
	EXPECT_EQ(stats.woodUsed, 124u);

	EXPECT_EQ(stats.influence.buckets.at(0), 0.25f);
	EXPECT_EQ(stats.influence.capacity, 400u);
	EXPECT_EQ(stats.influence.count, 7u);
	EXPECT_EQ(stats.influence.period, 100);
	EXPECT_EQ(stats.influence.samples, 9);
	EXPECT_EQ(stats.population.count, 11u);

	EXPECT_EQ(stats.godsDefeated, 200u);
	EXPECT_EQ(stats.alignmentAtStart, 0.75f);
	EXPECT_EQ(stats.killedVillagers, 202u);
	EXPECT_EQ(stats.maxWorldBelief, 3000.0f);
	EXPECT_EQ(stats.minWorldBelief, 10.0f);
	EXPECT_EQ(stats.creatureValue, 1.5f);
	EXPECT_EQ(stats.creatureAtStart, 0.5f);
	EXPECT_EQ(stats.buildingStopped, 207u);
	EXPECT_EQ(stats.u1084.at(2), 210u);

	EXPECT_EQ(statics.linesOfCode, 5000u);
	EXPECT_EQ(statics.maxFrameRate, 60.0f);
	EXPECT_EQ(statics.minFrameRate, 0.0f);
	EXPECT_EQ(statics.startTime, 1731600000);
	EXPECT_EQ(statics.version, 113u);

	EXPECT_EQ(magic.castCount.at(0), 0u);
	EXPECT_EQ(magic.castCount.at(1), 300u);
	EXPECT_EQ(magic.castCount.at(35), 334u);
	EXPECT_EQ(magic.castCount.at(41), 335u);
	EXPECT_EQ(magic.chantsUsed, 4.0f);
	EXPECT_EQ(stats.sacrifices, 6u);
	EXPECT_EQ(statics.townCount, 3u);
}

// The objects created are put back as the original's load reads them: the record's count stands where the registry's
// entities made stand at the load, and counts on with them; a land's start makes it the registry's own again
TEST(LoadGameStats, TheObjectsCreatedGoOnFromTheRecordsCount)
{
	auto record = NumberedRecord();
	record.run[3] = 5000;
	game_stats::Stats stats;
	auto& statics = game_stats::GetStatics();
	const auto keptStatics = statics;
	ecs::components::PlayerMagic magic;
	ecs::load_game_stats::PutBack(record, stats, statics, magic, 1200);
	EXPECT_EQ(game_stats::ObjectsCreated(statics, 1200), 5000u);
	EXPECT_EQ(game_stats::ObjectsCreated(statics, 1210), 5010u);
	statics = keptStatics;
	EXPECT_EQ(game_stats::ObjectsCreated(game_stats::Statics {}, 1210), 1210u);
}

// The town totals are put back as the save kept them, the turn they were summed in first, and a save writes them again
// until the figures sum the towns once more; a record never summed (turn 0) puts none back
TEST(LoadGameStats, TheTownTotalsArePutBackAndWrittenAgain)
{
	auto record = NumberedRecord();
	record.townStats = {120, 40, 22, 18, 50, 3, 30, 12, 6, 1};
	game_stats::Stats stats;
	auto& statics = game_stats::GetStatics();
	const auto keptStatics = statics;
	ecs::components::PlayerMagic magic;
	ecs::load_game_stats::PutBack(record, stats, statics, magic);
	ASSERT_TRUE(stats.loadedTownTotals.has_value());
	EXPECT_EQ(*stats.loadedTownTotals, record.townStats);
	const auto bytes = game_stats::Serialize(stats, magic);
	statics = keptStatics;
	// after the player's three words (players out, place, loss reason)
	std::array<uint32_t, 10> written {};
	std::memcpy(written.data(), bytes.data() + 12, sizeof(written));
	EXPECT_EQ(written, record.townStats);

	auto never = NumberedRecord();
	never.townStats = {};
	game_stats::Stats none;
	ecs::load_game_stats::PutBack(never, none, statics, magic);
	statics = keptStatics;
	EXPECT_FALSE(none.loadedTownTotals.has_value());
}

// Put back and written again, a player's figures are the record's: what the save writes from the statistics is what
// the load read
TEST(LoadGameStats, PutBackFiguresSerializeAsTheRecordHeldThem)
{
	auto record = NumberedRecord();
	// no town totals (never summed) and no objects created: they are written as openblack has them, zeros here
	record.townStats = {};
	record.run[3] = 0;
	game_stats::Stats stats;
	auto& statics = game_stats::GetStatics();
	const auto keptStatics = statics;
	ecs::components::PlayerMagic magic;
	ecs::load_game_stats::PutBack(record, stats, statics, magic);
	const auto bytes = game_stats::Serialize(stats, magic);
	statics = keptStatics;

	// the bytes after the player in the record's order, the objects created (written from the registry, none here) as 0
	std::vector<uint8_t> expected;
	const auto append = [&expected](const auto& value) {
		const auto* begin = reinterpret_cast<const uint8_t*>(&value);
		expected.insert(expected.end(), begin, begin + sizeof(value));
	};
	append(record.playersThatLeft);
	append(record.position);
	append(record.lossReason);
	append(record.townStats);
	append(record.counts);
	for (const auto* graph : {&record.influence, &record.population})
	{
		append(graph->data);
		append(graph->size);
		append(graph->index);
		append(graph->samplesPerSlot);
		append(graph->samplesInSlot);
	}
	append(record.results);
	append(record.run);
	append(record.casts);
	append(record.last);
	EXPECT_EQ(bytes, expected);
}
