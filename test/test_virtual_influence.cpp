/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/VirtualInfluence.h"

using namespace openblack;
using namespace openblack::virtual_influence;

namespace
{
constexpr Settings k_Settings {.maxDistance = 50.0f, .maxTurns = 100, .chantsToDouble = 1000.0f};

/// The hand's place at (x, z) metres
map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres({x, z});
}

TurnInputs Inside(map_coords::MapCoords hand, uint32_t turn)
{
	return {.hand = hand, .turn = turn, .handShielded = false, .handInInfluence = true, .influencePower = 0.0f, .chants = 0.0f};
}

TurnInputs Outside(map_coords::MapCoords hand, uint32_t turn, float power = 0.0f, float chants = 0.0f)
{
	return {
	    .hand = hand, .turn = turn, .handShielded = false, .handInInfluence = false, .influencePower = power, .chants = chants};
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// What a turn out takes, as the rule is stated: one less the product of how near and how fresh
float Loss(float distance, uint32_t turnsOut, float power, float chants)
{
	const float timeScale = 1.0f / (chants / k_Settings.chantsToDouble + 1.0f);
	const float nearness = gutils::GetDistanceModifier(distance, 0.2f * power + k_Settings.maxDistance);
	const float freshness = gutils::GetDistanceModifier(timeScale * static_cast<float>(turnsOut),
	                                                    0.2f * power + static_cast<float>(k_Settings.maxTurns));
	return 1.0f - freshness * nearness;
}
} // namespace

TEST(VirtualInfluence, InInfluenceTheHandMarksThePlaceAndWinsATenthBack)
{
	State state {.fraction = 0.45f};
	ProcessTurn(state, Inside(At(100.0f, 200.0f), 30), k_Settings);
	EXPECT_EQ(state.anchor, At(100.0f, 200.0f));
	EXPECT_EQ(state.lastInsideTurn, 30u);
	EXPECT_FLOAT_EQ(state.fraction, 0.55f);
	for (uint32_t turn = 31; turn < 40; ++turn)
	{
		ProcessTurn(state, Inside(At(100.0f, 200.0f), turn), k_Settings);
	}
	EXPECT_EQ(state.fraction, 1.0f);
}

TEST(VirtualInfluence, OutsideItWanesWithDistanceAndTime)
{
	State state;
	ProcessTurn(state, Inside(At(100.0f, 100.0f), 10), k_Settings);
	// A step past the border, the turn after: hardly anything goes
	ProcessTurn(state, Outside(At(100.0f, 105.0f), 11), k_Settings);
	const float distance = gutils::GetDistanceInMetres(At(100.0f, 100.0f), At(100.0f, 105.0f));
	float expected = 1.0f - Loss(distance, 1, 0.0f, 0.0f);
	EXPECT_FLOAT_EQ(state.fraction, expected);
	EXPECT_GT(state.fraction, 0.9f);
	// Further and longer out it goes faster, and what is under a tenth is gone at once
	float before = state.fraction;
	for (uint32_t turn = 12; turn < 200 && state.fraction != 0.0f; ++turn)
	{
		ProcessTurn(state, Outside(At(100.0f, 140.0f), turn), k_Settings);
		EXPECT_LT(state.fraction, before);
		EXPECT_TRUE(state.fraction == 0.0f || state.fraction >= 0.1f);
		before = state.fraction;
	}
	EXPECT_EQ(state.fraction, 0.0f);
}

TEST(VirtualInfluence, InfluencePowerAndChantsMakeItLast)
{
	State weak;
	ProcessTurn(weak, Inside(At(0.0f, 0.0f), 0), k_Settings);
	State strong = weak;
	State chanting = weak;
	ProcessTurn(weak, Outside(At(0.0f, 30.0f), 40), k_Settings);
	ProcessTurn(strong, Outside(At(0.0f, 30.0f), 40, 500.0f), k_Settings);
	ProcessTurn(chanting, Outside(At(0.0f, 30.0f), 40, 0.0f, 3000.0f), k_Settings);
	EXPECT_GT(strong.fraction, weak.fraction);
	EXPECT_GE(chanting.fraction, weak.fraction);
	const float distance = gutils::GetDistanceInMetres(At(0.0f, 0.0f), At(0.0f, 30.0f));
	const float expected = 1.0f - Loss(distance, 40, 500.0f, 0.0f);
	EXPECT_FLOAT_EQ(strong.fraction, expected < 0.1f ? 0.0f : expected);
}

