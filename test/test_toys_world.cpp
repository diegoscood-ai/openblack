/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The toys of the testbed scenes objects.kick_ball and objects.throw_die, and the others of the toy table, played turn by
// turn in a small world of the game's own systems and data: each toy's model is its own size, a land script's size
// multiplies it, the table's weights are the toys', a skittle knocked over lies where it stops and a die lies as the
// physics leaves it. Every expected value is the original's (docs/wiki/project/progress/nature/toys.md). The models' own sizes
// and the table's weights are facts of the shipped data, which synthetic data can't stand for: those tests run on the
// game's data only.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <variant>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PhysicsEntry.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Transform;

namespace
{
/// The middle of a map cell, well inside the map
const glm::vec2 k_Over {1005.0f, 1205.0f};

/// A toy of the table: its row, its model and, on synthetic data, the box that stands for it
struct Toy
{
	MobileStaticInfo row;
	MeshId model;
	glm::vec3 box;
};
constexpr std::array<Toy, 5> k_Toys {{
    {MobileStaticInfo::ToyBall, MeshId::ObjectToyBall, {2.0f, 2.0f, 2.0f}},
    {MobileStaticInfo::ToyBowlingBall, MeshId::ObjectToyBowlingBall, {1.4f, 1.4f, 1.4f}},
    {MobileStaticInfo::ToyCuddly, MeshId::ObjectToyCuddly, {2.8f, 3.7f, 1.8f}},
    {MobileStaticInfo::ToyDie, MeshId::ObjectToyDice, {2.8f, 2.8f, 2.8f}},
    {MobileStaticInfo::ToySkittle, MeshId::ObjectToySkittle, {1.2f, 3.1f, 1.2f}},
}};

class ToysWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		// each toy's row: its model, and a weight of its own
		for (const auto& toy : k_Toys)
		{
			auto& row = Info().mobileStatic.at(static_cast<size_t>(toy.row));
			row.meshId = toy.model;
			row.weight = 100.0f * toy.box.y;
		}
	}

	/// The scene's toy of the kind
	[[nodiscard]] static MobileStaticInfo SceneToy(std::string_view scene)
	{
		return std::get<MobileStaticInfo>(Scene(scene).objects.at(0).type);
	}

	/// The toy's model into the cache, and the model
	[[nodiscard]] const graphics::L3DMesh& Model(MobileStaticInfo row) const
	{
		const auto& toy = *std::ranges::find(k_Toys, row, &Toy::row);
		const auto id = Info().mobileStatic.at(static_cast<size_t>(row)).meshId;
		// on synthetic data a box of the toy's size standing on its base
		LoadMesh(id, glm::vec3(-0.5f * toy.box.x, 0.0f, -0.5f * toy.box.z),
		         glm::vec3(0.5f * toy.box.x, toy.box.y, 0.5f * toy.box.z));
		return *Locator::resources::value().GetMeshes().Handle(resources::HashIdentifier(id));
	}

	/// A toy of the kind standing on the land, this high above it
	[[nodiscard]] entt::entity MakeToy(MobileStaticInfo row, float scale = 1.0f, glm::vec2 at = k_Over,
	                                   float height = 0.0f) const
	{
		static_cast<void>(Model(row));
		return ecs::archetypes::MobileStaticArchetype::CreateFromInfo(glm::vec3(at.x, k_Ground + height, at.y), row, 0.0f, 0.0f,
		                                                              scale);
	}

	/// Turns until the thing is out of the physics or at rest in it, or the turns are up; whether it came to rest
	bool PlayUntilStill(entt::entity thing, int turns)
	{
		for (int turn = 0; turn < turns; ++turn)
		{
			Turn();
			const auto* po = Dynamics().Find(thing);
			if (po == nullptr || po->body.resting)
			{
				return true;
			}
		}
		return false;
	}
};

[[nodiscard]] glm::vec3 UpOf(const glm::mat3& rotation)
{
	return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
}
} // namespace

// The toy ball is a sphere 2.15 across, the bowling ball 1.40; the cuddly toy is 2.87 wide, 3.81 tall and 1.85 deep; the
// die a cube 2.78 on a side; the skittle 1.23 by 1.26 across and 3.13 tall: each model's box, as the game sizes things
TEST_P(ToysWorld, EachToysModelIsItsOwnSize)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the shipped models' own sizes: no synthetic data stands for them";
	}
	constexpr float k_Measured = 0.006f; // the sizes are given to the hundredth
	const auto near = [](glm::vec3 size, glm::vec3 expected) {
		EXPECT_NEAR(size.x, expected.x, k_Measured);
		EXPECT_NEAR(size.y, expected.y, k_Measured);
		EXPECT_NEAR(size.z, expected.z, k_Measured);
	};
	const auto size = [this](MobileStaticInfo row) { return Model(row).GetBoundingBox().Size(); };
	near(size(SceneToy("objects.kick_ball")), glm::vec3(2.15f));
	near(size(MobileStaticInfo::ToyBowlingBall), glm::vec3(1.40f));
	near(size(MobileStaticInfo::ToyCuddly), glm::vec3(2.87f, 3.81f, 1.85f));
	near(size(SceneToy("objects.throw_die")), glm::vec3(2.78f));
	near(size(MobileStaticInfo::ToySkittle), glm::vec3(1.23f, 3.13f, 1.26f));
}

