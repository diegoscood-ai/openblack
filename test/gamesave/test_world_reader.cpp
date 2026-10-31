/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <SaveSlots.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

// The walk of the world file from the game object, on a synthetic file made with the records' writers, and, as an
// integration test, on the game's own saves (OPENBLACK_TEST_GAME_PATH; skipped without them). See
// docs/wiki/engine/saved-games.md#the-classes-a-save-meets.

using namespace openblack::gamesave;

namespace
{
/// A number that names no class of the game, so the reader never knows it
constexpr uint32_t k_UnknownType = 0xFFF0;

/// A file as the game writes one: the game object, its first player, the player's computer player with its action
/// state and its eight attitudes, the player's statistics, then an object of a class the reader does not know
struct SyntheticWorld
{
	std::vector<uint8_t> bytes;
	/// Where the unknown object's record starts, and where the first attitude's does
	size_t citadelAt {0};
	size_t firstAttitudeAt {0};
};

SyntheticWorld MakeWorld()
{
	std::map<ObjectNumber, SavedRecord> records;
	GameHead game;
	game.gameFlags = 0x00010024;
	game.landNumber = 1;
	game.players = {2, 0, 0, 0, 0, 0, 0, 0};
	PlayerRecord player;
	player.computerPlayer = 3;
	player.gameStats = 13;
	player.citadel = 14;
	player.tribalPower.assign(9, 1.0f);
	player.maxTribalPower.assign(9, 1.0f);
	player.name.assign(32, u'\0');
	player.magicTypeEnabled.assign(42, 0);
	player.magicTypeEverBeenEnabled.assign(42, 0);
	records[2] = player;
	ComputerPlayerRecord computer;
	computer.player = 2;
	computer.actionState = 4;
	computer.table.assign(42, 0);
	for (ObjectNumber number = 5; number <= 12; ++number)
	{
		computer.attitudes.push_back(number);
		records[number] = ComputerAttitudeRecord {.thing = {.extra = number - 5}, .attitude = 1.0f};
	}
	records[3] = computer;
	records[4] = ActionStateRecord {.computerPlayer = 3};
	records[13] = GameStatsRecord {.player = 2};

	SyntheticWorld world;
	world.bytes = WriteVanillaFraming(".\\DATA\\LANDSCAPE\\Land1.lnd");
	const auto framing = ReadVanillaFraming(world.bytes);
	StreamWriter writer({.objects = 1, .checksum = framing->checksum});
	WriteNewObject writeNew;
	writeNew = [&](StreamWriter& stream, ObjectNumber number) {
		const auto at = world.bytes.size() + stream.Bytes().size();
		if (number == 5)
		{
			world.firstAttitudeAt = at;
		}
		if (const auto found = records.find(number); found != records.end())
		{
			Write(stream, found->second, writeNew);
			return;
		}
		// The unknown object: the part every object writes, and values the reader cannot know
		world.citadelAt = at;
		WriteThing(stream, k_UnknownType, {});
		stream.Put(uint32_t {77});
	};
	Write(writer, game, writeNew);
	const auto body = writer.Take();
	world.bytes.insert(world.bytes.end(), body.begin(), body.end());
	return world;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The world files of every profile's saved games, or none without the game's data
std::vector<std::filesystem::path> WorldFiles()
{
	std::vector<std::filesystem::path> files;
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (game == nullptr || !std::filesystem::is_directory(std::filesystem::path(game) / "Profiles", error))
	{
		return files;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(game) / "Profiles"))
	{
		const auto name = entry.path().filename().string();
		if (entry.is_regular_file() && entry.path().extension() == ".sav" && !name.starts_with("Script_") &&
		    entry.path().string().find(std::string(k_SavedGamesFolder)) != std::string::npos)
		{
			files.push_back(entry.path());
		}
	}
	return files;
}
} // namespace

