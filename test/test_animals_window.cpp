/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/AnimalsModel.h"

using namespace openblack;
using namespace openblack::debug::animals;

TEST(AnimalsWindow, noAnimalNoneNearest)
{
	EXPECT_FALSE(Nearest({}, glm::vec3(0.0f)).has_value());
}

TEST(AnimalsWindow, nearestOverTheLandLeavesHeightOut)
{
	const std::vector<AnimalAt> animals {
	    {.entity = entt::entity {3}, .position = {10.0f, 0.0f, 10.0f}},
	    // nearer over the land, though far above the point
	    {.entity = entt::entity {7}, .position = {2.0f, 500.0f, 1.0f}},
	    {.entity = entt::entity {9}, .position = {-6.0f, 0.0f, 0.0f}},
	};
	EXPECT_EQ(Nearest(animals, glm::vec3(0.0f)), entt::entity {7});
	EXPECT_EQ(Nearest(animals, glm::vec3(9.0f, 0.0f, 9.0f)), entt::entity {3});
}

TEST(AnimalsWindow, nearestTieKeepsTheFirst)
{
	const std::vector<AnimalAt> animals {
	    {.entity = entt::entity {5}, .position = {1.0f, 0.0f, 0.0f}},
	    {.entity = entt::entity {2}, .position = {-1.0f, 0.0f, 0.0f}},
	};
	EXPECT_EQ(Nearest(animals, glm::vec3(0.0f)), entt::entity {5});
}

TEST(AnimalsWindow, speciesNames)
{
	EXPECT_EQ(SpeciesName(AnimalInfo::Lion), "lion");
	EXPECT_EQ(SpeciesName(AnimalInfo::Cow), "cow");
	EXPECT_EQ(SpeciesName(AnimalInfo::PuzzlePig), "puzzle pig");
	EXPECT_EQ(SpeciesName(AnimalInfo::None), "unknown");
	EXPECT_EQ(SpeciesName(AnimalInfo::_COUNT), "unknown");
}

TEST(AnimalsWindow, stateLabelEndsTheNameAtItsNull)
{
	std::array<char, 16> name {};
	EXPECT_EQ(StateLabel(43, name), "43");
	const std::string_view wander = "WANDER";
	std::ranges::copy(wander, name.begin());
	EXPECT_EQ(StateLabel(32, name), "32 WANDER");
}

TEST(AnimalsWindow, headingInDegrees)
{
	EXPECT_FLOAT_EQ(HeadingDegrees(0), 0.0f);
	EXPECT_FLOAT_EQ(HeadingDegrees(512), 90.0f);
	EXPECT_FLOAT_EQ(HeadingDegrees(1024), 180.0f);
}
