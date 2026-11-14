/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shields' service (ECS/Systems/Implementations/MagicShieldSystem): it numbers the shield objects, which orders
// their list, and hands a new land and the reaction check on to the shields under src/Magic, whose miracles' list is
// the spells'.

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/MagicShield.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "Locator.h"
#include "Magic/Objects/MapShield.h"

using namespace openblack;

namespace
{
class MagicShieldSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the locator only hands the spells' list to the shields under src/Magic; the test reads the list of the
		// miracles' service it made, which owns it
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_magic = &Locator::magicSystem::emplace<ecs::systems::MagicSystem>();
	}
	void TearDown() override
	{
		// the listener resets the miracles' service
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] ecs::Registry& Registry() const { return *_registry; }
	[[nodiscard]] std::vector<entt::entity>& ShieldSpells() const { return _magic->SpellStore().ShieldSpells(); }
	/// A shield object with the service's next serial
	entt::entity MakeShield()
	{
		const auto object = Registry().Create();
		Registry().Assign<ecs::components::MagicShield>(object).serial = _system.NextShieldSerial();
		return object;
	}

	ecs::systems::MagicShieldSystem _system;

private:
	ecs::Registry* _registry {nullptr};
	ecs::systems::MagicSystemInterface* _magic {nullptr};
};
} // namespace

TEST_F(MagicShieldSystemTest, ResetEmptiesTheShieldSpellsAndStartsTheSerialsAgain)
{
	// a new land: the spells' list goes and the serials start from 0, the entities stay for the registry's reset
	const auto object = MakeShield();
	EXPECT_EQ(_system.NextShieldSerial(), 1u);
	const auto spell = Registry().Create();
	ShieldSpells().push_back(spell);
	_system.Reset();
	EXPECT_TRUE(ShieldSpells().empty());
	EXPECT_EQ(_system.NextShieldSerial(), 0u);
	EXPECT_TRUE(Registry().Valid(object));
	EXPECT_TRUE(Registry().Valid(spell));
}

TEST_F(MagicShieldSystemTest, TheShieldsAreListedNewestFirst)
{
	// one gone leaves the others in their order
	const auto first = MakeShield();
	const auto second = MakeShield();
	const auto third = MakeShield();
	const auto fourth = MakeShield();
	EXPECT_EQ(magic::map_shield::Shields(), (std::vector {fourth, third, second, first}));
	Registry().Destroy(second);
	EXPECT_EQ(magic::map_shield::Shields(), (std::vector {fourth, third, first}));
}

TEST_F(MagicShieldSystemTest, NoShieldKeepsNoReactionOff)
{
	EXPECT_FALSE(_system.KeepsReactionOff(glm::vec3(10.0f, 0.0f, 10.0f), glm::vec3(500.0f, 0.0f, 500.0f)));
}

TEST_F(MagicShieldSystemTest, AShieldThatHasGoneKeepsNoReactionOff)
{
	// a shield destroyed is passed over
	Registry().Destroy(MakeShield());
	EXPECT_FALSE(_system.KeepsReactionOff(glm::vec3(10.0f, 0.0f, 10.0f), glm::vec3(500.0f, 0.0f, 500.0f)));
}

TEST_F(MagicShieldSystemTest, ATurnPassesOverAShieldThatHasGone)
{
	const auto gone = MakeShield();
	const auto standing = MakeShield();
	Registry().Destroy(gone);
	_system.ProcessTurn();
	// the spiritual shield's object does nothing in the turn: it is still the one shield
	EXPECT_EQ(magic::map_shield::Shields(), std::vector {standing});
}
