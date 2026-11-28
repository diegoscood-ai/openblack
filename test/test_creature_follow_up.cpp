/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <deque>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFollowUp.h"

using namespace openblack;
using namespace openblack::creature_follow_up;
using creature_mind::Step;

namespace
{
/// Gives the rolls in turn, and records the ranges asked for and the shares drawn
struct FakeRolls
{
	std::deque<uint32_t> rolls;
	float share {0.0f};
	std::vector<uint32_t> ranges;
	uint32_t sharesDrawn {0};

	[[nodiscard]] std::function<uint32_t(uint32_t)> Function()
	{
		return [this](uint32_t range) {
			ranges.push_back(range);
			const auto roll = rolls.front();
			rolls.pop_front();
			return roll;
		};
	}
	[[nodiscard]] std::function<float()> Share()
	{
		return [this]() {
			++sharesDrawn;
			return share;
		};
	}
};

std::vector<Step> Slap(Feelings feelings, std::deque<uint32_t> rolls, float share = 0.0f)
{
	FakeRolls fake {.rolls = std::move(rolls), .share = share};
	return ShowSlapFeeling(feelings, fake.Function(), fake.Share(), 10);
}
} // namespace

TEST(CreatureFollowUp, ASlapFrightensACreatureNoAngrierThanItIsKind)
{
	const auto agenda = Slap({.compassion = 0.5f, .anger = 0.5f}, {0});
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, k_Frightened);
}

TEST(CreatureFollowUp, ASlapMakesAnAngryCreatureAngryOrRude)
{
	EXPECT_EQ(Slap({.compassion = 0.2f, .anger = 0.5f}, {0, 0})[0].animation, k_Angry);
	EXPECT_EQ(Slap({.compassion = 0.2f, .anger = 0.5f}, {0, 1})[0].animation, k_RudeGesture);
}

TEST(CreatureFollowUp, ASlapCanMakeItSadOrCry)
{
	EXPECT_EQ(Slap({}, {1})[0].animation, k_Sad);
	EXPECT_EQ(Slap({}, {2})[0].animation, k_Cry);
}

TEST(CreatureFollowUp, ASlapCanMakeItLookAtThePlayerSadlyForOneToTwoSeconds)
{
	const auto shortest = Slap({}, {3}, 0.0f);
	ASSERT_EQ(shortest.size(), 1u);
	EXPECT_EQ(shortest[0].kind, Step::Kind::LookAtCamera);
	EXPECT_FLOAT_EQ(shortest[0].seconds, 1.0f);
	EXPECT_EQ(shortest[0].face, creature_face::Cue::Sad);
	EXPECT_EQ(shortest[0].heldEyes, creature_mind::Eyes::Stoned);
	EXPECT_FLOAT_EQ(Slap({}, {3}, 0.99f)[0].seconds, 1.9f);
	EXPECT_EQ(StareTurns(0.0f, 10), 10u);
	EXPECT_EQ(StareTurns(0.99999994f, 10), 19u);
	EXPECT_EQ(StareTurns(0.5f, 10), 15u);
}

TEST(CreatureFollowUp, ASlapRollsFourWaysFirstAndDrawsAShareOnlyForTheLook)
{
	FakeRolls angry {.rolls = {0, 0}};
	static_cast<void>(ShowSlapFeeling({.compassion = 0.0f, .anger = 1.0f}, angry.Function(), angry.Share(), 10));
	EXPECT_EQ(angry.ranges, (std::vector<uint32_t> {4, 2}));
	EXPECT_EQ(angry.sharesDrawn, 0u);
	FakeRolls look {.rolls = {3}};
	static_cast<void>(ShowSlapFeeling({}, look.Function(), look.Share(), 10));
	EXPECT_EQ(look.ranges, (std::vector<uint32_t> {4}));
	EXPECT_EQ(look.sharesDrawn, 1u);
}
