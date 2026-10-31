/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mana path a worship site's miracles and the hand past the border let out: its maths, a spark's step pinned to the
// original's own numbers, and the rule (Particles/Rules/ManaPath.cpp) on the game's SF_ManaPathNew fed through the
// effect's motes. Wiki: docs/wiki/physics/particle-rules.md#the-mana-path.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandomTesting.h"
#include "Locator.h"
#include "Particles/ManaPathMaths.h"
#include "Particles/Noise.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::mana_path;

namespace
{
constexpr Settings k_Steady {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = true};
constexpr Settings k_Curved {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = false};
constexpr Draws k_Draws {.frequency = 0.4f, .amplitude = 0.2f, .phase = 0.5f};
constexpr float k_Step = 0.1f;
constexpr float k_Ground = 7.0f;
} // namespace

TEST(ManaPath, APathKnowsItsLengthAcrossTheGroundAndTheWayAcross)
{
	const auto path = MakePath({10.0f, 5.0f, 20.0f}, {13.0f, 50.0f, 24.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(path.length, 5.0f);
	// A quarter turn from the way along (0.6, 0.8)
	EXPECT_FLOAT_EQ(path.side.x, 0.8f);
	EXPECT_FLOAT_EQ(path.side.y, 0.0f);
	EXPECT_FLOAT_EQ(path.side.z, -0.6f);
	// Each spark weaves by its own share of 0.6 to 1.4 of the file's frequency and size, starting along the noise by
	// its draw out of 256
	EXPECT_FLOAT_EQ(path.frequency, 0.5f);
	EXPECT_FLOAT_EQ(path.amplitude, 4.8f);
	EXPECT_FLOAT_EQ(path.phase, 128.0f);
}

TEST(ManaPath, ASparkIsTurnedAlongItsPathAQuarterTurnRound)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Heading(path), std::numbers::pi_v<float>);
}

TEST(ManaPath, ASteadySparkCrossesItsPathAtItsSpeed)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Steady, 1.0f), 0.5f);
	// The point is held to the path's ends
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 0.25f).x, 20.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 1.5f).x, 80.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, -1.0f).x, 0.0f);
}

TEST(ManaPath, ACurvedSparkTakesItsTimeAndLeavesAndArrivesAlongThePath)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {120.0f, 0.0f, 0.0f}, k_Curved, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Curved, 1.5f), 0.5f);
	// Its ends, and with tangents as long as the path the curve runs evenly
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.0f).x, 0.0f, 1e-4f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 1.0f).x, 120.0f, 1e-3f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.25f).x, 30.0f, 1e-3f);
	// The curve is not held to its ends
	EXPECT_GT(PointOnPath(path, k_Curved, 1.2f).x, 120.0f);
}

TEST(ManaPath, TheWeaveGrowsHoldsAndDiesAway)
{
	EXPECT_FLOAT_EQ(WeaveShare(-0.1f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.1f), 0.5f);
	EXPECT_FLOAT_EQ(WeaveShare(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.8f), 1.0f);
	EXPECT_NEAR(WeaveShare(0.9f), 0.5f, 1e-5f);
	EXPECT_NEAR(WeaveShare(1.0f), 0.0f, 1e-5f);
	EXPECT_FLOAT_EQ(WeaveShare(1.1f), 0.0f);
}

TEST(ManaPath, ASparkWeavesAcrossItsPathByTheNoise)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	// The noise is read along the path by its frequency over its length, from its phase
	EXPECT_FLOAT_EQ(WeaveNoiseAt(path, 0.5f), 0.5f * 10.0f * 0.5f + 128.0f);
	// Across a path along z is along x
	const auto woven = Woven(path, {0.0f, 0.0f, 5.0f}, 0.5f, 0.5f);
	EXPECT_FLOAT_EQ(woven.x, 0.5f * 4.8f);
	EXPECT_FLOAT_EQ(woven.y, 5.0f);
	// Not at all where it starts
	const auto start = Woven(path, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(start.x, 0.0f);
}

TEST(ManaPath, TheGroundIsReadOnTheMapsFixedGrid)
{
	// 1 m is 6553.6 steps, truncated to 6553
	EXPECT_FLOAT_EQ(GroundPoint({1.0f, 0.0f}).x, 6553.0f * 10.0f / 65536.0f);
	EXPECT_FLOAT_EQ(GroundPoint({0.0f, 10.0f}).y, 10.0f);
}

