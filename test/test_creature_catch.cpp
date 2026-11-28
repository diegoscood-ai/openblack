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
#include <numbers>

#include <gtest/gtest.h>

#include "Creature/CreatureCatch.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::creature_catch;

namespace
{
/// A creature at the world's middle facing along negative z, the way a heading of 0 faces
constexpr Stance k_Facing {.position = {0.0f, 0.0f, 0.0f}, .heading = 0.0f};

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

// The game's world asks the creature's kept dominant desire: one made dominant over the others for a while lets a
// creature not free to react set about a catch all the same
TEST(CreatureCatch, ADesireMadeDominantCountsAsTheOnlyDesire)
{
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	auto& mind = registry.Assign<ecs::components::CreatureMindState>(creature);
	ecs::GamePhysicsHooksWorld world;
	EXPECT_FALSE(world.CatchEligibility(creature).onlyDesire);
	mind.dominantDesire.desire = creature_desires::Desire::Anger;
	EXPECT_TRUE(world.CatchEligibility(creature).onlyDesire);
	mind.dominantDesire.desire.reset();
	EXPECT_FALSE(world.CatchEligibility(creature).onlyDesire);
	Locator::entitiesRegistry::reset();
}

// The game's world asks a thrown thing only its cannot-be-picked-up flag: a beach temple puzzle piece's own refusal is
// the hand's alone, so a piece of no puzzle, which the hand refuses, may be caught until the flag is set
TEST(CreatureCatch, APuzzlePiecesOwnRefusalDoesNotStopTheCatch)
{
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto ring = registry.Create();
	registry.Assign<ecs::components::MobileObject>(ring, MobileObjectInfo::HanoiPuzzlePart1);
	registry.AssignState<ecs::components::HanoiBlock>(ring);
	ecs::GamePhysicsHooksWorld world;
	EXPECT_TRUE(ecs::object_flags::IsCannotBePickedUp(ring));
	EXPECT_TRUE(world.MayBeCaught(ring));
	ecs::object_flags::SetPickupable(ring, false);
	EXPECT_FALSE(world.MayBeCaught(ring));
	Locator::entitiesRegistry::reset();
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
	const auto low = Weigh({2.0f, 1.0f, -3.0f}, k_Facing, hands, 1.0f, false);
	EXPECT_FALSE(low.clamped);
	EXPECT_FLOAT_EQ(low.weights[0], 1.0f);
	const auto high = Weigh({-2.0f, 5.0f, -3.0f}, k_Facing, hands, 1.0f, false);
	EXPECT_FLOAT_EQ(high.weights[3], 1.0f);
	const auto middle = Weigh({0.0f, 3.0f, -3.0f}, k_Facing, hands, 1.0f, false);
	for (const float weight : middle.weights)
	{
		EXPECT_FLOAT_EQ(weight, 0.25f);
	}
	// Far above the highest it leans past its limit and misses
	EXPECT_TRUE(Weigh({0.0f, 20.0f, -3.0f}, k_Facing, hands, 1.0f, false).clamped);
}

TEST(CreatureCatch, ReadyItWaitsStepsCatchesOrGivesUp)
{
	// The hand closes 1 to the right in the high corner and 1 to the left in the other, at a model scale of 1: a step is
	// needed beyond 1 + 1.2 × (−1 − 1) = −1.4 to either side
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 0.0f),
	                                      glm::vec3(-1.0f, 2.0f, 0.0f)};
	const float lead = 0.5f;
	// Ahead at 10, closing at 10 a second: due in a second, so it waits
	EXPECT_EQ(ReadyToCatch({0.5f, 2.0f, -10.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead).readiness, Readiness::Wait);
	// Due in half a second, as the hand closes: it catches, with the left hand when the thing is to its left
	const auto now = ReadyToCatch({0.5f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead);
	EXPECT_EQ(now.readiness, Readiness::Catch);
	EXPECT_TRUE(now.mirrored);
	EXPECT_FALSE(ReadyToCatch({-0.5f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead).mirrored);
	// Too late
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -2.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead).readiness, Readiness::GiveUp);
	// Behind it
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead).readiness, Readiness::GiveUp);
	// Passing out of reach: it steps across
	const auto step = ReadyToCatch({3.0f, 2.0f, -10.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, lead);
	EXPECT_EQ(step.readiness, Readiness::Step);
	EXPECT_TRUE(step.mirrored);
}

