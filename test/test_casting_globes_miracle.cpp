/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fakes of the miracles own the real stores of the miracles' service
#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
using openblack::ecs::components::SpellSeed;

namespace
{
/// A seed and the miracle cast from it over the tests' fake world: a registry, zeroed tables, the animals' shared state
/// the spell classes register with, and the inert miracles' service, each put back as it was when the test ends
class CastingSeeds: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		auto info = std::make_unique<InfoConstants>();
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] InfoConstants& Info() const { return *_info; }

	/// A miracle the first player cast at (100, 0, 100), with its prayer power and age
	[[nodiscard]] static entt::entity MakeSpell(MagicType type, float prayerPower, float age)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = type;
		component.spellClass = ClassOf(type);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		chants::SetChants(component, prayerPower);
		component.age = age;
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		return spell;
	}

	/// A seed of that type linked both ways with the miracle cast from it
	[[nodiscard]] static entt::entity MakeSeed(SpellSeedType type, entt::entity spell)
	{
		const auto seed = Registry().Create();
		auto& component = Registry().Assign<SpellSeed>(seed);
		component.seedType = type;
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.spell = spell;
		Registry().Get<Spell>(spell).seed = seed;
		return seed;
	}

	/// The wood miracle's tables as the game's: 500 the first time at 3 prayer power each
	void GameWoodTables() const
	{
		for (auto& row : Info().magicWood)
		{
			row.resourceAmountFirstEvent = 500;
			row.resourceAmountPerEvent = 20;
			row.costPerUnit = 3;
		}
	}

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
};
} // namespace

// Letting go of a held miracle: the seed keeps the miracle's prayer power and age, the miracle closes and the seed is
// free to be pressed again, as long as the power left is enough for another cast (the wood: 3 x 500 = 1500)
TEST_F(CastingSeeds, LettingGoStoresThePowerAndAgeLeftInTheSeedWhileTheyAreEnoughToCastAgain)
{
	GameWoodTables();
	const auto spell = MakeSpell(MagicType::Wood, 1500.0f, 4.5f);
	const auto seed = MakeSeed(SpellSeedType::Wood, spell);

	seed::ApplyUnlockProcess(seed);

	ASSERT_TRUE(Registry().Valid(seed));
	const auto& component = Registry().Get<const SpellSeed>(seed);
	EXPECT_FLOAT_EQ(component.storedChants, 1500.0f);
	EXPECT_FLOAT_EQ(component.chantStore, 1500.0f);
	EXPECT_FLOAT_EQ(component.storedAge, 4.5f);
	EXPECT_EQ(component.spell, entt::entity {entt::null});
	EXPECT_FALSE(component.hasCast);
	EXPECT_TRUE(Registry().Get<const Spell>(spell).closedDown);
	EXPECT_EQ(Registry().Get<const Spell>(spell).seed, entt::entity {entt::null});
}

TEST_F(CastingSeeds, LettingGoWithTooLittlePowerLeftToCastAgainDeletesTheSeed)
{
	GameWoodTables();
	const auto spell = MakeSpell(MagicType::Wood, 1499.0f, 4.5f);
	const auto seed = MakeSeed(SpellSeedType::Wood, spell);

	seed::ApplyUnlockProcess(seed);

	EXPECT_FALSE(Registry().Valid(seed));
	EXPECT_TRUE(Registry().Get<const Spell>(spell).closedDown);
}

// A plain miracle (the heal) always has enough to be cast again; a seed whose info deletes it once cast goes
TEST_F(CastingSeeds, AHealSeedIsKeptButASeedDeletedOnceCastGoesWhenLetGo)
{
	const auto heal = MakeSpell(MagicType::Heal, 10.0f, 1.0f);
	const auto kept = MakeSeed(SpellSeedType::Heal, heal);
	seed::ApplyUnlockProcess(kept);
	ASSERT_TRUE(Registry().Valid(kept));
	EXPECT_FLOAT_EQ(Registry().Get<const SpellSeed>(kept).storedChants, 10.0f);

	Info().spellSeed.at(static_cast<size_t>(SpellSeedType::Water)).deleteSeedOnceCast = 1;
	const auto water = MakeSpell(MagicType::Heal, 10.0f, 1.0f);
	const auto gone = MakeSeed(SpellSeedType::Water, water);
	seed::ApplyUnlockProcess(gone);
	EXPECT_FALSE(Registry().Valid(gone));
}

// Held in the hand, a seed whose miracle has closed is deleted at its next turn; one whose miracle runs is kept
TEST_F(CastingSeeds, ASeedInTheHandIsDeletedOnceItsMiracleHasClosed)
{
	const auto spell = MakeSpell(MagicType::Heal, 10.0f, 1.0f);
	const auto seed = MakeSeed(SpellSeedType::Heal, spell);

	seed::ProcessInHand(seed);
	ASSERT_TRUE(Registry().Valid(seed));

	Registry().Get<Spell>(spell).closedDown = true;
	seed::ProcessInHand(seed);
	EXPECT_FALSE(Registry().Valid(seed));
}

// The miracle's deletion deletes the seed linked to it
TEST_F(CastingSeeds, DeletingAMiracleDeletesItsSeed)
{
	const auto spell = MakeSpell(MagicType::Heal, 10.0f, 1.0f);
	const auto seed = MakeSeed(SpellSeedType::Heal, spell);

	DeleteSpell(spell);

	EXPECT_FALSE(Registry().Valid(spell));
	EXPECT_FALSE(Registry().Valid(seed));
}
