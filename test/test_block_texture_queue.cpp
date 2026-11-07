/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land's block texture queue (block_texture::TextureQueue), pinned to the original's own queue run natively: the
// distance bits, the sort and its ties, the six requests a frame with the nearest worked twice, all the strips at once
// at the highest detail, the prints after the last strip, and the one finished request that ends a frame's work. And
// the blocks' marks (block_texture::BlockMarks): a change before a block's first draw is painted at once, a later one
// is asked for by the block's next draw.

#include <cstdint>

#include <bit>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/BlockTextureQueue.h"

using namespace openblack::block_texture;

namespace
{

constexpr int32_t k_LowerDetail = 3;

/// The blocks and strips of the requests, the next to be worked first
std::vector<std::pair<uint16_t, int32_t>> State(const TextureQueue& queue)
{
	std::vector<std::pair<uint16_t, int32_t>> out;
	for (const auto& request : queue.Requests())
	{
		out.emplace_back(request.block, request.strip);
	}
	return out;
}

std::vector<Step> Frame(TextureQueue& queue, glm::vec3 eye, int32_t detailIndex)
{
	std::vector<Step> steps;
	queue.Update(eye, detailIndex, steps);
	return steps;
}

std::vector<Step> WholeBlock(uint16_t block)
{
	std::vector<Step> steps;
	for (int32_t strip = 0; strip < k_StripsPerBlock; ++strip)
	{
		steps.push_back({.kind = StepKind::Strip, .block = block, .strip = strip});
	}
	steps.push_back({.kind = StepKind::Prints, .block = block, .strip = k_StripsPerBlock});
	steps.push_back({.kind = StepKind::Finish, .block = block, .strip = k_StripsPerBlock + 2});
	return steps;
}

void ExpectSteps(const std::vector<Step>& actual, const std::vector<Step>& expected)
{
	ASSERT_EQ(actual.size(), expected.size());
	for (size_t i = 0; i < actual.size(); ++i)
	{
		EXPECT_EQ(actual[i].kind, expected[i].kind) << i;
		EXPECT_EQ(actual[i].block, expected[i].block) << i;
		if (expected[i].kind == StepKind::Strip)
		{
			EXPECT_EQ(actual[i].strip, expected[i].strip) << i;
		}
	}
}

} // namespace

TEST(BlockTextureQueue, DistanceMatchesTheOriginalBitForBit)
{
	const glm::vec3 eye {1234.567f, 345.25f, 987.125f};
	EXPECT_EQ(std::bit_cast<uint32_t>(RequestDistance({80.0f, 0.0f, 80.0f}, eye)), 0x44BC8AF9u);
	EXPECT_EQ(std::bit_cast<uint32_t>(RequestDistance({1040.0f, 0.0f, 2960.0f}, eye)), 0x44FB891Cu);
	EXPECT_EQ(std::bit_cast<uint32_t>(RequestDistance({5040.0f, 0.0f, 80.0f}, eye)), 0x45757435u);
	const glm::vec3 near {100.0f, 50.0f, 90.0f};
	EXPECT_EQ(std::bit_cast<uint32_t>(RequestDistance({240.0f, 0.0f, 80.0f}, near)), 0x4314FF24u);
	EXPECT_EQ(std::bit_cast<uint32_t>(RequestDistance({400.0f, 0.0f, 80.0f}, near)), 0x439826B8u);
}

TEST(BlockTextureQueue, ANewRequestGoesInFrontAndAnAskedOneStartsAgainInItsPlace)
{
	TextureQueue queue;
	queue.Request(4, {320.0f, 1600.0f});
	ASSERT_EQ(queue.Requests().size(), 1u);
	EXPECT_EQ(queue.Requests()[0].centre, glm::vec3(400.0f, 0.0f, 1680.0f));
	EXPECT_EQ(queue.Requests()[0].strip, 0);
	std::ignore = Frame(queue, {}, k_LowerDetail);
	EXPECT_EQ(queue.Requests()[0].strip, 2);
	queue.Request(9, {0.0f, 0.0f});
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{9, 0}, {4, 2}}));
	EXPECT_EQ(queue.Requests()[0].centre, glm::vec3(80.0f, 0.0f, 80.0f));
	queue.Request(4, {320.0f, 1600.0f});
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{9, 0}, {4, 0}}));
	EXPECT_TRUE(queue.Has(4));
	EXPECT_FALSE(queue.Has(5));
	queue.Clear();
	EXPECT_TRUE(queue.Requests().empty());
}

