/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EmitterRuleLightningSprite (Particles/Rules/StormFlash): when the storm in the hand emits a sprite, and the light
// each one gives its cloud; then the rule on a small synthetic effect. The expected values of the maths are the
// original's, run with the same inputs and a fake draw.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <bit>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/StormFlash.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// A fake random draw: returns `value` and records the bound it was asked for
struct FakeRandom
{
	float value {0.0f};
	std::vector<float> asked;
	float operator()(float max)
	{
		asked.push_back(max);
		return value;
	}
};

struct EmitCase
{
	float age, dt, frequency;
	int maxAtoms, maxTotal;
	bool randomise;
	size_t atoms;
	bool first;
	float nextTime;
	int emitted;
	float draw;
	// the original's answer
	bool emits;
	uint32_t nextTimeAfter;
	int emittedAfter;
};

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The in-hand storm's shape: a root point atom (standing for the cloud) with the sprites' group under it, here with
/// a fixed wait of 1 / 0.48 s between sprites (no Randomise), which no step of 0.1 s lands on
constexpr std::string_view k_EffectText = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleSpriteCreator ParticleSpriteCreator_Lightning
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 5.5
PROPERTY NumFrames INTEGER 8
PROPERTY FileOffset INTEGER 40
PROPERTY TextureFileName STRING .\Data\Textures\S_SpriteSheet3.raw
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_RootAtom
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EmitterRuleLightningSprite EmitterRuleSimple_Lightning
BEGINPROPERTIES
PROPERTY EmissionFreq FLOAT 0.48
PROPERTY Group INTEGER 1
PROPERTY MaxAtoms INTEGER 4
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticleSpriteCreator_Lightning
PROPERTY Randomise BOOL 0
ENDPROPERTIES
ENDCLASS
)";

std::unique_ptr<Effect> MakeStorm()
{
	auto file = File::Parse(std::string(k_EffectText), "SF_StormFlashTest");
	EXPECT_TRUE(file.has_value());
	return std::make_unique<Effect>(std::make_shared<const File>(std::move(*file)), glm::vec3(0.0f), 1.0f);
}

/// The root atom, the cloud the flashes light
const Atom* Cloud(const Effect& effect)
{
	std::vector<Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, Creator::Kind::Point);
	return atoms.size() == 1 ? atoms.front().atom : nullptr;
}

size_t Sprites(const Effect& effect)
{
	std::vector<Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, Creator::Kind::Sprite);
	return atoms.size();
}
} // namespace

TEST(StormFlash, EmissionMatchesTheOriginal)
{
	const EmitCase cases[] = {
	    // the first step starts the clock at the age, and one comes at once: 0 + (0.5 + 0.25) / 0.8
	    {0.0f, 0.1f, 0.8f, 4, -1, true, 0, true, 0.0f, 0, 0.5f, true, 0x3F700000u, 1},
	    {0.1f, 0.1f, 0.8f, 4, -1, true, 0, false, 0.0f, 0, 0.3f, true, 0x3F300000u, 1},
	    // age + dt only reaching the due time: not yet
	    {0.1f, 0.1f, 0.8f, 4, -1, true, 0, false, 0.2f, 0, 0.3f, false, Bits(0.2f), 0},
	    {0.1f, 0.1f, 0.8f, 4, -1, true, 0, false, 0.19f, 0, 0.3f, true, 0x3F60A3D7u, 1},
	    // more atoms than MaxAtoms: none; exactly MaxAtoms: one more
	    {0.1f, 0.1f, 0.8f, 4, -1, true, 5, false, 0.0f, 0, 0.3f, false, 0x00000000u, 0},
	    {0.1f, 0.1f, 0.8f, 4, -1, true, 4, false, 0.0f, 0, 0.3f, true, 0x3F300000u, 1},
	    // MaxTotalAtomsToEmit reached
	    {0.1f, 0.1f, 0.8f, 4, 3, true, 0, false, 0.0f, 3, 0.3f, false, 0x00000000u, 3},
	    // without Randomise: 1 / EmissionFreq, and no draw
	    {0.1f, 0.1f, 1.8f, 4, -1, false, 0, false, 0.0f, 0, 0.3f, true, 0x3F0E38E4u, 1},
	    // the in-hand storms' rates (power-up levels)
	    {2.35f, 0.1f, 2.8f, 4, -1, true, 1, false, 2.3f, 7, 0.7499f, true, 0x402A0E0Bu, 8},
	    {2.35f, 0.0333f, 1.8f, 4, -1, true, 1, false, 2.3f, 7, 0.123f, true, 0x40207654u, 8},
	};
	for (const auto& c : cases)
	{
		const storm_flash::Emission emission {
		    .frequency = c.frequency, .maxAtoms = c.maxAtoms, .maxTotal = c.maxTotal, .randomise = c.randomise};
		storm_flash::State state {.first = c.first, .nextTime = c.nextTime, .emitted = c.emitted};
		FakeRandom random {.value = c.draw};
		const bool emits =
		    storm_flash::ShouldEmit(emission, state, c.age, c.dt, c.atoms, [&random](float max) { return random(max); });
		EXPECT_EQ(emits, c.emits) << "age " << c.age << " next " << c.nextTime;
		EXPECT_EQ(Bits(state.nextTime), c.nextTimeAfter) << "age " << c.age << " next " << c.nextTime;
		EXPECT_EQ(state.emitted, c.emittedAfter);
		EXPECT_FALSE(state.first);
		// one draw of random(0.75), only for a sprite that comes with Randomise
		const auto draws = (c.emits && c.randomise) ? std::vector<float> {0.75f} : std::vector<float> {};
		EXPECT_EQ(random.asked, draws);
	}
}

