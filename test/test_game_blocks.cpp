/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land blocks' object arrays (src/ECS/DrawList/GameBlocks.h) against the original's rules: the append order, the
// "last inserted" and "last removed" skips, the one-entry remove that takes whatever is there, the count that drops
// with no match, the block of a cell or the outside array; the map cells filing a multi-cell object in each block's
// array, an object outside every block and one off the map; and the draw-list service built directly (no locator):
// the rebuild request event, its last write winning, one Update's list order and passes, and the map clear keeping
// the listed marks; the game's objects as the list reads them from a registry, whose listed mark goes with its entity,
// so that an index made again is listed afresh; and the land flag, raised when a visible block's land clip changes,
// which rebuilds the list between the turn rule's rebuilds. Synthetic values only.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <optional>
#include <set>
#include <vector>

#include <LNDFile.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "Common/EventManager.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/DrawList/GameBlocks.h"
#include "ECS/DrawList/LandClip.h"
#include "ECS/DrawList/RegistryObjects.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DrawListSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using draw_list::BlockArray;
using draw_list::BlockOf;
using V = std::vector<entt::entity>;

namespace
{
/// Entities made by hand: nothing here reads a registry
entt::entity E(uint32_t index)
{
	return static_cast<entt::entity>(index);
}

V Objects(const BlockArray& array)
{
	const auto objects = array.Objects();
	return {objects.begin(), objects.end()};
}
} // namespace

TEST(GameBlocks, InsertAppendsAndSkipsOnlyTheLastInserted)
{
	BlockArray array;
	array.Insert(E(1));
	array.Insert(E(2));
	array.Insert(E(2)); // the last inserted: skipped
	array.Insert(E(3));
	array.Insert(E(1)); // not the last one: no other duplicate check
	EXPECT_EQ(Objects(array), (V {E(1), E(2), E(3), E(1)}));
	EXPECT_EQ(array.Count(), 4);
	// a remove forgets the last inserted
	array.Remove(E(9));
	array.Insert(E(1));
	EXPECT_EQ(array.Count(), 4);
	EXPECT_EQ(Objects(array).back(), E(1));
}

TEST(GameBlocks, RemoveSwapsTheLastIntoTheHoleAndSkipsTheLastRemoved)
{
	BlockArray array;
	for (uint32_t i = 1; i <= 4; ++i)
	{
		array.Insert(E(i));
	}
	array.Remove(E(1));
	EXPECT_EQ(Objects(array), (V {E(4), E(2), E(3)}));
	array.Remove(E(1)); // the last removed: skipped, nothing is lost
	EXPECT_EQ(Objects(array), (V {E(4), E(2), E(3)}));
	array.Remove(E(3)); // the last entry: nothing moves
	EXPECT_EQ(Objects(array), (V {E(4), E(2)}));
	// an insert forgets the last removed: the same object is searched again
	array.Insert(E(5));
	array.Remove(E(3)); // not there: the count still drops, and the last entry goes
	EXPECT_EQ(Objects(array), (V {E(4), E(2)}));
	EXPECT_EQ(array.Count(), 2);
}

TEST(GameBlocks, OneEntryGoesWhicheverObjectIsRemoved)
{
	BlockArray array;
	array.Insert(E(1));
	array.Remove(E(7));
	EXPECT_TRUE(Objects(array).empty());
	EXPECT_EQ(array.Count(), 0);
}

TEST(GameBlocks, RemoveFromAnEmptyArrayTakesTheCountBelowZero)
{
	BlockArray array;
	array.Remove(E(1));
	EXPECT_EQ(array.Count(), -1);
	EXPECT_TRUE(Objects(array).empty());
	array.Insert(E(2)); // only brings the count back up: no entry is kept
	EXPECT_EQ(array.Count(), 0);
	EXPECT_TRUE(Objects(array).empty());
	array.Insert(E(3));
	EXPECT_EQ(Objects(array), (V {E(3)}));
}

