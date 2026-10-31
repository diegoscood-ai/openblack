/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ObjectOrder.h"

using namespace openblack::graphics::object_order;

namespace
{
entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}
} // namespace

TEST(ObjectOrder, EachRowTakesItsObjectsPlaceInTheList)
{
	const std::array visited {E(7), E(3), E(9)};
	const std::array rows {Row {.index = 0, .object = E(9)}, Row {.index = 1, .object = E(7)},
	                       Row {.index = 2, .object = E(3)}};
	const auto keys = RowKeys(visited, rows, 3);
	ASSERT_EQ(keys.size(), 3u);
	EXPECT_EQ(keys[0], (Key {.place = 2, .step = k_OwnStep}));
	EXPECT_EQ(keys[1], (Key {.place = 0, .step = k_OwnStep}));
	EXPECT_EQ(keys[2], (Key {.place = 1, .step = k_OwnStep}));
}

TEST(ObjectOrder, WhatTheListDidNotDrawComesAfterIt)
{
	const std::array visited {E(1)};
	// row 1 is an object the list left out, row 2 belongs to no object
	const std::array rows {Row {.index = 0, .object = E(1)}, Row {.index = 1, .object = E(5)}};
	const auto keys = RowKeys(visited, rows, 3);
	EXPECT_EQ(keys[1].band, Band::NotPlaced);
	EXPECT_EQ(keys[2].band, Band::NotPlaced);
	EXPECT_LT(keys[0], keys[1]);
}

TEST(ObjectOrder, TheTailComesAfterTheListAndThePhysicsObjectsAfterTheTail)
{
	const std::array visited {E(1)};
	const std::array tail {E(6), E(2)};
	const std::array physics {E(8)};
	const std::array rows {Row {.index = 0, .object = E(1)}, Row {.index = 1, .object = E(2)}, Row {.index = 2, .object = E(6)},
	                       Row {.index = 3, .object = E(8)}, Row {.index = 4, .object = E(5)}};
	const auto keys = RowKeys({.visited = visited, .tail = tail, .physics = physics}, rows, 5);
	EXPECT_EQ(keys[0], (Key {.band = Band::List, .place = 0, .step = k_OwnStep}));
	EXPECT_EQ(keys[1], (Key {.band = Band::Tail, .place = 1, .step = k_OwnStep}));
	EXPECT_EQ(keys[2], (Key {.band = Band::Tail, .place = 0, .step = k_OwnStep}));
	EXPECT_EQ(keys[3], (Key {.band = Band::AfterLeashes, .place = 0, .step = k_OwnStep}));
	// what no part claims stays between the list and its tail
	EXPECT_EQ(keys[4].band, Band::NotPlaced);
	EXPECT_LT(keys[0], keys[4]);
	EXPECT_LT(keys[4], keys[2]);
	EXPECT_LT(keys[2], keys[1]);
	EXPECT_LT(keys[1], keys[3]);
	// an object in two parts keeps the first
	const std::array both {E(1)};
	EXPECT_EQ(RowKeys({.visited = visited, .tail = both}, rows, 1)[0].band, Band::List);
}

TEST(ObjectOrder, TheTailIsTheRootsTheWalkersTheSoulsTheChestsThenTheMarks)
{
	const std::array roots {E(1), E(2)};
	const std::array walkers {E(3)};
	const std::array souls {E(5), E(4)};
	const std::array chests {E(6)};
	const std::array marks {E(8), E(7)};
	EXPECT_EQ(TailOrder(roots, walkers, souls, chests, marks), (std::vector {E(1), E(2), E(3), E(5), E(4), E(6), E(8), E(7)}));
	EXPECT_TRUE(TailOrder({}, {}, {}, {}, {}).empty());
}

TEST(ObjectOrder, ACopyDrawnInsideItsOwnersDrawGoesBeforeTheOwnersModel)
{
	// the totem's second plinth and icon, then its own plinth and icon, all in the totem's draw
	const std::array visited {E(4), E(2)};
	const std::array rows {Row {.index = 0, .object = E(2), .step = 0}, Row {.index = 1, .object = E(2), .step = 1},
	                       Row {.index = 2, .object = E(2)}, Row {.index = 3, .object = E(2), .step = 3},
	                       Row {.index = 4, .object = E(4)}};
	const auto keys = RowKeys(visited, rows, 5);
	EXPECT_LT(keys[0], keys[1]);
	EXPECT_LT(keys[1], keys[2]);
	EXPECT_LT(keys[2], keys[3]);
	// all after the object the list drew first
	EXPECT_LT(keys[4], keys[0]);
}

