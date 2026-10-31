/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fireflies (ECS/FireFlies) on a fake land: at most fifty at once, out at dusk to the nearest light found within
// 300 m and two metres over it, home at dawn to the nearest tree or rock, and caught one at a time when the hand picks
// up the tree they hide in. The values are the original game's: the cap, the search radius, the two metres, the 15 m
// to the side and the hovering height of 4 when nothing is found.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FireFlies.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "Worship/FireFlyReward.h"
#include "support/FileSystemFakes.h"
#include "support/RestoreService.h"
#include "support/SkyFrameGuard.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The original's cap on the fireflies of a land
constexpr size_t k_MaxFireFlies = 50;
/// Where the first firefly hides: a tree in the middle of a cell edge, whole metres a map coordinate holds exactly
constexpr glm::vec3 k_Perch {1000.0f, 0.0f, 1000.0f};
/// The drift of a firefly out of hiding drawn at game time 0 with every phase drawn as 0: the big orbit's 8 m plus
/// the small one's 1 m, both along x
constexpr glm::vec3 k_FullDrift {9.0f, 0.0f, 0.0f};

class FireFlies: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		// every tree row of info.dat is a FOREST_TREE (the map cells read the row's type)
		for (auto& tree : info->tree)
		{
			tree.type = ObjectType::ForestTree;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		// no land: the ground is at 0 everywhere. The sprite sheet is not found, so the sprites keep no texture
		Locator::terrainSystem::reset();
		Locator::filesystem::emplace<test::MemoryFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
		ecs::ClearFireFlies();
		// a cell is always searched and its search never ends early, the spawn takes a tree, and the first of a list
		// (the newest; of two, the older); every float drawn is 0
		game_random::testing::SetGameRand([](uint32_t n) { return n == 2 || n == 3 ? 1u : 0u; },
		                                  [](float /*range*/) { return 0.0f; });
		_evening.Reset();
		_evening.SetCycle(DayNightClock::k_DefaultDuration, 0.5f, 0.0f);
		_evening.SetScriptTime(22.25f);
		_morning.Reset();
		_morning.SetScriptTime(10.0f);
	}
	void TearDown() override
	{
		game_random::testing::SetGameRand({}, {});
		ecs::ClearFireFlies();
		ecs::ClearForests();
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A tree in its cell's fixed list
	static entt::entity TreeAt(const glm::vec3& position)
	{
		const auto tree = Reg().Create();
		ecs::object_index::Assign(tree);
		Reg().Assign<Transform>(tree, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f});
		ecs::map_cells::InsertMapObject(tree);
		return tree;
	}
	/// A street lantern in its cell
	static entt::entity LanternAt(const glm::vec3& position)
	{
		const auto lantern = Reg().Create();
		ecs::object_index::Assign(lantern);
		Reg().Assign<Transform>(lantern, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<StreetLantern>(lantern);
		ecs::map_cells::InsertMapObject(lantern);
		return lantern;
	}

	void Evening(int turns) const
	{
		for (int i = 0; i < turns; ++i)
		{
			ecs::ProcessFireFliesTurn(_evening);
		}
	}
	void Morning(int turns) const
	{
		for (int i = 0; i < turns; ++i)
		{
			ecs::ProcessFireFliesTurn(_morning);
		}
	}
	/// One sprite a firefly
	[[nodiscard]] static size_t Count() { return Reg().Size<Sprite>(); }
	/// Where every firefly is drawn at game time 0
	[[nodiscard]] static std::vector<glm::vec3> Drawn()
	{
		ecs::UpdateFireFlies(0.0f, glm::vec3(0.0f));
		std::vector<glm::vec3> positions;
		Reg().Each<const Sprite, const Transform>(
		    [&positions](entt::entity /*unused*/, const Sprite& /*unused*/, const Transform& transform) {
			    positions.push_back(transform.position);
		    });
		return positions;
	}
	static void ExpectAllAt(const std::vector<glm::vec3>& drawn, const glm::vec3& at)
	{
		for (const auto& position : drawn)
		{
			EXPECT_FLOAT_EQ(position.x, at.x);
			EXPECT_FLOAT_EQ(position.y, at.y);
			EXPECT_FLOAT_EQ(position.z, at.z);
		}
	}

	/// After dusk (visual hour 21, night) and in the morning (before noon, day)
	DayNightClock _evening;
	DayNightClock _morning;

private:
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::filesystem> _restoreFileSystem;
	test::RestoreService<Locator::resources> _restoreResources;
	test::SkyFrameGuard _sky;
};
} // namespace

