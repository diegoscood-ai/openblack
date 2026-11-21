/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <bit>
#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureCaveTrophies.h"

#include "../oracle/OracleData.h"

using namespace openblack::CreatureCaveTrophies;

namespace
{
std::vector<Trophy> Belts(const std::vector<Trophy>& trophies)
{
	std::vector<Trophy> belts;
	std::ranges::copy_if(trophies, std::back_inserter(belts), [](const Trophy& trophy) { return !trophy.medal; });
	return belts;
}
} // namespace

TEST(CreatureCaveTrophies, IconNames)
{
	EXPECT_EQ(IconName(0), "I_BELT_WHITE_01");
	EXPECT_EQ(IconName(27), "I_BELT_BLACK_04");
	EXPECT_EQ(IconName(28), "I_MEDAL_WOOD01");
	EXPECT_EQ(IconName(33), "I_MEDAL_BRONZE01");
	EXPECT_EQ(IconName(52), "I_MEDAL_GEM05");
}

TEST(CreatureCaveTrophies, AnEvenBalanceFillsBothRowsAlike)
{
	// Half of 34 is 17: white, yellow and blue whole, and two of green
	const auto belts = Belts(Choose(0.0f, {}));
	ASSERT_EQ(belts.size(), 8u);
	EXPECT_EQ(belts[0].point, 16u);
	EXPECT_EQ(belts[0].icon, 3u);
	EXPECT_EQ(belts[3].point, 19u);
	EXPECT_EQ(belts[3].icon, 3u * 4u + 1u);
	EXPECT_EQ(belts[4].point, 23u);
}

TEST(CreatureCaveTrophies, AFullBalanceFillsOneRow)
{
	const auto belts = Belts(Choose(1.0f, {}));
	ASSERT_EQ(belts.size(), 7u);
	EXPECT_EQ(belts.back().point, 22u);
	// 34 less 30 for black leaves four
	EXPECT_EQ(belts.back().icon, 27u);
}

TEST(CreatureCaveTrophies, MedalsByLearning)
{
	MiracleLearning learning;
	learning.overall = 50.0f;
	learning.best = {100.0f, 39.0f, 3.0f, 0.0f};
	auto trophies = Choose(0.0f, learning);
	std::erase_if(trophies, [](const Trophy& trophy) { return !trophy.medal; });
	// 3% is under a level, and none for nothing learnt
	ASSERT_EQ(trophies.size(), 3u);
	EXPECT_EQ(trophies[0].point, 6u);
	EXPECT_EQ(IconName(trophies[0].icon), "I_MEDAL_SILVER02");
	EXPECT_EQ(trophies[1].point, 8u);
	EXPECT_EQ(IconName(trophies[1].icon), "I_MEDAL_GEM05");
	EXPECT_EQ(trophies[2].point, 10u);
	EXPECT_EQ(IconName(trophies[2].icon), "I_MEDAL_BRONZE04");
}

TEST(CreatureCaveTrophies, BeltsAndMedalsPastWoodShine)
{
	MiracleLearning learning;
	// 20% is the last of wood, 24% the first of bronze
	learning.overall = 20.0f;
	learning.best = {24.0f, 0.0f, 0.0f, 0.0f};
	const auto trophies = Choose(0.0f, learning);
	for (const auto& trophy : trophies)
	{
		if (!trophy.medal)
		{
			EXPECT_TRUE(trophy.environmentMapped);
		}
		else if (trophy.point == 6)
		{
			EXPECT_EQ(IconName(trophy.icon), "I_MEDAL_WOOD05");
			EXPECT_FALSE(trophy.environmentMapped);
		}
		else
		{
			EXPECT_EQ(IconName(trophy.icon), "I_MEDAL_BRONZE01");
			EXPECT_TRUE(trophy.environmentMapped);
		}
	}
}

TEST(CreatureCaveTrophies, LearningOfTheMiracles)
{
	const std::array<int32_t, 5> percents {12, 97, 9, 100, 30};
	const auto learning = LearningOf(percents);
	EXPECT_FLOAT_EQ(learning.overall, 248.0f / 42.0f);
	EXPECT_EQ(learning.best, (std::array<float, 4> {100.0f, 97.0f, 30.0f, 12.0f}));
}

// Where the creature's room places a trophy at a point, run by the oracle on points of every kind: the axes made unit
// long by the 3D engine's inverse square root and turned a quarter back, bit for bit
TEST(CreatureCaveTrophies, ATrophyIsPlacedAtItsPointAsTheOriginalPlacesIt)
{
	const auto table = openblack::oracle::Table::Load("trophy_place.txt");
	ASSERT_TRUE(table.has_value());
	const auto value = [&table](size_t row, const std::string& name) {
		const auto column = table->Column(name);
		EXPECT_TRUE(column.has_value()) << name;
		return table->Float(row, column.value_or(0));
	};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto axis = [&](int first) {
			return glm::vec4(value(row, "m" + std::to_string(first)), value(row, "m" + std::to_string(first + 1)),
			                 value(row, "m" + std::to_string(first + 2)), 0.0f);
		};
		const glm::mat4 point {axis(0), axis(3), axis(6), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)};
		const auto place = openblack::CreatureCaveTrophies::PointPlacement(point);
		bool same = true;
		for (glm::length_t r = 0; r < 3; ++r)
		{
			for (glm::length_t c = 0; c < 3; ++c)
			{
				same = same && std::bit_cast<uint32_t>(place[r][c]) ==
				                   std::bit_cast<uint32_t>(value(row, "r" + std::to_string((3 * r) + c)));
			}
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row;
		}
	}
	EXPECT_EQ(different, 0u);
}
