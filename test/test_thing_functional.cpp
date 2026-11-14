/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules by which a thing still works (ECS/ThingFunctional.h), on fake facts.

#include <limits>

#include <gtest/gtest.h>

#include "ECS/ThingFunctional.h"

using namespace openblack::ecs::thing_functional;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
}

TEST(ThingFunctional, NothingUnavailableWorks)
{
	for (const auto kind : {Kind::Plain, Kind::Living, Kind::Villager, Kind::MultiMapFixed})
	{
		EXPECT_FALSE(IsFunctional({.kind = kind, .available = false}));
	}
	EXPECT_TRUE(IsFunctional({.kind = Kind::Plain, .available = true, .life = 0.0f, .built = false}));
}

TEST(ThingFunctional, ALivingThingWorksWhileItsLifeIsNotZero)
{
	EXPECT_TRUE(IsFunctional({.kind = Kind::Living, .available = true, .life = 0.01f}));
	EXPECT_TRUE(IsFunctional({.kind = Kind::Living, .available = true, .life = -0.5f}));
	EXPECT_FALSE(IsFunctional({.kind = Kind::Living, .available = true, .life = 0.0f}));
	// a life that is not a number counts as none
	EXPECT_FALSE(IsFunctional({.kind = Kind::Living, .available = true, .life = k_NaN}));
}

TEST(ThingFunctional, AVillagerStopsInTheTwoTopStatesOfItsDeath)
{
	EXPECT_TRUE(IsFunctional({.kind = Kind::Villager, .available = true, .villagerTop = 12}));
	EXPECT_FALSE(IsFunctional({.kind = Kind::Villager, .available = true, .villagerTop = 13}));
	EXPECT_FALSE(IsFunctional({.kind = Kind::Villager, .available = true, .villagerTop = 14}));
	EXPECT_TRUE(IsFunctional({.kind = Kind::Villager, .available = true, .villagerTop = 15}));
}

TEST(ThingFunctional, AFixedObjectWorksBuiltAndAboveItsLifeForWorking)
{
	const Facts working {
	    .kind = Kind::MultiMapFixed, .available = true, .life = 1.0f, .built = true, .nonFunctionalBelow = 0.75f};
	EXPECT_TRUE(IsFunctional(working));
	auto facts = working;
	facts.built = false;
	EXPECT_FALSE(IsFunctional(facts));
	facts = working;
	facts.life = 0.75f;
	EXPECT_FALSE(IsFunctional(facts));
	facts.life = 0.76f;
	EXPECT_TRUE(IsFunctional(facts));
	// a life that is not a number is not at or below the mark
	facts.life = k_NaN;
	EXPECT_TRUE(IsFunctional(facts));
}
