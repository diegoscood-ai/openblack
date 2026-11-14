/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rigid body's own rules, on hand-made bodies: the engine's constants as the original stores them, the shapes of
// trees and living things, the start of the rest counter, the speed limit and the deletion under the sea.

#include <cstdint>

#include <array>
#include <bit>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/ModelInverseSquareRoot.h"
#include "Physics/Body.h"
#include "Physics/BodyShapes.h"

using openblack::physics::Body;
using openblack::physics::PhysicsData;
using openblack::physics::StepResult;
namespace physics = openblack::physics;
namespace shapes = openblack::physics::shapes;

namespace
{
/// A small dynamic body of three points and one face, its centre at `origin`
Body MakeBody(glm::vec3 origin)
{
	constexpr std::array<glm::vec3, 3> k_Local = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.5f),
	                                              glm::vec3(0.0f, 0.0f, -1.0f)};
	constexpr std::array<std::array<uint32_t, 3>, 1> k_Triangles = {{{0, 1, 2}}};
	constexpr PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	Body body;
	body.Initialise(1.0f, 1.0f);
	body.SetUpConstants(5.0f, k_Data, true);
	body.BuildShape(k_Local, k_Triangles, glm::vec3(0.0f), 1.0f, 1.0f, glm::mat3(1.0f), origin);
	return body;
}
} // namespace

TEST(PhysicsBodyRules, TheEngineConstantsAreTheOriginals)
{
	// gravity, the speed limit and the look-ahead as the original's floats; twenty steps of 5 ms a turn of 0.1 s
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_Gravity), 0x411CF5C3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_MaxSpeed), 0x42F80000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_LookAheadSeconds), 0x3D75C28Fu);
	// the spin limit is 3 pi as a float, not 9.42478 rounded
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_MaxSpin), 0x4116CBE4u);
	// the rest limit's growth per count is a fifteen-thousandth as a float, not 6.66667e-05 rounded
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_RestLimitPerCount), 0x388BCF65u);
	// and so is the density a body under the sea gains at each step
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::k_SoakPerStep), 0x388BCF65u);
	EXPECT_EQ(physics::k_StepsPerTurn, 20);
	EXPECT_FLOAT_EQ(physics::k_StepSeconds * static_cast<float>(physics::k_StepsPerTurn), 0.1f);
}

TEST(PhysicsBodyRules, AModelFollowingTheLandHasEachPointRaisedAboutTheCentreOfItsPointsAsTheyWere)
{
	// four points of a flat square; the land raises them by 0, 1, 2 and 4 model units. The centre stays the mean of the
	// points before the raise, each point is raised by its own, then taken about that centre and scaled
	const std::array<glm::vec3, 4> points {glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, -1.0f),
	                                       glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(-1.0f, 0.0f, 1.0f)};
	const std::array<std::array<uint32_t, 3>, 2> triangles {{{0, 1, 2}, {0, 2, 3}}};
	const std::array<float, 4> raises {0.0f, 1.0f, 2.0f, 4.0f};
	constexpr float k_Scale = 2.0f;
	const auto raised = shapes::FromModel(points, triangles, k_Scale, raises);
	EXPECT_EQ(raised.centreOfMass, glm::vec3(0.0f));
	ASSERT_EQ(raised.points.size(), points.size());
	for (size_t i = 0; i < points.size(); ++i)
	{
		EXPECT_EQ(raised.points.at(i), (points.at(i) + glm::vec3(0.0f, raises.at(i), 0.0f)) * k_Scale) << i;
	}
	EXPECT_FLOAT_EQ(raised.radius, glm::length(glm::vec3(-1.0f, 4.0f, 1.0f)) * k_Scale);
	EXPECT_EQ(raised.faces.size(), triangles.size());
	// with no raises it is the model's own points
	const auto flat = shapes::FromModel(points, triangles, k_Scale);
	for (size_t i = 0; i < points.size(); ++i)
	{
		EXPECT_EQ(flat.points.at(i), points.at(i) * k_Scale) << i;
	}
}

TEST(PhysicsBodyRules, ATreeIsASixteenPointSpindleWithItsCentreLowerWhileRooted)
{
	const auto rooted = shapes::Tree(10.0f, 1.0f, 2.0f, true);
	EXPECT_EQ(rooted.points.size(), 16u);
	EXPECT_EQ(rooted.faces.size(), 24u);
	// the centre of mass in the model's own units: 0.4 of the height rooted, 0.5 dead
	EXPECT_FLOAT_EQ(rooted.centreOfMass.y, 0.4f * 10.0f / 2.0f);
	EXPECT_FLOAT_EQ(rooted.dragFactor, 0.3f);
	const auto dead = shapes::Tree(10.0f, 1.0f, 2.0f, false);
	EXPECT_FLOAT_EQ(dead.centreOfMass.y, 0.5f * 10.0f / 2.0f);
}

TEST(PhysicsBodyRules, ALivingBodyIsTwelvePointsCentredAtHalfHeightWithTwiceTheDrag)
{
	for (const auto kind : {shapes::Living::Villager, shapes::Living::Animal})
	{
		const auto body = shapes::LivingBody(kind, 2.0f, 0.5f, 1.0f);
		EXPECT_EQ(body.points.size(), 12u);
		EXPECT_EQ(body.faces.size(), 20u);
		EXPECT_FLOAT_EQ(body.centreOfMass.y, 1.0f);
		EXPECT_FLOAT_EQ(body.dragFactor, 2.0f);
	}
}

TEST(PhysicsBodyRules, TheRestCounterStartsAtMinusTheScaledHalfHeightInThousandths)
{
	Body body;
	body.Initialise(1.7f, 1.0f);
	EXPECT_EQ(body.RestCounter(), -1700);
}

