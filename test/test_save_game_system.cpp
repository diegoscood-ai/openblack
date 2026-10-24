/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The save service: the slots' folders and files of the current profile, the safe write of a save's files through
// temporary files, and the world file's counter of the loads that did not end (docs/wiki/engine/saved-games.md)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <filesystem>
#include <optional>
#include <vector>

#include <SaveGameInfo.h>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "support/FileSystemFakes.h"

using namespace openblack;
using openblack::ecs::systems::SaveFile;
using openblack::ecs::systems::SaveGameSystem;
using openblack::ecs::systems::SaveWrite;

namespace
{
const std::filesystem::path k_Player = std::filesystem::path("Profiles") / "Player";

std::vector<uint8_t> WorldBytes(uint32_t attempts)
{
	std::vector<uint8_t> bytes(16, 0xAB);
	std::memcpy(bytes.data(), &attempts, sizeof(attempts));
	return bytes;
}

uint32_t Attempts(const std::vector<uint8_t>& bytes)
{
	uint32_t attempts = 0;
	std::memcpy(&attempts, bytes.data(), sizeof(attempts));
	return attempts;
}

SaveGameSystem Kept(test::MemoryFileSystem& files)
{
	return {&files, []() -> std::optional<std::filesystem::path> { return k_Player; }};
}
} // namespace

TEST(SaveGameSystem, WithNoProfileKeptThereAreNoSlotsAndNothingIsWritten)
{
	test::MemoryFileSystem files;
	SaveGameSystem saves(&files, []() -> std::optional<std::filesystem::path> { return std::nullopt; });
	EXPECT_FALSE(saves.SlotFolder(3, 1).has_value());
	EXPECT_FALSE(saves.WorldFile(20, 1).has_value());
	EXPECT_FALSE(saves.IsAutoSaveValid());
	EXPECT_FALSE(saves.Summary(3, 1).hasWorld);
	EXPECT_EQ(files.writes, 0);
}

TEST(SaveGameSystem, TheSlotsFoldersAreThePlayersAndTheirLandsForTheAutosaves)
{
	test::MemoryFileSystem files;
	const auto saves = Kept(files);
	EXPECT_EQ(saves.SlotFolder(3, 2), k_Player / "Saved Games" / "3");
	EXPECT_EQ(saves.SlotFolder(16, 2), k_Player / "Saved Games" / "16" / "2");
	EXPECT_EQ(saves.WorldFile(20, 2), k_Player / "Saved Games" / "20" / "20.sav");
	// A load of the previous land reads the folder of the land before
	EXPECT_EQ(saves.WorldFile(14, 2), k_Player / "Saved Games" / "14" / "1" / "14.sav");
	EXPECT_FALSE(saves.SlotFolder(21, 1).has_value());
}

TEST(SaveGameSystem, ASlotsSummaryHasItsInfoItsWorldAndItsLoadAttempts)
{
	test::MemoryFileSystem files;
	const auto saves = Kept(files);
	const auto folder = *saves.SlotFolder(5, 1);
	files.files[folder / "SaveGame.inf"] = gamesave::Write({.slot = 5, .date = u"12:00:00 1", .name = u"Mine"});
	files.files[folder / "5.sav"] = WorldBytes(1);
	const auto summary = saves.Summary(5, 1);
	ASSERT_TRUE(summary.info.has_value());
	EXPECT_EQ(summary.info->name, u"Mine");
	EXPECT_TRUE(summary.hasWorld);
	EXPECT_EQ(summary.loadAttempts, 1u);
	EXPECT_TRUE(summary.loadable);
	files.files[folder / "5.sav"] = WorldBytes(2);
	EXPECT_FALSE(saves.Summary(5, 1).loadable);
	EXPECT_FALSE(saves.Summary(6, 1).hasWorld);
}

TEST(SaveGameSystem, TheAutosaveIsValidWhenItsWorldFileIsThereWhateverItsCounter)
{
	test::MemoryFileSystem files;
	const auto saves = Kept(files);
	EXPECT_FALSE(saves.IsAutoSaveValid());
	files.files[*saves.WorldFile(20, 1)] = WorldBytes(2);
	EXPECT_TRUE(saves.IsAutoSaveValid());
}

TEST(SaveGameSystem, ASavesFilesGoThroughTemporaryFilesThenTakeTheirNames)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(3, 1);
	const auto script = world.parent_path() / "Script_3.sav";
	files.files[world] = {1, 2, 3};
	const std::vector<SaveFile> save {{.path = world, .bytes = {4, 5}}, {.path = script, .bytes = {6}}};
	EXPECT_EQ(saves.WriteFiles(save), SaveWrite::Written);
	EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {4, 5}));
	EXPECT_EQ(files.files.at(script), (std::vector<uint8_t> {6}));
	EXPECT_FALSE(files.files.contains(SaveGameSystem::TemporaryPath(world)));
	EXPECT_FALSE(files.files.contains(SaveGameSystem::TemporaryPath(script)));
}

TEST(SaveGameSystem, AFileThatFailsKeepsEveryOldFile)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(3, 1);
	const auto script = world.parent_path() / "Script_3.sav";
	files.files[world] = {1, 2, 3};
	files.incomplete.insert(SaveGameSystem::TemporaryPath(script));
	const std::vector<SaveFile> save {{.path = world, .bytes = {4, 5}}, {.path = script, .bytes = {6, 7}}};
	EXPECT_EQ(saves.WriteFiles(save), SaveWrite::Failed);
	EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {1, 2, 3}));
	// The first file not opening writes nothing at all
	files.unwritable.insert(SaveGameSystem::TemporaryPath(world));
	EXPECT_EQ(saves.WriteFiles(save), SaveWrite::NotOpened);
	EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {1, 2, 3}));
}

TEST(SaveGameSystem, ALoadCountsItsAttemptAndTheEndOfTheLoadSetsItBack)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(20, 1);
	EXPECT_FALSE(saves.BeginLoad(world));
	files.files[world] = WorldBytes(0);
	ASSERT_TRUE(saves.BeginLoad(world));
	EXPECT_EQ(Attempts(files.files.at(world)), 1u);
	// The rest of the file stays as it was
	EXPECT_EQ(files.files.at(world)[4], 0xAB);
	ASSERT_TRUE(saves.BeginLoad(world));
	EXPECT_EQ(Attempts(files.files.at(world)), 2u);
	// Two loads that did not end: the save is refused, and its counter stays
	EXPECT_FALSE(saves.BeginLoad(world));
	EXPECT_EQ(Attempts(files.files.at(world)), 2u);
	saves.EndLoad(world);
	EXPECT_EQ(Attempts(files.files.at(world)), 0u);
	EXPECT_TRUE(saves.BeginLoad(world));
}

TEST(SaveGameSystem, AWorldFileTooShortForItsCounterIsRefused)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(20, 1);
	files.files[world] = {1, 0};
	EXPECT_FALSE(saves.BeginLoad(world));
	EXPECT_EQ(saves.Summary(20, 1).loadAttempts, SaveGameSystem::k_UnreadLoadAttempts);
}
