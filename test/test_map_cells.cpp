/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ordered object lists of the map cells (ECS/MapCells): DoesObjectTypeCountAsFixed 0x601510 (table 0x60152C),
// Fixed::InsertMapObjectToCell 0x52DEA0 (head of the fixed list), Object::InsertMapObjectToCell 0x636830 (tail of the
// fixed list / head of the mobile one), RemoveMapObjectFromCell 0x6368D0, NewCollideDescriptor 0x46AB10 / 0x46AD80,
// FindTypeOnMap 0x6015E0, IsFixed 0x601EA0, MoveMapObject 0x636A40 / 0x52E4F0, the searches and the town walks.

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Registry.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
class MapCells: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		map_cells::Clear();
		map_cells::detail::SetShapeProviderForTests(nullptr);
		object_index::OnLoadMap();
	}
	void TearDown() override
	{
		map_cells::detail::SetShapeProviderForTests(nullptr);
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity Make(const glm::vec3& position)
	{
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}
	/// A MultiMapFixed (MobileStatic, type 28 MOBILE_STATIC without info.dat)
	static entt::entity Rock(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<MobileStatic>(e);
		return e;
	}
	/// A SingleMapFixed (type 6 FOREST_TREE)
	static entt::entity TreeAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Tree>(e);
		return e;
	}
	/// An Object of a fixed type (21 POT: the tail of the fixed list)
	static entt::entity PotAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Pot>(e);
		return e;
	}
	/// An Object of a mobile type (4 ANIMAL)
	static entt::entity AnimalAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Animal>(e);
		return e;
	}
	static std::vector<entt::entity> Fixed(glm::ivec2 cell)
	{
		std::vector<entt::entity> list;
		for (auto e = map_cells::FirstFixed(cell); e != entt::null; e = map_cells::GetMapChild(e, cell))
		{
			list.push_back(e);
		}
		return list;
	}
	static std::vector<entt::entity> Mobile(glm::ivec2 cell)
	{
		std::vector<entt::entity> list;
		for (auto e = map_cells::FirstMobile(cell); e != entt::null; e = map_cells::GetMapChild(e, cell))
		{
			list.push_back(e);
		}
		return list;
	}
};

using V = std::vector<entt::entity>;
const glm::vec3 k_P(55.0f, 0.0f, 55.0f); // cell (5, 5)
const glm::ivec2 k_C(5, 5);
const entt::entity k_Null = entt::null; // gtest cannot print entt::null_t
} // namespace

TEST(MapCellsTable, CountsAsFixedIsTheExeTable)
{
	// the jump table 0x60152C read from runblack.exe: its entries that go to 0x601520 (return 1)
	const std::vector<int> fixed = {0,  6,  7,  8,  9,  11, 12, 14, 18, 19, 21, 22, 23, 24, 25, 26,
	                                28, 29, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 43, 44};
	for (int t = 0; t < 45; ++t)
	{
		const bool expected = std::find(fixed.begin(), fixed.end(), t) != fixed.end();
		EXPECT_EQ(map_cells::CountsAsFixed(static_cast<ObjectType>(t)), expected) << t;
	}
	// cmp eax, 0x2C; ja (unsigned): -1, -2 and 45 give 0
	EXPECT_FALSE(map_cells::CountsAsFixed(ObjectType::Any));
	EXPECT_FALSE(map_cells::CountsAsFixed(ObjectType::Invalid));
	EXPECT_FALSE(map_cells::CountsAsFixed(static_cast<ObjectType>(45)));
}

