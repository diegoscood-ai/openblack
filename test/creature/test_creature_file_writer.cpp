/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Saving a creature at a land change: its mind file written through the file system service and read back through
// the mind cache, its physique file, and what the live creature adds to them. A file system in memory stands for the
// disk; the data is synthetic

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <istream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <PhysiqueFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "FileSystem/FileSystemInterface.h"
#include "FileSystem/Stream.h"
#include "Locator.h"
#include "Resources/CreatureFileWriter.h"
#include "Resources/Loaders.h"
#include "support/FileSystemFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::creaturemind;
using openblack::test::MemoryFileSystem;
using openblack::test::ReadOnlyFileSystem;

namespace
{
/// A synthetic mind: a Mandrill's row, a name and a body
MindFileData SampleMind(float fatness)
{
	MindFileData mind;
	mind.speciesRow = 14;
	mind.name = u"Tester";
	mind.alignment = 0.25f;
	mind.physique.strength = 0.75f;
	mind.physique.fatness = fatness;
	mind.physique.previousFatness = 0.3f;
	mind.physique.size = 1.5f;
	return mind;
}

class CreatureFileWriterTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		// the mind loader reads through the Locator's file system: the memory one is injected there
		_memory = &static_cast<MemoryFileSystem&>(Locator::filesystem::emplace<MemoryFileSystem>());
	}

	/// The mind the cache holds for the file after a load (the cache's own copy, until it is dropped)
	[[nodiscard]] const creature::CreatureMind* LoadThroughCache(const std::string& id, const std::filesystem::path& path)
	{
		const auto loaded = _minds.Load(id, resources::CreatureMindLoader::FromDiskTag {}, path);
		const auto handle = _minds.Handle(loaded.first->first);
		return handle ? &*handle : nullptr;
	}

	const test::RestoreService<Locator::filesystem> _fileSystem;
	MemoryFileSystem* _memory {nullptr};
	resources::CreatureMindManager _minds;
};
} // namespace

TEST_F(CreatureFileWriterTest, AMindWrittenIsReadBackThroughTheCache)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/C00000001.erc";
	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000001.erc", path, SampleMind(0.4f)));
	EXPECT_EQ(_memory->writes, 1);
	EXPECT_EQ(_memory->files.at(path), Write(SampleMind(0.4f)));

	const auto mind = LoadThroughCache("C00000001.erc", path);
	ASSERT_NE(mind, nullptr);
	ASSERT_TRUE(mind->Loaded());
	EXPECT_EQ(mind->data.speciesRow, 14u);
	EXPECT_EQ(mind->data.name, u"Tester");
	EXPECT_EQ(mind->data.alignment.value_or(0.0f), 0.25f);
	EXPECT_EQ(mind->data.physique.strength, 0.75f);
	EXPECT_EQ(mind->data.physique.fatness, 0.4f);
	EXPECT_EQ(mind->data.physique.previousFatness, 0.3f);
	EXPECT_EQ(mind->data.physique.size.value_or(0.0f), 1.5f);
	EXPECT_EQ(Write(mind->data), Write(SampleMind(0.4f)));
}

TEST_F(CreatureFileWriterTest, SavingOverACachedMindMakesTheNextLoadReadTheNewFile)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/C00000002.erc";
	_memory->files[path] = Write(SampleMind(0.2f));
	ASSERT_EQ(LoadThroughCache("C00000002.erc", path)->data.physique.fatness, 0.2f);

	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000002.erc", path, SampleMind(0.9f)));
	EXPECT_FALSE(_minds.Contains(std::string("C00000002.erc")));
	EXPECT_EQ(LoadThroughCache("C00000002.erc", path)->data.physique.fatness, 0.9f);
}

TEST_F(CreatureFileWriterTest, AFileThatCannotBeWrittenLeavesTheCacheAlone)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/C00000003.erc";
	_memory->files[path] = Write(SampleMind(0.2f));
	ASSERT_TRUE(LoadThroughCache("C00000003.erc", path)->Loaded());

	ReadOnlyFileSystem readOnly;
	EXPECT_FALSE(resources::SaveCreatureMind(readOnly, _minds, "C00000003.erc", path, SampleMind(0.9f)));
	EXPECT_TRUE(_minds.Contains(std::string("C00000003.erc")));
	EXPECT_EQ(LoadThroughCache("C00000003.erc", path)->data.physique.fatness, 0.2f);
}

