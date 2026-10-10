/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world camera's mouse: dragging round the edge of the screen turns the camera with the cursor held on a ring, and
// dragging up and down tilts it; the self-tilting camera tilts itself and keeps to its height; the wheel and both
// buttons zoom and turn it; Ctrl and Shift glide it to a clear view; any camera input drops a flight; the double click's
// flight looks at its point from the best side. Worked out from
// fake controls, a fake window and a fake level land; the camera help's events are counted by the help profile, one
// game turn a frame

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraDrag.h"
#include "Camera/CameraHelp.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "EngineConfig.h"
#include "Help/HelpProfile.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using input::BindableActionMap;
using input::UnbindableActionMap;
namespace hp = openblack::help_profile;

namespace
{
constexpr glm::ivec2 k_Screen {800, 600};
constexpr auto k_Frame = 33ms;
constexpr float k_FieldOfView = 70.0f;
/// The right edge, half way down, where the hints offer turning
constexpr glm::uvec2 k_RightEdge {795, 300};
/// The very bottom of the screen, where the hints offer turning and tilting
constexpr glm::uvec2 k_Bottom {400, 597};

/// Level land at 0
class LevelIsland final: public LandIslandInterface
{
public:
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { throw std::logic_error("no cells"); }
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2) const final { return {0, 0, 0, 0}; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { throw std::logic_error("no countries"); }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }
};

/// Every line of sight going down meets the land at y = 0
class LevelDynamics final: public ecs::systems::DynamicsSystemInterface
{
public:
	void Reset() final {}
	void Update(std::chrono::microseconds&) final {}
	void AddRigidBody(btRigidBody*) final {}
	void RegisterRigidBodies() final {}
	void RegisterIslandRigidBodies(LandIslandInterface&) final {}
	void UpdatePhysicsTransforms() final {}
	[[nodiscard]] std::optional<std::pair<ecs::components::Transform, RigidBodyDetails>>
	RayCastClosestHit(const glm::vec3& origin, const glm::vec3& direction, float) const final
	{
		if (!(direction.y < 0.0f))
		{
			return std::nullopt;
		}
		ecs::components::Transform transform {};
		transform.position = origin + direction * (-origin.y / direction.y);
		return {{transform, {.type = RigidBodyType::Terrain, .id = 0, .userData = nullptr}}};
	}
};

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

/// The keys and buttons held, the mouse and the hand, as a test sets them; the cursor goes where the camera puts it
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
	[[nodiscard]] float GetMouseWheelDelta() const final { return wheel; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final { return {hand, std::nullopt}; }
	void WarpCursor(glm::ivec2 position) final
	{
		mouse = glm::uvec2(position);
		warps.push_back(position);
	}
	void Frame() final {}
	void ProcessEvent(const SDL_Event&) final {}

	/// The mouse moved by a pixel delta this frame
	void MoveMouse(glm::ivec2 delta)
	{
		mouseDelta = delta;
		mouse = glm::uvec2(glm::ivec2(mouse) + delta);
	}

	uint64_t held {0};
	uint8_t heldUnbindable {0};
	std::optional<glm::vec3> hand;
	glm::uvec2 mouse {k_Screen.x / 2, k_Screen.y / 2};
	glm::ivec2 mouseDelta {0, 0};
	float wheel {0.0f};
	/// Where the camera put the cursor, in order
	std::vector<glm::ivec2> warps;
};

/// The angle between the camera's views across the land, from the focus to the camera
float TurnBetween(glm::vec3 originBefore, glm::vec3 focusBefore, glm::vec3 originAfter, glm::vec3 focusAfter)
{
	const auto before = glm::xz(originBefore - focusBefore);
	const auto after = glm::xz(originAfter - focusAfter);
	return std::abs(std::atan2(before.x * after.y - before.y * after.x, glm::dot(before, after)));
}

/// The camera's pitch, up from the land to the camera as seen from the focus
float PitchOf(glm::vec3 origin, glm::vec3 focus)
{
	const auto diff = origin - focus;
	return std::atan2(diff.y, glm::length(glm::xz(diff)));
}

