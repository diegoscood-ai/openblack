/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <memory>

#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "3D/AxisAlignedBoundingBox.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameAngle.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyPlacement.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/LastDrawnMatrix.h"
#include "ECS/Components/ObjectMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreaturePose.h"
#include "ECS/DrawList/ObjectDraw.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "support/RestoreService.h"

// An object's own placement (ECS/ObjectPlacement.h): the game's two constructors, cell by cell against the original's
// bits (the exe oracle's runs, 2026-10-28; docs/wiki/engine/matrices-and-zoomer.md#the-objects-own-placement)

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The point every case was placed at
constexpr std::array<uint32_t, 3> k_Point {0x44DB6300, 0x41FA0000, 0x45284840};

/// An upright placement: the y angle, the scale and the original's 9 cells of its rows (its translation is the point)
struct YawCase
{
	uint32_t angle;
	uint32_t scale;
	std::array<uint32_t, 9> cells;
};

/// The first three are the branches without a turn or at scale 1; the rest are turns at a scale where rounding the
/// cosine and the sine to floats before the scale's product gives another last bit
constexpr std::array<YawCase, 15> k_YawCases {{
    {0x00000000,
     0x3FD9999A,
     {0x3FD9999A, 0x00000000, 0x00000000, 0x00000000, 0x3FD9999A, 0x00000000, 0x00000000, 0x00000000, 0x3FD9999A}},
    {0x3F400000,
     0x3F800000,
     {0x3F3B4FF6, 0x00000000, 0x3F2E7FE1, 0x00000000, 0x3F800000, 0x00000000, 0xBF2E7FE1, 0x00000000, 0x3F3B4FF6}},
    {0x00000000,
     0x3F800000,
     {0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000}},
    {0xC038030E,
     0x3F357FEB,
     {0xBF2F18D2, 0x80000000, 0xBE3F2181, 0x00000000, 0x3F357FEB, 0x00000000, 0x3E3F2181, 0x00000000, 0xBF2F18D2}},
    {0xC033BD23,
     0x3F7093B2,
     {0xBF6358F0, 0x80000000, 0xBE9D5BC0, 0x00000000, 0x3F7093B2, 0x00000000, 0x3E9D5BC0, 0x00000000, 0xBF6358F0}},
    {0x40320C9A,
     0x3FA0A7A7,
     {0xBF96616A, 0x00000000, 0x3EE21F2E, 0x00000000, 0x3FA0A7A7, 0x00000000, 0xBEE21F2E, 0x80000000, 0xBF96616A}},
    {0xC01B402D,
     0x3F2392B2,
     {0xBEF6DA9E, 0x80000000, 0xBED6AE2D, 0x00000000, 0x3F2392B2, 0x00000000, 0x3ED6AE2D, 0x00000000, 0xBEF6DA9E}},
    {0x3FFF596F,
     0x3FC47C2E,
     {0xBF21B6FE, 0x00000000, 0x3FB313A3, 0x00000000, 0x3FC47C2E, 0x00000000, 0xBFB313A3, 0x80000000, 0xBF21B6FE}},
    {0xBED250BC,
     0x3F487DDE,
     {0x3F37D036, 0x00000000, 0xBEA01EA0, 0x00000000, 0x3F487DDE, 0x00000000, 0x3EA01EA0, 0x00000000, 0x3F37D036}},
    {0xBEF2C95C,
     0x3F09690A,
     {0x3EF47F55, 0x00000000, 0xBE7AFA30, 0x00000000, 0x3F09690A, 0x00000000, 0x3E7AFA30, 0x00000000, 0x3EF47F55}},
    {0xBFF519E7,
     0x3F87387E,
     {0xBEB671C9, 0x80000000, 0xBF7E9785, 0x00000000, 0x3F87387E, 0x00000000, 0x3F7E9785, 0x00000000, 0xBEB671C9}},
    {0x3F965148,
     0x3FF4B281,
     {0x3F3CF943, 0x00000000, 0x3FE1B806, 0x00000000, 0x3FF4B281, 0x00000000, 0xBFE1B806, 0x00000000, 0x3F3CF943}},
    {0xBED274F2,
     0x3F0A8DEF,
     {0x3EFE06BF, 0x00000000, 0xBE5D72FC, 0x00000000, 0x3F0A8DEF, 0x00000000, 0x3E5D72FC, 0x00000000, 0x3EFE06BF}},
    {0xC0377AF1,
     0x3F3ED137,
     {0xBF37A982, 0x80000000, 0xBE4F0D9D, 0x00000000, 0x3F3ED137, 0x00000000, 0x3E4F0D9D, 0x00000000, 0xBF37A982}},
    {0xBFBAAA18,
     0x3FBB4EB2,
     {0x3E283136, 0x00000000, 0xBFBA1FAF, 0x00000000, 0x3FBB4EB2, 0x00000000, 0x3FBA1FAF, 0x00000000, 0x3E283136}},
}};

