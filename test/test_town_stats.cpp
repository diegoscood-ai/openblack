/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// town_stats: the mesh id of an info record (MeshIdHash) is the resource id resources::HashIdentifier gives, AbodesOf
// keeps the order of a sort on each abode's creation index, ties included, the town turn's stats take the villagers'
// counts as the town keeps them running, and the space left in the homes is their places less everyone in them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace town_stats = openblack::ecs::town_stats;

namespace
{
class TownStatsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		_town = Reg().Create();
		Reg().Assign<Town>(_town).id = k_TownId;
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static constexpr uint32_t k_TownId = 3;
	entt::entity _town {entt::null};
};
} // namespace

TEST(TownStatsMeshIdHash, IsTheResourceIdOfTheMesh)
{
	for (uint32_t id = 0; id < 5000; ++id)
	{
		const auto mesh = static_cast<MeshId>(id);
		ASSERT_EQ(town_stats::MeshIdHash(mesh), resources::HashIdentifier(mesh)) << id;
	}
	for (const uint32_t id : {9999u, 10000u, 65535u, 1000000u, 4294967295u})
	{
		const auto mesh = static_cast<MeshId>(id);
		EXPECT_EQ(town_stats::MeshIdHash(mesh), resources::HashIdentifier(mesh)) << id;
	}
}

TEST_F(TownStatsTest, AbodesOfIsTheSortOnTheCreationIndexTiesIncluded)
{
	// more abodes than the sort's small-range threshold, with repeated indices and abodes without one (-1), between
	// abodes of another town
	std::vector<entt::entity> abodes;
	for (uint32_t i = 0; i < 90; ++i)
	{
		const auto abode = Reg().Create();
		Reg().Assign<Abode>(abode).townId = i % 4 == 3 ? k_TownId + 1 : k_TownId;
		if (i % 5 != 0)
		{
			Reg().Assign<ObjectCreationIndex>(abode, (i * 7u) % 23u);
		}
	}
	// the order the abodes had before: the town's abodes in the storage's order, sorted on Of() looked up each time
	Reg().Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == k_TownId)
		{
			abodes.push_back(entity);
		}
	});
	std::sort(abodes.begin(), abodes.end(),
	          [](entt::entity a, entt::entity b) { return ecs::object_index::Of(a) > ecs::object_index::Of(b); });
	EXPECT_EQ(town_stats::AbodesOf(_town), abodes);
	EXPECT_TRUE(town_stats::AbodesOf(Reg().Create()).empty()); // not a town
}

TEST(TownStatsVillagerCounts, KeepOnlyTheCountsMovedAsVillagersComeAndGo)
{
	TownStats stats {.adults = 7,
	                 .children = 3,
	                 .abodesWithPlaces = 4,
	                 .civicBuildings = 2,
	                 .civicPlans = 1,
	                 .totalSpaceLeft = 20,
	                 .adultPlaces = 12,
	                 .childPlaces = 8,
	                 .freeAdultPlaces = 5,
	                 .males = 6,
	                 .females = 4,
	                 .foodForDinner = 31.0f,
	                 .foodCarried = 9.0f,
	                 .woodCarried = 14.0f,
	                 .woodAtSites = 40.0f};
	stats.disciples.at(2) = 1;
	stats.abodesByNumber.at(0) = 3;
	const auto kept = town_stats::VillagerCounts(stats);
	EXPECT_EQ(kept.adults, 7u);
	EXPECT_EQ(kept.children, 3u);
	EXPECT_EQ(kept.males, 6u);
	EXPECT_EQ(kept.females, 4u);
	EXPECT_EQ(kept.disciples, stats.disciples);
	EXPECT_EQ(kept.foodForDinner, 31.0f);
	EXPECT_EQ(kept.foodCarried, 9.0f);
	EXPECT_EQ(kept.woodCarried, 14.0f);
	// the abodes', plans' and sites' counts are the town turn's to count again
	EXPECT_EQ(kept.abodesWithPlaces, 0u);
	EXPECT_EQ(kept.civicBuildings, 0u);
	EXPECT_EQ(kept.civicPlans, 0u);
	EXPECT_EQ(kept.totalSpaceLeft, 0);
	EXPECT_EQ(kept.adultPlaces, 0u);
	EXPECT_EQ(kept.childPlaces, 0u);
	EXPECT_EQ(kept.freeAdultPlaces, 0);
	EXPECT_EQ(kept.woodAtSites, 0.0f);
	EXPECT_EQ(kept.abodesByNumber.at(0), 0u);
}

TEST(TownStatsFoodForDinner, ATribeChangeMovesItByTheNewFoodLessTheOld)
{
	EXPECT_EQ(town_stats::AddFoodForDinnerChange(30.0f, 10, 15), 35.0f);
	EXPECT_EQ(town_stats::AddFoodForDinnerChange(30.0f, 15, 10), 25.0f);
	EXPECT_EQ(town_stats::AddFoodForDinnerChange(0.0f, 12, 12), 0.0f);
}

TEST_F(TownStatsTest, TheTownTurnReadsTheVillagersAsTheTownKeepsThem)
{
	auto& stats = Reg().Get<Town>(_town).stats;
	stats.adults = 4;
	stats.children = 2;
	stats.foodForDinner = 18.0f;
	stats.woodCarried = 5.0f;
	stats.abodesWithPlaces = 3; // no abode of the town is counted: the turn finds none
	// a villager linked to the town without joining it is not in the counts
	Reg().Assign<Villager>(Reg().Create()).town = _town;
	const auto turn = town_stats::Compute(_town);
	EXPECT_EQ(turn.adults, 4u);
	EXPECT_EQ(turn.children, 2u);
	EXPECT_EQ(turn.foodForDinner, 18.0f);
	EXPECT_EQ(turn.woodCarried, 5.0f);
	EXPECT_EQ(turn.abodesWithPlaces, 0u);
}

TEST(TownStatsSpaceLeft, AHomeAddsItsPlacesLessEveryoneHousedInIt)
{
	// 4 adult places and 3 child places, 2 adults and 1 child in: 4 left
	auto left = town_stats::AddAbodeSpaceLeft(0, 4, 3, 2, 1);
	EXPECT_EQ(left, 4);
	// a full home adds nothing
	left = town_stats::AddAbodeSpaceLeft(left, 2, 2, 2, 2);
	EXPECT_EQ(left, 4);
	// an over-full home takes from the others' space
	left = town_stats::AddAbodeSpaceLeft(left, 2, 0, 5, 0);
	EXPECT_EQ(left, 1);
	// a home with no places and nobody in it adds nothing
	EXPECT_EQ(town_stats::AddAbodeSpaceLeft(left, 0, 0, 0, 0), 1);
}
