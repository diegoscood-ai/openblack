/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include "Gui/Controls.h"
#include "Gui/Dialog.h"

using openblack::gui::IsCaretShown;

TEST(EditBoxCaret, ShownOnTheOddFifthsOfASecondOfTheTickCount)
{
	EXPECT_FALSE(IsCaretShown(0));
	EXPECT_FALSE(IsCaretShown(199));
	EXPECT_TRUE(IsCaretShown(200));
	EXPECT_TRUE(IsCaretShown(399));
	EXPECT_FALSE(IsCaretShown(400));
	EXPECT_FALSE(IsCaretShown(599));
	EXPECT_TRUE(IsCaretShown(600));
	EXPECT_TRUE(IsCaretShown(1000 * 3600 + 200));
	EXPECT_FALSE(IsCaretShown(1000 * 3600 + 400));
	// The tick count wraps at 2^32: 4294967295 / 200 = 21474836, even
	EXPECT_FALSE(IsCaretShown(UINT32_MAX));
}

// The menu's dialog controls, as every dialog of the menu uses them (the statistics box's too): the control under the
// pointer is the one lit, the button going down on a control gives it the focus and holds it, and letting go over the
// control it went down on makes it act; let go elsewhere, or gone down elsewhere, nothing acts
TEST(DialogControls, AControlActsWhenTheButtonIsLetGoOverTheControlItWentDownOn)
{
	openblack::gui::Dialog dialog;
	int clicks = 0;
	auto& button = dialog.Add<openblack::gui::Button>(openblack::gui::DialogRect {.min = {100, 100}, .max = {200, 140}}, u"OK");
	button.onClick = [&clicks]() { ++clicks; };
	dialog.MouseMove({150, 120});
	EXPECT_TRUE(dialog.IsPointerOverControl());
	dialog.MouseMove({10, 10});
	EXPECT_FALSE(dialog.IsPointerOverControl());

	dialog.MouseDown({150, 120});
	EXPECT_TRUE(dialog.IsHolding());
	EXPECT_EQ(dialog.GetFocus(), &button);
	EXPECT_TRUE(dialog.MouseUp({190, 130}));
	EXPECT_FALSE(dialog.IsHolding());
	EXPECT_EQ(clicks, 1);

	// let go away from it: no click
	dialog.MouseDown({150, 120});
	EXPECT_FALSE(dialog.MouseUp({10, 10}));
	EXPECT_EQ(clicks, 1);
	// gone down away from it and let go over it: no click, and the focus goes nowhere
	dialog.MouseDown({10, 10});
	EXPECT_EQ(dialog.GetFocus(), nullptr);
	EXPECT_FALSE(dialog.MouseUp({150, 120}));
	EXPECT_EQ(clicks, 1);
}

// A hidden control takes no pointer and no click
TEST(DialogControls, AHiddenControlNeitherLightsUpNorActs)
{
	openblack::gui::Dialog dialog;
	int clicks = 0;
	auto& button = dialog.Add<openblack::gui::Button>(openblack::gui::DialogRect {.min = {100, 100}, .max = {200, 140}}, u"OK");
	button.onClick = [&clicks]() { ++clicks; };
	button.visible = false;
	dialog.MouseMove({150, 120});
	EXPECT_FALSE(dialog.IsPointerOverControl());
	dialog.MouseDown({150, 120});
	EXPECT_FALSE(dialog.MouseUp({150, 120}));
	EXPECT_EQ(clicks, 0);
}