TEST_F(FireFlies, ALandHoldsAtMostFifty)
{
	ASSERT_GT(_evening.GetVisualTime(), 12.0f);
	ASSERT_GT(_evening.SkyType(), 1.0f);
	ASSERT_LT(_morning.GetVisualTime(), 12.0f);
	ASSERT_LT(_morning.SkyType(), 1.0f);
	for (int i = 0; i < 60; ++i)
	{
		TreeAt(glm::vec3(1000.0f + 10.0f * static_cast<float>(i), 0.0f, 2000.0f));
	}
	// sixty trees, and fifty fireflies come out on the first evening turn
	Evening(1);
	EXPECT_EQ(Count(), k_MaxFireFlies);
	// the next evening makes none while fifty are still about
	Morning(1);
	Evening(1);
	EXPECT_EQ(Count(), k_MaxFireFlies);
}

TEST_F(FireFlies, ComingOutItHoversTwoMetresAboveTheNearestLightItFinds)
{
	TreeAt(k_Perch);
	// the search goes round the cells in rings: a light four rings out at 56.6 m is found first, and one five rings
	// out at 50 m takes its place as the nearer
	LanternAt(glm::vec3(1040.0f, 0.0f, 1040.0f));
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	// one comes out each evening turn, and each flies at most some 28 s
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), glm::vec3(1050.0f, 2.0f, 1000.0f) + k_FullDrift);
}

TEST_F(FireFlies, WithNoLightWithinThreeHundredMetresItHovers15MetresAsideAtFour)
{
	TreeAt(k_Perch);
	// 320 m away: past the 3600 cells of the search round its tree
	LanternAt(glm::vec3(1320.0f, 0.0f, 1000.0f));
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), glm::vec3(1015.0f, 4.0f, 1000.0f) + k_FullDrift);
}

TEST_F(FireFlies, GoingHomeItHidesInTheNearestTreeOrRock)
{
	// of two trees, the newer comes first in the tree list and the spawn takes the older, at the perch
	TreeAt(k_Perch);
	const auto nearer = TreeAt(glm::vec3(1080.0f, 0.0f, 1000.0f));
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), glm::vec3(1050.0f, 2.0f, 1000.0f) + k_FullDrift);
	// from the light, the tree 30 m away rather than its own 50 m away; resting, it is drawn with no drift
	Morning(300);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), Reg().Get<const Transform>(nearer).position);
}

TEST_F(FireFlies, WithNoTreeOrRockToGoHomeToItRests15MetresAsideOnTheGroundAndGoesAtNightfall)
{
	const auto perch = TreeAt(k_Perch);
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ecs::DeleteTree(perch);
	Morning(300);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), glm::vec3(1065.0f, 0.0f, 1000.0f));
	// at nightfall the first in the list finds nothing where it rests and goes, one each evening turn
	Evening(1);
	EXPECT_EQ(Count(), k_MaxFireFlies - 1);
	Evening(static_cast<int>(k_MaxFireFlies) - 1);
	EXPECT_EQ(Count(), 0u);
}

TEST_F(FireFlies, PickingUpTheTreeItHidesInCatchesIt)
{
	const auto perch = TreeAt(k_Perch);
	const auto home = TreeAt(glm::vec3(1080.0f, 0.0f, 1000.0f));
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	Morning(300);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	// the tree they came out of is empty now
	worship::fire_fly::OnPlacedInMagicHand(perch);
	EXPECT_EQ(Count(), k_MaxFireFlies);
	// the tree they went home to has them all
	worship::fire_fly::OnPlacedInMagicHand(home);
	EXPECT_EQ(Count(), k_MaxFireFlies - 1);
}

