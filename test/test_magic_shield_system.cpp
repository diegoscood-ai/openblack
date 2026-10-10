/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shields' service (ECS/Systems/Implementations/MagicShieldSystem): it holds no state, and hands a new land and
// the reaction check on to the shields under src/Magic, whose lists are the magic objects' and the spells'.

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/MagicObjectsSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
class MagicShieldSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the locator only hands the lists to the shields under src/Magic; the test reads the lists of the miracles'
		// service it made, which owns them
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_magic = &Locator::magicSystem::emplace<ecs::systems::MagicSystem>();
	}
	void TearDown() override
	{
		// the listener resets the miracles' service
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] ecs::Registry& Registry() const { return *_registry; }
	[[nodiscard]] std::vector<entt::entity>& ShieldObjects() const { return _magic->MagicObjects().Shields(); }
	[[nodiscard]] std::vector<entt::entity>& ShieldSpells() const { return _magic->SpellStore().ShieldSpells(); }

	ecs::systems::MagicShieldSystem _system;

private:
	ecs::Registry* _registry {nullptr};
	ecs::systems::MagicSystemInterface* _magic {nullptr};
};
} // namespace

TEST_F(MagicShieldSystemTest, ResetEmptiesTheShieldObjectsAndTheShieldSpells)
{
	// a new land: both lists go, the entities stay for the registry's reset
	const auto object = Registry().Create();
	const auto spell = Registry().Create();
	ShieldObjects().push_back(object);
	ShieldSpells().push_back(spell);
	_system.Reset();
	EXPECT_TRUE(ShieldObjects().empty());
	EXPECT_TRUE(ShieldSpells().empty());
	EXPECT_TRUE(Registry().Valid(object));
	EXPECT_TRUE(Registry().Valid(spell));
}

TEST_F(MagicShieldSystemTest, NoShieldKeepsNoReactionOff)
{
	EXPECT_FALSE(_system.KeepsReactionOff(glm::vec3(10.0f, 0.0f, 10.0f), glm::vec3(500.0f, 0.0f, 500.0f)));
}

TEST_F(MagicShieldSystemTest, AShieldThatHasGoneKeepsNoReactionOff)
{
	// a shield destroyed while still listed is passed over
	const auto object = Registry().Create();
	ShieldObjects().push_back(object);
	Registry().Destroy(object);
	EXPECT_FALSE(_system.KeepsReactionOff(glm::vec3(10.0f, 0.0f, 10.0f), glm::vec3(500.0f, 0.0f, 500.0f)));
}

TEST_F(MagicShieldSystemTest, ATurnPassesOverAShieldThatHasGone)
{
	const auto object = Registry().Create();
	ShieldObjects().push_back(object);
	Registry().Destroy(object);
	_system.ProcessTurn();
	// the list is the shields', not the turn's: it is left as it was
	ASSERT_EQ(ShieldObjects().size(), 1u);
	EXPECT_EQ(ShieldObjects().front(), object);
}
