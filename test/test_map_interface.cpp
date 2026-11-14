/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map's cells under raffclar's MapInterface names (ECS/Map.h): MapProduction reads our ordered lists
// (ECS/MapCells), so a cell's things that stay put, its things that move and the whole walk come in the lists' order,
// Sync takes in what moved or was made without the hooks, and Refile puts a thing at the front of its cell.

#define LOCATOR_IMPLEMENTATIONS

#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MapProduction.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
using V = std::vector<entt::entity>;
const glm::vec3 k_P(55.0f, 0.0f, 55.0f); // cell (5, 5)
const MapInterface::CellId k_C(5, 5);

V ToVector(std::span<const entt::entity> span)
{
	return {span.begin(), span.end()};
}

class MapInterfaceCells: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
	}
	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity Make(const glm::vec3& position)
	{
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}
	/// A building-like thing that stays put (MobileStatic): the front of the fixed list
	static entt::entity Rock(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<MobileStatic>(e);
		return e;
	}
	/// A thing that counts as staying put but can be carried: the back of the fixed list
	static entt::entity PotAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Pot>(e);
		return e;
	}
	/// A thing that moves: the front of the mobile list
	static entt::entity AnimalAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Animal>(e);
		return e;
	}

	MapProduction _map;
};
} // namespace

TEST_F(MapInterfaceCells, ACellIsReadInTheListsOrder)
{
	const auto a = Rock(k_P);
	const auto pot = PotAt(k_P);
	const auto b = Rock(k_P);
	const auto first = AnimalAt(k_P);
	const auto second = AnimalAt(k_P);
	for (const auto e : {a, pot, b, first, second})
	{
		map_cells::InsertMapObject(e);
	}
	EXPECT_EQ(ToVector(_map.GetFixedInGridCell(k_C)), (V {b, a, pot}));
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {second, first}));
	EXPECT_EQ(_map.GetAllInCell(glm::ivec2(k_C)), (V {b, a, pot, second, first}));
	// by position: the cell it falls in
	EXPECT_EQ(ToVector(_map.GetFixedInGridCell(k_P)), (V {b, a, pot}));
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_P)), (V {second, first}));
	// the two views are kept apart
	const auto fixed = _map.GetFixedInGridCell(k_C);
	const auto mobile = _map.GetMobileInGridCell(k_C);
	EXPECT_EQ(ToVector(fixed), (V {b, a, pot}));
	EXPECT_EQ(ToVector(mobile), (V {second, first}));
}

TEST_F(MapInterfaceCells, NothingOffTheMap)
{
	map_cells::InsertMapObject(AnimalAt(k_P));
	EXPECT_TRUE(_map.GetFixedInGridCell(MapInterface::CellId(0xFFFF, 5)).empty());
	EXPECT_TRUE(_map.GetMobileInGridCell(MapInterface::CellId(5, 0x200)).empty());
	EXPECT_TRUE(_map.GetAllInCell({-1, 5}).empty());
	EXPECT_TRUE(_map.GetAllInCell({5, 512}).empty());
}

TEST_F(MapInterfaceCells, SyncTakesInWhatWasMadeAndWhatMoved)
{
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	_map.Sync();
	// the oldest first, so it ends last
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {b, a}));
	// moved without the hooks: in its new cell after the next sync
	Reg().Get<Transform>(a).position = glm::vec3(75.0f, 0.0f, 55.0f);
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {b, a}));
	_map.Sync();
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {b}));
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(MapInterface::CellId(7, 5))), (V {a}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapInterfaceCells, RefileGoesToTheFrontOfItsCell)
{
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	map_cells::InsertMapObject(a);
	map_cells::InsertMapObject(b);
	ASSERT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {b, a}));
	// in the same cell, out and in again at the front
	_map.Refile(a);
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {a, b}));
	// not in the map yet: in for the first time
	const auto c = AnimalAt(k_P);
	_map.Refile(c);
	EXPECT_EQ(ToVector(_map.GetMobileInGridCell(k_C)), (V {c, a, b}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}
