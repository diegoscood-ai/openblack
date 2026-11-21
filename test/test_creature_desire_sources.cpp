/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
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

// The distance the hand moved adds its squares from z to x, as the original's: adding them from x gives a movement one
// step shorter here
TEST(CreatureDesireSources, TheHandsMovementAddsItsSquaresFromZToX)
{
	PlayerInterest interest {.lastHand = glm::vec3 {std::bit_cast<float>(0x44D20CCDu), std::bit_cast<float>(0x422B0974u),
	                                                std::bit_cast<float>(0x44A444B6u)}};
	const glm::vec3 hand {std::bit_cast<float>(0x44D27A52u), std::bit_cast<float>(0x42238BE3u),
	                      std::bit_cast<float>(0x44A3446Au)};
	static_cast<void>(InterestNow(interest, hand, std::nullopt));
	EXPECT_EQ(std::bit_cast<uint32_t>(interest.handMovement), 0x410BB113u);
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
