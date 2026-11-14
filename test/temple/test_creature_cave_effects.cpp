/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <functional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureCaveEffects.h"
#include "3D/TempleLight.h"
#include "3D/TempleSounds.h"
#include "3D/TempleWaterfall.h"

using namespace openblack;

namespace
{
/// The bounds' middle, counting the draws
struct FakeRandom
{
	int draws {0};
	float operator()(float min, float max)
	{
		++draws;
		return (min + max) * 0.5f;
	}
};

/// The bounds asked for, answered from a list in turn
struct RecordingRandom
{
	std::vector<float> values;
	std::vector<std::pair<float, float>> bounds;
	float operator()(float min, float max)
	{
		bounds.emplace_back(min, max);
		return values.at(bounds.size() - 1);
	}
};

/// The bounds the local stream is asked for, answered with half of each
struct LocalRecordingRandom
{
	std::vector<float> bounds;
	float operator()(float x)
	{
		bounds.push_back(x);
		return x * 0.5f;
	}
};
} // namespace

TEST(CreatureCaveEffects, SmokeFadesAsItAges)
{
	// Steady at 79 until a quarter of its life, then fading to nothing at its end
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(0, false), 0x4f);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(225, false), 0x4f);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(900, false), 0);
	// Smoke thrown up fades in first
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(0, true), 0);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(50, true), 39);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(100, true), 0x4f);
}

TEST(CreatureCaveEffects, FlamesRunBackThroughTheirFramesAQuarterApart)
{
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(0, 0), 31u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(32, 0), 30u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(0, 1), 23u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(32 * 32, 0), 31u);
}

TEST(CreatureCaveEffects, SmokeDriftsUpAndStartsOverAtItsPlace)
{
	CreatureCaveEffects::Smoke smoke;
	smoke.place = {10.0f, 0.0f, 0.0f};
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
		particle.position = smoke.place;
	}
	// A second: 255 steps of age, rising at 2.55 a second
	FakeRandom random;
	CreatureCaveEffects::StepSmoke(smoke, 1000, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 255);
	EXPECT_NEAR(smoke.particles[0].position.y, 2.55f, 1e-4f);
	EXPECT_TRUE(smoke.particles[0].hidden);
	// Past its life it starts over, seen, from the smoke's place, as far on as it went past
	smoke.particles[0].age = 890;
	smoke.particles[0].position = {50.0f, 50.0f, 50.0f};
	CreatureCaveEffects::StepSmoke(smoke, 100, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, (890 + 25) % 900);
	EXPECT_FALSE(smoke.particles[0].hidden);
	EXPECT_NEAR(smoke.particles[0].position.x, 10.0f, 1e-4f);
	EXPECT_NEAR(smoke.particles[0].position.y, 15.0f / 255.0f * 2.55f, 1e-4f);
	// Smoke that is not thrown up draws nothing as it starts over
	EXPECT_EQ(random.draws, 0);
}

TEST(CreatureCaveEffects, AgeTakesEachFramesWholeStepsOnly)
{
	CreatureCaveEffects::Smoke smoke;
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
	}
	FakeRandom random;
	// 2 ms is half a step of age, dropped each frame: the age stays
	CreatureCaveEffects::StepSmoke(smoke, 2, std::ref(random));
	CreatureCaveEffects::StepSmoke(smoke, 2, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 0);
	// 16 ms is 4.08 steps: 4 each frame
	CreatureCaveEffects::StepSmoke(smoke, 16, std::ref(random));
	CreatureCaveEffects::StepSmoke(smoke, 16, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 8);
	EXPECT_EQ(smoke.particles[9].age, 8);
}

TEST(CreatureCaveEffects, RisingSmokeIsThrownUpAsItStartsOver)
{
	CreatureCaveEffects::Smoke smoke;
	smoke.rising = true;
	smoke.drift = glm::vec3(0.0f);
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
	}
	smoke.particles[0].age = 899;
	FakeRandom random;
	CreatureCaveEffects::StepSmoke(smoke, 100, std::ref(random));
	// Three draws for the one particle that started over, thrown up at the middle of 3 to 4
	EXPECT_EQ(random.draws, 3);
	EXPECT_FALSE(smoke.particles[0].hidden);
	EXPECT_TRUE(smoke.particles[1].hidden);
	const float time = 24.0f / 255.0f;
	EXPECT_NEAR(smoke.particles[0].position.y, (3.5f * time) - (0.5f * 3.0f * 1.5f * time * time), 1e-4f);
}