/// A leaning placement: x, y, z angles, the scale and the original's 9 cells of its rows
struct YxzCase
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t scale;
	std::array<uint32_t, 9> cells;
};

constexpr std::array<YxzCase, 6> k_YxzCases {{
    {0x3FF09017,
     0xBF8C03BC,
     0xBFD0B51F,
     0x3F800000,
     {0xBF5F512F, 0xBE9B3AF9, 0xBEC46174, 0xBED0BF34, 0x3C9487A4, 0x3F69B5E9, 0xBE8A2778, 0x3F73E80B, 0xBE0EC603}},
    {0x4001B519,
     0xC01ACAE4,
     0xBE18E464,
     0x3FC380CF,
     {0xBFA23E3A, 0xBDCCDB09, 0xBF58A76D, 0xBF3A1305, 0xBF2A3A1E, 0x3F95672C, 0xBEE3C7E8, 0x3FAF897B, 0x3F0114C2}},
    {0x3F331428,
     0xC00E907B,
     0x3F52269F,
     0x3FB35FE1,
     {0xBD76D605, 0xBF48DD84, 0xBF946BF5, 0xBF8E7B38, 0x3F3B15F9, 0xBEDF924C, 0x3F596448, 0x3F66FB85, 0xBF2798E1}},
    {0x400D317A,
     0xC03DA946,
     0x4037D763,
     0x3F2E0DA4,
     {0x3F2BBBF9, 0x3DDBAC4C, 0xBCDD6F5C, 0xBDACDC2D, 0x3EC7273D, 0xBF0D1C46, 0xBD926CC1, 0x3F0C16E2, 0x3ECB514B}},
    {0x3FFC03E8,
     0x3D3C4174,
     0x4006FD8D,
     0x3F33CB6E,
     {0xBEC545E8, 0x3E6F572F, 0x3F09E6AD, 0x3F1649E9, 0x3E0EF50E, 0x3EB7FAC5, 0x3C4CF03F, 0x3F25BC88, 0xBE8B3EB4}},
    {0xBF8F928E,
     0xC02600FB,
     0x3FE028C5,
     0x3FBCC3F5,
     {0xBEE8CC5B, 0xBF2141FE, 0x3FA07330, 0xBFAE6C34, 0xBDEB5957, 0xBF0D51AD, 0x3EAABCA7, 0xBFAA0B4E, 0xBF0BEF51}},
}};

float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

glm::vec3 Point()
{
	return {F(k_Point[0]), F(k_Point[1]), F(k_Point[2])};
}

/// Cell i of the original (row i / 3, element i % 3) is glm's column i / 3, row i % 3; the translation is the point
/// as it is. The zero cells are compared by value: their sign is not kept (the original's -0 where the turn's products
/// are both -0)
void ExpectCells(const glm::mat4& m, const std::array<uint32_t, 9>& cells, const glm::vec3& point = Point())
{
	for (int i = 0; i < 9; ++i)
	{
		const float got = m[i / 3][i % 3];
		const auto want = cells.at(static_cast<size_t>(i));
		if (F(want) == 0.0f)
		{
			EXPECT_EQ(got, 0.0f) << "cell " << i;
		}
		else
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(got), want) << "cell " << i;
		}
	}
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(m[3][i]), std::bit_cast<uint32_t>(point[i])) << "translation " << i;
		EXPECT_EQ(m[i][3], 0.0f);
	}
	EXPECT_EQ(m[3][3], 1.0f);
}

} // namespace