TEST_F(FireFlies, OnePickUpCatchesOneFirefly)
{
	TreeAt(k_Perch);
	const auto home = TreeAt(glm::vec3(1080.0f, 0.0f, 1000.0f));
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	Morning(300);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	// fifty rest in one tree: each pick-up of it takes one
	worship::fire_fly::OnPlacedInMagicHand(home);
	EXPECT_EQ(Count(), k_MaxFireFlies - 1);
	worship::fire_fly::OnPlacedInMagicHand(home);
	EXPECT_EQ(Count(), k_MaxFireFlies - 2);
}

TEST_F(FireFlies, OnesOutHoveringAreNotCaught)
{
	const auto perch = TreeAt(k_Perch);
	const auto lantern = LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	// all out by the house: neither the tree they left nor the lantern they hover by gives one
	worship::fire_fly::OnPlacedInMagicHand(perch);
	worship::fire_fly::OnPlacedInMagicHand(lantern);
	EXPECT_EQ(Count(), k_MaxFireFlies);
}

TEST_F(FireFlies, TheTreeMustBeAtTheirSpotsAltitudeToo)
{
	TreeAt(k_Perch);
	const auto home = TreeAt(glm::vec3(1080.0f, 0.0f, 1000.0f));
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	Morning(300);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	// the same x and z a metre higher is another spot
	Reg().Get<Transform>(home).position.y = 1.0f;
	worship::fire_fly::OnPlacedInMagicHand(home);
	EXPECT_EQ(Count(), k_MaxFireFlies);
	Reg().Get<Transform>(home).position.y = 0.0f;
	worship::fire_fly::OnPlacedInMagicHand(home);
	EXPECT_EQ(Count(), k_MaxFireFlies - 1);
}

TEST_F(FireFlies, InTheMorningTheyGoHomeOneEachTurn)
{
	TreeAt(k_Perch);
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	// the afternoon sends none home and none out, but lets the flights already begun end
	DayNightClock afternoon;
	afternoon.Reset();
	afternoon.SetScriptTime(14.0f);
	const auto atHome = []() {
		const auto drawn = Drawn();
		return std::ranges::count_if(drawn, [](const glm::vec3& at) { return glm::distance(at, k_Perch) < 0.01f; });
	};
	ASSERT_EQ(atHome(), 0);
	// each morning turn sends the next one home, and only it
	for (int turn = 1; turn <= 3; ++turn)
	{
		Morning(1);
		for (int i = 0; i < 300; ++i)
		{
			ecs::ProcessFireFliesTurn(afternoon);
		}
		EXPECT_EQ(atHome(), turn);
	}
}

TEST_F(FireFlies, BetweenMorningAndDuskAndBetweenNightAndDawnNothingChanges)
{
	// late morning to dusk: the visual hour past noon, a day sky; the dark hours to dawn: before noon, a night sky
	DayNightClock afternoon;
	afternoon.Reset();
	afternoon.SetScriptTime(14.0f);
	ASSERT_GT(afternoon.GetVisualTime(), 12.0f);
	ASSERT_LT(afternoon.SkyType(), 1.0f);
	DayNightClock beforeDawn;
	beforeDawn.Reset();
	beforeDawn.SetCycle(DayNightClock::k_DefaultDuration, 0.5f, 0.0f);
	beforeDawn.SetScriptTime(3.0f);
	ASSERT_LT(beforeDawn.GetVisualTime(), 12.0f);
	ASSERT_GT(beforeDawn.SkyType(), 1.0f);
	TreeAt(k_Perch);
	LanternAt(glm::vec3(1050.0f, 0.0f, 1000.0f));
	// in the afternoon none are made
	for (int i = 0; i < 100; ++i)
	{
		ecs::ProcessFireFliesTurn(afternoon);
	}
	EXPECT_EQ(Count(), 0u);
	// out after dusk, they stay out through the dark hours and into the afternoon
	Evening(400);
	ASSERT_EQ(Count(), k_MaxFireFlies);
	const glm::vec3 atTheLight = glm::vec3(1050.0f, 2.0f, 1000.0f) + k_FullDrift;
	for (int i = 0; i < 300; ++i)
	{
		ecs::ProcessFireFliesTurn(beforeDawn);
	}
	EXPECT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), atTheLight);
	for (int i = 0; i < 300; ++i)
	{
		ecs::ProcessFireFliesTurn(afternoon);
	}
	EXPECT_EQ(Count(), k_MaxFireFlies);
	ExpectAllAt(Drawn(), atTheLight);
}
