/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"

using namespace openblack;
using creature_desires::Desire;
using creature_desires::Desires;
using creature_desires::detail::DominantDesire;
namespace sources = creature_desires::sources;

namespace
{
constexpr uint32_t k_TurnsPerSecond = 10;
constexpr float k_Floor = 0.05f;

/// Every desire active, each with its own value and maximum, none held down; anger and the wish to make friends with
/// the sources the game's tables give them
Desires Some()
{
	Desires desires;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		auto& state = desires.desires.at(i);
		state.activated = true;
		state.max = 1.0f + 0.1f * static_cast<float>(i);
		state.value = 0.2f + 0.01f * static_cast<float>(i);
	}
	desires[Desire::Anger].sources = {
	    {.type = sources::k_AngerFromDamage, .value = 0.3f},
	    {.type = sources::k_AngerFromSadness, .value = 0.4f},
	    {.type = sources::k_InnateAggression, .value = 0.5f},
	};
	desires[Desire::BeFriends].sources = {
	    {.type = 34, .value = 0.1f},
	    {.type = sources::k_InnateFriendliness, .value = 0.2f},
	};
	return desires;
}

constexpr std::array k_Never {Desire::IdleWithPlayer, Desire::RestoreHealth,  Desire::BeFriends,        Desire::ManifestState,
                              Desire::Rest,           Desire::PlayWithPlayer, Desire::HangAroundAtHome, Desire::LookAround};
constexpr std::array k_Needs {Desire::Hunger, Desire::Poo, Desire::Tiredness, Desire::Water};

template <std::size_t N>
bool In(const std::array<Desire, N>& list, Desire desire)
{
	return std::ranges::find(list, desire) != list.end();
}
} // namespace

TEST(CreatureDominantDesire, TheExclusionTable)
{
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		const bool always = !In(k_Never, desire) && !In(k_Needs, desire);
		EXPECT_EQ(creature_desires::detail::HeldDownByDominant(desire, false), always) << i;
		EXPECT_EQ(creature_desires::detail::HeldDownByDominant(desire, true), always || In(k_Needs, desire)) << i;
	}
}

TEST(CreatureDominantDesire, OthersAreHeldDownForTheSecondsButNotTheNeeds)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 120.0f, false, k_TurnsPerSecond, k_Floor);
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		const bool held = desire != Desire::Anger && !In(k_Never, desire) && !In(k_Needs, desire);
		EXPECT_EQ(desires[desire].suppressedTurns, held ? 1200u : 0u) << i;
	}
}

TEST(CreatureDominantDesire, NeedsTooHoldsTheBodysNeedsDown)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Impress, 120.0f, true, k_TurnsPerSecond, k_Floor);
	for (const auto need : k_Needs)
	{
		EXPECT_EQ(desires[need].suppressedTurns, 1200u);
	}
	for (const auto never : k_Never)
	{
		EXPECT_EQ(desires[never].suppressedTurns, 0u);
	}
	EXPECT_TRUE(dominant.needsToo);
}

TEST(CreatureDominantDesire, ALongerHoldAlreadyThereIsKept)
{
	auto desires = Some();
	desires[Desire::Curiosity].suppressedTurns = 5000;
	desires[Desire::Anger].suppressedTurns = 5000;
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 120.0f, false, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 5000u);
	// The chosen desire is let go of
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
}

TEST(CreatureDominantDesire, TheChosenIsFullyDominantWithItsSourcesFull)
{
	auto desires = Some();
	desires[Desire::Anger].activated = false;
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 36000.0f, false, k_TurnsPerSecond, k_Floor);
	ASSERT_TRUE(dominant.desire.has_value());
	EXPECT_EQ(*dominant.desire, Desire::Anger);
	EXPECT_EQ(dominant.turns, 0u);
	EXPECT_EQ(dominant.seconds, 36000u);
	EXPECT_FALSE(dominant.needsToo);
	EXPECT_TRUE(desires[Desire::Anger].activated);
	EXPECT_EQ(desires[Desire::Anger].value, desires[Desire::Anger].max);
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		if (static_cast<Desire>(i) != Desire::Anger)
		{
			EXPECT_EQ(desires.desires.at(i).value, k_Floor) << i;
		}
	}
	// Every source full but anger from being damaged
	const auto& anger = desires[Desire::Anger].sources;
	EXPECT_FLOAT_EQ(anger.at(0).value, 0.3f);
	EXPECT_EQ(anger.at(1).value, 1.0f);
	EXPECT_EQ(anger.at(2).value, 1.0f);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
}