TEST(BlockTextureQueue, AtTheHighestDetailTheNearestIsPaintedWholeAndFinishesAlone)
{
	TextureQueue queue;
	// asked for in this order, so the queue holds blocks 3, 1, 2
	queue.Request(2, {160.0f, 0.0f});
	queue.Request(1, {0.0f, 0.0f});
	queue.Request(3, {320.0f, 0.0f});
	const glm::vec3 eye {100.0f, 50.0f, 90.0f};
	ExpectSteps(Frame(queue, eye, k_AllStripsDetailIndex), WholeBlock(1));
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{2, 0}, {3, 0}}));
	EXPECT_EQ(std::bit_cast<uint32_t>(queue.Requests()[0].distance), 0x4314FF24u);
	ExpectSteps(Frame(queue, eye, k_AllStripsDetailIndex), WholeBlock(2));
	ExpectSteps(Frame(queue, eye, k_AllStripsDetailIndex), WholeBlock(3));
	EXPECT_TRUE(queue.Requests().empty());
	EXPECT_TRUE(Frame(queue, eye, k_AllStripsDetailIndex).empty());
}

TEST(BlockTextureQueue, BelowTheHighestDetailSixAreWorkedAndTheNearestTwice)
{
	TextureQueue queue;
	for (uint16_t block = 8; block >= 1; --block)
	{
		queue.Request(block, {160.0f * block, 0.0f});
	}
	const glm::vec3 eye {0.0f, 30.0f, 80.0f};
	ExpectSteps(Frame(queue, eye, k_LowerDetail), {{StepKind::Strip, 1, 0},
	                                               {StepKind::Strip, 1, 1},
	                                               {StepKind::Strip, 2, 0},
	                                               {StepKind::Strip, 3, 0},
	                                               {StepKind::Strip, 4, 0},
	                                               {StepKind::Strip, 5, 0},
	                                               {StepKind::Strip, 6, 0}});
	for (int frame = 2; frame <= 8; ++frame)
	{
		std::ignore = Frame(queue, eye, k_LowerDetail);
	}
	EXPECT_EQ(State(queue),
	          (std::vector<std::pair<uint16_t, int32_t>> {{1, 16}, {2, 8}, {3, 8}, {4, 8}, {5, 8}, {6, 8}, {7, 0}, {8, 0}}));
	// the ninth frame puts in the prints and finishes the first, and works nothing else
	ExpectSteps(Frame(queue, eye, k_LowerDetail), {{StepKind::Prints, 1, 16}, {StepKind::Finish, 1, 18}});
	ExpectSteps(Frame(queue, eye, k_LowerDetail), {{StepKind::Strip, 2, 8},
	                                               {StepKind::Strip, 2, 9},
	                                               {StepKind::Strip, 3, 8},
	                                               {StepKind::Strip, 4, 8},
	                                               {StepKind::Strip, 5, 8},
	                                               {StepKind::Strip, 6, 8},
	                                               {StepKind::Strip, 7, 0}});
	EXPECT_EQ(State(queue),
	          (std::vector<std::pair<uint16_t, int32_t>> {{2, 10}, {3, 9}, {4, 9}, {5, 9}, {6, 9}, {7, 1}, {8, 0}}));
	std::ignore = Frame(queue, eye, k_LowerDetail);
	EXPECT_EQ(State(queue),
	          (std::vector<std::pair<uint16_t, int32_t>> {{2, 12}, {3, 10}, {4, 10}, {5, 10}, {6, 10}, {7, 2}, {8, 0}}));
}

TEST(BlockTextureQueue, EqualDistancesKeepTheirOrder)
{
	const glm::vec3 eye {80.0f, 0.0f, 80.0f};
	TextureQueue queue;
	queue.Request(7, {0.0f, 0.0f});
	queue.Request(6, {160.0f, 0.0f});
	queue.Request(5, {0.0f, 160.0f});
	ExpectSteps(Frame(queue, eye, k_AllStripsDetailIndex), WholeBlock(7));
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{5, 0}, {6, 0}}));
	EXPECT_EQ(std::bit_cast<uint32_t>(queue.Requests()[0].distance), 0x43200000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(queue.Requests()[1].distance), 0x43200000u);

	TextureQueue swapped;
	swapped.Request(7, {0.0f, 0.0f});
	swapped.Request(5, {0.0f, 160.0f});
	swapped.Request(6, {160.0f, 0.0f});
	std::ignore = Frame(swapped, eye, k_AllStripsDetailIndex);
	EXPECT_EQ(State(swapped), (std::vector<std::pair<uint16_t, int32_t>> {{6, 0}, {5, 0}}));
}

