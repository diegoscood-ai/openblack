/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The need signs over a worship site's altar (docs/wiki/magic/worship.md#the-altars-need-signs): the desires for rest
// and for the people hiding, pinned to the original's own results (the exe run on the same inputs); where each sign
// stands; the hand over a sign; and its tooltip. Pure rules, no game.

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ShowNeeds.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "Worship/AltarNeeds.h"

using namespace openblack;
namespace altar_needs = openblack::worship::altar_needs;
namespace hand_tooltips = openblack::ecs::systems::hand_tooltips;

namespace
{
struct RestCase
{
	/// Each group's dancers' lives, in the dance's order
	std::vector<std::vector<float>> groups;
	uint32_t desire;
};

struct HidingCase
{
	int32_t atSite;
	int32_t dancers;
	uint32_t desire;
};
} // namespace

TEST(AltarNeeds, TheRestSignIsTheDancersAverageWantOfLifeAtMostOne)
{
	// the original's results, as bits, for the dancers' lives group by group in the dance's order
	const std::array<RestCase, 14> cases {{
	    {{}, 0x00000000},
	    {{{}}, 0x00000000},
	    {{{1.0f}}, 0x00000000},
	    {{{0.3f}}, 0x3F333333},
	    {{{0.3f, 0.7f, 0.1f}}, 0x3F222222},
	    {{{0.9f, 0.85f, 0.2f, 0.55f, 0.33f}}, 0x3EDE3540},
	    {{{0.123f, 0.456f, 0.789f, 0.011f, 0.999f, 0.5f, 0.25f}}, 0x3F0D9AC5},
	    {{{0.123f, 0.456f, 0.789f}, {0.011f, 0.999f, 0.5f, 0.25f}}, 0x3F0D9AC5},
	    // the same five dancers summed group by group, and in the order they would have joined: the rounding differs,
	    // and the original's is the group by group sum
	    {{{0.37f, 0.555f}, {0.37f, 0.9999f, 0.37f}}, 0x3EEF1D40},
	    {{{0.37f, 0.37f, 0.555f, 0.9999f, 0.37f}}, 0x3EEF1D3E},
	    {{{0.9999f}, {0.123f, 0.0001f}, {0.61f, 0.97f, 0.21f}}, 0x3F03B645},
	    {{{0.123f, 0.0001f}, {0.9999f}, {0.61f, 0.97f, 0.21f}}, 0x3F03B645},
	    // a life below 0 wants more than all: held at 1
	    {{{-0.5f}}, 0x3F800000},
	    {{{-1.0f, 0.2f}}, 0x3F800000},
	}};
	for (const auto& c : cases)
	{
		const float desire = altar_needs::DesireForRest(altar_needs::AverageLifeDesired(c.groups));
		EXPECT_EQ(std::bit_cast<uint32_t>(desire), c.desire) << c.groups.size() << " groups";
	}
	// at 1 or below it is kept as it is
	EXPECT_EQ(altar_needs::DesireForRest(1.0f), 1.0f);
	EXPECT_EQ(altar_needs::DesireForRest(0.25f), 0.25f);
	EXPECT_EQ(altar_needs::DesireForRest(1.5f), 1.0f);
}

TEST(AltarNeeds, TheHidingSignCountsAHundredthForEachPersonNotDancing)
{
	const std::array<HidingCase, 8> cases {{
	    {2, 2, 0x00000000},
	    {3, 1, 0x3CA3D70A},
	    {9, 2, 0x3D8F5C29},
	    {13, 6, 0x3D8F5C29},
	    {37, 2, 0x3EB33333},
	    {99, 0, 0x3F7D70A3},
	    {150, 1, 0x3F800000},
	    // fewer at the site than dance: the unsigned count wraps around, the sign shows full
	    {1, 3, 0x3F800000},
	}};
	for (const auto& c : cases)
	{
		const float desire = altar_needs::PeopleHidingIndicator(c.atSite, c.dancers);
		EXPECT_EQ(std::bit_cast<uint32_t>(desire), c.desire) << c.atSite << " at the site, " << c.dancers << " dancing";
	}
}

TEST(AltarNeeds, EachSignStandsAtItsOwnPoint)
{
	// an altar's sign at its site's point of its row; a workshop's at its workshop's point 9 whatever its row
	EXPECT_EQ(ecs::show_needs::NeedsPoint(true, ecs::show_needs::k_HungerInfo), 0);
	EXPECT_EQ(ecs::show_needs::NeedsPoint(true, ecs::show_needs::k_LifeInfo), 1);
	EXPECT_EQ(ecs::show_needs::NeedsPoint(true, ecs::show_needs::k_HidingInfo), 2);
	EXPECT_EQ(ecs::show_needs::NeedsPoint(false, ecs::show_needs::k_WorkshopInfo), 9);
	EXPECT_EQ(ecs::show_needs::NeedsPoint(false, 0), 9);
}

TEST(AltarNeeds, TheHandOverASignIsOverObject)
{
	// a sign can be acted on, takes no locked select and never goes into the hand: Over Object, in the influence or out
	for (const bool inInfluence : {true, false})
	{
		EXPECT_EQ(hand_tooltips::ObjectHandState({.interactable = true,
		                                          .inInfluence = inInfluence,
		                                          .mustBeInInfluence = true,
		                                          .validForLockedSelect = false,
		                                          .validForPlaceInHand = false,
		                                          .cannotBePickedUp = false}),
		          18);
	}
}

TEST(AltarNeeds, ASignsToolTipIsItsRows)
{
	// hunger and rest with how full they show, in hundredths, with no button
	const auto hunger = hand_tooltips::NeedsSignToolTip(ecs::show_needs::k_HungerInfo, 0.5f, 7);
	EXPECT_EQ(hunger.text, 0xECEu);
	EXPECT_EQ(hunger.action, hand_tooltips::k_ActionNone);
	EXPECT_EQ(hunger.align, 0u);
	EXPECT_FALSE(hunger.force);
	ASSERT_EQ(hunger.numbers.count, 1u);
	EXPECT_EQ(hunger.numbers.values[0], 50.0);
	const auto rest = hand_tooltips::NeedsSignToolTip(ecs::show_needs::k_LifeInfo, 0.333f, 0);
	EXPECT_EQ(rest.text, 0xEFFu);
	ASSERT_EQ(rest.numbers.count, 1u);
	EXPECT_EQ(rest.numbers.values[0], static_cast<double>(0.333f * 100.0f));
	// the hiding sign keeps the plain text, with no number
	const auto hiding = hand_tooltips::NeedsSignToolTip(ecs::show_needs::k_HidingInfo, 0.9f, 0);
	EXPECT_EQ(hiding.text, 0xE73u);
	EXPECT_EQ(hiding.numbers.count, 0u);
	// a workshop's: the wood it still wants
	const auto workshop = hand_tooltips::NeedsSignToolTip(ecs::show_needs::k_WorkshopInfo, 0.9f, 2500);
	EXPECT_EQ(workshop.text, 0xEFBu);
	ASSERT_EQ(workshop.numbers.count, 1u);
	EXPECT_EQ(workshop.numbers.values[0], 2500.0);
}
