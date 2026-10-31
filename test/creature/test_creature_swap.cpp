/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "Common/ModelInverseSquareRoot.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureSwap.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_swap;
using creature_mind::Senses;
using creature_mind::Step;

namespace
{
uint32_t NoRandom(uint32_t /*range*/)
{
	return 0;
}

creature_mind::IdleMind Swapping()
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, {Step {.kind = Step::Kind::SwapMind, .animation = 109}});
	return mind;
}

Senses SwapSenses(bool spellsGone, bool settled, bool bodyBusy = false)
{
	return {.seconds = 0.1f,
	        .bodyBusy = bodyBusy,
	        .swap = {.spellsGone = spellsGone, .settled = settled}};
}
} // namespace

// The breeder's five bonus creatures are there only once unlocked; every other species always is
TEST(CreatureSwap, OnlyTheBonusCreaturesNeedUnlocking)
{
	const Unlocked none;
	for (const int32_t bonus : {3, 6, 14, 15, 16})
	{
		EXPECT_FALSE(IsAvailable(bonus, none)) << bonus;
	}
	for (const int32_t other : {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 17, 99, -1})
	{
		EXPECT_TRUE(IsAvailable(other, none)) << other;
	}
}

TEST(CreatureSwap, AnUnlockedBonusCreatureIsAvailable)
{
	Unlocked unlocked;
	unlocked.set(static_cast<size_t>(Bonus::Rhino));
	EXPECT_TRUE(IsAvailable(15, unlocked));
	EXPECT_FALSE(IsAvailable(16, unlocked));
	EXPECT_EQ(BonusOf(3), Bonus::Leopard);
	EXPECT_EQ(BonusOf(6), Bonus::Horse);
	EXPECT_EQ(BonusOf(14), Bonus::Mandrill);
	EXPECT_EQ(BonusOf(16), Bonus::Gorilla);
	EXPECT_FALSE(BonusOf(11).has_value());
}

// The game reproduced has all five bonus creatures unlocked, so the breeder can offer every species
TEST(CreatureSwap, TheGameHasEveryBonusCreatureUnlocked)
{
	EXPECT_TRUE(k_GameUnlocks.all());
	for (const int32_t species : {0, 3, 6, 11, 14, 15, 16, 17, -1})
	{
		EXPECT_TRUE(IsAvailable(species, k_GameUnlocks)) << species;
	}
}

// Each turn the body closes 4% of the gap to its new size and alignment
TEST(CreatureSwap, EachTurnClosesAShareOfTheGap)
{
	const auto next = Ease({.size = 1.0f, .alignment = 0.0f}, {.size = 2.0f, .alignment = -1.0f});
	EXPECT_FLOAT_EQ(next.size, 1.04f);
	EXPECT_FLOAT_EQ(next.alignment, -0.04f);
}

// The share and the sum are each rounded to a float, not worked out at a higher precision and rounded once
TEST(CreatureSwap, EachStepOfTheEaseIsRoundedToAFloat)
{
	const auto now = std::bit_cast<float>(0x3E8E7A57u);
	const auto target = std::bit_cast<float>(0x3FBBA8C4u);
	const auto next = Ease({.size = now, .alignment = now}, {.size = target, .alignment = target});
	EXPECT_EQ(std::bit_cast<uint32_t>(next.size), 0x3EA6CDE4u);
	EXPECT_EQ(std::bit_cast<uint32_t>(next.alignment), 0x3EA6CDE4u);
	const auto shrunk = Ease({.size = std::bit_cast<float>(0x3FFBFCBEu), .alignment = 0.0f},
	                         {.size = std::bit_cast<float>(0x3F3B63AEu), .alignment = 0.0f});
	EXPECT_EQ(std::bit_cast<uint32_t>(shrunk.size), 0x3FF5A7D4u);
}

TEST(CreatureSwap, TheAlignmentStaysWithinItsRange)
{
	const auto next = Ease({.size = 1.0f, .alignment = 0.99f}, {.size = 1.0f, .alignment = 2.0f});
	EXPECT_FLOAT_EQ(next.alignment, 1.0f);
}

TEST(CreatureSwap, SettledWhenBothGapsAreUnderAHundredth)
{
	EXPECT_TRUE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.0099f, .alignment = 0.4901f}));
	EXPECT_FALSE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.011f, .alignment = 0.5f}));
	EXPECT_FALSE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.0f, .alignment = 0.489f}));
}

// The hundredth is the float one, a little under a true hundredth: a gap of exactly that much is not yet settled
TEST(CreatureSwap, AGapOfTheFloatHundredthIsNotYetSettled)
{
	EXPECT_FALSE(IsSettled({.size = 0.0f, .alignment = 0.0f}, {.size = 0.01f, .alignment = 0.0f}));
	EXPECT_FALSE(IsSettled({.size = 0.0f, .alignment = 0.0f}, {.size = 0.0f, .alignment = 0.01f}));
	EXPECT_TRUE(IsSettled({.size = 0.0f, .alignment = 0.0f}, {.size = 0.0099999f, .alignment = 0.0099999f}));
}

// Growing by a whole size takes about 113 turns, eleven seconds at ten turns a second
TEST(CreatureSwap, AWholeSizeTakesAboutElevenSeconds)
{
	Shape body {.size = 1.0f, .alignment = 0.0f};
	const Shape target {.size = 2.0f, .alignment = 0.0f};
	int turns = 0;
	while (!IsSettled(body, target) && turns < 1000)
	{
		body = Ease(body, target);
		++turns;
	}
	EXPECT_NEAR(turns, 113, 1);
}

