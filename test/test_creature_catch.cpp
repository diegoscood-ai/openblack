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

#include <gtest/gtest.h>

#include "Creature/CreatureCatch.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_catch;

namespace
{
Approach Coming(glm::vec3 velocity)
{
	// A creature of size 1 plays at 1.175 times; a step of 1175 ms is then 1 s, closing at 1175 ms is 1 s in
	return {.thing = {0.0f, 10.0f, 20.0f},
	        .velocity = velocity,
	        .creature = {0.0f, 0.0f, 0.0f},
	        .size = 1.0f,
	        .modelScale = 1.0f,
	        .catchMs = 1175.0f,
	        .stepMs = 1175.0f,
	        .stepTravel = -10.0f};
}
} // namespace

TEST(CreatureCatch, ItReachesWhatPassesNearInTime)
{
	ASSERT_FLOAT_EQ(creature_layers::PlaybackRate(1.0f), 1.175f);
	// Passing straight through it in 4 s, with 3 s to spare: it reaches (0.5 + 3) × 10 = 35 m
	EXPECT_TRUE(Reaches(Coming({0.0f, 0.0f, -5.0f})));
	// Too slow across the land
	EXPECT_FALSE(Reaches(Coming({0.0f, -20.0f, -0.9f})));
	// Passing closest too late
	EXPECT_FALSE(Reaches(Coming({0.0f, 0.0f, -3.0f})));
	// Flying away: it passed closest in the past
	EXPECT_FALSE(Reaches(Coming({0.0f, 0.0f, 5.0f})));
}

