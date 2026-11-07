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
#include <stdexcept>
#include <string>
#include <vector>

#include <SaveGameInfo.h>
#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedSlot.h"
#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "support/FileSystemFakes.h"

using namespace openblack;
using openblack::ecs::systems::SaveContents;
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
	// and leaves its temporary files, as the game does
	EXPECT_TRUE(files.files.contains(SaveGameSystem::TemporaryPath(world)));
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

// The game opens the world file for writing before it reads it: a file it cannot write (a read-only one) is not loaded
TEST(SaveGameSystem, AWorldFileWhoseCounterCannotBeWrittenIsRefused)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(20, 1);
	files.files[world] = WorldBytes(0);
	files.unwritable.insert(world);
	EXPECT_FALSE(saves.BeginLoad(world));
	EXPECT_EQ(Attempts(files.files.at(world)), 0u);
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

TEST(SaveGameSystem, ASaveWritesTheWorldThenTheMindsThenTheScriptsThenGivesTheirNames)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto folder = k_Player / "Saved Games" / "16" / "2";
	const auto world = folder / "16.sav";
	const auto script = folder / "Script_16.sav";
	files.files[world] = {1, 2, 3};
	bool scriptWritten = false;
	const SaveContents contents {
	    .world = WorldBytes(0),
	    .minds = {{0, {7}}, {1, {8, 9}}},
	    .scriptState =
	        [&](const std::filesystem::path& file) {
		        // the world file is in its temporary file and the minds are written, the old world file still there
		        EXPECT_EQ(file, SaveGameSystem::TemporaryPath(script));
		        EXPECT_TRUE(files.files.contains(SaveGameSystem::TemporaryPath(world)));
		        EXPECT_TRUE(files.files.contains(folder / "SavedCreature1"));
		        EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {1, 2, 3}));
		        files.files[file] = {5};
		        scriptWritten = true;
		        return true;
	        },
	};
	EXPECT_EQ(saves.Save(16, 2, contents), SaveWrite::Written);
	EXPECT_TRUE(scriptWritten);
	EXPECT_TRUE(files.folders.contains(folder));
	EXPECT_EQ(files.files.at(world), WorldBytes(0));
	EXPECT_EQ(files.files.at(script), (std::vector<uint8_t> {5}));
	EXPECT_EQ(files.files.at(folder / "SavedCreature0"), (std::vector<uint8_t> {7}));
	EXPECT_FALSE(files.files.contains(SaveGameSystem::TemporaryPath(world)));
	EXPECT_FALSE(files.files.contains(SaveGameSystem::TemporaryPath(script)));
}

TEST(SaveGameSystem, ASaveKeepsTheHelpRecordAfterTheMindsAndBeforeTheScripts)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto folder = k_Player / "Saved Games" / "16" / "2";
	std::vector<std::string> order;
	const SaveContents contents {
	    .world = WorldBytes(0),
	    .minds = {{0, {7}}},
	    .helpRecord =
	        [&] {
		        EXPECT_TRUE(files.files.contains(folder / "SavedCreature0"));
		        order.emplace_back("help");
	        },
	    .scriptState =
	        [&](const std::filesystem::path& file) {
		        files.files[file] = {5};
		        order.emplace_back("scripts");
		        return true;
	        },
	};
	EXPECT_EQ(saves.Save(16, 2, contents), SaveWrite::Written);
	EXPECT_EQ(order, (std::vector<std::string> {"help", "scripts"}));
}

TEST(SaveGameSystem, ASaveWhoseScriptsFailKeepsTheOldFilesAndOneWhoseWorldDoesNotOpenWritesNothing)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(3, 1);
	files.files[world] = {1, 2, 3};
	SaveContents contents {
	    .world = WorldBytes(0), .minds = {{0, {7}}}, .scriptState = [](const auto& /*file*/) { return false; }};
	EXPECT_EQ(saves.Save(3, 1, contents), SaveWrite::Failed);
	EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {1, 2, 3}));

	files.files.clear();
	files.files[world] = {1, 2, 3};
	files.unwritable.insert(SaveGameSystem::TemporaryPath(world));
	bool asked = false;
	contents.scriptState = [&asked](const auto& /*file*/) {
		asked = true;
		return true;
	};
	EXPECT_EQ(saves.Save(3, 1, contents), SaveWrite::NotOpened);
	EXPECT_FALSE(asked);
	EXPECT_FALSE(files.files.contains(world.parent_path() / "SavedCreature0"));
	EXPECT_EQ(files.files.at(world), (std::vector<uint8_t> {1, 2, 3}));
}

