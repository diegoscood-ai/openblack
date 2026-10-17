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

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/ModelLight.h"

using namespace openblack;

namespace
{
constexpr glm::vec3 k_Hand {100.0f, 50.0f, 100.0f};
constexpr glm::vec3 k_Camera {100.0f, 50.0f, 200.0f};
constexpr float k_Night = 0.0f;
constexpr float k_Day = 2.0f;
} // namespace

TEST(ModelLight, TheSunByDay)
{
	EXPECT_EQ(model_light::FrameLight(k_Hand, 0.0f, k_Camera, k_Day, false), model_light::k_Sun);
}

TEST(ModelLight, TheHandCarriesTheLightAtNight)
{
	const auto light = model_light::FrameLight(k_Hand, 0.0f, k_Camera, k_Night, false);
	// A little before the hand, towards the camera
	EXPECT_NEAR(glm::distance(light, k_Hand), 3.0f, 1e-4f);
	EXPECT_GT(light.z, k_Hand.z);
	// Never lower than 10 over the ground under the hand
	const auto low = model_light::FrameLight({100.0f, 0.0f, 100.0f}, 5.0f, k_Camera, k_Night, false);
	EXPECT_GE(low.y, 15.0f - 3.0f);
}

TEST(ModelLight, NoNightLightOnTheHandInsideTheTemple)
{
	// Inside the temple the hand carries no light, so it doesn't light itself there
	EXPECT_EQ(model_light::FrameLight(k_Hand, 0.0f, k_Camera, k_Night, true), model_light::k_Sun);
	EXPECT_EQ(model_light::FrameLight(k_Hand, 0.0f, k_Camera, k_Day, true), model_light::k_Sun);
}

TEST(ModelLight, AnimatedObjectsTakeTheirOwnRule)
{
	// Facing away, and edge on, they keep 0.4 of the light; there is no ambient term
	EXPECT_EQ(model_light::AnimatedIntensity(-1.0f), 102);
	EXPECT_EQ(model_light::AnimatedIntensity(0.0f), 102);
	// The share faced is taken 1.3 times, truncated
	EXPECT_EQ(model_light::AnimatedIntensity(0.25f), 184);
	// Full from about 0.46 on
	EXPECT_EQ(model_light::AnimatedIntensity(0.47f), 255);
	EXPECT_EQ(model_light::AnimatedIntensity(1.0f), 255);
	// Each channel times it, shifted down by 8, the alpha kept
	EXPECT_EQ(model_light::ApplyAnimated(0x80FF4020u, 0.0f), 0x8065190Cu);
	EXPECT_EQ(model_light::ApplyAnimated(0xFFFFFFFFu, 1.0f), 0xFFFEFEFEu);
}

namespace
{
// Read from the original game's own vertex light for animated objects, run on these N.L values with the object
// colours 0x80FF4020 and 0x01818181: the intensity and the vertex colour it wrote, bit for bit
struct AnimatedLightCase
{
	uint32_t dot;
	int intensity;
	uint32_t orange;
	uint32_t grey;
};
constexpr std::array<AnimatedLightCase, 18> k_AnimatedLightCases {{
    {0xBF800000u, 102, 0x8065190Cu, 0x01333333u}, // facing away
    {0x80000000u, 102, 0x8065190Cu, 0x01333333u}, // minus zero, edge on
    {0x00000000u, 102, 0x8065190Cu, 0x01333333u}, // edge on
    {0x0DA24260u, 102, 0x8065190Cu, 0x01333333u}, // barely facing (1e-30)
    {0x3C23D70Au, 105, 0x80681A0Du, 0x01343434u}, // 0.01
    {0x3DCCCCCDu, 135, 0x80862110u, 0x01444444u}, // 0.1
    {0x3E7D4C11u, 183, 0x80B62D16u, 0x015C5C5Cu}, // the last value truncated to 183
    {0x3E7D4C12u, 184, 0x80B72E17u, 0x015C5C5Cu}, // the first value reaching 184
    {0x3E800000u, 184, 0x80B72E17u, 0x015C5C5Cu}, // 0.25
    {0x3ECCCCCDu, 234, 0x80E93A1Du, 0x01757575u}, // 0.4
    {0x3EEB851Fu, 254, 0x80FD3F1Fu, 0x017F7F7Fu}, // 0.46, not yet full
    {0x3EEC4EC3u, 254, 0x80FD3F1Fu, 0x017F7F7Fu}, // the last value below the clamp
    {0x3EEC4EC4u, 255, 0x80FE3F1Fu, 0x01808080u}, // exactly at the clamp
    {0x3F800000u, 255, 0x80FE3F1Fu, 0x01808080u}, // facing the light
    {0x40000000u, 255, 0x80FE3F1Fu, 0x01808080u}, // above 1
    {0x7F800000u, 255, 0x80FE3F1Fu, 0x01808080u}, // infinity
    {0xFF800000u, 102, 0x8065190Cu, 0x01333333u}, // minus infinity
    {0x7FC00000u, 102, 0x8065190Cu, 0x01333333u}, // NaN counts as facing away
}};
} // namespace

TEST(ModelLight, AnimatedObjectsMatchTheOriginal)
{
	for (const auto& c : k_AnimatedLightCases)
	{
		const float dot = std::bit_cast<float>(c.dot);
		EXPECT_EQ(model_light::AnimatedIntensity(dot), c.intensity) << std::hex << c.dot;
		EXPECT_EQ(model_light::ApplyAnimated(0x80FF4020u, dot), c.orange) << std::hex << c.dot;
		EXPECT_EQ(model_light::ApplyAnimated(0x01818181u, dot), c.grey) << std::hex << c.dot;
	}
}
