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
#include <tuple>
#include <vector>

#include <entt/resource/resource.hpp>
#include <glm/gtx/transform.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "Camera/Camera.h"
#include "Camera/CameraModeStack.h"
#include "Camera/CameraPathControl.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Common/Zoomer.h"
#include "ECS/Systems/CameraStackInterface.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "GameClock.h"
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

/// The camera's stack of modes, which holds the placed paths' entries: a new player's mode goes over them, or is popped,
/// there
camera_stack::ModeStack& Stack()
{
	return Locator::cameraStack::value().Modes();
}
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

	/// The new player's modes the take-backs put on the camera's stack
	uint32_t _pushed {0};
	/// No script holds the camera: its mode can be left
	CameraPathSystem _paths {[] { return true; }, [this] { return ++_pushed; }};
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

TEST_F(CameraPathSystemTest, AReleasedPathFollowsOnUntilTheTurnsCameraChecksTakeItOff)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.Update(16000us);
	// another owner's release changes nothing
	_paths.Release(k_OtherOwner);
	EXPECT_FALSE(_paths.RemoveInvalid());
	EXPECT_TRUE(_paths.HoldsCamera());
	// let go of, the path still has the camera and is still followed until the turn's checks
	_paths.Release(k_Owner);
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.FollowAt(k_Owner, 200, true);
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2520.0f, 5.0f, 2450.0f));
	EXPECT_TRUE(_paths.RemoveInvalid()); // the one that had the camera went
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_FALSE(_paths.CurrentPlaced().has_value());
	// a path taken off is no longer followed
	const auto destination = _camera->GetOriginZoomer().GetDestination();
	_paths.FollowAt(k_Owner, 900, true);
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), destination);
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
	EXPECT_TRUE(_paths.RemoveInvalid());
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
	EXPECT_EQ(_pushed, 1u);

	constexpr CameraPathSystem::PathOwner k_Third = 9;
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	ASSERT_TRUE(_paths.Begin(k_Third, Path(), _placement, 1.0f));
	_paths.HandlePlayerControl({.movementKey = false, .frameMilliseconds = 0, .grippingLand = true});
	// the paths under the player's camera do not have it while it is there; their owners' releases take them off at the
	// turn, quietly
	EXPECT_EQ(_pushed, 2u);
	EXPECT_FALSE(_paths.HoldsCamera());
	_paths.Release(k_Owner);
	_paths.Release(k_OtherOwner);
	_paths.Release(k_Third);
	EXPECT_FALSE(_paths.RemoveInvalid()); // none of them had the camera
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_EQ(_paths.PlacedCount(), 0u);
}

TEST_F(CameraPathSystemTest, UnderAScriptsCameraNoKeyNorGripTakesThePathBack)
{
	// Only the current camera mode reads the movement keys and the grip: with a script's camera on top the path keeps
	// its place, and once the script's camera is gone the same input takes it back
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true, .scriptCameraOver = true});
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true});
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_EQ(_pushed, 2u); // one for the key, one for the grip
}

TEST_F(CameraPathSystemTest, ANewPlayersCameraOverThePathsKeepsThemUnderItUntilItIsPopped)
{
	// The end of a script's camera over the paths: none has the camera while the new player's mode is there, their
	// releases still mark them and the turn's camera checks take them off quietly
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	Stack().CoverPaths(5);
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_FALSE(_paths.CurrentPlaced().has_value());
	EXPECT_EQ(_paths.PlacedCount(), 2u);
	_paths.Release(k_OtherOwner);
	EXPECT_FALSE(_paths.RemoveInvalid());
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_EQ(_paths.PlacedCount(), 1u);
	// Another player's mode's pop changes nothing; this one's gives the camera back to the path left on top
	Stack().UncoverPaths(4);
	EXPECT_FALSE(_paths.HoldsCamera());
	Stack().UncoverPaths(5);
	EXPECT_TRUE(_paths.HoldsCamera());
	ASSERT_TRUE(_paths.CurrentPlaced().has_value());
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_Owner);
}