namespace
{
/// The original's step of one spark, its noise and its land given: the age's bits, then how far along it is and where
/// it is drawn. The path runs from (100.5, 3, 200.25) to (130.75, 7, 160.5), its weave's draws 0.4, 0.2 and 0.5, with
/// SF_ManaPathNew's settings; the noise reads 0.37 and the land 7 everywhere
struct SparkCase
{
	bool constantSpeed;
	uint32_t age;
	uint32_t progress;
	uint32_t noiseAt;
	std::array<uint32_t, 3> position;
};

constexpr std::array k_SparkCases {
    SparkCase {true, 0x00000000, 0x00000000, 0x43000000, {0x42C90000, 0x40ECCCCD, 0x43484000}},
    SparkCase {true, 0x3DCCCCCD, 0x3DA3FFFF, 0x43005B87, {0x42CCB714, 0x40ECCCCD, 0x4344A318}},
    SparkCase {true, 0x3F000000, 0x3ECCFFFE, 0x4301C9A1, {0x42DE6707, 0x40ECCCCD, 0x433742D1}},
    SparkCase {true, 0x3F800000, 0x3F4CFFFE, 0x43039342, {0x42F6A319, 0x40ECCCCD, 0x43275984}},
    SparkCase {true, 0x3F99999A, 0x3F75FFFE, 0x43044A4F, {0x43014AF8, 0x40ECCCCD, 0x4321D7D4}},
    SparkCase {true, 0x3FA66666, 0x3F853FFF, 0x430476F4, {0x4302C000, 0x40ECCCCD, 0x43208000}},
    SparkCase {true, 0xBF000000, 0xBECCFFFE, 0x43000000, {0x42C90000, 0x40ECCCCD, 0x43484000}},
    SparkCase {false, 0x3E99999A, 0x3DCCCCCD, 0x4300724C, {0x42D3B6F2, 0x40ECCCCD, 0x433FBF28}},
    SparkCase {false, 0x3FC00000, 0x3F000000, 0x43023B7A, {0x42E46DC7, 0x40ECCCCD, 0x43334D31}},
    SparkCase {false, 0x40533333, 0x3F8CCCCD, 0x430476F4, {0x430B580E, 0x40ECCCCD, 0x43153506}},
};

glm::vec3 Bits(uint32_t x, uint32_t y, uint32_t z)
{
	return {std::bit_cast<float>(x), std::bit_cast<float>(y), std::bit_cast<float>(z)};
}

/// The path the original's step was given, field by field
Path OriginalsPath()
{
	return {
	    .from = Bits(0x42C90000, 0x40400000, 0x43484000),
	    .to = Bits(0x4302C000, 0x40E00000, 0x43208000),
	    .side = Bits(0xBF4BB7FF, 0x00000000, 0xBF1B07FF),
	    .length = std::bit_cast<float>(0x4247CE0Eu),
	    .cubic = Bits(0x42A8C18A, 0x413284CE, 0xC2DDC0F8),
	    .square = Bits(0xC2FD224F, 0xC185E39A, 0x432650BA),
	    .linear = Bits(0x4290E0C5, 0x41194267, 0xC2BE607C),
	    .constant = Bits(0x42C90000, 0x40400000, 0x43484000),
	    .amplitude = std::bit_cast<float>(0x40994E3Cu),
	    .frequency = std::bit_cast<float>(0x3DB70D20u),
	    .phase = std::bit_cast<float>(0x43000000u),
	};
}

/// SF_ManaPathNew's rule as the game's file sets it
Settings FileSettings(bool constantSpeed)
{
	return {.speed = 40.0f,
	        .timeToTravel = 3.0f,
	        .noiseFrequency = 0.0893805f,
	        .noiseAmplitude = 5.9885f,
	        .height = 0.4f,
	        .constantSpeed = constantSpeed};
}

/// Where the last step read the noise; the noise it is given is 0.37
float g_NoiseAt = 0.0f;
float FixedNoise(float at)
{
	g_NoiseAt = at;
	return 0.37f;
}
} // namespace