TEST(CreatureCaveEffects, ARestartThrowsTheSprayUpDrawnInDepthFirst)
{
	CreatureCaveEffects::Smoke smoke;
	smoke.rising = true;
	smoke.drift = glm::vec3(0.0f);
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
	}
	smoke.particles[0].age = 899;
	RecordingRandom random {{0.5f, 3.25f, -1.0f}};
	CreatureCaveEffects::StepSmoke(smoke, 100, std::ref(random));
	const std::vector<std::pair<float, float>> bounds {{-4.0f, 4.0f}, {3.0f, 4.0f}, {-4.0f, 4.0f}};
	EXPECT_EQ(random.bounds, bounds);
	// The first draw goes in depth, the last across; only the height falls back
	EXPECT_FLOAT_EQ(smoke.particles[0].velocity.z, 0.5f);
	EXPECT_FLOAT_EQ(smoke.particles[0].velocity.x, -1.0f);
	const float time = 24.0f / 255.0f;
	EXPECT_FLOAT_EQ(smoke.particles[0].velocity.y, 3.25f - (3.0f * time * 1.5f));
}

TEST(CreatureCaveEffects, TheFlamesAreDrawnInDepthFirstThenTheirSize)
{
	RecordingRandom random {{0.1f, 0.2f, 0.3f, 0.4f}};
	const auto flame = CreatureCaveEffects::FlameOf({1.0f, 2.0f, 3.0f}, 2, std::ref(random));
	const std::vector<std::pair<float, float>> bounds {{-1.0f, 1.0f}, {1.0f, 2.0f}, {-1.0f, 1.0f}, {0.8f, 1.2f}};
	EXPECT_EQ(random.bounds, bounds);
	EXPECT_FLOAT_EQ(flame.place.x, 1.0f + 0.3f);
	EXPECT_FLOAT_EQ(flame.place.y, 2.0f + 0.2f);
	EXPECT_FLOAT_EQ(flame.place.z, 3.0f + 0.1f);
	// Each flame half a unit bigger than the one before it
	EXPECT_FLOAT_EQ(flame.size, (0.4f + 1.0f) + 2.0f);
}

TEST(CreatureCaveEffects, TheFiresSmokeIsTwoAboveItsDraw)
{
	RecordingRandom random {{0.1f, 0.2f, 0.3f}};
	const auto place = CreatureCaveEffects::FireSmokePlace({1.0f, 2.0f, 3.0f}, std::ref(random));
	const std::vector<std::pair<float, float>> bounds {{-1.0f, 1.0f}, {-1.0f, 1.0f}, {-1.0f, 1.0f}};
	EXPECT_EQ(random.bounds, bounds);
	EXPECT_FLOAT_EQ(place.x, 1.0f + 0.1f);
	EXPECT_FLOAT_EQ(place.y, (2.0f + 0.2f) + 2.0f);
	EXPECT_FLOAT_EQ(place.z, 3.0f + 0.3f);
}

TEST(CreatureCaveEffects, TheSprayAndTheMistsArePlacedFromTheLocalStream)
{
	// Each spray's place in a frame: depth, then down, then across
	LocalRecordingRandom random;
	auto place = CreatureCaveEffects::SprayPlace(std::ref(random));
	EXPECT_EQ(random.bounds, (std::vector<float> {1.0f, 23.0f, 5.0f}));
	EXPECT_FLOAT_EQ(place.x, (2.5f + 160.0f) - 2.5f);
	EXPECT_FLOAT_EQ(place.y, -30.0f - 11.5f);
	EXPECT_FLOAT_EQ(place.z, (0.5f - 16.0f) - 0.5f);
	// Its first place as the room starts, in the same order
	random.bounds.clear();
	place = CreatureCaveEffects::SprayFirstPlace(std::ref(random));
	EXPECT_EQ(random.bounds, (std::vector<float> {1.0f, 18.0f, 1.0f}));
	EXPECT_FLOAT_EQ(place.x, (0.5f + 115.0f) - 0.5f);
	EXPECT_FLOAT_EQ(place.y, -25.0f - 9.0f);
	EXPECT_FLOAT_EQ(place.z, (0.5f - 123.0f) - 0.5f);
	// The mists: across, then down, then in depth
	random.bounds.clear();
	place = CreatureCaveEffects::MistPlace(std::ref(random));
	EXPECT_EQ(random.bounds, (std::vector<float> {1.0f, 8.0f, 1.0f}));
	EXPECT_FLOAT_EQ(place.x, (0.5f + 160.0f) - 0.5f);
	EXPECT_FLOAT_EQ(place.y, -45.0f - 4.0f);
	EXPECT_FLOAT_EQ(place.z, (0.5f - 30.0f) - 0.5f);
}