TEST_F(MapCells, FixedListHeadForFixedTailForObjects)
{
	const auto a = Rock(k_P);
	const auto p1 = PotAt(k_P);
	const auto b = Rock(k_P);
	const auto p2 = PotAt(k_P);
	for (const auto e : {a, p1, b, p2})
	{
		map_cells::InsertMapObject(e);
	}
	EXPECT_EQ(Fixed(k_C), (V {b, a, p1, p2}));
	EXPECT_TRUE(Mobile(k_C).empty());
	// the walk: the fixed list, then the mobile one
	const auto animal = AnimalAt(k_P);
	map_cells::InsertMapObject(animal);
	EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {b, a, p1, p2, animal}));
	map_cells::RemoveMapObject(a); // the middle
	EXPECT_EQ(Fixed(k_C), (V {b, p1, p2}));
	map_cells::RemoveMapObject(b); // the head
	map_cells::RemoveMapObject(p2); // the tail
	EXPECT_EQ(Fixed(k_C), (V {p1}));
	EXPECT_EQ(map_cells::GetMapChild(p1, k_C), k_Null);
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, MobileListHeadDoublyLinked)
{
	const auto v1 = AnimalAt(k_P);
	const auto v2 = AnimalAt(k_P);
	const auto v3 = AnimalAt(k_P);
	for (const auto e : {v1, v2, v3})
	{
		map_cells::InsertMapObject(e);
	}
	EXPECT_EQ(Mobile(k_C), (V {v3, v2, v1}));
	map_cells::RemoveMapObject(v2);
	EXPECT_EQ(Mobile(k_C), (V {v3, v1}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	map_cells::RemoveMapObject(v3);
	EXPECT_EQ(Mobile(k_C), (V {v1}));
	map_cells::InsertMapObject(v2);
	EXPECT_EQ(Mobile(k_C), (V {v2, v1}));
	map_cells::RemoveMapObject(v1); // the tail
	EXPECT_EQ(Mobile(k_C), (V {v2}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, MultiCellObjectInEveryCellOfItsShape)
{
	// a circle of 3 m at (10, 10): the 7.1 m circles of the four cells around it touch it; reach 4 -> cells 0..1
	map_cells::detail::SetShapeProviderForTests([](entt::entity, map_collide::Shape& shape, float& reach) {
		shape = {{10.0f, 10.0f}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	});
	const auto under = PotAt(glm::vec3(5.0f, 0.0f, 15.0f)); // cell (0, 1)
	map_cells::InsertMapObject(under);
	const auto house = Rock(glm::vec3(10.0f, 0.0f, 10.0f));
	map_cells::InsertMapObject(house);
	EXPECT_EQ(map_cells::CellsOf(house), (std::vector<glm::ivec2> {{0, 0}, {0, 1}, {1, 0}, {1, 1}}));
	for (const auto cell : {glm::ivec2(0, 0), glm::ivec2(0, 1), glm::ivec2(1, 0), glm::ivec2(1, 1)})
	{
		EXPECT_EQ(map_cells::FirstFixed(cell), house);
		EXPECT_TRUE(map_cells::IsFixed(cell));
	}
	EXPECT_EQ(map_cells::GetMapChild(house, {0, 1}), under); // its own next per cell
	EXPECT_EQ(map_cells::GetMapChild(house, {1, 1}), k_Null);
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	map_cells::RemoveMapObject(house);
	EXPECT_EQ(map_cells::FirstFixed({0, 1}), under);
	EXPECT_EQ(map_cells::FirstFixed({1, 1}), k_Null);
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST(MapCellsDescriptor, CellsOrderClipFallbackAndCut)
{
	// x outer, z inner
	const map_collide::Shape round {{10.0f, 10.0f}, 3.0f, {}, 0.0f, "round"};
	EXPECT_EQ(map_cells::DescriptorCells(round, 4.0f), (std::vector<glm::ivec2> {{0, 0}, {0, 1}, {1, 0}, {1, 1}}));
	// ftol truncates towards 0: (12 - 4) * 0.1 = 0.8 -> 0, (12 + 4) * 0.1 = 1.6 -> 1
	const map_collide::Shape small {{12.0f, 12.0f}, 0.5f, {}, 0.0f, "small"};
	EXPECT_EQ(map_cells::DescriptorCells(small, 4.0f), (std::vector<glm::ivec2> {{1, 1}}));
	// x0 < 0 and x1 >= 0: x0 = 0 (only a clip, the box keeps x1)
	const map_collide::Shape edge {{2.0f, 55.0f}, 3.0f, {}, 0.0f, "edge"};
	const auto edgeCells = map_cells::DescriptorCells(edge, 15.0f);
	ASSERT_FALSE(edgeCells.empty());
	EXPECT_EQ(edgeCells.front().x, 0);
	// both < 0: x0 = x1 = 0; the circle at (5, z) misses a shape at x -15, so the middle cell (0, 5) is the fallback
	const map_collide::Shape off {{-15.0f, 55.0f}, 0.5f, {}, 0.0f, "off"};
	EXPECT_EQ(map_cells::DescriptorCells(off, 2.0f), (std::vector<glm::ivec2> {{0, 5}}));
	// no cell hits (the children are far away): the middle, ((w / 2) d) + d / 2. 3 x 2 and 2 x 3 boxes
	const map_collide::Shape miss32 {{15.0f, 10.0f}, 1.0f, {{1000.0f, 1000.0f}}, 0.1f, "miss"};
	EXPECT_EQ(map_cells::DescriptorCells(miss32, 9.0f), (std::vector<glm::ivec2> {{1, 1}}));
	const map_collide::Shape miss23 {{10.0f, 15.0f}, 1.0f, {{1000.0f, 1000.0f}}, 0.1f, "miss"};
	EXPECT_EQ(map_cells::DescriptorCells(miss23, 9.0f), (std::vector<glm::ivec2> {{1, 1}}));
	// the middle off the map (x 512): GetNext gives NULL there and InsertMapObject stops (nothing)
	const map_collide::Shape past {{5125.0f, 10.0f}, 1.0f, {{1000.0f, 1000.0f}}, 0.1f, "past"};
	EXPECT_TRUE(map_cells::DescriptorCells(past, 9.0f).empty());
	// cells past 512 are never tested: only the ones on the map are marked
	const map_collide::Shape border {{5118.0f, 10.0f}, 6.0f, {}, 0.0f, "border"};
	for (const auto cell : map_cells::DescriptorCells(border, 7.0f))
	{
		EXPECT_LT(cell.x, 512);
	}
}

TEST_F(MapCells, FindTypeTwoContracts)
{
	const auto rock = Rock(k_P);
	const auto pot = PotAt(k_P);
	const auto tree = TreeAt(k_P);
	const auto animal = AnimalAt(k_P);
	for (const auto e : {rock, pot, tree, animal})
	{
		map_cells::InsertMapObject(e);
	}
	// ANY: the fixed list (tree, rock heads; pot tail), then the mobile one
	V any;
	for (auto o = map_cells::FindType(k_C, ObjectType::Any); o != entt::null; o = map_cells::FindType(k_C, ObjectType::Any, o))
	{
		any.push_back(o);
	}
	EXPECT_EQ(any, (V {tree, rock, pot, animal}));
	// POT 21 counts as fixed: only the fixed list
	EXPECT_EQ(map_cells::FindType(k_C, ObjectType::Pot), pot);
	EXPECT_EQ(map_cells::FindType(k_C, ObjectType::Pot, pot), k_Null);
	// ANIMAL 4: only the mobile list
	EXPECT_EQ(map_cells::FindType(k_C, ObjectType::Animal), animal);
	// after: from its child
	EXPECT_EQ(map_cells::FindType(k_C, ObjectType::MobileStatic, tree), rock);
	EXPECT_EQ(map_cells::FindType(k_C, ObjectType::ForestTree, tree), k_Null);
	// off the map
	EXPECT_EQ(map_cells::FindType({512, 0}, ObjectType::Any), k_Null);
	EXPECT_EQ(map_cells::FindFixedOnMap(k_C), rock);
}

TEST_F(MapCells, IsFixedLooksAtTheHeadOnly)
{
	const auto tree = TreeAt(k_P);
	map_cells::InsertMapObject(tree);
	EXPECT_FALSE(map_cells::IsFixed(k_C)); // a SingleMapFixed
	const auto rock = Rock(k_P);
	map_cells::InsertMapObject(rock);
	EXPECT_TRUE(map_cells::IsFixed(k_C)); // a MultiMapFixed at the head
	const auto young = TreeAt(k_P);
	map_cells::InsertMapObject(young);
	EXPECT_FALSE(map_cells::IsFixed(k_C)); // a new tree over it
	// a pot goes to the tail: the head stays
	const auto pot = PotAt(k_P);
	map_cells::InsertMapObject(pot);
	EXPECT_FALSE(map_cells::IsFixed(k_C));
}

TEST_F(MapCells, MoveMapObjectOnlyOnACellChange)
{
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	map_cells::InsertMapObject(a);
	map_cells::InsertMapObject(b);
	ASSERT_EQ(Mobile(k_C), (V {b, a}));
	// the same cell: the position changes, the list doesn't (0x636A40)
	map_cells::MoveMapObject(a, glm::vec3(58.0f, 0.0f, 51.0f));
	EXPECT_EQ(Mobile(k_C), (V {b, a}));
	EXPECT_FLOAT_EQ(Reg().Get<Transform>(a).position.x, 58.0f);
	// another cell: the head of the new one
	const auto c = AnimalAt(glm::vec3(65.0f, 0.0f, 55.0f));
	map_cells::InsertMapObject(c);
	map_cells::MoveMapObject(a, glm::vec3(66.0f, 0.0f, 55.0f));
	EXPECT_EQ(Mobile(k_C), (V {b}));
	EXPECT_EQ(Mobile({6, 5}), (V {a, c}));
	// a MultiMapFixed: the same MapCoords does nothing; another goes to the head
	const auto r1 = Rock(k_P);
	const auto r2 = Rock(k_P);
	map_cells::InsertMapObject(r1);
	map_cells::InsertMapObject(r2);
	ASSERT_EQ(Fixed(k_C), (V {r2, r1}));
	map_cells::MoveMapObject(r1, k_P);
	EXPECT_EQ(Fixed(k_C), (V {r2, r1}));
	map_cells::MoveMapObject(r1, glm::vec3(56.0f, 0.0f, 55.0f));
	EXPECT_EQ(Fixed(k_C), (V {r1, r2}));
	// SetXYZAnglesAndScale: to the head even in place
	map_cells::OnAnglesOrScaleChanged(r2);
	EXPECT_EQ(Fixed(k_C), (V {r2, r1}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, SyncInsertsByCreationIndexAndPrunes)
{
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	const auto c = AnimalAt(k_P);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {c, b, a})); // a first (the oldest), so it ends last
	// moved by an owner without hooks: the head of the new cell at the next Sync
	Reg().Get<Transform>(a).position = glm::vec3(75.0f, 0.0f, 55.0f);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {c, b}));
	EXPECT_EQ(Mobile({7, 5}), (V {a}));
	// deleted
	Reg().Destroy(b);
	EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {c})); // the readers skip it at once
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {c}));
	// held out (a tornado carries it): out at once, back at the head when let go
	const auto d = AnimalAt(k_P);
	map_cells::Sync();
	ASSERT_EQ(Mobile(k_C), (V {d, c}));
	map_cells::SetHeldOutOfMap(c, true);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {d}));
	map_cells::SetHeldOutOfMap(c, false);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {c, d}));
	// a tree felled into a dead tree (same entity, another class): out and in again as a MultiMapFixed
	const auto tree = TreeAt(k_P);
	map_cells::Sync();
	EXPECT_EQ(map_cells::KindOf(tree), map_cells::InsertKind::SingleMapFixed);
	Reg().Remove<Tree>(tree);
	Reg().Assign<DeadTree>(tree);
	map_cells::Sync();
	EXPECT_EQ(map_cells::KindOf(tree), map_cells::InsertKind::MultiMapFixed);
	EXPECT_TRUE(map_cells::IsFixed(k_C));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, SyncLeavesHookedOrderAlone)
{
	const auto old = AnimalAt(k_P);
	map_cells::Sync();
	const auto hooked = AnimalAt(k_P);
	map_cells::InsertMapObject(hooked);
	map_cells::Sync(); // nothing to do
	EXPECT_EQ(Mobile(k_C), (V {hooked, old}));
	EXPECT_EQ(map_cells::ObjectCount(), 2u);
}