class WorldCameraMouse: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		Locator::windowing::emplace<FakeWindowing>();
		Locator::terrainSystem::emplace<LevelIsland>();
		Locator::dynamicsSystem::emplace<LevelDynamics>();
		_help = &Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>().Get();
		Locator::cinematicDirectorSystem::reset();
		Locator::config::reset();
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		_camera->SetProjectionMatrixPerspective(k_FieldOfView, static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y),
		                                        1.0f, 65536.0f);
		hp::Reset();
		hp::SetQueries({.paused = [] { return false; }, .scriptWideScreen = [] { return false; }});
	}
	void TearDown() override
	{
		hp::SetQueries({});
		hp::Reset();
	}

	/// The camera at origin, looking at focus on the land; the hand on the land at the focus
	void Place(glm::vec3 origin, glm::vec3 focus)
	{
		_camera->SetOrigin(origin);
		_camera->SetFocus(focus);
		_actions->hand = focus;
	}

	/// A frame of the game: the controls, then the camera's update (the camera goes where the model sends it), then a
	/// turn of the help profile, so that each frame's events count
	void Frame()
	{
		_model.HandleActions(k_Frame);
		std::ignore = _model.Update(k_Frame, *_camera);
		_camera->SetOrigin(_model.GetTargetOrigin());
		_camera->SetFocus(_model.GetTargetFocus());
		hp::Process();
	}

	/// A frame in which the camera follows the model as the game's does: what the update gives the camera's zoomers,
	/// which then move
	std::optional<CameraModel::CameraInterpolationUpdateInfo> FlownFrame()
	{
		_model.HandleActions(k_Frame);
		auto info = _model.Update(k_Frame, *_camera);
		_camera->UpdateZoomers(info, std::chrono::duration<float>(k_Frame).count());
		hp::Process();
		return info;
	}

	/// No flight on its way: for longer than a flight's first half, every frame gives the camera the model's own place,
	/// so no last leg comes
	void ExpectNoFlight()
	{
		for (int i = 0; i < 40; ++i)
		{
			const auto info = FlownFrame();
			ASSERT_TRUE(info.has_value());
			EXPECT_EQ(info->origin, _model.GetTargetOrigin());
			EXPECT_EQ(info->focus, _model.GetTargetFocus());
		}
	}

	/// How many frames counted a help event
	[[nodiscard]] static float Counted(hp::Event event) { return hp::TotalEvents(static_cast<int32_t>(event)).value(); }

	DefaultWorldCameraModel _model;
	Camera* _camera {nullptr};
	FakeActions* _actions {nullptr};
	camera_help::CameraHelp* _help {nullptr};

private:
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::dynamicsSystem> _restoreDynamics;
	test::RestoreService<Locator::cameraHelpSystem> _restoreHelp;
	test::RestoreService<Locator::cinematicDirectorSystem> _restoreCinematics;
	test::RestoreService<Locator::config> _restoreConfig;
	test::RestoreService<Locator::camera> _restoreCamera;
};

constexpr glm::vec3 k_Origin {2560.0f, 300.0f, 2000.0f};
constexpr glm::vec3 k_Focus {2560.0f, 0.0f, 2500.0f};
} // namespace

TEST_F(WorldCameraMouse, UntilAnEdgeDragIsDecidedTheCameraStays)
{
	Place(k_Origin, k_Focus);
	_actions->mouse = k_RightEdge;
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	const auto origin = _model.GetTargetOrigin();
	// 10 pixels down a screen 800 wide is under a fiftieth: not decided
	_actions->MoveMouse({0, 10});
	Frame();
	ASSERT_TRUE(_model.GetHandCues().dragging);
	EXPECT_FALSE(_model.GetHandCues().dragMode.has_value());
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	EXPECT_TRUE(_actions->warps.empty());
	EXPECT_EQ(Counted(hp::Event::Rotate), 0.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 0.0f);
}

