/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rock of the testbed scene physics.throw_one, thrown and played turn by turn in a small world of the game's own
// systems and data, and the things it meets as in physics.throw_many: its body is every point of its model about their
// middle, its radius the farthest of them and its air drag 0.3 of its radius squared times its material's; it flies
// without turning until it touches, the touch off its centre turns it, and it comes to rest still; what it flies near
// is woken within its radius plus a tenth of a second of its sideways travel, and a sleeping thing it strikes is
// knocked into flight. Every expected value is the original's (docs/wiki/physics/physics.md#manager,
// docs/wiki/project/progress/physics/object_dynamics.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/PhysicsEntry.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Transform;

namespace
{
constexpr std::string_view k_Scene = "physics.throw_one";
/// The middle of a map cell, well inside the map
const glm::vec2 k_Over {1005.0f, 1205.0f};

class ThrowWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		// the rock's row: of the rock kind, its model a box
		auto& row = Info().mobileStatic.at(static_cast<size_t>(SceneRockType()));
		row.mobileType = static_cast<MobileStaticInfo>(2);
		row.meshId = MeshId::ObjectRockChalk;
		row.weight = 300.0f;
		// a standing tree and the bonfire: their models boxes
		Info().tree.at(static_cast<size_t>(TreeInfo::Beech)).normal = MeshId::TreeBeech;
		auto& bonfire = Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Bonfire));
		bonfire.meshId = MeshId::ObjectRockChalk;
		bonfire.weight = 300.0f;
	}

	/// A rock flying level at the thing from 10 m east of it, 3 m up: whether it went past it, the thing never in the
	/// physics
	bool FliesThrough(entt::entity thing)
	{
		const auto at = Registry().Get<const ecs::components::Transform>(thing).position;
		const auto rock = Rock(glm::vec2(at.x + 10.0f, at.z), 1.0f, 3.0f);
		auto* po = Dynamics().AddObject(rock, glm::vec3(-25.0f, 0.0f, 0.0f), glm::vec3(0.0f));
		EXPECT_NE(po, nullptr);
		for (int turn = 0; turn < 10; ++turn)
		{
			Turn();
			EXPECT_EQ(Dynamics().Find(thing), nullptr) << "turn " << turn;
			const auto* now = Dynamics().Find(rock);
			if (now != nullptr && now->body.Centre().x < at.x - 5.0f)
			{
				return now->body.velocity.x < -20.0f;
			}
		}
		return false;
	}

	[[nodiscard]] static MobileStaticInfo SceneRockType()
	{
		return std::get<MobileStaticInfo>(Scene(k_Scene).objects.at(0).type);
	}

	[[nodiscard]] const graphics::L3DMesh& RockModel() const
	{
		const auto id = Info().mobileStatic.at(static_cast<size_t>(SceneRockType())).meshId;
		// on synthetic data a box 2 m wide and 1.5 m tall, standing on its base
		LoadMesh(id, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.5f, 1.0f));
		return *Locator::resources::value().GetMeshes().Handle(resources::HashIdentifier(id));
	}

	/// A rock of the scene's kind standing on the land, this high above it
	[[nodiscard]] entt::entity Rock(glm::vec2 at = k_Over, float scale = 1.0f, float height = 0.0f) const
	{
		static_cast<void>(RockModel());
		return ecs::archetypes::MobileStaticArchetype::CreateFromInfo(glm::vec3(at.x, k_Ground + height, at.y), SceneRockType(),
		                                                              0.0f, 0.0f, scale);
	}

	/// The scene's rock thrown as the scene throws it: let go at the throw's height above the land and velocity
	[[nodiscard]] ecs::PhysicsEntry& ThrownRock() const
	{
		const auto& thrown = Scene(k_Scene).throws.at(0);
		const auto rock = Rock(k_Over, Scene(k_Scene).objects.at(0).scale, thrown.height);
		auto* po = Dynamics().AddObject(rock, thrown.velocity, glm::vec3(0.0f));
		EXPECT_NE(po, nullptr);
		return *po;
	}

	/// The model's points the body is built from: those of its physics parts, or of its first level of detail when it
	/// has none, or of all of them when no part is of the first level
	[[nodiscard]] static std::vector<glm::vec3> BodyPointsOf(const graphics::L3DMesh& model)
	{
		const auto& parts = model.GetSubMeshes();
		const bool physics = std::ranges::any_of(parts, [](const auto& part) { return part->IsPhysics(); });
		const bool first = std::ranges::any_of(parts, [](const auto& part) { return (part->GetFlags().lodMask & 1) != 0; });
		std::vector<glm::vec3> points;
		for (const auto& part : parts)
		{
			if (physics ? part->IsPhysics() : (!first || (part->GetFlags().lodMask & 1) != 0))
			{
				const auto& own = part->GetCollisionPositions();
				points.insert(points.end(), own.begin(), own.end());
			}
		}
		return points;
	}
};
} // namespace

