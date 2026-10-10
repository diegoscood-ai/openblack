/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world camera's hand cues: what the camera tells the hand it does with the mouse, worked out from fake controls

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include "Camera/CameraDrag.h"
#include "Camera/CameraHelp.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using input::BindableActionMap;
using input::UnbindableActionMap;

namespace
{
constexpr glm::ivec2 k_Screen {800, 600};
constexpr auto k_Frame = 33ms;
/// The right edge, half way down, where the hints offer turning
constexpr glm::uvec2 k_RightEdge {795, 300};
constexpr glm::uvec2 k_Middle {400, 300};

class FakeWindowing final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const final { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const final { return {}; }
	[[nodiscard]] uint32_t GetID() const final { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const final { return k_Screen; }
	[[nodiscard]] float GetAspectRatio() const final { return static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y); }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode) final { return *this; }
};

/// The keys and buttons held, the mouse and the hand, as a test sets them
class FakeActions final: public input::GameActionInterface
{
public:
	[[nodiscard]] bool GetBindable(BindableActionMap action) const final { return (held & static_cast<uint64_t>(action)) != 0; }
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap action) const final
	{
		return (heldUnbindable & static_cast<uint8_t>(action)) != 0;
	}
	[[nodiscard]] bool GetBindableChanged(BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap) const final { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap) const final { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const final { return mouse; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const final { return mouseDelta; }
	[[nodiscard]] float GetMouseWheelDelta() const final { return 0.0f; }
	void WarpCursor(glm::ivec2 position) final { mouse = glm::uvec2(position); }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final { return {hand, std::nullopt}; }
	void Frame() final {}
	void ProcessEvent(const SDL_Event&) final {}

	uint64_t held {0};
	uint8_t heldUnbindable {0};
	glm::uvec2 mouse {k_Middle};
	glm::ivec2 mouseDelta {0, 0};
	std::optional<glm::vec3> hand {glm::vec3(100.0f, 0.0f, 100.0f)};
};

class WorldCameraHints: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		Locator::windowing::emplace<FakeWindowing>();
		_help = &Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>().Get();
		// No cinema bars: the controls measure by the whole screen
		Locator::cinematicDirectorSystem::reset();
	}

	/// The hints the cursor's place offers, over no land
	static uint32_t IdleHints(glm::uvec2 cursor)
	{
		return camera_drag::IdleTricons(
		    camera_drag::NormalisedCursor(glm::ivec2(cursor), k_Screen, camera_drag::ViewHeight(k_Screen, false)), false);
	}

	DefaultWorldCameraModel _model;
	FakeActions* _actions {nullptr};
	camera_help::CameraHelp* _help {nullptr};

private:
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::cameraHelpSystem> _restoreHelp;
	test::RestoreService<Locator::cinematicDirectorSystem> _restoreCinematics;
};
} // namespace

TEST(WorldCameraHandCues, ANewCameraOffersNoHintsAndDragsNothing)
{
	const DefaultWorldCameraModel model;
	const auto cues = model.GetHandCues();
	EXPECT_EQ(cues.tricons, camera_drag::tricon::k_Idle);
	EXPECT_FALSE(cues.dragging);
	EXPECT_FALSE(cues.dragMode.has_value());
	EXPECT_FALSE(cues.clearViewGrip);
}

TEST_F(WorldCameraHints, FollowTheCursorWithNothingDragged)
{
	ASSERT_NE(IdleHints(k_RightEdge), camera_drag::tricon::k_Idle);
	_actions->mouse = k_RightEdge;
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, IdleHints(k_RightEdge));
	EXPECT_FALSE(_model.GetHandCues().dragging);
	_actions->mouse = k_Middle;
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, camera_drag::tricon::k_Idle);
}

TEST_F(WorldCameraHints, StayWhileTheCameraTurnsUnlessTurningIsTakenAway)
{
	_actions->mouse = k_RightEdge;
	_model.HandleActions(k_Frame);
	_actions->mouse = k_Middle;
	_actions->held = static_cast<uint64_t>(BindableActionMap::ROTATE_LEFT);
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, IdleHints(k_RightEdge));
	// Without the rotate feature the key turns nothing, and the hints follow the cursor again
	_help->Enable(0, camera_help::Bit(camera_help::Feature::Rotate));
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, camera_drag::tricon::k_Idle);
}

