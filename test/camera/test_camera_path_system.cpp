/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A camera path placed in the world, as a miracle's effect takes its caster's camera along: refused while a script
// holds the camera, it glides the camera onto the path over the pause and the lag, follows the path's time 0.3 s behind
// once it plays, lets go when its owner releases it, and gives the camera back to a movement key or a land grip. And
// the pure rules: the glide's time, the point placed in the original's order, the path's time at a drawn frame, the
// last frame

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <algorithm>
#include <bit>
#include <chrono>
#include <memory>
#include <vector>

#include <entt/resource/resource.hpp>
#include <glm/gtx/transform.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "Camera/Camera.h"
#include "Camera/CameraPathControl.h"
#include "Common/Zoomer.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::ecs::systems::CameraPathSystem;

namespace
{
/// A .cam file of a path along x, looking ahead of itself
std::vector<uint8_t> MakeCam(uint32_t durationMs, uint32_t pointCount)
{
	std::vector<uint8_t> data(3 * sizeof(uint32_t) + pointCount * 6 * sizeof(float));
	const uint32_t header[3] {static_cast<uint32_t>(data.size()), durationMs, pointCount};
	std::memcpy(data.data(), header, sizeof(header));
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		const float point[6] {static_cast<float>(i) * 10.0f, 5.0f, 0.0f, static_cast<float>(i) * 10.0f + 1.0f, 5.0f, 0.0f};
		std::memcpy(&data[sizeof(header) + i * sizeof(point)], point, sizeof(point));
	}
	return data;
}

constexpr CameraPathSystem::PathOwner k_Owner = 7;
constexpr CameraPathSystem::PathOwner k_OtherOwner = 8;

class CameraPathSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		_camera = &Locator::camera::emplace(glm::vec3(2560.0f, 0.0f, 2560.0f));
	}

	/// A path of a second over 11 points, as the resource cache holds one
	[[nodiscard]] static entt::resource<CameraPath> Path()
	{
		auto path = std::make_shared<CameraPath>("test");
		EXPECT_TRUE(path->LoadFromBuffer(MakeCam(1000, 11)));
		return entt::resource<CameraPath>(path);
	}

	/// The camera's zoomers moved on by a frame, as the camera's update moves them
	static void Step(Zoomer3& origin, Zoomer3& focus, std::chrono::microseconds dt)
	{
		const float seconds = std::min(std::chrono::duration<float>(dt).count(), 0.1f);
		origin.Update(seconds);
		focus.Update(seconds);
	}

	/// No script holds the camera: its mode can be left
	CameraPathSystem _paths {[] { return true; }};
	Camera* _camera {nullptr};
	/// Near the world's centre, so that the camera stays on the world's disc
	const glm::mat4 _placement {glm::translate(glm::vec3(2500.0f, 0.0f, 2450.0f))};

private:
	test::RestoreService<Locator::camera> _restoreCamera;
};
} // namespace

TEST_F(CameraPathSystemTest, APlacedPathHoldsTheCameraUntilStopped)
{
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_FALSE(_paths.IsPathing());
	EXPECT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	EXPECT_TRUE(_paths.HoldsCamera());
	EXPECT_TRUE(_paths.IsPathing());
	_paths.Stop();
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_FALSE(_paths.IsPathing());
}

TEST_F(CameraPathSystemTest, NoPathOrAnEmptyOneIsNotFollowed)
{
	EXPECT_FALSE(_paths.Begin(k_Owner, entt::resource<CameraPath>(), _placement, 1.0f));
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_FALSE(_paths.Begin(k_Owner, entt::resource<CameraPath>(std::make_shared<CameraPath>("empty")), _placement, 1.0f));
	EXPECT_FALSE(_paths.HoldsCamera());
}

TEST_F(CameraPathSystemTest, AScriptHoldingTheCameraRefusesIt)
{
	CameraPathSystem paths {[] { return false; }};
	const auto origin = _camera->GetOriginZoomer();
	EXPECT_FALSE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	EXPECT_FALSE(paths.HoldsCamera());
	// nothing moves the camera
	paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), origin.GetCurrentValue());
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), origin.GetDestination());
}