// Every point of the model's physics parts (or of its first level of detail) is a point of the body, about their middle
// and times the object's scale
TEST_P(ThrowWorld, TheBodyIsEveryPointOfItsModelAboutTheirMiddleTimesTheScale)
{
	constexpr float k_Scale = 1.5f;
	const auto& model = RockModel();
	const auto rock = Rock(k_Over, k_Scale);
	auto* po = Dynamics().AddObject(rock, glm::vec3(0.0f), glm::vec3(0.0f));
	ASSERT_NE(po, nullptr);
	const auto points = BodyPointsOf(model);
	ASSERT_EQ(po->body.Points().size(), points.size());
	glm::vec3 middle(0.0f);
	for (const auto& point : points)
	{
		middle += point;
	}
	middle /= static_cast<float>(points.size());
	for (size_t i = 0; i < points.size(); ++i)
	{
		EXPECT_EQ(po->body.Points().at(i).local, (points.at(i) - middle) * k_Scale) << i;
	}
}

// The body's radius is the farthest of its points from its centre of mass, and its air drag its radius squared times
// its material's drag times 0.3
TEST_P(ThrowWorld, TheRadiusIsTheFarthestPointAndTheDragAThirdOfItsSquareTimesTheMaterials)
{
	auto& po = ThrownRock();
	float farthest = 0.0f;
	for (const auto& point : po.body.Points())
	{
		farthest = std::max(farthest, glm::length(point.local));
	}
	EXPECT_EQ(po.body.Radius(), farthest);
	const float drag = Dynamics().Constants(Dynamics().ConstantsType(po.entity)).drag;
	EXPECT_EQ(po.body.Drag(), po.body.Radius() * drag * po.body.Radius() * 0.3f);
}

// Thrown with no spin it flies without turning until it touches the land; the touch, off its centre, turns it
TEST_P(ThrowWorld, ItFliesWithoutTurningUntilAnOffCentreTouchTurnsIt)
{
	auto& po = ThrownRock();
	const auto rock = po.entity;
	bool touched = false;
	for (int turn = 0; turn < 100 && !touched; ++turn)
	{
		Turn();
		const auto* now = Dynamics().Find(rock);
		ASSERT_NE(now, nullptr) << "turn " << turn;
		touched = now->body.Contacts() > 0 || now->body.angularMomentum != glm::vec3(0.0f);
		if (!touched)
		{
			EXPECT_EQ(now->body.angularMomentum, glm::vec3(0.0f)) << "turn " << turn;
		}
		else
		{
			EXPECT_NE(now->body.angularMomentum, glm::vec3(0.0f)) << "turn " << turn;
		}
	}
	EXPECT_TRUE(touched);
}