TEST(SaveWorldReader, ReadsFromTheGameObjectDepthFirstAndStopsAtAClassItDoesNotKnow)
{
	const auto world = MakeWorld();
	const auto read = ReadWorld(world.bytes);
	EXPECT_EQ(read.framing.landscape, ".\\DATA\\LANDSCAPE\\Land1.lnd");
	ASSERT_EQ(read.objects.size(), 13u);
	EXPECT_TRUE(std::ranges::all_of(read.objects, [](const auto& record) { return record.has_value(); }));
	EXPECT_EQ(std::get<GameHead>(*read.objects[0]).landNumber, 1u);
	EXPECT_EQ(std::get<ComputerAttitudeRecord>(*read.objects[11]).thing.extra, 7u);
	EXPECT_EQ(read.positions[4], world.firstAttitudeAt);
	// Each record's read ends after those read inside it: the computer player's after its action state and
	// attitudes, the player's (stopped at the unknown object) after its statistics, and the game object's last
	const std::vector<ObjectNumber> loadOrder {4, 5, 6, 7, 8, 9, 10, 11, 12, 3, 13, 2, 1};
	EXPECT_EQ(read.loadOrder, loadOrder);
	// Every record's stored sum is the reader's own, the unknown object's too
	EXPECT_EQ(read.sumsRead, 14u);
	EXPECT_FALSE(read.firstWrongSum.has_value());
	// The unknown object's number was read; its record is not
	EXPECT_EQ(read.unreadType, k_UnknownType);
	EXPECT_EQ(read.unreadNumber, 14u);
	EXPECT_EQ(read.stoppedAt, world.citadelAt);
	const auto census = read.Census();
	EXPECT_EQ(census.at(k_ComputerAttitudeType), 8u);
	EXPECT_EQ(census.at(k_GameStatsType), 1u);
	// What was read writes back to the file's bytes up to the stop
	const auto written = WriteWorld(read);
	ASSERT_EQ(written.size(), world.citadelAt);
	EXPECT_TRUE(std::equal(written.begin(), written.end(), world.bytes.begin()));
}

TEST(SaveWorldReader, AWrongLayoutShowsAtTheNextStoredSum)
{
	auto world = MakeWorld();
	// One value of the computer player's table changes: its record reads the same way, but every sum after it moves
	const auto tableAt = world.firstAttitudeAt - 4 - 4 - 1 - 12 - 4 - (42 * 4);
	world.bytes[tableAt] = 0x01;
	const auto read = ReadWorld(world.bytes);
	ASSERT_TRUE(read.firstWrongSum.has_value());
	EXPECT_EQ(*read.firstWrongSum, world.firstAttitudeAt + 8);
	// The game does not check the sums, and neither does the read
	EXPECT_EQ(read.unreadType, k_UnknownType);
}

TEST(SaveWorldReader, AFileThatDoesNotStartAsAWorldFileReadsNothing)
{
	EXPECT_TRUE(ReadWorld(std::vector<uint8_t>(16, 0)).objects.empty());
	auto world = MakeWorld();
	world.bytes.resize(world.firstAttitudeAt + 10);
	const auto cut = ReadWorld(world.bytes);
	EXPECT_FALSE(cut.unreadType.has_value());
	EXPECT_FALSE(cut.stop.empty());
}

TEST(SavedGamesData, EveryWorldFileReadsFromTheGameObjectWithEverySumEqual)
{
	const auto files = WorldFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no saved games";
	}
	for (const auto& path : files)
	{
		const auto bytes = ReadAll(path);
		const auto read = ReadWorld(bytes);
		ASSERT_FALSE(read.objects.empty()) << path;
		EXPECT_FALSE(read.firstWrongSum.has_value()) << path << " at byte " << read.firstWrongSum.value_or(0);
		// The read goes through the game object, the first player, its computer player and its statistics, the
		// player's citadel or its first town, and on, and stops at a class it does not know yet, whose sum it checks, or
		// reads every object of the world file to its last byte
		if (!read.unreadType.has_value())
		{
			EXPECT_EQ(read.stop, k_ReadToTheEnd) << path;
		}
		const auto census = read.Census();
		EXPECT_GE(census.size(), 7u) << path;
		// Each computer player read writes its eight attitudes
		EXPECT_EQ((census.count(k_ComputerAttitudeType) == 1 ? census.at(k_ComputerAttitudeType) : 0u) % 8, 0u) << path;
		EXPECT_GE(census.at(k_ComputerAttitudeType), 8u) << path;
		EXPECT_TRUE(census.contains(CitadelRecord::k_Type) || census.contains(TownRecord::k_Type)) << path;
		EXPECT_GE(read.sumsRead, 15u) << path;
		const auto& game = std::get<GameHead>(*read.objects[0]);
		EXPECT_EQ(game.revision, k_GameRevision) << path;
		// The options' autosave box is on in every one of these saves
		EXPECT_NE(game.gameFlags & 0x10000u, 0u) << path;
		const auto& player = std::get<PlayerRecord>(*read.objects[1]);
		EXPECT_EQ(player.playerNumber, 0u) << path;
		EXPECT_EQ(player.magicTypeEnabled.size(), 42u) << path;
		const auto& computer = std::get<ComputerPlayerRecord>(*read.objects[2]);
		EXPECT_EQ(computer.player, 2u) << path;
		EXPECT_EQ(computer.actionState, 4u) << path;
		// The records read write back to the file's own bytes, up to where the read stopped
		const auto written = WriteWorld(read);
		ASSERT_EQ(written.size(), read.stoppedAt) << path;
		EXPECT_TRUE(std::equal(written.begin(), written.end(), bytes.begin())) << path;
	}
}