TEST(GameBlocks, ClearForgetsTheEntriesAndTheLastObjects)
{
	BlockArray array;
	array.Insert(E(1));
	array.Remove(E(1));
	array.Insert(E(2));
	array.Clear();
	EXPECT_EQ(array.Count(), 0);
	EXPECT_TRUE(Objects(array).empty());
	array.Insert(E(2)); // the last inserted was forgotten
	EXPECT_EQ(Objects(array), (V {E(2)}));

	draw_list::GameBlocks blocks;
	blocks.Of(std::optional<uint16_t>(5)).Insert(E(1));
	blocks.Of(std::nullopt).Insert(E(2));
	blocks.Clear();
	EXPECT_EQ(blocks.blocks[5].Count(), 0);
	EXPECT_EQ(blocks.global.Count(), 0);
}

TEST(GameBlocks, BlockOfACellOrTheOutsideArray)
{
	EXPECT_EQ(BlockOf({0, 0}, true), std::optional<uint16_t>(0));
	EXPECT_EQ(BlockOf({15, 16}, true), std::optional<uint16_t>(1));
	EXPECT_EQ(BlockOf({16, 15}, true), std::optional<uint16_t>(32));
	EXPECT_EQ(BlockOf({511, 511}, true), std::optional<uint16_t>(1023));
	// a block index past 31, a negative cell read as unsigned, or no land block: the outside array
	EXPECT_FALSE(BlockOf({512, 0}, true).has_value());
	EXPECT_FALSE(BlockOf({0, 512}, true).has_value());
	EXPECT_FALSE(BlockOf({-1, 0}, true).has_value());
	EXPECT_FALSE(BlockOf({0, 0}, false).has_value());
}

namespace
{
/// The map cells with a land of 2 x 2 blocks: cells 0..31 on each side have a block, the rest of the map none
class GameBlocksFiling: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		_cells = &Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		Locator::terrainSystem::emplace<test::WaterCellIsland>(static_cast<uint16_t>(2));
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
	/// A MultiMapFixed (MobileStatic)
	static entt::entity Rock(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<MobileStatic>(e);
		return e;
	}
	/// An Object of a fixed type, filed in its own cell only
	static entt::entity PotAt(const glm::vec3& p)
	{
		const auto e = Make(p);
		Reg().Assign<Pot>(e);
		return e;
	}

	[[nodiscard]] V Block(std::optional<uint16_t> slot) const { return Objects(_cells->Blocks()->Of(slot)); }

	test::RestoreService<Locator::terrainSystem> _terrain;
	test::RestoreService<Locator::mapShapeProvider> _shapes;
	ecs::systems::MapCellsSystemInterface* _cells {nullptr};
};
} // namespace