TEST(ObjectOrder, WithNoBlendedModelEveryRowIsInTheFirstSegment)
{
	// the draw as it was: one segment
	for (const auto key : {Key {.place = 0}, Key {.place = 40, .step = 3}, Key {.band = Band::NotPlaced}})
	{
		EXPECT_EQ(SegmentOf(key, {}), 0u);
	}
}

TEST(ObjectOrder, AnOpaqueModelDrawnAfterABlendedOneGoesInTheNextSegment)
{
	const std::array blended {Key {.place = 2}, Key {.place = 5}};
	EXPECT_EQ(SegmentOf({.place = 0}, blended), 0u);
	EXPECT_EQ(SegmentOf({.place = 1}, blended), 0u);
	EXPECT_EQ(SegmentOf({.place = 3}, blended), 1u);
	EXPECT_EQ(SegmentOf({.place = 4}, blended), 1u);
	EXPECT_EQ(SegmentOf({.place = 6}, blended), 2u);
	// the totem's solid model after its second one, at the same place
	const std::array ghost {Key {.place = 2, .step = 0}};
	EXPECT_EQ(SegmentOf({.place = 2, .step = k_OwnStep}, ghost), 1u);
	// at the same key the opaque one comes first, as everything the list did not draw comes before today's blended
	// models
	const std::array late {Key {.band = Band::NotPlaced}};
	EXPECT_EQ(SegmentOf({.band = Band::NotPlaced}, late), 0u);
}

TEST(ObjectOrder, OnlyAModelSortedByNeitherMeshNorObjectIsDrawnAtOnce)
{
	EXPECT_TRUE(DrawnAtOnce(false, false, false));
	EXPECT_FALSE(DrawnAtOnce(true, false, false));
	EXPECT_FALSE(DrawnAtOnce(false, true, false));
	EXPECT_FALSE(DrawnAtOnce(false, false, true));
}

TEST(ObjectOrder, AFlyingObjectLeavesTheListsDrawButStaysInTheQueue)
{
	// rows 0 and 1: a lying object; rows 2 and 3: a thrown orb (its bubble and its seed); row 4: a sunk one; row 5:
	// no object's
	const std::array rows {Row {.index = 0, .object = E(4)}, Row {.index = 1, .object = E(4)}, Row {.index = 2, .object = E(8)},
	                       Row {.index = 3, .object = E(8)}, Row {.index = 4, .object = E(6)}};
	const std::array physics {E(8)};
	const std::array sunk {E(6)};
	const auto split = SplitPhysics(rows, {}, 6, physics, sunk);
	EXPECT_EQ(split.list, (std::vector<uint8_t> {1, 1, 0, 0, 0, 1}));
	EXPECT_EQ(split.physics, (std::vector<uint8_t> {0, 0, 1, 1, 0, 0}));
	// the orb's bubble is queued in the depth sort by the physics objects' draw; the sunk one by neither
	EXPECT_EQ(split.queued, (std::vector<uint8_t> {1, 1, 1, 1, 0, 1}));
}

TEST(ObjectOrder, ARowLeftOutOfTheDrawStaysOutOfEveryPart)
{
	const std::array rows {Row {.index = 0, .object = E(4)}, Row {.index = 1, .object = E(8)},
	                       Row {.index = 2, .object = E(8)}};
	// the physics list unsorted
	const std::array physics {E(9), E(8), E(3)};
	const std::array<entt::entity, 0> sunk {};
	// row 1 left out (a citadel heart's, drawn in its own pass); every row past the mask's end drawn
	const std::array<uint8_t, 2> drawn {1, 0};
	const auto split = SplitPhysics(rows, drawn, 4, physics, sunk);
	EXPECT_EQ(split.list, (std::vector<uint8_t> {1, 0, 0, 1}));
	EXPECT_EQ(split.physics, (std::vector<uint8_t> {0, 0, 1, 0}));
	EXPECT_EQ(split.queued, (std::vector<uint8_t> {1, 0, 1, 1}));
	// an object in both lists is sunk: in neither draw
	const std::array both {E(8)};
	EXPECT_EQ(SplitPhysics(rows, {}, 3, both, both).queued, (std::vector<uint8_t> {1, 0, 0}));
}
