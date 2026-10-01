/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object size family (ECS/ObjectMetrics): the mesh fields of LH3DMesh::ComputeBoundingBox 0x8081B0, the mesh level
// (the inline copies, no override) and the object level (Object::Get2DRadius 0x638180, GetHeight 0x638120, GetRadius
// 0x638110 and the overrides of the vtables: Field 0x528E80, FishFarm 0x52C470, PileFood 0x66F180, MagicTeleport
// 0x5FCCB0, MagicFireBall 0x682D20 / 0x682D30, Creature 0x477F50), GetProportionRaised 0x66EB60 / 0x66F1B0 and the
// derived routines (GetHoldRadius 0x638C00, GetDefaultFireRadius 0x639AC0, GetVillagerHugRadius 0x4026B0, the
// distances 0x637FB0 / 0x5702B0)

#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace object = openblack::ecs::object;

namespace
{
constexpr entt::id_type k_Box = 1;     ///< min (-1, 0, -2), max (1, 4, 2): half extents (1, 2, 2)
constexpr entt::id_type k_Wide = 2;    ///< min (-3, -1, -1), max (3, 1, 1): half extents (3, 1, 1)
constexpr entt::id_type k_Missing = 3; ///< not loaded

std::optional<AxisAlignedBoundingBox> Boxes(entt::id_type id)
{
	switch (id)
	{
	case k_Box:
		return AxisAlignedBoundingBox {{-1.0f, 0.0f, -2.0f}, {1.0f, 4.0f, 2.0f}};
	case k_Wide:
		return AxisAlignedBoundingBox {{-3.0f, -1.0f, -1.0f}, {3.0f, 1.0f, 1.0f}};
	default:
		return std::nullopt;
	}
}

class ObjectMetrics: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		object::detail::SetMeshBoxProviderForTests(&Boxes);
		auto info = std::make_unique<InfoConstants>();
		auto& food = info->pot.at(static_cast<size_t>(PotInfo::MagicFood));
		food.potType = PotType::PileFood;
		food.resourceType = ResourceType::Food;
		food.maxAmountInPot = 1000;
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.potType = PotType::PileWood;
		wood.resourceType = ResourceType::Wood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		object::detail::SetMeshBoxProviderForTests(nullptr);
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity Make(entt::id_type mesh, float scale, glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
		registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		return entity;
	}
};
} // namespace

TEST(ObjectMetricsMesh, BoxFields)
{
	// 0x80831C..0x808379: half extents (max - min) x 0.5 and the half diagonal sqrt((hz^2 + hy^2) + hx^2)
	const auto half = object::HalfExtents({{-1.0f, 0.0f, -2.0f}, {1.0f, 4.0f, 2.0f}});
	EXPECT_EQ(half, glm::vec3(1.0f, 2.0f, 2.0f));
	EXPECT_FLOAT_EQ(object::HalfDiagonal(half), 3.0f);
	// 0x6381B7: s x max(hx, hz)
	EXPECT_FLOAT_EQ(object::Radius2D({1.0f, 9.0f, 2.0f}, 1.5f), 3.0f);
	EXPECT_FLOAT_EQ(object::Radius2D({3.0f, 9.0f, 2.0f}, 0.5f), 1.5f);
	// 0x638136..0x63813D: 2 x hy x s (the whole height, from the half height)
	EXPECT_FLOAT_EQ(object::Height({1.0f, 2.0f, 2.0f}, 1.5f), 6.0f);
}

TEST_F(ObjectMetrics, MeshLevelHasNoOverride)
{
	EXPECT_FLOAT_EQ(object::MeshRadius2D(k_Box, 2.0f), 4.0f);
	EXPECT_FLOAT_EQ(object::MeshHalfHeight(k_Box), 2.0f); // +0x28, no scale (Field::Draw 0x5287C5)
	EXPECT_FLOAT_EQ(object::MeshHalfDiagonal(k_Box), 3.0f);
	// no mesh: 0, never a stand-in size
	EXPECT_EQ(object::MeshRadius2D(k_Missing, 2.0f), 0.0f);
	EXPECT_EQ(object::MeshHalfHeight(k_Missing), 0.0f);
	EXPECT_FALSE(object::MeshHalfExtents(k_Missing).has_value());
}

