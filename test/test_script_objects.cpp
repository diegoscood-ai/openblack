/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ScriptObjectTable.h"

using namespace openblack::ecs::script_objects;

// The table's own cases of raffclar's test/test_script_objects.cpp (0222f9f5); the cases of his system come with it

TEST(ScriptObjects, OnlyTheNativesThatMoveOrSetObjectsTakeControl)
{
	EXPECT_TRUE(TakesControl(17));  // set script state
	EXPECT_TRUE(TakesControl(33));  // move a thing
	EXPECT_TRUE(TakesControl(218)); // start a refereed match
	EXPECT_FALSE(TakesControl(27)); // create
	EXPECT_FALSE(TakesControl(110));
}

TEST(ScriptObjects, AnObjectKeepsItsPlace)
{
	Table table;
	const auto first = table.Register(42, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, 1);
	// The scripts know it by the object itself, so it never takes a second place
	EXPECT_EQ(table.Register(42, false), first);
	EXPECT_TRUE(table.At(*first).createdByScript);
	EXPECT_EQ(table.Register(43, false), 2);
}

TEST(ScriptObjects, TheFirstReferenceIsTold)
{
	Table table;
	const auto place = *table.Register(7, false);
	EXPECT_EQ(table.AddReference(place), Referenced::First);
	EXPECT_EQ(table.AddReference(place), Referenced::Again);
	table.RemoveReference(place);
	table.RemoveReference(place);
	table.RemoveReference(place);
	EXPECT_EQ(table.At(place).count, 0);
	// Let go of every reference, the place is still the object's until it is freed
	EXPECT_EQ(table.Find(7), place);
	EXPECT_EQ(table.Unreferenced(), std::vector<uint16_t> {place});
	table.Free(place);
	EXPECT_FALSE(table.Find(7).has_value());
	EXPECT_TRUE(table.Unreferenced().empty());
	EXPECT_EQ(table.AddReference(0), Referenced::Nothing);
}

TEST(ScriptObjects, TheTableFillsAndIsClearedWithTheProgram)
{
	Table table;
	for (uint32_t object = 1; object < k_Places; ++object)
	{
		ASSERT_TRUE(table.Register(object, false).has_value());
	}
	EXPECT_FALSE(table.Register(9999, false).has_value());
	table.Clear();
	// Every place is free, and the search starts at the first again
	EXPECT_EQ(table.Register(9999, false), 1);
}

TEST(ScriptObjects, ANewLandKeepsThePlacesStillCounted)
{
	Table table;
	const auto counted = *table.Register(5, false);
	table.AddReference(counted);
	const auto free = *table.Register(6, false);
	table.ClearObjects();
	EXPECT_FALSE(table.Find(5).has_value());
	EXPECT_EQ(table.At(counted).count, 1);
	// The search goes on from where it stopped: past the counted place, the other is free again once it comes round
	const auto next = table.Register(7, false);
	ASSERT_TRUE(next.has_value());
	EXPECT_NE(*next, counted);
	EXPECT_EQ(*next, free);
}

TEST(ScriptObjects, ADeadTreeTakesItsTreesPlace)
{
	Table table;
	const auto place = *table.Register(5, false);
	table.Replace(5, 6);
	EXPECT_EQ(table.Find(6), place);
	EXPECT_FALSE(table.Find(5).has_value());
}