TEST_F(CreatureFileWriterTest, APhysiqueWrittenIsReadBack)
{
	const PhysiqueFileData physique {
	    .speciesRow = 14,
	    .size = 2.0f,
	    .strength = 0.8f,
	    .fatness = 0.4f,
	    .alignment = 0.1f,
	    .listA = {},
	    .listB = {7u, 8u, 9u},
	};
	const std::filesystem::path path = "game/Scripts/CreatureMind/PhysiqueC00000004.erc";
	ASSERT_TRUE(resources::SaveCreaturePhysique(*_memory, path, physique));
	const auto read = ReadPhysique(_memory->ReadAll(path));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->speciesRow, 14u);
	EXPECT_EQ(read->size, 2.0f);
	EXPECT_EQ(read->strength, 0.8f);
	EXPECT_EQ(read->fatness, 0.4f);
	EXPECT_EQ(read->alignment, 0.1f);
	EXPECT_TRUE(read->listA.empty());
	EXPECT_EQ(read->listB, (std::vector<uint32_t> {7u, 8u, 9u}));
}

TEST_F(CreatureFileWriterTest, ThePhysiqueKeepsTheSizeTheBodyKeeps)
{
	// the 3D body keeps 0.05 for any size up to 0.05 (and for one that is not a number), the size itself below 4, and 4
	// from there on
	const std::vector<std::pair<float, float>> sizes {
	    {0.01f, 0.05f}, {0.05f, 0.05f}, {-1.0f, 0.05f}, {std::numeric_limits<float>::quiet_NaN(), 0.05f},
	    {0.06f, 0.06f}, {3.99f, 3.99f}, {4.0f, 4.0f},   {4.5f, 4.0f},
	};
	const std::filesystem::path path = "game/Scripts/CreatureMind/PhysiqueC00000005.erc";
	for (const auto& [given, kept] : sizes)
	{
		const creature_mind_body::BodyNow body {.speciesRow = 14, .size = given};
		ASSERT_TRUE(resources::SaveCreaturePhysique(*_memory, path, creature_mind_body::ToPhysique(body, std::nullopt)));
		const auto read = ReadPhysique(_memory->ReadAll(path));
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(read->size, kept) << given;
	}
}

TEST_F(CreatureFileWriterTest, TheFirstSaveOverAFileBacksUpItsOldBytesOnce)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/C00000006.erc";
	const auto backup = resources::BackupPath(path);
	EXPECT_EQ(backup, std::filesystem::path("game/Scripts/CreatureMind/C00000006.erc.bak"));
	const auto old = Write(SampleMind(0.2f));
	_memory->files[path] = old;

	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000006.erc", path, SampleMind(0.5f)));
	ASSERT_TRUE(_memory->files.contains(backup));
	EXPECT_EQ(_memory->files.at(backup), old);
	EXPECT_EQ(_memory->files.at(path), Write(SampleMind(0.5f)));

	// a second save writes over the file again and leaves the backup as the first made it
	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000006.erc", path, SampleMind(0.8f)));
	EXPECT_EQ(_memory->files.at(backup), old);
	EXPECT_EQ(_memory->files.at(path), Write(SampleMind(0.8f)));
}

TEST_F(CreatureFileWriterTest, ABackupAlreadyThereIsNeverTouched)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/C00000007.erc";
	const std::vector<uint8_t> kept {1, 2, 3};
	_memory->files[resources::BackupPath(path)] = kept;
	_memory->files[path] = Write(SampleMind(0.2f));
	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000007.erc", path, SampleMind(0.5f)));
	EXPECT_EQ(_memory->files.at(resources::BackupPath(path)), kept);
	EXPECT_EQ(_memory->writes, 1);
}

TEST_F(CreatureFileWriterTest, ANewFileHasNoBackup)
{
	const std::filesystem::path mindPath = "game/Scripts/CreatureMind/C00000008.erc";
	const std::filesystem::path physiquePath = "game/Scripts/CreatureMind/PhysiqueC00000008.erc";
	ASSERT_TRUE(resources::SaveCreatureMind(*_memory, _minds, "C00000008.erc", mindPath, SampleMind(0.5f)));
	ASSERT_TRUE(resources::SaveCreaturePhysique(*_memory, physiquePath, PhysiqueFileData {.speciesRow = 14}));
	EXPECT_FALSE(_memory->files.contains(resources::BackupPath(mindPath)));
	EXPECT_FALSE(_memory->files.contains(resources::BackupPath(physiquePath)));
	EXPECT_EQ(_memory->writes, 2);
}

