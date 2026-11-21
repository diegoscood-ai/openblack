/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The prints that things give the land without being in the map's cells, as the holder of a singing stone gives one:
// where a print lies, when the land takes it and the order the land paints them in
// (docs/wiki/world/terrain.md#the-prints-the-16th-step)

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/LandFootprintRules.h"

using namespace openblack::ecs::land_footprints;

TEST(LandFootprintRules, AGroundPrintLiesAtTheMapPlaceOfItsThingWithNoTurn)
{
	// 1.0001 m is 6554 units of the map's fixed point, which is 1.00006104 m; the height is kept
	const auto placement = GroundPrintPlacement({1.0001f, -20.7f, 2.5f});
	EXPECT_EQ(std::bit_cast<uint32_t>(placement[9]), 0x3F800200u);
	EXPECT_EQ(placement[10], -20.7f);
	EXPECT_EQ(placement[11], 2.5f);
	const std::array<float, 9> axes {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	for (size_t i = 0; i < axes.size(); ++i)
	{
		EXPECT_EQ(placement.at(i), axes.at(i)) << i;
	}
}

TEST(LandFootprintRules, GroundPrintsArePaintedNewestFirst)
{
	const std::vector<GroundPrintThing> things {
	    {.mesh = 7, .placement = GroundPrintPlacement({10.0f, 0.0f, 10.0f}), .made = 1},
	    {.mesh = 7, .placement = GroundPrintPlacement({20.0f, 0.0f, 10.0f}), .made = 0},
	    {.mesh = 7, .placement = GroundPrintPlacement({30.0f, 0.0f, 10.0f}), .made = 2},
	};
	EXPECT_EQ(GroundPrintOrder(things), (std::vector<size_t> {2, 0, 1}));
}

TEST(LandFootprintRules, APrintTheLandDidNotTakeIsNeverPainted)
{
	const std::vector<GroundPrintThing> things {
	    {.mesh = 7, .placement = GroundPrintPlacement({10.0f, 0.0f, 10.0f}), .made = 0, .printed = false},
	    {.mesh = 7, .placement = GroundPrintPlacement({20.0f, 0.0f, 10.0f}), .made = 1},
	};
	EXPECT_EQ(GroundPrintOrder(things), (std::vector<size_t> {1}));
}

TEST(LandFootprintRules, TheLandTakesNoSecondPrintOfAModelAtThePlaceOfAnother)
{
	const auto at = GroundPrintPlacement({10.0f, 3.0f, 10.0f});
	const std::vector<GroundPrintThing> things {{.mesh = 7, .placement = at, .made = 0}};
	// The same model with the same placement: not taken
	EXPECT_FALSE(IsNewGroundPrint(things, 7, at));
	// Another model at the same place, or the same model at another height: taken
	EXPECT_TRUE(IsNewGroundPrint(things, 8, at));
	EXPECT_TRUE(IsNewGroundPrint(things, 7, GroundPrintPlacement({10.0f, 5.0f, 10.0f})));
	// A print the land did not take does not keep a new one out
	const std::vector<GroundPrintThing> left {{.mesh = 7, .placement = at, .made = 0, .printed = false}};
	EXPECT_TRUE(IsNewGroundPrint(left, 7, at));
}

TEST(LandFootprintRules, APrintsBoxIsItsCornersAtItsPlace)
{
	const std::vector<glm::vec2> corners {{-2.0f, 1.0f}, {3.0f, -4.0f}, {0.5f, 6.0f}};
	const auto box = GroundPrintBox(corners, GroundPrintPlacement({100.0f, 0.0f, 200.0f}));
	EXPECT_EQ(box, glm::vec4(98.0f, 196.0f, 103.0f, 206.0f));
	// No corners: the box stays at nothing, wherever the thing is
	EXPECT_EQ(GroundPrintBox({}, GroundPrintPlacement({100.0f, 0.0f, 200.0f})), glm::vec4(0.0f));
}

TEST(LandFootprintRules, APrintMarksTheBlocksItsBoxReaches)
{
	// 160 m to a block, each side truncated toward zero
	const auto [low, high] = GroundPrintBlocks({159.0f, 320.0f, 481.0f, 639.9f});
	EXPECT_EQ(low, glm::ivec2(0, 2));
	EXPECT_EQ(high, glm::ivec2(3, 3));
	const auto [lowEdge, highEdge] = GroundPrintBlocks({-10.0f, -170.0f, 10.0f, 10.0f});
	EXPECT_EQ(lowEdge, glm::ivec2(0, -1));
	EXPECT_EQ(highEdge, glm::ivec2(0, 0));
}