TEST(CreatureCatch, ReadyReachOnTheNearSideAlwaysSteps)
{
	// The hand closes 1 to the left in the high corner and 1 to the right in the other: the reach, 1 + 1.2 × 2 = 3.4,
	// comes out on the near side, so even a thing straight ahead is taken a step for
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(-1.0f, 2.0f, 0.0f),
	                                      glm::vec3(1.0f, 2.0f, 0.0f)};
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -5.0f}, {0.0f, 0.0f, 10.0f}, k_Facing, hands, 1.0f, 0.5f).readiness, Readiness::Step);
}

TEST(CreatureCatch, ReadyWithNoClosingSpeed)
{
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 0.0f),
	                                      glm::vec3(-1.0f, 2.0f, 0.0f)};
	// Ahead and not coming nearer: it never arrives, so the creature waits
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, -5.0f}, {5.0f, 0.0f, 0.0f}, k_Facing, hands, 1.0f, 0.5f).readiness, Readiness::Wait);
	// Level with it and not coming nearer: when it arrives can't be told, and it gives up
	EXPECT_EQ(ReadyToCatch({0.0f, 2.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, k_Facing, hands, 1.0f, 0.5f).readiness, Readiness::GiveUp);
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

TEST(CreatureCatch, TheSideAndAheadAreAlongTheHeadingsSines)
{
	// Turned a quarter round, ahead is along x and the right along negative z
	const Stance turned {.position = {10.0f, 0.0f, 20.0f}, .heading = std::numbers::pi_v<float> / 2.0f};
	EXPECT_NEAR(AheadOf({15.0f, 3.0f, 20.0f}, turned), 5.0f, 1e-5f);
	EXPECT_NEAR(RightOf({15.0f, 3.0f, 20.0f}, turned), 0.0f, 1e-5f);
	EXPECT_NEAR(RightOf({10.0f, 3.0f, 17.0f}, turned), 3.0f, 1e-5f);
	EXPECT_NEAR(AheadOf({10.0f, 3.0f, 17.0f}, turned), 0.0f, 1e-5f);
	// The height is over the creature's feet, whatever the heading
	const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 5.0f, 0.0f),
	                                      glm::vec3(-1.0f, 5.0f, 0.0f)};
	const Stance raised {.position = {0.0f, 100.0f, 0.0f}, .heading = 1.0f};
	EXPECT_FLOAT_EQ(Weigh({0.0f, 103.0f, 0.0f}, raised, hands, 1.0f, false).height, 0.5f);
}

TEST(CreatureCatch, AWeightThatCantBeMeasuredIsKeptAtTheLowerLimit)
{
	// The high and low hands at one height: the height is 0 over 0, kept at the lower limit, and missed
	const std::array<glm::vec3, 4> level {glm::vec3(0.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 2.0f, 0.0f),
	                                      glm::vec3(-1.0f, 2.0f, 0.0f)};
	const auto blend = Weigh({0.0f, 2.0f, -3.0f}, k_Facing, level, 1.0f, false);
	EXPECT_TRUE(blend.clamped);
	EXPECT_EQ(blend.height, k_LeastWeight);
}

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
float Float(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}
glm::vec3 Point(const std::array<uint32_t, 3>& bits)
{
	return {Float(bits[0]), Float(bits[1]), Float(bits[2])};
}