TEST(ObjectPlacement, AnUprightPlacementHasTheOriginalsCells)
{
	for (const auto& c : k_YawCases)
	{
		ExpectCells(affine::PlacementMatrix(Point(), F(c.angle), F(c.scale)), c.cells);
	}
}

// Why the placement is kept apart: the model rebuilt from the Transform's rotation rounds the cosine and the sine to
// floats before the scale's product, and gives another last bit in these cases
TEST(ObjectPlacement, TheTransformsModelRoundsTheTurnBeforeTheScale)
{
	for (size_t i = 3; i < k_YawCases.size(); ++i)
	{
		const auto& c = k_YawCases.at(i);
		const auto model = affine::Model(Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
		const auto placed = affine::PlacementMatrix(Point(), F(c.angle), F(c.scale));
		EXPECT_NE(model, placed) << "case " << i;
	}
}

TEST(ObjectPlacement, ALeaningPlacementHasTheOriginalsCells)
{
	for (const auto& c : k_YxzCases)
	{
		ExpectCells(affine::PlacementYXZ(Point(), F(c.y), F(c.x), F(c.z), F(c.scale)), c.cells);
	}
}

TEST(ObjectPlacement, WithoutAPlacementTheCellsAreTheTransformsModel)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(3);
	const auto& transform = registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	EXPECT_EQ(object_placement::Cells(registry, entity), affine::Model(transform));
}

TEST(ObjectPlacement, APlacementKeepsItsCellsItsAngleAndItsScale)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(4);
	registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), F(c.scale));
	ExpectCells(object_placement::Cells(registry, entity), c.cells);
	const auto& placement = registry.Get<const ObjectMatrix>(entity);
	EXPECT_EQ(std::bit_cast<uint32_t>(placement.yAngle), c.angle);
	EXPECT_EQ(std::bit_cast<uint32_t>(placement.scale), c.scale);

	const auto& lean = k_YxzCases.at(1);
	object_placement::PlaceYXZ(registry, entity, Point(), glm::vec3(F(lean.x), F(lean.y), F(lean.z)), F(lean.scale));
	ExpectCells(object_placement::Cells(registry, entity), lean.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(registry.Get<const ObjectMatrix>(entity).yAngle), lean.y);
}

// The game's setters place the model again only for an object that has a placement
TEST(ObjectPlacement, AnObjectWithoutAPlacementGetsNoneFromItsSetters)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, Point(), glm::mat3(1.0f), glm::vec3(1.5f));
	ASSERT_TRUE(placed_turn::SetAngles(registry, entity, glm::vec3(0.0f, 0.5f, 0.0f)));
	object_placement::PlaceAgain(registry, entity);
	EXPECT_FALSE(registry.AllOf<ObjectMatrix>(entity));
}

TEST(ObjectPlacement, TurningAnUprightObjectPlacesItAgainWithTheNewAngle)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& before = k_YawCases.at(3);
	const auto& after = k_YawCases.at(4);
	registry.Assign<Transform>(entity, Point(), affine::AngleY(F(before.angle)), glm::vec3(F(after.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(before.angle), F(after.scale));
	ASSERT_TRUE(placed_turn::SetAngles(registry, entity, glm::vec3(0.0f, F(after.angle), 0.0f)));
	ExpectCells(object_placement::Cells(registry, entity), after.cells);
}

TEST(ObjectPlacement, ScalingAnUprightObjectKeepsThePlacementsAngle)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(5);
	auto& transform = registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(1.0f));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), 1.0f);
	transform.scale = glm::vec3(F(c.scale));
	object_placement::PlaceAgain(registry, entity);
	ExpectCells(object_placement::Cells(registry, entity), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(registry.Get<const ObjectMatrix>(entity).scale), c.scale);
}

// A move that makes no placement of its own (the physics landing, a walk, a drop) is read as placed again where the
// object is now, with its class's constructor
TEST(ObjectPlacement, AMovedObjectIsReadAsPlacedAgainWhereItIsNow)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(6);
	auto& transform = registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), F(c.scale));
	transform.position += glm::vec3(10.0f, 0.5f, -3.0f);
	ExpectCells(object_placement::Cells(registry, entity), c.cells, transform.position);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, entity).scale), c.scale);
}