TEST(VirtualInfluence, AShieldTakesItAllAndAScriptCanTurnItOff)
{
	State state;
	ProcessTurn(state, Inside(At(0.0f, 0.0f), 0), k_Settings);
	auto shielded = Outside(At(0.0f, 5.0f), 1);
	shielded.handShielded = true;
	ProcessTurn(state, shielded, k_Settings);
	EXPECT_EQ(state.fraction, 0.0f);

	State off {.fraction = 0.5f, .disabled = true};
	ProcessTurn(off, Inside(At(20.0f, 30.0f), 3), k_Settings);
	EXPECT_EQ(off.fraction, 0.5f);
	EXPECT_EQ(off.anchor, map_coords::MapCoords {});
}

TEST(VirtualInfluence, AtTheMapsClearTheHandIsOutOfInfluenceUnlessItIsEverywhere)
{
	// the land's citadels, towns and rings are deleted: unshielded, out of influence, no chants, the power as it was
	const auto inputs = AtClearMap(At(10.0f, 20.0f), 500, false, 30.0f);
	EXPECT_EQ(inputs.hand, At(10.0f, 20.0f));
	EXPECT_EQ(inputs.turn, 500u);
	EXPECT_FALSE(inputs.handShielded);
	EXPECT_FALSE(inputs.handInInfluence);
	EXPECT_EQ(inputs.influencePower, 30.0f);
	EXPECT_EQ(inputs.chants, 0.0f);
	// so the strength wanes as for any turn out, from the last place in
	State state;
	ProcessTurn(state, Inside(At(0.0f, 0.0f), 490), k_Settings);
	ProcessTurn(state, inputs, k_Settings);
	const float distance = gutils::GetDistanceInMetres(At(0.0f, 0.0f), At(10.0f, 20.0f));
	const float expected = 1.0f - Loss(distance, 10, 30.0f, 0.0f);
	EXPECT_FLOAT_EQ(state.fraction, expected < 0.1f ? 0.0f : expected);
	EXPECT_EQ(state.turnHand, At(10.0f, 20.0f));
	// with the influence everywhere the hand is in it there too: it marks the place
	State everywhere {.fraction = 0.5f};
	ProcessTurn(everywhere, AtClearMap(At(10.0f, 20.0f), 500, true, 30.0f), k_Settings);
	EXPECT_EQ(everywhere.anchor, At(10.0f, 20.0f));
	EXPECT_EQ(everywhere.lastInsideTurn, 500u);
	EXPECT_FLOAT_EQ(everywhere.fraction, 0.6f);
}

TEST(VirtualInfluence, TheHumPlaysOutsideWhileTheLastPlaceInIsStillIn)
{
	State state {.anchor = At(10.0f, 10.0f), .fraction = 0.8f};
	EXPECT_TRUE(HumPlays(state, false, false, false, true));
	// Not inside, nor under a shield, nor once the last place in has fallen out of influence
	EXPECT_FALSE(HumPlays(state, false, true, false, true));
	EXPECT_FALSE(HumPlays(state, true, false, false, true));
	EXPECT_FALSE(HumPlays(state, false, false, true, true));
	EXPECT_FALSE(HumPlays(state, false, false, false, false));
	// Nor with nothing left, nor before the hand has been in influence, nor turned off
	EXPECT_FALSE(HumPlays(State {.anchor = At(10.0f, 10.0f), .fraction = 0.0f}, false, false, false, true));
	EXPECT_FALSE(HumPlays(State {.fraction = 0.8f}, false, false, false, true));
	EXPECT_FALSE(HumPlays(State {.anchor = At(10.0f, 10.0f), .fraction = 0.8f, .disabled = true}, false, false, false, true));
	// A last place in whose x is 0 counts as none, whatever its z
	EXPECT_FALSE(HumPlays(State {.anchor = At(0.0f, 10.0f), .fraction = 0.8f}, false, false, false, true));
}

TEST(VirtualInfluence, TheHumsPitchIsTheStrengthLeft)
{
	State state {.fraction = 0.734f, .soundFraction = 1.0f};
	EXPECT_EQ(HumPitchPercent(state), 73u);
	state.fraction = 0.1f;
	EXPECT_EQ(HumPitchPercent(state), 10u);
	// Never above what it began at
	state.fraction = 0.9f;
	state.soundFraction = 0.6f;
	EXPECT_EQ(HumPitchPercent(state), 60u);
}

TEST(VirtualInfluence, NearTheHandThePlaceIsInInfluenceAsMuchAsTheStrength)
{
	const auto hand = map_coords::FromMetres({500.0f, 500.0f});
	State state;
	state.fraction = 0.5f;
	// Half strength reaches five metres, the edge itself not counted
	const auto near = Grant(state, hand, map_coords::FromMetres({503.0f, 500.0f}));
	ASSERT_TRUE(near.has_value());
	EXPECT_FLOAT_EQ(*near, 0.5f);
	const float edge = gutils::GetDistanceInMetres(hand, map_coords::FromMetres({505.0f, 500.0f}));
	EXPECT_EQ(Grant(state, hand, map_coords::FromMetres({505.0f, 500.0f})).has_value(), edge < 5.0f);
	EXPECT_FALSE(Grant(state, hand, map_coords::FromMetres({506.0f, 500.0f})).has_value());
}