TEST_F(WorldCameraMouse, AnEdgeDragPutsTheCursorOnTheRingAndTurnsTheCameraByTheAngleSwept)
{
	const auto viewHeight = camera_drag::ViewHeight(k_Screen, false);
	Place(k_Origin, k_Focus);
	_actions->mouse = k_RightEdge;
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();

	// 40 pixels down the right edge decides it: the camera turns round the edge
	const auto originBefore = _model.GetTargetOrigin();
	const auto focusBefore = _model.GetTargetFocus();
	_actions->MoveMouse({0, 40});
	const auto step = camera_drag::EdgeRotate(glm::ivec2(_actions->mouse), glm::ivec2(k_RightEdge), k_Screen, viewHeight);
	Frame();
	ASSERT_EQ(_model.GetHandCues().dragMode, camera_drag::DragMode::EdgeRotate);
	ASSERT_EQ(_actions->warps.size(), 1u);
	EXPECT_EQ(_actions->warps[0], step.cursor);
	ASSERT_LT(step.angle, -0.01f);
	EXPECT_NEAR(TurnBetween(originBefore, focusBefore, _model.GetTargetOrigin(), _model.GetTargetFocus()), -step.angle, 1e-3f);
	// The camera help: a drag, turning left. The player's own turn report does not count it again
	EXPECT_EQ(Counted(hp::Event::Rotate), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCW), 0.0f);

	// Held still on the ring: no drag, but it counts as turning left again, as the original's test does
	_actions->MoveMouse({0, 0});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Rotate), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 2.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCW), 0.0f);

	// Back up the edge turns it right
	const auto onRing = glm::ivec2(_actions->mouse);
	_actions->MoveMouse({0, -80});
	const auto back = camera_drag::EdgeRotate(glm::ivec2(_actions->mouse), onRing, k_Screen, viewHeight);
	ASSERT_GT(back.angle, 0.01f);
	Frame();
	EXPECT_EQ(_actions->warps.back(), back.cursor);
	EXPECT_EQ(Counted(hp::Event::Rotate), 2.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCW), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 2.0f);
}

TEST_F(WorldCameraMouse, AnEdgeDragDoesNothingOnceTurningIsTakenAway)
{
	Place(k_Origin, k_Focus);
	_actions->mouse = k_RightEdge;
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	_actions->MoveMouse({0, 40});
	Frame();
	ASSERT_EQ(_model.GetHandCues().dragMode, camera_drag::DragMode::EdgeRotate);
	ASSERT_EQ(_actions->warps.size(), 1u);
	ASSERT_EQ(Counted(hp::Event::RotateCCW), 1.0f);

	// A script takes turning away during the drag: the camera, the cursor and the camera help are left alone
	_help->Enable(0, camera_help::Bit(camera_help::Feature::Rotate));
	const auto origin = _model.GetTargetOrigin();
	_actions->MoveMouse({0, 40});
	Frame();
	EXPECT_EQ(_model.GetHandCues().dragMode, camera_drag::DragMode::EdgeRotate);
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	EXPECT_EQ(_actions->warps.size(), 1u);
	EXPECT_EQ(Counted(hp::Event::Rotate), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 1.0f);
}

TEST_F(WorldCameraMouse, ADragUpFromTheBottomTiltsTheCamera)
{
	Place(k_Origin, k_Focus);
	_actions->mouse = k_Bottom;
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	const auto pitchBefore = PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus());

	// 20 pixels up: more up and down than across, so it tilts; down the whole screen tilts by 2.33333 fields of view
	_actions->MoveMouse({0, -20});
	Frame();
	ASSERT_EQ(_model.GetHandCues().dragMode, camera_drag::DragMode::Pitch);
	const auto step = camera_drag::PitchStep(-20, k_Screen.y, glm::radians(k_FieldOfView));
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), pitchBefore - step, 1e-3f);
	EXPECT_TRUE(_actions->warps.empty());
	// The camera help counts the tilt once, as a drag's, and no turning
	EXPECT_EQ(Counted(hp::Event::Pitch), 1.0f);
	EXPECT_EQ(Counted(hp::Event::Rotate), 0.0f);

	// Held still: nothing tilts and nothing counts
	_actions->MoveMouse({0, 0});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Pitch), 1.0f);
}

TEST_F(WorldCameraMouse, InAWindowTheVeryBottomOffersTiltingNearerTheMiddle)
{
	// 0.47 of the view's height below the middle: tilting only in a window
	const glm::uvec2 nearBottom {400, 582};
	Place(k_Origin, k_Focus);
	_actions->mouse = nearBottom;
	Frame();
	EXPECT_EQ(_model.GetHandCues().tricons & camera_drag::tricon::k_Pitch, 0u);

	Locator::config::emplace().displayMode = windowing::DisplayMode::Fullscreen;
	Frame();
	EXPECT_EQ(_model.GetHandCues().tricons & camera_drag::tricon::k_Pitch, 0u);

	Locator::config::value().displayMode = windowing::DisplayMode::Windowed;
	Frame();
	EXPECT_NE(_model.GetHandCues().tricons & camera_drag::tricon::k_Pitch, 0u);
}