/// The original's own answers to the reach test, from the game's function run on these inputs (each a float's bits): the
/// thing's place and flight across the land, the creature's place, size and model scale, the moment the hand closes and
/// the step's length in milliseconds, the step's travel across, and whether it reaches. The first twelve lie at the
/// edge of the reach, where working the sums out in another order gives the other answer
struct ReachCase
{
	uint32_t thingX, thingZ, velocityX, velocityZ, creatureX, creatureZ, size, modelScale;
	int32_t catchMs, stepMs;
	uint32_t stepTravel;
	bool reaches;
};
constexpr ReachCase k_ReachCases[] = {
    {0xC1A30E5E, 0x425070D3, 0x413A63E5, 0xC02237BF, 0xBEBF5FCA, 0x3ED3359A, 0x3F9744BC, 0x3FE4A667, 735, 1531, 0xC15F29DF,
     true},
    {0xC2211CA2, 0x3FDA57A6, 0x41C37637, 0xC169BF45, 0x3FDB4325, 0x409187F4, 0x3FE3BA2A, 0x40018270, 723, 589, 0xC13B4096,
     true},
    {0xC261E191, 0xC1E62FA8, 0x414205BD, 0x41E51834, 0xC094999D, 0xC05C761E, 0x3EA3E633, 0x3F9AE7EB, 827, 800, 0xC176C0F0,
     true},
    {0xC269A455, 0x41C0FD28, 0x41A53F05, 0xC198FDA4, 0xBEEBC55F, 0x40192C85, 0x3F7D2FFE, 0x3F7CE16E, 438, 494, 0xC0A9A37A,
     true},
    {0x3F03BF89, 0x4267AFC0, 0x41925074, 0xC16810FB, 0x40839D9E, 0x401C7037, 0x3FCFB0B0, 0x402257BA, 930, 572, 0xC1378FA3,
     true},
    {0xC06F4E49, 0xC26AEA23, 0xC10A969B, 0x410529E1, 0x3F9EBEF7, 0xC02B738F, 0x3FF3F620, 0x400A9331, 791, 742, 0xC0FD79D2,
     true},
    {0x425BDC40, 0x4226B56D, 0xC1EFBD0A, 0xC18B55ED, 0x4083497A, 0xBE99AA47, 0x3FFBB9E2, 0x3FBF2D02, 249, 1459, 0xC0AF38A6,
     false},
    {0xC2025EA8, 0x40E3F77D, 0x41A926E6, 0x40DB7638, 0xC00CA8DB, 0x40858E25, 0x3F259255, 0x3F0A9B9D, 651, 1142, 0xC1815C50,
     false},
    {0x41BFCC98, 0xC204845D, 0xC0ABDFC8, 0x41BC78B6, 0xBF8F6546, 0xBEFD6CC8, 0x3F227B13, 0x402DF8DE, 113, 1071, 0xC02EEE58,
     false},
    {0x420E247E, 0x41E54776, 0xC0FCF705, 0xC0D2BA53, 0xC05CEFC6, 0x3F695258, 0x3F19F4B5, 0x3F396EF2, 1030, 876, 0xBF475EA2,
     false},
    {0xC2689884, 0xC13EA3F5, 0x413FE2AD, 0x41E75626, 0x403963B3, 0x3FCD17FA, 0x3FAAD50D, 0x3F0BDDD7, 777, 686, 0xC2AB9752,
     false},
    {0x423F492D, 0xC1B8772E, 0xBDC266EB, 0x41D04A16, 0x4098BAB3, 0xBE8C1453, 0x3F26AC1F, 0x3F9E82F0, 220, 700, 0xC195EACC,
     false},
    {0xC1353C63, 0x425C2EA5, 0x41C2B0F8, 0xC1AFE6E1, 0xC081A6C7, 0xC081F088, 0x3EAFA2CC, 0x401DC881, 247, 543, 0xC15475FB,
     true},
    {0x4149F569, 0x41EBFA08, 0xC17513D0, 0xC1A50967, 0x4010BB4A, 0x409CF46F, 0x3FD84F6C, 0x4037B6F8, 301, 1417, 0xC1439169,
     true},
    {0xC10C12FE, 0x420EDB34, 0xC0C2096A, 0xC12599E1, 0xC02E37E6, 0x4084B19F, 0x3F000000, 0x3FD9819E, 1107, 1063, 0xC167C216,
     true},
    {0x417890D8, 0x4232A888, 0xC0FE099E, 0xC1CC0EF7, 0x3F4CD934, 0xBF825D9E, 0x3F8669B8, 0x3F8CDF6C, 115, 1414, 0xC0588E65,
     true},
    {0xC220D302, 0x41B62D1F, 0x41019996, 0xBFA08225, 0xC035C036, 0x403B490D, 0x3F800000, 0x3FE1A07E, 583, 1914, 0xBF0FE0B1,
     false},
    {0xC1779721, 0x4123E560, 0xC1CEC0BF, 0x418D0020, 0xC02B5BB7, 0xC02B1342, 0x3EBE72E1, 0x40028ADC, 169, 973, 0xC13086CF,
     false},
    {0x41BA28A6, 0x418EDD11, 0x41D23278, 0xC1E4DA37, 0x401A4102, 0xC083B10A, 0x3F000000, 0x3FD10036, 1230, 1791, 0xC0E5FBA4,
     false},
    {0xBFEAF3BD, 0xC21648A7, 0xC1DE2C4B, 0x41AFA005, 0xC029AF89, 0x40335DB1, 0x3F000000, 0x3F4E62C6, 1075, 1444, 0xC18503CB,
     false},
};

TEST(CreatureCatch, ItReachesAsTheOriginalWorksItOut)
{
	for (const auto& c : k_ReachCases)
	{
		const Approach approach {.thing = {std::bit_cast<float>(c.thingX), 0.0f, std::bit_cast<float>(c.thingZ)},
		                         .velocity = {std::bit_cast<float>(c.velocityX), 0.0f, std::bit_cast<float>(c.velocityZ)},
		                         .creature = {std::bit_cast<float>(c.creatureX), 0.0f, std::bit_cast<float>(c.creatureZ)},
		                         .size = std::bit_cast<float>(c.size),
		                         .modelScale = std::bit_cast<float>(c.modelScale),
		                         .catchMs = static_cast<float>(c.catchMs),
		                         .stepMs = static_cast<float>(c.stepMs),
		                         .stepTravel = std::bit_cast<float>(c.stepTravel)};
		EXPECT_EQ(Reaches(approach), c.reaches) << std::hex << c.thingX << " " << c.stepTravel;
	}
}