// The stack's push order (docs/wiki/camera/game-camera.md#openblack): a path started over a current dual camera takes
// the camera, from the zoomers as the dual camera left them; the dual camera's release then finds no current dual camera
// and pops nothing; the path taken off at the turn's check leaves the dual camera current again, from the camera's
// zoomers as the path left them
TEST_F(CameraPathSystemTest, APathStartedOverADualCameraTakesTheCameraAndTheDualsReleasePopsNothing)
{
	script_camera::StartDualWithPoint(static_cast<entt::entity>(11), glm::vec3(2600.0f, 0.0f, 2600.0f),
	                                  _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	auto& state = script_camera::Get();
	state.position.SetPosition({2400.0f, 40.0f, 2400.0f}); // where the dual camera has the zoomers
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	EXPECT_TRUE(_paths.HoldsCamera());
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), glm::vec3(2400.0f, 40.0f, 2400.0f));
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	_paths.Update(16000us);
	_paths.Release(k_Owner);
	EXPECT_TRUE(_paths.RemoveInvalid());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(state.position.GetCurrentValue(), _camera->GetOriginZoomer().GetCurrentValue());
	EXPECT_EQ(state.position.GetDestination(), _camera->GetOriginZoomer().GetDestination());
}

// Creature Mode's follow started while a path has the camera is pushed over it, the current mode: the path keeps its
// place under the follow without the camera (its cleanup changes nothing openblack draws: the near plane is set again
// every frame), and has it again once the follow comes off. A path started over the follow has the camera
TEST_F(CameraPathSystemTest, AFollowStartedOverAPathHasTheCameraUntilItComesOff)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	Stack().PushFollow();
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_EQ(_paths.PlacedCount(), 1u);
	// nothing moves the camera for the path
	const auto origin = _camera->GetOriginZoomer();
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), origin.GetDestination());
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.Release(k_OtherOwner);
	EXPECT_TRUE(_paths.RemoveInvalid());
	EXPECT_FALSE(_paths.HoldsCamera());
	EXPECT_TRUE(Stack().RemoveFollow());
	EXPECT_TRUE(_paths.HoldsCamera());
}

TEST_F(CameraPathSystemTest, AFullCameraStackDeletesTheLowestPathAndItsOwnersReleaseFindsNothing)
{
	EXPECT_FALSE(Stack().LowestOther().has_value());
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	ASSERT_TRUE(Stack().LowestOther().has_value());
	// The lowest, the first placed, goes; the one on top keeps the camera
	_paths.DeleteLowest();
	EXPECT_EQ(_paths.PlacedCount(), 1u);
	EXPECT_TRUE(_paths.HoldsCamera());
	ASSERT_TRUE(_paths.CurrentPlaced().has_value());
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_OtherOwner);
	_paths.Release(k_Owner);
	_paths.RemoveInvalid();
	EXPECT_EQ(_paths.PlacedCount(), 1u);
	EXPECT_EQ(_pushed, 0u); // nothing restarted
	_paths.DeleteLowest();
	EXPECT_EQ(_paths.PlacedCount(), 0u);
	EXPECT_FALSE(Stack().LowestOther().has_value());
	_paths.DeleteLowest();
	EXPECT_EQ(_paths.PlacedCount(), 0u);
}

