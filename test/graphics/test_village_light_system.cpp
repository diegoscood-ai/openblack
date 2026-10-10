/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// VillageLightSystem: the night lights come on when the mean of the land's colour is under 120; the system reports
// the last frame's answer (IsDark), and does nothing without a land or a day and night clock. A light makes six draws
// when its lantern is made, and three per 30 ms tick while it is dark only, checked with a fake stream.

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/NightLights.h"
#include "3D/NightLightsState.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::systems::VillageLightSystem;
using Milliseconds = std::chrono::duration<float, std::milli>;
using Range = std::pair<float, float>;

namespace
{
/// A fake stream that keeps the range of every draw and gives its bottom
night_lights::Draw Recording(std::vector<Range>& draws)
{
	return [&draws](float a, float b) {
		draws.emplace_back(a, b);
		return a;
	};
}

/// Three lights made from a stream that is thrown away, their flicker clocks at 0
std::vector<night_lights::VillageLight> ThreeLights()
{
	std::vector<Range> unused;
	auto starts = graphics::frame_anim::k_LanternFileStarts;
	std::vector<night_lights::VillageLight> lights;
	for (int i = 0; i < 3; ++i)
	{
		lights.push_back(night_lights::MakeLight(static_cast<entt::entity>(i), glm::vec3(10.0f * static_cast<float>(i)), 0,
		                                         starts, Recording(unused)));
	}
	return lights;
}

constexpr Range k_Jitter {-0.5f, 0.5f};
constexpr Range k_Size {-0.1f, 0.1f};
} // namespace

TEST(VillageLightSystem, TheLightsComeOnWhenTheLandIsDark)
{
	EXPECT_FALSE(night_lights::IsDark(LandLightTable::ToColour(0xFFFFFFFFu)));
	EXPECT_FALSE(night_lights::IsDark(LandLightTable::ToColour(0xFF787878u)));
	// (120 + 120 + 119) / 3 is under 120
	EXPECT_TRUE(night_lights::IsDark(LandLightTable::ToColour(0xFF787877u)));
	EXPECT_TRUE(night_lights::IsDark(LandLightTable::ToColour(0xFF000000u)));
	// the alpha byte does not count
	EXPECT_TRUE(night_lights::IsDark(LandLightTable::ToColour(0x00787877u)));
}

TEST(VillageLightSystem, TheMeanIsOfTheWholeBytes)
{
	EXPECT_FLOAT_EQ(night_lights::LandColourMean(LandLightTable::ToColour(0xFF787877u)), (120.0f + 120.0f + 119.0f) / 3.0f);
	EXPECT_FLOAT_EQ(night_lights::LandColourMean(LandLightTable::ToColour(0xFF0000FFu)), 85.0f);
	// a channel between two bytes is rounded to the nearer
	EXPECT_FLOAT_EQ(night_lights::LandColourMean(glm::vec3(120.4f / 255.0f)), 120.0f);
	EXPECT_FLOAT_EQ(night_lights::LandColourMean(glm::vec3(119.6f / 255.0f)), 120.0f);
}

TEST(VillageLightSystem, NotDarkBeforeTheFirstUpdate)
{
	const VillageLightSystem lights;
	EXPECT_FALSE(lights.IsDark());
}

TEST(VillageLightSystem, NothingHappensWithoutALand)
{
	const test::RestoreService<Locator::terrainSystem> terrain;
	const test::RestoreService<Locator::dayNightClock> clock;
	Locator::terrainSystem::reset();
	Locator::dayNightClock::reset();
	VillageLightSystem lights;

	lights.Update(Milliseconds(16.0f));

	EXPECT_FALSE(lights.IsDark());
	// the images were not loaded and no light was found
	EXPECT_FALSE(lights.GetState().loaded);
	EXPECT_TRUE(lights.GetState().lights.empty());
}