/// The original's answers while it waits for a thing, on random cases: its heading, where it and the thing are, how
/// the thing flies, the two high corners' hands across, its size and drawn scale, when the hand closes, and what it does
struct ReadyCase
{
	uint32_t heading;
	std::array<uint32_t, 3> creature;
	std::array<uint32_t, 3> thing;
	std::array<uint32_t, 3> velocity;
	uint32_t highSideX;
	uint32_t highOtherX;
	uint32_t size;
	uint32_t modelScale;
	int32_t catchMs;
	Readiness readiness;
	bool mirrored;
};
constexpr std::array<ReadyCase, 12> k_ReadyCases {{
    {0x3FEC85E9u,
     {0x444D23D2u, 0x430EE73Cu, 0x43DC7442u},
     {0x444F0933u, 0x432779A8u, 0x43DD9C19u},
     {0xC1B74EA9u, 0xC0901E3Au, 0xC0A211CAu},
     0x3FA3B644u,
     0xC0047CD2u,
     0x3F43D703u,
     0x3FEA1D4Du,
     414,
     Readiness::Catch,
     true},
    {0x3E1A981Bu,
     {0xC4E465EBu, 0x4304524Du, 0x448091AAu},
     {0xC4E72D59u, 0x43219170u, 0x4478DA3Du},
     {0xC180DF61u, 0xC0CEDAA6u, 0x40AA599Cu},
     0xC002F1CDu,
     0x4022011Eu,
     0x3F50FC88u,
     0x3F26FDF4u,
     1285,
     Readiness::Step,
     false},
    {0xBF809F6Eu,
     {0x446A24A0u, 0x41CC6620u, 0x4316477Bu},
     {0x446932B4u, 0x41F681F2u, 0x42E5504Eu},
     {0x4078141Eu, 0x40F6CE33u, 0x3DD2A6B3u},
     0x3FDBE198u,
     0xBFB6F4ABu,
     0x3F1286C3u,
     0x3F4DCA1Du,
     1108,
     Readiness::Step,
     true},
    {0x3EF659FEu,
     {0x43D74CDBu, 0x42D5DA73u, 0xC4DCC638u},
     {0x43DECA4Eu, 0x42E18F82u, 0xC4DF1469u},
     {0x40EEB4C7u, 0xC0CAEBE3u, 0xBFF03F73u},
     0x3E290E8Au,
     0xC07E8002u,
     0x3F7D5BA6u,
     0x3FC43FF4u,
     655,
     Readiness::GiveUp,
     false},
    {0xBF2BF4BAu,
     {0x43BF081Au, 0x41BD465Bu, 0xC34FFEDCu},
     {0x43AD374Eu, 0x424CFFFEu, 0xC369DE6Du},
     {0x410FCB44u, 0x40DF8566u, 0x40C1B3D7u},
     0x4053D1F8u,
     0x3FA0291Du,
     0x3F9BB63Au,
     0x4013958Du,
     1186,
     Readiness::Step,
     false},
    {0x40455B00u,
     {0x44A167D4u, 0x413EED04u, 0xC4E6D9B9u},
     {0x44A3E06Cu, 0x410B0668u, 0xC4E70C65u},
     {0xC1041A15u, 0xC1131EF8u, 0xC0AE2D78u},
     0xBEDF4E4Fu,
     0xC0623327u,
     0x3F23852Bu,
     0x3FCB70BEu,
     702,
     Readiness::GiveUp,
     false},
    {0xBD4AF62Du,
     {0xC493BC36u, 0x42F73072u, 0xC4359680u},
     {0xC493E134u, 0x4309BBADu, 0xC438B666u},
     {0x402BF09Bu, 0xC0EDD904u, 0x41850C7Du},
     0x402540B1u,
     0xC01C1EBDu,
     0x3EFA36C2u,
     0x40013F00u,
     898,
     Readiness::Wait,
     false},
    {0x40149E1Fu,
     {0xC47FF6E5u, 0x433EB776u, 0xC4CB72EFu},
     {0xC480BBE0u, 0x434DD15Au, 0xC4C74833u},
     {0xC05C9021u, 0xC0EB1E4Du, 0x416866FDu},
     0xBF76F3BFu,
     0x3F4274F8u,
     0x3FB5D992u,
     0x3ECEA720u,
     443,
     Readiness::Step,
     true},
    {0x3FAD0A01u,
     {0x4481C951u, 0x432CF364u, 0x44A8A2B7u},
     {0x4482E65Cu, 0x43380444u, 0x44A84539u},
     {0xC09A4C04u, 0x40D8CC62u, 0x3F2F5C9Du},
     0x3FB7A621u,
     0xC03D64F3u,
     0x3FB5CBF0u,
     0x3F595369u,
     1333,
     Readiness::Wait,
     false},
    {0xBFC34A3Au,
     {0xC49EEB25u, 0x4312F7B9u, 0xC4F0F9B8u},
     {0xC4A0D188u, 0x432CBCA4u, 0xC4F12D61u},
     {0x4181ACDBu, 0xC04250CAu, 0xBDA3B3B0u},
     0x3F8134CFu,
     0xC0011DC2u,
     0x3F9447CEu,
     0x3F162C38u,
     936,
     Readiness::Catch,
     true},
    {0x3FFF4420u,
     {0xC4649788u, 0x42ED38B7u, 0x43C83855u},
     {0xC463855Eu, 0x430D78EFu, 0x43C92E9Fu},
     {0xC10A3C1Du, 0x40C436D7u, 0xC0A5B71Eu},
     0x401275A5u,
     0xBFE430D4u,
     0x3F966389u,
     0x401E2373u,
     501,
     Readiness::Catch,
     false},
    {0x403D91A0u,
     {0xC433852Du, 0x42A2D7E9u, 0xC36DCEDDu},
     {0xC432E09Au, 0x42D21BFDu, 0xC362E302u},
     {0xC00D1129u, 0x403C6364u, 0xC1685B47u},
     0x3FE986F9u,
     0xC03B6E41u,
     0x3EF2BFCFu,
     0x3F4D95C7u,
     958,
     Readiness::Catch,
     false},
}};

