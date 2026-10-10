/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureSpellMind.h"

using namespace openblack;
using namespace openblack::creature_spell_mind;
using openblack::creature_desires::Desire;

namespace
{
creature_desires::Desires Some()
{
	creature_desires::Desires desires;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		auto& state = desires.desires.at(i);
		state.activated = true;
		state.max = 1.0f;
		state.value = 0.2f + 0.01f * static_cast<float>(i);
	}
	desires[Desire::BeFriends].sources = {
	    {.type = 34, .value = 0.1f},
	    {.type = creature_desires::sources::k_InnateFriendliness, .value = 0.2f},
	};
	return desires;
}
} // namespace

constexpr float k_TurnsPerSecond = 10.0f;
constexpr uint32_t k_HeldTurns = 200000;
/// The species' desire floor
constexpr float k_Floor = 0.05f;

TEST(CreatureSpellMind, ASpellsDesireIsWantedAboveAllAndMostOthersHeldDown)
{
	auto desires = Some();
	const auto cheat = SetCheatDominant(desires, Desire::Anger, true, k_TurnsPerSecond, k_Floor);
	ASSERT_TRUE(cheat.desire.has_value());
	EXPECT_EQ(*cheat.desire, Desire::Anger);
	EXPECT_EQ(cheat.seconds, 20000u);
	EXPECT_EQ(desires[Desire::Anger].value, desires[Desire::Anger].max);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
	// Every other desire is wanted as little as any can be
	EXPECT_EQ(desires[Desire::Curiosity].value, k_Floor);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, k_HeldTurns);
	// The body's needs only because all are to be held down
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, k_HeldTurns);
	// Never these
	for (const auto never : {Desire::IdleWithPlayer, Desire::RestoreHealth, Desire::BeFriends, Desire::ManifestState,
	                         Desire::Rest, Desire::PlayWithPlayer, Desire::HangAroundAtHome, Desire::LookAround})
	{
		EXPECT_EQ(desires[never].suppressedTurns, 0u);
	}
	EXPECT_FALSE(HeldDownByCheat(Desire::Water, false));
	EXPECT_TRUE(HeldDownByCheat(Desire::Water, true));
}

TEST(CreatureSpellMind, ALeashOrAScriptChoosesItsOwnSeconds)
{
	auto desires = Some();
	const auto cheat = SetCheatDominant(desires, Desire::Impress, false, k_TurnsPerSecond, k_Floor, 120.0f);
	EXPECT_EQ(cheat.seconds, 120u);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 1200u);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	EXPECT_EQ(CheatTurnsLeft(cheat, k_TurnsPerSecond), 1210u);
}

TEST(CreatureSpellMind, CompassionAlsoFreesTheWishToMakeFriends)
{
	auto desires = Some();
	desires[Desire::BeFriends].suppressedTurns = 500;
	(void)SetCheatDominant(desires, Desire::Compassion, true, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(desires[Desire::BeFriends].suppressedTurns, 0u);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(0).value, 1.0f);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(1).value, 1.0f);
	// Compassion is made dominant after it, so its value is back at the floor
	EXPECT_EQ(desires[Desire::BeFriends].value, k_Floor);
	EXPECT_EQ(desires[Desire::Compassion].value, desires[Desire::Compassion].max);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, k_HeldTurns);
}

TEST(CreatureSpellMind, TheCheatLastsItsTimeThenLetsEverythingGo)
{
	auto desires = Some();
	auto cheat = SetCheatDominant(desires, Desire::Anger, true, k_TurnsPerSecond, k_Floor);
	desires[Desire::Anger].suppressedTurns = 5;
	EXPECT_TRUE(StepCheat(desires, cheat, k_TurnsPerSecond));
	// Its own desire is let go each turn
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
	cheat.turns = static_cast<uint32_t>(k_CheatSeconds * k_TurnsPerSecond) + 10;
	EXPECT_FALSE(StepCheat(desires, cheat, k_TurnsPerSecond));
	EXPECT_FALSE(cheat.desire.has_value());
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 0u);
}

TEST(CreatureSpellMind, WearingOffItIsWantedLeast)
{
	auto desires = Some();
	// The others sit at the floor while the cheat holds; here it is the weakest of their values before
	constexpr float k_WeakestFloor = 0.2f;
	auto cheat = SetCheatDominant(desires, Desire::Scratch, true, k_TurnsPerSecond, k_WeakestFloor);
	MakeLeastDominant(desires, Desire::Scratch);
	// raffclar's expectation, kept until MakeLeastDominant is measured against the original (creature.md Pending)
	EXPECT_NEAR(desires[Desire::Scratch].value, 0.2f / k_LeastDominantFactor, 1e-5f);
	ClearCheatDominance(desires, cheat);
	EXPECT_FALSE(cheat.desire.has_value());
	for (const auto& state : desires.desires)
	{
		EXPECT_EQ(state.suppressedTurns, 0u);
	}
}

TEST(CreatureSpellMind, ClearingWithNoCheatLeavesTheDesiresAlone)
{
	auto desires = Some();
	desires[Desire::Sadness].suppressedTurns = 40;
	Cheat cheat;
	ClearCheatDominance(desires, cheat);
	EXPECT_EQ(desires[Desire::Sadness].suppressedTurns, 40u);
}

TEST(CreatureSpellMind, FullyDominantOverOthersLeavesItsSources)
{
	auto desires = Some();
	desires[Desire::BeFriends].suppressedTurns = 500;
	MakeFullyDominantOverOthers(desires, Desire::BeFriends, k_Floor);
	EXPECT_EQ(desires[Desire::BeFriends].suppressedTurns, 0u);
	EXPECT_EQ(desires[Desire::BeFriends].value, desires[Desire::BeFriends].max);
	EXPECT_EQ(desires[Desire::Anger].value, k_Floor);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(0).value, 0.1f);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(1).value, 0.2f);
}

TEST(CreatureSpellMind, FullyDominantOverOthersLetsAnInactiveDesireGoOnly)
{
	auto desires = Some();
	desires[Desire::BeFriends].activated = false;
	desires[Desire::BeFriends].suppressedTurns = 500;
	const float before = desires[Desire::BeFriends].value;
	MakeFullyDominantOverOthers(desires, Desire::BeFriends, k_Floor);
	EXPECT_EQ(desires[Desire::BeFriends].suppressedTurns, 0u);
	EXPECT_FALSE(desires[Desire::BeFriends].activated);
	EXPECT_EQ(desires[Desire::BeFriends].value, before);
	EXPECT_NE(desires[Desire::Anger].value, k_Floor);
}
