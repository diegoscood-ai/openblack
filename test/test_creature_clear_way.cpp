/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureClearWay.h"
#include "Creature/CreatureDesires.h"

using namespace openblack;
using namespace openblack::creature_clear_way;

namespace
{
constexpr Actions k_Actions {.throwAround = 263, .kick = 236};
constexpr auto k_Anger = static_cast<uint32_t>(creature_desires::Desire::Anger);
constexpr auto k_Compassion = static_cast<uint32_t>(creature_desires::Desire::Compassion);

/// A random function that gives the numbers it is given in turn, and keeps the ranges it was asked for
struct Draws
{
	std::vector<uint32_t> results;
	std::vector<uint32_t> ranges;
	size_t next {0};
	uint32_t operator()(uint32_t range)
	{
		ranges.push_back(range);
		return results.at(next++);
	}
};
} // namespace

TEST(CreatureClearWay, NeverClearsATempleHeartAStoragePitOrATownCentre)
{
	const std::array miracles {Miracle {.known = true, .desire = k_Anger, .action = 99}};
	for (const auto& obstacle : {Obstacle {.templeHeart = true, .canPickUp = true, .canKick = true},
	                             Obstacle {.storagePit = true, .canPickUp = true, .canKick = true},
	                             Obstacle {.townCentre = true, .canPickUp = true, .canKick = true}})
	{
		EXPECT_EQ(ClearingAction(obstacle, miracles, k_Actions), std::nullopt);
	}
}

TEST(CreatureClearWay, ThrowsWhatItCanPickUpBeforeKicking)
{
	EXPECT_EQ(ClearingAction({.canPickUp = true, .canKick = true}, {}, k_Actions), std::optional<uint32_t>(263));
	EXPECT_EQ(ClearingAction({.canKick = true}, {}, k_Actions), std::optional<uint32_t>(236));
}

TEST(CreatureClearWay, CastsTheFirstKnownAngryMiracle)
{
	const std::array miracles {
	    Miracle {.known = false, .desire = k_Anger, .action = 10},     // not known
	    Miracle {.known = true, .desire = k_Compassion, .action = 11}, // not for anger
	    Miracle {.known = true, .desire = k_Anger, .action = 0},       // no casting action
	    Miracle {.known = true, .desire = k_Anger, .action = 13},      // the first
	    Miracle {.known = true, .desire = k_Anger, .action = 14},      // a later one
	};
	EXPECT_EQ(ClearingAction({}, miracles, k_Actions), std::optional<uint32_t>(13));
	EXPECT_EQ(ClearingAction({}, std::span(miracles).first(3), k_Actions), std::nullopt);
}

TEST(CreatureClearWay, NotesSixteenThingsAtMostWithNoRepeats)
{
	std::vector<entt::entity> met;
	EXPECT_TRUE(MayNote(met, entt::entity {5}));
	met.push_back(entt::entity {5});
	EXPECT_FALSE(MayNote(met, entt::entity {5}));
	for (uint32_t i = 6; met.size() < k_MostMet; ++i)
	{
		met.push_back(entt::entity {i});
	}
	EXPECT_FALSE(MayNote(met, entt::entity {100}));
}

TEST(CreatureClearWay, ChoosesNothingAndDrawsNothingWithNothingMet)
{
	Draws draws;
	EXPECT_EQ(Choose(0, std::ref(draws)), std::nullopt);
	EXPECT_TRUE(draws.ranges.empty());
}

TEST(CreatureClearWay, OneTimeInFourAnyOfThemElseOneOfTheFirstThree)
{
	Draws any {.results = {0, 7}};
	EXPECT_EQ(Choose(10, std::ref(any)), std::optional<size_t>(7));
	EXPECT_EQ(any.ranges, (std::vector<uint32_t> {4, 10}));

	Draws few {.results = {1, 2}};
	EXPECT_EQ(Choose(10, std::ref(few)), std::optional<size_t>(2));
	EXPECT_EQ(few.ranges, (std::vector<uint32_t> {4, 3}));

	Draws two {.results = {3, 1}};
	EXPECT_EQ(Choose(2, std::ref(two)), std::optional<size_t>(1));
	EXPECT_EQ(two.ranges, (std::vector<uint32_t> {4, 2}));
}

TEST(CreatureClearWay, WalksOnToWithinItsHeightOrEightMetres)
{
	EXPECT_EQ(WalkOnDistance(15.0f), 15.0f);
	EXPECT_EQ(WalkOnDistance(8.0f), 8.0f);
	EXPECT_EQ(WalkOnDistance(3.0f), 8.0f);
	EXPECT_TRUE(std::isnan(WalkOnDistance(std::numeric_limits<float>::quiet_NaN())));

	const auto steps = WalkOn({100.0f, 200.0f}, 22.5f);
	ASSERT_EQ(steps.size(), 1U);
	const auto& walk = steps.front();
	EXPECT_EQ(walk.kind, creature_mind::Step::Kind::Move);
	EXPECT_EQ(walk.movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_EQ(walk.movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_EQ(walk.movement.maxDistance, 22.5f);
	ASSERT_TRUE(walk.gaze.has_value());
	EXPECT_TRUE(walk.gaze->goingTowards);
}