TEST_F(GameBlocksFiling, AMultiCellObjectIsFiledOncePerBlockInCellOrder)
{
	// a circle of 3 m at (160, 160), reach 4: cells 15..16 on each side, (15, 15) (15, 16) (16, 15) (16, 16) in that
	// order, one in each of the blocks 0, 1, 32 and 33
	test::FakeMapShapeProvider shapeProvider;
	shapeProvider.meshShape = [](entt::entity, map_collide::Shape& shape, float& reach) {
		shape = {{160.0f, 160.0f}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	};
	Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>(shapeProvider);
	const auto pot = PotAt(glm::vec3(155.0f, 0.0f, 155.0f)); // cell (15, 15), block 0
	map_cells::InsertMapObject(pot);
	const auto rock = Rock(glm::vec3(160.0f, 0.0f, 160.0f));
	map_cells::InsertMapObject(rock);
	ASSERT_EQ(map_cells::CellsOf(rock), (std::vector<glm::ivec2> {{15, 15}, {15, 16}, {16, 15}, {16, 16}}));
	EXPECT_EQ(Block(0), (V {pot, rock}));
	EXPECT_EQ(Block(1), (V {rock}));
	EXPECT_EQ(Block(32), (V {rock}));
	EXPECT_EQ(Block(33), (V {rock}));
	EXPECT_TRUE(Block(std::nullopt).empty());

	map_cells::RemoveMapObject(rock);
	EXPECT_EQ(Block(0), (V {pot}));
	EXPECT_TRUE(Block(1).empty());
	EXPECT_TRUE(Block(32).empty());
	EXPECT_TRUE(Block(33).empty());
}

TEST_F(GameBlocksFiling, OutsideEveryBlockOrOffTheMap)
{
	// cell (40, 5): the land has no block there
	const auto outside = PotAt(glm::vec3(405.0f, 0.0f, 55.0f));
	map_cells::InsertMapObject(outside);
	EXPECT_EQ(Block(std::nullopt), (V {outside}));
	// cell (512, 1): off the map, in no array
	const auto off = PotAt(glm::vec3(5125.0f, 0.0f, 15.0f));
	map_cells::InsertMapObject(off);
	EXPECT_EQ(Block(std::nullopt), (V {outside}));
	for (size_t slot = 0; slot < draw_list::k_BlockSlots; ++slot)
	{
		EXPECT_TRUE(Block(static_cast<uint16_t>(slot)).empty()) << slot;
	}
	// clearing the map empties the arrays
	const auto inside = PotAt(glm::vec3(55.0f, 0.0f, 55.0f));
	map_cells::InsertMapObject(inside);
	EXPECT_EQ(Block(0), (V {inside}));
	map_cells::Clear();
	EXPECT_TRUE(Block(0).empty());
	EXPECT_TRUE(Block(std::nullopt).empty());
}

namespace
{
/// Every object available, none hidden, human or complex; the listed marks in a set
class AllAvailable final: public draw_list::EntityProbe
{
public:
	[[nodiscard]] bool Available(entt::entity entity) const override { return entity != entt::null; }
	[[nodiscard]] bool DontDraw(entt::entity) const override { return false; }
	[[nodiscard]] bool IsHuman(entt::entity) const override { return false; }
	[[nodiscard]] bool IsComplex(entt::entity) const override { return false; }
	[[nodiscard]] bool Listed(entt::entity entity) const override { return listed.contains(entity); }
	void SetListed(entt::entity entity, bool on) override
	{
		if (on)
		{
			listed.insert(entity);
		}
		else
		{
			listed.erase(entity);
		}
	}

	std::set<entt::entity> listed;
};

/// A camera that sees both test blocks whole: X = x, Y = y and the depth z + 1000
systems::DrawListFrame Frame()
{
	affine::AffineMatrix m;
	m.m[11] = 1000.0f;
	return {.camera = {.eye = {0.0f, 0.0f, 0.0f},
	                   .focus = {0.0f, 0.0f, 1.0f},
	                   .worldToClipping = m,
	                   .nearW = 0.3f,
	                   .half = {320.0f, 240.0f},
	                   .maxScreen = std::nullopt,
	                   .landReflection = false}};
}

/// A camera looking along z at a block at the map's origin, which fills a 640 x 480 screen: X = x - 80 (80 - x with
/// `fromBehind`, so that the block faces away), Y = z - 80 and the depth `depth`. The level-of-detail lines then lie
/// far beyond the block, which keeps the full grid
systems::DrawListFrame LandFrame(bool fromBehind, float depth = 200.0f)
{
	const float sign = fromBehind ? -1.0f : 1.0f;
	affine::AffineMatrix m;
	m.m = {sign, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -80.0f * sign, -80.0f, depth};
	return {.camera = {.eye = {0.0f, 0.0f, 0.0f},
	                   .focus = {0.0f, 0.0f, 1.0f},
	                   .viewDirection = {0.0f, 0.0f, 1.0f},
	                   .worldToClipping = m,
	                   .nearW = 1.0f,
	                   .half = {320.0f, 240.0f},
	                   .maxScreen = glm::vec2(639.0f, 479.0f),
	                   .landReflection = false}};
}
} // namespace

TEST(DrawListSystem, TheRebuildRequestIsAStoreTheLastWriteWins)
{
	EventManager manager;
	systems::DrawListSystem drawList(&manager);
	EXPECT_EQ(drawList.RebuildCount(), 1); // a rebuild is asked for before the first frame
	manager.Create(events::DrawListRebuildRequested {2});
	EXPECT_EQ(drawList.RebuildCount(), 2);
	manager.Create(events::DrawListRebuildRequested {1});
	EXPECT_EQ(drawList.RebuildCount(), 1);
}

TEST(DrawListSystem, TheHandlerDoesNothingOnceTheServiceHasGone)
{
	EventManager manager;
	{
		systems::DrawListSystem first(&manager);
	}
	systems::DrawListSystem second(&manager);
	second.OnRebuildRequested(0);
	manager.Create(events::DrawListRebuildRequested {2});
	EXPECT_EQ(second.RebuildCount(), 2);
}

TEST(DrawListSystem, OneFrameNearestBlockFirstThenTheStillPass)
{
	const auto a = E(1);
	const auto g = E(2);
	const auto b = E(3);
	draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(a);
	arrays.blocks[32].Insert(b);
	arrays.global.Insert(g);
	// block 0 at (0, 0) and block 32 at (160, 0): the first is nearer the eye at the origin
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	    {.mapPos = {160.0f, 0.0f}, .highestAltitude = 0, .slot = 32, .initial = {}},
	};
	AllAvailable objects;
	systems::DrawListSystem drawList;
	V drawn;
	const draw_list::Consumer consumer = [&drawn, g](entt::entity e) -> std::optional<bool> {
		drawn.push_back(e);
		return e != g; // the outside object is off screen
	};

	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 0, .objects = &objects}, consumer);
	EXPECT_EQ(std::vector<uint16_t>(drawList.VisibleBlocks().begin(), drawList.VisibleBlocks().end()),
	          (std::vector<uint16_t> {0, 1}));
	// the outside objects follow the first block
	EXPECT_EQ(V(drawList.Entries().begin(), drawList.Entries().end()), (V {a, g, b}));
	EXPECT_EQ(drawn, (V {a, g, b}));
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_FALSE(drawList.LandFlag());
	EXPECT_EQ(drawList.RebuildCount(), 0);
	EXPECT_EQ(drawList.RebuildTurn(), 0u);
	EXPECT_TRUE(objects.Listed(a));
	EXPECT_EQ(drawList.ActiveOf(a), std::optional(draw_list::Active::Yes));
	EXPECT_EQ(drawList.ActiveOf(g), std::optional(draw_list::Active::No));
	EXPECT_FALSE(drawList.ActiveOf(E(9)).has_value());

	// the same camera, five turns on: no rebuild, the still pass draws the active entries only
	drawn.clear();
	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 5, .objects = &objects}, consumer);
	EXPECT_FALSE(drawList.LastPassFull());
	EXPECT_EQ(drawn, (V {a, b}));
	EXPECT_EQ(drawList.RebuildTurn(), 0u);

	// the map is cleared: the list empties, the marks stay, so the next rebuild takes none of them again
	drawList.OnClearMap();
	EXPECT_EQ(drawList.Count(), 0u);
	EXPECT_TRUE(objects.Listed(a));
	drawList.OnRebuildRequested(1);
	drawn.clear();
	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 6, .objects = &objects}, consumer);
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_EQ(drawList.Count(), 0u);
	EXPECT_TRUE(drawn.empty());
	EXPECT_EQ(drawList.RebuildTurn(), 6u);
}

