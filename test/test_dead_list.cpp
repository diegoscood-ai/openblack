/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dead list's order and pass rules, on plain numbers. The orders are the original's, run on fake things
// (docs/wiki/engine/loop.md#the-dead-list)

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/DeadList.h"

using openblack::ecs::DeadList;

namespace
{

std::vector<int> Order(const DeadList<int>& list)
{
	std::vector<int> things;
	list.Each([&things](int thing) { things.push_back(thing); });
	return things;
}

} // namespace

TEST(DeadList, MarkedGoToTheHeadOnce)
{
	DeadList<int> list;
	EXPECT_TRUE(list.Mark(0));
	EXPECT_TRUE(list.Mark(1));
	EXPECT_FALSE(list.Mark(1));
	EXPECT_TRUE(list.Mark(2));
	EXPECT_EQ(Order(list), (std::vector<int> {2, 1, 0}));
	EXPECT_FALSE(list.Passed(1));
}

TEST(DeadList, TheFirstPassNotesTheNextFrees)
{
	DeadList<int> list;
	list.Mark(0);
	list.Mark(1);
	list.Mark(2);
	std::vector<int> freed;
	const auto free = [&freed](int thing) { freed.push_back(thing); };

	EXPECT_EQ(list.Process(false, free), 0U);
	EXPECT_TRUE(freed.empty());
	EXPECT_TRUE(list.Passed(0) && list.Passed(1) && list.Passed(2));

	// marked between the passes: at the head, not yet noted
	list.Mark(3);
	EXPECT_EQ(Order(list), (std::vector<int> {3, 2, 1, 0}));
	EXPECT_EQ(list.Process(false, free), 3U);
	EXPECT_EQ(freed, (std::vector<int> {2, 1, 0}));
	EXPECT_EQ(Order(list), (std::vector<int> {3}));
	EXPECT_TRUE(list.Passed(3));

	EXPECT_EQ(list.Process(false, free), 1U);
	EXPECT_EQ(freed, (std::vector<int> {2, 1, 0, 3}));
	EXPECT_TRUE(list.Empty());
}

TEST(DeadList, DrainFreesEveryThingFromTheHead)
{
	DeadList<int> list;
	list.Mark(0);
	list.Mark(1);
	list.Mark(2);
	std::vector<int> freed;
	EXPECT_EQ(list.Process(true, [&freed](int thing) { freed.push_back(thing); }), 3U);
	EXPECT_EQ(freed, (std::vector<int> {2, 1, 0}));
	EXPECT_TRUE(list.Empty());
}

TEST(DeadList, MarkedWhileFreeingWaitsForTheNextPass)
{
	DeadList<int> list;
	list.Mark(0);
	list.Mark(1);
	list.Process(false, [](int) {});
	std::vector<int> freed;
	// freeing 1 marks 10: it goes to the head, behind the walk, and the walk goes on to 0
	list.Process(false, [&](int thing) {
		freed.push_back(thing);
		if (thing == 1)
		{
			list.Mark(10);
		}
	});
	EXPECT_EQ(freed, (std::vector<int> {1, 0}));
	EXPECT_EQ(Order(list), (std::vector<int> {10}));
	EXPECT_FALSE(list.Passed(10));
}

TEST(DeadList, DrainWalksAgainUntilEmpty)
{
	DeadList<int> list;
	list.Mark(0);
	list.Mark(1);
	std::vector<int> freed;
	const auto count = list.Process(true, [&](int thing) {
		freed.push_back(thing);
		if (thing < 10)
		{
			list.Mark(thing + 10);
		}
	});
	EXPECT_EQ(count, 4U);
	// the first walk frees 1 and 0 (marking 11 then 10 at the head), the second 10 then 11
	EXPECT_EQ(freed, (std::vector<int> {1, 0, 10, 11}));
	EXPECT_TRUE(list.Empty());
}

TEST(DeadList, MarkedBeforeTheTurnsPassGoesNextTurnAfterItOneTurnMore)
{
	// one pass per turn; a thing marked before it in turn 0, another after it
	DeadList<int> list;
	std::vector<std::pair<int, int>> freedAt;
	int turn = 0;
	const auto free = [&](int thing) { freedAt.emplace_back(thing, turn); };
	list.Mark(1);
	list.Process(false, free);
	list.Mark(2);
	for (turn = 1; turn <= 2; ++turn)
	{
		list.Process(false, free);
	}
	EXPECT_EQ(freedAt, (std::vector<std::pair<int, int>> {{1, 1}, {2, 2}}));
}