TEST(CreatureCaveEffects, TheSprayDriftsAsDrawnInDepthFirst)
{
	std::vector<float> values {0.1f, 0.2f, 0.3f};
	size_t next = 0;
	const auto drift = CreatureCaveEffects::SprayDrift([&values, &next](float) { return values.at(next++); });
	EXPECT_FLOAT_EQ(drift.z, 0.1f - 0.5f);
	EXPECT_FLOAT_EQ(drift.y, 0.2f);
	EXPECT_FLOAT_EQ(drift.x, 0.3f - 0.5f);
}

TEST(CreatureCaveEffects, TheMistsAreAFaintBlueInTheTemplesLight)
{
	// In full light: 0x204080C0, each byte, its alpha too, by 255 over 256
	TempleLight light;
	EXPECT_EQ(CreatureCaveEffects::MistColour(light), 0x1F3F7FBFu);
	// Half light, 128 of 255; the alpha stays as in full light
	light.multiply = glm::vec3(128.0f / 255.0f);
	EXPECT_EQ(CreatureCaveEffects::MistColour(light), 0x1F204060u);
}

TEST(TempleWaterfall, SlidesByTheFramesWholeMillisecondsAsTheOriginal)
{
	// The original's single precision steps, frames of 16 milliseconds from the start
	float slide = 0.0f;
	slide = temple_waterfall::Advance(slide, 16);
	EXPECT_EQ(std::bit_cast<uint32_t>(slide), 0x39B02928u);
	slide = temple_waterfall::Advance(slide, 16);
	EXPECT_EQ(std::bit_cast<uint32_t>(slide), 0x3A302928u);
	slide = temple_waterfall::Advance(slide, 16);
	EXPECT_EQ(std::bit_cast<uint32_t>(slide), 0x3A841EDEu);
	// Down the waterfall only, ten of the texture over a whole slide
	EXPECT_EQ(temple_waterfall::Offset(slide).x, 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_waterfall::Offset(slide).y), 0xBC252696u);
}

TEST(TempleWaterfall, KeepsOnlyThePartPastAWholeSlide)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_waterfall::Advance(0.99999f, 1000)), 0x3CABF340u);
	// At 16 milliseconds a frame the first whole slide is passed on the 2977th frame
	float slide = 0.0f;
	for (int frame = 1; frame < 2977; ++frame)
	{
		slide = temple_waterfall::Advance(slide, 16);
	}
	const auto wrapped = temple_waterfall::Advance(slide, 16);
	EXPECT_LT(wrapped, slide);
	EXPECT_EQ(std::bit_cast<uint32_t>(wrapped), 0x3985A000u);
}

TEST(CreatureCaveEffects, TheRoomsSoundsAtTheFireAndTheWaterfall)
{
	const glm::vec3 fire {1.0f, 2.0f, 3.0f};
	const auto fireSound = temple_sounds::CreatureCaveFire(fire);
	EXPECT_EQ(fireSound.sound, static_cast<entt::id_type>(audio::SoundId::G_FireCreatureCave_01));
	EXPECT_TRUE(fireSound.is3D);
	EXPECT_FALSE(fireSound.track);
	EXPECT_EQ(fireSound.position, fire);
	const auto water = temple_sounds::CreatureCaveWater();
	EXPECT_EQ(water.sound, static_cast<entt::id_type>(audio::SoundId::G_WaterCreatureCave_01));
	EXPECT_TRUE(water.is3D);
	EXPECT_EQ(water.position, glm::vec3(160.0f, -45.0f, -30.0f));
	// Played every frame the room is drawn, they carry on while they play
	EXPECT_EQ(fireSound.mode, 2);
	EXPECT_EQ(water.mode, 2);
}
