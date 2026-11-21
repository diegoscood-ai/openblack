/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The island's camera behind the temple: inside the citadel the camera's current mode goes on moving the island's camera
// every frame under the rooms' own camera, so a flight under way as the player goes in has gone on by the way out. The
// camera's model moves it while it is the current mode (the player's mode, Creature Mode's follow, the creature
// interaction's camera); a placed path that is the current mode runs its own update on it; with another mode current
// only the zoomers go on. No control of the player reaches it. Worked out with fake camera models, the temple's way in
// and out done as TempleInterior does them

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CameraPath.h"
#include "Camera/Camera.h"
#include "Camera/CameraChecks.h"
#include "Camera/CameraModeStack.h"
#include "Camera/CameraModel.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CameraStack.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::camera_stack::ModeStack;
using openblack::camera_stack::StackedMode;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr glm::vec3 k_FarOrigin {2000.0f, 80.0f, 2000.0f};
constexpr glm::vec3 k_FarFocus {2000.0f, 0.0f, 2080.0f};
constexpr auto k_Frame = 20ms;
/// The flight's time from its start to the far place
constexpr auto k_FlightTime = 2s;

/// A flight: its first update sends the camera to the far place over two seconds, and it then lets the camera's zoomers
/// get there. Counts its updates and the controls it is given
class FlightModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		++updates;
		if (_setOff)
		{
			return std::nullopt;
		}
		_setOff = true;
		return CameraInterpolationUpdateInfo {.origin = k_FarOrigin, .focus = k_FarFocus, .duration = k_FlightTime};
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override { ++controlsRead; }
	void SkipActions() override { ++controlsSkipped; }
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return k_FarOrigin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return k_FarFocus; }

	int updates {0};
	int controlsRead {0};
	int controlsSkipped {0};

private:
	bool _setOff {false};
};

/// The temple's rooms' camera: it holds the camera where the temple's room is
class RoomModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return glm::vec3(0.0f, 10.0f, 0.0f); }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return glm::vec3(0.0f); }
};

/// A placed path's .cam data: a second over 11 points along x at height 5, each looking one ahead along x
entt::resource<CameraPath> StraightPath()
{
	constexpr uint32_t k_Points = 11;
	std::vector<uint8_t> data(3 * sizeof(uint32_t) + k_Points * 6 * sizeof(float));
	const std::array<uint32_t, 3> header {static_cast<uint32_t>(data.size()), 1000, k_Points};
	std::memcpy(data.data(), header.data(), sizeof(header));
	for (uint32_t i = 0; i < k_Points; ++i)
	{
		const auto x = static_cast<float>(i) * 10.0f;
		const std::array<float, 6> point {x, 5.0f, 0.0f, x + 1.0f, 5.0f, 0.0f};
		std::memcpy(&data[sizeof(header) + i * sizeof(point)], point.data(), sizeof(point));
	}
	auto path = std::make_shared<CameraPath>("test");
	EXPECT_TRUE(path->LoadFromBuffer(data));
	return entt::resource<CameraPath>(path);
}

/// The camera outside, with a flight set off and under way for a few frames
std::pair<std::unique_ptr<Camera>, FlightModel*> CameraInFlight(int frames)
{
	auto camera = std::make_unique<Camera>();
	camera->SetOrigin(k_Origin).SetFocus(k_Focus);
	auto model = std::make_unique<FlightModel>();
	auto* flight = model.get();
	std::ignore = camera->SetModel(std::move(model));
	for (int frame = 0; frame < frames; ++frame)
	{
		camera->Update(k_Frame);
	}
	return {std::move(camera), flight};
}

/// The way in, as TempleInterior::Activate does it: the island's camera takes the camera's view and model, and the rooms'
/// model has the camera
std::unique_ptr<Camera> GoIn(Camera& camera)
{
	auto world = std::make_unique<Camera>();
	camera.CopyViewTo(*world);
	std::ignore = world->SetModel(camera.SetModel(std::make_unique<RoomModel>()));
	camera.SetOrigin(glm::vec3(0.0f, 10.0f, 0.0f)).SetFocus(glm::vec3(0.0f));
	return world;
}

/// The way out, as TempleInterior::Deactivate does it: the camera gets its model and the island's zoomers back
void GoOut(Camera& camera, Camera& world)
{
	std::ignore = camera.SetModel(world.SetModel(nullptr));
	camera.GetOriginZoomer() = world.GetOriginZoomer();
	camera.GetFocusZoomer() = world.GetFocusZoomer();
}