/// The original's blend as it reaches, on random cases: its heading, where it and the thing are, the low and high
/// corners' hands up, the two high corners' hands across, its drawn scale, the left hand, and the height and side kept
struct BlendCase
{
	uint32_t heading;
	std::array<uint32_t, 3> creature;
	std::array<uint32_t, 3> thing;
	uint32_t lowOtherY;
	uint32_t highOtherY;
	uint32_t highSideX;
	uint32_t highOtherX;
	uint32_t modelScale;
	bool mirrored;
	uint32_t height;
	uint32_t side;
};
constexpr std::array<BlendCase, 8> k_BlendCases {{
    {0xBD4AF62Du,
     {0xC493BC36u, 0x42F73072u, 0xC4359680u},
     {0xC493E134u, 0x4309BBADu, 0xC438B666u},
     0x4101F70Eu,
     0x40CC1093u,
     0x402540B1u,
     0xC01C1EBDu,
     0x40013F00u,
     false,
     0x3F247CFEu,
     0x3F112825u},
    {0x40123B42u,
     {0xC4B4C68Au, 0x410BAF6Cu, 0xC35F5F00u},
     {0xC4B2547Au, 0x41A5822Cu, 0xC34E610Cu},
     0x41022BB1u,
     0x40960490u,
     0x4035219Bu,
     0xC0234459u,
     0x3FC9B5A9u,
     false,
     0x3E228C05u,
     0x3F05CC8Fu},
    {0xBF12C395u,
     {0x43C2BF1Fu, 0x42216C58u, 0xC3B1DA6Cu},
     {0x43BAC960u, 0x4243D4CEu, 0xC3BE56F5u},
     0x4034B1D9u,
     0x40FC6F8Au,
     0x3FC7C07Cu,
     0xC036D2AFu,
     0x3FB29808u,
     false,
     0x3F28E536u,
     0x3EA6B141u},
    {0xBFEF18A5u,
     {0xC4A414EDu, 0x43188696u, 0xC3E45441u},
     {0xC4A67335u, 0x43268083u, 0xC3E1A9FBu},
     0x40E02692u,
     0x3F0123F2u,
     0x401B7522u,
     0xBFFD596Au,
     0x40070125u,
     true,
     0x3D6EE82Du,
     0x3F1976ABu},
    {0xBFA25059u,
     {0xC477FBE8u, 0x42FD3400u, 0x41E65EDDu},
     {0xC47A8EF0u, 0x430D93C4u, 0x41C6B870u},
     0x4103C75Cu,
     0x3F57BB87u,
     0x3FC6F0F8u,
     0xBFDECD9Cu,
     0x3FE2D6F8u,
     true,
     0xBCED5912u,
     0x3F17BC0Cu},
    {0x3FB63054u,
     {0x4492A6DFu, 0x42D4C389u, 0x43C83C5Cu},
     {0x449278C0u, 0x42E945BBu, 0x43C794CAu},
     0x40C3E5ABu,
     0x40A12E6Bu,
     0x407806CAu,
     0xBECD37A9u,
     0x3FF0EE6Fu,
     false,
     0x3F1F0A23u,
     0x3F8BF662u},
    {0x3FEC85E9u,
     {0x444D23D2u, 0x430EE73Cu, 0x43DC7442u},
     {0x444F0933u, 0x432779A8u, 0x43DD9C19u},
     0x403E4A4Cu,
     0x4105DAD2u,
     0x3FA3B644u,
     0xC0047CD2u,
     0x3FEA1D4Du,
     false,
     0x3F99999Au,
     0x3EB71732u},
    {0x3E1A981Bu,
     {0xC4E465EBu, 0x4304524Du, 0x448091AAu},
     {0xC4E72D59u, 0x43219170u, 0x4478DA3Du},
     0x40BD5DC3u,
     0x40860199u,
     0xC002F1CDu,
     0x4022011Eu,
     0x3F26FDF4u,
     false,
     0xBE4CCCCDu,
     0xBE4CCCCDu},
}};
} // namespace