// A turn no constructor makes (the physics' or the hand's) is drawn and read as the Transform's model
TEST(ObjectPlacement, ATurnNoConstructorMakesIsTheTransformsModel)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(7);
	auto& transform = registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), F(c.scale));
	transform.rotation = affine::RotationYXZ(0.3f, 0.2f, -0.1f);
	EXPECT_EQ(object_placement::Cells(registry, entity), affine::Model(transform));
}

// A move the game makes without its model (a script's move of a thing that is not alive) leaves the model where it
// was: drawn, picked and tested on the screen at the old place, until a later move places it again
TEST(ObjectPlacement, AMoveWithoutTheModelLeavesThePlacementBehind)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(8);
	auto& transform = registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), F(c.scale));

	object_placement::MoveWithoutModel(registry, entity, [&transform] { transform.position = glm::vec3(100.0f, 2.0f, 50.0f); });
	EXPECT_EQ(transform.position, glm::vec3(100.0f, 2.0f, 50.0f));
	ExpectCells(object_placement::Cells(registry, entity), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, entity).scale), c.scale);

	// moved again by a move that places its model: read as placed where it is now
	transform.position = glm::vec3(120.0f, 3.0f, 60.0f);
	ExpectCells(object_placement::Cells(registry, entity), c.cells, transform.position);
}

TEST(ObjectPlacement, AMoveWithoutTheModelOfAnObjectWithoutAPlacementMakesNone)
{
	Registry registry;
	const auto entity = registry.Create();
	auto& transform = registry.Assign<Transform>(entity, Point(), glm::mat3(1.0f), glm::vec3(1.0f));
	object_placement::MoveWithoutModel(registry, entity, [&transform] { transform.position = glm::vec3(100.0f, 2.0f, 50.0f); });
	EXPECT_FALSE(registry.AllOf<ObjectMatrix>(entity));
	EXPECT_EQ(object_placement::Cells(registry, entity), affine::Model(transform));
}

namespace
{

/// The y angle (bits) of a villager's draw whose quarter turn on is the angle of k_YawCases[5], exactly
constexpr uint32_t k_DrawnYawOfCase5 = 0x3F9B0959;

/// A creature's body yaw (bits) whose quarter turn on is the angle of k_YawCases[3], exactly
constexpr uint32_t k_BodyYawOfCase3 = 0xC08E457E;

/// A villager drawn between its turn's two places: its Transform where its turn put it, its drawn pose a little
/// behind, at its drawn yaw
entt::entity VillagerBetweenTurns(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(100.0f, 5.0f, 200.0f), affine::AngleY(0.5f), glm::vec3(1.25f));
	registry.Assign<Villager>(entity);
	auto& draw = registry.Assign<DrawPosition>(entity);
	draw.started = true;
	draw.turnStart = glm::vec3(99.0f, 5.0f, 198.0f);
	draw.position = glm::vec3(99.5f, 5.0f, 199.0f);
	draw.rotation = affine::AngleY(0.4f + glm::half_pi<float>());
	draw.yaw = 0.4f;
	draw.objectYaw = 0.4f;
	draw.hasObjectYaw = true;
	return entity;
}

/// How the draw places the fixture: the yaw constructor at the drawn point, a quarter turn on from the drawn yaw
glm::mat4 PlacedAsDrawn(const Registry& registry, entt::entity entity)
{
	const auto& draw = registry.Get<const DrawPosition>(entity);
	return affine::PlacementMatrix(draw.position, draw.objectYaw + glm::half_pi<float>(),
	                               registry.Get<const Transform>(entity).scale.x);
}

/// The draws read the land from the locator: without one, nothing is sheared
bool NoLand()
{
	return !Locator::terrainSystem::has_value();
}

} // namespace

