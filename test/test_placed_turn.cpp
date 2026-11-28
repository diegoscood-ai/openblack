/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

// The angles a placed thing keeps apart from its turn (ECS/PlacedTurn.h), on a registry of the test's own

using namespace openblack;
using ecs::components::Transform;
using ecs::components::TurnAngles;

namespace
{

void ExpectNear(const glm::mat3& a, const glm::mat3& b, float tolerance)
{
	for (glm::length_t c = 0; c < 3; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(a[c][r], b[c][r], tolerance) << "column " << c << " row " << r;
		}
	}
}

entt::entity Rock(ecs::Registry& registry, const glm::mat3& rotation = glm::mat3(1.0f))
{
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(0.0f), rotation, glm::vec3(1.0f));
	registry.Assign<ecs::components::MobileStatic>(rock, MobileStaticInfo::Rock);
	return rock;
}

entt::entity Feature(ecs::Registry& registry)
{
	const auto feature = registry.Create();
	registry.Assign<Transform>(feature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(2.0f, 3.0f, 4.0f));
	registry.Assign<ecs::components::Feature>(feature, FeatureInfo::AztcOlmechead);
	return feature;
}

} // namespace

TEST(PlacedTurn, OnlyMobileStaticsAndMobileObjectsLean)
{
	ecs::Registry registry;
	const auto rock = Rock(registry);
	const auto singingStoneBase = registry.Create();
	registry.Assign<ecs::components::MobileStatic>(singingStoneBase, MobileStaticInfo::SingingStoneBase);
	const auto deadTree = registry.Create();
	registry.Assign<ecs::components::DeadTree>(deadTree, TreeInfo::Pine);
	const auto shark = registry.Create();
	registry.Assign<ecs::components::Shark>(shark);
	const auto feature = Feature(registry);
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, rock));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, deadTree));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, shark));
	// the singing stone's base is a plain object, and so is a feature
	EXPECT_FALSE(ecs::placed_turn::CanLean(registry, singingStoneBase));
	EXPECT_FALSE(ecs::placed_turn::CanLean(registry, feature));
	EXPECT_FALSE(ecs::placed_turn::CanLean(registry, entt::entity {entt::null}));
}

TEST(PlacedTurn, AThingThatDoesNotLeanKeepsOnlyItsTurn)
{
	EXPECT_EQ(ecs::placed_turn::KeptAngles(glm::vec3(0.3f, 4.0f, -0.2f), false), glm::vec3(0.0f, 4.0f, 0.0f));
	EXPECT_EQ(ecs::placed_turn::KeptAngles(glm::vec3(0.3f, 4.0f, -0.2f), true), glm::vec3(0.3f, 4.0f, -0.2f));
}

TEST(PlacedTurn, TheTurnIsTheGamesTurnOfTheAngles)
{
	EXPECT_EQ(ecs::placed_turn::TurnOf(glm::vec3(0.4f, 0.8f, -0.3f), true), affine::RotationYXZ(0.8f, 0.4f, -0.3f));
	EXPECT_EQ(ecs::placed_turn::TurnOf(glm::vec3(0.0f, 0.8f, 0.0f), false), affine::AngleY(0.8f));
	// with no lean the two turns are the same, bit for bit
	for (const float y : {0.0f, 0.7f, -2.9f, 3.1f, 4.0f})
	{
		EXPECT_EQ(ecs::placed_turn::TurnOf(glm::vec3(0.0f, y, 0.0f), true),
		          ecs::placed_turn::TurnOf(glm::vec3(0.0f, y, 0.0f), false));
	}
}

TEST(PlacedTurn, AThingReadsBackTheAnglesItGot)
{
	// a lean past a quarter turn makes the same turn as other angles, but the thing keeps the angles it got
	ecs::Registry registry;
	const auto rock = Rock(registry);
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(2.0f, 4.0f, -0.5f)));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(2.0f, 4.0f, -0.5f));
	const auto& transform = registry.Get<const Transform>(rock);
	EXPECT_EQ(transform.rotation, affine::RotationYXZ(4.0f, 2.0f, -0.5f));
	EXPECT_GT(std::abs(ecs::placed_turn::AnglesOfTurn(transform.rotation, true).x - 2.0f), 0.5f);
}