TEST_F(MapCells, TownsByPlayerThenAge)
{
	const auto town = [](uint32_t id, PlayerNames owner, glm::vec3 p, Tribe tribe) {
		const auto e = Make(p);
		auto& t = Reg().Assign<Town>(e);
		t.id = id;
		t.owner = owner;
		Reg().Assign<Tribe>(e, tribe);
		return e;
	};
	const auto neutral = town(0, PlayerNames::NEUTRAL, glm::vec3(100.0f, 0.0f, 0.0f), Tribe::NORSE);
	const auto p1b = town(3, PlayerNames::PLAYER_TWO, glm::vec3(100.0f, 0.0f, 0.0f), Tribe::CELTIC);
	const auto p0b = town(2, PlayerNames::PLAYER_ONE, glm::vec3(300.0f, 0.0f, 0.0f), Tribe::CELTIC);
	const auto p0a = town(1, PlayerNames::PLAYER_ONE, glm::vec3(100.0f, 0.0f, 0.0f), Tribe::NORSE);
	V order;
	map_cells::ForEachTown([&order](entt::entity t) {
		order.push_back(t);
		return true;
	});
	EXPECT_EQ(order, (V {p0a, p0b, p1b, neutral}));
	EXPECT_EQ(map_cells::TownsOf(PlayerNames::PLAYER_ONE), (V {p0a, p0b}));
	const auto at = map_coords::FromMetres(glm::vec2(0.0f, 0.0f));
	// three towns at the same distance: the first of the walk (strict <)
	EXPECT_EQ(map_cells::GetNearestTown(at, 500.0f), p0a);
	// SpellShield's 500 (0x43FA0000) finds a town 300 m away; a radius at the distance does not
	const auto far = map_coords::FromMetres(glm::vec2(600.0f, 0.0f));
	EXPECT_EQ(map_cells::GetNearestTown(far, 500.0f), p0b);
	const float d = gutils::GetDistanceInMetres(far, map_coords::FromMetres(glm::vec2(300.0f, 0.0f)));
	EXPECT_EQ(map_cells::GetNearestTown(far, d), k_Null);
	// a tribe
	EXPECT_EQ(map_cells::GetNearestTownToPos(at, Tribe::CELTIC, map_cells::k_AnyAbodeType, 1e9f), p1b);
	EXPECT_EQ(map_cells::GetNearestTownToPos(at, std::nullopt, map_cells::k_AnyAbodeType, 1e9f), p0a);
	// the global list: the first always, then strictly nearer
	EXPECT_EQ(map_cells::FindNearestTownInList(at), neutral);
}