// The mind keeps the same share of the sightings each miracle needs in its new species, rounded down
TEST(CreatureSwap, SightingsKeepTheirShareInTheNewSpecies)
{
	EXPECT_EQ(RescaledSightings(10, 10.0f, 15.0f), 15u);
	EXPECT_EQ(RescaledSightings(7, 10.0f, 5.0f), 3u);
	EXPECT_EQ(RescaledSightings(7, 0.0f, 5.0f), 7u);
	EXPECT_EQ(RescaledSightings(0, 10.0f, 5.0f), 0u);
}

// The swap waits for the spells on both creatures to end, bringing its own to their end meanwhile
TEST(CreatureSwap, TheSwapWaitsForTheSpellsToEnd)
{
	auto mind = Swapping();
	const auto commands = creature_mind::Think(mind, SwapSenses(false, false), NoRandom);
	EXPECT_TRUE(commands.finishSpells);
	EXPECT_FALSE(commands.playOnce.has_value());
	EXPECT_FALSE(mind.stepStarted);
}

// Then its action plays and the beam starts, once its body is free
TEST(CreatureSwap, TheActionAndTheBeamStartOnceTheBodyIsFree)
{
	auto mind = Swapping();
	auto commands = creature_mind::Think(mind, SwapSenses(true, false, true), NoRandom);
	EXPECT_FALSE(commands.playOnce.has_value());
	EXPECT_FALSE(commands.finishSpells);
	commands = creature_mind::Think(mind, SwapSenses(true, false), NoRandom);
	EXPECT_EQ(commands.playOnce, 109u);
	EXPECT_TRUE(commands.startSwapBeam);
	EXPECT_FALSE(commands.easeSwap);
}

// From the next turn it takes on its new shape, done once it has, whether or not the action has played out
TEST(CreatureSwap, ItEasesUntilSettled)
{
	auto mind = Swapping();
	static_cast<void>(creature_mind::Think(mind, SwapSenses(true, false), NoRandom));
	auto commands = creature_mind::Think(mind, SwapSenses(true, false, true), NoRandom);
	EXPECT_TRUE(commands.easeSwap);
	EXPECT_EQ(mind.step, 0u);
	commands = creature_mind::Think(mind, SwapSenses(true, true, true), NoRandom);
	EXPECT_FALSE(commands.easeSwap);
	EXPECT_EQ(mind.step, 1u);
}

// The beam of a creature with a player is in its player's colour, the neutral player's included; a creature of no
// player, as the player's old body is once the swap has given the player the other, takes the local player's
TEST(CreatureSwap, TheBeamTakesItsPlayersColourOrTheLocalPlayersWithNone)
{
	EXPECT_EQ(BeamPlayer(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(BeamPlayer(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE), PlayerNames::NEUTRAL);
	EXPECT_EQ(BeamPlayer(PlayerNames::_COUNT, PlayerNames::PLAYER_THREE), PlayerNames::PLAYER_THREE);
	EXPECT_TRUE(HasPlayer(PlayerNames::NEUTRAL));
	EXPECT_FALSE(HasPlayer(PlayerNames::_COUNT));
}

// The beam fires its plasma on every third game turn of the easing
TEST(CreatureSwap, ThePlasmaFiresEveryThirdGameTurn)
{
	EXPECT_TRUE(IsPlasmaTurn(0));
	EXPECT_FALSE(IsPlasmaTurn(1));
	EXPECT_FALSE(IsPlasmaTurn(2));
	EXPECT_TRUE(IsPlasmaTurn(3));
	EXPECT_TRUE(IsPlasmaTurn(300));
}

// From the creature's bone to the other's, along the line between them both ways, for a second and a half at speed 1,
// its alpha 40 more than the draw
TEST(CreatureSwap, ThePlasmaRunsAlongTheLineBetweenTheTwoBones)
{
	const glm::vec3 own {1.0f, 2.0f, 3.0f};
	const glm::vec3 other {4.0f, 2.0f, 7.0f};
	const auto plasma = SwapPlasma(own, other, 34);
	EXPECT_EQ(plasma.start, own);
	EXPECT_EQ(plasma.end, other);
	const float inverse = gutils::ModelInverseSquareRoot(25.0f);
	EXPECT_EQ(plasma.startTangent, glm::vec3(3.0f * inverse, 0.0f, 4.0f * inverse));
	EXPECT_EQ(plasma.endTangent, plasma.startTangent);
	EXPECT_EQ(plasma.life, 1.5f);
	EXPECT_EQ(plasma.speed, 1.0f);
	EXPECT_EQ(plasma.alpha, 74);
	EXPECT_EQ(SwapPlasma(own, other, 0).alpha, 40);
}

// Bones nearer than a hundredth have no line between them: the plasma leaves and arrives straight up
TEST(CreatureSwap, ThePlasmaBetweenBonesTooNearGoesStraightUp)
{
	const auto plasma = SwapPlasma({1.0f, 1.0f, 1.0f}, {1.005f, 1.0f, 1.0f}, 0);
	const float inverse = gutils::ModelInverseSquareRoot(1.0f);
	EXPECT_EQ(plasma.startTangent, glm::vec3(0.0f, inverse, 0.0f));
	EXPECT_EQ(plasma.endTangent, glm::vec3(0.0f, inverse, 0.0f));
}