// A spark steps along its path, weaves aside and lies above the land as the original's (bits from the original's own
// step: docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPath, ASparksStepIsTheOriginals)
{
	const auto path = OriginalsPath();
	for (const auto& c : k_SparkCases)
	{
		glm::vec2 groundAt {-1.0f};
		const auto step =
		    Step(path, FileSettings(c.constantSpeed), std::bit_cast<float>(c.age), FixedNoise, [&groundAt](glm::vec2 at) {
			    groundAt = at;
			    return 7.0f;
		    });
		SCOPED_TRACE(c.age);
		EXPECT_EQ(std::bit_cast<uint32_t>(step.progress), c.progress);
		EXPECT_EQ(std::bit_cast<uint32_t>(g_NoiseAt), c.noiseAt);
		EXPECT_EQ(std::bit_cast<uint32_t>(step.position.x), c.position[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(step.position.y), c.position[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(step.position.z), c.position[2]);
		// the land is read where the spark is, on the map's grid
		EXPECT_EQ(groundAt, GroundPoint({step.position.x, step.position.z}));
	}
}

// The path is worked out from the mote's two points and the spark's draws as the original's rule works it out
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPath, APathIsWorkedOutAsTheOriginals)
{
	const auto expected = OriginalsPath();
	const auto path =
	    MakePath(expected.from, expected.to, FileSettings(true), {.frequency = 0.4f, .amplitude = 0.2f, .phase = 0.5f});
	EXPECT_EQ(path.side, expected.side);
	EXPECT_EQ(std::bit_cast<uint32_t>(path.length), std::bit_cast<uint32_t>(expected.length));
	EXPECT_EQ(path.cubic, expected.cubic);
	EXPECT_EQ(path.square, expected.square);
	EXPECT_EQ(path.linear, expected.linear);
	EXPECT_EQ(path.constant, expected.constant);
	EXPECT_EQ(std::bit_cast<uint32_t>(path.amplitude), std::bit_cast<uint32_t>(expected.amplitude));
	EXPECT_EQ(std::bit_cast<uint32_t>(path.frequency), std::bit_cast<uint32_t>(expected.frequency));
	EXPECT_EQ(std::bit_cast<uint32_t>(path.phase), std::bit_cast<uint32_t>(expected.phase));
}

TEST(ManaPath, AlongIsTheProgressHeldToItsEnds)
{
	EXPECT_EQ(Along(-0.5f), 0.0f);
	EXPECT_EQ(Along(0.0f), 0.0f);
	EXPECT_EQ(Along(0.25f), 0.25f);
	EXPECT_EQ(Along(1.0f), 1.0f);
	EXPECT_EQ(Along(1.5f), 1.0f);
	EXPECT_EQ(Along(std::nanf("")), 0.0f);
}

namespace
{
/// The game's SF_ManaPathNew, its rule with this creator
std::string ManaPathFile(const std::string& creator = "ParticleSpriteCreatorGood")
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 1\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS UR_ManaPathNew UR_ManaPathNew0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY Gain FLOAT 0.827434\n";
	text += "PROPERTY GlowLength FLOAT 5.24734\n";
	text += "PROPERTY GlowPosSpeed FLOAT 40\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY Height FLOAT 0.4\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY NoiseAmplitude FLOAT 5.9885\n";
	text += "PROPERTY NoiseFrequency FLOAT 0.0893805\n";
	text += "PROPERTY PCreator PERSIS_PNTR " + creator + "\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "PROPERTY TimeToTravel FLOAT 3\n";
	text += "PROPERTY UseConstantSpeed BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreatorGood\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY CentreAtBase BOOL 0\n";
	text += "PROPERTY ColorA INTEGER 15\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 46\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY FrameRate FLOAT 24\n";
	text += "PROPERTY InitFrame INTEGER 0\n";
	text += "PROPERTY InitialScale FLOAT 2\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "PROPERTY MaterialUpdateZBuffer BOOL 0\n";
	text += "PROPERTY NumFrames INTEGER 8\n";
	text += "PROPERTY NumSpritesPerRow INTEGER 8\n";
	text += "PROPERTY PlayAnim BOOL 1\n";
	text += "PROPERTY RandomiseFrameDirection BOOL 1\n";
	text += "PROPERTY RandomiseInitFrame BOOL 1\n";
	text += "PROPERTY RandomiseScale BOOL 0\n";
	text += "PROPERTY ScaleAlpha INTEGER 255\n";
	text += "PROPERTY SetHorozontal BOOL 1\n";
	text += "PROPERTY SetVertical BOOL 0\n";
	text += "PROPERTY SpriteOriginX FLOAT 0\n";
	text += "PROPERTY SpriteOriginY FLOAT 0\n";
	text += "PROPERTY StretchVertically FLOAT 3\n";
	text += "PROPERTY FileOffset INTEGER 40\n";
	text += "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet2.raw\n";
	text += "PROPERTY UseAdditiveAlpha BOOL 1\n";
	text += "PROPERTY UsePlayerColor BOOL 0\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

std::unique_ptr<openblack::particles::Effect> MakeManaPath(const std::string& text = ManaPathFile())
{
	auto file = openblack::particles::File::Parse(text, "test");
	EXPECT_TRUE(file.has_value());
	return std::make_unique<openblack::particles::Effect>(std::make_shared<const openblack::particles::File>(std::move(*file)),
	                                                      glm::vec3(0.0f), 1.0f,
	                                                      openblack::game_random::particles::NetGameType::Local);
}

/// The effect's sprites, in its draw walk
std::vector<const openblack::particles::Atom*> Sparks(const openblack::particles::Effect& effect)
{
	std::vector<openblack::particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, openblack::particles::Creator::Kind::Sprite);
	std::vector<const openblack::particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		atoms.push_back(atom.atom);
	}
	return atoms;
}

