/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The belief sprites' maths (Particles/BeliefSpriteMaths), pinned to the original's own numbers: its rule run on the
// spell file's properties, sprite by sprite and age by age. Then the rule (Particles/Rules/Belief.cpp) on small
// effects fed through the towns' queue.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Town/TownBelief.h"
#include "Locator.h"
#include "Particles/BeliefSpriteMaths.h"
#include "Particles/Noise.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack::particles::maths;

namespace
{
/// SF_BeliefSprite's rule: the file's values, the rest the defaults
BeliefSpriteRule FileRule()
{
	BeliefSpriteRule rule;
	rule.riseSpeed = 12.0f;
	rule.maxWiggleOffset = 2.0f;
	rule.ageAtMaxOffset = 0.5f;
	rule.noiseFreq = 2.0f;
	rule.scaleMax = 1.0f;
	rule.scaleMin = 0.0f;
	rule.scaleFreq = 12.0f;
	return rule;
}

/// One sprite's step in the original, started at (100.5, 20.25, -300.75): the age's bits, its random byte, its start
/// alpha, then where it is, its alpha, its stretch's bits and whether it goes
struct StepCase
{
	uint32_t age;
	uint8_t random;
	uint8_t startAlpha;
	uint32_t position[3];
	uint8_t alpha;
	uint32_t stretch;
	bool expired;
};

constexpr StepCase k_Steps[] = {
    {0x00000000, 0, 20, {0x42C90000, 0x41A20000, 0xC3966000}, 20, 0x3F800000, false},
    {0x00000000, 13, 150, {0x42C90000, 0x41A20000, 0xC3966000}, 150, 0x3F89AA31, false},
    {0x3D4CCCCD, 77, 97, {0x42C90403, 0x41A83BF7, 0xC3963EBD}, 97, 0x3F444746, false},
    {0x3D4CCCCD, 200, 20, {0x42C977B7, 0x41A765C9, 0xC3967FDB}, 20, 0x3F97EED4, false},
    {0x3DCCCCCD, 255, 150, {0x42C96FBD, 0x41AD8327, 0xC3964D18}, 150, 0x3F40959B, false},
    {0x3DCCCCCD, 1, 97, {0x42C8B52F, 0x41ABD6CB, 0xC39669C5}, 97, 0x3FBF6117, false},
    {0x3E800000, 0, 20, {0x42C89FA2, 0x41BA0000, 0xC3967817}, 20, 0x3F89081C, false},
    {0x3E800000, 13, 150, {0x42CA9B00, 0x41BA0000, 0xC3962D57}, 150, 0x3F60A505, false},
    {0x3F000000, 77, 97, {0x42CA81A8, 0x41E057A8, 0xC3969824}, 97, 0x3F1D3C9C, false},
    {0x3F000000, 200, 20, {0x42C44141, 0x41D7F9DB, 0xC39600D1}, 20, 0x3F800D85, false},
    {0x3F4CCCCD, 255, 150, {0x42C6D3B3, 0x41FE193C, 0xC395E414}, 150, 0x3FBA1882, false},
    {0x3F4CCCCD, 1, 97, {0x42C6DA1C, 0x41F0B65A, 0xC39724A8}, 97, 0x3F4C3829, false},
    {0x3F800000, 0, 20, {0x42C6D560, 0x42010000, 0xC396EAA8}, 20, 0x3F3B5194, false},
    {0x3F800000, 13, 150, {0x42CC1789, 0x42010000, 0xC3971A28}, 150, 0x3FA0A360, false},
    {0x3FC00000, 77, 97, {0x42CB7A98, 0x422E837B, 0xC395ADD0}, 97, 0x3F5252D3, false},
    {0x3FC00000, 200, 20, {0x42C66377, 0x4221F6C9, 0xC39525B4}, 20, 0x3F1C07B9, false},
    {0x3FD9999A, 255, 150, {0x42C48E25, 0x4232DACF, 0xC39599D8}, 120, 0x3F575C1B, false},
    {0x3FD9999A, 1, 97, {0x42CA000E, 0x4224A1C0, 0xC395C4A6}, 78, 0x3FBE432D, false},
    {0x40000000, 0, 20, {0x42CA5407, 0x42310000, 0xC3960AFE}, 10, 0x3F0C1602, false},
    {0x40000000, 13, 150, {0x42CA8418, 0x42310000, 0xC39740CA}, 75, 0x3FB2DE54, false},
    {0x40200000, 77, 97, {0x42C95E4E, 0x426CDB23, 0xC395B015}, 0, 0x3F89A978, false},
    {0x40200000, 200, 20, {0x42CB48CC, 0x4257F0A4, 0xC396F53A}, 0, 0x3FBE71AE, false},
    {0x40333333, 255, 150, {0x42CA3A24, 0x42722C27, 0xC396D521}, 0, 0x3FBEDCA8, false},
    {0x40333333, 1, 97, {0x42C87C98, 0x425ABF1E, 0xC397063B}, 0, 0x3FA8A594, false},
    {0x4033D70A, 0, 20, {0x42C7A62A, 0x4257E148, 0xC396B676}, 0, 0x3FAF8D0E, true},
    {0x4033D70A, 13, 150, {0x42CA4A59, 0x4257E148, 0xC395E746}, 0, 0x3F456A16, true},
    {0x40400000, 77, 97, {0x42C7DC35, 0x4286037B, 0xC396FF34}, 0, 0x3F997F6F, true},
    {0x40400000, 200, 20, {0x42CA575F, 0x4272ED92, 0xC395CA41}, 0, 0x3FA7AD5D, true},
    {0x40E9999A, 255, 150, {0x42CBCF3A, 0x42FA998F, 0xC3958191}, 0, 0x3F968F9F, true},
    {0x40E9999A, 1, 97, {0x42C94A23, 0x42DC0FFF, 0xC395C279}, 0, 0x3F7BE99A, true},
};

/// The start alpha and the sound of a new sprite for the belief it shows, in the original
struct StartCase
{
	int32_t amount;
	uint8_t alpha;
	bool sound;
};

constexpr StartCase k_Starts[] = {
    {-5, 20, false},  {0, 20, false},   {1, 20, false},    {9, 20, false},     {10, 20, false}, {11, 21, true},
    {25, 42, true},   {54, 84, true},   {55, 85, true},    {56, 86, true},     {77, 117, true}, {99, 149, true},
    {100, 150, true}, {101, 150, true}, {1000, 150, true}, {10000, 150, true},
};

constexpr glm::vec3 k_Start {100.5f, 20.25f, -300.75f};
} // namespace

