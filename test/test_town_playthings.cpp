/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Ball.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/TownPlaythings.h"

using namespace openblack;
using namespace openblack::ecs::town_playthings;

TEST(TownPlaythings, ATownKeepsOneFootballWhileItIsAbout)
{
	std::vector<entt::entity> playthings;
	const auto first = static_cast<entt::entity>(1);
	const auto second = static_cast<entt::entity>(2);
	bool firstAbout = true;
	const auto about = [&](entt::entity thing) { return thing == first && firstAbout; };
	EXPECT_TRUE(Add(playthings, first, about));
	EXPECT_FALSE(Add(playthings, second, about));
	firstAbout = false;
	EXPECT_TRUE(Add(playthings, second, about));
	// The newest comes first
	EXPECT_EQ(playthings, (std::vector {second, first}));
}

TEST(TownPlaythings, TheNearestTownTakesItThePlayersBeforeTheNeutral)
{
	const std::array towns {
	    Candidate {
	        .town = static_cast<entt::entity>(10), .owner = PlayerNames::NEUTRAL, .id = 0, .position = {100.0f, 0.0f, 0.0f}},
	    Candidate {
	        .town = static_cast<entt::entity>(11), .owner = PlayerNames::PLAYER_TWO, .id = 5, .position = {0.0f, 0.0f, 100.0f}},
	    Candidate {
	        .town = static_cast<entt::entity>(12), .owner = PlayerNames::PLAYER_ONE, .id = 9, .position = {400.0f, 0.0f, 0.0f}},
	};
	// Equally far, a player's town is looked at first and kept
	EXPECT_EQ(Nearest(towns, {0.0f, 0.0f, 0.0f}), static_cast<entt::entity>(11));
	EXPECT_EQ(Nearest(towns, {390.0f, 0.0f, 0.0f}), static_cast<entt::entity>(12));
	EXPECT_FALSE(Nearest({}, {0.0f, 0.0f, 0.0f}).has_value());
}

TEST(TownPlaythings, ATownGivenABallKeepsTheOneItHasUntilThatOneIsToBeDeleted)
{
	ecs::Registry registry;
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	const auto first = registry.Create();
	registry.Assign<ecs::components::Ball>(first);
	const auto second = registry.Create();
	registry.Assign<ecs::components::Ball>(second);

	EXPECT_TRUE(GiveBall(registry, town, first));
	EXPECT_FALSE(GiveBall(registry, town, second));
	EXPECT_EQ(registry.Get<ecs::components::Town>(town).playthings, (std::vector {first}));
	// The first is to be deleted: no longer about, so the town takes the second, which comes first
	registry.Assign<ecs::components::Unavailable>(first);
	EXPECT_TRUE(GiveBall(registry, town, second));
	EXPECT_EQ(registry.Get<ecs::components::Town>(town).playthings, (std::vector {second, first}));
}

TEST(TownPlaythings, AGoneBallOrAThingOfAnotherKindDoesNotKeepTheTownsPlace)
{
	ecs::Registry registry;
	const auto town = registry.Create();
	auto& playthings = registry.Assign<ecs::components::Town>(town).playthings;
	// A thing that is not a ball, and a ball since gone
	const auto other = registry.Create();
	const auto gone = registry.Create();
	registry.Assign<ecs::components::Ball>(gone);
	playthings = {other, gone};
	registry.Destroy(gone);
	const auto ball = registry.Create();
	registry.Assign<ecs::components::Ball>(ball);

	EXPECT_TRUE(GiveBall(registry, town, ball));
	EXPECT_EQ(registry.Get<ecs::components::Town>(town).playthings.front(), ball);
}
