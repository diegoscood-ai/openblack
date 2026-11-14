/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The list of script files STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING keeps: names apart by spaces, commas or tabs, compared
// without case

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptNameLists.h"

using openblack::script::name_lists::HoldsFile;

TEST(ScriptFileList, OneNameIsFoundWithoutCase)
{
	EXPECT_TRUE(HoldsFile("LandControlAll.txt", "LandControlAll.txt"));
	EXPECT_TRUE(HoldsFile("LandControlAll.txt", "landcontrolall.TXT"));
	EXPECT_FALSE(HoldsFile("LandControlAll.txt", "LandControl1.txt"));
}

TEST(ScriptFileList, NamesAreApartBySpacesCommasOrTabs)
{
	constexpr auto k_List = " a.txt,b.txt\tc.txt ,, d.txt ";
	for (const auto* name : {"a.txt", "b.txt", "c.txt", "d.txt"})
	{
		EXPECT_TRUE(HoldsFile(k_List, name)) << name;
	}
	EXPECT_FALSE(HoldsFile(k_List, "e.txt"));
	EXPECT_FALSE(HoldsFile(k_List, "a.tx"));
}

TEST(ScriptFileList, AnEmptyListKeepsNothing)
{
	EXPECT_FALSE(HoldsFile("", "a.txt"));
	EXPECT_FALSE(HoldsFile(" ,\t", ""));
}