TEST(CreatureDominantDesire, AnInactiveDesireKeepsTheValuesButGetsItsSourcesAndIsActivated)
{
	auto desires = Some();
	desires[Desire::BeFriends].activated = false;
	const auto before = desires[Desire::Curiosity].value;
	creature_desires::MakeFullyDominantWithFullSources(desires, Desire::BeFriends, k_Floor);
	EXPECT_EQ(desires[Desire::Curiosity].value, before);
	EXPECT_TRUE(desires[Desire::BeFriends].activated);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(0).value, 1.0f);
	EXPECT_EQ(desires[Desire::BeFriends].sources.at(1).value, 1.0f);
}

TEST(CreatureDominantDesire, CompassionAlsoFreesTheWishToMakeFriends)
{
	auto desires = Some();
	desires[Desire::BeFriends].activated = false;
	desires[Desire::BeFriends].suppressedTurns = 500;
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Compassion, 36000.0f, false, k_TurnsPerSecond, k_Floor);
	const auto& friends = desires[Desire::BeFriends];
	EXPECT_EQ(friends.suppressedTurns, 0u);
	EXPECT_TRUE(friends.activated);
	EXPECT_EQ(friends.sources.at(0).value, 1.0f);
	EXPECT_EQ(friends.sources.at(1).value, 1.0f);
	// Compassion takes over after it, so its value is back at the floor
	EXPECT_EQ(friends.value, k_Floor);
	EXPECT_EQ(desires[Desire::Compassion].value, desires[Desire::Compassion].max);
}

TEST(CreatureDominantDesire, AnotherDesireLeavesTheWishToMakeFriendsAlone)
{
	auto desires = Some();
	desires[Desire::BeFriends].suppressedTurns = 500;
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 36000.0f, false, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(desires[Desire::BeFriends].suppressedTurns, 500u);
	EXPECT_FLOAT_EQ(desires[Desire::BeFriends].sources.at(0).value, 0.1f);
}

TEST(CreatureDominantDesire, EachTurnTheChosenIsLetGoOfAndCounted)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 120.0f, false, k_TurnsPerSecond, k_Floor);
	desires[Desire::Anger].suppressedTurns = 7;
	creature_desires::detail::StepDominant(dominant, desires, k_TurnsPerSecond);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
	EXPECT_EQ(dominant.turns, 1u);
	EXPECT_TRUE(dominant.desire.has_value());
	// The others stay held
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 1200u);
}

TEST(CreatureDominantDesire, ItRunsOutOnTheTurnTheWholeSecondsPassItsSeconds)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 2.0f, false, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(creature_desires::detail::DominantTurnsLeft(dominant, k_TurnsPerSecond), 30u);
	for (int turn = 1; turn <= 29; ++turn)
	{
		creature_desires::detail::StepDominant(dominant, desires, k_TurnsPerSecond);
		ASSERT_TRUE(dominant.desire.has_value()) << turn;
	}
	EXPECT_EQ(creature_desires::detail::DominantTurnsLeft(dominant, k_TurnsPerSecond), 1u);
	creature_desires::detail::StepDominant(dominant, desires, k_TurnsPerSecond);
	EXPECT_FALSE(dominant.desire.has_value());
	EXPECT_EQ(dominant.turns, 30u);
	for (const auto& state : desires.desires)
	{
		EXPECT_EQ(state.suppressedTurns, 0u);
	}
	EXPECT_EQ(creature_desires::detail::DominantTurnsLeft(dominant, k_TurnsPerSecond), 0u);
}