TEST(VillageLightSystem, ALightDrawsSixNumbersWhenItIsMadeInTheOriginalsOrder)
{
	std::vector<Range> draws;
	auto starts = graphics::frame_anim::k_LanternFileStarts;
	const auto light =
	    night_lights::MakeLight(static_cast<entt::entity>(7), glm::vec3(1.0f, 2.0f, 3.0f), 1, starts, Recording(draws));

	// the clock, then each sprite's size (the two flames only) and start: clock, size0, start0, size1, start1, start2
	const std::vector<Range> expected {{0.0f, 30.0f}, k_Size, {0.0f, 31.0f}, k_Size, {0.0f, 31.0f}, {0.0f, 31.0f}};
	EXPECT_EQ(draws, expected);
	EXPECT_EQ(light.owner, static_cast<entt::entity>(7));
	EXPECT_EQ(light.position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(light.type, 1);
	EXPECT_EQ(light.timer, 0.0f);
	EXPECT_FLOAT_EQ(light.flameSize[0], 0.9f);
	EXPECT_FLOAT_EQ(light.flameSize[1], 0.9f);
	// every sprite rewrites its entry of the shared start table, the glow's too
	for (const int start : starts)
	{
		EXPECT_EQ(start, graphics::frame_anim::LanternStart(0.0f));
	}
}

TEST(VillageLightSystem, NoFlickerDrawsWhileItIsLight)
{
	auto lights = ThreeLights();
	std::vector<Range> draws;

	night_lights::Flicker(lights, false, 100, Recording(draws));

	EXPECT_TRUE(draws.empty());
	// the clocks stand still
	for (const auto& light : lights)
	{
		EXPECT_EQ(light.timer, 0.0f);
		EXPECT_EQ(light.offset, glm::vec2(0.0f));
	}
}

TEST(VillageLightSystem, WhileDarkEachLightDrawsThreeNumbersPerTick)
{
	auto lights = ThreeLights();
	std::vector<Range> draws;
	// the newest light, at the end of the list, draws first
	std::vector<float> answers {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
	night_lights::Flicker(lights, true, 31, [&](float a, float b) {
		draws.emplace_back(a, b);
		const float answer = answers.front();
		answers.erase(answers.begin());
		return answer;
	});

	const std::vector<Range> expected {k_Jitter, k_Jitter, k_Size, k_Jitter, k_Jitter, k_Size, k_Jitter, k_Jitter, k_Size};
	EXPECT_EQ(draws, expected);
	EXPECT_EQ(lights[2].offset, glm::vec2(0.1f, 0.2f));
	EXPECT_FLOAT_EQ(lights[2].glowSize, 3.3f);
	EXPECT_EQ(lights[0].offset, glm::vec2(0.7f, 0.8f));
	EXPECT_FLOAT_EQ(lights[0].glowSize, 3.9f);
	// 31 ms wraps to 1; 10 ms more is not a tick
	for (const auto& light : lights)
	{
		EXPECT_FLOAT_EQ(light.timer, 1.0f);
	}
	draws.clear();
	night_lights::Flicker(lights, true, 10, Recording(draws));
	EXPECT_TRUE(draws.empty());
}

TEST(VillageLightSystem, AddingALightDrawsOnlyForTheNewOne)
{
	std::vector<Range> draws;
	VillageLightSystem lights(Recording(draws));

	lights.AddLight(static_cast<entt::entity>(1), glm::vec3(5.0f), 0);
	ASSERT_EQ(draws.size(), 6u);
	const auto first = lights.GetState().lights.at(0);

	lights.AddLight(static_cast<entt::entity>(2), glm::vec3(9.0f), 1);

	// six more draws, and the first light is as it was made
	EXPECT_EQ(draws.size(), 12u);
	const auto& state = lights.GetState();
	ASSERT_EQ(state.lights.size(), 2u);
	EXPECT_EQ(state.lights[0].owner, first.owner);
	EXPECT_EQ(state.lights[0].position, first.position);
	EXPECT_EQ(state.lights[0].timer, first.timer);
	EXPECT_EQ(state.lights[0].flameSize, first.flameSize);
	EXPECT_EQ(state.lights[0].glowSize, first.glowSize);
	// the new light goes after it: the flicker walks the list newest first
	EXPECT_EQ(state.lights[1].owner, static_cast<entt::entity>(2));
	EXPECT_EQ(state.lights[1].type, 1);
}