TEST(BeliefSpriteMaths, TheStartAlphaFollowsTheBeliefAsTheOriginals)
{
	const auto rule = FileRule();
	for (const auto& c : k_Starts)
	{
		EXPECT_EQ(BeliefSpriteStartAlpha(rule, c.amount), c.alpha) << c.amount;
		EXPECT_EQ(BeliefSpritePlaysSound(rule, c.amount), c.sound) << c.amount;
	}
}

TEST(BeliefSpriteMaths, ASpriteRisesWigglesPulsesAndFadesAsTheOriginals)
{
	const auto rule = FileRule();
	for (const auto& c : k_Steps)
	{
		const auto age = std::bit_cast<float>(c.age);
		const auto position = BeliefSpritePosition(rule, k_Start, age, c.random, openblack::particles::noise::SignedValueNoise);
		EXPECT_EQ(std::bit_cast<uint32_t>(position.x), c.position[0]) << age << " " << int {c.random};
		EXPECT_EQ(std::bit_cast<uint32_t>(position.y), c.position[1]) << age << " " << int {c.random};
		EXPECT_EQ(std::bit_cast<uint32_t>(position.z), c.position[2]) << age << " " << int {c.random};
		EXPECT_EQ(BeliefSpriteAlpha(rule, c.startAlpha, age), c.alpha) << age;
		EXPECT_EQ(std::bit_cast<uint32_t>(BeliefSpriteStretch(rule, age, c.random)), c.stretch) << age;
		EXPECT_EQ(BeliefSpriteExpired(rule, age), c.expired) << age;
	}
}

