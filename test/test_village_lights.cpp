/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's village light cases on ours: night_lights (when the lights come on, how bright they are by the hour, and
// their flicker) and frame_anim (the flames' clock and cells, and the sprite cells). Ours: the colour a house with
// people at home gives its windows at night, on a day-night clock of the test's own.
//
// Left out: his Strength, VillageStrength, Place and Threshold cases. Ours works those out inside
// night_lights::StampLight, with no function of their own to call, and our cell weights are worked out in floats where
// his are in doubles, so his corner case (100, 0) would weigh 255 with ours, not 254.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/NightLights.h"
#include "3D/NightLightsState.h"
#include "support/SkyFrameGuard.h"

using namespace openblack;
namespace frame_anim = openblack::graphics::frame_anim;

namespace
{
/// The land's colour 0xRRGGBB as our light table's base colour, 0 to 1 a channel
glm::vec3 BaseColour(uint32_t colour)
{
	return glm::vec3(static_cast<float>((colour >> 16u) & 0xFFu), static_cast<float>((colour >> 8u) & 0xFFu),
	                 static_cast<float>(colour & 0xFFu)) /
	       255.0f;
}

struct FlickerStep
{
	float timer;
	bool flickers;
};

/// One light's flicker timer after `milliseconds` more of game time, while it is dark, and whether it flickered: a
/// flicker draws three numbers
FlickerStep AdvanceFlicker(float timer, uint32_t milliseconds)
{
	std::vector<night_lights::VillageLight> lights(1);
	lights[0].timer = timer;
	int draws = 0;
	night_lights::Flicker(lights, true, milliseconds, [&draws](float low, float /*high*/) {
		++draws;
		return low;
	});
	return {.timer = lights[0].timer, .flickers = draws != 0};
}

/// The flames' clock after `milliseconds` more, and how far along the loop it is
std::pair<int, int> AdvanceFlames(int clock, uint32_t milliseconds)
{
	const int step = frame_anim::LanternAdvance(clock, milliseconds);
	return {clock, step};
}
} // namespace

TEST(VillageLights, ComeOnWhenTheLandIsDark)
{
	EXPECT_FALSE(night_lights::IsDark(BaseColour(0xFFFFFF)));
	EXPECT_FALSE(night_lights::IsDark(BaseColour(0x787878)));
	// (120 + 120 + 119) / 3 is under 120
	EXPECT_TRUE(night_lights::IsDark(BaseColour(0x787877)));
	EXPECT_TRUE(night_lights::IsDark(BaseColour(0x000000)));
}

TEST(VillageLights, FollowTheHour)
{
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(12.0f), 0.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(16.5f), 0.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(17.0f), 127.5f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(17.5f), 255.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(23.0f), 255.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(3.0f), 255.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(6.0f), 255.0f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(6.5f), 127.5f);
	EXPECT_FLOAT_EQ(night_lights::VillageLightIntensity(7.0f), 0.0f);
}

TEST(VillageLights, FlickerEveryThirtyMilliseconds)
{
	auto step = AdvanceFlicker(20.0f, 5);
	EXPECT_FLOAT_EQ(step.timer, 25.0f);
	EXPECT_FALSE(step.flickers);
	step = AdvanceFlicker(25.0f, 5);
	EXPECT_FLOAT_EQ(step.timer, 30.0f);
	EXPECT_FALSE(step.flickers);
	step = AdvanceFlicker(20.0f, 15);
	EXPECT_FLOAT_EQ(step.timer, 5.0f);
	EXPECT_TRUE(step.flickers);
	// Once however long the frame
	step = AdvanceFlicker(0.0f, 70);
	EXPECT_FLOAT_EQ(step.timer, 10.0f);
	EXPECT_TRUE(step.flickers);
}

TEST(VillageLights, FlamesLoopEverySevenHundredMilliseconds)
{
	auto step = AdvanceFlames(0, 350);
	EXPECT_EQ(step.first, 350);
	EXPECT_EQ(step.second, 15);
	// The clock only wraps once past the loop
	step = AdvanceFlames(600, 100);
	EXPECT_EQ(step.first, 700);
	EXPECT_EQ(step.second, 31);
	step = AdvanceFlames(600, 150);
	EXPECT_EQ(step.first, 50);
	EXPECT_EQ(step.second, 2);
}

