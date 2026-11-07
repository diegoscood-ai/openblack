/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers' table in the game's info.dat, which the villagers are made from: nine tribes of seven villagers each
// (a housewife, then six jobs), each tribe with its own models but the Africans, who wear the Aztecs'; the housewives
// women and every other job a man; then the twenty-one special kinds. Integration tests: they need the original's data
// (OPENBLACK_GAME_PATH) and skip without it, as the facts are the data's own and no synthetic table could stand for
// them.
// docs/wiki/project/progress/villager/life_cycle.md, special_villagers.md

#include <cstddef>

#include <set>

#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "support/GameInfoDat.h"

using namespace openblack;

namespace
{
constexpr size_t k_Tribes = 9;
constexpr size_t k_PerTribe = 7;
constexpr size_t k_FirstSpecial = k_Tribes * k_PerTribe;
constexpr size_t k_Specials = 21;

std::unique_ptr<InfoConstants> Info()
{
	const auto game = test::GamePath();
	return game.has_value() ? test::ReadInfoDat(*game) : nullptr;
}
} // namespace

// Nine tribes, each of seven villagers in a row of the table, each tribe with models no other tribe's villagers share,
// but the African tribe, whose rows give the Aztecs' models: the game has no African villager model
TEST(VillagerTableData, NineTribesOfSevenAndTheAfricansWearTheAztecsModels)
{
	const auto info = Info();
	if (info == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no info.dat there: the table is the game's own data";
	}
	const auto models = [&info](size_t tribe) {
		std::set<MeshId> own;
		for (size_t job = 0; job < k_PerTribe; ++job)
		{
			const auto& row = info->villager.at(tribe * k_PerTribe + job);
			EXPECT_EQ(row.tribeType, static_cast<Tribe>(tribe)) << tribe << " " << job;
			own.insert(row.stdDetail);
		}
		return own;
	};
	const auto african = static_cast<size_t>(Tribe::AFRICAN);
	const auto aztec = static_cast<size_t>(Tribe::AZTEC);
	EXPECT_EQ(models(african), models(aztec));
	EXPECT_TRUE(models(aztec).contains(MeshId::PersonAztecMaleA2));
	std::set<MeshId> seen;
	for (size_t tribe = 0; tribe < k_Tribes; ++tribe)
	{
		if (tribe == african)
		{
			continue;
		}
		for (const auto mesh : models(tribe))
		{
			EXPECT_FALSE(seen.contains(mesh)) << "tribe " << tribe << " shares a model";
			seen.insert(mesh);
		}
	}
}

// The first villager of each tribe, the housewife, is a woman, and every other job a man
TEST(VillagerTableData, HousewivesAreWomenAndTheOtherJobsMen)
{
	const auto info = Info();
	if (info == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no info.dat there: the table is the game's own data";
	}
	for (size_t row = 0; row < k_FirstSpecial; ++row)
	{
		EXPECT_EQ(info->villager.at(row).sex, row % k_PerTribe == 0 ? SexType::Female : SexType::Male) << row;
	}
}

// After the tribes' rows the table holds the twenty-one special kinds, and nothing more
TEST(VillagerTableData, TwentyOneSpecialKindsFollowTheTribes)
{
	const auto info = Info();
	if (info == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no info.dat there: the table is the game's own data";
	}
	ASSERT_EQ(info->villager.size(), k_FirstSpecial + k_Specials);
	EXPECT_EQ(static_cast<size_t>(VillagerInfo::PiedPiper), k_FirstSpecial);
	EXPECT_EQ(static_cast<size_t>(VillagerInfo::SailorAccordian), k_FirstSpecial + k_Specials - 1);
}