TEST(DrawListSystem, DrawnThisFrameIsEveryDrawTheLastUpdateCalled)
{
	const auto a = E(1);
	const auto g = E(2);
	const auto b = E(3);
	draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(a);
	arrays.blocks[32].Insert(b);
	arrays.global.Insert(g);
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	    {.mapPos = {160.0f, 0.0f}, .highestAltitude = 0, .slot = 32, .initial = {}},
	};
	AllAvailable objects;
	systems::DrawListSystem drawList;
	EXPECT_TRUE(drawList.DrawnThisFrame().empty()); // before the first Update
	V calls;
	const draw_list::Consumer consumer = [&calls, g](entt::entity e) -> std::optional<bool> {
		calls.push_back(e);
		return e != g;
	};
	const auto drawn = [&drawList]() { return V(drawList.DrawnThisFrame().begin(), drawList.DrawnThisFrame().end()); };

	// the full pass: every entry, in list order
	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 0, .objects = &objects}, consumer);
	EXPECT_EQ(drawn(), (V {a, g, b}));
	EXPECT_EQ(drawn(), calls);

	// the still pass: the active ones only, the last frame's forgotten
	calls.clear();
	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 1, .objects = &objects}, consumer);
	EXPECT_FALSE(drawList.LastPassFull());
	EXPECT_EQ(drawn(), (V {a, b}));
	EXPECT_EQ(drawn(), calls);

	// an emptied list draws nothing
	drawList.OnClearMap();
	drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = 2, .objects = &objects}, consumer);
	EXPECT_TRUE(drawList.DrawnThisFrame().empty());
}