TEST(BlockTextureQueue, TheHighestDetailPartWayPaintsTheRestAndBeyondWithoutPrints)
{
	TextureQueue queue;
	queue.Request(2, {160.0f, 0.0f});
	queue.Request(1, {0.0f, 0.0f});
	const glm::vec3 eye {};
	for (int frame = 1; frame <= 5; ++frame)
	{
		std::ignore = Frame(queue, eye, k_LowerDetail);
	}
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{1, 10}, {2, 5}}));
	std::vector<Step> rest;
	for (int32_t strip = 10; strip < 10 + k_StripsPerBlock; ++strip)
	{
		rest.push_back({.kind = StepKind::Strip, .block = 1, .strip = strip});
	}
	rest.push_back({.kind = StepKind::Finish, .block = 1, .strip = 28});
	ExpectSteps(Frame(queue, eye, k_AllStripsDetailIndex), rest);
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{2, 5}}));
}

TEST(BlockTextureQueue, AFinishedRequestEndsTheFramesWork)
{
	TextureQueue queue;
	queue.Request(2, {160.0f, 0.0f});
	const glm::vec3 eye {};
	for (int frame = 1; frame <= 8; ++frame)
	{
		std::ignore = Frame(queue, eye, k_LowerDetail);
	}
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{2, 16}}));
	queue.Request(3, {320.0f, 0.0f});
	queue.Request(1, {0.0f, 0.0f});
	ExpectSteps(Frame(queue, eye, k_LowerDetail),
	            {{StepKind::Strip, 1, 0}, {StepKind::Strip, 1, 1}, {StepKind::Prints, 2, 16}, {StepKind::Finish, 2, 18}});
	EXPECT_EQ(State(queue), (std::vector<std::pair<uint16_t, int32_t>> {{1, 2}, {3, 0}}));
}

TEST(BlockTextureMarks, AChangeBeforeTheFirstDrawIsPaintedAtOnceAndOneAfterItWaitsForTheDraw)
{
	BlockMarks marks;
	marks.Reset(3);
	// nothing is out of date as the land opens
	EXPECT_FALSE(marks.TakePaintNow(0));
	EXPECT_FALSE(marks.TakeDrawn(0));
	// block 1 changes before it is first drawn: painted at once, and its draw asks for nothing
	marks.Changed(1);
	EXPECT_TRUE(marks.TakePaintNow(1));
	EXPECT_FALSE(marks.TakePaintNow(1));
	EXPECT_FALSE(marks.TakeDrawn(1));
	// once drawn, a change waits for the next draw, which asks for it once
	marks.Changed(1);
	marks.Changed(1);
	EXPECT_FALSE(marks.TakePaintNow(1));
	EXPECT_TRUE(marks.TakeDrawn(1));
	EXPECT_FALSE(marks.TakeDrawn(1));
	// every block out of date: block 0 (drawn) and block 2 (not drawn) are each asked for by their next draw
	std::ignore = marks.TakeDrawn(0);
	marks.AllOutOfDate();
	EXPECT_TRUE(marks.TakeDrawn(0));
	EXPECT_TRUE(marks.TakeDrawn(2));
	// blocks past the land's are left alone
	marks.Changed(7);
	EXPECT_FALSE(marks.TakePaintNow(7));
	EXPECT_FALSE(marks.TakeDrawn(7));
}

namespace
{

/// A rectangle of block places, both ends in
using Places = std::pair<glm::ivec2, glm::ivec2>;

Places Marked(const MarkingObject& object)
{
	const auto range = BlocksMarkedBy(object);
	EXPECT_TRUE(range.has_value());
	return range.has_value() ? Places {range->min, range->max} : Places {};
}

} // namespace

// The blocks an object marks out of date, pinned to the original's own rule run natively on fake objects (the blocks
// it marked, within a window of 15 x 17 block places)
TEST(BlockTextureMarkedBlocks, AHouseMarksTheBlocksItsShadowAndFootprintReach)
{
	const std::vector<glm::vec2> square {{-8.0f, -8.0f}, {8.0f, -8.0f}, {8.0f, 8.0f},
	                                     {-8.0f, -8.0f}, {8.0f, 8.0f},  {-8.0f, 8.0f}};
	// its shadow, cast 12 m along the sun from the top of its box, reaches the next block on x and on z
	EXPECT_EQ(Marked({.position = {318.0f, 641.5f},
	                  .shadow = true,
	                  .footprint = true,
	                  .boxCentre = {0.5f, 6.0f, -1.0f},
	                  .boxHalfSize = {5.0f, 6.0f, 4.0f},
	                  .footprintCorners = square}),
	          Places({1, 3}, {2, 4}));
	const std::vector<glm::vec2> big {{-50.0f, -40.0f}, {50.0f, -40.0f}, {50.0f, 40.0f},
	                                  {-50.0f, -40.0f}, {50.0f, 40.0f},  {-50.0f, 40.0f}};
	EXPECT_EQ(Marked({.position = {1200.0f, 1200.0f},
	                  .yAngle = 1.0f,
	                  .scale = 3.0f,
	                  .shadow = true,
	                  .footprint = true,
	                  .boxCentre = {0.0f, 20.0f, 5.0f},
	                  .boxHalfSize = {40.0f, 20.0f, 30.0f},
	                  .footprintCorners = big}),
	          Places({6, 6}, {9, 9}));
	const std::vector<glm::vec2> triangle {{-15.0f, -10.0f}, {15.0f, -10.0f}, {15.0f, 10.0f}};
	EXPECT_EQ(Marked({.position = {1000.25f, 1500.75f},
	                  .yAngle = 0.7853982f,
	                  .scale = 1.4f,
	                  .shadow = true,
	                  .footprint = true,
	                  .boxCentre = {0.0f, 10.0f, 0.0f},
	                  .boxHalfSize = {12.0f, 10.0f, 7.0f},
	                  .footprintCorners = triangle}),
	          Places({6, 9}, {6, 9}));
}

