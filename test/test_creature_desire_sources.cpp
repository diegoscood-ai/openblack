/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDesireSources.h"

using namespace openblack;
using namespace openblack::creature_desire_sources;

TEST(CreatureDesireSources, TheHandsMovementAndWhatItHoldsInterestTheCreature)
{
	PlayerInterest interest;
	// The first turn only notes where the hand is
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {0.0f}, std::nullopt), 0.0f);
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {30.0f, 0.0f, 0.0f}, std::nullopt), 30.0f * 0.98f / 60.0f);
	// A still hand's movement fades
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {30.0f, 0.0f, 0.0f}, std::nullopt), 30.0f * 0.98f * 0.98f / 60.0f);
	// The movement counts up to sixty
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {1000.0f, 0.0f, 0.0f}, std::nullopt), 0.98f);
	// What it holds counts up to a radius of thirty
	PlayerInterest held {.lastHand = glm::vec3 {0.0f}};
	EXPECT_FLOAT_EQ(InterestNow(held, glm::vec3 {0.0f}, 15.0f), 0.5f);
	EXPECT_FLOAT_EQ(InterestNow(held, glm::vec3 {0.0f}, 100.0f), 1.0f);
	// Without a player there is nothing of interest
	EXPECT_FLOAT_EQ(InterestNow(held, std::nullopt, 15.0f), 0.0f);
}

TEST(CreatureDesireSources, InterestRisesAtOnceAndHalvesTowardsLess)
{
	PlayerInterest interest;
	EXPECT_FLOAT_EQ(interest.value, 1.0f);
	UpdateInterest(interest, 0.0f);
	EXPECT_FLOAT_EQ(interest.value, 0.5f);
	UpdateInterest(interest, 0.0f);
	EXPECT_FLOAT_EQ(interest.value, 0.25f);
	UpdateInterest(interest, 0.8f);
	EXPECT_FLOAT_EQ(interest.value, 0.8f);
}