TEST(CreatureCatch, WhoMayTry)
{
	EXPECT_TRUE(MayTry({}));
	EXPECT_FALSE(MayTry({.heldByHand = true}));
	EXPECT_FALSE(MayTry({.plansToFight = true}));
	EXPECT_FALSE(MayTry({.fighting = true}));
	EXPECT_FALSE(MayTry({.scriptControlled = true}));
	EXPECT_FALSE(MayTry({.plansToCatch = true}));
	// Not free to react, it doesn't, unless a script gave it a desire to follow alone
	EXPECT_FALSE(MayTry({.availableForReaction = false}));
	EXPECT_TRUE(MayTry({.availableForReaction = false, .onlyDesire = true}));
	EXPECT_FALSE(MayTry({.availableForReaction = false, .onlyDesire = true, .plansToCatch = true}));
}

TEST(CreatureCatch, WeightDrawAndSpeedToStart)
{
	EXPECT_FLOAT_EQ(MostWeight(1000.0f), 800.0f);
	// Another player's throw: a draw of 0, 1 or 2 out of a hundred
	EXPECT_TRUE(GoesForOtherPlayersThrow(0));
	EXPECT_TRUE(GoesForOtherPlayersThrow(2));
	EXPECT_FALSE(GoesForOtherPlayersThrow(3));
	EXPECT_FALSE(GoesForOtherPlayersThrow(99));
	// Across and up or down, not along the other axis
	EXPECT_TRUE(FastEnoughToStart({0.0f, 1.0f, 0.0f}));
	EXPECT_TRUE(FastEnoughToStart({0.8f, -0.8f, 0.0f}));
	EXPECT_FALSE(FastEnoughToStart({0.0f, 0.5f, 30.0f}));
}

TEST(CreatureCatch, TheTurnsOfTheStepAndTheCatch)
{
	// The hand closes 1175 ms in for a creature of size 1, whose animations play at 1.175 times: a second
	EXPECT_FLOAT_EQ(LeadSeconds(1175.0f, 1.0f), 1.0f);
	// A looping clock goes back round once past its end, and never reaches it
	EXPECT_EQ(AdvanceCyclic(100, 117, 1000), 217);
	EXPECT_EQ(AdvanceCyclic(950, 117, 1000), 67);
	EXPECT_EQ(AdvanceCyclic(950, 2117, 1000), 999);
	EXPECT_EQ(AdvanceCyclic(950, 0, 1000), 950);
	// The step ends in the turn its clock would reach the end
	EXPECT_FALSE(StepEnds(882, 117, 1000));
	EXPECT_TRUE(StepEnds(883, 117, 1000));
	// The catch played on ends the same way; drawn back, once less than a turn's step is left
	EXPECT_TRUE(CatchEnds(false, 883, 117, 1000));
	EXPECT_FALSE(CatchEnds(true, 117, 117, 1000));
	EXPECT_TRUE(CatchEnds(true, 116, 117, 1000));
	EXPECT_EQ(CatchClock(false, 950, 117, 1000), 67);
	EXPECT_EQ(CatchClock(true, 100, 117, 1000), 0);
	EXPECT_EQ(CatchClock(true, 300, 117, 1000), 183);
}

TEST(CreatureCatch, ItMissesWhatPassesBeyondItsReach)
{
	// Passing 40 m to the side
	auto approach = Coming({0.0f, 0.0f, -5.0f});
	approach.thing.x = 40.0f;
	EXPECT_FALSE(Reaches(approach));
	approach.thing.x = 30.0f;
	EXPECT_TRUE(Reaches(approach));
}

TEST(CreatureCatch, TheCatchingAnimationsBlendByHeightAndSide)
{
	// The hand closes low and to one side, low and to the other, high and to one side, high and to the other
	const std::array<glm::vec3, 4> hands {glm::vec3(2.0f, 1.0f, 0.0f), glm::vec3(-2.0f, 1.0f, 0.0f),
	                                      glm::vec3(2.0f, 5.0f, 0.0f), glm::vec3(-2.0f, 5.0f, 0.0f)};
	const auto low = Weigh({2.0f, 1.0f, -3.0f}, hands, 1.0f, false);
	EXPECT_FALSE(low.clamped);
	EXPECT_FLOAT_EQ(low.weights[0], 1.0f);
	const auto high = Weigh({-2.0f, 5.0f, -3.0f}, hands, 1.0f, false);
	EXPECT_FLOAT_EQ(high.weights[3], 1.0f);
	const auto middle = Weigh({0.0f, 3.0f, -3.0f}, hands, 1.0f, false);
	for (const float weight : middle.weights)
	{
		EXPECT_FLOAT_EQ(weight, 0.25f);
	}
	// Far above the highest it leans past its limit and misses
	EXPECT_TRUE(Weigh({0.0f, 20.0f, -3.0f}, hands, 1.0f, false).clamped);
}

