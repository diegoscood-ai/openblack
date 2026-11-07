/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The huts and boulders of the testbed scene physics.building_breaks, played turn by turn in a small world of the game's
// own systems and data: a blow is judged by the boulder's momentum, and up to 2000 a hut stays whole; above it the hut
// cracks, its broken part flies off with 0.3 of the boulder's velocity, the pieces hit only the land, and the hut's life
// becomes the share of its model left. Every expected value is the original's
// (docs/wiki/physics/physics-impacts.md, docs/wiki/project/progress/physics/impact_damage.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PhysicsEntry.h"
#include "Physics/DamageMesh.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::BuildingDamage;
using ecs::components::Fragment;
using ecs::components::Transform;

namespace
{
constexpr std::string_view k_Scene = "physics.building_breaks";
/// The scene's first hut and the first boulder thrown at it, by their places in its objects and throws
constexpr size_t k_Hut = 0;
constexpr size_t k_Boulder = 3;
constexpr size_t k_Throw = 0;
/// Where the hut stands: the middle of a map cell, well inside the map
const glm::vec2 k_HutAt {1005.0f, 1235.0f};

class BuildingBreakWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		// the boulder's row: of the rock kind, its model a box
		auto& boulder = Info().mobileStatic.at(static_cast<size_t>(BoulderType()));
		boulder.mobileType = static_cast<MobileStaticInfo>(2);
		boulder.meshId = MeshId::Boulder2Chalk;
		boulder.weight = 400.0f;
		// the hut's row: its model a box, its crush defence 0.2; the physical destruction's effect crushes by one life
		auto& hut = Info().abode.at(static_cast<size_t>(HutType()));
		hut.meshId = MeshId::BuildingCeltic1;
		hut.defenceMultiplierCrush = 0.2f;
		Info().effect.at(3).effectCrush = 1.0f;
	}

	[[nodiscard]] static AbodeInfo HutType() { return std::get<AbodeInfo>(Scene(k_Scene).objects.at(k_Hut).type); }
	[[nodiscard]] static MobileStaticInfo BoulderType()
	{
		return std::get<MobileStaticInfo>(Scene(k_Scene).objects.at(k_Boulder).type);
	}

	/// The scene's hut, or another building, of no town, standing on the land
	[[nodiscard]] entt::entity Hut(std::optional<AbodeInfo> type = std::nullopt) const
	{
		const auto row = type.value_or(HutType());
		// on synthetic data a box 8 m wide and 5 m tall, standing on its base, its walls of half-metre squares
		LoadMesh(Info().abode.at(static_cast<size_t>(row)).meshId, glm::vec3(-4.0f, 0.0f, -4.0f), glm::vec3(4.0f, 5.0f, 4.0f),
		         0.5f);
		return ecs::archetypes::AbodeArchetype::Create(
		    ecs::components::Abode::k_NoTown, glm::vec3(k_HutAt.x, k_Ground, k_HutAt.y), row, 0.0f, 1.0f, 0, 0, false, false);
	}

	/// The scene's boulder where the scene puts it from its hut, thrown at it as the scene throws it, at `speed` times the
	/// throw's velocity
	[[nodiscard]] ecs::PhysicsEntry& ThrownBoulder() const
	{
		const auto& scene = Scene(k_Scene);
		const auto from = k_HutAt + (scene.objects.at(k_Boulder).offset - scene.objects.at(k_Hut).offset);
		const auto& thrown = scene.throws.at(k_Throw);
		return Boulder(glm::vec3(from.x, k_Ground + thrown.height, from.y), thrown.velocity);
	}

	/// A boulder of the scene's kind let go there at that velocity
	[[nodiscard]] ecs::PhysicsEntry& Boulder(glm::vec3 at, glm::vec3 velocity) const
	{
		// on synthetic data a box 2 m wide and 2 m tall
		LoadMesh(Info().mobileStatic.at(static_cast<size_t>(BoulderType())).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 2.0f, 1.0f));
		const auto boulder = ecs::archetypes::MobileStaticArchetype::CreateFromInfo(at, BoulderType(), 0.0f, 0.0f, 1.0f);
		auto* po = Dynamics().AddObject(boulder, velocity, glm::vec3(0.0f));
		EXPECT_NE(po, nullptr);
		return *po;
	}

	[[nodiscard]] static float LifeOf(entt::entity thing) { return ecs::life::LifeOf(thing); }

	/// The share of the hut's model its damage left, read from a copy of its broken model
	[[nodiscard]] static float Remaining(entt::entity hut)
	{
		auto broken = *Registry().Get<const BuildingDamage>(hut).mesh;
		return broken.GetRemaining();
	}

	[[nodiscard]] static std::vector<entt::entity> Fragments()
	{
		std::vector<entt::entity> found;
		Registry().Each<const Fragment>([&found](entt::entity thing, const Fragment& /*fragment*/) { found.push_back(thing); });
		return found;
	}

	/// Turns until the hut is broken or the turns are up: the boulder's velocity as the turn of the blow ended
	std::optional<glm::vec3> PlayUntilBroken(entt::entity hut, entt::entity boulder, int turns)
	{
		for (int turn = 0; turn < turns; ++turn)
		{
			Turn();
			if (Registry().AllOf<BuildingDamage>(hut) && Registry().Get<const BuildingDamage>(hut).mesh)
			{
				const auto* po = Dynamics().Find(boulder);
				return po != nullptr ? std::optional(po->body.velocity) : std::nullopt;
			}
		}
		return std::nullopt;
	}
};
} // namespace

