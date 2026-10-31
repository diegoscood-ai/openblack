/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Livings' axes in the physics (docs/wiki/physics/physics.md#bodies): the draw's quarter turn, SetUpPos with rows that
// already carry the scale (the object's world matrix) and the rotation set from DecomposeYXZ(rows) per class.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <memory>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "support/DynamicsFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::physics::Body;
using openblack::physics::PhysicsData;

namespace
{
void ExpectNear(const glm::mat3& a, const glm::mat3& b, float eps)
{
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(a[c][r], b[c][r], eps) << "column " << c << " row " << r;
		}
	}
}

void ExpectSameBits(glm::vec3 a, glm::vec3 b)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(a.x), std::bit_cast<uint32_t>(b.x));
	EXPECT_EQ(std::bit_cast<uint32_t>(a.y), std::bit_cast<uint32_t>(b.y));
	EXPECT_EQ(std::bit_cast<uint32_t>(a.z), std::bit_cast<uint32_t>(b.z));
}

Body MakeBody(const glm::mat3& rows, bool rowsScaled)
{
	constexpr std::array<glm::vec3, 3> k_Local = {glm::vec3(1.3f, 0.2f, -0.7f), glm::vec3(-0.9f, 0.6f, 0.4f),
	                                              glm::vec3(0.1f, -1.1f, 0.8f)};
	constexpr std::array<std::array<uint32_t, 3>, 1> k_Triangles = {{{0, 1, 2}}};
	constexpr PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	Body body;
	body.Initialise(1.7f, 1.0f);
	body.SetUpConstants(6.75f, k_Data, true);
	body.BuildShape(k_Local, k_Triangles, glm::vec3(0.21f, -0.13f, 0.37f), 1.5f, 1.0f, rows,
	                glm::vec3(1810.5f, 54.25f, 2640.75f), 1.0f, rowsScaled);
	return body;
}
} // namespace

TEST(LivingAxes, DrawQuarterTurnOfAYawIsTheYawPlusAQuarter)
{
	ecs::Registry registry;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<openblack::test::FakeDynamicsWorld>(registry));
	for (const float a : {0.0f, 0.4f, -1.3f, 2.9f})
	{
		const auto turned = dynamics.DrawQuarterTurn(affine::AngleY(a));
		ExpectNear(turned, affine::AngleY(a + glm::half_pi<float>()), 1e-6f);
		EXPECT_EQ(turned[1], affine::AngleY(a)[1]); // row 1 unchanged
	}
}

TEST(LivingAxes, SetUpPosTakesScaledRowsAsTheyAre)
{
	// the unit rows times the body's scale (the Transform's path) and the same rows already scaled (the world matrix's
	// path): the same body, bit for bit; the scaled rows scaled again would not be
	constexpr float k_Scale = 1.7f;
	const auto unit = affine::AngleY(0.7f);
	glm::mat3 scaled;
	for (int k = 0; k < 3; ++k)
	{
		scaled[k] = unit[k] * k_Scale;
	}
	const auto a = MakeBody(unit, false);
	const auto b = MakeBody(scaled, true);
	ExpectSameBits(a.Centre(), b.Centre());
	for (int k = 0; k < 3; ++k)
	{
		ExpectSameBits(a.Axes()[k], b.Axes()[k]);
	}
	const auto twice = MakeBody(scaled, false);
	EXPECT_GT(glm::length(twice.Centre() - a.Centre()), 1e-3f);
}

class LivingAxesRegistry: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};

TEST_F(LivingAxesRegistry, RotationFromRowsPerClass)
{
	const auto rows = affine::RotationYXZ(0.3f, 0.2f, -0.1f);
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(rows, y, x, z);
	ecs::systems::DynamicsSystem dynamics(std::make_unique<openblack::test::FakeDynamicsWorld>(Reg()));

	// a plain object: a villager keeps only the yaw and stands upright
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager);
	const auto upright = dynamics.RotationFromRows(villager, rows);
	EXPECT_EQ(upright, affine::AngleY(y));
	EXPECT_EQ(upright[1], glm::vec3(0.0f, 1.0f, 0.0f));

	// a mobile object (a pot): RotationYXZ(DecomposeYXZ(rows)), the tilt kept
	const auto pot = Reg().Create();
	Reg().Assign<Pot>(pot);
	ExpectNear(dynamics.RotationFromRows(pot, rows), rows, 1e-6f);
	EXPECT_EQ(dynamics.RotationFromRows(pot, rows), affine::RotationYXZ(y, x, z));

	// a Tree is a plain object too (yaw only: a replanted tree stands straight); a DeadTree is a mobile static: RotationYXZ
	const auto tree = Reg().Create();
	Reg().Assign<Tree>(tree);
	EXPECT_EQ(dynamics.RotationFromRows(tree, rows), affine::AngleY(y));
	const auto deadTree = Reg().Create();
	Reg().Assign<DeadTree>(deadTree);
	EXPECT_EQ(dynamics.RotationFromRows(deadTree, rows), affine::RotationYXZ(y, x, z));

	// every kind that leans takes all three angles: a shark is the original's whale, a mobile object; the singing
	// stone's base is a plain object, and keeps the rows here
	const auto shark = Reg().Create();
	Reg().Assign<Shark>(shark);
	EXPECT_EQ(dynamics.RotationFromRows(shark, rows), affine::RotationYXZ(y, x, z));
	const auto base = Reg().Create();
	Reg().Assign<MobileStatic>(base, MobileStaticInfo::SingingStoneBase);
	EXPECT_EQ(dynamics.RotationFromRows(base, rows), rows);

	// anything else: the rows as they are
	const auto other = Reg().Create();
	EXPECT_EQ(dynamics.RotationFromRows(other, rows), rows);
}