namespace
{
/// The tutorial levels' self-tilting pitch and height
constexpr float k_TutorialPitch = 0.448799f;
constexpr float k_TutorialHeight = 15.0f;

/// The pitch the self-tilting camera measures from, the original's heading and pitch between two points
float AutoPitchFrom(glm::vec3 origin, glm::vec3 focus)
{
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(origin, focus, heading, pitch);
	return pitch;
}
} // namespace

TEST_F(WorldCameraMouse, TheSelfTiltingCameraTiltsAFifthOfTheWayAndKeepsToItsHeight)
{
	Place(k_Origin, k_Focus);
	Frame();
	_help->SetAutoPitch(k_TutorialPitch, k_TutorialHeight, true);
	const auto start = _model.GetTargetOrigin();
	const auto pitchBefore = PitchOf(start, _model.GetTargetFocus());
	const auto input = camera_help::AutoPitchInput(k_TutorialPitch, AutoPitchFrom(start, _model.GetTargetFocus()),
	                                               std::chrono::duration<float>(k_Frame).count());
	ASSERT_TRUE(input.has_value());
	Frame();
	// The tilt is pitch input at 0.002 a unit; the camera stays where it was, 15 above the land
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), pitchBefore - *input * 0.002f, 1e-4f);
	EXPECT_EQ(_model.GetTargetOrigin(), glm::vec3(start.x, k_TutorialHeight, start.z));
	// It is not the player's tilt: the camera help counts nothing
	EXPECT_EQ(Counted(hp::Event::Pitch), 0.0f);
	EXPECT_EQ(Counted(hp::Event::Rotate), 0.0f);
}

TEST_F(WorldCameraMouse, TheSelfTiltingCameraTiltsWithoutThePitchFeature)
{
	Place(k_Origin, k_Focus);
	Frame();
	_help->SetAutoPitch(k_TutorialPitch, k_TutorialHeight, true);
	_help->Enable(0, camera_help::Bit(camera_help::Feature::Pitch));
	const auto pitchBefore = PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus());
	Frame();
	EXPECT_LT(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), pitchBefore - 1e-3f);
	EXPECT_EQ(Counted(hp::Event::Pitch), 0.0f);
}

TEST_F(WorldCameraMouse, TheSelfTiltingCameraDropsThePlayersTiltAndKeepsItsHeightOnlyWhenIdleOrTilting)
{
	// Already at the self-tilting pitch: nothing to tilt
	const glm::vec3 origin {2560.0f, 500.0f * std::tan(k_TutorialPitch), 2000.0f};
	Place(origin, k_Focus);
	Frame();
	_help->SetAutoPitch(k_TutorialPitch, k_TutorialHeight, true);
	ASSERT_FALSE(camera_help::AutoPitchInput(k_TutorialPitch, AutoPitchFrom(_model.GetTargetOrigin(), _model.GetTargetFocus()),
	                                         std::chrono::duration<float>(k_Frame).count())
	                 .has_value());

	// The player tilts: the tilt is dropped, and the camera, moved by the player, keeps its height
	const auto pitchBefore = PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus());
	_actions->held = static_cast<uint64_t>(BindableActionMap::TILT_UP);
	Frame();
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), pitchBefore, 1e-4f);
	EXPECT_NEAR(_model.GetTargetOrigin().y, origin.y, 0.1f);
	EXPECT_EQ(Counted(hp::Event::Pitch), 0.0f);

	// Let go, nothing is done: the camera goes to 15 above the land
	_actions->held = 0;
	const auto start = _model.GetTargetOrigin();
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), glm::vec3(start.x, k_TutorialHeight, start.z));
}

TEST_F(WorldCameraMouse, GrippingTheLandStopsTheSelfTiltingCamera)
{
	Place(k_Origin, k_Focus);
	Frame();
	_help->SetAutoPitch(k_TutorialPitch, k_TutorialHeight, true);
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	const auto pitchBefore = PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus());
	Frame();
	Frame();
	ASSERT_TRUE(_model.GetHandCues().dragging);
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), pitchBefore, 1e-4f);
	EXPECT_NEAR(_model.GetTargetOrigin().y, k_Origin.y, 0.1f);
}