TEST(VirtualInfluence, AtTheHandAnyStrengthCountsAndNoneDoesNot)
{
	const auto hand = map_coords::FromMetres({120.0f, 80.0f});
	State state;
	state.fraction = 0.1f;
	EXPECT_TRUE(Grant(state, hand, hand).has_value());
	state.fraction = 0.0f;
	EXPECT_FALSE(Grant(state, hand, hand).has_value());
}

TEST(VirtualInfluence, TheGrantNeverExceedsFull)
{
	const auto hand = map_coords::FromMetres({120.0f, 80.0f});
	State state;
	state.fraction = 1.5f;
	EXPECT_FLOAT_EQ(Grant(state, hand, hand).value_or(0.0f), 1.0f);
	// And reaches no further than full strength does
	EXPECT_FALSE(Grant(state, hand, map_coords::FromMetres({131.0f, 80.0f})).has_value());
}

TEST(VirtualInfluence, ASwitchedOffTurnLeavesTheHandsPlaceUnnoted)
{
	// Switched off, the turn returns before the hand's place is noted
	State state;
	state.disabled = true;
	ProcessTurn(state, Outside(At(30.0f, 40.0f), 5), k_Settings);
	EXPECT_EQ(state.turnHand, map_coords::MapCoords {});
	EXPECT_EQ(state.anchor, map_coords::MapCoords {});
	// Switched on, every turn notes it, in influence or out
	state.disabled = false;
	ProcessTurn(state, Outside(At(30.0f, 40.0f), 6), k_Settings);
	EXPECT_EQ(state.turnHand, At(30.0f, 40.0f));
}

TEST(VirtualInfluence, AScriptSwitchingItOffTakesTheStrengthAndOnLeavesItAsItIs)
{
	State state {.fraction = 0.7f};
	SetEnabled(state, false);
	EXPECT_TRUE(state.disabled);
	EXPECT_EQ(state.fraction, 0.0f);
	// Switched on again it starts from nothing and wins its strength back a tenth a turn in influence
	SetEnabled(state, true);
	EXPECT_FALSE(state.disabled);
	EXPECT_EQ(state.fraction, 0.0f);
	ProcessTurn(state, Inside(At(30.0f, 40.0f), 7), k_Settings);
	EXPECT_FLOAT_EQ(state.fraction, 0.1f);
	state.fraction = 0.6f;
	SetEnabled(state, true);
	EXPECT_EQ(state.fraction, 0.6f);
}

TEST(VirtualInfluence, TheOwnInterfaceMeasuresFromItsActionPointUnlessPausedOrAnotherInterface)
{
	// The interface's own question always; any other while the game runs
	EXPECT_TRUE(MeasuresFromActionPoint(true, true, false));
	EXPECT_TRUE(MeasuresFromActionPoint(true, true, true));
	EXPECT_TRUE(MeasuresFromActionPoint(true, false, false));
	EXPECT_FALSE(MeasuresFromActionPoint(true, false, true));
	// Another interface measures from its hand's place at the turn
	EXPECT_FALSE(MeasuresFromActionPoint(false, true, false));
	EXPECT_FALSE(MeasuresFromActionPoint(false, false, false));
}

TEST(VirtualInfluence, TheManaPathLeadsHalfwayBackRoundingHalvesToEven)
{
	const map_coords::MapCoords hand {.x = 1000, .z = 2000, .altitude = 0.0f};
	// Even differences halve exactly
	EXPECT_EQ(ManaPathStart(hand, {.x = 3000, .z = 1000, .altitude = 0.0f}),
	          (map_coords::MapCoords {.x = 2000, .z = 1500, .altitude = 0.0f}));
	// Odd ones round to the even neighbour: 1.5 to 2, 2.5 to 2, -1.5 to -2
	EXPECT_EQ(ManaPathStart(hand, {.x = 1003, .z = 2005, .altitude = 0.0f}).x, 1002);
	EXPECT_EQ(ManaPathStart(hand, {.x = 1003, .z = 2005, .altitude = 0.0f}).z, 2002);
	EXPECT_EQ(ManaPathStart(hand, {.x = 997, .z = 2000, .altitude = 0.0f}).x, 998);
}