TEST(DrawListRegistryObjects, TheMarkGoesWithItsEntityAndANewVersionIsNotListed)
{
	ecs::Registry registry;
	draw_list::RegistryObjects objects(registry);
	const auto first = registry.Create();
	EXPECT_FALSE(objects.DontDraw(first));
	registry.AssignState<DontDraw>(first);
	EXPECT_TRUE(objects.DontDraw(first));
	objects.SetListed(first, true);
	EXPECT_TRUE(objects.Listed(first));
	EXPECT_EQ(registry.Size<DrawListed>(), 1u);

	registry.Destroy(first);
	EXPECT_EQ(registry.Size<DrawListed>(), 0u); // the mark went with the entity
	const auto again = registry.Create();
	ASSERT_EQ(entt::to_entity(again), entt::to_entity(first));   // the same index
	ASSERT_NE(entt::to_version(again), entt::to_version(first)); // with a new version
	EXPECT_FALSE(objects.Available(first));
	EXPECT_TRUE(objects.Available(again));
	EXPECT_FALSE(objects.Listed(again));
	EXPECT_FALSE(objects.Listed(first));
	EXPECT_FALSE(objects.DontDraw(again));
	EXPECT_FALSE(objects.Available(entt::null));

	// a rebuild clearing the old entry's mark leaves the new entity's alone, and marking the old one does nothing
	objects.SetListed(again, true);
	objects.SetListed(first, false);
	EXPECT_TRUE(objects.Listed(again));
	objects.SetListed(first, true);
	EXPECT_EQ(registry.Size<DrawListed>(), 1u);
}

TEST(DrawListRegistryObjects, AVillagerIsHumanAndTheCreatureIsComplex)
{
	ecs::Registry registry;
	draw_list::RegistryObjects objects(registry);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature);
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot);

	EXPECT_TRUE(objects.IsHuman(villager));
	EXPECT_FALSE(objects.IsComplex(villager));
	EXPECT_FALSE(objects.IsHuman(creature));
	EXPECT_TRUE(objects.IsComplex(creature));
	// any other object is neither, so its Active flag is its on-screen flag alone
	EXPECT_FALSE(objects.IsHuman(pot));
	EXPECT_FALSE(objects.IsComplex(pot));
	EXPECT_FALSE(objects.IsHuman(entt::null));
	EXPECT_FALSE(objects.IsComplex(entt::null));

	// a destroyed villager or creature answers neither, and an index made again is a plain object
	registry.Destroy(villager);
	registry.Destroy(creature);
	EXPECT_FALSE(objects.IsHuman(villager));
	EXPECT_FALSE(objects.IsComplex(creature));
	const auto again = registry.Create();
	EXPECT_FALSE(objects.IsHuman(again));
	EXPECT_FALSE(objects.IsComplex(again));
}

