/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics' frame pose (DynamicsSystem::UpdateFrame) is the thing's own matrix: a villager or an animal in flight,
// out of the map cells and so drawn by nothing of its own, is read where this frame poses it, not where the hand let it
// go. Not what the hand holds, whose draw places it (docs/wiki/physics/physics.md#manager)

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/LastDrawnMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The physics of a test with its own entities
class PhysicsFramePoseTest: public ::testing::Test
{
protected:
	PhysicsFramePoseTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_dynamics = std::make_unique<ecs::systems::DynamicsSystem>(std::make_unique<test::FakeDynamicsWorld>(Entities()));
	}

	static ecs::Registry& Entities() { return Locator::entitiesRegistry::value(); }

	/// A villager listed in the physics, flying: its turn started at one place and ends at another, both well above
	/// the sea, at the scale it went in with
	entt::entity FlyingVillager()
	{
		const auto entity = Entities().Create();
		Entities().Assign<Transform>(
		    entity,
		    Transform {.position = glm::vec3(30.0f, 12.0f, 40.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.25f)});
		Entities().Assign<Villager>(entity);
		auto entry = std::make_unique<ecs::PhysicsEntry>();
		entry->entity = entity;
		entry->body.Initialise(1.25f, 2.0f);
		entry->body.RestorePose(glm::mat3(1.0f), glm::vec3(32.0f, 14.0f, 41.0f));
		entry->turnStarted = true;
		entry->turnStartRotation = glm::mat3(1.0f);
		entry->turnStartCentre = glm::vec3(30.0f, 12.0f, 40.0f);
		_dynamics->GetState().objects.push_back(std::move(entry));
		return entity;
	}

	/// The matrix the hand left on it where it let it go
	static void LetGo(entt::entity entity)
	{
		Entities().Assign<LastDrawnMatrix>(
		    entity, LastDrawnMatrix {
		                .cells = glm::mat4x3(affine::Model(glm::vec3(30.0f, 12.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.25f))),
		                .scale = 1.25f,
		                .yAngle = 0.5f});
	}

	const test::RestoreService<Locator::entitiesRegistry> _restore;
	std::unique_ptr<ecs::systems::DynamicsSystem> _dynamics;
};
} // namespace

TEST_F(PhysicsFramePoseTest, AVillagerInFlightIsReadWhereTheFramePosesIt)
{
	const auto villager = FlyingVillager();
	ASSERT_TRUE(villager != entt::null);
	LetGo(villager);

	_dynamics->UpdateFrame(0.5f, 0.02f);

	const auto* pose = Entities().TryGet<const PhysicsDrawPose>(villager);
	ASSERT_TRUE(pose != nullptr);
	const auto& kept = Entities().Get<const LastDrawnMatrix>(villager);
	EXPECT_EQ(kept.cells, glm::mat4x3(affine::Model(pose->position, pose->rotation, glm::vec3(*pose->scale))));
	EXPECT_EQ(glm::vec3(kept.cells[3]), pose->position);
	EXPECT_EQ(kept.scale, 1.25f);

	// the next frame poses it further on, and it is read there
	const auto first = kept.cells;
	_dynamics->UpdateFrame(0.75f, 0.02f);
	EXPECT_NE(Entities().Get<const LastDrawnMatrix>(villager).cells, first);
	EXPECT_EQ(glm::vec3(Entities().Get<const LastDrawnMatrix>(villager).cells[3]),
	          Entities().Get<const PhysicsDrawPose>(villager).position);
}

TEST_F(PhysicsFramePoseTest, WhatTheHandHoldsKeepsTheMatrixTheHandsDrawLeft)
{
	const auto villager = FlyingVillager();
	ASSERT_TRUE(villager != entt::null);
	LetGo(villager);
	Entities().Assign<HandDrawPose>(villager);
	const auto before = Entities().Get<const LastDrawnMatrix>(villager).cells;

	_dynamics->UpdateFrame(0.5f, 0.02f);

	EXPECT_EQ(Entities().Get<const LastDrawnMatrix>(villager).cells, before);
}

TEST_F(PhysicsFramePoseTest, AThingAParticleCarriesHasNoBodySoTheFramePosesNothingOfIt)
{
	// carried by a particle: in the physics with no body of its own, so not listed (its particle's draw places it)
	const auto carried = Entities().Create();
	ASSERT_TRUE(carried != entt::null);
	Entities().Assign<Transform>(
	    carried, Transform {.position = glm::vec3(5.0f, 8.0f, 6.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	Entities().Assign<Villager>(carried);
	LetGo(carried);
	const auto before = Entities().Get<const LastDrawnMatrix>(carried).cells;
	const auto flying = FlyingVillager();
	ASSERT_TRUE(flying != entt::null);

	_dynamics->UpdateFrame(0.5f, 0.02f);

	EXPECT_EQ(Entities().Get<const LastDrawnMatrix>(carried).cells, before);
	EXPECT_FALSE(Entities().AllOf<PhysicsDrawPose>(carried));
}

TEST_F(PhysicsFramePoseTest, ABodyAtRestLeavesTheMatrixAsItWas)
{
	const auto villager = FlyingVillager();
	ASSERT_TRUE(villager != entt::null);
	LetGo(villager);
	_dynamics->GetState().objects.back()->body.resting = true;
	const auto before = Entities().Get<const LastDrawnMatrix>(villager).cells;

	_dynamics->UpdateFrame(0.5f, 0.02f);

	EXPECT_EQ(Entities().Get<const LastDrawnMatrix>(villager).cells, before);
}
