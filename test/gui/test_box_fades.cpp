/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <Gui/BoxFades.h>
#include <gtest/gtest.h>

using openblack::Zoomer;
using openblack::gui::BoxFades;
using Switch = BoxFades<int>::Switch;

TEST(BoxFades, AShownBoxCrossFadesOverTheOneItReplaces)
{
	BoxFades<int> fades;
	fades.Show(1, Switch::Replace);
	fades.Update(0.6f);
	ASSERT_EQ(fades.Shown()->second, 1.0f);
	fades.Show(2, Switch::CrossFade);
	// The new box starts from nothing, the old from where it was, both drawn
	EXPECT_EQ(fades.Shown()->first, 2);
	EXPECT_EQ(fades.Shown()->second, 0.0f);
	ASSERT_TRUE(fades.Leaving().has_value());
	EXPECT_EQ(fades.Leaving()->first, 1);
	EXPECT_EQ(fades.Leaving()->second, 1.0f);
	// Each on the zoomer's curve: in over half a second, out over a fifth
	Zoomer in;
	in.SetDestinationWithSpeedAndTime(1.0f, 0.0f, 0.5f);
	Zoomer out;
	out.SetPosition(1.0f);
	out.SetDestinationWithSpeedAndTime(0.0f, 0.0f, 0.2f);
	fades.Update(0.1f);
	in.Update(0.1f);
	out.Update(0.1f);
	EXPECT_EQ(fades.Shown()->second, in.value);
	EXPECT_EQ(fades.Leaving()->second, out.value);
	// The old box is gone after a fifth of a second, the new one whole after half a second
	fades.Update(0.1f);
	EXPECT_FALSE(fades.Leaving().has_value());
	EXPECT_LT(fades.Shown()->second, 1.0f);
	fades.Update(0.3f);
	EXPECT_EQ(fades.Shown()->second, 1.0f);
}

TEST(BoxFades, ATabReplacesTheBoxAtOnceAndFadesItInAgainFromNothing)
{
	BoxFades<int> fades;
	fades.Show(1, Switch::Replace);
	fades.Update(0.6f);
	fades.Show(2, Switch::CrossFade);
	// A tab while the first still fades out: both boxes before it go at once
	fades.Show(3, Switch::Replace);
	EXPECT_FALSE(fades.Leaving().has_value());
	fades.Update(0.6f);
	// The tab of the box already shown: it fades in again from nothing
	fades.Show(3, Switch::Replace);
	EXPECT_FALSE(fades.Leaving().has_value());
	EXPECT_EQ(fades.Shown()->first, 3);
	EXPECT_EQ(fades.Shown()->second, 0.0f);
}

TEST(BoxFades, SnappingPutsTheBoxShownAtOnceWhole)
{
	BoxFades<int> fades;
	fades.SnapShown();
	EXPECT_FALSE(fades.Shown().has_value());
	fades.Show(1, Switch::Replace);
	fades.Update(0.1f);
	fades.SnapShown();
	EXPECT_EQ(fades.Shown()->second, 1.0f);
	fades.Update(0.1f);
	EXPECT_EQ(fades.Shown()->second, 1.0f);
}

TEST(BoxFades, HidingFadesTheBoxShownOutAndShowsNone)
{
	BoxFades<int> fades;
	EXPECT_FALSE(fades.Shown().has_value());
	fades.Hide();
	EXPECT_FALSE(fades.Leaving().has_value());
	fades.Show(1, Switch::Replace);
	fades.Update(0.6f);
	fades.Hide();
	EXPECT_FALSE(fades.Shown().has_value());
	ASSERT_TRUE(fades.Leaving().has_value());
	EXPECT_EQ(fades.Leaving()->first, 1);
	fades.Update(0.1f);
	EXPECT_TRUE(fades.Leaving().has_value());
	fades.Update(0.11f);
	EXPECT_FALSE(fades.Leaving().has_value());
}
