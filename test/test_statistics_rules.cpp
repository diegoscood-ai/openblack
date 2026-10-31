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
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/StatisticsRules.h"

using namespace openblack;
using namespace openblack::statistics;

namespace
{

/// A town of the player's with so many men and women, homes, civic buildings and wonders, all of them housed or
/// homeless
TownCounts Town(uint32_t men, uint32_t women, uint32_t abodes = 0, uint32_t civic = 0, uint32_t wonders = 0)
{
	return {.men = men,
	        .women = women,
	        .abodes = abodes,
	        .civic = civic,
	        .wonders = wonders,
	        .housed = men + women,
	        .housedMen = men};
}

} // namespace

TEST(StatisticsBuildings, EarlierTownsCountAgainInTheTotal)
{
	const std::array towns {Town(0, 0, 3, 1), Town(0, 0, 2, 2)};
	// First town: 3 + 1; second: (3 + 2) + (1 + 2)
	EXPECT_EQ(TotalBuildings(towns), 4u + 8u);
	EXPECT_EQ(TotalBuildings(std::span(towns).first(1)), 4u);
}

TEST(StatisticsMiracles, EachTypeCountsTowardsItsTotal)
{
	EXPECT_EQ(MiracleKindOf(MagicType::FlockGround), MiracleKind::Aggressive);
	EXPECT_EQ(MiracleKindOf(MagicType::FlockFlying), MiracleKind::Nice);
	EXPECT_EQ(MiracleKindOf(MagicType::CreatureSpellItchy), MiracleKind::Creature);
	EXPECT_FALSE(MiracleKindOf(MagicType::CreatureSpellHungry).has_value());
	EXPECT_FALSE(MiracleKindOf(MagicType::None).has_value());
	std::array<uint32_t, 42> casts {};
	casts.at(static_cast<size_t>(MagicType::Fireball)) = 2;
	casts.at(static_cast<size_t>(MagicType::Tornado)) = 1;
	casts.at(static_cast<size_t>(MagicType::Water)) = 3;
	casts.at(static_cast<size_t>(MagicType::CreatureSpellThirsty)) = 5;
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Aggressive), 3u);
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Nice), 3u);
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Creature), 0u);
}

TEST(StatisticsWorld, SharesAreWholePercentagesRoundedDown)
{
	EXPECT_EQ(Percent(0.999f), 99);
	EXPECT_EQ(Percent(1.0f / 3.0f), 33);
	EXPECT_EQ(Percent(0.0f), 0);
}

TEST(StatisticsWorld, TheScrollIsWorkedOutFromThePlayersTowns)
{
	auto first = Town(3, 5, 4, 2, 1);
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Builder)) = 2;
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Worship)) = 3;
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Protection)) = 1;
	// A missionary living in the player's own town counts where it preaches
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Missionary)) = 1;
	auto second = Town(2, 0, 1, 0, 0);
	second.disciples.at(static_cast<size_t>(VillagerDisciple::Trader)) = 1;
	const std::array towns {first, second};
	const auto figures = World({
	    .worldPopulation = 30,
	    .towns = towns,
	    .missionariesInOtherTowns = 1,
	    .missionaries = 2,
	    .deaths = 4,
	    .births = 6,
	    .sacrifices = 1,
	});
	EXPECT_EQ(figures.population, 30);
	// 10 of 30 believe, 5 of 10 are men
	EXPECT_EQ(figures.believersPercent, 33);
	EXPECT_EQ(figures.malePercent, 50);
	EXPECT_EQ(figures.deaths, 4);
	EXPECT_EQ(figures.births, 6);
	EXPECT_EQ(figures.sacrifices, 1);
	EXPECT_EQ(figures.buildings, (4 + 2) + (5 + 2));
	EXPECT_EQ(figures.wonders, 1);
	// Builders 2, worshippers 3, protectors 1 and a trader, and the missionary in another town
	EXPECT_EQ(figures.disciples, 8);
	EXPECT_EQ(figures.discipleKinds, (std::array<int32_t, 8> {2, 0, 0, 0, 0, 2, 0, 1}));
}