// The scene's boulder strikes the hut with a momentum (its speed times its mass) above 2000: the hut cracks
TEST_P(BuildingBreakWorld, ABlowOfMomentumAbove2000CracksTheHut)
{
	const auto hut = Hut();
	auto& po = ThrownBoulder();
	ASSERT_GT(glm::length(po.body.velocity) * po.body.Mass(), 2000.0f);
	const auto boulder = po.entity;
	ASSERT_TRUE(PlayUntilBroken(hut, boulder, 60).has_value());
	EXPECT_LT(Remaining(hut), 1.0f);
}

// A blow of a momentum of 2000 or less leaves the hut whole: no part of it breaks and it loses no life
TEST_P(BuildingBreakWorld, ABlowOfMomentumUpTo2000LeavesTheHutWhole)
{
	const auto hut = Hut();
	// let go just in front of the hut's south wall, flying at it level, slowly enough that its momentum is at most 2000
	const float reach = ecs::object::Get2DRadius(hut);
	auto& po = Boulder(glm::vec3(k_HutAt.x, k_Ground + 1.0f, k_HutAt.y - reach - 3.0f), glm::vec3(0.0f));
	const float speed = 1900.0f / po.body.Mass();
	po.body.velocity = glm::vec3(0.0f, 0.0f, speed);
	bool struck = false;
	for (int turn = 0; turn < 100; ++turn)
	{
		Turn();
		EXPECT_FALSE(Registry().AllOf<BuildingDamage>(hut)) << "turn " << turn;
		EXPECT_EQ(LifeOf(hut), 1.0f) << "turn " << turn;
		struck = struck || Dynamics().Find(hut) != nullptr;
	}
	// it reached the hut: the hut was in the physics, an obstacle it struck
	EXPECT_TRUE(struck);
	EXPECT_TRUE(Fragments().empty());
}

// The broken part flies off as a piece of its own with 0.3 of the boulder's velocity at the blow; the parts loosened
// with it fall still
TEST_P(BuildingBreakWorld, TheBrokenPartFliesOffWithThreeTenthsOfTheBouldersVelocity)
{
	const auto hut = Hut();
	const auto boulder = ThrownBoulder().entity;
	const auto velocity = PlayUntilBroken(hut, boulder, 60);
	ASSERT_TRUE(velocity.has_value());
	const auto pieces = Fragments();
	ASSERT_FALSE(pieces.empty());
	size_t flying = 0;
	for (const auto piece : pieces)
	{
		const auto* po = Dynamics().Find(piece);
		ASSERT_NE(po, nullptr);
		if (po->body.velocity == *velocity * 0.3f)
		{
			++flying;
		}
		else
		{
			EXPECT_EQ(po->body.velocity, glm::vec3(0.0f));
		}
	}
	EXPECT_GE(flying, 1u);
}

// The pieces hit only the land: nothing they fly into collides with them, and they live 100 turns for each triangle
// they broke off with
TEST_P(BuildingBreakWorld, ThePiecesHitOnlyTheLandAndLastAHundredTurnsATriangle)
{
	const auto hut = Hut();
	const auto boulder = ThrownBoulder().entity;
	ASSERT_TRUE(PlayUntilBroken(hut, boulder, 60).has_value());
	const auto pieces = Fragments();
	ASSERT_FALSE(pieces.empty());
	for (const auto piece : pieces)
	{
		EXPECT_FALSE(Dynamics().InteractsWithPhysicsObjects(piece));
		const auto* po = Dynamics().Find(piece);
		ASSERT_NE(po, nullptr);
		EXPECT_NE(po->flags & ecs::PhysicsEntry::k_NoObjectCollision, 0u);
		const auto& fragment = Registry().Get<const Fragment>(piece);
		EXPECT_GT(fragment.turnsLeft, 0);
		EXPECT_EQ(fragment.turnsLeft % 100, 0);
	}
}

// The hut's life becomes the share of its model the blows left
TEST_P(BuildingBreakWorld, TheHutsLifeBecomesTheShareOfItsModelLeft)
{
	const auto hut = Hut();
	const auto boulder = ThrownBoulder().entity;
	ASSERT_TRUE(PlayUntilBroken(hut, boulder, 60).has_value());
	EXPECT_FLOAT_EQ(LifeOf(hut), Remaining(hut));
}

// A storage pit takes a blow as the other buildings do: struck hard it cracks
TEST_P(BuildingBreakWorld, AStoragePitTakesABlowAsTheOtherBuildingsDo)
{
	if (!GameData())
	{
		GTEST_SKIP() << "a storage pit's piles stand at its shipped model's own points: no synthetic model stands for it";
	}
	const auto pit = Hut(AbodeInfo::CelticStoragePit);
	ASSERT_NE(pit, entt::entity {entt::null});
	ASSERT_TRUE(Registry().AllOf<ecs::components::StoragePit>(pit));
	const auto boulder = ThrownBoulder().entity;
	ASSERT_TRUE(PlayUntilBroken(pit, boulder, 60).has_value());
	EXPECT_LT(Remaining(pit), 1.0f);
}

INSTANTIATE_TEST_SUITE_P(Data, BuildingBreakWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