constexpr uint32_t k_Red = 0xFF4646;
} // namespace

TEST(ManaPathRule, TheGameRegistersTheRule)
{
	EXPECT_NE(openblack::particles::FindModifierFactory("UR_ManaPathNew"), nullptr);
}

// A mote becomes a spark at its start, in its colour with the sprite's alpha, turned along its path, as the original
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPathRule, AMoteBecomesASparkInItsColourAtItsStart)
{
	// no land: the spark lies at the rule's height
	const openblack::test::RestoreService<openblack::Locator::terrainSystem> keepLand;
	openblack::Locator::terrainSystem::reset();
	auto effect = MakeManaPath();
	effect->AddSpellPoint({.from = {10.0f, 0.0f, 20.0f}, .to = {10.0f, 0.0f, 100.0f}, .rgb = k_Red});
	effect->Step(0.1f);
	EXPECT_EQ(effect->SpellPointCount(), 0u);
	const auto sparks = Sparks(*effect);
	ASSERT_EQ(sparks.size(), 1u);
	EXPECT_EQ(sparks[0]->colour, (std::array<uint8_t, 4> {0xFF, 0x46, 0x46, 15}));
	// no weave where it starts, and the rule's height above the land
	EXPECT_EQ(sparks[0]->position, glm::vec3(10.0f, 0.4f, 20.0f));
	const auto path = MakePath({10.0f, 0.0f, 20.0f}, {10.0f, 0.0f, 100.0f}, FileSettings(true), {});
	EXPECT_EQ(sparks[0]->rotation, openblack::affine::AngleY(Heading(path)));
}

// Its weave's three draws come after the sprite's own, on the effect's stream, as the original
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPathRule, ItsDrawsComeAfterTheSpritesOnTheLocalStream)
{
	const openblack::test::RestoreService<openblack::Locator::terrainSystem> keepLand;
	openblack::Locator::terrainSystem::reset();
	auto& random = openblack::Locator::gameRandom::value();
	random.Load({.synced = 0x1234u, .local = 0x88F89Fu});
	const glm::vec3 from {0.0f, 0.0f, 0.0f};
	const glm::vec3 to {200.0f, 0.0f, 50.0f};
	auto effect = MakeManaPath();
	effect->AddSpellPoint({.from = from, .to = to, .rgb = k_Red});
	effect->Step(0.1f);
	// the atom's random byte, the sprite's first frame and its direction, then the weave's frequency, size and phase
	uint32_t seed = 0x88F89Fu;
	static_cast<void>(openblack::game_random::SeededRandom(0x100, seed));
	static_cast<void>(openblack::game_random::SeededRandom(8, seed));
	static_cast<void>(openblack::game_random::SeededRandom(0x100, seed));
	Draws draws {};
	draws.frequency = openblack::game_random::FloatRand(0.8f, seed);
	draws.amplitude = openblack::game_random::FloatRand(0.8f, seed);
	draws.phase = openblack::game_random::FloatRand(1.0f, seed);
	EXPECT_EQ(random.Current().local, seed);
	EXPECT_EQ(random.Current().synced, 0x1234u);
	// the next step puts it where that path has it after a tenth of a second
	effect->Step(0.1f);
	const auto expected = Step(MakePath(from, to, FileSettings(true), draws), FileSettings(true), 0.1f,
	                           openblack::particles::noise::SignedValueNoise, [](glm::vec2) { return 0.0f; });
	const auto sparks = Sparks(*effect);
	ASSERT_EQ(sparks.size(), 1u);
	EXPECT_EQ(sparks[0]->position, expected.position);
}