namespace
{
/// How far a unit of zoom input moves the camera: 0.0015 x three times its height above its focus, at least 60, at most
/// 2000
float ZoomPerInput(glm::vec3 origin, glm::vec3 focus)
{
	return 0.0015f * glm::clamp(3.0f * (origin.y - focus.y), 60.0f, 2000.0f);
}
} // namespace

TEST_F(WorldCameraMouse, TheWheelZoomsSixtyANotchWhateverTheFrameTime)
{
	Place(k_Origin, k_Focus);
	Frame();
	// Two notches away from the player, with the zoom in action the wheel performs: 120 of zoom input, nearer
	auto origin = _model.GetTargetOrigin();
	auto focus = _model.GetTargetFocus();
	auto expected = glm::distance(origin, focus) - 120.0f * ZoomPerInput(origin, focus);
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_IN);
	_actions->wheel = 2.0f;
	Frame();
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), expected, 0.5f);
	EXPECT_EQ(Counted(hp::Event::Zoom), 1.0f);

	// A notch towards the player in a frame three times as long: 60 back out, the same per notch
	origin = _model.GetTargetOrigin();
	focus = _model.GetTargetFocus();
	expected = glm::distance(origin, focus) + 60.0f * ZoomPerInput(origin, focus);
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	_actions->wheel = -1.0f;
	_model.HandleActions(3 * k_Frame);
	std::ignore = _model.Update(3 * k_Frame, *_camera);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), expected, 0.5f);
}

TEST_F(WorldCameraMouse, AZoomKeyIsANotchAFrameAndTheWheelWithoutOneIsLost)
{
	Place(k_Origin, k_Focus);
	Frame();
	// The zoom out key without the wheel: one notch out, 60 of zoom input
	auto origin = _model.GetTargetOrigin();
	auto focus = _model.GetTargetFocus();
	const auto expected = glm::distance(origin, focus) + 60.0f * ZoomPerInput(origin, focus);
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	Frame();
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), expected, 0.5f);

	// The wheel turned with no zoom action performed zooms nothing
	_actions->held = 0;
	_actions->wheel = 3.0f;
	Frame();
	origin = _model.GetTargetOrigin();
	focus = _model.GetTargetFocus();
	Frame();
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), glm::distance(origin, focus), 1e-3f);
	EXPECT_EQ(Counted(hp::Event::Zoom), 1.0f);
}

TEST_F(WorldCameraMouse, BothButtonsZoomAndTurnOnceTheMouseHasMovedFarEnoughAcross)
{
	Place(k_Origin, k_Focus);
	Frame();
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK);

	// 30 down zooms out by 30 x 1.9; 10 across is under a fortieth of the 800 wide screen: no turn
	auto origin = _model.GetTargetOrigin();
	auto focus = _model.GetTargetFocus();
	const auto expected = glm::distance(origin, focus) + 30.0f * 1.9f * ZoomPerInput(origin, focus);
	_actions->MoveMouse({10, 30});
	Frame();
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), expected, 0.5f);
	EXPECT_NEAR(TurnBetween(origin, focus, _model.GetTargetOrigin(), _model.GetTargetFocus()), 0.0f, 1e-4f);
	EXPECT_EQ(Counted(hp::Event::Rotate), 0.0f);

	// 15 more across: 25 since the press, past 20, so it turns by 15 x 1.9 of turn input, pi / 800 a unit
	origin = _model.GetTargetOrigin();
	focus = _model.GetTargetFocus();
	_actions->MoveMouse({15, 0});
	Frame();
	EXPECT_NEAR(TurnBetween(origin, focus, _model.GetTargetOrigin(), _model.GetTargetFocus()),
	            15.0f * 1.9f * glm::pi<float>() / 800.0f, 1e-3f);
	EXPECT_EQ(Counted(hp::Event::Rotate), 1.0f);
	EXPECT_EQ(Counted(hp::Event::RotateCW), 1.0f);

	// Once turning, any move across turns
	origin = _model.GetTargetOrigin();
	focus = _model.GetTargetFocus();
	_actions->MoveMouse({-4, 0});
	Frame();
	EXPECT_NEAR(TurnBetween(origin, focus, _model.GetTargetOrigin(), _model.GetTargetFocus()),
	            4.0f * 1.9f * glm::pi<float>() / 800.0f, 1e-3f);
	EXPECT_EQ(Counted(hp::Event::RotateCCW), 1.0f);

	// Let go and pressed again: the move across starts from the new press
	_actions->heldUnbindable = 0;
	_actions->MoveMouse({0, 0});
	Frame();
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK);
	origin = _model.GetTargetOrigin();
	focus = _model.GetTargetFocus();
	_actions->MoveMouse({10, 0});
	Frame();
	EXPECT_NEAR(TurnBetween(origin, focus, _model.GetTargetOrigin(), _model.GetTargetFocus()), 0.0f, 1e-4f);
}