TEST(VillageLights, FlamesPlayBackwards)
{
	// Our flames take their start from the table every light shares, one entry a flame
	EXPECT_EQ(frame_anim::LanternCell(0, 0, {0, 0, 0}), 31);
	EXPECT_EQ(frame_anim::LanternCell(1, 0, {0, 0, 0}), 30);
	// The second flame is ten cells on, from where it starts
	EXPECT_EQ(frame_anim::LanternCell(1, 1, {0, 13, 0}), (10 + 31 - 14) & 31);
	EXPECT_EQ(frame_anim::LanternCell(20, 0, {13, 0, 0}), (31 - 1) & 31);
}

TEST(VillageLights, SpriteCellsAreEightAcross)
{
	// The top left of a cell: the first of its four corners
	EXPECT_EQ(frame_anim::SpriteCellUv(0)[0], glm::vec2(0.0f));
	EXPECT_EQ(frame_anim::SpriteCellUv(56)[0], glm::vec2(0.0f, 7.0f / 8.0f));
	EXPECT_EQ(frame_anim::SpriteCellUv(11)[0], glm::vec2(3.0f / 8.0f, 1.0f / 8.0f));
}

namespace
{
/// A clock whose script hours 1.75 and 22.25 are the visual hours 3 and 21, both at night: the whole night cycle at
/// the visual hour 6 (half the day night, no change between)
DayNightClock NightClock(float scriptHour)
{
	DayNightClock clock;
	clock.Reset();
	clock.SetCycle(DayNightClock::k_DefaultDuration, 0.5f, 0.0f);
	clock.SetScriptTime(scriptHour);
	return clock;
}
} // namespace

TEST(VillageLights, WindowsOfAHomeWithPeopleInLightUpAtNight)
{
	const test::SkyFrameGuard sky;
	// Midnight: t = 0 + the house's offset, the fraction of |x + z| 0.1 + y = 0.25. Its strength (3 - t) 8192 is over
	// 255, so the grey is whole: flicker[trunc(250) & 7] = 3, (3 << 2) & 0x1F | 0xE0 = 0xEC
	const auto midnight = NightClock(0.0f);
	ASSERT_FLOAT_EQ(midnight.GetVisualTime(), 0.0f);
	ASSERT_TRUE(midnight.IsVisualNight());
	EXPECT_EQ(night_lights::WindowColour(midnight, {10.0f, 0.25f, 20.0f}, true), 0xFFECECECu);
	// Nobody at home: lit as by day
	EXPECT_EQ(night_lights::WindowColour(midnight, {10.0f, 0.25f, 20.0f}, false), 0u);

	// The visual hour 3: off from there on, until 20.5
	const auto three = NightClock(1.75f);
	ASSERT_FLOAT_EQ(three.GetVisualTime(), 3.0f);
	ASSERT_TRUE(three.IsVisualNight());
	EXPECT_EQ(night_lights::WindowColour(three, {0.0f, 0.0f, 0.0f}, true), 0u);
	// The offset is cut towards nought, as the original's: y = -1/64 takes t back to 2.984375, a strength of 128 that
	// halves the grey, flicker[2984 & 7] = 0: 0xE0 x 128 >> 8 = 0x70
	EXPECT_EQ(night_lights::WindowColour(three, {0.0f, -0.015625f, 0.0f}, true), 0xFF707070u);

	// The visual hour 21: (t - 20.5) 8192 = 4096, flicker[21000 & 7] = 0 and with the offset 0.25 flicker[21250 & 7] = 3
	const auto evening = NightClock(22.25f);
	ASSERT_FLOAT_EQ(evening.GetVisualTime(), 21.0f);
	ASSERT_TRUE(evening.IsVisualNight());
	EXPECT_EQ(night_lights::WindowColour(evening, {0.0f, 0.0f, 0.0f}, true), 0xFFE0E0E0u);
	EXPECT_EQ(night_lights::WindowColour(evening, {10.0f, 0.25f, 20.0f}, true), 0xFFECECECu);

	// By day nobody's windows light up
	DayNightClock noon;
	noon.Reset();
	ASSERT_FALSE(noon.IsVisualNight());
	EXPECT_EQ(night_lights::WindowColour(noon, {10.0f, 0.25f, 20.0f}, true), 0u);
}
