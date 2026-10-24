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

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <gtest/gtest.h>

// The slots' folders and files and the save room's info file, on synthetic values
// (docs/wiki/engine/saved-games.md#where-saves-live).

using namespace openblack::gamesave;

namespace
{
template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}
} // namespace

TEST(SaveGameInfoFile, TheFileHas260BytesAndReadsBack)
{
	const SaveGameInfo info {.slot = 4, .date = u"10:15:00 Friday, 16 October 2026", .name = u"Before the ogre"};
	const auto bytes = Write(info);
	ASSERT_EQ(bytes.size(), k_SaveGameInfoSize);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 4u);
	EXPECT_EQ(At<char16_t>(bytes, 4), u'1');
	EXPECT_EQ(At<char16_t>(bytes, 132), u'B');
	EXPECT_EQ(ReadSaveGameInfo(bytes), info);
}

TEST(SaveGameInfoFile, ALongDateIsLeftOutAndALongNameIsCut)
{
	const SaveGameInfo info {.slot = 0, .date = std::u16string(62, u'x'), .name = std::u16string(70, u'n')};
	const auto read = ReadSaveGameInfo(Write(info));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->date.empty());
	EXPECT_EQ(read->name, std::u16string(63, u'n'));
	EXPECT_FALSE(ReadSaveGameInfo(std::vector<uint8_t>(259)).has_value());
}

TEST(SaveGameInfoFile, EachTextIsSixtyFourCharactersWithZerosAfterItsEnd)
{
	const auto bytes = Write({.slot = 20, .date = u"22:01", .name = u"Exit"});
	std::vector<uint8_t> golden(k_SaveGameInfoSize, 0);
	golden[0] = 20;
	const std::u16string_view date = u"22:01";
	const std::u16string_view name = u"Exit";
	std::memcpy(golden.data() + 4, date.data(), date.size() * 2);
	std::memcpy(golden.data() + 0x84, name.data(), name.size() * 2);
	EXPECT_EQ(bytes, golden);
}

TEST(SaveGameInfoFile, ATextEndsAtItsZeroWhateverTheBytesAfterIt)
{
	// The game copies each text into a buffer of its stack and writes the whole buffer, so what follows a text's zero
	// is what the stack held
	auto bytes = Write({.slot = 15, .date = u"9:00", .name = u"Land"});
	std::fill(bytes.begin() + 4 + 5 * 2, bytes.begin() + 0x84, uint8_t {0xCC});
	std::fill(bytes.begin() + 0x84 + 5 * 2, bytes.end(), uint8_t {0x41});
	const auto read = ReadSaveGameInfo(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->slot, 15u);
	EXPECT_EQ(read->date, u"9:00");
	EXPECT_EQ(read->name, u"Land");
}

TEST(SaveSlots, TheLandSlotsHaveAFolderForEachLand)
{
	EXPECT_EQ(SlotFolder(3, 1), std::filesystem::path("Saved Games") / "3");
	EXPECT_EQ(SlotFolder(14, 2), std::filesystem::path("Saved Games") / "14" / "2");
	EXPECT_EQ(SlotFolder(18, 2), std::filesystem::path("Saved Games") / "18" / "2");
	EXPECT_EQ(SlotFolder(20, 2), std::filesystem::path("Saved Games") / "20");
	// Only the autosaves keep the minds in the land's folder
	EXPECT_EQ(MindFolder(14, 2), std::filesystem::path("Saved Games") / "14");
	EXPECT_EQ(MindFolder(16, 2), std::filesystem::path("Saved Games") / "16" / "2");
	EXPECT_EQ(SavedCreatureFileName(0), "SavedCreature0");
	EXPECT_TRUE(IsPlayerSlot(13));
	EXPECT_FALSE(IsPlayerSlot(14));
	EXPECT_FALSE(IsSlot(21));
}

TEST(SaveSlots, TheFilesOfASlotAreNamedByTheSlot)
{
	EXPECT_EQ(VanillaWorldFileName(20), "20.sav");
	EXPECT_EQ(VanillaScriptFileName(15), "Script_15.sav");
	EXPECT_EQ(SlotFolder(15, 3) / VanillaWorldFileName(15), std::filesystem::path("Saved Games") / "15" / "3" / "15.sav");
	EXPECT_EQ(MindFolder(18, 4), std::filesystem::path("Saved Games") / "18");
	EXPECT_EQ(MindFolder(17, 1) / SavedCreatureFileName(3),
	          std::filesystem::path("Saved Games") / "17" / "1" / "SavedCreature3");
	// A load of the previous land's slot reads the land before, and of the next land's slot the land after
	EXPECT_EQ(LoadSlotFolder(14, 3), SlotFolder(14, 2));
	EXPECT_EQ(LoadSlotFolder(18, 3), SlotFolder(18, 4));
	EXPECT_EQ(LoadSlotFolder(16, 3), SlotFolder(16, 3));
	EXPECT_EQ(SaveBeforeRoomLoad(14), k_NextLandSlot);
	EXPECT_EQ(SaveBeforeRoomLoad(18), k_PreviousLandSlot);
	EXPECT_FALSE(SaveBeforeRoomLoad(5).has_value());
}

TEST(SaveSlots, TheRoomReadsTheAutosavesOfTheLandAndTheLandsBeforeAndAfter)
{
	const auto folderOf = [](std::string_view name, uint32_t land) {
		const auto found = RoomFolderOf(name, land);
		return found.has_value() ? found->folder : std::filesystem::path("none");
	};
	EXPECT_EQ(folderOf("3", 2), std::filesystem::path("3"));
	EXPECT_EQ(folderOf("15", 2), std::filesystem::path("15") / "2");
	EXPECT_EQ(folderOf("17", 0), std::filesystem::path("17") / "0");
	EXPECT_EQ(folderOf("14", 2), std::filesystem::path("14") / "1");
	EXPECT_EQ(folderOf("18", 2), std::filesystem::path("18") / "3");
	EXPECT_EQ(folderOf("20", 2), std::filesystem::path("20"));
	// The previous land's folder only from the second land, the next land's only up to the fourth
	EXPECT_EQ(folderOf("14", 1), std::filesystem::path("14"));
	EXPECT_EQ(folderOf("18", 4), std::filesystem::path("18") / "5");
	EXPECT_EQ(folderOf("18", 5), std::filesystem::path("18"));
	EXPECT_EQ(folderOf("18", 0), std::filesystem::path("18"));
	EXPECT_EQ(folderOf(".", 1), std::filesystem::path("none"));
	EXPECT_EQ(folderOf("..", 1), std::filesystem::path("none"));
	// The file's number comes from the start of the folder's name
	EXPECT_EQ(RoomFolderOf("15", 1)->fileSlot, 15);
	EXPECT_EQ(RoomFolderOf("Pictures", 1)->fileSlot, 0);
	EXPECT_EQ(LeadingNumber(" 07x"), 7);
	EXPECT_EQ(LeadingNumber("-3"), -3);
	EXPECT_EQ(LeadingNumber(""), 0);
}

TEST(SaveSlots, TheFoldersAreReadInTheOrderOfTheirNames)
{
	std::vector<std::string> names {"3", "20", "Pictures", "10", "1", "0", "15"};
	std::ranges::sort(names, ListedBefore);
	EXPECT_EQ(names, (std::vector<std::string> {"0", "1", "10", "15", "20", "3", "Pictures"}));
	EXPECT_TRUE(ListedBefore("abc", "ABD"));
	EXPECT_FALSE(ListedBefore("ABC", "abc"));
}