TEST(StatisticsWorld, TheShareOfMenIsOfThePeopleHousedAndHomeless)
{
	// The town's running counts say 5 men of 10, but of the 8 people in its homes and homeless 2 are men
	auto town = Town(5, 5);
	town.housed = 8;
	town.housedMen = 2;
	const std::array towns {town};
	EXPECT_EQ(World({.worldPopulation = 20, .towns = towns}).malePercent, 25);
	// Nobody housed or homeless: no share of men, whatever the running counts
	town.housed = 0;
	town.housedMen = 0;
	const std::array empty {town};
	EXPECT_EQ(World({.worldPopulation = 20, .towns = empty}).malePercent, 0);
}

TEST(StatisticsWorld, NobodyAnywhereGivesNoShares)
{
	const auto figures = World({});
	EXPECT_EQ(figures.believersPercent, 0);
	EXPECT_EQ(figures.malePercent, 0);
	EXPECT_EQ(figures.buildings, 0);
}

TEST(StatisticsBelief, TheBeliefInThePlayerIsSummedOverEveryTownThenTimesAThousand)
{
	const std::array beliefs {0.25f, 0.5f, 0.125f};
	EXPECT_EQ(TotalBelief(beliefs), 875.0f);
	// Summed first, in the towns' order, and only then scaled
	const std::array tenths {0.1f, 0.2f};
	EXPECT_EQ(TotalBelief(tenths), (0.1f + 0.2f) * 1000.0f);
	EXPECT_EQ(TotalBelief({}), 0.0f);
}

TEST(StatisticsTowns, TheTownFiguresAreSummedOverThePlayersTowns)
{
	auto first = Town(8, 6, 3, 1, 1);
	first.adults = 10;
	first.children = 4;
	first.adultPlaces = 12;
	first.childPlaces = 6;
	auto second = Town(3, 3, 2, 2, 0);
	second.adults = 5;
	second.children = 1;
	second.adultPlaces = 8;
	second.childPlaces = 2;
	const std::array towns {first, second};
	PlayerFigures figures;
	SetTownFigures(figures, towns);
	EXPECT_EQ(figures.towns, 2u);
	EXPECT_EQ(figures.population, 20u);
	EXPECT_EQ(figures.men, 11u);
	EXPECT_EQ(figures.women, 9u);
	// The places for adults and for children in every home of the player's
	EXPECT_EQ(figures.capacity, 12u + 6u + 8u + 2u);
	EXPECT_EQ(figures.abodes, 5u);
	EXPECT_EQ(figures.civic, 3u);
	EXPECT_EQ(figures.wonders, 1u);
	EXPECT_EQ(figures.totalBuildings, (3u + 1u) + (5u + 3u));

	PlayerFigures none;
	SetTownFigures(none, {});
	EXPECT_EQ(none.towns, 0u);
	EXPECT_EQ(none.capacity, 0u);
	EXPECT_EQ(none.abodes, 0u);
	EXPECT_EQ(none.civic, 0u);
	EXPECT_EQ(none.wonders, 0u);
}

