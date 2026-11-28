/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::creation_draws (ECS/CreationDraws.h): the draws an object's creation takes from the C runtime's random numbers,
// at once in play and, while a saved game's load makes its objects class by class, held back and taken at the load's
// end in the order the objects' reads ended. The draws here come from a fake source on the C runtime's formula with a
// seed of the test's own.

#include <cstdint>

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/CreationDraws.h"

namespace draws = openblack::ecs::creation_draws;
namespace crt = openblack::game_random::crt;

namespace
{
constexpr uint32_t k_Seed = 12345;

/// A fake random source: rand() on the test's seed
struct FakeRandom
{
	uint32_t seed {k_Seed};
	int32_t Next() { return crt::Next(seed); }
};

/// What each owner drew, in its draws' order
using Drawn = std::unordered_map<entt::entity, std::vector<int32_t>>;

auto DrawFor(FakeRandom& random, Drawn& drawn, entt::entity owner, int count)
{
	return [&random, &drawn, owner, count]() {
		for (int i = 0; i < count; ++i)
		{
			drawn[owner].push_back(random.Next());
		}
	};
}

std::vector<int32_t> Sequence(size_t count)
{
	FakeRandom random;
	std::vector<int32_t> values;
	for (size_t i = 0; i < count; ++i)
	{
		values.push_back(random.Next());
	}
	return values;
}

auto RankOf(std::unordered_map<entt::entity, size_t> ranks)
{
	return [ranks = std::move(ranks)](entt::entity owner) -> std::optional<size_t> {
		const auto found = ranks.find(owner);
		return found != ranks.end() ? std::optional(found->second) : std::nullopt;
	};
}

bool Alive(entt::entity /*owner*/)
{
	return true;
}

constexpr auto k_Mist = entt::entity {1};
constexpr auto k_Heart = entt::entity {2};
constexpr auto k_Lantern = entt::entity {3};
} // namespace

TEST(CreationDraws, InPlayEachDrawIsTakenAsItsObjectIsMade)
{
	draws::Queue queue;
	FakeRandom random;
	Drawn drawn;
	draws::Run(queue, k_Mist, DrawFor(random, drawn, k_Mist, 1));
	EXPECT_EQ(drawn[k_Mist], Sequence(1));
	draws::Run(queue, k_Heart, DrawFor(random, drawn, k_Heart, 12));
	const auto values = Sequence(13);
	EXPECT_EQ(drawn[k_Heart], std::vector<int32_t>(values.begin() + 1, values.end()));
	EXPECT_TRUE(queue.entries.empty());
}

TEST(CreationDraws, ALoadTakesTheDrawsAtItsEndInTheOrderTheReadsEnded)
{
	draws::Queue queue;
	FakeRandom random;
	Drawn drawn;
	draws::BeginLoad(queue);
	// made class by class: the mist first, then the heart, then the lantern
	draws::Run(queue, k_Mist, DrawFor(random, drawn, k_Mist, 1));
	draws::Run(queue, k_Heart, DrawFor(random, drawn, k_Heart, 12));
	draws::Run(queue, k_Lantern, DrawFor(random, drawn, k_Lantern, 3));
	EXPECT_TRUE(drawn.empty());
	// read in the file as the lantern, the mist, the heart
	draws::EndLoad(queue, RankOf({{k_Lantern, 0}, {k_Mist, 1}, {k_Heart, 2}}), Alive);
	const auto values = Sequence(16);
	EXPECT_EQ(drawn[k_Lantern], std::vector<int32_t>(values.begin(), values.begin() + 3));
	EXPECT_EQ(drawn[k_Mist], std::vector<int32_t>(values.begin() + 3, values.begin() + 4));
	EXPECT_EQ(drawn[k_Heart], std::vector<int32_t>(values.begin() + 4, values.end()));
	EXPECT_FALSE(queue.deferring);
	EXPECT_TRUE(queue.entries.empty());
}

TEST(CreationDraws, OneObjectsDrawsKeepTheirOrder)
{
	draws::Queue queue;
	std::vector<int> order;
	draws::BeginLoad(queue);
	draws::Run(queue, k_Heart, [&order]() { order.push_back(1); });
	draws::Run(queue, k_Mist, [&order]() { order.push_back(0); });
	draws::Run(queue, k_Heart, [&order]() { order.push_back(2); });
	draws::EndLoad(queue, RankOf({{k_Mist, 0}, {k_Heart, 1}}), Alive);
	EXPECT_EQ(order, (std::vector<int> {0, 1, 2}));
}

TEST(CreationDraws, AnObjectWithoutARankDrawsAfterTheRankedOnes)
{
	draws::Queue queue;
	std::vector<entt::entity> order;
	draws::BeginLoad(queue);
	draws::Run(queue, k_Lantern, [&order]() { order.push_back(k_Lantern); });
	draws::Run(queue, k_Heart, [&order]() { order.push_back(k_Heart); });
	draws::Run(queue, k_Mist, [&order]() { order.push_back(k_Mist); });
	const auto taken = draws::EndLoad(queue, RankOf({{k_Mist, 0}}), Alive);
	EXPECT_EQ(order, (std::vector<entt::entity> {k_Mist, k_Lantern, k_Heart}));
	EXPECT_EQ(taken.held, 3U);
	EXPECT_EQ(taken.ranked, 1U);
	EXPECT_EQ(taken.drawn, 3U);
}

TEST(CreationDraws, AnObjectGoneBeforeTheEndDrawsNothing)
{
	draws::Queue queue;
	std::vector<entt::entity> order;
	draws::BeginLoad(queue);
	draws::Run(queue, k_Mist, [&order]() { order.push_back(k_Mist); });
	draws::Run(queue, k_Heart, [&order]() { order.push_back(k_Heart); });
	const auto taken = draws::EndLoad(queue, RankOf({}), [](entt::entity owner) { return owner != k_Mist; });
	EXPECT_EQ(order, (std::vector<entt::entity> {k_Heart}));
	EXPECT_EQ(taken.held, 2U);
	EXPECT_EQ(taken.drawn, 1U);
}

TEST(CreationDraws, AfterTheLoadDrawsAreTakenAtOnce)
{
	draws::Queue queue;
	int taken = 0;
	draws::BeginLoad(queue);
	draws::EndLoad(queue, RankOf({}), Alive);
	draws::Run(queue, k_Mist, [&taken]() { ++taken; });
	EXPECT_EQ(taken, 1);
}

TEST(CreationDraws, ALoadThatStopsLeavesNothingWaiting)
{
	draws::Queue queue;
	int taken = 0;
	draws::BeginLoad(queue);
	draws::Run(queue, k_Mist, [&taken]() { ++taken; });
	draws::Drop(queue);
	EXPECT_EQ(taken, 0);
	EXPECT_TRUE(queue.entries.empty());
	draws::Run(queue, k_Mist, [&taken]() { ++taken; });
	EXPECT_EQ(taken, 1);
}