TEST_F(CameraPathSystemTest, TheCameraGlidesOntoThePathOnceOverThePauseAndTheLag)
{
	auto origin = _camera->GetOriginZoomer();
	auto focus = _camera->GetFocusZoomer();
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.Update(16000us);

	// from the camera's zoomers as they were, over the pause and 0.3 s more, to the placed point at the path's time 0
	origin.SetDestinationWithTime(glm::vec3(2500.0f, 5.0f, 2450.0f), camera_path::PlacedGlideSeconds(1.0f));
	focus.SetDestinationWithTime(glm::vec3(2501.0f, 5.0f, 2450.0f), camera_path::PlacedGlideSeconds(1.0f));
	Step(origin, focus, 16000us);
	EXPECT_EQ(_camera->GetOrigin(), origin.GetCurrentValue());
	EXPECT_EQ(_camera->GetFocus(), focus.GetCurrentValue());

	// before it plays the glide is not set again, even when the path's time moves
	_paths.FollowAt(k_Owner, 500, false);
	_paths.Update(16000us);
	Step(origin, focus, 16000us);
	EXPECT_EQ(_camera->GetOrigin(), origin.GetCurrentValue());
	EXPECT_EQ(_camera->GetFocus(), focus.GetCurrentValue());
	EXPECT_TRUE(_paths.HoldsCamera());
}

TEST_F(CameraPathSystemTest, WhileItPlaysTheCameraIsSentEveryUpdateToArriveThreeTenthsLater)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.Update(16000us);
	auto origin = _camera->GetOriginZoomer();
	auto focus = _camera->GetFocusZoomer();

	_paths.FollowAt(k_Owner, 500, true);
	for (int frame = 0; frame < 3; ++frame)
	{
		// the path's point at 500 of its 1000 ms, half way along x, sent afresh each update
		_paths.Update(16000us);
		origin.SetDestinationWithTime(glm::vec3(2550.0f, 5.0f, 2450.0f), 0.3f);
		focus.SetDestinationWithTime(glm::vec3(2551.0f, 5.0f, 2450.0f), 0.3f);
		Step(origin, focus, 16000us);
		EXPECT_EQ(_camera->GetOrigin(), origin.GetCurrentValue()) << frame;
		EXPECT_EQ(_camera->GetFocus(), focus.GetCurrentValue()) << frame;
	}
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2550.0f, 5.0f, 2450.0f));
}

TEST_F(CameraPathSystemTest, ReleaseLetsGoOfItsOwnersPathAndLeavesTheCameraWhereItIs)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.Update(16000us);
	// another owner's release changes nothing
	_paths.Release(k_OtherOwner);
	EXPECT_TRUE(_paths.HoldsCamera());
	const auto origin = _camera->GetOriginZoomer();
	_paths.Release(k_Owner);
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), origin.GetCurrentValue());
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), origin.GetDestination());
	// a path let go of is no longer followed
	_paths.FollowAt(k_Owner, 900, true);
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), origin.GetDestination());
}

TEST_F(CameraPathSystemTest, ALaterPathGoesOnTopAndTheOneUnderItHasTheCameraAgainWhenItGoes)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.FollowAt(k_Owner, 200, true);
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), glm::translate(glm::vec3(2400.0f, 0.0f, 2400.0f)), 2.0f));
	// an owner places one path
	EXPECT_FALSE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2400.0f, 5.0f, 2400.0f));
	_paths.Release(k_OtherOwner);
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2520.0f, 5.0f, 2450.0f));
}

TEST_F(CameraPathSystemTest, AMovementKeyThatMovesTheCameraOrALandGripTakesItBack)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	// 2 ms at 400 a second is less than a whole unit: the key doesn't count
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 2, .grippingLand = false});
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.HandlePlayerControl({.movementKey = false, .frameMilliseconds = 16, .grippingLand = false});
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 3, .grippingLand = false});
	EXPECT_FALSE(_paths.HoldsCamera());

	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	_paths.HandlePlayerControl({.movementKey = false, .frameMilliseconds = 0, .grippingLand = true});
	// the paths under the player's camera never have it again, and their owners' releases find nothing
	EXPECT_FALSE(_paths.HoldsCamera());
	_paths.Release(k_Owner);
	_paths.Release(k_OtherOwner);
	EXPECT_FALSE(_paths.HoldsCamera());
}