TEST_F(WorldCameraMouse, WithoutTheirFeatureTheMiddleButtonAndBothButtonsDoNothing)
{
	Place(k_Origin, k_Focus);
	_help->Enable(0, camera_help::Bit(camera_help::Feature::JustZoom));
	Frame();
	const auto origin = _model.GetTargetOrigin();
	const auto distance = glm::distance(origin, _model.GetTargetFocus());

	_actions->held = static_cast<uint64_t>(BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	_actions->MoveMouse({30, 30});
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), origin);

	_actions->held = 0;
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK);
	_actions->MoveMouse({30, 30});
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), distance, 1e-3f);
	EXPECT_EQ(Counted(hp::Event::Rotate), 0.0f);
	EXPECT_EQ(Counted(hp::Event::Pitch), 0.0f);
	EXPECT_EQ(Counted(hp::Event::Zoom), 0.0f);
}

namespace
{
/// Where the clear view ends: the hand's point on the land, seen from the camera's heading, at its distance and tilted to
/// a quarter of the sum of the camera's pitch and three quarters of pi
std::pair<glm::vec3, glm::vec3> ClearViewOf(glm::vec3 origin, glm::vec3 focus, glm::vec3 hand, float distance)
{
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(origin, focus, heading, pitch);
	const auto point = glm::vec3(hand.x, 0.0f, hand.z);
	return {
	    script_camera::PointFromDistanceHeadingAndPitch(point, distance, heading, (pitch + 0.75f * glm::pi<float>()) * 0.25f),
	    point};
}

constexpr auto k_ClearView =
    static_cast<uint64_t>(BindableActionMap::ZOOM_ON) | static_cast<uint64_t>(BindableActionMap::ROTATE_ON);
/// Two seconds of frames: the half-second glide, planned again every frame, has settled within a hundred thousandth
constexpr int k_ClearViewFrames = 60;
} // namespace

TEST_F(WorldCameraMouse, CtrlAndShiftGlideToAClearViewOfTheHandsPointAndBack)
{
	Place(k_Origin, k_Focus);
	Frame();
	const auto origin = _model.GetTargetOrigin();
	const auto focus = _model.GetTargetFocus();
	// More than 300 from its focus: the close view is 50 away
	ASSERT_GT(glm::distance(origin, focus), 300.0f);
	const auto [closeOrigin, closeFocus] = ClearViewOf(origin, focus, k_Focus, 50.0f);

	_actions->held = k_ClearView;
	Frame();
	// Already on its way after a frame
	EXPECT_LT(glm::distance(_model.GetTargetOrigin(), closeOrigin), glm::distance(origin, closeOrigin));
	EXPECT_GT(glm::distance(_model.GetTargetOrigin(), closeOrigin), 1.0f);
	for (int i = 1; i < k_ClearViewFrames; ++i)
	{
		Frame();
	}
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), closeOrigin), 0.0f, 0.05f);
	EXPECT_NEAR(glm::distance(_model.GetTargetFocus(), closeFocus), 0.0f, 0.05f);

	// Let go: back to within a hundredth of the way, where the glide stops moving the camera
	_actions->held = 0;
	for (int i = 0; i < k_ClearViewFrames; ++i)
	{
		Frame();
	}
	EXPECT_LT(glm::distance(_model.GetTargetOrigin(), origin), 0.02f * glm::distance(origin, closeOrigin));
}