TEST(StormFlash, FlashLightsTheCloudAsTheOriginal)
{
	struct Case
	{
		float age, strikeAge, specLife;
		uint32_t specular;
	};
	const Case cases[] = {
	    {1.0f, 1.0f, 0.5f, 0xFEC7C7FEu},   // the strike's own step: v = 255
	    {1.05f, 1.0f, 0.5f, 0xE4B2B2E4u},  //
	    {1.1f, 1.0f, 0.5f, 0xCA9E9ECAu},   //
	    {1.2f, 1.0f, 0.5f, 0x97767697u},   //
	    {1.25f, 1.0f, 0.5f, 0x7E63637Eu},  //
	    {1.3f, 1.0f, 0.5f, 0x654F4F65u},   //
	    {1.4f, 1.0f, 0.5f, 0x32272732u},   //
	    {1.45f, 1.0f, 0.5f, 0x18131318u},  //
	    {1.49f, 1.0f, 0.5f, 0x04030304u},  //
	    {1.5f, 1.0f, 0.5f, 0x00000000u},   // at SpecLife exactly: v = 0, alpha 0 too
	    {3.7f, 3.3f, 0.5f, 0x31272731u},   //
	    {0.123f, 0.1f, 0.5f, 0xF2BDBDF2u}, //
	    {5.0f, 4.6f, 0.8f, 0x7E63637Eu},   //
	};
	for (const auto& c : cases)
	{
		storm_flash::State state {.striking = true, .strikeAge = c.strikeAge};
		const auto specular = storm_flash::FlashStep(state, c.age, c.specLife);
		ASSERT_TRUE(specular.has_value());
		EXPECT_EQ(*specular, c.specular) << "age " << c.age;
		EXPECT_TRUE(state.striking);
	}
}

TEST(StormFlash, FlashEndsBlackPastSpecLife)
{
	for (const float age : {1.5001f, 1.6f})
	{
		storm_flash::State state {.striking = true, .strikeAge = 1.0f};
		EXPECT_EQ(storm_flash::FlashStep(state, age, 0.5f), std::optional<uint32_t>(storm_flash::k_FlashOff));
		EXPECT_FALSE(state.striking);
	}
	EXPECT_EQ(storm_flash::k_FlashOff, 0xFF000000u);
	// with no flash on, the cloud keeps its specular
	storm_flash::State idle {};
	EXPECT_EQ(storm_flash::FlashStep(idle, 2.0f, 0.5f), std::nullopt);
}

TEST(StormFlash, TheGameRegistersTheRule)
{
	EXPECT_NE(FindModifierFactory("EmitterRuleLightningSprite"), nullptr);
}

TEST(StormFlash, EachSpriteLightsItsCloudForSpecLife)
{
	const game_random::testing::ScopedState random;
	auto effect = MakeStorm();
	// the step that makes the cloud makes its first sprite too, at full light
	effect->Step(0.1f);
	const Atom* cloud = Cloud(*effect);
	ASSERT_NE(cloud, nullptr);
	EXPECT_EQ(Sprites(*effect), 1u);
	EXPECT_EQ(cloud->specular, 0xFEC7C7FEu);
	// it fades over SpecLife (0.5 s), then the cloud goes black and stays so
	uint32_t last = cloud->specular;
	for (int step = 0; step < 4; ++step)
	{
		effect->Step(0.1f);
		EXPECT_LT(cloud->specular >> 24u, last >> 24u) << "step " << step;
		EXPECT_EQ((cloud->specular >> 16u) & 0xFFu, (cloud->specular >> 8u) & 0xFFu);
		last = cloud->specular;
	}
	for (int step = 0; step < 3; ++step)
	{
		effect->Step(0.1f);
	}
	EXPECT_EQ(cloud->specular, storm_flash::k_FlashOff);
	EXPECT_EQ(Sprites(*effect), 1u);
	// the next sprite and a new flash come on the step that passes 2.08 s after the first
	for (int step = 0; step < 12; ++step)
	{
		effect->Step(0.1f);
	}
	EXPECT_EQ(Sprites(*effect), 1u);
	effect->Step(0.1f);
	EXPECT_EQ(Sprites(*effect), 2u);
	EXPECT_EQ(cloud->specular, 0xFEC7C7FEu);
	EXPECT_FALSE(effect->Finished());
}
