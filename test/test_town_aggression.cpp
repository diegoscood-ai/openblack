/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A town's memory of attacks (ecs::town_aggression): the rise, the wear of the multipliers, the fade into the wants of
// protection and mercy, the multipliers' recovery and the scripts' time since an attack. Pure rules, no game data.

#include <limits>

#include <gtest/gtest.h>

#include "ECS/TownAggression.h"
#include "Enums.h"

using namespace openblack;
namespace aggression = openblack::ecs::town_aggression;

namespace
{
constexpr float k_Epsilon = 1e-5f;

constexpr size_t Index(PlayerNames player)
{
	return static_cast<size_t>(player);
}
} // namespace

TEST(TownAggression, AnAttackAddsByItsMultiplierWhichWearsDown)
{
	aggression::Record record;
	aggression::Attacked(record, PlayerNames::PLAYER_TWO, false, 1.0f, 0.5f, 100);
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::PLAYER_TWO)), 1.5f);
	EXPECT_FLOAT_EQ(record.protectionMultiplier, 0.9f);
	EXPECT_EQ(record.lastAggressor, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(record.lastTurn, 100u);
	aggression::Attacked(record, PlayerNames::PLAYER_TWO, false, 1.0f, 0.5f, 101);
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::PLAYER_TWO)), 2.4f);
	// Its own player's attacks are its want of mercy
	aggression::Attacked(record, PlayerNames::PLAYER_ONE, true, 1.0f, 0.0f, 102);
	aggression::ProcessTurn(record, PlayerNames::PLAYER_ONE);
	EXPECT_NEAR(record.protection, 2.4f * 0.999f, k_Epsilon);
	EXPECT_NEAR(record.mercy, 0.999f, k_Epsilon);
	EXPECT_FLOAT_EQ(record.mercyMultiplier, 0.9f);
	// A multiplier recovers while its want is small, twice over as the game works it out
	aggression::Record calm;
	calm.protectionMultiplier = 0.5f;
	aggression::ProcessTurn(calm, PlayerNames::NEUTRAL);
	EXPECT_NEAR(calm.protectionMultiplier, 0.5f * 1.001f * 1.001f, 1e-6f);
}

TEST(TownAggression, OnlyTheFirstHarmOfAPlayerGetsTheAddition)
{
	aggression::Record record;
	// No harm at all: the first attack still counts its addition
	aggression::Attacked(record, PlayerNames::PLAYER_THREE, false, 0.0f, 0.25f, 7);
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::PLAYER_THREE)), 0.25f);
	EXPECT_EQ(record.lastTurns.at(Index(PlayerNames::PLAYER_THREE)), 7u);
	// The next one has none, and counts by the worn multiplier
	aggression::Attacked(record, PlayerNames::PLAYER_THREE, false, 1.0f, 0.25f, 9);
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::PLAYER_THREE)), 0.25f + 0.9f);
	EXPECT_FLOAT_EQ(record.protectionMultiplier, 0.9f * 0.9f);
	EXPECT_FLOAT_EQ(record.mercyMultiplier, 1.0f);
	EXPECT_EQ(record.lastTurn, 9u);
}

TEST(TownAggression, ItFadesEachTurnAndANeutralTownWantsOnlyProtection)
{
	aggression::Record record;
	aggression::Attacked(record, PlayerNames::NEUTRAL, true, 1.0f, 0.0f, 1);
	aggression::Attacked(record, PlayerNames::PLAYER_ONE, false, 1.0f, 0.0f, 1);
	aggression::ProcessTurn(record, PlayerNames::NEUTRAL);
	aggression::ProcessTurn(record, PlayerNames::NEUTRAL);
	const float faded = 1.0f * 0.999f * 0.999f;
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::NEUTRAL)), faded);
	EXPECT_FLOAT_EQ(record.aggression.at(Index(PlayerNames::PLAYER_ONE)), faded);
	// In a neutral town everyone's aggression, its own player's too, is protection
	EXPECT_FLOAT_EQ(record.protection, faded + faded);
	EXPECT_EQ(record.mercy, 0.0f);
	// A want of 0.1 or more keeps its multiplier worn; the other recovers to at most 1
	EXPECT_FLOAT_EQ(record.protectionMultiplier, 0.9f);
	EXPECT_FLOAT_EQ(record.mercyMultiplier, 0.9f * 1.001f * 1.001f * 1.001f * 1.001f);
	aggression::Record whole;
	aggression::ProcessTurn(whole, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(whole.protectionMultiplier, 1.0f);
	EXPECT_EQ(whole.mercyMultiplier, 1.0f);
}

TEST(TownAggression, TheScriptsAreToldTheSecondsSinceAnAttackStillHeld)
{
	aggression::Record record;
	constexpr float k_Never = std::numeric_limits<float>::max();
	// Never attacked by them: the largest float
	EXPECT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_ONE, 50, 100), k_Never);
	aggression::Attacked(record, PlayerNames::PLAYER_ONE, false, 1.0f, 0.0f, 20);
	// 30 turns of 100 ms
	EXPECT_FLOAT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_ONE, 50, 100), 3.0f);
	EXPECT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_TWO, 50, 100), k_Never);
	// It is held while above 0.15, compared as a double: 0.15f is just above it, a little less is as never attacked
	record.aggression.at(Index(PlayerNames::PLAYER_ONE)) = 0.15f;
	EXPECT_FLOAT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_ONE, 50, 100), 3.0f);
	record.aggression.at(Index(PlayerNames::PLAYER_ONE)) = 0.1499f;
	EXPECT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_ONE, 50, 100), k_Never);
	// No such player
	EXPECT_EQ(aggression::SecondsSinceAttacked(record, PlayerNames::_COUNT, 50, 100), k_Never);
}