TEST_F(WorldCameraHints, ADragTakesThoseFromBeforeThePressAndIsDecidedAsTheMouseMoves)
{
	const auto hints = IdleHints(k_RightEdge);
	const auto pressedAt =
	    camera_drag::NormalisedCursor(glm::ivec2(k_RightEdge), k_Screen, camera_drag::ViewHeight(k_Screen, false));
	camera_drag::DragClassifier expected;

	_actions->mouse = k_RightEdge;
	_model.HandleActions(k_Frame);
	// Pressed on the second frame, 66 ms in
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_model.HandleActions(k_Frame);
	expected.Start(hints, pressedAt, 66);
	auto cues = _model.GetHandCues();
	EXPECT_TRUE(cues.dragging);
	EXPECT_EQ(cues.tricons, expected.GetTricons());
	EXPECT_EQ(cues.dragMode, expected.GetMode());

	// The mouse moves down, which decides it (the cursor over no land)
	_actions->mouseDelta = {0, 40};
	_model.HandleActions(k_Frame);
	expected.Move({0, 40}, k_Screen, camera_drag::ViewHeight(k_Screen, false), 99, false, camera_drag::k_DefaultFeatures);
	ASSERT_TRUE(expected.GetMode().has_value());
	cues = _model.GetHandCues();
	EXPECT_TRUE(cues.dragging);
	EXPECT_EQ(cues.tricons, expected.GetTricons());
	EXPECT_EQ(cues.dragMode, expected.GetMode());

	// Let go, the drag ends and the hints follow the cursor, which the turn round the edge put on its ring
	ASSERT_EQ(expected.GetMode(), camera_drag::DragMode::EdgeRotate);
	const auto ring = camera_drag::EdgeRotate(glm::ivec2(k_RightEdge), glm::ivec2(k_RightEdge), k_Screen,
	                                          camera_drag::ViewHeight(k_Screen, false));
	EXPECT_EQ(glm::ivec2(_actions->mouse), ring.cursor);
	_actions->held = 0;
	_actions->mouseDelta = {0, 0};
	_model.HandleActions(k_Frame);
	cues = _model.GetHandCues();
	EXPECT_FALSE(cues.dragging);
	EXPECT_FALSE(cues.dragMode.has_value());
	EXPECT_EQ(cues.tricons, IdleHints(glm::uvec2(ring.cursor)));
}

TEST_F(WorldCameraHints, NoDragWithoutTheHandOrWhileTurningAroundTheMouse)
{
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_actions->hand = std::nullopt;
	_model.HandleActions(k_Frame);
	EXPECT_FALSE(_model.GetHandCues().dragging);

	_actions->hand = glm::vec3(100.0f, 0.0f, 100.0f);
	_actions->held |= static_cast<uint64_t>(BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	_model.HandleActions(k_Frame);
	EXPECT_FALSE(_model.GetHandCues().dragging);
}

TEST_F(WorldCameraHints, AnEdgeDragPutsTheCursorOnTheRingAndLeavesTheCameraToItsUpdate)
{
	const auto origin = _model.GetTargetOrigin();
	const auto focus = _model.GetTargetFocus();
	_actions->mouse = k_RightEdge;
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_model.HandleActions(k_Frame);
	_actions->mouseDelta = {40, 0};
	_model.HandleActions(k_Frame);
	ASSERT_TRUE(_model.GetHandCues().dragging);
	ASSERT_EQ(_model.GetHandCues().dragMode, camera_drag::DragMode::EdgeRotate);
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	EXPECT_EQ(_model.GetTargetFocus(), focus);
	const auto ring = camera_drag::EdgeRotate(glm::ivec2(k_RightEdge), glm::ivec2(k_RightEdge), k_Screen,
	                                          camera_drag::ViewHeight(k_Screen, false));
	EXPECT_EQ(glm::ivec2(_actions->mouse), ring.cursor);
}
