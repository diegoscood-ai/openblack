/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a Living takes from a physics impact (living::ImpactDamage): the info's physics impact effect times
// (G - 2) x 0.03, its positive crush and hit times the Living's own defence multipliers. A hand-made info.dat, no
// game data.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/LivingPhysics.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace living = openblack::ecs::living;

namespace
{
constexpr auto k_Animal = static_cast<AnimalInfo>(0);

class LivingImpactTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	/// The physics impact effect with this crush, hit and heal; the animal's crush and hit multipliers
	static void SetInfo(float crush, float hit, float heal, float crushDefence, float hitDefence)
	{
		auto info = std::make_unique<InfoConstants>();
		auto& effect = info->effect.at(3);
		effect.effectCrush = crush;
		effect.effectHit = hit;
		effect.effectHeal = heal;
		auto& animal = info->animal.at(static_cast<size_t>(k_Animal));
		animal.defenceMultiplierBurn = 1.0f;
		animal.defenceMultiplierCrush = crushDefence;
		animal.defenceMultiplierHit = hitDefence;
		Locator::infoConstants::reset(info.release());
	}

	static entt::entity MakeAnimal()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto animal = registry.Create();
		registry.Assign<Animal>(animal, Animal {.type = k_Animal});
		return animal;
	}
};
} // namespace

TEST_F(LivingImpactTest, TheCrushCountsByTheAnimalsOwnMultiplier)
{
	SetInfo(1.0f, 0.0f, 0.0f, 2.0f, 4.0f);
	const auto animal = MakeAnimal();
	const auto damage = living::ImpactDamage(animal, 4.0f);
	ASSERT_TRUE(damage.has_value());
	// (4 - 2) x 0.03 = 0.06, x 1 crush, x 2
	const float scale = (4.0f - 2.0f) * 0.03f;
	EXPECT_EQ(*damage, scale * 2.0f);
}

TEST_F(LivingImpactTest, AHitInTheEffectCountsTooANegativeOneNot)
{
	SetInfo(1.0f, 0.5f, 0.0f, 2.0f, 4.0f);
	const auto animal = MakeAnimal();
	const float scale = (3.0f - 2.0f) * 0.03f;
	const auto damage = living::ImpactDamage(animal, 3.0f);
	ASSERT_TRUE(damage.has_value());
	EXPECT_FLOAT_EQ(*damage, scale * 2.0f + scale * 0.5f * 4.0f);
	SetInfo(1.0f, -0.5f, 0.0f, 2.0f, 4.0f);
	EXPECT_FLOAT_EQ(*living::ImpactDamage(animal, 3.0f), scale * 2.0f);
}

TEST_F(LivingImpactTest, WithoutTheInfoTheCrushIsOne)
{
	const auto animal = MakeAnimal();
	const auto damage = living::ImpactDamage(animal, 2.5f);
	ASSERT_TRUE(damage.has_value());
	EXPECT_EQ(*damage, (2.5f - 2.0f) * 0.03f);
}