// It lands, rolls and comes to rest, and at rest it neither moves nor turns
TEST_P(ThrowWorld, ItComesToRestWithItsVelocityAndSpinCleared)
{
	auto& po = ThrownRock();
	const auto rock = po.entity;
	bool settled = false;
	for (int turn = 0; turn < 300 && !settled; ++turn)
	{
		Turn();
		const auto* now = Dynamics().Find(rock);
		settled = now == nullptr || now->body.resting;
		if (now != nullptr && now->body.resting)
		{
			EXPECT_EQ(now->body.velocity, glm::vec3(0.0f)) << "turn " << turn;
			EXPECT_EQ(now->body.angularMomentum, glm::vec3(0.0f)) << "turn " << turn;
		}
	}
	EXPECT_TRUE(settled);
	// still where it came to rest
	const auto rest = Registry().Get<const Transform>(rock).position;
	for (int turn = 0; turn < 20; ++turn)
	{
		Turn();
	}
	EXPECT_EQ(Registry().Get<const Transform>(rock).position, rest);
}

// What a moving body flies near is woken: the things in the map cells within its radius plus a tenth of a second of its
// sideways travel of its centre, as the turn starts; a thing further away sleeps on
TEST_P(ThrowWorld, AMovingBodyWakesWhatIsWithinItsRadiusPlusATenthOfASecondsSidewaysTravel)
{
	// high above the land, far from anything it could touch this turn
	const auto flyer = Rock(k_Over, 1.0f, 40.0f);
	auto* po = Dynamics().AddObject(flyer, glm::vec3(0.0f), glm::vec3(0.0f));
	ASSERT_NE(po, nullptr);
	const float radius = po->body.Radius();
	// a small rock 12 m past the flyer's radius: further than a falling flyer reaches, nearer than one going at 124 m/s
	const auto sleeper = Rock(k_Over + glm::vec2(radius + 12.0f, 0.0f), 0.5f);
	Turn();
	EXPECT_EQ(Dynamics().Find(sleeper), nullptr);

	po = Dynamics().Find(flyer);
	ASSERT_NE(po, nullptr);
	po->body.velocity = glm::vec3(0.0f, 0.0f, 124.0f);
	Turn();
	const auto* woken = Dynamics().Find(sleeper);
	ASSERT_NE(woken, nullptr);
	EXPECT_TRUE(woken->body.resting);
}

// A sleeping thing struck hard by a thrown one is knocked into flight, a body of its own
TEST_P(ThrowWorld, ASleepingThingStruckHardIsKnockedIntoFlight)
{
	const auto sleeper = Rock(k_Over);
	const auto thrown = Rock(k_Over + glm::vec2(8.0f, 0.0f), 1.0f, 0.5f);
	ASSERT_NE(Dynamics().AddObject(thrown, glm::vec3(-30.0f, 0.0f, 0.0f), glm::vec3(0.0f)), nullptr);
	bool knocked = false;
	for (int turn = 0; turn < 30 && !knocked; ++turn)
	{
		Turn();
		const auto* po = Dynamics().Find(sleeper);
		knocked = po != nullptr && !po->body.resting && po->body.velocity != glm::vec3(0.0f);
	}
	EXPECT_TRUE(knocked);
}

// Standing trees are not in the way of what flies: a thrown rock goes through one as if it were not there
TEST_P(ThrowWorld, AThrownThingGoesThroughAStandingTree)
{
	LoadMesh(Info().tree.at(static_cast<size_t>(TreeInfo::Beech)).normal, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 8.0f, 1.0f));
	const auto tree = ecs::archetypes::TreeArchetype::Create(0, glm::vec3(k_Over.x, k_Ground, k_Over.y), TreeInfo::Beech, false,
	                                                         0.0f, 1.0f, 1.0f);
	EXPECT_TRUE(FliesThrough(tree));
}

// The bonfire takes no part in the physics: it is not in the way of what flies, and a blow does nothing to it
TEST_P(ThrowWorld, AThrownThingGoesThroughTheBonfire)
{
	LoadMesh(Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Bonfire)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 6.0f, 1.0f));
	const auto bonfire = ecs::archetypes::BonfireArchetype::Create(glm::vec3(k_Over.x, k_Ground, k_Over.y), 0.0f, 1.0f);
	ASSERT_NE(bonfire, entt::entity {entt::null});
	EXPECT_TRUE(FliesThrough(bonfire));
}

INSTANTIATE_TEST_SUITE_P(Data, ThrowWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