TEST_F(WorldCameraMouse, TheClearViewIs15AwayFromACameraBetween50And300FromItsFocus)
{
	// 100 from its focus, 99 once the camera has settled on the land 1 short of it
	Place({2560.0f, 60.0f, 2420.0f}, k_Focus);
	Frame();
	const auto [closeOrigin, closeFocus] = ClearViewOf(_model.GetTargetOrigin(), _model.GetTargetFocus(), k_Focus, 15.0f);
	_actions->held = k_ClearView;
	for (int i = 0; i < k_ClearViewFrames; ++i)
	{
		Frame();
	}
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), closeOrigin), 0.0f, 0.05f);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), 15.0f, 0.05f);
}

TEST_F(WorldCameraMouse, TheClearViewIs10AwayFromACameraNearerThan50ToItsFocus)
{
	// 40 from its focus, 39 once settled
	Place({2560.0f, 24.0f, 2468.0f}, k_Focus);
	Frame();
	const auto [closeOrigin, closeFocus] = ClearViewOf(_model.GetTargetOrigin(), _model.GetTargetFocus(), k_Focus, 10.0f);
	_actions->held = k_ClearView;
	for (int i = 0; i < k_ClearViewFrames; ++i)
	{
		Frame();
	}
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), closeOrigin), 0.0f, 0.05f);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), _model.GetTargetFocus()), 10.0f, 0.05f);
}

TEST_F(WorldCameraMouse, TheWheelIsOffDuringTheClearView)
{
	Place(k_Origin, k_Focus);
	Frame();
	_actions->held = k_ClearView | static_cast<uint64_t>(BindableActionMap::ZOOM_IN);
	_actions->wheel = 2.0f;
	Frame();
	Frame();
	EXPECT_EQ(Counted(hp::Event::Zoom), 0.0f);
}

TEST_F(WorldCameraMouse, WithoutTheHandOverTheLandTheClearViewLeavesTheCamera)
{
	Place(k_Origin, k_Focus);
	_actions->hand.reset();
	Frame();
	const auto origin = _model.GetTargetOrigin();
	_actions->held = k_ClearView;
	for (int i = 0; i < 5; ++i)
	{
		Frame();
	}
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
}

namespace
{
/// A bookmark's view, 2000 across from the camera
constexpr glm::vec3 k_FlightOrigin {4560.0f, 300.0f, 2000.0f};
constexpr glm::vec3 k_FlightFocus {4560.0f, 0.0f, 2500.0f};
/// A flight's first leg, to its middle point in 0.9 of 1.5 s
constexpr double k_FirstLegMicroseconds = 1'350'000.0;
} // namespace

TEST_F(WorldCameraMouse, AKeyboardMoveDropsABookmarkFlight)
{
	Place(k_Origin, k_Focus);
	Frame();
	_model.SetFlight(k_FlightOrigin, k_FlightFocus);
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_NEAR(static_cast<double>(info->duration.count()), k_FirstLegMicroseconds, 1.0);

	// Moving forwards drops the flight: the camera gets the frame's own place, from where the leg was taking it
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE_FORWARDS);
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->origin, _model.GetTargetOrigin());
	EXPECT_EQ(info->focus, _model.GetTargetFocus());

	// Let go, the last leg never comes
	_actions->held = 0;
	ExpectNoFlight();
}

TEST_F(WorldCameraMouse, AZoomDropsABookmarkFlight)
{
	Place(k_Origin, k_Focus);
	Frame();
	_model.SetFlight(k_FlightOrigin, k_FlightFocus);
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_NEAR(static_cast<double>(info->duration.count()), k_FirstLegMicroseconds, 1.0);

	// A notch of the zoom key drops it too
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->origin, _model.GetTargetOrigin());
	EXPECT_EQ(info->focus, _model.GetTargetFocus());

	_actions->held = 0;
	ExpectNoFlight();
}

TEST_F(WorldCameraMouse, WithNoInputABookmarkFlightFliesOn)
{
	Place(k_Origin, k_Focus);
	Frame();
	_model.SetFlight(k_FlightOrigin, k_FlightFocus);
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_NEAR(static_cast<double>(info->duration.count()), k_FirstLegMicroseconds, 1.0);

	// Nothing reaches the camera until the zoomers are past 0.75 s; then the last leg, to the bookmark's view in 1.5 s
	int frames = 0;
	for (info = FlownFrame(); !info.has_value() && frames < 60; info = FlownFrame())
	{
		++frames;
	}
	ASSERT_TRUE(info.has_value());
	EXPECT_GT(frames, 0);
	EXPECT_EQ(info->origin, k_FlightOrigin);
	EXPECT_EQ(info->focus, k_FlightFocus);
	EXPECT_EQ(info->duration, 1'500'000us);
}