TEST(StatisticsCreature, TheCreaturesNeedsAgeAndAlignmentAreShownAsTheyAreNow)
{
	const CreatureNow creature {
	    .body = CreatureBody {.alignment = -0.5f, .size = 3.0f},
	    .needs = CreatureNeedsNow {.life = 0.75f, .energy = 0.25f, .exhaustion = 0.125f, .age = 37},
	    .tallies = {.poos = 2},
	};
	PlayerFigures figures;
	SetCreatureFigures(figures, creature, 0.25f, 2.0f);
	// Damage is one less its life, hunger one less its energy, tiredness its exhaustion, each times 100
	EXPECT_EQ(figures.creatureDamage, 25.0f);
	EXPECT_EQ(figures.creatureHunger, 75.0f);
	EXPECT_EQ(figures.creatureTiredness, 12.5f);
	EXPECT_EQ(figures.creatureAge, 37u);
	EXPECT_EQ(figures.creatureAlignment, -0.5f);
	EXPECT_EQ(figures.creatureAlignmentChange, -0.75f);
	EXPECT_EQ(figures.creatureGrowth, 0.5f);
	EXPECT_EQ(figures.creature.poos, 2u);
	// Full of life and energy and not tired at all reads 0 for all three
	PlayerFigures fit;
	const CreatureNow rested {.needs = CreatureNeedsNow {.life = 1.0f, .energy = 1.0f, .exhaustion = 0.0f}};
	SetCreatureFigures(fit, rested, 0.0f, 0.0f);
	EXPECT_EQ(fit.creatureDamage, 0.0f);
	EXPECT_EQ(fit.creatureHunger, 0.0f);
	EXPECT_EQ(fit.creatureTiredness, 0.0f);
}

TEST(StatisticsCreature, APlayerWithoutACreatureShowsNothingForIt)
{
	PlayerFigures figures;
	SetCreatureFigures(figures, std::nullopt, 0.25f, 2.0f);
	EXPECT_EQ(figures.creatureDamage, 0.0f);
	EXPECT_EQ(figures.creatureHunger, 0.0f);
	EXPECT_EQ(figures.creatureTiredness, 0.0f);
	EXPECT_EQ(figures.creatureAge, 0u);
	EXPECT_EQ(figures.creatureAlignment, 0.0f);
	EXPECT_EQ(figures.creatureAlignmentChange, 0.0f);
	EXPECT_EQ(figures.creatureGrowth, 0.0f);
	EXPECT_EQ(figures.creature.peopleKilled, 0u);
	EXPECT_EQ(figures.creature.animalsKilled, 0u);
	EXPECT_EQ(figures.creature.creaturesKilled, 0u);
	EXPECT_EQ(figures.creature.battlesFought, 0u);
	EXPECT_EQ(figures.creature.battlesWon, 0u);
	EXPECT_EQ(figures.creature.poos, 0u);
	EXPECT_EQ(figures.creature.mushroomsEaten, 0u);
}

TEST(StatisticsRules, TheBoxCountsTheHumanAndComputerGodsUnderFourAndShowsThatManyFromPlayerOne)
{
	using enum PlayerNames;
	// The campaign: the human and two computer gods, the neutral player never
	const std::array<BoxPlayer, 4> campaign {{{PLAYER_ONE, 1}, {PLAYER_TWO, 2}, {PLAYER_THREE, 2}, {NEUTRAL, 3}}};
	EXPECT_EQ(PlayersShown(campaign), 3u);
	// A playground turning on only the third player: two shown, players one and two
	const std::array<BoxPlayer, 3> gap {{{PLAYER_ONE, 1}, {PLAYER_TWO, 0}, {PLAYER_THREE, 2}}};
	EXPECT_EQ(PlayersShown(gap), 2u);
	// A player a script turned off without ever turning it on is not counted
	const std::array<BoxPlayer, 2> off {{{PLAYER_ONE, 1}, {PLAYER_TWO, 0}}};
	EXPECT_EQ(PlayersShown(off), 1u);
	// A computer god past the fourth place is not counted
	const std::array<BoxPlayer, 2> fifth {{{PLAYER_ONE, 1}, {PLAYER_FIVE, 2}}};
	EXPECT_EQ(PlayersShown(fifth), 1u);
	// Four at most, and none without a player
	const std::array<BoxPlayer, 4> four {{{PLAYER_ONE, 1}, {PLAYER_TWO, 2}, {PLAYER_THREE, 2}, {PLAYER_FOUR, 2}}};
	EXPECT_EQ(PlayersShown(four), 4u);
	EXPECT_EQ(PlayersShown({}), 0u);
}