// The table weighs the ball 100, the skittle 250, the cuddly toy and the die 1000 and the bowling ball 2000
TEST_P(ToysWorld, TheTableWeighsEachToy)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the shipped table's weights: no synthetic data stands for them";
	}
	const auto weight = [this](MobileStaticInfo row) { return Info().mobileStatic.at(static_cast<size_t>(row)).weight; };
	EXPECT_EQ(weight(MobileStaticInfo::ToyBall), 100.0f);
	EXPECT_EQ(weight(MobileStaticInfo::ToySkittle), 250.0f);
	EXPECT_EQ(weight(MobileStaticInfo::ToyCuddly), 1000.0f);
	EXPECT_EQ(weight(MobileStaticInfo::ToyDie), 1000.0f);
	EXPECT_EQ(weight(MobileStaticInfo::ToyBowlingBall), 2000.0f);
}

// A land script's size multiplies the model's: a toy of size 2.62 stands 2.62 times its model's height
TEST_P(ToysWorld, ALandScriptsSizeMultipliesTheModelsSize)
{
	constexpr float k_Size = 2.62f; // the tutorial's giant skittles' smallest
	for (const auto& toy : k_Toys)
	{
		const auto& model = Model(toy.row);
		const auto thing = MakeToy(toy.row, k_Size);
		EXPECT_FLOAT_EQ(ecs::object::GetHeight(thing), model.GetBoundingBox().Size().y * k_Size) << static_cast<int>(toy.row);
	}
}

// A skittle struck hard falls over and lies where it stops: nothing stands it up again
TEST_P(ToysWorld, AKnockedSkittleLiesWhereItStops)
{
	const auto skittle = MakeToy(MobileStaticInfo::ToySkittle);
	const float height = ecs::object::GetHeight(skittle);
	// a bowling ball flying level at two thirds of its height
	const auto ball = MakeToy(MobileStaticInfo::ToyBowlingBall, 1.0f, k_Over + glm::vec2(6.0f, 0.0f), height * 0.6f);
	ASSERT_NE(Dynamics().AddObject(ball, glm::vec3(-20.0f, 0.0f, 0.0f), glm::vec3(0.0f)), nullptr);
	bool knocked = false;
	for (int turn = 0; turn < 30 && !knocked; ++turn)
	{
		Turn();
		knocked = Dynamics().Find(skittle) != nullptr && !Dynamics().Find(skittle)->body.resting;
	}
	ASSERT_TRUE(knocked);
	ASSERT_TRUE(PlayUntilStill(skittle, 300));
	const auto& lying = Registry().Get<const Transform>(skittle);
	EXPECT_LT(std::abs(UpOf(lying.rotation).y), 0.5f);
	const auto rest = lying.position;
	const auto turned = lying.rotation;
	for (int turn = 0; turn < 50; ++turn)
	{
		Turn();
	}
	EXPECT_EQ(Registry().Get<const Transform>(skittle).position, rest);
	EXPECT_EQ(Registry().Get<const Transform>(skittle).rotation, turned);
}

// A die thrown spinning comes to rest as the physics leaves it, on a face or leaning on an edge, and stays so: nothing
// turns it onto a face or reads which face is up
TEST_P(ToysWorld, AThrownDieLiesAsThePhysicsLeavesIt)
{
	const auto die = MakeToy(SceneToy("objects.throw_die"), 1.0f, k_Over, 5.0f);
	ASSERT_NE(Dynamics().AddObject(die, glm::vec3(6.0f, 3.0f, 2.0f), glm::vec3(2.0f, 1.0f, -3.0f)), nullptr);
	ASSERT_TRUE(PlayUntilStill(die, 400));
	const auto rest = Registry().Get<const Transform>(die);
	for (int turn = 0; turn < 50; ++turn)
	{
		Turn();
	}
	EXPECT_EQ(Registry().Get<const Transform>(die).position, rest.position);
	EXPECT_EQ(Registry().Get<const Transform>(die).rotation, rest.rotation);
}

INSTANTIATE_TEST_SUITE_P(Data, ToysWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
