/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script functions' stack counts as the original's script functions pop and push them

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "CHLApi.h"

using namespace openblack;
using openblack::chlapi::CHLApi;

namespace
{
const lhvm::NativeFunction* Find(CHLApi& api, const std::string& name)
{
	const auto& table = api.GetFunctionsTable();
	const auto it = std::find_if(table.begin(), table.end(), [&name](const auto& f) { return f.name == name; });
	return it != table.end() ? &*it : nullptr;
}
} // namespace

TEST(ChlApiStack, IsAffectedBySpellPopsTwo)
{
	CHLApi api;
	const auto* function = Find(api, "IS_AFFECTED_BY_SPELL");
	ASSERT_NE(function, nullptr);
	// IsAffectedBySpell: two pops, one bool pushed
	EXPECT_EQ(function->stackIn, 2);
	EXPECT_EQ(function->stackOut, 1u);
}

TEST(ChlApiStack, GetActionTextForObjectPopsNothing)
{
	chlapi::CHLApi api;
	const auto* function = Find(api, "GET_ACTION_TEXT_FOR_OBJECT");
	ASSERT_NE(function, nullptr);
	// GetActionTextForObject: no pop, one int pushed
	EXPECT_EQ(function->stackIn, 0);
	EXPECT_EQ(function->stackOut, 1u);
}

TEST(ChlApiStack, LeashFunctionsPopAndPushAsTheOriginal)
{
	CHLApi api;
	struct Counts
	{
		const char* name;
		int stackIn;
		int stackOut;
	};
	// the thing and the creature, the creature, the creature, a creature (a bool), the creature and the value, the
	// creature and the thing (a bool), a creature (an int), the value, the player
	for (const auto& [name, stackIn, stackOut] : {
	         Counts {"ATTACH_OBJECT_LEASH_TO_OBJECT", 2, 0},
	         Counts {"ATTACH_OBJECT_LEASH_TO_HAND", 1, 0},
	         Counts {"DETACH_OBJECT_LEASH", 1, 0},
	         Counts {"IS_LEASHED", 1, 1},
	         Counts {"SET_LEASH_WORKS", 2, 0},
	         Counts {"IS_LEASHED_TO_OBJECT", 2, 1},
	         Counts {"GET_OBJECT_LEASH_TYPE", 1, 1},
	         Counts {"SET_DRAW_LEASH", 1, 0},
	         Counts {"TOGGLE_LEASH", 1, 0},
	     })
	{
		const auto* function = Find(api, name);
		ASSERT_NE(function, nullptr) << name;
		EXPECT_EQ(function->stackIn, stackIn) << name;
		EXPECT_EQ(function->stackOut, stackOut) << name;
	}
}

TEST(ChlApiStack, ResourceFunctionsPopAndPushAsTheOriginal)
{
	CHLApi api;
	struct Counts
	{
		const char* name;
		int stackIn;
		int stackOut;
	};
	// the thing and the type; the thing, the amount and the type; the thing, the quantity and the type. Each pushes one
	// float, 0 when the thing is missing or not an object
	for (const auto& [name, stackIn, stackOut] : {
	         Counts {"GET_RESOURCE", 2, 1},
	         Counts {"ADD_RESOURCE", 3, 1},
	         Counts {"REMOVE_RESOURCE", 3, 1},
	     })
	{
		const auto* function = Find(api, name);
		ASSERT_NE(function, nullptr) << name;
		EXPECT_EQ(function->stackIn, stackIn) << name;
		EXPECT_EQ(function->stackOut, stackOut) << name;
	}
}

TEST(ChlApiStack, WindFunctionsPopAndPushAsTheOriginal)
{
	CHLApi api;
	struct Counts
	{
		const char* name;
		int stackIn;
		int stackOut;
	};
	// the radius and the point (a bool back); the player and the value, nothing back; the player alone (a bool back).
	// The game's own table lists 1 / 1, 2 / 1 and 2 / 1, its functions pop and push these
	for (const auto& [name, stackIn, stackOut] : {
	         Counts {"IS_WIND_MAGIC_AT_POS", 4, 1},
	         Counts {"SET_PLAYER_WIND_RESISTANCE", 2, 0},
	         Counts {"GET_PLAYER_WIND_RESISTANCE", 1, 1},
	     })
	{
		const auto* function = Find(api, name);
		ASSERT_NE(function, nullptr) << name;
		EXPECT_EQ(function->stackIn, stackIn) << name;
		EXPECT_EQ(function->stackOut, stackOut) << name;
	}
}