TEST_F(ObjectMetrics, ObjectBase)
{
	const auto rock = Make(k_Wide, 2.0f);
	EXPECT_FLOAT_EQ(object::Get2DRadius(rock), 6.0f); // 0x638180: 2 x max(3, 1)
	EXPECT_FLOAT_EQ(object::GetRadius(rock), 6.0f);   // 0x638110 = vt +0x64
	EXPECT_FLOAT_EQ(object::GetHeight(rock), 4.0f);   // 0x638120: 2 x 1 x 2
	EXPECT_FLOAT_EQ(object::GetMeshRadius(rock), std::sqrt(11.0f)); // 0x636BD0: +0x30, no scale
	// without a loaded mesh, or without a Mesh at all: 0 (GameThing 0x405150, 0x6381E9, 0x638140)
	const auto bare = Make(k_Missing, 1.0f);
	EXPECT_EQ(object::Get2DRadius(bare), 0.0f);
	EXPECT_EQ(object::GetHeight(bare), 0.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	EXPECT_EQ(object::Get2DRadius(thing), 0.0f);
	EXPECT_EQ(object::GetHeight(thing), 0.0f);
	EXPECT_EQ(object::Get2DRadius(entt::null), 0.0f);
}

TEST_F(ObjectMetrics, FieldAndFishFarmAreFiveMetres)
{
	// Field 0x528E80 / FishFarm 0x52C470: 5, whatever the mesh and the scale; the height stays the Object one
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = Make(k_Wide, 3.0f);
	registry.Assign<Field>(field);
	EXPECT_EQ(object::Get2DRadius(field), 5.0f);
	EXPECT_EQ(object::GetRadius(field), 5.0f);
	EXPECT_EQ(object::GetMeshRadius(field), 5.0f); // 0x528A30
	EXPECT_FLOAT_EQ(object::GetHeight(field), 6.0f);
	// the inline mesh-level copies (IsSuitableForFixed 0x603E1E, the scaffolds) see its mesh
	EXPECT_FLOAT_EQ(object::MeshRadius2D(k_Wide, 3.0f), 9.0f);

	const auto farm = Make(k_Box, 1.0f);
	registry.Assign<FishFarm>(farm);
	EXPECT_EQ(object::Get2DRadius(farm), 5.0f);
	EXPECT_EQ(object::GetMeshRadius(farm), 5.0f); // 0x52C480
}

TEST_F(ObjectMetrics, MagicObjects)
{
	auto& registry = Locator::entitiesRegistry::value();
	// MagicTeleport 0x5FCCB0 = 6 (no mesh)
	const auto teleport = registry.Create();
	registry.Assign<MagicTeleport>(teleport);
	EXPECT_EQ(object::Get2DRadius(teleport), 6.0f);
	EXPECT_EQ(object::GetHeight(teleport), 0.0f);
	// MagicFireBall 0x682D20 = GetScale x 1; 0x682D30 = the same as its height
	const auto ball = Make(k_Box, 2.5f);
	registry.Assign<MagicFireBall>(ball);
	EXPECT_EQ(object::Get2DRadius(ball), 2.5f);
	EXPECT_EQ(object::GetHeight(ball), 2.5f);
	// a map shield's object scale (Object +0x50), not the drawn Transform one
	const auto shield = Make(k_Box, 1.0f);
	registry.Assign<MapShield>(shield).objectScale = 3.0f;
	EXPECT_FLOAT_EQ(object::Get2DRadius(shield), 6.0f);
	EXPECT_FLOAT_EQ(object::GetHeight(shield), 12.0f);
}

TEST_F(ObjectMetrics, CreatureHeight)
{
	// Creature 0x477F50: the user size x 15 [0x8C2C40], not the mesh
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = Make(k_Box, 0.5f);
	registry.Assign<Creature>(creature);
	EXPECT_FLOAT_EQ(object::GetHeight(creature), 7.5f);
}

TEST_F(ObjectMetrics, ProportionRaised)
{
	using object::PileFoodProportionRaised;
	using object::PileWoodProportionRaised;
	// 0x66EB60: 200 of 1000 -> p = 0.95 x 0.2 + 0.05 = 0.24 -> 1 - 0.76^2
	const float p = (1.0f - 0.05f) * 0.2f + 0.05f;
	EXPECT_FLOAT_EQ(PileFoodProportionRaised(200, 1000), 1.0f - (1.0f - p) * (1.0f - p));
	// an empty pile is 0 (0x66EBB7..0x66EBC2), not 1 - 0.95^2
	EXPECT_EQ(PileFoodProportionRaised(0, 1000), 0.0f);
	EXPECT_EQ(PileFoodProportionRaised(5000, 1000), 1.0f);
	// 0x66F1B0: the floor and the clamp, no square
	EXPECT_FLOAT_EQ(PileWoodProportionRaised(200, 1000), p);
	EXPECT_EQ(PileWoodProportionRaised(0, 1000), 0.0f);
	EXPECT_EQ(PileWoodProportionRaised(5000, 1000), 1.0f);
}

TEST_F(ObjectMetrics, PileFoodRadius)
{
	// PileFood 0x66F180 = GetProportionRaised x Object::Get2DRadius; PileWood keeps the Object one
	auto& registry = Locator::entitiesRegistry::value();
	const auto food = Make(k_Box, 1.0f);
	registry.Assign<Pot>(food, static_cast<uint16_t>(200), static_cast<uint16_t>(1000), PotInfo::MagicFood);
	EXPECT_FLOAT_EQ(object::Get2DRadius(food), 2.0f * object::PileFoodProportionRaised(200, 1000));
	EXPECT_FLOAT_EQ(object::ObjectGet2DRadius(food), 2.0f);
	EXPECT_FLOAT_EQ(object::GetHeight(food), 4.0f); // GetHeight is not overridden
	registry.Get<Pot>(food).amount = 0;
	EXPECT_EQ(object::Get2DRadius(food), 0.0f);

	const auto wood = Make(k_Box, 1.0f);
	registry.Assign<Pot>(wood, static_cast<uint16_t>(200), static_cast<uint16_t>(1000), PotInfo::MagicWood);
	EXPECT_FLOAT_EQ(object::Get2DRadius(wood), 2.0f);
	EXPECT_FLOAT_EQ(object::GetProportionRaised(wood), object::PileWoodProportionRaised(200, 1000));
}

TEST_F(ObjectMetrics, Derived)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = Make(k_Box, 1.0f); // R2D 2, height 4
	EXPECT_FLOAT_EQ(object::GetHoldRadius(stone, true), 3.0f);  // 0x638C00: 0.75 x height
	EXPECT_FLOAT_EQ(object::GetHoldRadius(stone, false), 2.0f); // else Get2DRadius
	EXPECT_FLOAT_EQ(object::GetDefaultFireRadius(stone), 2.0f); // 0x639AC0
	EXPECT_FLOAT_EQ(object::GetVillagerHugRadius(stone), 2.0f * 1.05f + 0.0005f); // 0x4026B0

	const auto tree = Make(k_Box, 1.0f);
	registry.Assign<Tree>(tree);
	EXPECT_FLOAT_EQ(object::GetHoldRadius(tree, true), 2.0f * 0.2f); // 0x74B610
	EXPECT_FLOAT_EQ(object::GetVillagerHugRadius(tree), 0.2f);       // 0x74A1A0: min(0.1 x 2, 0.25)
	const auto bigTree = Make(k_Box, 2.0f);
	registry.Assign<Tree>(bigTree);
	EXPECT_EQ(object::GetVillagerHugRadius(bigTree), 0.25f);
	EXPECT_EQ(object::GetRoutePlanRadius(bigTree), 0.25f); // 0x74A140

	const auto dead = Make(k_Box, 1.0f);
	registry.Assign<DeadTree>(dead);
	EXPECT_FLOAT_EQ(object::GetDefaultFireRadius(dead), 4.0f * 0.35f); // 0x510E10
	const auto site = Make(k_Box, 1.0f);
	registry.Assign<WorshipSite>(site);
	EXPECT_EQ(object::GetDefaultFireRadius(site), 14.0f); // 0x77DE10 -> 0x77DDD0
}

