/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/SpecialVillagers.h"

using namespace openblack;
using namespace openblack::ecs::special_villager;

namespace
{
std::array<Row, k_NameCount> AlternatingRows()
{
	std::array<Row, k_NameCount> rows {};
	for (size_t i = 0; i < rows.size(); ++i)
	{
		rows.at(i) = {.sex = i % 2 == 0 ? SexType::Male : SexType::Female, .hasName = true};
	}
	return rows;
}
} // namespace

TEST(SpecialVillagers, TwoInTenTryForAName)
{
	EXPECT_TRUE(TriesNamed(0));
	EXPECT_TRUE(TriesNamed(1));
	EXPECT_FALSE(TriesNamed(2));
	EXPECT_FALSE(TriesNamed(9));
}

TEST(SpecialVillagers, PickWalksFromTheDrawToTheFirstFreeNameOfTheSex)
{
	const auto rows = AlternatingRows();
	std::array<bool, k_NameCount> used {};
	EXPECT_EQ(PickName(rows, used, SexType::Male, 4), 4);
	EXPECT_EQ(PickName(rows, used, SexType::Female, 4), 5);
	used.at(5) = true;
	EXPECT_EQ(PickName(rows, used, SexType::Female, 4), 7);
	// Round the end of the table
	EXPECT_EQ(PickName(rows, used, SexType::Male, 47), 0);
	// Any sex
	EXPECT_EQ(PickName(rows, used, SexType::None, 5), 6);
}

TEST(SpecialVillagers, NoNameWhenEveryOneOfTheSexIsGiven)
{
	auto rows = AlternatingRows();
	std::array<bool, k_NameCount> used {};
	for (size_t i = 1; i < used.size(); i += 2)
	{
		used.at(i) = true;
	}
	EXPECT_FALSE(PickName(rows, used, SexType::Female, 0).has_value());
	// Rows without a name are passed over
	rows.at(2).hasName = false;
	EXPECT_EQ(PickName(rows, used, SexType::Male, 2), 4);
}

TEST(SpecialVillagers, NamedVillagerTakesItsNamesAgeUnlessBorn)
{
	EXPECT_EQ(NamedAge(25, 40), 40u);
	EXPECT_EQ(NamedAge(1, 40), 1u);
}

// The original's own pick, run on the same tables: the row it chose from the place it drew (6, 11, 34 and 18, the
// draws below 48 from four seeds), for a man, a woman and any sex
TEST(SpecialVillagers, PickMatchesTheOriginal)
{
	const auto rows = AlternatingRows();
	const std::array<bool, k_NameCount> none {};
	struct Case
	{
		uint32_t start;
		SexType sex;
		uint8_t row;
	};
	constexpr std::array k_Cases {
	    Case {6, SexType::Male, 6},   Case {6, SexType::Female, 7},   Case {6, SexType::None, 6},
	    Case {11, SexType::Male, 12}, Case {11, SexType::Female, 11}, Case {11, SexType::None, 11},
	    Case {34, SexType::Male, 34}, Case {34, SexType::Female, 35}, Case {34, SexType::None, 34},
	    Case {18, SexType::Male, 18}, Case {18, SexType::Female, 19}, Case {18, SexType::None, 18},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(PickName(rows, none, c.sex, c.start), std::optional<uint8_t>(c.row)) << c.start;
	}
	// every man's name given but row 46's
	std::array<bool, k_NameCount> used {};
	for (size_t i = 0; i < used.size(); i += 2)
	{
		used.at(i) = i != 46;
	}
	EXPECT_EQ(PickName(rows, used, SexType::Male, 6), 46);
	EXPECT_EQ(PickName(rows, used, SexType::Male, 11), 46);
	// only row 2 free for a man: from 34 round past the end
	for (size_t i = 0; i < used.size(); i += 2)
	{
		used.at(i) = i != 2;
	}
	EXPECT_EQ(PickName(rows, used, SexType::Male, 34), 2);
	// none free
	used.at(2) = true;
	EXPECT_FALSE(PickName(rows, used, SexType::Male, 6).has_value());
	// the first 40 rows with no name, all men
	std::array<Row, k_NameCount> men {};
	for (size_t i = 0; i < men.size(); ++i)
	{
		men.at(i) = {.sex = SexType::Male, .hasName = i >= 40};
	}
	EXPECT_EQ(PickName(men, none, SexType::Male, 6), 40);
	EXPECT_EQ(PickName(men, none, SexType::Male, 11), 40);
}