// The game reads a villager's or an animal's matrix as its last draw left it, between its turn's two places, and not
// where its turn has put it since, until it is drawn again
TEST(ObjectPlacement, ALivingThingIsReadAsItsLastDrawLeftItNotWhereItsTurnPutIt)
{
	if (!NoLand())
	{
		GTEST_SKIP() << "a land is in the locator";
	}
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	auto& transform = registry.Get<Transform>(entity);
	auto& draw = registry.Get<DrawPosition>(entity);
	EXPECT_EQ(object_placement::Cells(registry, entity), affine::Model(transform)); // never placed: its Transform

	object_placement::KeepDrawn(registry, entity);
	const auto drawn = PlacedAsDrawn(registry, entity);
	EXPECT_EQ(object_placement::Cells(registry, entity), drawn);
	EXPECT_EQ(object_placement::Of(registry, entity).scale, 1.25f);

	// the next turn moves it, and the frames not drawn move its pose on: still read as the last draw left it
	transform.position = glm::vec3(101.0f, 5.5f, 201.0f);
	draw.position = glm::vec3(100.2f, 5.2f, 200.4f);
	EXPECT_EQ(object_placement::Cells(registry, entity), drawn);

	// drawn again: read as this draw leaves it
	object_placement::KeepDrawn(registry, entity);
	EXPECT_EQ(object_placement::Cells(registry, entity), PlacedAsDrawn(registry, entity));
	EXPECT_NE(object_placement::Cells(registry, entity), drawn);
}

// The draw places a villager with the original's yaw constructor, bit for bit (the oracle's cells of k_YawCases[5]),
// at its drawn yaw a quarter turn on, not with the Transform's model of its turn
TEST(ObjectPlacement, ADrawnVillagerIsPlacedByTheOriginalsYawConstructorAtItsDrawnYaw)
{
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	const auto& c = k_YawCases.at(5);
	registry.Get<Transform>(entity).scale = glm::vec3(F(c.scale));
	auto& draw = registry.Get<DrawPosition>(entity);
	draw.position = Point();
	draw.objectYaw = F(k_DrawnYawOfCase5);
	ASSERT_EQ(std::bit_cast<uint32_t>(draw.objectYaw + glm::half_pi<float>()), c.angle);
	const auto placed = DrawnLivingPlacement(registry, entity, nullptr);
	ASSERT_TRUE(placed.has_value());
	ExpectCells(placed->cells, c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(placed->yAngle), c.angle);
	EXPECT_EQ(std::bit_cast<uint32_t>(placed->scale), c.scale);
}

// An animal is placed at its own y angle a quarter turn on (its game angle's, as its turn faces it)
TEST(ObjectPlacement, AnAnimalIsPlacedAtItsOwnYAngleAQuarterTurnOn)
{
	Registry registry;
	const auto entity = registry.Create();
	const float yAngle = game_math::ConvertGameAngleTo3D(300);
	registry.Assign<Transform>(entity, Point(), openblack::physics::living::DrawnRotation(yAngle), glm::vec3(1.5f));
	registry.Assign<Animal>(entity, AnimalInfo::Cow, 12u);
	registry.Assign<AnimalBrain>(entity).angle = 300;
	const auto placed = DrawnLivingPlacement(registry, entity, nullptr);
	ASSERT_TRUE(placed.has_value());
	EXPECT_EQ(placed->cells, affine::PlacementMatrix(Point(), yAngle + glm::half_pi<float>(), 1.5f));
}

// The draw's slope shear adds row 1 times each rise to rows 0 and 2, and a rise is kept within 0.3 as the original's
// min then max, a NaN coming out 0.3
TEST(ObjectPlacement, TheDrawsSlopeShearAddsRowOneToRowsZeroAndTwoWithinThreeTenths)
{
	const auto placed = affine::PlacementMatrix(Point(), 0.7f, 1.2f);
	const auto sheared = ShearOnTheSlope(placed, 0.1f, -0.2f);
	EXPECT_EQ(sheared[0], placed[0] + 0.1f * placed[1]);
	EXPECT_EQ(sheared[1], placed[1]);
	EXPECT_EQ(sheared[2], placed[2] + -0.2f * placed[1]);
	EXPECT_EQ(sheared[3], placed[3]);
	EXPECT_EQ(ClampShear(0.1f), 0.1f);
	EXPECT_EQ(ClampShear(0.5f), 0.3f);
	EXPECT_EQ(ClampShear(-0.5f), -0.3f);
	EXPECT_EQ(ClampShear(std::numeric_limits<float>::quiet_NaN()), 0.3f);
}