TEST(BlockTextureMarkedBlocks, AShadowAloneOrAFootprintAloneCounts)
{
	// a tall thing's shadow reaches the next block along the sun
	EXPECT_EQ(Marked({.position = {150.0f, 2000.0f},
	                  .yAngle = 2.5f,
	                  .scale = 0.8f,
	                  .shadow = true,
	                  .boxCentre = {0.0f, 30.0f, 0.0f},
	                  .boxHalfSize = {3.0f, 30.0f, 3.0f}}),
	          Places({0, 12}, {1, 12}));
	// a building site: its footprint and not its box
	const std::vector<glm::vec2> small {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}};
	EXPECT_EQ(Marked({.position = {479.9f, 159.99f},
	                  .yAngle = -1.2f,
	                  .footprint = true,
	                  .boxCentre = {0.0f, 4.0f, 0.0f},
	                  .boxHalfSize = {6.0f, 4.0f, 6.0f},
	                  .footprintCorners = small}),
	          Places({2, 0}, {3, 1}));
	// a footprint with no triangles: only its place
	EXPECT_EQ(Marked({.position = {800.0f, 800.0f},
	                  .yAngle = 0.3f,
	                  .footprint = true,
	                  .boxCentre = {0.0f, 4.0f, 0.0f},
	                  .boxHalfSize = {60.0f, 4.0f, 60.0f}}),
	          Places({5, 5}, {5, 5}));
	// neither: nothing is marked
	EXPECT_FALSE(
	    BlocksMarkedBy({.position = {500.0f, 500.0f}, .boxCentre = {}, .boxHalfSize = {100.0f, 100.0f, 100.0f}}).has_value());
}

TEST(BlockTextureMarkedBlocks, TheSidesAreTruncatedAndClampedAsTheOriginalDoes)
{
	// a corner a hair past a block's side, toward zero, stays in the block before
	const std::vector<glm::vec2> edge {{-0.0001f, 0.0f}, {0.0f, -0.0001f}, {159.99f, 0.0f}};
	EXPECT_EQ(Marked({.position = {320.0f, 480.0f}, .footprint = true, .footprintCorners = edge}), Places({1, 2}, {2, 3}));
	const std::vector<glm::vec2> point {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
	EXPECT_EQ(Marked({.position = {159.99998f, 319.99997f}, .footprint = true, .footprintCorners = point}),
	          Places({0, 1}, {0, 1}));
	// below the map: the lower sides become 0
	const std::vector<glm::vec2> nearEdge {{-10.0f, -10.0f}, {10.0f, -10.0f}, {0.0f, 10.0f}};
	EXPECT_EQ(Marked({.position = {5.0f, 3.0f},
	                  .yAngle = 3.0f,
	                  .scale = 2.0f,
	                  .shadow = true,
	                  .footprint = true,
	                  .boxCentre = {1.0f, 2.0f, 1.0f},
	                  .boxHalfSize = {4.0f, 2.0f, 4.0f},
	                  .footprintCorners = nearEdge}),
	          Places({0, 0}, {0, 0}));
	// wholly at a negative x: the lower side becomes 0 and the upper one, negative, the last place, so the whole row
	// of blocks is marked
	const std::vector<glm::vec2> around {{-5.0f, -5.0f}, {5.0f, 5.0f}, {0.0f, 5.0f}};
	EXPECT_EQ(Marked({.position = {-500.0f, 330.0f}, .footprint = true, .footprintCorners = around}),
	          Places({0, 2}, {k_LastBlockPlace, 2}));
}

