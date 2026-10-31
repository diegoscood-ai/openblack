/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A tree a fire burns away goes through the tree's own deletion (fire::traits::DestroyedByEffect): what watches trees
// going is told, as the original's tree deletion tells it, and the tree is gone.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class BurntTree: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(std::make_unique<test::FakeDynamicsWorld>(registry));
	}
	void TearDown() override
	{
		Locator::dynamicsSystem::reset();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(BurntTree, ATreeBurntAwayGoesThroughTheTreesOwnDeletion)
{
	std::vector<std::pair<entt::entity, ecs::TreeDeletion>> told;
	ecs::AddTreeDeletedListener([&told](entt::entity tree, ecs::TreeDeletion how) { told.emplace_back(tree, how); });
	const auto tree = Reg().Create();
	Reg().Assign<Transform>(tree, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f});
	ecs::fire::traits::DestroyedByEffect(tree);
	ASSERT_EQ(told.size(), 1u);
	EXPECT_EQ(told.front().first, tree);
	EXPECT_EQ(told.front().second, ecs::TreeDeletion::Removed);
	EXPECT_FALSE(Reg().Valid(tree));
}