// The debug switch's smooth pose is only drawn: the game reads the original's pose
TEST(ObjectPlacement, TheGameNeverReadsTheSmoothPoseOnlyDrawn)
{
	if (!NoLand())
	{
		GTEST_SKIP() << "a land is in the locator";
	}
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	auto& draw = registry.Get<DrawPosition>(entity);
	draw.drawPosition = glm::vec3(99.8f, 5.0f, 199.6f);
	draw.drawRotation = affine::AngleY(0.45f);
	draw.hasDrawPose = true;
	object_placement::KeepDrawn(registry, entity);
	EXPECT_EQ(object_placement::Cells(registry, entity), PlacedAsDrawn(registry, entity));
	EXPECT_NE(glm::vec3(object_placement::Cells(registry, entity)[3]), draw.drawPosition);
}

// A creature is read at its body model where the last frame placed it (creature_pose::PlaceBody): the yaw constructor
// at its place, its body's yaw a quarter turn on and the size it is drawn at (smaller in its pen; the oracle's cells),
// however its turn moves it before the next frame
TEST(ObjectPlacement, ACreatureIsReadAtItsBodyModelWhereTheLastFramePlacedIt)
{
	Registry registry;
	const auto creature = registry.Create();
	const auto& c = k_YawCases.at(3);
	auto& transform = registry.Assign<Transform>(creature, Point(), affine::AngleY(1.0f), glm::vec3(2.0f));
	registry.Assign<Creature>(creature);
	auto& locomotion = registry.Assign<CreatureLocomotion>(creature);
	locomotion.started = true;
	locomotion.heading = -F(k_BodyYawOfCase3); // the body's yaw is minus the walk's heading
	auto& pose = registry.Assign<CreatureDrawPose>(creature);
	pose.scale = glm::vec3(F(c.scale));
	pose.inPen = true;
	creature_pose::PlaceBody(registry, creature);
	ExpectCells(object_placement::Cells(registry, creature), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, creature).scale), c.scale);

	transform.position = glm::vec3(52.0f, 2.0f, 61.0f);
	ExpectCells(object_placement::Cells(registry, creature), c.cells);
}

// While the hand holds it, a thing is read at the matrix the hand draws it with, and its own draw leaves nothing; the
// hand's last matrix stays when it lets go, until its next draw
TEST(ObjectPlacement, AThingInTheHandIsReadAsTheHandDrawsIt)
{
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	object_placement::KeepDrawn(registry, entity);
	const auto drawn = object_placement::Cells(registry, entity);
	auto& pose = registry.Assign<HandDrawPose>(entity);
	pose.position = glm::vec3(98.0f, 12.0f, 197.0f);
	pose.rotation = affine::AngleY(1.2f);
	const auto held = DrawnModel(registry, entity, false);
	EXPECT_EQ(object_placement::Cells(registry, entity), held);
	EXPECT_NE(held, drawn);
	EXPECT_FALSE(DrawnLivingPlacement(registry, entity, nullptr).has_value());
	object_placement::KeepHeld(registry, entity);
	registry.Remove<HandDrawPose>(entity);
	EXPECT_EQ(object_placement::Cells(registry, entity), held);
}