TEST_F(MapCells, FindNearForScriptAndSpiral)
{
	const auto near = PotAt(glm::vec3(52.0f, 0.0f, 52.0f));
	const auto farther = PotAt(glm::vec3(75.0f, 0.0f, 55.0f));
	map_cells::Sync();
	const auto at = map_coords::FromMetres(glm::vec2(55.0f, 55.0f));
	const auto any = [](entt::entity) { return true; };
	EXPECT_EQ(map_cells::FindNearForScript(at, any, 30.0f), near);
	EXPECT_EQ(map_cells::FindNearForScript(at, [near](entt::entity e) { return e != near; }, 30.0f), farther);
	// the square only: not cut at r inside it (the cell of 75 is within +-25 m), nothing outside it
	EXPECT_EQ(map_cells::FindNearForScript(at, [near](entt::entity e) { return e != near; }, 15.0f), farther);
	EXPECT_EQ(map_cells::FindNearForScript(at, [near](entt::entity e) { return e != near; }, 5.0f), k_Null);
	// the spiral: d < r, excluded
	EXPECT_EQ(map_cells::FindNearestInSpiral(at, any, 30.0f), near);
	EXPECT_EQ(map_cells::FindNearestInSpiral(at, any, 30.0f, near), farther);
	EXPECT_EQ(map_cells::FindNearestInSpiral(at, any, 4.0f), k_Null);
	// FindNearType: one list (POT: the fixed one), not cut at r
	EXPECT_EQ(map_cells::FindNearType(at, ObjectType::Pot, 30.0f), near);
	EXPECT_EQ(map_cells::FindNearType(at, ObjectType::Any, 30.0f), k_Null); // ANY: the mobile list only
}

