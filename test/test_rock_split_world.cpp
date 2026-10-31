/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rocks of the testbed scene physics.rock_split, played turn by turn in a small world of the game's own systems
// and data: each hard landing wears a rock by the knock's G past 4, a rock worn below a hundredth splits into two rocks
// of its kind at 0.7935 of its scale, either side of it along the game's random angle, flying on with its velocity and
// half its spin or starting still, and a burning rock's halves burn on in its blaze. Every expected value is the
// original's (docs/wiki/physics/physics-impacts.md#rocks-that-split,
// docs/wiki/project/progress/nature/rocks_splitting_and_heat.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <bit>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlacedTurn.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Life;
using ecs::components::MobileStatic;
using ecs::components::Transform;

namespace
{
/// The scene's first rock, its throw and the point it is thrown over
constexpr std::string_view k_Scene = "physics.rock_split";
const glm::vec2 k_Over {1000.0f, 1200.0f};
/// The original's first draw of GameFloatRand(2 pi) from a new campaign's streams
const float k_FirstAngle = std::bit_cast<float>(0x408A11D0u);

class RockSplitWorld: public test::world::WorldHarness
{
protected:
	[[nodiscard]] static MobileStaticInfo SceneRockType()
	{
		return std::get<MobileStaticInfo>(Scene(k_Scene).objects.at(0).type);
	}

	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		// the rock's row: of the rock kind, its model a box, burning at 500 and holding heat as 4000, as every rock
		auto& row = Info().mobileStatic.at(static_cast<size_t>(SceneRockType()));
		row.mobileType = static_cast<MobileStaticInfo>(2);
		row.meshId = MeshId::ObjectRockVolcanic;
		row.weight = 400.0f;
		row.combustionTemperature = 500.0f;
		row.heatCapacity = 4000.0f;
	}

	/// A rock of the scene's first kind, standing on the land
	[[nodiscard]] entt::entity Rock(float scale, float yAngle = 0.0f, glm::vec2 at = k_Over) const
	{
		const auto type = SceneRockType();
		// on synthetic data a box 2 m wide and 1.5 m tall, standing on its base
		LoadMesh(Info().mobileStatic.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 1.5f, 1.0f));
		return ecs::archetypes::MobileStaticArchetype::CreateFromInfo(glm::vec3(at.x, k_Ground, at.y), type, 0.0f, yAngle,
		                                                              scale);
	}

	/// The scene's first rock at the scene's scale, thrown as the scene throws it: lifted above the land and let go at the
	/// throw's velocity
	[[nodiscard]] entt::entity HurledRock() const
	{
		const auto& scene = Scene(k_Scene);
		const auto rock = Rock(scene.objects.at(0).scale);
		Hurl(rock, scene.throws.at(0).height, scene.throws.at(0).velocity);
		return rock;
	}

	void Hurl(entt::entity rock, float height, glm::vec3 velocity) const
	{
		auto& transform = Registry().Get<Transform>(rock);
		transform.position.y = k_Ground + height;
		ASSERT_NE(Dynamics().AddObject(rock, velocity, glm::vec3(0.0f)), nullptr);
	}

	/// A thing's life: full when it has none of its own yet
	[[nodiscard]] static float LifeOf(entt::entity thing)
	{
		const auto* life = Registry().TryGet<const Life>(thing);
		return life != nullptr ? life->value : 1.0f;
	}

	/// Every rock of the scene's kind but this one, in the order they were made
	[[nodiscard]] static std::vector<entt::entity> OtherRocks(entt::entity rock)
	{
		std::vector<entt::entity> found;
		Registry().Each<const MobileStatic>([&](entt::entity thing, const MobileStatic& statics) {
			if (thing != rock && statics.type == SceneRockType())
			{
				found.push_back(thing);
			}
		});
		std::ranges::sort(found);
		return found;
	}

	/// Turns until the rock is gone or the turns are up; whether it went
	bool PlayUntilGone(entt::entity rock, int turns)
	{
		for (int turn = 0; turn < turns; ++turn)
		{
			Turn();
			if (!Registry().Valid(rock))
			{
				return true;
			}
		}
		return false;
	}

	/// The rock in the physics, flying, with a new campaign's random streams, so the split's draw is their first
	ecs::PhysicsEntry& FlyingRock(entt::entity rock) const
	{
		auto* po = Dynamics().AddObject(rock, glm::vec3(0.0f), glm::vec3(0.0f));
		EXPECT_NE(po, nullptr);
		Locator::gameRandom::emplace<GameRandomTesting>(
		    GameRandomSeeds {.synced = game_random::k_InitialSeed, .local = game_random::k_InitialSeed});
		return *po;
	}
};
} // namespace

