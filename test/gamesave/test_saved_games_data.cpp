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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ObjectStream.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <VanillaSaveFile.h>
#include <gtest/gtest.h>

// Integration tests: the saved games of the game's own profiles, read only. They need the original game's data
// (OPENBLACK_TEST_GAME_PATH, with its Profiles folder) and skip themselves without it. The synthetic twins are in
// test_save_stream.cpp and test_save_slots.cpp (docs/wiki/engine/saved-games.md#checked-on-the-real-saves).

using namespace openblack::gamesave;

namespace
{
/// The game object's type, the first object of every world file
constexpr uint32_t k_GameType = 0x6A;

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// A file of a slot, with the slot and the land of the folders it sits in
struct SlotFile
{
	std::filesystem::path path;
	/// The file's place under the profile's folder
	std::filesystem::path relative;
	uint32_t slot {0};
	std::optional<uint32_t> land;
};

std::optional<uint32_t> Number(const std::filesystem::path& part)
{
	const auto text = part.string();
	if (text.empty() || !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }))
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::stoul(text));
}

/// Every file in the slots' folders of every profile, or none without the game's data
std::vector<SlotFile> SlotFiles()
{
	std::vector<SlotFile> files;
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return files;
	}
	const auto profiles = std::filesystem::path(game) / "Profiles";
	std::error_code error;
	if (!std::filesystem::is_directory(profiles, error))
	{
		return files;
	}
	for (const auto& profile : std::filesystem::directory_iterator(profiles))
	{
		const auto saves = profile.path() / k_SavedGamesFolder;
		if (!profile.is_directory() || !std::filesystem::is_directory(saves, error))
		{
			continue;
		}
		for (const auto& entry : std::filesystem::recursive_directory_iterator(saves))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}
			const auto inSaves = std::filesystem::relative(entry.path(), saves);
			const auto first = Number(*inSaves.begin());
			if (!first.has_value())
			{
				continue; // the pictures' folder
			}
			SlotFile file {
			    .path = entry.path(), .relative = std::filesystem::relative(entry.path(), profile.path()), .slot = *first};
			const auto parts = std::distance(inSaves.begin(), inSaves.end());
			if (parts == 3)
			{
				file.land = Number(*std::next(inSaves.begin()));
			}
			files.push_back(std::move(file));
		}
	}
	return files;
}

bool IsWorldFile(const SlotFile& file)
{
	const auto name = file.path.filename().string();
	return file.path.extension() == ".sav" && !name.starts_with("Script_");
}
} // namespace

TEST(SavedGamesData, EveryInfoFileReadsAndWritesBackUpToTheEndOfItsTexts)
{
	const auto files = SlotFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no saved games";
	}
	size_t read = 0;
	for (const auto& file : files)
	{
		if (file.path.filename() != k_InfoFileName)
		{
			continue;
		}
		const auto bytes = ReadAll(file.path);
		ASSERT_EQ(bytes.size(), k_SaveGameInfoSize) << file.relative;
		const auto info = ReadSaveGameInfo(bytes);
		ASSERT_TRUE(info.has_value()) << file.relative;
		EXPECT_EQ(info->slot, file.slot) << file.relative;
		EXPECT_FALSE(info->date.empty()) << file.relative;
		EXPECT_EQ(file.relative.parent_path(), SlotFolder(file.slot, file.land.value_or(0))) << file.relative;
		// The game leaves its stack's bytes after each text's zero, so the written file equals the game's up to there
		const auto written = Write(*info);
		const auto equalTo = [&](size_t from, size_t count) {
			return std::equal(written.begin() + static_cast<std::ptrdiff_t>(from),
			                  written.begin() + static_cast<std::ptrdiff_t>(from + count),
			                  bytes.begin() + static_cast<std::ptrdiff_t>(from));
		};
		EXPECT_TRUE(equalTo(0, 4)) << file.relative;
		EXPECT_TRUE(equalTo(4, (info->date.size() + 1) * 2)) << file.relative;
		EXPECT_TRUE(equalTo(0x84, (info->name.size() + 1) * 2)) << file.relative;
		++read;
	}
	EXPECT_GT(read, 0u);
}

TEST(SavedGamesData, EveryWorldFileStartsAsTheGameWritesItWithTheGameObjectsSum)
{
	const auto files = SlotFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no saved games";
	}
	size_t read = 0;
	for (const auto& file : files)
	{
		if (!IsWorldFile(file))
		{
			continue;
		}
		EXPECT_EQ(file.path.filename().string(), VanillaWorldFileName(file.slot)) << file.relative;
		EXPECT_EQ(file.relative.parent_path(), SlotFolder(file.slot, file.land.value_or(0))) << file.relative;
		const auto bytes = ReadAll(file.path);
		const auto framing = ReadVanillaFraming(bytes);
		ASSERT_TRUE(framing.has_value()) << file.relative;
		EXPECT_EQ(framing->attempts, 0u) << file.relative;
		EXPECT_EQ(framing->revision, k_VanillaRevision) << file.relative;
		// The start written again is the file's own
		const auto start = WriteVanillaFraming(framing->landscape);
		ASSERT_EQ(start.size(), framing->body) << file.relative;
		EXPECT_TRUE(std::equal(start.begin(), start.end(), bytes.begin())) << file.relative;

		// The game object follows with no reference; its record keeps the running sum of the file before it
		StreamReader reader(bytes, {.objects = 1, .checksum = framing->checksum}, framing->body);
		uint32_t type = 0;
		uint32_t extra = 0;
		reader.Get(type);
		reader.Get(extra);
		const auto expected = reader.Checksum();
		uint32_t stored = 0;
		reader.Get(stored);
		ASSERT_FALSE(reader.Failed()) << file.relative;
		EXPECT_EQ(type, k_GameType) << file.relative;
		EXPECT_EQ(extra, 0u) << file.relative;
		EXPECT_EQ(stored, expected) << file.relative;
		++read;
	}
	EXPECT_GT(read, 0u);
}

TEST(SavedGamesData, EveryMindFileSitsInItsSlotsMindFolder)
{
	const auto files = SlotFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no saved games";
	}
	size_t minds = 0;
	for (const auto& file : files)
	{
		if (!file.path.filename().string().starts_with("SavedCreature"))
		{
			continue;
		}
		// The minds of the previous and the next land's slots sit in the slot's folder; only the autosaves' have a land
		EXPECT_EQ(file.relative.parent_path(), MindFolder(file.slot, file.land.value_or(0))) << file.relative;
		EXPECT_EQ(file.land.has_value(), HasLandFolderForMinds(file.slot)) << file.relative;
		++minds;
	}
	if (minds == 0)
	{
		GTEST_SKIP() << "The saved games have no creature";
	}
}