// A path placed on a full camera stack goes through the stack's one push (docs/wiki/camera/game-camera.md#openblack): the
// bottom mode is deleted first whatever it is, through its owner, the player's own mode, then the lowest path by the
// camera path system, then the lowest dual camera; the stack never holds more than its 12 places
TEST_F(CameraPathSystemTest, APathPlacedOnAFullStackDeletesTheBottomModeThroughItsOwner)
{
	constexpr CameraPathSystem::PathOwner k_Third = 9;
	constexpr CameraPathSystem::PathOwner k_Fourth = 10;
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	auto& paths = static_cast<CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<CameraPathSystem>([] { return true; }, [] { return uint32_t {0}; }));
	auto& stack = Stack();
	ASSERT_TRUE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	for (int i = 0; i < 10; ++i)
	{
		stack.Push({.kind = camera_stack::StackedMode::Kind::Dual, .dual = {.point = {static_cast<float>(i), 0.0f, 0.0f}}},
		           stack.OtherCount());
	}
	ASSERT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	ASSERT_TRUE(paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_EQ(paths.PlacedCount(), 2u);
	// the first path, pushed before the dual cameras, is the bottom: the path system deletes it, nothing restarted
	ASSERT_TRUE(paths.Begin(k_Third, Path(), _placement, 1.0f));
	EXPECT_EQ(paths.PlacedCount(), 2u);
	EXPECT_EQ(stack.OtherCount(), 2u);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	EXPECT_EQ(_pushed, 0u);
	// then the lowest dual camera
	ASSERT_TRUE(paths.Begin(k_Fourth, Path(), _placement, 1.0f));
	EXPECT_EQ(paths.PlacedCount(), 3u);
	ASSERT_EQ(stack.Size(), 9u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	EXPECT_TRUE(paths.HoldsCamera());
	ASSERT_TRUE(paths.CurrentPlaced().has_value());
	EXPECT_EQ(paths.CurrentPlaced()->owner, k_Fourth);
	// the deleted path's owner finds nothing to release
	paths.Release(k_Owner);
	EXPECT_FALSE(paths.RemoveInvalid());
	EXPECT_EQ(paths.PlacedCount(), 3u);
}

// A path popped off a stack it leaves empty, with the player's own mode deleted by a full stack
// (docs/wiki/camera/game-camera.md#openblack): its take-off (the end of a creature hold) makes a new player's mode, the
// bottom from then on, which starts from the camera as it is with no flight back, its mode's seconds from 0
TEST_F(CameraPathSystemTest, APathTakenOffAStackItLeavesEmptyWithThePlayersModeGoneStartsANewPlayersMode)
{
	auto& stack = Stack();
	// Eleven dual cameras fill the stack over the player's own mode: the path's push deletes that mode
	for (int i = 0; i < 11; ++i)
	{
		stack.Push({.kind = camera_stack::StackedMode::Kind::Dual, .dual = {.point = {static_cast<float>(i), 0.0f, 0.0f}}},
		           stack.OtherCount());
	}
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	ASSERT_TRUE(stack.PlayersModeGone());
	// The dual cameras under the path go, leaving it alone on the stack
	std::ignore = stack.RemoveInvalid([](const camera_stack::DualMode& /*dual*/) { return false; });
	ASSERT_TRUE(stack.Empty());
	ASSERT_TRUE(_paths.HoldsCamera());
	auto* player = dynamic_cast<DefaultWorldCameraModel*>(&_camera->GetModel());
	ASSERT_NE(player, nullptr);
	const auto restarts = player->GetRestarts().count;
	script_camera::Get().modeSeconds = 1.0f;

	EXPECT_TRUE(_paths.TakeOffCurrent());
	EXPECT_EQ(_paths.PlacedCount(), 0u);
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(&_camera->GetModel(), player);
	EXPECT_EQ(player->GetRestarts().count, restarts + 1);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	EXPECT_EQ(_pushed, 0u);
}

// With the player's own mode still under it, the path's take-off makes no new player's mode
TEST_F(CameraPathSystemTest, APathTakenOffOverThePlayersModeMakesNoNewOne)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	auto* player = dynamic_cast<DefaultWorldCameraModel*>(&_camera->GetModel());
	ASSERT_NE(player, nullptr);
	const auto restarts = player->GetRestarts().count;
	EXPECT_TRUE(_paths.TakeOffCurrent());
	EXPECT_FALSE(Stack().PlayersModeGone());
	EXPECT_EQ(player->GetRestarts().count, restarts);
}

TEST_F(CameraPathSystemTest, TheTakeBackPutsANewPlayersModeOverThePathsWhichComeBackWhenItIsPopped)
{
	uint32_t pushed = 0;
	CameraPathSystem paths {[] { return true; }, [&pushed] { return ++pushed; }};
	ASSERT_TRUE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = false});
	EXPECT_EQ(pushed, 1u);
	EXPECT_FALSE(paths.HoldsCamera());
	// The path stays placed under the player's mode, its owner's
	EXPECT_EQ(paths.PlacedCount(), 1u);
	EXPECT_FALSE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	// A path placed over the player's mode has the camera, and its going at the turn leaves the player's mode current
	ASSERT_TRUE(paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	EXPECT_TRUE(paths.HoldsCamera());
	paths.Release(k_OtherOwner);
	paths.RemoveInvalid();
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_EQ(paths.PlacedCount(), 1u);
	// Popped, the player's mode gives the camera back to the path under it
	Stack().UncoverPaths(1);
	EXPECT_TRUE(paths.HoldsCamera());
}

TEST_F(CameraPathSystemTest, AKeyAndAGripInOneFramePutTwoPlayersModesOnTop)
{
	uint32_t pushed = 0;
	CameraPathSystem paths {[] { return true; }, [&pushed] { return ++pushed; }};
	ASSERT_TRUE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true});
	EXPECT_EQ(pushed, 2u);
	// The path lies under the first: the second one's pop leaves the first over it
	Stack().UncoverPaths(2);
	EXPECT_FALSE(paths.HoldsCamera());
	Stack().UncoverPaths(1);
	EXPECT_TRUE(paths.HoldsCamera());
	// No longer the current mode, the paths under a player's mode read no take-back
	Stack().CoverPaths(3);
	paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true});
	EXPECT_EQ(pushed, 2u);
}