TEST(CreatureDominantDesire, TheSecondsAreKeptWhole)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 2.9f, false, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(dominant.seconds, 2u);
	// The others are held for the seconds as given, in whole turns
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 29u);
}

TEST(CreatureDominantDesire, SettingItAgainStartsTheCountAgain)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 2.0f, false, k_TurnsPerSecond, k_Floor);
	for (int turn = 0; turn < 25; ++turn)
	{
		creature_desires::detail::StepDominant(dominant, desires, k_TurnsPerSecond);
	}
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 2.0f, false, k_TurnsPerSecond, k_Floor);
	EXPECT_EQ(dominant.turns, 0u);
	EXPECT_EQ(creature_desires::detail::DominantTurnsLeft(dominant, k_TurnsPerSecond), 30u);
}

TEST(CreatureDominantDesire, ClearLetsEveryDesireGo)
{
	auto desires = Some();
	DominantDesire dominant;
	creature_desires::detail::SetDominant(dominant, desires, Desire::Anger, 120.0f, true, k_TurnsPerSecond, k_Floor);
	creature_desires::detail::ClearDominant(dominant, desires);
	EXPECT_FALSE(dominant.desire.has_value());
	for (const auto& state : desires.desires)
	{
		EXPECT_EQ(state.suppressedTurns, 0u);
	}
}

TEST(CreatureDominantDesire, ClearWithNothingForcedLeavesTheDesiresAlone)
{
	auto desires = Some();
	desires[Desire::Sadness].suppressedTurns = 40;
	DominantDesire dominant;
	creature_desires::detail::ClearDominant(dominant, desires);
	EXPECT_EQ(desires[Desire::Sadness].suppressedTurns, 40u);
}

TEST(CreatureDominantDesire, ATurnWithNothingForcedChangesNothing)
{
	auto desires = Some();
	desires[Desire::Sadness].suppressedTurns = 40;
	const auto before = desires;
	DominantDesire dominant;
	creature_desires::detail::StepDominant(dominant, desires, k_TurnsPerSecond);
	EXPECT_FALSE(dominant.desire.has_value());
	EXPECT_EQ(dominant.turns, 0u);
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto& now = desires.desires.at(i);
		const auto& was = before.desires.at(i);
		EXPECT_EQ(now.activated, was.activated) << i;
		EXPECT_EQ(now.value, was.value) << i;
		EXPECT_EQ(now.suppressedTurns, was.suppressedTurns) << i;
		ASSERT_EQ(now.sources.size(), was.sources.size()) << i;
		for (size_t s = 0; s < now.sources.size(); ++s)
		{
			EXPECT_EQ(now.sources[s].value, was.sources[s].value) << i;
		}
	}
	EXPECT_EQ(desires.sum, before.sum);
}

TEST(CreatureDominantDesire, TheDominantIsTheFirstActivatedDesireWantedMost)
{
	auto desires = Some();
	// the strongest is the last one; held down it still counts
	desires[Desire::Steal].suppressedTurns = 50;
	EXPECT_EQ(creature_desires::FindDominant(desires), Desire::Steal);
	// left out, the next strongest; not activated, it does not count
	EXPECT_EQ(creature_desires::FindDominant(desires, Desire::Steal), Desire::LookAround);
	desires[Desire::LookAround].activated = false;
	EXPECT_EQ(creature_desires::FindDominant(desires, Desire::Steal), Desire::MissFriend);
	// of equals, the first
	desires[Desire::Anger].value = 5.0f;
	desires[Desire::Play].value = 5.0f;
	EXPECT_EQ(creature_desires::FindDominant(desires), Desire::Anger);
	// none wanted at all: the first desire, as when it is the one wanted most
	for (auto& state : desires.desires)
	{
		state.value = 0.0f;
	}
	EXPECT_EQ(creature_desires::FindDominant(desires), Desire::Impress);
}