// A thing in the physics' flight that keeps no matrix of its own from it (a rock, a poo) is read this frame at the
// physics' pose, with its own scale, as the original's frame pose writes its 3D object's matrix; at rest, in the hand
// or a creature's body, as the game reads it otherwise
TEST(ObjectPlacement, AThingInFlightIsReadThisFrameAtThePhysicsPoseWithItsOwnScale)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(4);
	registry.Assign<Transform>(entity, Point(), affine::AngleY(F(c.angle)), glm::vec3(F(c.scale)));
	object_placement::PlaceYaw(registry, entity, Point(), F(c.angle), F(c.scale));
	ExpectCells(object_placement::DrawnOf(registry, entity).cells, c.cells);

	const glm::vec3 flying {612.0f, 40.0f, 2700.0f};
	registry.Assign<PhysicsDrawPose>(entity, PhysicsDrawPose {
	                                             .position = flying,
	                                             .rotation = affine::RotationYXZ(0.3f, 0.2f, -0.1f),
	                                             .scale = 2.5f,
	                                         });
	const auto drawn = object_placement::DrawnOf(registry, entity);
	EXPECT_EQ(drawn.cells, DrawnModel(registry, entity, false));
	EXPECT_EQ(glm::vec3(drawn.cells[3]), flying);
	EXPECT_EQ(std::bit_cast<uint32_t>(drawn.scale), c.scale);
	// the other readers still take its own placement
	ExpectCells(object_placement::Cells(registry, entity), c.cells);

	// held: the matrix the hand draws it with
	auto& hand = registry.Assign<HandDrawPose>(entity);
	hand.position = glm::vec3(98.0f, 12.0f, 197.0f);
	hand.rotation = affine::AngleY(1.2f);
	EXPECT_EQ(object_placement::DrawnOf(registry, entity).cells, object_placement::Cells(registry, entity));
	EXPECT_EQ(glm::vec3(object_placement::DrawnOf(registry, entity).cells[3]), hand.position);
	registry.Remove<HandDrawPose>(entity);

	// a creature: its body model where the last frame placed it
	glm::mat4 body(1.0f);
	body[3] = glm::vec4(5.0f, 6.0f, 7.0f, 1.0f);
	registry.Assign<CreatureBodyPlacement>(entity, CreatureBodyPlacement {.cells = body, .scale = 3.0f});
	EXPECT_EQ(object_placement::DrawnOf(registry, entity).cells, body);
	EXPECT_EQ(object_placement::DrawnOf(registry, entity).scale, 3.0f);
}

// A setter's placing of a villager or an animal (a scale, a script's placing, its angles) places its model where it is
// now, upright at its own y angle and its scale, as the original's setters do (the oracle's cells); a creature's body
// keeps what its frame left
TEST(ObjectPlacement, ASetterPlacesALivingThingWhereItIsAtItsOwnYAngle)
{
	Registry registry;
	const auto living = registry.Create();
	const auto& c = k_YawCases.at(7);
	auto& transform =
	    registry.Assign<Transform>(living, Point(), openblack::physics::living::DrawnRotation(F(c.angle)), glm::vec3(1.0f));
	registry.Assign<Villager>(living);
	registry.Assign<WallHug>(living, glm::vec2(0.0f), glm::vec2(0.0f), F(c.angle), 0.1f);
	object_placement::PlaceLiving(registry, living, glm::vec3(1.0f), 0.0f, 1.0f);
	transform.scale = glm::vec3(F(c.scale));
	object_placement::PlaceAgain(registry, living);
	ExpectCells(object_placement::Cells(registry, living), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, living).scale), c.scale);

	const auto creature = registry.Create();
	registry.Assign<Transform>(creature, glm::vec3(50.0f, 2.0f, 60.0f), affine::AngleY(1.0f), glm::vec3(2.0f));
	registry.Assign<Creature>(creature);
	creature_pose::PlaceBody(registry, creature);
	const auto kept = object_placement::Cells(registry, creature);
	object_placement::PlaceAgain(registry, creature);
	EXPECT_EQ(object_placement::Cells(registry, creature), kept);
}

// An angle setter places a villager's or an animal's model again only for a new angle: where it is now, at that angle
// with no quarter turn on, and its scale (the oracle's cells); the angle it has leaves the last draw's matrix
TEST(ObjectPlacement, AnAngleSetterPlacesTheModelOnlyForANewAngle)
{
	if (!NoLand())
	{
		GTEST_SKIP() << "a land is in the locator";
	}
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	object_placement::KeepDrawn(registry, entity);
	const auto drawn = object_placement::Cells(registry, entity);
	object_placement::PlaceLivingTurned(registry, entity, 0.4f, 0.4f);
	EXPECT_EQ(object_placement::Cells(registry, entity), drawn);

	const auto& c = k_YawCases.at(8);
	auto& transform = registry.Get<Transform>(entity);
	transform.position = Point();
	transform.scale = glm::vec3(F(c.scale));
	object_placement::PlaceLivingTurned(registry, entity, 0.4f, F(c.angle));
	ExpectCells(object_placement::Cells(registry, entity), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(registry.Get<const LastDrawnMatrix>(entity).yAngle), c.angle);
}

