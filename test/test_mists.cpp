/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "3D/Mists.h"
#include "ECS/MistFades.h"
#include "Graphics/Haze.h"
#include "Graphics/Mists.h"
#include "Graphics/ModelLight.h"

namespace mists = openblack::mists;
namespace mist_fade = openblack::ecs::mist_fade;

TEST(Mists, FramesFollowTheCounter)
{
	EXPECT_EQ(mists::Frame(0), 0);
	EXPECT_EQ(mists::Frame(20), 1);
	EXPECT_EQ(mists::Frame(319), 15);
	// Past sixteen frames they start again, and the counter's own end is frame 13
	EXPECT_EQ(mists::Frame(320), 0);
	EXPECT_EQ(mists::Frame(900), 13);

	EXPECT_EQ(mists::FrameOffset(9, false), glm::vec2(0.125f, 0.125f));
	EXPECT_EQ(mists::FrameOffset(9, true), glm::vec2(0.125f, 0.375f));
	EXPECT_EQ(mists::StartCounter(15.9f), 15);
}

TEST(Mists, AnimationTakesEachFramesWholeStepsOnly)
{
	// A millisecond at a time is a quarter of a step each frame, dropped every frame: the counter stays
	int counter = 0;
	for (int i = 0; i < 1000; ++i)
	{
		mists::Advance(counter, 1);
	}
	EXPECT_EQ(counter, 0);
	// A whole second at once is 255 steps; sixteen frames of 16 ms are 4 steps each, not 4.08
	int once = 0;
	mists::Advance(once, 1000);
	EXPECT_EQ(once, 255);
	int frames = 0;
	for (int i = 0; i < 16; ++i)
	{
		mists::Advance(frames, 16);
	}
	EXPECT_EQ(frames, 64);

	// It wraps only once it passes 900
	int full = 900;
	mists::Advance(full, 0);
	EXPECT_EQ(full, 900);
	mists::Advance(full, 4);
	EXPECT_EQ(full, 1);
}

TEST(Mists, ShrinkEdgeOn)
{
	// Seen from straight above: the full size; seen level: edgeShrink times smaller
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {0.0f, -100.0f, 0.0f}), 10.0f);
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {100.0f, 0.0f, 0.0f}), 10.0f / 3.0f);
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {0.0f, 0.0f, 0.0f}), 10.0f);
}

TEST(Mists, MistFadeTakesTenStepsASecondAndThenEnds)
{
	// The one fade of the shipped scripts: scale 1 to 2 and alpha 50 to 0 over 10 seconds
	auto fade = mist_fade::Start(1.0f, 2.0f, 50.0f, 0.0f, 10.0f);
	EXPECT_EQ(fade.steps, 100.0f);
	EXPECT_EQ(fade.alphaStep, -0.5f);
	float size = 1.0f;
	for (int i = 0; i < 100; ++i)
	{
		ASSERT_TRUE(mist_fade::Step(fade, size)) << i;
	}
	EXPECT_EQ(fade.alpha, 0.0f);
	EXPECT_NEAR(size, 2.0f, 1e-4f);
	// the step past the last one ends the fade and changes nothing
	const float last = size;
	EXPECT_FALSE(mist_fade::Step(fade, size));
	EXPECT_EQ(size, last);
	EXPECT_EQ(fade.taken, 100.0f);
}

TEST(Mists, MistFadeKeepsItsTransparencyToAByte)
{
	// above 255 is 255, below nought is nought; the alpha is the whole part
	auto up = mist_fade::Start(1.0f, 1.0f, 250.0f, 300.0f, 1.0f);
	float size = 1.0f;
	ASSERT_TRUE(mist_fade::Step(up, size));
	EXPECT_EQ(up.alpha, 255.0f);
	auto down = mist_fade::Start(1.0f, 1.0f, 3.0f, -50.0f, 1.0f);
	ASSERT_TRUE(mist_fade::Step(down, size));
	EXPECT_EQ(down.alpha, 0.0f);
	EXPECT_EQ(mist_fade::AlphaOf(25.5f), 25u);
	EXPECT_EQ(mist_fade::AlphaOf(255.0f), 255u);
	EXPECT_EQ(mist_fade::AlphaOf(0.9f), 0u);
}

TEST(Mists, EdgeShrinkingPuffsAreLitFromAboveTheOthersByTheLand)
{
	EXPECT_EQ(mists::LightOf(true, true), mists::Light::FromAbove);
	EXPECT_EQ(mists::LightOf(true, false), mists::Light::FromAbove);
	EXPECT_EQ(mists::LightOf(false, true), mists::Light::Land);
	EXPECT_EQ(mists::LightOf(false, false), mists::Light::Frame);
	// straight above, far up, with ambient 210
	EXPECT_EQ(mists::k_LightFromAbove, glm::vec3(0.0f, 500000.0f, 0.0f));
	EXPECT_EQ(openblack::model_light::k_MistAmbient, 210);
}

TEST(Mists, TheLandCellsColourAddsToAMistsLight)
{
	namespace haze = openblack::graphics::haze;
	constexpr uint32_t k_Colour = 0x80FF8040u;
	constexpr uint32_t k_LandLight = 0xFF808080u;
	constexpr uint32_t k_Cells = 0xFF102030u;
	// No haze: the colour times the land's light byte by byte, trunc(c l / 255), and the cells' colour as the specular
	const auto plain = mists::LitByLand(k_Colour, k_LandLight, k_Cells, haze::Params {}, 1000.0f);
	EXPECT_EQ(plain.rgb, glm::vec3(128.0f, 64.0f, 32.0f));
	EXPECT_EQ(plain.specular, glm::vec3(16.0f, 32.0f, 48.0f));
	// Nearer than the haze starts it stays so
	const auto nearer = mists::LitByLand(k_Colour, k_LandLight, k_Cells, haze::Params {.on = true}, 10.0f);
	EXPECT_EQ(nearer.rgb, plain.rgb);
	EXPECT_EQ(nearer.specular, plain.specular);
	// Past the haze's far end: the haze colour (64) is added to the cells' colour, and the light darkened by k / 256
	const auto farther = mists::LitByLand(k_Colour, k_LandLight, k_Cells, haze::Params {.on = true, .k = 128}, 1000.0f);
	EXPECT_EQ(farther.specular, glm::vec3(80.0f, 96.0f, 112.0f));
	EXPECT_EQ(farther.rgb, glm::vec3(64.0f, 32.0f, 16.0f));
}
