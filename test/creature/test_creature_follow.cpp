/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creature Mode's pen and the follow camera against the script camera's; the follow camera's and Creature Mode's
// other tests are in test_creature_mode.cpp

#include <array>

#include <gtest/gtest.h>

#include "Camera/CreatureFollow.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureMode.h"
#include "ECS/ObjectMetrics.h"

using namespace openblack;

namespace
{
creature_follow::View View(float yaw, float pitch, float distance)
{
	return {.yaw = yaw, .pitch = pitch, .distance = distance};
}
} // namespace

// The mode's rules

TEST(CreatureMode, PenIsTheHomeThenTheTempleThenTheFallback)
{
	const glm::vec3 home {1.0f, 0.0f, 1.0f};
	const glm::vec3 temple {2.0f, 0.0f, 2.0f};
	const glm::vec3 none {3.0f, 0.0f, 3.0f};
	EXPECT_EQ(creature_mode::PenOf(home, temple, none), home);
	EXPECT_EQ(creature_mode::PenOf(std::nullopt, temple, none), temple);
	EXPECT_EQ(creature_mode::PenOf(std::nullopt, std::nullopt, none), none);
}

// The follow is the script camera's

TEST(CreatureFollow, TurnsAndPlacesAsTheScriptCameraFollows)
{
	const glm::vec3 focus {1900.0f, 40.0f, 2500.0f};
	for (const auto& [x, y, z] : {std::array {30.0f, 20.0f, -50.0f}, std::array {-80.0f, 5.0f, 10.0f},
	                              std::array {0.005f, 60.0f, -0.002f}, std::array {-3.0f, -2.0f, -400.0f}})
	{
		const auto point = focus + glm::vec3(x, y, z);
		float heading = 0.0f;
		float pitch = 0.0f;
		script_camera::HeadingAndPitchFromPoints(point, focus, heading, pitch);
		const auto turned = creature_follow::HeadingPitchOf(point, focus);
		EXPECT_EQ(turned.heading, heading);
		EXPECT_EQ(turned.pitch, pitch);
		const auto view = View(heading, pitch, 75.0f);
		EXPECT_EQ(creature_follow::Origin(focus, view),
		          script_camera::PointFromDistanceHeadingAndPitch(focus, 75.0f, heading, pitch));
	}
}

TEST(CreatureFollow, EasesAndKeepsWithinTheScriptCamerasBounds)
{
	for (const auto seconds : {0.0f, 0.5f, 1.0f, 1.999f, 2.0f, 2.5f, 60.0f})
	{
		EXPECT_EQ(creature_follow::EaseSeconds(seconds), script_camera::FollowSeconds(seconds, 1.0f)) << seconds;
	}
	for (const auto distance : {-1.0f, 1.0f, 2.0f, 80.0f, 1499.0f, 1500.0f, 9000.0f})
	{
		for (const auto pitch : {-1.0f, 0.0f, 0.2416609824f, 0.3f, 1.4f})
		{
			const auto view = creature_follow::Clamped(View(0.0f, pitch, distance));
			EXPECT_EQ(view.distance, script_camera::ClampFollowDistance(distance));
			EXPECT_EQ(view.pitch, script_camera::FollowPitch(pitch));
		}
	}
	// The named bounds are the script camera's
	EXPECT_EQ(script_camera::ClampFollowDistance(0.0f), creature_follow::k_MinDistance);
	EXPECT_EQ(script_camera::ClampFollowDistance(1e6f), creature_follow::k_MaxDistance);
	EXPECT_EQ(script_camera::FollowPitch(-1.0f), creature_follow::k_MinPitch);
	EXPECT_EQ(creature_follow::ViewingDistance(15.0f), script_camera::ThingViewingDistance(15.0f));
	EXPECT_EQ(creature_follow::ViewingDistance(1.0f), creature_follow::k_ViewingDistancePerHeight);
}

TEST(CreatureMode, ACreaturesHeightIsTheOneItsSizeGivesEverywhere)
{
	EXPECT_EQ(creature_mode::k_HeightPerSize, ecs::object::k_CreatureHeightPerScale);
}
