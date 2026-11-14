/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The keys' names as the game asks the system for them: what it asks with, the room it gives, and the Controls page's
// and the help's key texts made from them

#include <cstdint>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/ControlBindings.h"
#include "Input/EnglishUsKeyNames.h"
#include "Input/SystemKeyName.h"

using namespace openblack;
namespace key_name = openblack::input::system_key_name;
namespace control_bindings = openblack::gui::control_bindings;

namespace
{
struct Asked
{
	int32_t param;
	int32_t room;
};
std::vector<Asked> g_Asked;

std::u16string FakeNames(int32_t param, int32_t room)
{
	g_Asked.push_back({param, room});
	switch ((param >> 16) & 0xFF)
	{
	case 0x1D:
		return u"CTRL";
	case 0x39:
		return u"BARRA ESPACIADO";
	case 0x1F:
		return u"S";
	default:
		return {};
	}
}

struct SourceFixture: ::testing::Test
{
	void SetUp() override
	{
		g_Asked.clear();
		key_name::SetSource(&FakeNames);
	}
	void TearDown() override { key_name::SetSource(nullptr); }
};
} // namespace

TEST(SystemKeyName, TheGameAsksForTheScanCodeWithTheExtendedBitAndNoLeftOrRight)
{
	// the scan code at bit 16, the code's 0x80 (an extended key) at bit 24, and bit 25 always set
	EXPECT_EQ(key_name::NameParam(0x1D), 0x021D0000);
	EXPECT_EQ(key_name::NameParam(0x9D), 0x031D0000);
	EXPECT_EQ(key_name::NameParam(0x36), 0x02360000);
	EXPECT_EQ(key_name::NameParam(0x39), 0x02390000);
	EXPECT_EQ(key_name::NameParam(0xC8), 0x03480000);
	EXPECT_EQ(key_name::NameParam(0x3B), 0x023B0000);
}

TEST(SystemKeyName, WithNoSourceTheDefaultsKeepTheirOwnNames)
{
	key_name::SetSource(nullptr);
	EXPECT_FALSE(key_name::Name(0x1D).has_value());
	EXPECT_EQ(control_bindings::KeyName(0x1D), u"CTRL");
	EXPECT_EQ(control_bindings::KeyName(0x39), u"SPACE");
}

TEST_F(SourceFixture, AKeysNameIsTheSystemsWithRoomForFifteenLetters)
{
	EXPECT_EQ(control_bindings::KeyName(0x39), u"BARRA ESPACIADO");
	ASSERT_EQ(g_Asked.size(), 1u);
	EXPECT_EQ(g_Asked.at(0).param, 0x02390000);
	EXPECT_EQ(g_Asked.at(0).room, 16);
}

TEST_F(SourceFixture, AKeyTheSystemHasNoNameForHasNone)
{
	// the game shows what the system gives, nothing for a key it has no name for
	EXPECT_EQ(control_bindings::KeyName(0xC8), u"");
}

TEST_F(SourceFixture, TheKeyTextJoinsTheSystemsNamesOfTheModifierAndTheKey)
{
	const control_bindings::Binding quickSave {
	    .name = "HELP_TEXT_TOOLTIP_155", .key = 0x1F, .modifier = 0x1D, .mouse = control_bindings::Mouse::None};
	EXPECT_EQ(control_bindings::KeyText(quickSave), u"CTRL + S");
}

TEST(SystemKeyName, ARunWithNoRealInputNamesTheKeysAsAnEnglishUsLayoutDoes)
{
	// a test run's names are the same on every machine, whatever its own layout
	key_name::InstallSystemNames([] { return false; });
	EXPECT_EQ(control_bindings::KeyName(0x39), u"Space");
	EXPECT_EQ(control_bindings::KeyName(0x36), u"Shift");
	EXPECT_EQ(control_bindings::KeyName(0x9D), u"Ctrl");
	EXPECT_EQ(control_bindings::KeyName(0xC8), u"Up");
	EXPECT_EQ(control_bindings::KeyName(0x2E), u"C");
	const control_bindings::Binding quickSave {
	    .name = "HELP_TEXT_TOOLTIP_155", .key = 0x1F, .modifier = 0x1D, .mouse = control_bindings::Mouse::None};
	EXPECT_EQ(control_bindings::KeyText(quickSave), u"Ctrl + S");
	key_name::InstallSystemNames(nullptr);
	EXPECT_EQ(control_bindings::KeyName(0x39), u"SPACE");
}

TEST(SystemKeyName, TheEnglishUsNamesAreThatLayoutsAsTheGameAsksForThem)
{
	// read from the system with that layout: 166 codes have a name, none longer than the room's 15 letters, and the
	// left and right Shift, Ctrl and Alt give one name
	EXPECT_EQ(key_name::k_EnglishUsNames.size(), 166u);
	for (const auto& entry : key_name::k_EnglishUsNames)
	{
		EXPECT_FALSE(entry.name.empty());
		EXPECT_LT(entry.name.size(), 16u);
	}
	EXPECT_EQ(key_name::EnglishUsName(0x2A), key_name::EnglishUsName(0x36));
	EXPECT_EQ(key_name::EnglishUsName(0x1D), key_name::EnglishUsName(0x9D));
	EXPECT_EQ(key_name::EnglishUsName(0x38), key_name::EnglishUsName(0xB8));
	EXPECT_EQ(key_name::EnglishUsName(0x01), u"Esc");
	EXPECT_EQ(key_name::EnglishUsName(0x0E), u"Backspace");
	EXPECT_EQ(key_name::EnglishUsName(0x1C), u"Enter");
	EXPECT_EQ(key_name::EnglishUsName(0x9C), u"Num Enter");
	EXPECT_EQ(key_name::EnglishUsName(0x2B), u"\\");
	EXPECT_EQ(key_name::EnglishUsName(0x3A), u"Caps Lock");
	EXPECT_EQ(key_name::EnglishUsName(0x44), u"F10");
	EXPECT_EQ(key_name::EnglishUsName(0x58), u"F12");
	EXPECT_EQ(key_name::EnglishUsName(0xB7), u"Prnt Scrn");
	EXPECT_EQ(key_name::EnglishUsName(0xC9), u"Page Up");
	EXPECT_EQ(key_name::EnglishUsName(0xD3), u"Delete");
	EXPECT_EQ(key_name::EnglishUsName(0xD4), u"<00>");
	EXPECT_EQ(key_name::EnglishUsName(0xDB), u"Left Windows");
	EXPECT_EQ(key_name::EnglishUsName(0xDD), u"Application");
	EXPECT_EQ(key_name::EnglishUsName(0x55), u"");
	EXPECT_EQ(key_name::EnglishUsName(0x00), u"");
}

#if defined(_WIN32)
TEST(SystemKeyName, TheSystemGivesOneNameForTheLeftAndRightKeys)
{
	// Integration: this system's own names. Whatever the keyboard layout, the left and right Ctrl and Shift are not
	// told apart, and a name fits the room
	key_name::InstallSystemNames([] { return true; });
	const auto leftCtrl = key_name::Name(0x1D);
	const auto rightCtrl = key_name::Name(0x9D);
	const auto leftShift = key_name::Name(0x2A);
	const auto rightShift = key_name::Name(0x36);
	key_name::InstallSystemNames(nullptr);
	ASSERT_TRUE(leftCtrl.has_value() && rightCtrl.has_value() && leftShift.has_value() && rightShift.has_value());
	EXPECT_EQ(*leftCtrl, *rightCtrl);
	EXPECT_EQ(*leftShift, *rightShift);
	EXPECT_LT(leftShift->size(), 16u);
}
#endif