/// A stack of modes of its own in the locator for the test
class BehindTempleStack: public testing::Test
{
protected:
	void SetUp() override { Locator::cameraStack::emplace<ecs::systems::CameraStack>(); }
	ModeStack& Modes() { return Locator::cameraStack::value().Modes(); }

private:
	test::RestoreService<Locator::cameraStack> _restore;
};
} // namespace

// Inside the citadel the flight goes on under the rooms' camera: by the way out the island's camera is at the end of it
TEST_F(BehindTempleStack, AFlightUnderWayGoingInHasGoneOnByTheWayOut)
{
	auto [camera, flight] = CameraInFlight(5);
	const auto goingIn = camera->GetOrigin();
	ASSERT_GT(glm::distance(goingIn, k_FarOrigin), 100.0f);
	auto world = GoIn(*camera);
	// three seconds in the temple, longer than the flight
	for (int frame = 0; frame < 150; ++frame)
	{
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	// the rooms' camera was not moved by it
	EXPECT_EQ(camera->GetOrigin(), glm::vec3(0.0f, 10.0f, 0.0f));
	GoOut(*camera, *world);
	EXPECT_LT(glm::distance(camera->GetOrigin(), k_FarOrigin), 0.01f);
	EXPECT_LT(glm::distance(camera->GetFocus(), k_FarFocus), 0.01f);
	EXPECT_EQ(&camera->GetModel(), flight);
	EXPECT_EQ(flight->updates, 5 + 150);
}

// Half way through the flight in the temple, the island's camera is part of the way: it moves as it does outside
TEST_F(BehindTempleStack, BehindTheTempleTheIslandsCameraMovesAsItDoesOutside)
{
	auto [outside, outsideFlight] = CameraInFlight(5);
	auto [inside, insideFlight] = CameraInFlight(5);
	auto world = GoIn(*inside);
	for (int frame = 0; frame < 20; ++frame)
	{
		outside->Update(k_Frame);
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	EXPECT_EQ(world->GetOrigin(), outside->GetOrigin());
	EXPECT_EQ(world->GetFocus(), outside->GetFocus());
	EXPECT_GT(glm::distance(world->GetOrigin(), k_FarOrigin), 1.0f);
	EXPECT_EQ(insideFlight->updates, outsideFlight->updates);
}

// The camera's mode reads none of the player's controls in the citadel: each frame it is told its controls were not read
TEST_F(BehindTempleStack, BehindTheTempleTheModelReadsNoControls)
{
	auto [camera, flight] = CameraInFlight(1);
	auto world = GoIn(*camera);
	const int skippedGoingIn = flight->controlsSkipped;
	for (int frame = 0; frame < 10; ++frame)
	{
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	EXPECT_EQ(flight->controlsRead, 0);
	EXPECT_EQ(flight->controlsSkipped, skippedGoingIn + 10);
}

// With another mode the current one whose update is not run here (a path's entry with no path system to run it), the
// camera's model is not updated behind the temple; the zoomers go on to where they were going
TEST_F(BehindTempleStack, WithAnotherModeCurrentOnlyTheZoomersGoOn)
{
	auto [camera, flight] = CameraInFlight(5);
	auto world = GoIn(*camera);
	std::ignore = Modes().PushPath(1);
	for (int frame = 0; frame < 150; ++frame)
	{
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	EXPECT_EQ(flight->updates, 5);
	EXPECT_LT(glm::distance(world->GetOrigin(), k_FarOrigin), 0.01f);
}

// A placed path that is the current mode runs its own update behind the temple, on the island's camera: its glide onto
// the path, then its aim as its animation plays, the zoomers moving on; the camera's model and the rooms' camera are
// not moved by it
TEST_F(BehindTempleStack, APlacedPathCurrentRunsItsUpdateOnTheIslandsCamera)
{
	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	auto& drawn = Locator::camera::emplace(glm::vec3(0.0f));
	auto& paths = static_cast<ecs::systems::CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<ecs::systems::CameraPathSystem>([] { return true; }));
	auto [camera, flight] = CameraInFlight(5);
	auto world = GoIn(*camera);
	constexpr ecs::systems::CameraPathSystemInterface::PathOwner k_Owner = 7;
	// placed by the world's middle, inside the sphere the camera's update keeps every destination in
	auto placement = glm::mat4(1.0f);
	placement[3] = glm::vec4(2560.0f, 0.0f, 2560.0f, 1.0f);
	ASSERT_TRUE(paths.Begin(k_Owner, StraightPath(), placement, 1.0f));
	const auto drawnOrigin = drawn.GetOrigin();
	// its first update glides onto the path's start
	camera_checks::UpdateBehindTemple(*world, k_Frame);
	EXPECT_EQ(world->GetOriginZoomer().GetDestination(), glm::vec3(2560.0f, 5.0f, 2560.0f));
	EXPECT_EQ(world->GetFocusZoomer().GetDestination(), glm::vec3(2561.0f, 5.0f, 2560.0f));
	// three seconds: the glide, over the pause and a little more, is over
	for (int frame = 0; frame < 150; ++frame)
	{
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	EXPECT_LT(glm::distance(world->GetOrigin(), glm::vec3(2560.0f, 5.0f, 2560.0f)), 0.01f);
	EXPECT_LT(glm::distance(world->GetFocus(), glm::vec3(2561.0f, 5.0f, 2560.0f)), 0.01f);
	// the animation drawn half way, playing: the camera is sent there afresh
	paths.FollowAt(k_Owner, 500, true);
	camera_checks::UpdateBehindTemple(*world, k_Frame);
	EXPECT_EQ(world->GetOriginZoomer().GetDestination(), glm::vec3(2610.0f, 5.0f, 2560.0f));
	EXPECT_EQ(flight->updates, 5);
	EXPECT_EQ(drawn.GetOrigin(), drawnOrigin);
	EXPECT_EQ(camera->GetOrigin(), glm::vec3(0.0f, 10.0f, 0.0f));
	paths.ClearPlaced();
}

// A placed path under a new player's mode is not the current mode: that mode's model moves the island's camera
TEST_F(BehindTempleStack, APlacedPathUnderANewPlayersModeRunsNoUpdateBehindTheTemple)
{
	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	std::ignore = Locator::camera::emplace(glm::vec3(0.0f));
	auto& paths = static_cast<ecs::systems::CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<ecs::systems::CameraPathSystem>([] { return true; }));
	auto [camera, flight] = CameraInFlight(5);
	auto world = GoIn(*camera);
	ASSERT_TRUE(paths.Begin(7, StraightPath(), glm::mat4(1.0f), 1.0f));
	const auto playersMode = Modes().PushPlayer(false);
	ASSERT_TRUE(Modes().CoverPaths(playersMode));
	for (int frame = 0; frame < 150; ++frame)
	{
		camera_checks::UpdateBehindTemple(*world, k_Frame);
	}
	EXPECT_EQ(flight->updates, 5 + 150);
	EXPECT_LT(glm::distance(world->GetOrigin(), k_FarOrigin), 0.01f);
	paths.ClearPlaced();
}

// The camera's own model is the current mode for the player's mode, a follow and the creature interaction's camera; a
// placed path, the dance camera, a script's camera mode and a dual camera move the camera by their own updates
TEST(BehindTemple, TheCamerasModelIsTheCurrentModeForThePlayersModeAFollowAndTheInteraction)
{
	EXPECT_TRUE(camera_checks::CameraModelIsCurrent(nullptr));
	{
		ModeStack stack;
		EXPECT_TRUE(camera_checks::CameraModelIsCurrent(&stack));
		std::ignore = stack.PushPath(1);
		EXPECT_FALSE(camera_checks::CameraModelIsCurrent(&stack));
		std::ignore = stack.PushFollow(2);
		EXPECT_TRUE(camera_checks::CameraModelIsCurrent(&stack));
		std::ignore = stack.PushDance(entt::entity {3});
		EXPECT_FALSE(camera_checks::CameraModelIsCurrent(&stack));
	}
	{
		ModeStack stack;
		std::ignore = stack.Push({.kind = StackedMode::Kind::Script});
		EXPECT_FALSE(camera_checks::CameraModelIsCurrent(&stack));
		std::ignore = stack.Push({.kind = StackedMode::Kind::Dual});
		EXPECT_FALSE(camera_checks::CameraModelIsCurrent(&stack));
		std::ignore = stack.PushPlayer(false);
		EXPECT_TRUE(camera_checks::CameraModelIsCurrent(&stack));
	}
	{
		ModeStack stack;
		std::ignore = stack.Push({.kind = StackedMode::Kind::Interaction});
		EXPECT_TRUE(camera_checks::CameraModelIsCurrent(&stack));
	}
}