TEST(DrawListSystem, AnUnavailableObjectIsNulledByBothPassesAndNotCollected)
{
	ecs::Registry registry;
	draw_list::RegistryObjects objects(registry);
	const auto still = registry.Create();
	const auto moved = registry.Create();
	const auto stays = registry.Create();
	const auto late = registry.Create();
	draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(still);
	arrays.blocks[0].Insert(moved);
	arrays.blocks[0].Insert(stays);
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	};
	systems::DrawListSystem drawList;
	V drawn;
	const draw_list::Consumer consumer = [&drawn](entt::entity e) -> std::optional<bool> {
		drawn.push_back(e);
		return true;
	};
	const auto update = [&](uint32_t turn, float focusZ) {
		drawn.clear();
		auto frame = Frame();
		frame.camera.focus.z = focusZ;
		drawList.Update(frame, {.blocks = blocks, .blockArrays = &arrays, .turn = turn, .objects = &objects}, consumer);
	};
	const auto entries = [&drawList]() { return V(drawList.Entries().begin(), drawList.Entries().end()); };

	update(0, 1.0f);
	EXPECT_EQ(drawn, (V {still, moved, stays}));

	// two objects are to be deleted: still in the registry, out of their cells' reach for the list
	registry.AssignState<Unavailable>(still);
	EXPECT_TRUE(registry.Valid(still));
	EXPECT_FALSE(objects.Available(still));
	EXPECT_TRUE(objects.Listed(still)); // the mark stays on an unavailable object

	// the still pass nulls it without drawing it
	update(1, 1.0f);
	EXPECT_FALSE(drawList.LastPassFull());
	EXPECT_EQ(drawn, (V {moved, stays}));
	EXPECT_EQ(entries(), (V {entt::null, moved, stays}));
	EXPECT_EQ(drawList.ActiveOf(still), std::nullopt);

	// the full pass after a camera move, with no rebuild, nulls the other one
	registry.AssignState<Unavailable>(moved);
	update(2, 3.0f);
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_EQ(drawList.RebuildTurn(), 0u);
	EXPECT_EQ(drawn, (V {stays}));
	EXPECT_EQ(entries(), (V {entt::null, entt::null, stays}));

	// the rebuild skips an unavailable object in a reachable array, and does not mark it
	registry.AssignState<Unavailable>(late);
	arrays.blocks[0].Insert(late);
	update(11, 3.0f);
	EXPECT_EQ(drawList.RebuildTurn(), 11u);
	EXPECT_EQ(entries(), (V {stays}));
	EXPECT_EQ(drawn, (V {stays}));
	EXPECT_FALSE(objects.Listed(late));
}

TEST(DrawListSystem, ADestroyedObjectIsNulledAndItsIndexMadeAgainJoinsAtTheNextRebuild)
{
	ecs::Registry registry;
	draw_list::RegistryObjects objects(registry);
	const auto old = registry.Create();
	const auto other = registry.Create();
	draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(old);
	arrays.blocks[0].Insert(other);
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	};
	systems::DrawListSystem drawList;
	V drawn;
	const draw_list::Consumer consumer = [&drawn](entt::entity e) -> std::optional<bool> {
		drawn.push_back(e);
		return true;
	};
	const auto update = [&](uint32_t turn) {
		drawn.clear();
		drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = turn, .objects = &objects}, consumer);
	};
	const auto entries = [&drawList]() { return V(drawList.Entries().begin(), drawList.Entries().end()); };

	update(0);
	EXPECT_EQ(drawn, (V {old, other}));
	// three still frames: no rebuild, and the entries the full pass found active are drawn again
	for (int frame = 0; frame < 3; ++frame)
	{
		update(1);
		EXPECT_FALSE(drawList.LastPassFull());
		EXPECT_EQ(drawn, (V {old, other}));
	}

	// the object goes, and the map files a new entity with the same index where it was
	arrays.blocks[0].Remove(old);
	registry.Destroy(old);
	const auto made = registry.Create();
	ASSERT_EQ(entt::to_entity(made), entt::to_entity(old));
	arrays.blocks[0].Insert(made);

	// the tenth turn after the rebuild: still no rebuild, the old entry is nulled and the new entity waits
	update(10);
	EXPECT_EQ(drawn, (V {other}));
	EXPECT_EQ(entries(), (V {entt::null, other}));
	EXPECT_FALSE(objects.Listed(made));

	// the eleventh rebuilds: the new entity is not taken as listed, so it joins after the other one
	update(11);
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_EQ(drawList.RebuildTurn(), 11u);
	EXPECT_EQ(entries(), (V {other, made}));
	EXPECT_EQ(drawn, (V {other, made}));
	EXPECT_TRUE(objects.Listed(made));
	EXPECT_TRUE(objects.Listed(other));
}