TEST(VirtualInfluence, TheManaPathsHalfwayPointKeepsHalfTheAltitudesWay)
{
	// The game's own halfway point, run natively: the hand at altitude 0, as the hand's draw gives it, and above it
	struct Case
	{
		map_coords::MapCoords hand;
		map_coords::MapCoords anchor;
		int32_t x;
		int32_t z;
		uint32_t altitudeBits;
	};
	const std::array<Case, 5> cases {{
	    {{1000, 2000, 0.0f}, {3000, 1000, 7.25f}, 2000, 1500, 0x40680000u},
	    {{1000, 2000, 1.0f}, {1003, 2005, 3.0f}, 1002, 2002, 0x40000000u},
	    {{0, 0, 0.0f}, {-3, -5, 0.1f}, -2, -2, 0x3D4CCCCDu},
	    {{6553600, 13107200, 0.0f}, {6553605, 13107197, 12.345678f}, 6553602, 13107198, 0x40C587E6u},
	    {{0, 0, 0.0f}, {16777219, 7, 0.001f}, 8388610, 4, 0x3A03126Fu},
	}};
	for (const auto& c : cases)
	{
		const auto start = ManaPathStart(c.hand, c.anchor);
		EXPECT_EQ(start.x, c.x);
		EXPECT_EQ(start.z, c.z);
		EXPECT_EQ(BitsOf(start.altitude), c.altitudeBits);
	}
}

TEST(VirtualInfluence, AManaPathSparkIsDueOncePastTenWeighedMilliseconds)
{
	ManaPathClock clock;
	// 16 ms at half strength is 8: not yet; another 4 ms makes 10, which is not past ten
	EXPECT_FALSE(EmitManaPath(clock, 0.5f, 16.0f).has_value());
	EXPECT_FALSE(EmitManaPath(clock, 0.5f, 4.0f).has_value());
	EXPECT_FLOAT_EQ(clock.emission, 10.0f);
	// Then one more and it is due, starting again from nothing, scaled by half of 255 truncated
	const auto scale = EmitManaPath(clock, 0.5f, 1.0f);
	ASSERT_TRUE(scale.has_value());
	EXPECT_EQ(*scale, 127);
	EXPECT_FLOAT_EQ(clock.emission, 0.0f);
}

TEST(VirtualInfluence, AManaPathSparkAtFullStrengthIsNotDimmedPastFull)
{
	const State state {.fraction = 1.5f};
	ManaPathClock clock;
	const auto scale = EmitManaPath(clock, Strength(state), 11.0f);
	ASSERT_TRUE(scale.has_value());
	EXPECT_EQ(*scale, 255);
}

TEST(VirtualInfluence, TheSparksTimeIsOneCountForEveryHand)
{
	// Two hands letting out their mana paths in the same frames add to the one count: together they pass ten first
	ManaPathClock clock;
	EXPECT_FALSE(EmitManaPath(clock, 1.0f, 6.0f).has_value());
	const auto scale = EmitManaPath(clock, 0.8f, 6.0f);
	ASSERT_TRUE(scale.has_value());
	EXPECT_EQ(*scale, 204);
	EXPECT_EQ(clock.emission, 0.0f);
}

TEST(VirtualInfluence, ColoursAreScaledOnEveryChannelOutOf256)
{
	EXPECT_EQ(ScaleColour(0xFFFF4646u, 255), 0xFEFE4545u);
	EXPECT_EQ(ScaleColour(0xFF47FF54u, 128), 0x7F237F2Au);
	EXPECT_EQ(ScaleColour(0xFFFFFFFFu, 0), 0u);
}

TEST(VirtualInfluence, BackInInfluenceTheManaPathIsWorkedOutAfresh)
{
	State state;
	state.manaPathStart = map_coords::MapCoords {.x = 5, .z = 6, .altitude = 1.5f};
	ProcessTurn(state, Outside(At(30.0f, 40.0f), 5), k_Settings);
	EXPECT_EQ(state.manaPathStart.x, 5);
	ProcessTurn(state, Inside(At(30.0f, 40.0f), 6), k_Settings);
	EXPECT_EQ(state.manaPathStart, map_coords::MapCoords {});
}

// Only a turn where the strength wanes draws on the worship sites, as the original
// (docs/wiki/magic/influence-and-alignment.md#virtual-influence): switched on, out of influence and unshielded, even with
// no strength left
TEST(VirtualInfluence, OnlyAWaningTurnAsksTheWorshipSitesForChants)
{
	const State on;
	EXPECT_TRUE(Wanes(on, false, false));
	EXPECT_FALSE(Wanes(on, false, true));
	EXPECT_FALSE(Wanes(on, true, false));
	const State spent {.fraction = 0.0f};
	EXPECT_TRUE(Wanes(spent, false, false));
	const State off {.disabled = true};
	EXPECT_FALSE(Wanes(off, false, false));
}