TEST(BeliefSpriteMaths, TheRiseIsFasterForSomeRandomBytes)
{
	// no wiggle: the rise alone, its speed x (1 + (random mod 13) x 0.0249)
	auto rule = FileRule();
	rule.maxWiggleOffset = 0.0f;
	const auto noise = [](float) { return 0.0f; };
	EXPECT_FLOAT_EQ(BeliefSpritePosition(rule, glm::vec3(0.0f), 1.0f, 0, noise).y, 12.0f);
	EXPECT_FLOAT_EQ(BeliefSpritePosition(rule, glm::vec3(0.0f), 1.0f, 13, noise).y, 12.0f);
	EXPECT_FLOAT_EQ(BeliefSpritePosition(rule, glm::vec3(0.0f), 1.0f, 12, noise).y, (12.0f * 0.0249f + 1.0f) * 12.0f);
	EXPECT_FLOAT_EQ(BeliefSpritePosition(rule, glm::vec3(0.0f), 1.0f, 12, noise).x, 0.0f);
}

TEST(BeliefSpriteMaths, TheWiggleGrowsUntilItsAgeAndReadsTheNoiseAtTheRandomByte)
{
	// a noise that gives back where it is read: x reads k + t and k + 2t, z reads 0.3k + t and 0.3k + 2t
	auto rule = FileRule();
	rule.riseSpeed = 0.0f;
	const auto noise = [](float x) { return x; };
	// t = age x NoiseFreq = 0.5; at 0.25 the wiggle is half of MaxWiggleOffset, 1
	const auto half = BeliefSpritePosition(rule, glm::vec3(0.0f), 0.25f, 10, noise);
	EXPECT_FLOAT_EQ(half.x, 1.0f * ((10.0f + 0.5f) + (10.0f + 1.0f) * 0.5f));
	EXPECT_FLOAT_EQ(half.z, 1.0f * ((3.0f + 0.5f) + (3.0f + 1.0f) * 0.5f));
	EXPECT_FLOAT_EQ(half.y, 0.0f);
	// past AgeAtMaxOffset the wiggle stays at 2
	const auto full = BeliefSpritePosition(rule, glm::vec3(0.0f), 1.0f, 0, noise);
	EXPECT_FLOAT_EQ(full.x, 2.0f * (2.0f + 4.0f * 0.5f));
}

TEST(BeliefSpriteMaths, TheFadeAndTheEndOfLife)
{
	const auto rule = FileRule();
	EXPECT_EQ(BeliefSpriteAlpha(rule, 100, 1.5f), 100);
	EXPECT_EQ(BeliefSpriteAlpha(rule, 100, 2.0f), 50);
	EXPECT_EQ(BeliefSpriteAlpha(rule, 100, 2.5f), 0);
	EXPECT_EQ(BeliefSpriteAlpha(rule, 100, 9.0f), 0);
	// gone after FadeTimeEnd + 0.3, and at once for an age that is not a number
	EXPECT_FALSE(BeliefSpriteExpired(rule, 2.8f));
	EXPECT_TRUE(BeliefSpriteExpired(rule, 2.81f));
	EXPECT_TRUE(BeliefSpriteExpired(rule, std::numeric_limits<float>::quiet_NaN()));
}

TEST(BeliefSpriteMaths, TheDefaultsAreTheRulesOwn)
{
	const BeliefSpriteRule rule;
	EXPECT_EQ(rule.riseSpeed, 10.0f);
	EXPECT_EQ(rule.maxWiggleOffset, 10.0f);
	EXPECT_EQ(rule.ageAtMaxOffset, 2.0f);
	EXPECT_EQ(rule.noiseFreq, 1.0f);
	EXPECT_EQ(rule.scaleMax, 1.0f);
	EXPECT_EQ(rule.scaleMin, 0.3f);
	EXPECT_EQ(rule.scaleFreq, 1.0f);
	EXPECT_EQ(rule.fadeTimeStart, 1.5f);
	EXPECT_EQ(rule.fadeTimeEnd, 2.5f);
	EXPECT_EQ(rule.alphaMin, 20);
	EXPECT_EQ(rule.alphaMax, 150);
	EXPECT_EQ(rule.valueAlphaMin, 10);
	EXPECT_EQ(rule.valueAlphaMax, 100);
}