TEST_F(CreatureFileWriterTest, AnExistingPhysiqueIsBackedUpOnce)
{
	const std::filesystem::path path = "game/Scripts/CreatureMind/PhysiqueC00000009.erc";
	const auto old = WritePhysique({.speciesRow = 14, .size = 2.0f, .listB = {1u, 2u, 3u}});
	_memory->files[path] = old;
	ASSERT_TRUE(resources::SaveCreaturePhysique(*_memory, path, PhysiqueFileData {.speciesRow = 2}));
	ASSERT_TRUE(resources::SaveCreaturePhysique(*_memory, path, PhysiqueFileData {.speciesRow = 3}));
	EXPECT_EQ(_memory->files.at(resources::BackupPath(path)), old);
	EXPECT_EQ(_memory->files.at(path), WritePhysique({.speciesRow = 3}));
}

TEST_F(CreatureFileWriterTest, WithoutItsBackupTheUsersFileIsNotWrittenOver)
{
	// the choice: a backup that cannot be made stops that file's save, so the user's file is never lost; the creature
	// is then made on the next land from the file as it was
	const std::filesystem::path mindPath = "game/Scripts/CreatureMind/C0000000A.erc";
	const std::filesystem::path physiquePath = "game/Scripts/CreatureMind/PhysiqueC0000000A.erc";
	const auto oldMind = Write(SampleMind(0.2f));
	const auto oldPhysique = WritePhysique({.speciesRow = 14});
	_memory->files[mindPath] = oldMind;
	_memory->files[physiquePath] = oldPhysique;
	ASSERT_TRUE(LoadThroughCache("C0000000A.erc", mindPath)->Loaded());
	_memory->unwritable = {resources::BackupPath(mindPath), resources::BackupPath(physiquePath)};

	EXPECT_FALSE(resources::SaveCreatureMind(*_memory, _minds, "C0000000A.erc", mindPath, SampleMind(0.9f)));
	EXPECT_FALSE(resources::SaveCreaturePhysique(*_memory, physiquePath, PhysiqueFileData {.speciesRow = 2}));
	EXPECT_EQ(_memory->files.at(mindPath), oldMind);
	EXPECT_EQ(_memory->files.at(physiquePath), oldPhysique);
	EXPECT_FALSE(_memory->files.contains(resources::BackupPath(mindPath)));
	EXPECT_TRUE(_minds.Contains(std::string("C0000000A.erc")));
	EXPECT_EQ(_memory->writes, 0);
}

TEST(CreaturePhysiqueFile, TheLayoutIsTheSpeciesFourFloatsAndTwoCountedLists)
{
	const PhysiqueFileData physique {
	    .speciesRow = 14, .size = 2.0f, .strength = 0.5f, .fatness = 0.25f, .alignment = -1.0f, .listA = {1u}, .listB = {}};
	const std::vector<uint8_t> expected {
	    0x0E, 0x00, 0x00, 0x00,                         // the species row
	    0x00, 0x00, 0x00, 0x40,                         // 2.0
	    0x00, 0x00, 0x00, 0x3F,                         // 0.5
	    0x00, 0x00, 0x80, 0x3E,                         // 0.25
	    0x00, 0x00, 0x80, 0xBF,                         // -1.0
	    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // one number
	    0x00, 0x00, 0x00, 0x00,                         // none
	};
	EXPECT_EQ(WritePhysique(physique), expected);
}

TEST(CreaturePhysiqueFile, ShortOrOverlongBytesAreRefused)
{
	auto bytes = WritePhysique({.speciesRow = 1, .listA = {1u, 2u}, .listB = {3u}});
	EXPECT_TRUE(ReadPhysique(bytes).has_value());
	bytes.pop_back();
	EXPECT_FALSE(ReadPhysique(bytes).has_value());
	bytes = WritePhysique({});
	bytes.push_back(0);
	EXPECT_FALSE(ReadPhysique(bytes).has_value());
	// a list longer than the game keeps
	std::vector<uint8_t> tooLong(20, 0);
	for (const uint8_t b : {0x01, 0x04, 0x00, 0x00})
	{
		tooLong.push_back(b);
	}
	EXPECT_FALSE(ReadPhysique(tooLong).has_value());
}

// The mind file keeps the skin's marks with the body, its drops of blood and then its wounds, as the original's body
// save writes its damage; the file read back gives them again, which a load and a swap put on the skin
// (docs/wiki/project/progress/creature/marks.md, "Marks are kept in the creature's files")
TEST(CreatureFilesOfALiveCreature, TheMindFileKeepsTheSkinsMarksAndReadsThemBack)
{
	creature_marks::Marks marks;
	marks.blood = {{.u = 10, .v = 20, .skin = 1, .age = 5}};
	marks.wounds = {{.u = 0x12, .v = 0x34, .skin = 2, .age = 0x2A, .type = 5, .column = 6}};
	auto file = SampleMind(0.4f);
	file.physique.listA = {99u};
	const auto saved = creature_mind_body::ToMindFile(file, creature_mind_body::LiveBody {.marks = marks});
	ASSERT_EQ(saved.physique.listA.size(), 1u);
	ASSERT_EQ(saved.physique.listB.size(), 1u);
	EXPECT_EQ(saved.physique.listB[0], 0xF5AA3412u);
	MindFileData read;
	ASSERT_EQ(Read(Write(saved), read), MindResult::Success);
	const auto body = creature_mind_body::FromMindFile(read);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->marks.blood, marks.blood);
	EXPECT_EQ(body->marks.wounds, marks.wounds);
	// a body that keeps no marks leaves the file's
	const auto kept = creature_mind_body::ToMindFile(file, creature_mind_body::LiveBody {});
	EXPECT_EQ(kept.physique.listA, file.physique.listA);
}

