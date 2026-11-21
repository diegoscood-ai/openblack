/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature as a spell's caster: its body pays what the spell asks of it

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class CreatureSpellCasterTest: public ::testing::Test
{
protected:
	CreatureSpellCasterTest()
	{
		auto& row = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
		row.chantsPerEnergy = 36000.0f;
		row.spellEnergyFloor = 0.005f;
		row.spellSizeFactor = 1.0f;
		_world.Info().creatureMagicActionKnownAboutEntry.at(static_cast<size_t>(MagicType::Fireball)).field0x5c = 0.3f;
	}

	/// A creature of size 1 and strength 0.5, full of energy and not tired
	static entt::entity Caster()
	{
		auto& registry = test::creature_world::World::Registry();
		const auto creature = test::creature_world::World::MakeCreature();
		auto& body = registry.Get<Creature>(creature);
		body.size = 1.0f;
		body.strength = 0.5f;
		auto& needs = registry.Get<CreatureNeeds>(creature).needs;
		needs.energy = 1.0f;
		needs.exhaustion = 0.0f;
		return creature;
	}

	static entt::entity Miracle(MagicType type)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto spell = registry.Create();
		registry.Assign<Spell>(spell).magicType = type;
		return spell;
	}

	static SpellCreator CreatorOf(entt::entity creature)
	{
		return {.kind = SpellCreator::Kind::Creature, .player = PlayerNames::PLAYER_ONE, .entity = creature};
	}

	test::creature_world::World _world;
};

TEST_F(CreatureSpellCasterTest, ACreaturesBodyPaysForItsSpell)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Caster();
	const auto spell = Miracle(MagicType::Fireball);

	// 9000 chants over (1 x 1 + 0.5 + 1) x 36000 tire it by 0.1, its energy by 0.1 x the fireball's 0.3
	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(creature), spell, 9000.0f), 9000.0f);
	const auto& needs = registry.Get<CreatureNeeds>(creature).needs;
	EXPECT_NEAR(needs.exhaustion, 0.1f, 1e-6f);
	EXPECT_NEAR(needs.energy, 1.0f - (0.1f * 0.3f), 1e-6f);
}

TEST_F(CreatureSpellCasterTest, ItGivesNoMoreThanItHasAndANegativeAskIsReturned)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Caster();
	const auto spell = Miracle(MagicType::Fireball);

	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(creature), spell, -5.0f), -5.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.exhaustion, 0.0f);
	// the most it has: (1 + 0.5 + 1) x (1 - 0.005) x 36000
	const float most = 2.5f * (1.0f - 0.005f) * 36000.0f;
	EXPECT_NEAR(magic::creator::MaintainSpell(CreatorOf(creature), spell, 1e9f), most, 1.0f);
	registry.Get<CreatureNeeds>(creature).needs.energy = 0.004f;
	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(creature), spell, 100.0f), 0.0f);
}

TEST_F(CreatureSpellCasterTest, ACreatureWithNoBodyOrNoSpellPaysNothing)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Caster();
	const auto spell = Miracle(MagicType::Fireball);
	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(creature), entt::null, 100.0f), 0.0f);
	registry.Remove<CreatureNeeds>(creature);
	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(creature), spell, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(magic::creator::MaintainSpell(CreatorOf(entt::null), spell, 100.0f), 0.0f);
}
} // namespace