namespace
{
/// The game's SF_BeliefSprite: the rule with this creator, the file's sprite creator and a point creator
std::string BeliefFile(const std::string& creator = "ParticleSpriteCreator_Belief")
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 1\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS UR_BeliefSprite UR_BeliefSprite0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY AgeAtMaxOffset FLOAT 0.5\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY DieAge FLOAT 6\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY MaxWiggleOffset FLOAT 2\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY NoiseFreq FLOAT 2\n";
	text += "PROPERTY PCreator PERSIS_PNTR " + creator + "\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "PROPERTY RiseSpeed FLOAT 12\n";
	text += "PROPERTY ScaleFreq FLOAT 12\n";
	text += "PROPERTY ScaleMax FLOAT 1\n";
	text += "PROPERTY ScaleMin FLOAT 0\n";
	text += "PROPERTY UseLinearNoise BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreator_Belief\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 120\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY FrameRate FLOAT 24\n";
	text += "PROPERTY InitialScale FLOAT 1.4\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "PROPERTY NumFrames INTEGER 8\n";
	text += "PROPERTY NumSpritesPerRow INTEGER 8\n";
	text += "PROPERTY PlayAnim BOOL 1\n";
	text += "PROPERTY RandomiseFrameDirection BOOL 1\n";
	text += "PROPERTY RandomiseInitFrame BOOL 1\n";
	text += "PROPERTY RandomiseScale BOOL 0\n";
	text += "PROPERTY StretchVertically FLOAT 1.1\n";
	text += "PROPERTY FileOffset INTEGER 40\n";
	text += "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet2.raw\n";
	text += "PROPERTY UseAdditiveAlpha BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 120\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

std::unique_ptr<openblack::particles::Effect> MakeBelief(const std::string& text = BeliefFile())
{
	auto file = openblack::particles::File::Parse(text, "test");
	EXPECT_TRUE(file.has_value());
	return std::make_unique<openblack::particles::Effect>(std::make_shared<const openblack::particles::File>(std::move(*file)),
	                                                      glm::vec3(0.0f), 1.0f,
	                                                      openblack::game_random::particles::NetGameType::Local);
}

/// The effect's atoms of a kind, in its draw walk (the collection's order)
std::vector<const openblack::particles::Atom*>
Atoms(const openblack::particles::Effect& effect,
      openblack::particles::Creator::Kind kind = openblack::particles::Creator::Kind::Sprite)
{
	std::vector<openblack::particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, kind);
	std::vector<const openblack::particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		atoms.push_back(atom.atom);
	}
	return atoms;
}
} // namespace

TEST(BeliefSpriteRule, TheGameRegistersTheRule)
{
	EXPECT_NE(openblack::particles::FindModifierFactory("UR_BeliefSprite"), nullptr);
}