// The drawing never reads what a draw left for the game: a creature standing still is drawn at its Transform
TEST(ObjectPlacement, TheDrawingNeverReadsTheMatrixLeftForTheGame)
{
	Registry registry;
	const auto creature = registry.Create();
	auto& transform =
	    registry.Assign<Transform>(creature, glm::vec3(50.0f, 2.0f, 60.0f), affine::AngleY(1.0f), glm::vec3(2.0f));
	registry.Assign<Creature>(creature);
	creature_pose::PlaceBody(registry, creature);
	transform.position = glm::vec3(55.0f, 3.0f, 65.0f);
	EXPECT_EQ(DrawnModel(registry, creature), affine::Model(transform));
	EXPECT_NE(object_placement::Cells(registry, creature), affine::Model(transform));
}

// The screen test the field of view, the help bubble's sign, the guidance's remarks and the draw list share takes a
// villager's sphere through the matrix its last draw left: its centre and origin are the drawn place's, not the turn's
TEST(ObjectPlacement, TheScreenTestOfALivingThingTakesItsLastDrawnMatrix)
{
	if (!NoLand())
	{
		GTEST_SKIP() << "a land is in the locator";
	}
	Registry registry;
	const auto entity = VillagerBetweenTurns(registry);
	object_placement::KeepDrawn(registry, entity);
	const auto drawn = PlacedAsDrawn(registry, entity);
	registry.Get<Transform>(entity).position = glm::vec3(101.0f, 5.5f, 201.0f);
	const AxisAlignedBoundingBox box {.minima = {-1.0f, 0.0f, -1.0f}, .maxima = {1.0f, 4.0f, 1.0f}};
	const auto inputs = draw_list::OnScreenInputsFrom(object_placement::Of(registry, entity), box, false);
	EXPECT_EQ(inputs.origin, glm::vec3(drawn[3]));
	EXPECT_EQ(inputs.boxCentreWorld, affine::BoxCentreThroughObject(affine::FromModel(drawn), box.Center()));
	EXPECT_NE(inputs.origin, registry.Get<const Transform>(entity).position);
}

// A villager or an animal is placed at its creation as the original's creation places it: the yaw constructor at its
// place, its y angle and its scale (the oracle's cells), read so before its first draw however its turn moves it
TEST(ObjectPlacement, BeforeItsFirstDrawALivingThingIsReadAsItsCreationPlacedIt)
{
	Registry registry;
	const auto entity = registry.Create();
	const auto& c = k_YawCases.at(6);
	auto& transform = registry.Assign<Transform>(entity, Point(), glm::mat3(1.0f), glm::vec3(F(c.scale)));
	registry.Assign<Villager>(entity);
	object_placement::PlaceLiving(registry, entity, Point(), F(c.angle), F(c.scale));
	transform.position += glm::vec3(0.5f, 0.0f, -0.25f);
	ExpectCells(object_placement::Cells(registry, entity), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, entity).scale), c.scale);
	EXPECT_EQ(std::bit_cast<uint32_t>(registry.Get<const LastDrawnMatrix>(entity).yAngle), c.angle);
}

// A villager a saved game makes is placed as the original's load places it (its creation again): the yaw constructor
// at its place, its loaded y angle and its scale (the oracle's cells), read so before its first draw
TEST(ObjectPlacement, AVillagerALoadMakesIsReadAsTheLoadPlacedIt)
{
	const openblack::test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const openblack::test::RestoreService<Locator::infoConstants> restoreInfo;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const auto& c = k_YawCases.at(9);
	const auto villager =
	    archetypes::VillagerArchetype::Restore(Point(), static_cast<VillagerInfo>(0), F(c.angle), F(c.scale), false);
	ExpectCells(object_placement::Cells(registry, villager), c.cells);
	EXPECT_EQ(std::bit_cast<uint32_t>(object_placement::Of(registry, villager).scale), c.scale);
}