// A spark goes at the step after the one that finds it arrived, as the original
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPathRule, ASparkGoesTheStepAfterItHasArrived)
{
	auto effect = MakeManaPath();
	// 8 m at 40 m a second: a fifth of a second
	effect->AddSpellPoint({.from = {0.0f, 0.0f, 0.0f}, .to = {8.0f, 0.0f, 0.0f}, .rgb = k_Red});
	// the steps read the ages 0, 0.1, 0.2 (at its end, not past it) and 0.3 (past it): it goes at the fifth
	for (int step = 0; step < 4; ++step)
	{
		effect->Step(0.1f);
		EXPECT_EQ(effect->AtomCount(), 1u);
	}
	effect->Step(0.1f);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

// The last mote given is let out first, and a collection of a hundred sparks drops the motes given, as the original
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPathRule, TheLastMoteGivenIsLetOutFirst)
{
	auto effect = MakeManaPath();
	effect->AddSpellPoint({.from = {0.0f, 0.0f, 0.0f}, .to = {80.0f, 0.0f, 0.0f}, .rgb = 0x000001});
	effect->AddSpellPoint({.from = {0.0f, 0.0f, 0.0f}, .to = {80.0f, 0.0f, 0.0f}, .rgb = 0x000002});
	effect->Step(0.1f);
	const auto sparks = Sparks(*effect);
	ASSERT_EQ(sparks.size(), 2u);
	EXPECT_EQ(sparks[0]->colour[2], 2);
	EXPECT_EQ(sparks[1]->colour[2], 1);
}

TEST(ManaPathRule, AHundredSparksAreTheMost)
{
	auto effect = MakeManaPath();
	for (int i = 0; i < 101; ++i)
	{
		effect->AddSpellPoint({.from = {0.0f, 0.0f, 0.0f}, .to = {800.0f, 0.0f, 0.0f}, .rgb = k_Red});
	}
	effect->Step(0.1f);
	EXPECT_EQ(effect->AtomCount(), 100u);
	EXPECT_EQ(effect->SpellPointCount(), 0u);
}

// A path shorter than a centimetre across the ground makes no spark, though the spark drew its weave, as the original
// (docs/wiki/physics/particle-rules.md#the-mana-path)
TEST(ManaPathRule, AShortPathMakesNoSpark)
{
	auto& random = openblack::Locator::gameRandom::value();
	random.Load({.synced = 0u, .local = 0x88F89Fu});
	auto effect = MakeManaPath();
	effect->AddSpellPoint({.from = {5.0f, 0.0f, 5.0f}, .to = {5.005f, 30.0f, 5.0f}, .rgb = k_Red});
	effect->Step(0.1f);
	EXPECT_EQ(effect->AtomCount(), 0u);
	uint32_t seed = 0x88F89Fu;
	for (const uint32_t n : {0x100u, 8u, 0x100u})
	{
		static_cast<void>(openblack::game_random::SeededRandom(n, seed));
	}
	for (const float most : {0.8f, 0.8f, 1.0f})
	{
		static_cast<void>(openblack::game_random::FloatRand(most, seed));
	}
	EXPECT_EQ(random.Current().local, seed);
}

TEST(ManaPathRule, WithoutACreatorTheMotesStay)
{
	auto effect = MakeManaPath(ManaPathFile("NULL_STRING"));
	effect->AddSpellPoint({.from = {0.0f, 0.0f, 0.0f}, .to = {80.0f, 0.0f, 0.0f}, .rgb = k_Red});
	effect->Step(0.1f);
	EXPECT_EQ(effect->SpellPointCount(), 1u);
	EXPECT_EQ(effect->AtomCount(), 0u);
}