TEST_F(CameraPathSystemTest, TheDebugToolsReadThePathThatHasTheCamera)
{
	EXPECT_FALSE(_paths.CurrentPlaced().has_value());
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.5f));
	ASSERT_TRUE(_paths.CurrentPlaced().has_value());
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_Owner);
	EXPECT_FALSE(_paths.CurrentPlaced()->glided);
	EXPECT_FALSE(_paths.CurrentPlaced()->playing);
	EXPECT_FLOAT_EQ(_paths.CurrentPlaced()->pauseSeconds, 1.5f);
	_paths.Update(16000us);
	EXPECT_TRUE(_paths.CurrentPlaced()->glided);
	_paths.FollowAt(k_Owner, 640, true);
	EXPECT_EQ(_paths.CurrentPlaced()->pathMilliseconds, 640);
	EXPECT_TRUE(_paths.CurrentPlaced()->playing);
	// the one on top is the one that has the camera
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 2.0f));
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_OtherOwner);
	_paths.Release(k_OtherOwner);
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_Owner);
	_paths.Release(k_Owner);
	EXPECT_FALSE(_paths.CurrentPlaced().has_value());
}

TEST(CameraPathRules, TheGlideIsThePauseAndTheLagAddedInSinglePrecision)
{
	EXPECT_EQ(camera_path::PlacedGlideSeconds(4.0f), 4.0f + 0.3f);
	EXPECT_EQ(std::bit_cast<uint32_t>(camera_path::PlacedGlideSeconds(4.0f)), 0x4089999Au);
	EXPECT_EQ(camera_path::PlacedGlideSeconds(0.0f), 0.3f);
	EXPECT_EQ(std::bit_cast<uint32_t>(camera_path::k_PlacedLagSeconds), 0x3E99999Au);
}

TEST(CameraPathRules, APointIsPlacedSummingZThenYThenXThenTheTranslation)
{
	// with these columns the original's order cancels the large parts first and keeps the 1, where summing x first
	// would lose it
	glm::mat4 placement(1.0f);
	placement[1][0] = 1.0f;
	placement[2][0] = 1.0f;
	placement[3] = glm::vec4(0.5f, 2.0f, 3.0f, 1.0f);
	const auto placed = camera_path::PlacePoint(placement, glm::vec3(1.0f, 1.0e8f, -1.0e8f));
	EXPECT_EQ(placed.x, 1.5f);
	EXPECT_EQ(placed.y, 1.0e8f + 2.0f);
	EXPECT_EQ(placed.z, -1.0e8f + 3.0f);
	// a plain move and turn places a point as the matrix does
	const glm::mat4 turned =
	    glm::translate(glm::vec3(10.0f, 20.0f, 30.0f)) * glm::rotate(1.5707964f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto turnedPoint = camera_path::PlacePoint(turned, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_NEAR(turnedPoint.x, 10.0f, 1e-5f);
	EXPECT_NEAR(turnedPoint.y, 20.0f, 1e-5f);
	EXPECT_NEAR(turnedPoint.z, 29.0f, 1e-5f);
}

TEST(CameraPathRules, ThePathsTimeIsTheDrawnFramesWholeMilliseconds)
{
	// the forest's 6633 ms clip, in steps of 6.633 ms rounded towards zero
	EXPECT_EQ(camera_path::PathTimeFromFrame(6633, 0), 0);
	EXPECT_EQ(camera_path::PathTimeFromFrame(6633, 1), 6);
	EXPECT_EQ(camera_path::PathTimeFromFrame(6633, 2), 13);
	EXPECT_EQ(camera_path::PathTimeFromFrame(6633, 500), 3316);
	EXPECT_EQ(camera_path::PathTimeFromFrame(6633, 999), 6626);
}

TEST(CameraPathRules, ItLetsGoAtTheLastFrameNotBefore)
{
	EXPECT_FALSE(camera_path::ReachedLastFrame(0));
	EXPECT_FALSE(camera_path::ReachedLastFrame(998));
	EXPECT_TRUE(camera_path::ReachedLastFrame(999));
	EXPECT_TRUE(camera_path::ReachedLastFrame(1000));
	EXPECT_FALSE(camera_path::ReachedLastFrame(8, 10));
	EXPECT_TRUE(camera_path::ReachedLastFrame(9, 10));
}