TEST(CreatureFilesOfALiveCreature, TheMindTakesTheBodysFatnessAndKeepsTheRest)
{
	const auto saved =
	    creature_mind_body::ToMindFile(SampleMind(0.4f), creature_mind_body::LiveBody {.fatness = 0.65f, .shownFatness = 0.3f});
	EXPECT_EQ(saved.physique.fatness, 0.65f);
	// a creature with no tattoo, out of its growing-up scripts and at full fight health
	auto expected = SampleMind(0.65f);
	expected.inDevScript = 0;
	expected.tattooSlots = std::array<uint32_t, 8> {0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0};
	expected.fightHealth = 1.0f;
	EXPECT_EQ(Write(saved), Write(expected));
}

TEST(CreatureFilesOfALiveCreature, ThePhysiqueKeepsTheSkinListsOfTheFileItReplaces)
{
	const creature_mind_body::BodyNow body {
	    .speciesRow = 2, .size = 1.25f, .strength = 0.6f, .fatness = 0.3f, .alignment = -0.5f};
	const auto fresh = creature_mind_body::ToPhysique(body, std::nullopt);
	EXPECT_EQ(fresh.speciesRow, 2u);
	EXPECT_EQ(fresh.size, 1.25f);
	EXPECT_EQ(fresh.strength, 0.6f);
	EXPECT_EQ(fresh.fatness, 0.3f);
	EXPECT_EQ(fresh.alignment, -0.5f);
	EXPECT_TRUE(fresh.listA.empty());
	EXPECT_TRUE(fresh.listB.empty());

	const PhysiqueFileData previous {.speciesRow = 9, .size = 3.0f, .listA = {4u}, .listB = {5u, 6u}};
	const auto replaced = creature_mind_body::ToPhysique(body, previous);
	EXPECT_EQ(replaced.speciesRow, 2u);
	EXPECT_EQ(replaced.size, 1.25f);
	EXPECT_EQ(replaced.listA, previous.listA);
	EXPECT_EQ(replaced.listB, previous.listB);
}

// A creature with marks writes them in its physique file, its drops of blood and then its wounds, as the original's
// writes its cuts and then its bruises; read back, they are the marks it had
// (docs/wiki/project/progress/creature/marks.md, "Marks are kept in the creature's files")
TEST(CreatureFilesOfALiveCreature, ThePhysiqueKeepsTheSkinsMarksAndReadsThemBack)
{
	creature_marks::Marks marks;
	marks.blood = {{.u = 10, .v = 20, .skin = 1, .age = 5}};
	marks.wounds = {{.u = 0x12, .v = 0x34, .skin = 2, .age = 0x2A, .type = 5, .column = 6},
	                {.u = 255, .v = 0, .skin = 3, .age = 63, .type = 7, .column = 7}};
	const creature_mind_body::BodyNow body {.speciesRow = 2, .marks = marks};
	const PhysiqueFileData previous {.speciesRow = 9, .listA = {4u}, .listB = {5u, 6u}};
	const auto physique = creature_mind_body::ToPhysique(body, previous);
	ASSERT_EQ(physique.listA.size(), 1u);
	ASSERT_EQ(physique.listB.size(), 2u);
	// a wound's word: its place, age, skin, kind and column, and the two flag bits a wound is kept with
	EXPECT_EQ(physique.listB[0], 0xF5AA3412u);
	// a drop of blood keeps no flag bits
	EXPECT_EQ(physique.listA[0], 10u | (20u << 8U) | (5u << 16U) | (1u << 22U));
	const auto read = ReadPhysique(WritePhysique(physique));
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->listA.size(), 1u);
	ASSERT_EQ(read->listB.size(), 2u);
	EXPECT_EQ(creature_marks::FromFileWord(read->listA[0]), marks.blood[0]);
	EXPECT_EQ(creature_marks::FromFileWord(read->listB[0]), marks.wounds[0]);
	EXPECT_EQ(creature_marks::FromFileWord(read->listB[1]), marks.wounds[1]);
}