// A failed write as the game's: a save whose script file fails, or which throws on the way, keeps the old world files
// byte for byte and leaves its temporary files as far as they were written. A save that did not fail removes each old
// file and then renames its temporary file, looking at neither result, so a rename that fails loses the old file and
// leaves the save in its temporary file
TEST(SaveGameSystem, AFailedSaveKeepsTheOldFilesAndItsTemporaryFilesAsTheGameDoes)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto world = *saves.WorldFile(3, 1);
	const auto script = world.parent_path() / "Script_3.sav";
	const std::vector<uint8_t> oldWorld {1, 2, 3};
	const std::vector<uint8_t> oldScript {9};
	const auto expectTheOldFilesAndTheTemporaryFiles = [&] {
		EXPECT_EQ(files.files.at(world), oldWorld);
		EXPECT_EQ(files.files.at(script), oldScript);
		EXPECT_EQ(files.files.at(SaveGameSystem::TemporaryPath(world)), WorldBytes(0));
		EXPECT_EQ(files.files.at(SaveGameSystem::TemporaryPath(script)), (std::vector<uint8_t> {5}));
	};
	files.files[world] = oldWorld;
	files.files[script] = oldScript;
	const auto writeScript = [&files](const std::filesystem::path& file) {
		files.files[file] = {5};
		return true;
	};
	const auto failScript = [&files](const std::filesystem::path& file) {
		files.files[file] = {5};
		return false;
	};

	// the script machine's file written in part, then failing
	SaveContents contents {.world = WorldBytes(0), .scriptState = failScript};
	EXPECT_EQ(saves.Save(3, 1, contents), SaveWrite::Failed);
	expectTheOldFilesAndTheTemporaryFiles();

	// the script machine's file stopping with an exception, which the save does not let out
	contents.scriptState = [&files](const std::filesystem::path& file) -> bool {
		files.files[file] = {5};
		throw std::runtime_error("the disk is full");
	};
	EXPECT_EQ(saves.Save(3, 1, contents), SaveWrite::Failed);
	expectTheOldFilesAndTheTemporaryFiles();

	// the world file's temporary file that cannot take its name: the old world file is gone, the save is in the
	// temporary file, and the script machine's file is renamed all the same
	contents.scriptState = writeScript;
	files.unrenamable.insert(world);
	EXPECT_EQ(saves.Save(3, 1, contents), SaveWrite::Written);
	EXPECT_FALSE(files.files.contains(world));
	EXPECT_EQ(files.files.at(SaveGameSystem::TemporaryPath(world)), WorldBytes(0));
	EXPECT_EQ(files.files.at(script), (std::vector<uint8_t> {5}));
	EXPECT_FALSE(files.files.contains(SaveGameSystem::TemporaryPath(script)));
}

TEST(SaveGameSystem, WithNoProfileKeptASaveWritesNothing)
{
	test::MemoryFileSystem files;
	SaveGameSystem saves(&files, []() -> std::optional<std::filesystem::path> { return std::nullopt; });
	EXPECT_EQ(saves.Save(3, 1, {.world = WorldBytes(0)}), SaveWrite::NotOpened);
	EXPECT_EQ(files.writes, 0);
}