TEST(CreatureCatch, ReadyItWaitsStepsCatchesOrGivesUp)
{
	// The hand closes 1 to the right in the high corner and 1 to the left in the other, at a model scale of 1: a step is
	// needed beyond 1 + 1.2 × (−1 − 1) = −1.4 to either side
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 0.0f),
	                                      glm::vec3(-1.0f, 2.0f, 0.0f)};
	const float lead = 0.5f;
	// Ahead at 10, closing at 10 a second: due in a second, so it waits
	EXPECT_EQ(ReadyToCatch({0.5f, 2.0f, -10.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead).readiness, Readiness::Wait);
	// Due in half a second, as the hand closes: it catches, with the left hand when the thing is to its left
	const auto now = ReadyToCatch({0.5f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead);
	EXPECT_EQ(now.readiness, Readiness::Catch);
	EXPECT_TRUE(now.mirrored);
	EXPECT_FALSE(ReadyToCatch({-0.5f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead).mirrored);
	// Too late
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -2.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead).readiness, Readiness::GiveUp);
	// Behind it
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead).readiness, Readiness::GiveUp);
	// Passing out of reach: it steps across
	const auto step = ReadyToCatch({3.0f, 2.0f, -10.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, lead);
	EXPECT_EQ(step.readiness, Readiness::Step);
	EXPECT_TRUE(step.mirrored);
}

TEST(CreatureCatch, ReadyReachOnTheNearSideAlwaysSteps)
{
	// The hand closes 1 to the left in the high corner and 1 to the right in the other: the reach, 1 + 1.2 × 2 = 3.4,
	// comes out on the near side, so even a thing straight ahead is taken a step for
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(-1.0f, 2.0f, 0.0f),
	                                      glm::vec3(1.0f, 2.0f, 0.0f)};
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, hands, 1.0f, 0.5f).readiness, Readiness::Step);
}

TEST(CreatureCatch, ReadyWithNoClosingSpeed)
{
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 0.0f),
	                                      glm::vec3(-1.0f, 2.0f, 0.0f)};
	// Ahead and not coming nearer: it never arrives, so the creature waits
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -5.0f}, {5.0f, 0.0f, 0.0f}, hands, 1.0f, 0.5f).readiness, Readiness::Wait);
	// Level with it and not coming nearer: when it arrives can't be told, and it gives up
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, hands, 1.0f, 0.5f).readiness, Readiness::GiveUp);
}

TEST(CreatureCatch, StepMoveCarriesItsShareOfTheTravel)
{
	// A step of 4 across and 1 up over 400 ms, a 100 ms frame at a model scale of 2, facing the world's axes
	const glm::mat3 facing {1.0f};
	const auto move = StepMove({4.0f, 1.0f, 0.0f}, facing, 2.0f, 100.0f, 400.0f, false);
	EXPECT_FLOAT_EQ(move.x, 2.0f);
	EXPECT_FLOAT_EQ(move.y, 0.5f);
	EXPECT_FLOAT_EQ(move.z, 0.0f);
	// With the left hand the whole move goes the other way, height included
	const auto mirrored = StepMove({4.0f, 1.0f, 2.0f}, facing, 2.0f, 100.0f, 400.0f, true);
	EXPECT_FLOAT_EQ(mirrored.x, -2.0f);
	EXPECT_FLOAT_EQ(mirrored.y, -0.5f);
	EXPECT_FLOAT_EQ(mirrored.z, -1.0f);
	// Turned a quarter round, the step across goes along the other axis
	const glm::mat3 quarter {glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f)};
	const auto turned = StepMove({4.0f, 0.0f, 0.0f}, quarter, 1.0f, 400.0f, 400.0f, false);
	EXPECT_NEAR(turned.x, 0.0f, 1e-6f);
	EXPECT_FLOAT_EQ(turned.z, 4.0f);
}