TEST(CreatureCatch, ItWaitsStepsAndCatchesAsTheOriginalWorksItOut)
{
	for (const auto& c : k_ReadyCases)
	{
		const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(Float(c.highSideX), 0.0f, 0.0f),
		                                      glm::vec3(Float(c.highOtherX), 0.0f, 0.0f)};
		const Stance stance {.position = Point(c.creature), .heading = Float(c.heading)};
		const float lead = LeadSeconds(static_cast<float>(c.catchMs), Float(c.size));
		const auto ready = ReadyToCatch(Point(c.thing), Point(c.velocity), stance, hands, Float(c.modelScale), lead);
		EXPECT_EQ(ready.readiness, c.readiness);
		if (c.readiness == Readiness::Step || c.readiness == Readiness::Catch)
		{
			EXPECT_EQ(ready.mirrored, c.mirrored);
		}
	}
}

TEST(CreatureCatch, TheBlendIsTheOriginalsToTheBit)
{
	for (const auto& c : k_BlendCases)
	{
		const std::array<glm::vec3, 4> hands {glm::vec3(0.0f), glm::vec3(0.0f, Float(c.lowOtherY), 0.0f),
		                                      glm::vec3(Float(c.highSideX), 0.0f, 0.0f),
		                                      glm::vec3(Float(c.highOtherX), Float(c.highOtherY), 0.0f)};
		const Stance stance {.position = Point(c.creature), .heading = Float(c.heading)};
		const auto blend = Weigh(Point(c.thing), stance, hands, Float(c.modelScale), c.mirrored);
		EXPECT_EQ(Bits(blend.height), c.height);
		EXPECT_EQ(Bits(blend.side), c.side);
	}
}

TEST(CreatureCatch, ItLooksAtTheThingUntilItReachesAndItsEyesUntilTheHandCloses)
{
	const glm::vec3 thing {10.0f, 4.0f, -20.0f};
	// Turning, waiting or stepping: the head and the eyes on the thing
	const auto waiting = LookWhileCatching(false, thing);
	ASSERT_TRUE(waiting.head.has_value());
	EXPECT_EQ(*waiting.head, thing);
	ASSERT_TRUE(waiting.eyes.has_value());
	EXPECT_EQ(*waiting.eyes, thing);
	// Reaching: the head turns back ahead, the eyes keep the point
	const auto reaching = LookWhileCatching(true, thing);
	EXPECT_FALSE(reaching.head.has_value());
	ASSERT_TRUE(reaching.eyes.has_value());
	EXPECT_EQ(*reaching.eyes, thing);
	// The hand closed: nothing is looked at
	const auto closed = LookWhileCatching(true, std::nullopt);
	EXPECT_FALSE(closed.head.has_value());
	EXPECT_FALSE(closed.eyes.has_value());
}

TEST(CreatureCatch, UnderThePlayersHandItsHandClosingLetsGoOfTheHandsPoint)
{
	const glm::vec3 hand {3.0f, 30.0f, -8.0f};
	const CatchLook held {.head = hand, .eyes = hand};
	// Turning, waiting or stepping: the look stays the hand's
	const auto waiting = LookUnderTheHand(false, false, held);
	ASSERT_TRUE(waiting.head.has_value());
	EXPECT_EQ(*waiting.head, hand);
	ASSERT_TRUE(waiting.eyes.has_value());
	EXPECT_EQ(*waiting.eyes, hand);
	// Reaching: the head turns back ahead, the eyes keep the hand's point
	const auto reaching = LookUnderTheHand(true, false, held);
	EXPECT_FALSE(reaching.head.has_value());
	ASSERT_TRUE(reaching.eyes.has_value());
	EXPECT_EQ(*reaching.eyes, hand);
	// The creature's hand closes: the point the player's hand set is let go too
	const auto closed = LookUnderTheHand(true, true, held);
	EXPECT_FALSE(closed.head.has_value());
	EXPECT_FALSE(closed.eyes.has_value());
	// The player's hand sets its point again in its next frame, and the eyes follow it
	const auto after = LookUnderTheHand(true, false, {.head = std::nullopt, .eyes = hand});
	ASSERT_TRUE(after.eyes.has_value());
	EXPECT_EQ(*after.eyes, hand);
}