// Every hard knock takes (G - 4) x 0.005 of the rock's life, G the knock in the rock's weights (the impact over its
// mass times gravity); the first makes its life, full before
TEST_P(RockSplitWorld, EachHardLandingTakesAFiveHundredthOfALifeForEachGPastFour)
{
	const auto rock = HurledRock();
	int knocks = 0;
	for (int turn = 0; turn < 300 && Registry().Valid(rock); ++turn)
	{
		const float before = LifeOf(rock);
		Turn();
		if (!Registry().Valid(rock) || LifeOf(rock) == before)
		{
			continue;
		}
		const auto* po = Dynamics().Find(rock);
		ASSERT_NE(po, nullptr);
		const float g = po->GLoad();
		EXPECT_GT(g, 4.0f);
		EXPECT_EQ(LifeOf(rock), before - (g - 4.0f) * 0.005f) << "turn " << turn;
		++knocks;
	}
	EXPECT_GT(knocks, 0);
}

// A rock let go just above the land lands softly, no harder than four of its weights, and is not worn
TEST_P(RockSplitWorld, ASoftLandingDoesNotWearTheRock)
{
	const auto rock = Rock(Scene(k_Scene).objects.at(0).scale);
	Hurl(rock, 0.5f, glm::vec3(0.0f));
	for (int turn = 0; turn < 100; ++turn)
	{
		Turn();
		if (const auto* po = Dynamics().Find(rock))
		{
			EXPECT_LE(po->GLoad(), 4.0f) << "turn " << turn;
		}
		EXPECT_EQ(LifeOf(rock), 1.0f) << "turn " << turn;
	}
}

// A rock no taller than 0.7 m is never worn, however hard it lands
TEST_P(RockSplitWorld, ARockNoTallerThanSevenTenthsOfAMetreIsNeverWorn)
{
	const auto& scene = Scene(k_Scene);
	const auto probe = Rock(1.0f);
	const float scale = 0.7f / ecs::object::GetHeight(probe);
	ecs::ToBeDeleted(probe);
	ecs::ProcessDeadList(true);
	const auto rock = Rock(scale);
	ASSERT_LE(ecs::object::GetHeight(rock), 0.7f);
	Hurl(rock, scene.throws.at(0).height, scene.throws.at(0).velocity);
	for (int turn = 0; turn < 300; ++turn)
	{
		Turn();
		ASSERT_TRUE(Registry().Valid(rock));
		EXPECT_EQ(LifeOf(rock), 1.0f) << "turn " << turn;
	}
}

// A rock worn below a hundredth of a life by its landing splits into two rocks of its kind, each 0.7935 of its scale and
// at full life; the rock goes
TEST_P(RockSplitWorld, ARockWornOutByItsLandingSplitsIntoTwoRocksOfItsKindAtFullLife)
{
	const auto& scene = Scene(k_Scene);
	const auto rock = HurledRock();
	// any knock past 6 G takes it below a hundredth
	Registry().Assign<Life>(rock, Life {.value = 0.0199f});
	ASSERT_TRUE(PlayUntilGone(rock, 300));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	for (const auto half : halves)
	{
		EXPECT_EQ(Registry().Get<const Transform>(half).scale.x, scene.objects.at(0).scale * 0.7935f);
		EXPECT_EQ(LifeOf(half), 1.0f);
	}
}

// The halves stand either side of the rock, each 0.7935 of its 2D radius from it along the angle of the game's first
// random draw, turned to the rock's heading
TEST_P(RockSplitWorld, TheHalvesStandEitherSideAlongTheGamesRandomAngleAtTheRocksHeading)
{
	const float yAngle = 0.5f;
	const auto rock = Rock(2.0f, yAngle);
	const float offset = ecs::object::Get2DRadius(rock) * 0.7935f;
	const auto centre = map_coords::FromWorld(Registry().Get<const Transform>(rock).position);
	auto& po = FlyingRock(rock);
	Registry().Assign<Life>(rock, Life {.value = 0.005f});
	EXPECT_FALSE(ecs::object_physics::KnockRock(rock, po, 10.0f));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	const auto o = gutils::GetPosFromAngle(k_FirstAngle, offset);
	const auto plus = map_coords::ToWorld(centre + o);
	const auto minus = map_coords::ToWorld(centre - o);
	const auto& first = Registry().Get<const Transform>(halves.at(0)).position;
	const auto& second = Registry().Get<const Transform>(halves.at(1)).position;
	EXPECT_EQ(glm::vec2(first.x, first.z), glm::vec2(plus.x, plus.z));
	EXPECT_EQ(glm::vec2(second.x, second.z), glm::vec2(minus.x, minus.z));
	for (const auto half : halves)
	{
		EXPECT_EQ(ecs::placed_turn::AnglesOf(Registry(), half).y, yAngle);
	}
}