TEST(PhysicsBodyRules, NoBodyGoesFasterThanTheLimit)
{
	auto body = MakeBody(glm::vec3(100.0f, 50.0f, 100.0f));
	body.velocity = glm::vec3(300.0f, 0.0f, 400.0f);
	body.ClearForces();
	EXPECT_EQ(body.Integrate(), StepResult::Moved);
	EXPECT_FLOAT_EQ(glm::length(body.velocity), physics::k_MaxSpeed);
	EXPECT_FLOAT_EQ(body.velocity.x / body.velocity.z, 0.75f);
}

TEST(PhysicsBodyRules, ABodyMoreThanFourRadiiUnderTheSeaIsDeleted)
{
	auto above = MakeBody(glm::vec3(100.0f, 0.0f, 100.0f));
	const float radius = above.Radius();
	ASSERT_GT(radius, 0.0f);
	above.ClearForces();
	EXPECT_NE(above.Integrate(), StepResult::Delete);
	auto atTheLimit = MakeBody(glm::vec3(100.0f, -4.0f * radius, 100.0f));
	atTheLimit.ClearForces();
	EXPECT_NE(atTheLimit.Integrate(), StepResult::Delete);
	auto under = MakeBody(glm::vec3(100.0f, -4.0f * radius - 0.01f, 100.0f));
	under.ClearForces();
	EXPECT_EQ(under.Integrate(), StepResult::Delete);
}

TEST(PhysicsBodyRules, TwoBodiesTouchWithTheSofterSpringAndDampingAndAThirdOfTheSmallerFriction)
{
	const physics::ContactMaterial rock {.stiffness = 300.0f, .damping = 30.0f, .friction = 0.5f};
	const physics::ContactMaterial villager {.stiffness = 100.0f, .damping = 40.0f, .friction = 0.8f};
	const auto mixed = physics::MixContactMaterials(rock, villager);
	EXPECT_EQ(mixed.stiffness, 100.0f);
	EXPECT_EQ(mixed.damping, 30.0f);
	EXPECT_EQ(mixed.friction, 0.5f * 0.3f);
	EXPECT_EQ(physics::MixContactMaterials(villager, rock).friction, 0.5f * 0.3f);
}

TEST(PhysicsBodyRules, AContactPushesByItsSpringFirstThenWithItsDampingAndNeverPulls)
{
	// the step it goes in: depth x spring
	EXPECT_FLOAT_EQ(physics::ContactPush(300.0f, 30.0f, 0.1f, 0.0f, 1.0f, 1.0f), 30.0f);
	// then the spring on the first depth plus the damping on how much deeper, 200 a second, by ahead over length
	EXPECT_FLOAT_EQ(physics::ContactPush(300.0f, 30.0f, 0.15f, 0.1f, 2.0f, 1.0f),
	                300.0f * 0.1f + 30.0f * 0.05f * 200.0f * 2.0f);
	// coming back out faster than the spring holds: no pull
	EXPECT_EQ(physics::ContactPush(300.0f, 30.0f, 0.01f, 0.1f, 1.0f, 1.0f), 0.0f);
	// the damping term multiplied as the original does, (deeper x 200 x ahead / length) x damping, then the spring's
	// added: these give 1672.5107 (0x44D11058), where damping first gives one unit less
	const auto bits = [](uint32_t value) { return std::bit_cast<float>(value); };
	EXPECT_EQ(std::bit_cast<uint32_t>(physics::ContactPush(bits(0x4297AD4Du), bits(0x424BFE8Fu), bits(0x3E6AA1AEu),
	                                                       bits(0x3D9CB6E4u), bits(0x3FDE8A0Bu), bits(0x3FCFD64Fu))),
	          0x44D11058u);
	// a push that is not a number is none
	EXPECT_EQ(physics::ContactPush(300.0f, 30.0f, 0.15f, 0.1f, 1.0f, std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

// A line meets a creature's skeleton on the nearest of its parts' ellipsoids, there with the part's normal, scaled by
// the 3D engine's own inverse square root
TEST(PhysicsShapes, TheCreatureIsHitOnItsSkeleton)
{
	const physics::Ellipsoid part {.boxMin = glm::vec3(-1.0f), .boxMax = glm::vec3(1.0f)};
	const std::array bones {part};
	const auto hit = physics::EllipsoidHit(bones, {5.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f});
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->second, 4.0f, 1e-4f);
	EXPECT_EQ(hit->first.point, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(hit->first.normal.x),
	          std::bit_cast<uint32_t>(openblack::game_math::ModelInverseSquareRoot(1.0f)));
	EXPECT_FALSE(physics::EllipsoidHit(bones, {5.0f, 3.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}).has_value());
}

TEST(PhysicsShapes, EachBoneBoxHoldsItsVerticesAndItsOrigin)
{
	const std::array<glm::vec3, 3> positions {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(-1.0f, 0.5f, 0.5f),
	                                          glm::vec3(5.0f, 5.0f, 5.0f)};
	const std::array<uint16_t, 3> bones {0, 0, shapes::k_NoBone};
	const auto boxes = shapes::BoneBoxes(positions, bones, 2);
	ASSERT_EQ(boxes.size(), 2u);
	EXPECT_EQ(boxes[0].min, glm::vec3(-1.0f, 0.0f, 0.0f));
	EXPECT_EQ(boxes[0].max, glm::vec3(1.0f, 2.0f, 3.0f));
	// A bone with no vertices keeps a box at its origin
	EXPECT_EQ(boxes[1].min, glm::vec3(0.0f));
	EXPECT_EQ(boxes[1].max, glm::vec3(0.0f));
}
