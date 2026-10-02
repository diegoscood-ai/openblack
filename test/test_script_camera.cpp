/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// script_camera:: (src/Camera/ScriptCamera.h) against runblack.exe: fn_00461140 / CameraModeScript::CanExit 0x461B70,
// MoveCameraPosition 0x4616F0, CameraMode::Arrived 0x441700, GCamera::Update 0x441F80 (0.1 s cap, disc of the world,
// the drawn camera's nudge and ground clearance), SetCameraFov 0x443680 and fn_006ECD70

#include <cmath>
#include <functional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCamera.h"

using namespace openblack;

namespace
{
std::function<float(float, float)> Ground(float height)
{
	return [height](float, float) { return height; };
}

void Frames(int n, float seconds)
{
	for (int i = 0; i < n; ++i)
	{
		script_camera::Frame(seconds, 0, 0.0f);
	}
}
} // namespace

TEST(ScriptCamera, OneScriptModeAtATime)
{
	script_camera::Reset();
	EXPECT_FALSE(script_camera::Active());
	EXPECT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10}));
	EXPECT_TRUE(script_camera::Active());
	EXPECT_FALSE(script_camera::Begin({0, 10, 0}, {0, 0, 10})); // CanExit 0x461B70 = 0 while it lives
	EXPECT_TRUE(script_camera::End());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_FALSE(script_camera::End()); // no script mode: only the FOV goes back
	EXPECT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10}));
	script_camera::Reset();
}

TEST(ScriptCamera, BeginStartsFromTheDrawnCamera)
{
	script_camera::Reset();
	script_camera::Begin({100, 20, 50}, {110, 5, 60});
	EXPECT_TRUE(script_camera::ScriptArrived()); // nothing to move to
	const auto drawn = script_camera::DrawnCamera(Ground(0.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 20, 50));
	EXPECT_EQ(drawn.focus, glm::vec3(110, 5, 60));
	script_camera::Reset();
}

TEST(ScriptCamera, MoveArrivesAfterItsTime)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1200, 80, 900}, 2.0f);
	script_camera::MoveFocus({1300, 10, 1000}, 1.0f);
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(19, 0.1f); // 1.9 s
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(2, 0.1f); // Zoomer::Update 0x442727: at t >= T the value is the destination
	EXPECT_TRUE(script_camera::ScriptArrived());
	const auto drawn = script_camera::DrawnCamera(Ground(0.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(1200, 80, 900));
	EXPECT_EQ(drawn.focus, glm::vec3(1300, 10, 1000));
	script_camera::Reset();
}

TEST(ScriptCamera, CameraSecondsCappedAtATenth)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1010, 50, 1000}, 1.0f);
	Frames(5, 1.0f); // 0x441FB0: 5 frames of 1 s are 0.5 s
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(6, 1.0f);
	EXPECT_TRUE(script_camera::ScriptArrived());
	script_camera::Reset();
}

TEST(ScriptCamera, ShortTimeSetsAtOnce)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1500, 60, 1500}, 0.0005f); // 0x407D67: T < 0.001
	EXPECT_TRUE(script_camera::ScriptArrived());
	EXPECT_EQ(script_camera::DrawnCamera(Ground(0.0f)).origin, glm::vec3(1500, 60, 1500));
	script_camera::Reset();
}

TEST(ScriptCamera, DiscOfTheWorld)
{
	script_camera::Reset();
	script_camera::Begin({2560, 50, 2560}, {2600, 0, 2600});
	script_camera::MovePosition({2560 + 7000, 50, 2560}, 1.0f);
	Frames(1, 0.01f);
	// 0x44222C..0x44232A: the destination pulled back to d / (|d| 0.000285796) + centre in 3 s
	const auto destination = script_camera::Get().position.Destination();
	const float d = std::sqrt(7000.0f * 7000.0f + 50.0f * 50.0f);
	EXPECT_NEAR(destination.x, 2560.0f + 7000.0f / (d * script_camera::k_DiscScale), 0.05f);
	EXPECT_NEAR(destination.y, 50.0f / (d * script_camera::k_DiscScale), 0.01f);
	EXPECT_NEAR(script_camera::Get().position.axis[0].duration, 3.0f, 1e-6f);
	script_camera::Reset();
}

TEST(ScriptCamera, DrawnCamera)
{
	script_camera::Reset();
	script_camera::Begin({100, 5, 100}, {100.01f, 5, 100});
	// 0x4421D5: the same point -> position.x - 1, y + 1
	auto drawn = script_camera::DrawnCamera(Ground(-100.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(99, 6, 100));
	// 0x44242B: 1 m above the ground, the focus lifted as much
	script_camera::SetPositionAndFocus({100, 5, 100}, {150, 2, 100});
	drawn = script_camera::DrawnCamera(Ground(10.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 11, 100));
	EXPECT_EQ(drawn.focus, glm::vec3(150, 8, 100));
	drawn = script_camera::DrawnCamera(Ground(3.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 5, 100));
	script_camera::Reset();
}

TEST(ScriptCamera, FovFollowsGameTime)
{
	script_camera::Reset();
	auto& fov = script_camera::Get().fov;
	EXPECT_FLOAT_EQ(fov.value, script_camera::k_DefaultFov);
	script_camera::SetFov(40.0f * script_camera::k_DegreesToRadians, 1.0f);
	script_camera::Frame(0.1f, 0, 0.0f); // paused: g_game_time_inc = 0
	EXPECT_FLOAT_EQ(fov.value, script_camera::k_DefaultFov);
	for (int i = 0; i < 11; ++i)
	{
		script_camera::Frame(0.0f, 100, 0.1f);
	}
	EXPECT_FLOAT_EQ(fov.value, 40.0f * script_camera::k_DegreesToRadians);
	script_camera::Begin({0, 10, 0}, {0, 0, 10});
	script_camera::End(); // fn_006ECD70: 70 degrees in 0.5 s
	EXPECT_FLOAT_EQ(fov.destination, script_camera::k_DefaultFov);
	EXPECT_FLOAT_EQ(fov.duration, 0.5f);
	script_camera::Reset();
}

TEST(ScriptCamera, SetDropsTheMove)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1200, 80, 900}, 2.0f);
	script_camera::SetPosition({1050, 40, 1000}); // 0x461370: Zoomer::SetPosition
	EXPECT_TRUE(script_camera::ScriptArrived());
	script_camera::Reset();
}