TEST(PlacedTurn, AnUprightThingKeepsItsWholeTurnAndNoLean)
{
	ecs::Registry registry;
	const auto feature = Feature(registry);
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, feature, glm::vec3(0.4f, 4.0f, 0.1f)));
	// the angle about y is not brought back into a half turn either way, and the leans are gone
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, feature), glm::vec3(0.0f, 4.0f, 0.0f));
	const auto& transform = registry.Get<const Transform>(feature);
	EXPECT_EQ(transform.rotation, affine::AngleY(4.0f));
	// the matrix is built again with the even scale
	EXPECT_EQ(transform.scale, glm::vec3(2.0f));
}

TEST(PlacedTurn, ANewTurnReplacesTheKeptAngles)
{
	const TurnAngles kept {.angles = glm::vec3(2.0f, 4.0f, -0.5f), .turn = affine::RotationYXZ(4.0f, 2.0f, -0.5f)};
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, kept.turn, true), kept.angles);
	// another turn gives the angles the game finds in the turn
	const auto other = affine::RotationYXZ(0.7f, 0.2f, -0.4f);
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(other, y, x, z);
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, other, true), glm::vec3(x, y, z));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, other, false), glm::vec3(0.0f, y, 0.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(nullptr, other, true), glm::vec3(x, y, z));
}

TEST(PlacedTurn, KeptAnglesNeedATurn)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	ecs::placed_turn::KeepAngles(registry, thing, glm::vec3(1.0f));
	EXPECT_FALSE(registry.AllOf<TurnAngles>(thing));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, thing), glm::vec3(0.0f));
	EXPECT_FALSE(ecs::placed_turn::SetAngles(registry, thing, glm::vec3(1.0f)));
}

TEST(PlacedTurn, AMovedThingStandsUprightAndKeepsItsLeans)
{
	// as the original: x 0.5, y 1, z 0.25; moved, it stands upright on y; a lean set again brings the other one back
	ecs::Registry registry;
	const auto rock = Rock(registry);
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(0.5f, 1.0f, 0.25f)));
	ecs::placed_turn::StandUpright(registry, rock);
	EXPECT_EQ(registry.Get<const Transform>(rock).rotation, affine::AngleY(1.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(0.5f, 1.0f, 0.25f));
	auto angles = ecs::placed_turn::AnglesOf(registry, rock);
	angles.x = 0.0f;
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, angles));
	EXPECT_EQ(registry.Get<const Transform>(rock).rotation, affine::RotationYXZ(1.0f, 0.0f, 0.25f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(0.0f, 1.0f, 0.25f));
}

TEST(PlacedTurn, AThingWithNoKeptAnglesStandsUprightOnTheAngleOfItsTurn)
{
	ecs::Registry registry;
	const auto tilted = affine::RotationYXZ(0.6f, 0.3f, -0.2f);
	const auto rock = Rock(registry, tilted);
	ecs::placed_turn::StandUpright(registry, rock);
	EXPECT_EQ(registry.Get<const Transform>(rock).rotation, affine::AngleY(ecs::placed_turn::AnglesOfTurn(tilted, true).y));
	// a turn already about y alone is kept as it is
	const auto upright = Rock(registry, affine::AngleY(2.0f));
	ecs::placed_turn::StandUpright(registry, upright);
	EXPECT_EQ(registry.Get<const Transform>(upright).rotation, affine::AngleY(2.0f));
}

TEST(PlacedTurn, ATurnAboutTheUprightKeepsTheLeans)
{
	const auto angles = ecs::placed_turn::TurnedAboutUpright(glm::vec3(0.3f, 0.5f, -0.2f), 0.4f);
	EXPECT_EQ(angles, glm::vec3(0.3f, 0.5f - 0.4f, -0.2f));
	// the same as a turn of the world about the upright axis
	const auto turn = affine::RotationYXZ(0.5f, 0.3f, -0.2f);
	ExpectNear(affine::RotationYXZ(angles.y, angles.x, angles.z), glm::mat3(glm::eulerAngleY(0.4f)) * turn, 1e-5f);
}