TEST_F(MapCells, ClassesAsTheVtables)
{
	// map_cell_queries_original.md §3: MultiMapFixed::InsertMapObject 0x52E650 for the abodes (fields too), the mobile
	// statics and the Rocks (DeadTree, Fragment), MagicTeleport (: MobileStatic); SingleMapFixed 0x52E620 for trees and
	// shields; Object 0x636740 for the street lanterns (GStreetLantern : Object), pots and animals
	const auto with = [](auto component) {
		const auto e = Make(k_P);
		Reg().Assign<decltype(component)>(e);
		return e;
	};
	const auto field = with(Field {});
	const auto dead = with(DeadTree {});
	const auto fragment = with(Fragment {});
	const auto stone = with(MagicTeleport {});
	const auto rock = Rock(k_P);
	const auto tree = TreeAt(k_P);
	const auto shield = with(MapShield {});
	const auto lantern = with(StreetLantern {});
	const auto pot = PotAt(k_P);
	for (const auto e : {field, dead, fragment, stone, rock})
	{
		EXPECT_EQ(map_cells::KindOf(e), map_cells::InsertKind::MultiMapFixed);
	}
	EXPECT_EQ(map_cells::KindOf(tree), map_cells::InsertKind::SingleMapFixed);
	EXPECT_EQ(map_cells::KindOf(shield), map_cells::InsertKind::SingleMapFixed);
	EXPECT_EQ(map_cells::KindOf(lantern), map_cells::InsertKind::Object);
	EXPECT_EQ(map_cells::KindOf(pot), map_cells::InsertKind::Object);
	// the fire's trait (Object::AsMultiMapFixed vt +0x678) is the same class list
	for (const auto e : {field, dead, fragment, stone, rock, tree, shield, lantern, pot})
	{
		EXPECT_EQ(fire::traits::IsMultiMapFixed(e), map_cells::IsMultiMapFixedClass(e));
	}
	// a street lantern is type 28 (inferido, GStreetLantern::Create 0x7346E0 FindType(0x1C)): the tail of the fixed list
	map_cells::InsertMapObject(rock);
	map_cells::InsertMapObject(lantern);
	map_cells::InsertMapObject(tree);
	EXPECT_EQ(Fixed(k_C), (V {tree, rock, lantern}));
}

/// With OPENBLACK_TEST_GAME_PATH: the types come from info.dat (info +0x10)
TEST_F(MapCells, TypesFromInfoDat)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	Locator::infoConstants::reset(info.release());
	const auto& c = Locator::infoConstants::value();
	for (const auto& row : c.fieldType)
	{
		EXPECT_EQ(row.type, ObjectType::Field); // so FindType(ABODE) does not see a field (SpellForest NoAbodeCovers)
	}
	for (const auto& row : c.mobileStatic)
	{
		EXPECT_EQ(row.type, ObjectType::MobileStatic);
	}
	for (const auto& row : c.pot)
	{
		EXPECT_EQ(row.type, ObjectType::Pot);
	}
	for (const auto& row : c.tree)
	{
		EXPECT_EQ(row.type, ObjectType::ForestTree);
	}
	for (const auto& row : c.animal)
	{
		EXPECT_EQ(row.type, ObjectType::Animal);
	}
	for (const auto& row : c.villager)
	{
		EXPECT_EQ(row.type, ObjectType::Villager);
	}
	for (const auto& row : c.mapShield)
	{
		EXPECT_EQ(row.type, ObjectType::MapShield);
	}
	EXPECT_EQ(c.fishFarm.type, ObjectType::FishFarm);
	for (const auto& row : c.abode)
	{
		EXPECT_EQ(row.type, ObjectType::Abode);
	}
	// what each class reads
	const auto pot = PotAt(k_P);
	Reg().Get<Pot>(pot).type = PotInfo::FoodPile;
	EXPECT_EQ(map_cells::TypeOf(pot), ObjectType::Pot);
	const auto rock = Rock(k_P);
	EXPECT_EQ(map_cells::TypeOf(rock), ObjectType::MobileStatic);
	const auto tree = TreeAt(k_P);
	EXPECT_EQ(map_cells::TypeOf(tree), ObjectType::ForestTree);
	const auto animal = AnimalAt(k_P);
	EXPECT_EQ(map_cells::TypeOf(animal), ObjectType::Animal);
	// the one-off orb: GMobileObjectInfo 25 (0xD39F3C, ctor 0x72A3A0) is type 20, so the head of the mobile list
	EXPECT_EQ(c.mobileObject[static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed)].type, ObjectType::MobileObject);
	const auto orb = Make(k_P);
	Reg().Assign<OneOffSpellSeed>(orb);
	EXPECT_EQ(map_cells::TypeOf(orb), ObjectType::MobileObject);
	map_cells::InsertMapObject(orb);
	EXPECT_EQ(map_cells::FirstMobile(k_C), orb);
	// the rows the code takes as (inferido): print them for the record
	std::printf("mobileObject[OneOffSpellSeed].type %d, mobileObject[Whale].type %d, totemStatue[0] %d, bigForest[0] %d, "
	            "citadelHeart %d, worshipSite[0] %d, spellIcon[0] %d, spellIcon[1] %d, animatedStatic[0] %d, feature[0] %d, "
	            "creature[0] %d\n",
	            static_cast<int>(c.mobileObject[static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed)].type),
	            static_cast<int>(c.mobileObject[static_cast<size_t>(MobileObjectInfo::Whale)].type),
	            static_cast<int>(c.totemStatue[0].type), static_cast<int>(c.bigForest[0].type),
	            static_cast<int>(c.citadelHeart.type), static_cast<int>(c.worshipSite[0].type),
	            static_cast<int>(c.spellIcon[0].type), static_cast<int>(c.spellIcon[1].type),
	            static_cast<int>(c.animatedStatic[0].type), static_cast<int>(c.feature[0].type),
	            static_cast<int>(c.creature[0].type));
	Locator::infoConstants::reset();
}