TEST(DrawListSystem, AChangeInAVisibleBlocksLandClipRebuildsTheList)
{
	const std::array<lnd::LNDCell, draw_list::k_BlockCells> flat {};
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}, .coords = {0, 0}, .cells = flat},
	};
	const auto a = E(1);
	draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(a);
	AllAvailable objects;
	systems::DrawListSystem drawList;
	const draw_list::Consumer consumer = [](entt::entity) -> std::optional<bool> { return true; };
	const auto update = [&](const systems::DrawListFrame& frame, uint32_t turn) {
		drawList.Update(frame, {.blocks = blocks, .blockArrays = &arrays, .turn = turn, .objects = &objects}, consumer);
	};
	const auto facing = LandFrame(false);
	const auto away = LandFrame(true);
	const auto behindTheEye = LandFrame(false, -500.0f);

	// the first frame of a land: no result was kept at the load, so the block's triangles raise the flag
	update(facing, 0);
	EXPECT_TRUE(drawList.LandFlag());
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_EQ(drawList.RebuildTurn(), 0u);
	// the same result: no flag, and the turn rule waits
	update(facing, 1);
	EXPECT_FALSE(drawList.LandFlag());
	EXPECT_FALSE(drawList.LastPassFull());
	// seen from behind it keeps no triangle: the change rebuilds the list, ten turns before the turn rule would
	update(away, 2);
	EXPECT_TRUE(drawList.LandFlag());
	EXPECT_TRUE(drawList.LastPassFull());
	EXPECT_EQ(drawList.RebuildTurn(), 2u);
	EXPECT_EQ(drawList.RebuildCount(), 0);
	update(away, 3);
	EXPECT_FALSE(drawList.LandFlag());
	EXPECT_FALSE(drawList.LastPassFull());
	// culled, it has no land clip, and it keeps its last result
	update(behindTheEye, 4);
	EXPECT_TRUE(drawList.VisibleBlocks().empty());
	EXPECT_FALSE(drawList.LandFlag());
	EXPECT_FALSE(drawList.LastPassFull());
	// in view again with the result it had: no flag
	update(away, 5);
	EXPECT_EQ(drawList.VisibleBlocks().size(), 1u);
	EXPECT_FALSE(drawList.LandFlag());
	EXPECT_EQ(drawList.RebuildTurn(), 2u);
	update(facing, 6);
	EXPECT_TRUE(drawList.LandFlag());
	EXPECT_EQ(drawList.RebuildTurn(), 6u);

	// a new land: its blocks start without a result, so the first frame raises the flag again
	drawList.OnClearMap();
	update(facing, 7);
	EXPECT_TRUE(drawList.LandFlag());
	update(facing, 8);
	EXPECT_FALSE(drawList.LandFlag());
}

TEST(DrawListSystem, ABlockWithoutItsCellsHasNoLandClip)
{
	const std::vector<systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	};
	AllAvailable objects;
	systems::DrawListSystem drawList;
	const draw_list::Consumer consumer = [](entt::entity) -> std::optional<bool> { return true; };
	drawList.Update(LandFrame(false), {.blocks = blocks, .blockArrays = nullptr, .turn = 0, .objects = &objects}, consumer);
	EXPECT_EQ(drawList.VisibleBlocks().size(), 1u);
	EXPECT_FALSE(drawList.LandFlag());
}