TEST(PlacedTurn, AMovedThingTakesTheAnglesOfItsBody)
{
	// a body that tumbled past a quarter turn about x
	const auto axes = glm::mat3(glm::eulerAngleY(-0.6f) * glm::eulerAngleX(-2.2f) * glm::eulerAngleZ(0.3f));
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(axes, y, x, z);
	const auto leans = ecs::placed_turn::AnglesOfTurn(axes, true);
	EXPECT_EQ(leans, glm::vec3(x, y, z));
	// the turn made again from those angles is the body's, to the rounding of its floats
	ExpectNear(ecs::placed_turn::TurnOf(leans, true), axes, 1e-5f);
	// a thing that does not lean stands upright with the heading the game finds
	const auto upright = ecs::placed_turn::AnglesOfTurn(axes, false);
	EXPECT_EQ(upright, glm::vec3(0.0f, y, 0.0f));
	EXPECT_EQ(ecs::placed_turn::TurnOf(upright, false), affine::AngleY(y));
}

TEST(PlacedTurn, AScriptSetsOneAngleAndKeepsTheOtherTwoAsTheOriginal)
{
	using ecs::placed_turn::AnglesAfterScriptSet;
	using ecs::placed_turn::Axis;
	using script::property_rules::AngleFromScript;
	using script::property_rules::AngleToScript;
	ecs::Registry registry;
	const auto set = [&registry](entt::entity thing, Axis axis, float degrees) {
		const auto angles = AnglesAfterScriptSet(ecs::placed_turn::AnglesOf(registry, thing), axis, AngleFromScript(degrees));
		if (angles.has_value())
		{
			ecs::placed_turn::SetAngles(registry, thing, *angles);
		}
		return angles.has_value();
	};
	// the original's numbers on a rock at x 0.5, y 1, z 0.25: XANGLE 0 leaves y 1 and z 0.25
	const auto rock = Rock(registry);
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(0.5f, 1.0f, 0.25f)));
	EXPECT_TRUE(set(rock, Axis::X, 0.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(0.0f, 1.0f, 0.25f));
	EXPECT_EQ(registry.Get<const Transform>(rock).rotation, affine::RotationYXZ(1.0f, 0.0f, 0.25f));
	// ZANGLE 0 leaves x 0.5
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(0.5f, 1.0f, 0.25f)));
	EXPECT_TRUE(set(rock, Axis::Z, 0.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(0.5f, 1.0f, 0.0f));
	// ANGLE 90 keeps x and z and sets y to the original's 1.5707964
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(0.5f, 1.0f, 0.25f)));
	EXPECT_TRUE(set(rock, Axis::Y, 90.0f));
	const auto turned = ecs::placed_turn::AnglesOf(registry, rock);
	EXPECT_EQ(std::bit_cast<uint32_t>(turned.y), 0x3FC90FDBu);
	EXPECT_EQ(turned.x, 0.5f);
	EXPECT_EQ(turned.z, 0.25f);
	// the same angle again turns nothing; a lean is set again all the same
	EXPECT_FALSE(set(rock, Axis::Y, 90.0f));
	EXPECT_TRUE(set(rock, Axis::X, 0.5f * 57.29578f));
	// read back in degrees as the original's two float products: an angle of 1 is 57.295776
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, rock, glm::vec3(0.5f, 1.0f, 0.25f)));
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(ecs::placed_turn::AnglesOf(registry, rock).y)), 0x42652EE0u);
	// XANGLE 45 on a feature changes none of its angles
	const auto feature = Feature(registry);
	ASSERT_TRUE(ecs::placed_turn::SetAngles(registry, feature, glm::vec3(0.0f, 1.0f, 0.0f)));
	EXPECT_TRUE(set(feature, Axis::X, 45.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, feature), glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(registry.Get<const Transform>(feature).rotation, affine::AngleY(1.0f));
}
