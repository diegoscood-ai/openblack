/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::land_reseat: after the vortex or the temple flattens the land, the villagers, animals and wood and food piles
// standing on it keep their height above the new ground; everything else stays. Fake grounds, no game data

#include <bit>
#include <memory>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/MobileWalkPath.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LandReseat.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::ecs::land_reseat::ReseatedHeight;

namespace
{
/// The box of corners (10, 10) .. (14, 14): the ground in cells 9 .. 14 reads it
constexpr glm::ivec2 k_Min {10, 10};
constexpr glm::ivec2 k_Max {14, 14};

float FlatGround(glm::vec2)
{
	return 20.0f;
}

/// The land raised by 5 m in cells 10 .. 13, as after a flattening; cells 9 and 14 read the box but kept their ground
float RaisedGround(glm::vec2 xz)
{
	const bool inside = xz.x >= 100.0f && xz.x < 140.0f && xz.y >= 100.0f && xz.y < 140.0f;
	return inside ? 25.0f : 20.0f;
}

bool NeverOff(entt::entity)
{
	return false;
}

entt::entity Make(Registry& registry, glm::vec3 position)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}

entt::entity MakeVillager(Registry& registry, glm::vec3 position)
{
	const auto entity = Make(registry, position);
	registry.Assign<Villager>(entity);
	return entity;
}

entt::entity MakePile(Registry& registry, glm::vec3 position, float sink)
{
	const auto entity = Make(registry, position);
	registry.Assign<Pot>(entity);
	auto& pileSink = registry.Assign<PileSink>(entity, position.y);
	pileSink.offset.SetPosition(sink);
	registry.Get<Transform>(entity).position.y = position.y + sink;
	return entity;
}

/// Records on the flat ground, then re-seats on the raised one
size_t Flatten(Registry& registry)
{
	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	return land_reseat::Reseat(registry, grounds, RaisedGround);
}
} // namespace

TEST(LandReseat, UnchangedGroundGivesNothing)
{
	EXPECT_FALSE(ReseatedHeight(12.5f, 10.0f, 10.0f).has_value());
	// bitwise: -0 and +0 are different grounds
	EXPECT_TRUE(ReseatedHeight(1.0f, 0.0f, -0.0f).has_value());
}

TEST(LandReseat, KeepsTheHeightAboveTheGround)
{
	EXPECT_EQ(ReseatedHeight(10.0f, 10.0f, 15.0f), 15.0f); // raised, on the ground
	EXPECT_EQ(ReseatedHeight(12.0f, 10.0f, 4.0f), 6.0f);   // dropped, 2 m above it
}

TEST(LandReseat, TakesTheHeightAboveTheGroundFirst)
{
	// the height above the ground is taken first, then added to the new ground: (y - before) + after, not
	// (y + after) - before, which rounds differently
	const float y = 0.1f;
	const float before = 1000.3f;
	const float after = 1000.7f;
	const float aboveGround = y - before;
	const auto reseated = ReseatedHeight(y, before, after);
	ASSERT_TRUE(reseated.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(*reseated), std::bit_cast<uint32_t>(after + aboveGround));
}

TEST(LandReseat, GroundReadsTheCornersOfItsCell)
{
	// a point in cell 9 reads corners 9 and 10, in cell 14 corners 14 and 15
	EXPECT_TRUE(land_reseat::GroundReadsCorners({95.0f, 120.0f}, k_Min, k_Max));
	EXPECT_TRUE(land_reseat::GroundReadsCorners({145.0f, 145.0f}, k_Min, k_Max));
	EXPECT_FALSE(land_reseat::GroundReadsCorners({85.0f, 120.0f}, k_Min, k_Max));
	EXPECT_FALSE(land_reseat::GroundReadsCorners({120.0f, 155.0f}, k_Min, k_Max));
}

TEST(LandReseat, StandingVillagerAndAnimalFollowTheGround)
{
	Registry registry;
	const auto villager = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	const auto bird = Make(registry, {121.0f, 30.0f, 121.0f}); // 10 m above the ground
	registry.Assign<Animal>(bird);
	registry.Assign<AnimalBrain>(bird);

	EXPECT_EQ(Flatten(registry), 2u);
	EXPECT_EQ(registry.Get<Transform>(villager).position, glm::vec3(120.0f, 25.0f, 120.0f));
	EXPECT_EQ(registry.Get<Transform>(bird).position, glm::vec3(121.0f, 35.0f, 121.0f));
}

TEST(LandReseat, PileMovesItsSinkBase)
{
	Registry registry;
	const auto pile = MakePile(registry, {120.0f, 20.0f, 120.0f}, -0.5f);

	EXPECT_EQ(Flatten(registry), 1u);
	EXPECT_EQ(registry.Get<PileSink>(pile).baseY, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(pile).position.y, 24.5f);
}

TEST(LandReseat, StaticsStay)
{
	Registry registry;
	const auto abode = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<Abode>(abode);
	const auto tree = Make(registry, {121.0f, 20.0f, 121.0f});
	registry.Assign<Tree>(tree);
	const auto pot = Make(registry, {122.0f, 20.0f, 122.0f}); // a pot at rest has no sink offset
	registry.Assign<Pot>(pot);

	EXPECT_EQ(Flatten(registry), 0u);
	EXPECT_EQ(registry.Get<Transform>(abode).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(tree).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(pot).position.y, 20.0f);
}

TEST(LandReseat, WalkersFollowTheGround)
{
	Registry registry;
	const auto pathWalker = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<MoveStateLinearTag>(pathWalker);
	const auto trackWalker = MakeVillager(registry, {121.0f, 20.0f, 121.0f});
	registry.Assign<LivingWalkPath>(trackWalker);
	const auto movingAnimal = Make(registry, {122.0f, 20.0f, 122.0f});
	registry.Assign<Animal>(movingAnimal);
	registry.Assign<AnimalBrain>(movingAnimal).movedLastTurn = 0.3f;

	EXPECT_EQ(Flatten(registry), 3u);
	EXPECT_EQ(registry.Get<Transform>(pathWalker).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(trackWalker).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(movingAnimal).position.y, 25.0f);
}

TEST(LandReseat, ThingsOffTheGroundAreLeft)
{
	Registry registry;
	const auto carried = MakeVillager(registry, {122.0f, 20.0f, 122.0f});
	registry.Assign<CarriedByTornado>(carried);
	const auto gone = MakeVillager(registry, {123.0f, 20.0f, 123.0f});
	registry.Assign<Unavailable>(gone);
	const auto held = MakeVillager(registry, {124.0f, 20.0f, 124.0f});

	const auto grounds =
	    land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, [held](entt::entity thing) { return thing == held; });
	EXPECT_TRUE(grounds.empty());
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
	EXPECT_EQ(registry.Get<Transform>(carried).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(gone).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(held).position.y, 20.0f);
}

TEST(LandReseat, ThingsOutsideTheBoxOrOnUnchangedGroundStay)
{
	Registry registry;
	const auto far = MakeVillager(registry, {300.0f, 20.0f, 300.0f});
	// in a cell that reads the box, but whose ground did not change
	const auto edge = MakeVillager(registry, {145.0f, 20.0f, 120.0f});

	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	ASSERT_EQ(grounds.size(), 1u);
	EXPECT_EQ(grounds.front().thing, edge);
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
	EXPECT_EQ(registry.Get<Transform>(far).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(edge).position.y, 20.0f);
}

TEST(LandReseat, ThingsThatWentAwayAreSkipped)
{
	Registry registry;
	const auto villager = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	registry.Destroy(villager);
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
}
