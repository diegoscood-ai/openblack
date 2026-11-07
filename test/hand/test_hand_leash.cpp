/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's part of tying the leash (ECS/HandLeash.h): which hand takes the Action button's double click, and the thing
// it is on. When a tap is remembered is tested in hand/test_hand_click.cpp, the leash's own decisions in
// creature/test_leash_hand_tie.cpp

#include <optional>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/HandLeash.h"

using openblack::ecs::hand_leash::HandActivity;
using openblack::ecs::hand_leash::TakesDoubleClick;
using openblack::ecs::hand_leash::TieTarget;

namespace
{
constexpr auto k_Under = static_cast<entt::entity>(7);
constexpr auto k_Near = static_cast<entt::entity>(9);
} // namespace

TEST(HandLeashDoubleClick, theIdleHandTakesIt)
{
	static_assert(TakesDoubleClick({}));
	EXPECT_TRUE(TakesDoubleClick(HandActivity {}));
}

TEST(HandLeashDoubleClick, anyActionLetsItGo)
{
	EXPECT_FALSE(TakesDoubleClick({.gripping = true}));
	EXPECT_FALSE(TakesDoubleClick({.bubbleGrip = true}));
	EXPECT_FALSE(TakesDoubleClick({.tugging = true}));
	EXPECT_FALSE(TakesDoubleClick({.grabbing = true}));
	EXPECT_FALSE(TakesDoubleClick({.lockedSelect = true}));
	EXPECT_FALSE(TakesDoubleClick({.releaseArmed = true}));
	EXPECT_FALSE(TakesDoubleClick({.seedApply = true}));
}

TEST(HandLeashTarget, whatIsUnderTheHandWinsAndTheNearestIsNotLookedFor)
{
	int asked = 0;
	const auto target = TieTarget(k_Under, [&asked]() -> std::optional<entt::entity> {
		++asked;
		return k_Near;
	});
	EXPECT_EQ(target, k_Under);
	EXPECT_EQ(asked, 0);
}

TEST(HandLeashTarget, withNothingUnderTheHandTheNearestThing)
{
	int asked = 0;
	const auto target = TieTarget(std::nullopt, [&asked]() -> std::optional<entt::entity> {
		++asked;
		return k_Near;
	});
	EXPECT_EQ(target, k_Near);
	EXPECT_EQ(asked, 1);
}

TEST(HandLeashTarget, nothingUnderOrNearIsNone)
{
	const auto target = TieTarget(std::nullopt, []() -> std::optional<entt::entity> { return std::nullopt; });
	EXPECT_EQ(target, entt::entity {entt::null});
}
