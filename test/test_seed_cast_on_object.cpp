/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A seed cast on an object (Magic/Core/SpellSeed's CastOnObject, the hand's apply to an object): the spell goes on the
// object when the first seed of its magic type casts on objects, else at the object's map position, as a cast at that
// point would. The plain cast at a point is unchanged. Synthetic tables only: a creature miracle's seed row.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <utility>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Particles/SpellLink.h"
#include "support/RestoreService.h"

using namespace openblack;
using SpellComponent = openblack::ecs::components::Spell;
using SeedComponent = openblack::ecs::components::SpellSeed;
using CreatureSpellsComponent = openblack::ecs::components::CreatureSpells;

namespace
{
/// A fresh registry, spell list, animals' shared state and table of the game's constants whose only seed row is the "big"
/// creature miracle, cast by a gesture (not in the hand), each put back as it was when the test ends
class SeedCastOnObjectTest: public ::testing::Test
{
protected:
	SeedCastOnObjectTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// a fresh miracles' service, with an empty spell list and other stores
		Locator::magicSystem::emplace<ecs::systems::MagicSystem>();
		// the spell classes register the flock miracles' animals' dying with the animals' shared state
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		auto info = std::make_unique<InfoConstants>();
		auto& row = info->spellSeed.at(static_cast<size_t>(SpellSeedType::CreatureSpellBig));
		row.castType = SpellCastType::SpellCastHandGesture;
		row.magicTypes.fill(MagicType::CreatureSpellBig);
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	void SetCastOnObject(uint32_t castOnObject)
	{
		_info->spellSeed.at(static_cast<size_t>(SpellSeedType::CreatureSpellBig)).castOnObject = castOnObject;
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A loose seed of the big creature miracle, not in the local interface
	[[nodiscard]] static entt::entity Seed()
	{
		const auto seed = Registry().Create();
		auto& component = Registry().Assign<SeedComponent>(seed);
		component.seedType = SpellSeedType::CreatureSpellBig;
		// the tables have no miracle record to read its magic type from, so it is given as the last one cast
		component.lastMagic = MagicType::CreatureSpellBig;
		return seed;
	}

	/// A creature (all the miracle's receiver looks for) at a world point
	[[nodiscard]] static entt::entity CreatureAt(const glm::vec3& position)
	{
		const auto creature = Registry().Create();
		Registry().Assign<ecs::components::Transform>(creature, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Registry().Assign<ecs::components::Creature>(creature);
		return creature;
	}

	const psys::ProcessInfo _hand {};

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	const test::RestoreService<Locator::animalSystem> _restoreAnimals;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::magicSystem> _restoreMagic;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
};
} // namespace

TEST_F(SeedCastOnObjectTest, ASeedThatCastsOnObjectsPutsTheSpellOnTheObject)
{
	SetCastOnObject(1);
	const auto seed = Seed();
	const glm::vec3 where(120.5f, 0.0f, 340.25f);
	const auto creature = CreatureAt(where);

	entt::entity spell = entt::null;
	ASSERT_EQ(magic::seed::CastOnObject(seed, creature, &spell, 1.0f, _hand), 1);
	ASSERT_NE(spell, entt::entity {entt::null});

	// the creature miracle's own cast on an object: the creature takes it on and holds it, the miracle no longer runs
	// out by itself
	auto& registry = Registry();
	const auto& lookup = std::as_const(registry);
	ASSERT_TRUE(lookup.AllOf<CreatureSpellsComponent>(creature));
	EXPECT_EQ(registry.Get<CreatureSpellsComponent>(creature).spells[creature_spells::Spell::Big].miracle, spell);
	const auto& made = registry.Get<SpellComponent>(spell);
	EXPECT_EQ(made.magicType, MagicType::CreatureSpellBig);
	EXPECT_LT(made.duration, 0.0f);
	// made at the object's map position, and linked to the seed as any cast
	EXPECT_EQ(made.position, magic::ToMap(where));
	EXPECT_EQ(made.seed, seed);
	EXPECT_EQ(registry.Get<SeedComponent>(seed).spell, spell);
	EXPECT_TRUE(registry.Get<SeedComponent>(seed).hasCast);
}

TEST_F(SeedCastOnObjectTest, ASeedThatDoesNotCastOnObjectsCastsAtTheObjectsPosition)
{
	SetCastOnObject(0);
	const auto seed = Seed();
	const glm::vec3 where(120.5f, 0.0f, 340.25f);
	const auto creature = CreatureAt(where);

	entt::entity spell = entt::null;
	ASSERT_EQ(magic::seed::CastOnObject(seed, creature, &spell, 1.0f, _hand), 1);
	ASSERT_NE(spell, entt::entity {entt::null});

	// a plain cast at the object's map position: the creature takes nothing
	auto& registry = Registry();
	const auto& lookup = std::as_const(registry);
	EXPECT_FALSE(lookup.AllOf<CreatureSpellsComponent>(creature));
	const auto& made = registry.Get<SpellComponent>(spell);
	EXPECT_EQ(made.magicType, MagicType::CreatureSpellBig);
	EXPECT_EQ(made.position, magic::ToMap(where));
	EXPECT_EQ(made.castPos, magic::ToMap(where));
	EXPECT_EQ(made.seed, seed);

	// the same as a cast of another such seed at that point
	const auto other = Seed();
	entt::entity atPoint = entt::null;
	ASSERT_EQ(magic::seed::Cast(other, magic::ToMap(where), &atPoint, 1.0f, _hand), 1);
	const auto& pointSpell = registry.Get<SpellComponent>(atPoint);
	EXPECT_EQ(pointSpell.position, made.position);
	EXPECT_EQ(pointSpell.castPos, made.castPos);
	EXPECT_EQ(pointSpell.duration, made.duration);
	EXPECT_EQ(pointSpell.magicType, made.magicType);
	EXPECT_FALSE(lookup.AllOf<CreatureSpellsComponent>(creature));
}

TEST_F(SeedCastOnObjectTest, ACastAtAPointStillCastsThereWhateverTheSeedCastsOn)
{
	// the seed's cast at a point ignores castOnObject: the spell is made at the point, on nothing
	SetCastOnObject(1);
	const auto seed = Seed();
	const auto creature = CreatureAt(glm::vec3(10.0f, 0.0f, 10.0f));
	const glm::vec3 point(400.0f, 5.0f, 600.0f);

	entt::entity spell = entt::null;
	ASSERT_EQ(magic::seed::Cast(seed, point, &spell, 1.0f, _hand), 1);
	ASSERT_NE(spell, entt::entity {entt::null});

	auto& registry = Registry();
	const auto& lookup = std::as_const(registry);
	const auto& made = registry.Get<SpellComponent>(spell);
	EXPECT_EQ(made.magicType, MagicType::CreatureSpellBig);
	EXPECT_EQ(made.position, point);
	EXPECT_EQ(made.castPos, point);
	EXPECT_EQ(made.originalCastPos, point);
	EXPECT_EQ(made.seed, seed);
	EXPECT_EQ(registry.Get<SeedComponent>(seed).spell, spell);
	EXPECT_FALSE(lookup.AllOf<CreatureSpellsComponent>(creature));
}