TEST_F(WorldCameraMouse, TheDoubleClicksFlightIsKeptInTheFrameItStartsAndDroppedInTheNext)
{
	Place(k_Origin, k_Focus);
	Frame();
	// Double clicked with the button held, on the land 640 across from the middle of the screen: a flight with a middle
	// point. The grip drops flights, but the double click's starts after the controls, so it is kept
	_actions->hand = glm::vec3(3200.0f, 0.0f, 2500.0f);
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK);
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_NEAR(static_cast<double>(info->duration.count()), k_FirstLegMicroseconds, 1.0);
	EXPECT_NE(info->origin, _model.GetTargetOrigin());

	// The button still held in the next frame drops it
	_actions->heldUnbindable = 0;
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->origin, _model.GetTargetOrigin());
	EXPECT_EQ(info->focus, _model.GetTargetFocus());
}

namespace
{
/// Double clicked on the land 640 across from the middle of the screen: the flight 100 away
constexpr glm::vec3 k_ClickedPoint {3200.0f, 0.0f, 2500.0f};

/// The pitch the double click's flight takes on level land: a fifth of the camera's, plus a tenth of the land's own
/// tilt at the point (straight up, 1.5393804) and 3 pi / 25
float FlightPitch(float cameraPitch)
{
	return cameraPitch * 0.2f + 1.5393804f * 0.1f + 3.0f * glm::pi<float>() / 25.0f;
}
} // namespace

TEST_F(WorldCameraMouse, OnLevelLandTheDoubleClicksFlightKeepsTheCamerasHeading)
{
	Place(k_Origin, k_Focus);
	Frame();
	const auto originBefore = _model.GetTargetOrigin();
	const auto focusBefore = _model.GetTargetFocus();
	_actions->hand = k_ClickedPoint;
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK);
	ASSERT_TRUE(FlownFrame().has_value());
	// Every side sees the same land, so the heading the camera had wins
	EXPECT_EQ(_model.GetTargetFocus(), k_ClickedPoint);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), k_ClickedPoint), 100.0f, 1e-3f);
	EXPECT_NEAR(TurnBetween(originBefore, focusBefore, _model.GetTargetOrigin(), _model.GetTargetFocus()), 0.0f, 1e-4f);
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), FlightPitch(PitchOf(originBefore, focusBefore)),
	            1e-4f);
}

TEST_F(WorldCameraMouse, OverAHillTheDoubleClicksFlightLooksFromTheOpenSide)
{
	// A wall 100 high on the camera's side of the point (-z), from x = -0.25 to x = 0.5 of the way back: the camera's
	// heading, the first sixteenth of pi towards -x and the first two towards +x see it. Of the open ones the second
	// sixteenth towards -x is the nearest to the camera's heading
	const glm::vec2 point {k_ClickedPoint.x, k_ClickedPoint.z};
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([point](glm::vec2 xz) {
		const auto d = xz - point;
		const float back = -d.y;
		return back > 1.0f && back < 150.0f && d.x > -0.25f * back && d.x < 0.5f * back ? 100.0f : 0.0f;
	});
	Place(k_Origin, k_Focus);
	Frame();
	const auto originBefore = _model.GetTargetOrigin();
	const auto focusBefore = _model.GetTargetFocus();
	_actions->hand = k_ClickedPoint;
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK);
	ASSERT_TRUE(FlownFrame().has_value());
	EXPECT_EQ(_model.GetTargetFocus(), k_ClickedPoint);
	EXPECT_NEAR(glm::distance(_model.GetTargetOrigin(), k_ClickedPoint), 100.0f, 1e-3f);
	EXPECT_NEAR(TurnBetween(originBefore, focusBefore, _model.GetTargetOrigin(), _model.GetTargetFocus()),
	            glm::pi<float>() / 8.0f, 1e-4f);
	EXPECT_LT(_model.GetTargetOrigin().x, k_ClickedPoint.x);
	// The land's tilt is read at the point, level here: the same pitch as on level land
	EXPECT_NEAR(PitchOf(_model.GetTargetOrigin(), _model.GetTargetFocus()), FlightPitch(PitchOf(originBefore, focusBefore)),
	            1e-4f);
}