// A flying rock's halves fly on at its velocity with half its spin
TEST_P(RockSplitWorld, AFlyingRocksHalvesFlyOnAtItsVelocityWithHalfItsSpin)
{
	const auto rock = Rock(2.0f);
	auto& po = FlyingRock(rock);
	const glm::vec3 velocity(3.0f, -20.0f, 1.5f);
	const glm::vec3 spin(0.25f, 0.5f, -0.75f);
	po.body.velocity = velocity;
	po.body.angularMomentum = spin;
	po.body.resting = false;
	Registry().Assign<Life>(rock, Life {.value = 0.005f});
	EXPECT_FALSE(ecs::object_physics::KnockRock(rock, po, 10.0f));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	for (const auto half : halves)
	{
		const auto* own = Dynamics().Find(half);
		ASSERT_NE(own, nullptr);
		EXPECT_EQ(own->body.velocity, velocity);
		EXPECT_EQ(own->body.angularMomentum, spin * 0.5f);
	}
}

// A resting rock's halves start still
TEST_P(RockSplitWorld, ARestingRocksHalvesStartStill)
{
	const auto rock = Rock(2.0f);
	auto& po = FlyingRock(rock);
	po.body.velocity = glm::vec3(3.0f, -20.0f, 1.5f);
	po.body.angularMomentum = glm::vec3(0.25f, 0.5f, -0.75f);
	po.body.resting = true;
	Registry().Assign<Life>(rock, Life {.value = 0.005f});
	EXPECT_FALSE(ecs::object_physics::KnockRock(rock, po, 10.0f));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	for (const auto half : halves)
	{
		const auto* own = Dynamics().Find(half);
		ASSERT_NE(own, nullptr);
		EXPECT_EQ(own->body.velocity, glm::vec3(0.0f));
		EXPECT_EQ(own->body.angularMomentum, glm::vec3(0.0f));
	}
}

// A knock from a thing that is a rock never wears a rock
TEST_P(RockSplitWorld, AKnockFromARockDoesNotWearARock)
{
	const auto rock = Rock(2.0f);
	const auto other = Rock(2.0f, 0.0f, k_Over + glm::vec2(20.0f, 0.0f));
	auto& po = FlyingRock(rock);
	ASSERT_NE(Dynamics().AddObject(other, glm::vec3(0.0f), glm::vec3(0.0f)), nullptr);
	const auto& objects = Dynamics().GetState().objects;
	const auto slot = std::ranges::find_if(objects, [other](const auto& entry) { return entry->entity == other; });
	ASSERT_NE(slot, objects.end());
	po.hitBySlot = static_cast<uint32_t>(slot - objects.begin());
	EXPECT_TRUE(ecs::object_physics::KnockRock(rock, po, 50.0f));
	EXPECT_EQ(LifeOf(rock), 1.0f);
	EXPECT_FALSE(Registry().AllOf<Life>(rock));
}

// A burning rock's halves both take its fire, as hot as the rock was, and burn on as one blaze: neither half's fire
// leaves it as the halves fly apart, and the rock's own fire goes with the rock
TEST_P(RockSplitWorld, ABurningRocksHalvesBurnOnAsOneBlazeAsHotAsItWas)
{
	auto& fires = Fires();
	const auto rock = Rock(2.0f);
	fires.SetOnFire(rock, 1.0f);
	auto& po = FlyingRock(rock);
	po.body.velocity = glm::vec3(3.0f, 10.0f, 1.5f);
	po.body.resting = false;
	const float temperature = fires.GetTemperature(rock);
	Registry().Assign<Life>(rock, Life {.value = 0.005f});
	EXPECT_FALSE(ecs::object_physics::KnockRock(rock, po, 10.0f));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	const auto blaze = [&fires](entt::entity half) {
		const auto* fire = fires.FindByObject(half);
		return fire != nullptr ? fire->root : nullptr;
	};
	for (const auto half : halves)
	{
		ASSERT_NE(fires.FindByObject(half), nullptr);
		EXPECT_EQ(fires.FindByObject(half)->temperature, temperature);
	}
	EXPECT_EQ(blaze(halves.at(0)), blaze(halves.at(1)));
	// the rock and its fire go at the turn's end; the halves fly apart, still one blaze
	for (int turn = 0; turn < 10; ++turn)
	{
		Turn();
		EXPECT_NE(blaze(halves.at(0)), nullptr) << "turn " << turn;
		EXPECT_EQ(blaze(halves.at(0)), blaze(halves.at(1))) << "turn " << turn;
	}
	EXPECT_FALSE(Registry().Valid(rock));
	EXPECT_EQ(fires.FindByObject(rock), nullptr);
}

INSTANTIATE_TEST_SUITE_P(Data, RockSplitWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
