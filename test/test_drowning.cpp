/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a script's DROWNING asks of a thing that is not a villager (ecs::IsDrowning): that it is in the physics, has a
// body there and the body's centre is under the sea's level, as the original asks it.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Villager.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class DrowningTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_dynamics = &static_cast<ecs::systems::DynamicsSystem&>(Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
		    std::make_unique<test::FakeDynamicsWorld>(registry)));
	}
	void TearDown() override
	{
		Locator::dynamicsSystem::reset();
		Locator::entitiesRegistry::reset();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A thing listed in the physics with its body's centre at a height
	entt::entity Listed(float height)
	{
		const auto entity = Reg().Create();
		auto entry = std::make_unique<ecs::PhysicsEntry>();
		entry->entity = entity;
		entry->body.SetUpPos(glm::mat3(1.0f), glm::vec3(10.0f, height, 20.0f));
		_dynamics->GetState().objects.push_back(std::move(entry));
		return entity;
	}

private:
	ecs::systems::DynamicsSystem* _dynamics {nullptr};
};
} // namespace

TEST_F(DrowningTest, AThingDrownsOnlyInThePhysicsWithItsBodyUnderTheSea)
{
	// in the physics, its centre under 0: drowning; at 0 or above: not
	const auto under = Listed(-0.1f);
	Reg().Assign<InPhysics>(under);
	EXPECT_TRUE(ecs::IsDrowning(under));
	const auto atTheLevel = Listed(0.0f);
	Reg().Assign<InPhysics>(atTheLevel);
	EXPECT_FALSE(ecs::IsDrowning(atTheLevel));
	// a body at rest under the sea that only waits to be hit is not in the physics: not drowning
	const auto resting = Listed(-5.0f);
	EXPECT_FALSE(ecs::IsDrowning(resting));
	// no body, or no thing: not drowning
	const auto unlisted = Reg().Create();
	Reg().Assign<InPhysics>(unlisted);
	EXPECT_FALSE(ecs::IsDrowning(unlisted));
	EXPECT_FALSE(ecs::IsDrowning(entt::null));
}

TEST_F(DrowningTest, AVillagerDrownsOnlyInItsDrowningState)
{
	// a villager answers by its top state alone, wherever its body is
	const auto villager = Listed(-5.0f);
	Reg().Assign<InPhysics>(villager);
	Reg().Assign<Villager>(villager);
	auto& action = Reg().Assign<LivingAction>(villager, VillagerStates::Drowning, uint16_t {0});
	EXPECT_TRUE(ecs::IsDrowning(villager));
	// its final state drowning is not enough, nor a body under the sea
	action.states[static_cast<size_t>(LivingAction::Index::Top)] = static_cast<uint8_t>(VillagerStates::MoveToPos);
	action.states[static_cast<size_t>(LivingAction::Index::Final)] = static_cast<uint8_t>(VillagerStates::Drowning);
	EXPECT_FALSE(ecs::IsDrowning(villager));
}