TEST_F(ObjectMetrics, Distances)
{
	// 0x637FB0: distance - (R2D(b) + R2D(a)); 0x5702B0: distance - GetRadius
	const auto a = Make(k_Box, 1.0f, {100.0f, 0.0f, 100.0f});  // R2D 2
	const auto b = Make(k_Wide, 1.0f, {110.0f, 0.0f, 100.0f}); // R2D 3
	const float d = object::GetDistanceFromObject(a, b);
	EXPECT_NEAR(d, 5.0f, 0.01f);
	EXPECT_TRUE(object::IsTouching(a, b, 5.1f));
	EXPECT_FALSE(object::IsTouching(a, b, 4.9f));
	EXPECT_NEAR(object::GetDistanceFromObject(a, glm::vec3(103.0f, 0.0f, 100.0f)), 1.0f, 0.01f);
	EXPECT_TRUE(object::IsTouching(a, glm::vec3(101.0f, 0.0f, 100.0f)));
	EXPECT_FALSE(object::IsTouching(a, glm::vec3(103.0f, 0.0f, 100.0f)));
}

TEST_F(ObjectMetrics, BoundingSphere)
{
	// 0x637730: r = sqrt(R2D^2 + (H / 2)^2), centre (x, ground + altitude + H / 2, z); no island here, the ground is 0
	const auto a = Make(k_Box, 1.0f, {20.0f, 1.0f, 30.0f}); // R2D 2, H 4
	const auto sphere = object::GetBoundingSphere(a);
	EXPECT_FLOAT_EQ(sphere.radius, std::sqrt(4.0f + 4.0f));
	EXPECT_NEAR(sphere.centre.x, 20.0f, 1e-3f);
	EXPECT_NEAR(sphere.centre.z, 30.0f, 1e-3f);
	EXPECT_FLOAT_EQ(sphere.centre.y, 3.0f);
	EXPECT_FLOAT_EQ(object::GetTopPos(a), 5.0f); // 0x638160: altitude 1 + height 4
}

