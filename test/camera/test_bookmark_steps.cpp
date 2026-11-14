/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Tab keys of the camera's bookmarks (src/Camera/BookmarkSteps.h), as the original's key pass and bookmarks have
// them (docs/wiki/camera/player-camera.md#player-camera-the-tab-keys).

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Camera/BookmarkSteps.h"

using namespace openblack;
using bookmark_steps::Town;

namespace
{
std::array<bool, bookmark_steps::k_SteppedSlots> SetAt(std::initializer_list<int> slots)
{
	std::array<bool, bookmark_steps::k_SteppedSlots> set {};
	for (const int slot : slots)
	{
		set.at(static_cast<size_t>(slot)) = true;
	}
	return set;
}
} // namespace

TEST(BookmarkSteps, CtrlTabStepsToTheNextSetBookmarkRoundAllElevenSlots)
{
	const auto set = SetAt({1, 4, 6});
	EXPECT_EQ(bookmark_steps::Next(0, set), std::optional<uint8_t>(1));
	EXPECT_EQ(bookmark_steps::Next(1, set), std::optional<uint8_t>(4));
	EXPECT_EQ(bookmark_steps::Next(4, set), std::optional<uint8_t>(6));
	// past the last it goes round the town's slot, the spare one and the view before a jump, which are never stepped to
	EXPECT_EQ(bookmark_steps::Next(6, set), std::optional<uint8_t>(1));
	EXPECT_EQ(bookmark_steps::Next(bookmark_steps::k_TownSlot, set), std::optional<uint8_t>(1));
	EXPECT_EQ(bookmark_steps::Next(10, set), std::optional<uint8_t>(1));
	// the only one set is stepped to again from itself
	EXPECT_EQ(bookmark_steps::Next(4, SetAt({4})), std::optional<uint8_t>(4));
	EXPECT_EQ(bookmark_steps::Next(3, SetAt({})), std::nullopt);
}

TEST(BookmarkSteps, ShiftTabStepsToTheSetBookmarkBeforeButNeverToTheCurrentOne)
{
	const auto set = SetAt({1, 4, 6});
	EXPECT_EQ(bookmark_steps::Previous(6, set), std::optional<uint8_t>(4));
	EXPECT_EQ(bookmark_steps::Previous(4, set), std::optional<uint8_t>(1));
	EXPECT_EQ(bookmark_steps::Previous(1, set), std::optional<uint8_t>(6));
	EXPECT_EQ(bookmark_steps::Previous(10, set), std::optional<uint8_t>(6));
	EXPECT_EQ(bookmark_steps::Previous(0, set), std::optional<uint8_t>(6));
	EXPECT_EQ(bookmark_steps::Previous(4, SetAt({4})), std::nullopt);
}

TEST(BookmarkSteps, TabAloneGoesFromTheTailOfThePlayersTownsTowardsTheHead)
{
	const std::vector<Town> towns {{.id = 3, .hasPlace = true}, {.id = 7, .hasPlace = true}, {.id = 1, .hasPlace = true}};
	// the first Tab: from the tail, the town before it
	EXPECT_EQ(bookmark_steps::NextTown(towns, std::nullopt), std::optional<size_t>(1));
	// then on towards the head, and from the head to the tail
	EXPECT_EQ(bookmark_steps::NextTown(towns, 7u), std::optional<size_t>(0));
	EXPECT_EQ(bookmark_steps::NextTown(towns, 3u), std::optional<size_t>(2));
	EXPECT_EQ(bookmark_steps::NextTown(towns, 1u), std::optional<size_t>(1));
	// a number no town has any more starts again as the first Tab does
	EXPECT_EQ(bookmark_steps::NextTown(towns, 99u), std::optional<size_t>(1));
	// a single town is flown to every time
	EXPECT_EQ(bookmark_steps::NextTown(std::vector<Town> {{.id = 5, .hasPlace = true}}, std::nullopt),
	          std::optional<size_t>(0));
	EXPECT_EQ(bookmark_steps::NextTown(std::vector<Town> {}, std::nullopt), std::nullopt);
}

TEST(BookmarkSteps, TabAloneSkipsATownWithNoPlaceByWalkingOnFromTheTownBeforeIt)
{
	// from the tail: town 1 is the one before it, but it has no place, so the walk goes on from it: the town before
	// town 1 (the head) is taken
	const std::vector<Town> towns {{.id = 3, .hasPlace = true}, {.id = 7, .hasPlace = false}, {.id = 1, .hasPlace = true}};
	EXPECT_EQ(bookmark_steps::NextTown(towns, std::nullopt), std::optional<size_t>(0));
	// after the town flown to last, the one before it has no place and no other town has that number: the walk starts
	// again from the tail
	EXPECT_EQ(bookmark_steps::NextTown(towns, 1u), std::optional<size_t>(0));
	EXPECT_EQ(bookmark_steps::NextTown(std::vector<Town> {{.id = 2, .hasPlace = false}}, std::nullopt), std::nullopt);
}