TEST_F(MapCells, MultiCellRemovalKeepsTheOthersInEveryCell)
{
	// the rock of MultiCellObjectInEveryCellOfItsShape (cells 0..1 x 0..1) between trees: RemoveMapObjectFromCell
	// 0x6368D0 in each of its cells (GetMapParent of a fixed list is a walk from the head)
	map_cells::detail::SetShapeProviderForTests([](entt::entity, map_collide::Shape& shape, float& reach) {
		shape = {{10.0f, 10.0f}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	});
	const auto below00 = TreeAt(glm::vec3(5.0f, 0.0f, 5.0f));
	const auto below11 = TreeAt(glm::vec3(15.0f, 0.0f, 15.0f));
	map_cells::InsertMapObject(below00);
	map_cells::InsertMapObject(below11);
	const auto rock = Rock(glm::vec3(10.0f, 0.0f, 10.0f));
	map_cells::InsertMapObject(rock);
	const auto above00 = TreeAt(glm::vec3(5.0f, 0.0f, 5.0f));
	map_cells::InsertMapObject(above00);
	ASSERT_EQ(Fixed({0, 0}), (V {above00, rock, below00}));
	ASSERT_EQ(Fixed({1, 1}), (V {rock, below11}));
	ASSERT_EQ(Fixed({0, 1}), (V {rock}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	map_cells::RemoveMapObject(rock);
	EXPECT_EQ(Fixed({0, 0}), (V {above00, below00}));
	EXPECT_EQ(Fixed({1, 1}), (V {below11}));
	EXPECT_TRUE(Fixed({0, 1}).empty());
	EXPECT_TRUE(Fixed({1, 0}).empty());
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, MobileMiddleRemovalAndSyncMove)
{
	// the middle of the mobile list (its previous, MapParent +0x38, is relinked), then a cell change found by Sync:
	// out of the middle of the old cell, at the head of the new one (ActualMoveMapObject 0x638040)
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	const auto c = AnimalAt(k_P);
	const auto d = AnimalAt(k_P);
	for (const auto e : {a, b, c, d})
	{
		map_cells::InsertMapObject(e);
	}
	ASSERT_EQ(Mobile(k_C), (V {d, c, b, a}));
	map_cells::RemoveMapObject(c);
	EXPECT_EQ(Mobile(k_C), (V {d, b, a}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	const auto there = AnimalAt(glm::vec3(65.0f, 0.0f, 55.0f));
	map_cells::InsertMapObject(there);
	map_cells::InsertMapObject(c);
	ASSERT_EQ(Mobile(k_C), (V {c, d, b, a}));
	Reg().Get<Transform>(b).position = glm::vec3(66.0f, 0.0f, 54.0f);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {c, d, a}));
	EXPECT_EQ(Mobile({6, 5}), (V {b, there}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	// the tail, then the head
	map_cells::RemoveMapObject(a);
	map_cells::RemoveMapObject(c);
	EXPECT_EQ(Mobile(k_C), (V {d}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, HeldOutThenDestroyed)
{
	// a storm carries it (out of the map, SetHeldOutOfMap) and it is deleted before it lands
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	const auto rock = Rock(k_P);
	map_cells::Sync();
	ASSERT_EQ(map_cells::ObjectCount(), 3u);
	map_cells::SetHeldOutOfMap(b, true);
	map_cells::SetHeldOutOfMap(rock, true);
	EXPECT_EQ(Mobile(k_C), (V {a}));
	EXPECT_TRUE(Fixed(k_C).empty());
	Reg().Destroy(b);
	Reg().Destroy(rock);
	map_cells::Sync();
	EXPECT_EQ(map_cells::ObjectCount(), 1u);
	EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {a}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
	// a new entity that reuses a slot is a new object: it enters as one
	const auto fresh = AnimalAt(k_P);
	map_cells::Sync();
	EXPECT_EQ(Mobile(k_C), (V {fresh, a}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, ReadBatchSharesOneSnapshot)
{
	const auto a = AnimalAt(k_P);
	const auto b = AnimalAt(k_P);
	map_cells::Sync();
	{
		const map_cells::ReadBatch outer;
		const map_cells::ReadBatch inner; // nests
		EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {b, a}));
		// held out and invalid are still read live inside a batch
		map_cells::SetHeldOutOfMap(b, true);
		EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {a}));
		map_cells::SetHeldOutOfMap(b, false);
		Reg().Destroy(a);
		EXPECT_FALSE(map_cells::IsReadable(a));
	}
	map_cells::Sync();
	EXPECT_EQ(map_cells::ObjectsInCell(k_C), (V {b}));
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, CollideBitsFromTheFixedList)
{
	// MapCell::Collide 0x601BD0: no island here, so the landscape part is 1 (no block = water, 0x601C5D); a type 6 in the
	// fixed list gives 0x20, a type 0x12 gives 4 (0x601C99 / 0x601C9E); nothing from the mobile list nor the tail's pots
	map_cells::detail::SetShapeProviderForTests([](entt::entity, map_collide::Shape& shape, float& reach) {
		shape = {{10.0f, 10.0f}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	});
	const auto at = [](float x, float z) { return map_coords::FromMetres(glm::vec2(x, z)); };
	const auto tree = TreeAt(k_P);
	const auto pot = PotAt(k_P);
	const auto animal = AnimalAt(glm::vec3(65.0f, 0.0f, 55.0f));
	const auto field = Make(glm::vec3(10.0f, 0.0f, 10.0f));
	Reg().Assign<Field>(field);
	for (const auto e : {tree, pot, animal, field})
	{
		map_cells::InsertMapObject(e);
	}
	EXPECT_EQ(map_cells::Collide(at(55.0f, 55.0f)), 0x21u);
	EXPECT_EQ(map_cells::Collide(at(65.0f, 55.0f)), 0x01u); // an animal only
	EXPECT_EQ(map_cells::Collide(at(85.0f, 85.0f)), 0x01u); // nothing
	for (const auto cell : {glm::vec2(5.0f, 5.0f), glm::vec2(5.0f, 15.0f), glm::vec2(15.0f, 5.0f), glm::vec2(15.0f, 15.0f)})
	{
		EXPECT_EQ(map_cells::Collide(at(cell.x, cell.y)), 0x05u); // the field is in its four cells
	}
	// bit 8 never: the CollideWithFixe branch of 0x601CE0 is dead, even right on the tree
	EXPECT_EQ(map_cells::Collide(at(55.0f, 55.0f)) & 8u, 0u);
	// off the map (ToMap NULL): every bit (0x6033CC)
	EXPECT_EQ(map_cells::Collide(at(-5.0f, 55.0f)), 0xFFFFFFFFu);
	EXPECT_EQ(map_cells::Collide(at(55.0f, 5125.0f)), 0xFFFFFFFFu);
	// what the original has out of the list does not count (held out by a storm)
	map_cells::SetHeldOutOfMap(tree, true);
	EXPECT_EQ(map_cells::Collide(at(55.0f, 55.0f)), 0x01u);
}

TEST_F(MapCells, CollideWithFixedAgainstTheCollideData)
{
	// MapCell::CollideWithFixe 0x601D10: a 0.5 circle against GetCollideData of each object of the fixed list
	map_cells::detail::SetShapeProviderForTests([](entt::entity, map_collide::Shape& shape, float& reach) {
		shape = {{10.0f, 10.0f}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	});
	const auto at = [](float x, float z) { return map_coords::FromMetres(glm::vec2(x, z)); };
	const auto tree = TreeAt(k_P);
	const auto pot = PotAt(glm::vec3(52.0f, 0.0f, 52.0f));
	const auto rock = Rock(glm::vec3(10.0f, 0.0f, 10.0f));
	for (const auto e : {tree, pot, rock})
	{
		map_cells::InsertMapObject(e);
	}
	// the tree's 0.3 circle (Tree::CreateCollideData 0x74C5F0): 0.4 m away hits (0.3 + 0.5), 1 m away does not
	ASSERT_NE(map_cells::CollideDataOf(tree), nullptr);
	EXPECT_FLOAT_EQ(map_cells::CollideDataOf(tree)->radius, 0.3f);
	EXPECT_EQ(map_cells::CollideWithFixed(at(55.4f, 55.0f)), 0x29u);
	EXPECT_EQ(map_cells::CollideWithFixed(at(56.0f, 55.0f)), 0x21u);
	// a pot (Object::GetCollideData 0x419B30 = 0): never, even on it
	EXPECT_EQ(map_cells::CollideDataOf(pot), nullptr);
	EXPECT_EQ(map_cells::CollideWithFixed(at(52.0f, 52.0f)) & 8u, 0u);
	// the rock's shape (MultiMapFixed::CreateCollideData 0x52F550), in every cell of the rock
	ASSERT_NE(map_cells::CollideDataOf(rock), nullptr);
	EXPECT_EQ(map_cells::CollideWithFixed(at(13.2f, 10.0f)), 0x09u); // 3.2 < 3 + 0.5, cell (1, 1)
	EXPECT_EQ(map_cells::CollideWithFixed(at(9.0f, 9.0f)), 0x09u);   // cell (0, 0)
	EXPECT_EQ(map_cells::CollideWithFixed(at(14.0f, 10.0f)), 0x01u); // 4 > 3.5
	EXPECT_EQ(map_cells::CollideWithFixed(at(-5.0f, 10.0f)), 0xFFFFFFFFu);
	// a BigForest has none (0x439580 = ReleaseCollideData)
	const auto forest = Make(glm::vec3(10.0f, 0.0f, 10.0f));
	Reg().Assign<BigForest>(forest);
	map_cells::InsertMapObject(forest);
	EXPECT_EQ(map_cells::CollideDataOf(forest), nullptr);
	// out of the map: no data, and the cell no longer collides
	map_cells::RemoveMapObject(rock);
	EXPECT_EQ(map_cells::CollideDataOf(rock), nullptr);
	EXPECT_EQ(map_cells::CollideWithFixed(at(13.2f, 10.0f)), 0x01u);
	EXPECT_EQ(map_cells::CheckConsistency(), 0u);
}

TEST_F(MapCells, ForEachFixedWalksTheFixedListOnly)
{
	const auto rock = Rock(k_P);
	const auto pot = PotAt(k_P);
	const auto tree = TreeAt(k_P);
	const auto animal = AnimalAt(k_P);
	for (const auto e : {rock, pot, tree, animal})
	{
		map_cells::InsertMapObject(e);
	}
	V seen;
	map_cells::ForEachFixed(k_C, [&seen](entt::entity e) {
		seen.push_back(e);
		return true;
	});
	EXPECT_EQ(seen, (V {tree, rock, pot}));
	// stops when fn says so; filtered like the other readers
	seen.clear();
	map_cells::SetHeldOutOfMap(tree, true);
	map_cells::ForEachFixed(k_C, [&seen](entt::entity e) {
		seen.push_back(e);
		return false;
	});
	EXPECT_EQ(seen, (V {rock}));
	seen.clear();
	map_cells::ForEachFixed({512, 0}, [&seen](entt::entity e) {
		seen.push_back(e);
		return true;
	});
	EXPECT_TRUE(seen.empty());
}

TEST_F(MapCells, FindPlayerTownAtPosOnePlayerAndTiesToTheLater)
{
	const auto town = [](uint32_t id, PlayerNames owner, glm::vec3 p) {
		const auto e = Make(p);
		auto& t = Reg().Assign<Town>(e);
		t.id = id;
		t.owner = owner;
		Reg().Assign<Tribe>(e, Tribe::NORSE);
		return e;
	};
	const auto other = town(0, PlayerNames::PLAYER_TWO, glm::vec3(110.0f, 0.0f, 100.0f));
	const auto first = town(1, PlayerNames::PLAYER_ONE, glm::vec3(150.0f, 0.0f, 100.0f));
	const auto second = town(2, PlayerNames::PLAYER_ONE, glm::vec3(50.0f, 0.0f, 100.0f));
	const auto at = map_coords::FromMetres(glm::vec2(100.0f, 100.0f));
	// the same distance (50): <= (0x6F7312), the later one of the player's list wins; the other player's nearer town is
	// not looked at
	EXPECT_EQ(map_cells::FindPlayerTownAtPos(at, 1000.0f, PlayerNames::PLAYER_ONE), second);
	EXPECT_EQ(map_cells::FindPlayerTownAtPos(at, 1000.0f, PlayerNames::PLAYER_TWO), other);
	// r itself counts; below it, nothing
	const float d = gutils::GetDistanceInMetres(at, map_coords::FromMetres(glm::vec2(150.0f, 100.0f)));
	EXPECT_EQ(map_cells::FindPlayerTownAtPos(at, d, PlayerNames::PLAYER_ONE), second);
	EXPECT_EQ(map_cells::FindPlayerTownAtPos(at, d - 1.0f, PlayerNames::PLAYER_ONE), k_Null);
	EXPECT_EQ(map_cells::FindPlayerTownAtPos(at, 1000.0f, PlayerNames::PLAYER_THREE), k_Null);
	// GetNearestTown (strict <, every player) would give the other one
	EXPECT_EQ(map_cells::GetNearestTown(at, 1000.0f), other);
	(void)first;
}