TEST_F(CameraPathSystemTest, UnderTheCreatureInteractionsCameraThePathWaitsAndHasTheCameraAgainAfter)
{
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	_paths.FollowAt(k_Owner, 200, true);
	_paths.Update(16000us);
	const auto destination = _camera->GetOriginZoomer().GetDestination();
	// The creature interaction's camera takes the view: the path neither has the camera nor moves it, the player's keys
	// do not take it back, and no other path is placed
	_paths.SetCovered(true);
	EXPECT_TRUE(_paths.IsCovered());
	EXPECT_FALSE(_paths.HoldsCamera());
	_paths.FollowAt(k_Owner, 600, true);
	_camera->GetOriginZoomer().SetDestinationWithTime(glm::vec3(2000.0f, 50.0f, 2000.0f), 0.7f);
	_paths.Update(16000us);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2000.0f, 50.0f, 2000.0f));
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true});
	EXPECT_FALSE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	// Given back, the path has the camera again, at its time now
	_paths.SetCovered(false);
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.Update(16000us);
	EXPECT_NE(_camera->GetOriginZoomer().GetDestination(), destination);
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(2560.0f, 5.0f, 2450.0f));
}

TEST_F(CameraPathSystemTest, TakenOffByAnotherThePathUnderItHasTheCamera)
{
	EXPECT_FALSE(_paths.TakeOffCurrent());
	ASSERT_TRUE(_paths.Begin(k_Owner, Path(), _placement, 1.0f));
	ASSERT_TRUE(_paths.Begin(k_OtherOwner, Path(), _placement, 1.0f));
	EXPECT_TRUE(_paths.TakeOffCurrent());
	ASSERT_TRUE(_paths.CurrentPlaced().has_value());
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_Owner);
	// its owner's release finds nothing
	_paths.Release(k_OtherOwner);
	EXPECT_TRUE(_paths.HoldsCamera());
	EXPECT_TRUE(_paths.TakeOffCurrent());
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
	_paths.RemoveInvalid();
	EXPECT_EQ(_paths.CurrentPlaced()->owner, k_Owner);
	_paths.Release(k_Owner);
	_paths.RemoveInvalid();
	EXPECT_FALSE(_paths.CurrentPlaced().has_value());
}

TEST(CameraPathRules, TheKeyAndTheGripEachPutAPlayersModeOnTop)
{
	EXPECT_EQ(camera_path::PlayersModesOnTakeBack(false, 16, false), 0);
	EXPECT_EQ(camera_path::PlayersModesOnTakeBack(true, 2, false), 0); // less than a whole unit
	EXPECT_EQ(camera_path::PlayersModesOnTakeBack(true, 16, false), 1);
	EXPECT_EQ(camera_path::PlayersModesOnTakeBack(false, 0, true), 1);
	EXPECT_EQ(camera_path::PlayersModesOnTakeBack(true, 16, true), 2);
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

// The way into the temple touches no camera mode: a placed path stays on the stack while the player is inside, where the
// temple's rooms have the camera and no key takes it back; on the way out it has the camera again, and leaving for a
// place of the map puts a new player's mode over it, the path staying under it. The debug path player's stop leaves it
TEST_F(CameraPathSystemTest, APlacedPathWaitsThroughTheTempleAndLeavingForTheMapPutsAPlayersModeOverIt)
{
	uint32_t pushed = 0;
	CameraPathSystem paths {[] { return true; }, [&pushed] { return ++pushed; }};
	ASSERT_TRUE(paths.Begin(k_Owner, Path(), _placement, 1.0f));
	const auto sequenceBefore = game_clock::SequenceMode();
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_EQ(paths.PlacedCount(), 1u);
	paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 16, .grippingLand = true});
	EXPECT_EQ(pushed, 0u);
	EXPECT_FALSE(paths.PutPlayersModeOver());
	game_clock::SetSequenceMode(sequenceBefore);
	EXPECT_TRUE(paths.HoldsCamera());

	paths.StopPlayback();
	EXPECT_EQ(paths.PlacedCount(), 1u);
	EXPECT_TRUE(paths.HoldsCamera());
	EXPECT_TRUE(paths.PutPlayersModeOver());
	EXPECT_EQ(pushed, 1u);
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_EQ(paths.PlacedCount(), 1u);
	EXPECT_FALSE(paths.PutPlayersModeOver());
	Stack().UncoverPaths(1);
	EXPECT_TRUE(paths.HoldsCamera());
}
