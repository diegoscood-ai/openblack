/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics list as the original keeps it (docs/wiki/physics/physics.md#manager): an entry leaves it by the last one moving
// into its place, and an entry whose object went during a turn stays listed, as no object's, until the next turn start.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Unavailable.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Locator.h"
#include "Physics/TurnRules.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The entities of the physics list, in its order
std::vector<entt::entity> Listed(ecs::systems::DynamicsSystem& dynamics)
{
	std::vector<entt::entity> listed;
	for (const auto& entry : dynamics.GetState().objects)
	{
		listed.push_back(entry->entity);
	}
	return listed;
}
} // namespace

TEST(PhysicsList, TheLastEntryMovesIntoTheGap)
{
	std::vector<char> list {'A', 'B', 'C', 'D'};
	physics::turn::RemoveByMovingLast(list, 1);
	EXPECT_EQ(list, (std::vector<char> {'A', 'D', 'C'}));
	physics::turn::RemoveByMovingLast(list, 2);
	EXPECT_EQ(list, (std::vector<char> {'A', 'D'}));
	physics::turn::RemoveByMovingLast(list, 0);
	EXPECT_EQ(list, (std::vector<char> {'D'}));
	physics::turn::RemoveByMovingLast(list, 0);
	EXPECT_TRUE(list.empty());
}

TEST(PhysicsList, AWalkThatTakesEntriesOutLooksAgainAtTheOneThatMovedIn)
{
	// As the turn start walks the list: an entry that goes is replaced by the last, which is looked at in its turn.
	// Taking A and D out of A B C D E leaves E B C, where erasing would leave B C E
	std::vector<char> list {'A', 'B', 'C', 'D', 'E'};
	const auto gone = [](char c) { return c == 'A' || c == 'D'; };
	std::vector<char> looked;
	for (size_t i = 0; i < list.size();)
	{
		looked.push_back(list[i]);
		if (gone(list[i]))
		{
			physics::turn::RemoveByMovingLast(list, i);
			continue;
		}
		++i;
	}
	EXPECT_EQ(list, (std::vector<char> {'E', 'B', 'C'}));
	EXPECT_EQ(looked, (std::vector<char> {'A', 'E', 'B', 'C', 'D'}));
}

TEST(PhysicsList, RemoveObjectMovesTheLastEntryIntoItsPlace)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	test::QuietBuildingDamage buildings;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(registry, nullptr, &buildings));
	std::vector<entt::entity> things;
	for (int i = 0; i < 4; ++i)
	{
		const auto thing = registry.Create();
		// marked for deletion, so that taking it out of the physics puts nothing back in the map's cells
		registry.Assign<ecs::components::Unavailable>(thing);
		auto entry = std::make_unique<ecs::PhysicsEntry>();
		entry->entity = thing;
		dynamics.GetState().objects.push_back(std::move(entry));
		things.push_back(thing);
	}
	dynamics.RemoveObject(things[1]);
	EXPECT_EQ(Listed(dynamics), (std::vector<entt::entity> {things[0], things[3], things[2]}));
	dynamics.RemoveObject(things[2]);
	EXPECT_EQ(Listed(dynamics), (std::vector<entt::entity> {things[0], things[3]}));
}

TEST(PhysicsList, AnEntryWhoseObjectIsGoneIsNoObjectsWhileItWaitsForTheTurnStart)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	test::QuietBuildingDamage buildings;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(registry, nullptr, &buildings));
	const auto kept = registry.Create();
	const auto gone = registry.Create();
	for (const auto thing : {kept, gone})
	{
		auto entry = std::make_unique<ecs::PhysicsEntry>();
		entry->entity = thing;
		dynamics.GetState().objects.push_back(std::move(entry));
	}
	// deleted during a turn: its object is destroyed at once, its entry stays listed
	registry.Destroy(gone);
	ASSERT_EQ(dynamics.GetState().objects.size(), 2u);
	EXPECT_NE(dynamics.Find(kept), nullptr);
	EXPECT_EQ(dynamics.Find(gone), nullptr);
	EXPECT_FALSE(dynamics.IsFlying(gone));
	std::vector<entt::entity> seen;
	dynamics.ForEachEntry([&seen](const ecs::PhysicsEntry& entry) { seen.push_back(entry.entity); });
	EXPECT_EQ(seen, (std::vector<entt::entity> {kept}));
}