TEST(SaveGameSystem, ASlotsSaveCopiesTheMapPicturesPutsItsPicturesThenWritesItsInfoBeforeItsWorld)
{
	test::MemoryFileSystem files;
	auto saves = Kept(files);
	const auto folder = k_Player / "Saved Games" / "5";
	const auto pictures = k_Player / "Saved Games" / "Pictures";
	// the profile's map pictures: one of the large pages and one of the small are there
	files.files[k_Player / "Current" / "screenshots_hires_3_map_0.raw"] = {3};
	files.files[k_Player / "Current" / "screenshots_lores_1_map_0.raw"] = {1};
	files.files[pictures / gamesave::k_TemporaryPictureFileName] = gamesave::WritePictureFile(gamesave::NewPictureBlocks());
	const gamesave::SaveGameInfo info {.slot = 5, .date = u"12:00:00 1", .name = u"Mine"};
	const SaveContents contents {
	    .info = info,
	    .pictures = true,
	    .world = WorldBytes(0),
	    .scriptState =
	        [&](const std::filesystem::path& file) {
		        // the pictures and the info are written before the world's files
		        EXPECT_TRUE(files.files.contains(folder / "SaveGame.inf"));
		        EXPECT_TRUE(files.files.contains(pictures / gamesave::LargePictureFileName(5)));
		        files.files[file] = {5};
		        return true;
	        },
	};
	EXPECT_EQ(saves.Save(5, 1, contents), SaveWrite::Written);
	EXPECT_EQ(files.files.at(folder / "screenshots_hires_3_map_0.raw"), (std::vector<uint8_t> {3}));
	EXPECT_EQ(files.files.at(folder / "screenshots_lores_1_map_0.raw"), (std::vector<uint8_t> {1}));
	EXPECT_FALSE(files.files.contains(folder / "screenshots_hires_0_map_0.raw"));
	EXPECT_EQ(gamesave::ReadSaveGameInfo(files.files.at(folder / "SaveGame.inf")), info);
	EXPECT_EQ(files.files.at(pictures / gamesave::LargePictureFileName(5)).size(), gamesave::k_PictureFileSize);
	EXPECT_EQ(files.files.at(pictures / gamesave::SmallPictureFileName(5)).size(), gamesave::k_PictureFileSize);
	// the eleven large map pictures, then the three small
	const auto names = SaveGameSystem::MapPictureNames();
	ASSERT_EQ(names.size(), 14u);
	EXPECT_EQ(names.front(), "screenshots_hires_0_map_0.raw");
	EXPECT_EQ(names.at(10), "screenshots_hires_10_map_0.raw");
	EXPECT_EQ(names.back(), "screenshots_lores_2_map_0.raw");
}

TEST(SavedSlot, ASavesDateIsTheTimeThenTheDateWhenBothFit)
{
	EXPECT_EQ(ecs::saved_slot::DateText(u"22:01:40", u"jueves, 12 de septiembre de 2024"),
	          u"22:01:40 jueves, 12 de septiembre de 2024");
	// the two texts with their ends take 62 characters or more: no date
	const std::u16string time(30, u'1');
	EXPECT_EQ(ecs::saved_slot::DateText(time, std::u16string(30, u'2')), u"");
	EXPECT_EQ(ecs::saved_slot::DateText(time, std::u16string(29, u'2')), time + u" " + std::u16string(29, u'2'));
}

TEST(SavedSlot, TheSlotsTitlesAreTheExesHelpTexts)
{
	// the player's slots have none of their own; the previous land, the land autosaves, the next land, the quick save
	// and the autosave on exit have theirs
	for (uint32_t slot = 0; slot < 14; ++slot)
	{
		EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(slot), 0u);
	}
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(14), 0x1A20u);
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(15), 0x1A22u);
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(17), 0x1A22u);
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(18), 0x1A21u);
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(19), 0x1A05u);
	EXPECT_EQ(ecs::saved_slot::k_SlotTitleTexts.at(20), 0x1A23u);
}

TEST(SavedSlot, TheCreaturesMindsAreNumberedNewestFirst)
{
	ecs::Registry registry;
	const auto made = [&registry](uint32_t index) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Creature>(entity);
		registry.Assign<ecs::components::ObjectCreationIndex>(entity, index);
		return entity;
	};
	const auto first = made(10);
	const auto third = made(30);
	const auto second = made(20);
	EXPECT_EQ(ecs::saved_slot::CreaturesInListOrder(registry), (std::vector {third, second, first}));
}
