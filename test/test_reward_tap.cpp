/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reward system's tap on a chest, without the game's data: with no reward table a chest holds nothing, so only
// the opening itself and the chest's time are seen

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Reward.h"
#include "ECS/Registry.h"
#include "ECS/RewardOpening.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class RewardTap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	entt::entity Chest(bool fromSky)
	{
		return _rewards.Create(glm::vec3(0.0f), RewardObjectInfo::SmallFood, PlayerNames::PLAYER_ONE, entt::null, fromSky);
	}
	static const Reward& Of(entt::entity chest) { return Locator::entitiesRegistry::value().Get<Reward>(chest); }

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	ecs::systems::RewardSystem _rewards;
};
} // namespace

TEST_F(RewardTap, OnlyItsOwnPlayerOpensAChestOnTheLandAndOnlyOnce)
{
	const auto chest = Chest(false);
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Opened);
	EXPECT_EQ(Of(chest).turnsLeft, ecs::reward::k_OpenedTurns);
	// The closed chest gave way to the opened one
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<Mesh>(chest).id, entt::hashed_string("coffre").value());
	// A second tap does nothing
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_rewards.IsOpened(chest));
}

TEST_F(RewardTap, AChestStillFallingTakesTheTapButStaysShut)
{
	const auto chest = Chest(true);
	EXPECT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Falling);
}

TEST_F(RewardTap, AChestWithNoPlayerOpensForNoOne)
{
	// So what an opening chest gives always goes to the chest's own player
	const auto chest = _rewards.Create(glm::vec3(0.0f), RewardObjectInfo::SmallFood, std::nullopt, entt::null, false);
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::NEUTRAL));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Landed);
}

TEST_F(RewardTap, AnOpenedChestGoesTwoHundredTurnsLater)
{
	// Its deletion reaches the dead list and the map cells
	const test::ScopedWorldSystems worldSystems;
	const auto chest = Chest(false);
	ASSERT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	for (int turn = 1; turn < ecs::reward::k_OpenedTurns; ++turn)
	{
		_rewards.ProcessTurn();
	}
	EXPECT_TRUE(ecs::IsAvailable(chest));
	_rewards.ProcessTurn();
	EXPECT_FALSE(ecs::IsAvailable(chest));
}