TEST(BeliefSpriteRule, EveryQueuedSpriteIsTakenInOneStepTheLastFirst)
{
	namespace town_belief = openblack::ecs::town_belief;
	town_belief::QueueBeliefSprite({1.0f, 2.0f, 3.0f}, 55, 0xFF47FF54);
	town_belief::QueueBeliefSprite({-4.0f, 5.5f, 6.25f}, 100, 0xFFFF4646);
	auto effect = MakeBelief();
	effect->Step(0.1f);
	EXPECT_EQ(town_belief::BeliefSpriteCount(), 0u);
	const auto atoms = Atoms(*effect);
	ASSERT_EQ(atoms.size(), 2u);
	// the last one queued is made first: at its point (no rise nor wiggle at age 0), in its colour, with the alpha of
	// its belief
	EXPECT_EQ(atoms[0]->position, glm::vec3(-4.0f, 5.5f, 6.25f));
	EXPECT_EQ(atoms[0]->colour, (std::array<uint8_t, 4> {0xFF, 0x46, 0x46, 150}));
	EXPECT_EQ(atoms[1]->position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(atoms[1]->colour, (std::array<uint8_t, 4> {0x47, 0xFF, 0x54, 85}));
	EXPECT_EQ(atoms[0]->ruleScale, 1.0f);
}

TEST(BeliefSpriteRule, ItsDrawsAreOnTheLocalStream)
{
	auto& random = openblack::Locator::gameRandom::value();
	random.Load({.synced = 0x1234u, .local = 0x88F89Fu});
	openblack::ecs::town_belief::QueueBeliefSprite({0.0f, 0.0f, 0.0f}, 50, 0xFFFFFFFF);
	auto effect = MakeBelief();
	effect->Step(0.1f);
	// three draws: the atom's random byte, then the sprite's first frame and its direction
	uint32_t seed = 0x88F89Fu;
	const uint32_t randomByte = openblack::game_random::SeededRandom(0x100, seed);
	static_cast<void>(openblack::game_random::SeededRandom(8, seed));
	static_cast<void>(openblack::game_random::SeededRandom(0x100, seed));
	const auto atoms = Atoms(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0]->random, randomByte);
	EXPECT_EQ(random.Current().local, seed);
	EXPECT_EQ(random.Current().synced, 0x1234u);
}

TEST(BeliefSpriteRule, ASpriteRisesFadesAndGoes)
{
	openblack::ecs::town_belief::QueueBeliefSprite({10.0f, 0.0f, 10.0f}, 100, 0xFFFFFFFF);
	auto effect = MakeBelief();
	float lastY = -1.0f;
	int steps = 0;
	for (; steps < 100 && (steps == 0 || effect->AtomCount() > 0); ++steps)
	{
		effect->Step(0.1f);
		// a sprite with no alpha left is not drawn
		const auto atoms = Atoms(*effect);
		if (atoms.empty())
		{
			continue;
		}
		// the age the step read, before its time was added
		const float age = effect->AtomAge(*atoms[0]) - 0.1f;
		EXPECT_GT(atoms[0]->position.y, lastY);
		lastY = atoms[0]->position.y;
		// opaque until the fade starts, fading after it
		if (age < 1.45f)
		{
			EXPECT_EQ(atoms[0]->colour[3], 150);
		}
		if (age > 1.55f)
		{
			EXPECT_LT(atoms[0]->colour[3], 150);
		}
		// pulsing between 0.5 and 1.5
		EXPECT_GE(atoms[0]->stretch, 0.5f);
		EXPECT_LE(atoms[0]->stretch, 1.5f);
	}
	// made at age 0 and read every 0.1 s: gone at the step that reads an age past 2.8, the 29th or 30th
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_GE(steps, 29);
	EXPECT_LE(steps, 30);
}

TEST(BeliefSpriteRule, OnlyASpritePulses)
{
	openblack::ecs::town_belief::QueueBeliefSprite({0.0f, 0.0f, 0.0f}, 100, 0xFFFFFFFF);
	auto effect = MakeBelief(BeliefFile("ParticlePointCreator0"));
	for (int i = 0; i < 5; ++i)
	{
		effect->Step(0.1f);
	}
	// a point keeps its creator's stretch
	const auto atoms = Atoms(*effect, openblack::particles::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0]->stretch, 1.0f);
}

TEST(BeliefSpriteRule, WithoutACreatorTheQueueStays)
{
	openblack::ecs::town_belief::QueueBeliefSprite({0.0f, 0.0f, 0.0f}, 100, 0xFFFFFFFF);
	auto effect = MakeBelief(BeliefFile("NULL_STRING"));
	effect->Step(0.1f);
	EXPECT_EQ(openblack::ecs::town_belief::BeliefSpriteCount(), 1u);
	EXPECT_EQ(effect->AtomCount(), 0u);
}