TEST_F(ObjectMetrics, DerivedOverrides)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Living 0x5ED2F0 / MobileStatic 0x608F40: the bounding sphere with R2D x 0.5
	const auto animal = Make(k_Box, 1.0f, {20.0f, 0.0f, 30.0f}); // R2D 2, H 4
	registry.Assign<Animal>(animal);
	EXPECT_FLOAT_EQ(object::GetBoundingSphere(animal).radius, std::sqrt(1.0f + 4.0f));
	const auto rock = Make(k_Box, 1.0f, {20.0f, 0.0f, 30.0f});
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Boulder1Chalk);
	EXPECT_FLOAT_EQ(object::GetBoundingSphere(rock).radius, std::sqrt(1.0f + 4.0f));

	// MapShield 0x72C1C0: the top is 0; FishFarm 0x52C840: the hand's height above it is 5
	const auto shield = Make(k_Box, 1.0f, {0.0f, 1.0f, 0.0f});
	registry.Assign<MapShield>(shield).objectScale = 1.0f;
	EXPECT_EQ(object::GetTopPos(shield), 0.0f);
	const auto farm = Make(k_Box, 1.0f);
	registry.Assign<FishFarm>(farm);
	EXPECT_EQ(object::GetHeightForHandAboveInteractObject(farm), 5.0f);

	// CitadelHeart 0x4680C0: Get2DRadius x 0.33
	const auto heart = Make(k_Box, 1.0f);
	registry.Assign<Temple>(heart, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(object::GetRoutePlanRadius(heart), 2.0f * 0.33f);

	// WorshipSite 0x77DE20: from CalculateCentrePos 0x77DD40 (12.55 right, 26.1 back), minus 14 + R2D(b)
	const auto site = Make(k_Box, 1.0f, {100.0f, 0.0f, 100.0f});
	registry.Assign<WorshipSite>(site);
	const auto centre = object::WorshipSiteCentre(site);
	EXPECT_FLOAT_EQ(centre.x, 112.55f);
	EXPECT_FLOAT_EQ(centre.z, 100.0f - 26.1f);
	const auto b = Make(k_Wide, 1.0f, {centre.x + 20.0f, 0.0f, centre.z}); // R2D 3
	EXPECT_NEAR(object::GetDistanceFromObject(site, b), 20.0f - (14.0f + 3.0f), 0.01f);
	EXPECT_TRUE(object::IsTouching(site, b, 3.1f));
	EXPECT_FALSE(object::IsTouching(site, b, 2.9f));
}
