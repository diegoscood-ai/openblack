/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world camera's land grip: a grip too far ahead gives the drag up and flies to the hand's point, and a drag stops
// short of the sea only near the drawn camera. Worked out from fake controls, a fake level land and a fake sea. The
// camera help's Drag and double click events are counted by the help profile, one game turn a frame

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <LNDFile.h>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraDrag.h"
#include "Camera/CameraHelp.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "Help/HelpProfile.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"
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
constexpr glm::uvec2 k_Middle {400, 300};

void ExpectNear(glm::vec3 actual, glm::vec3 expected, float epsilon = 1e-2f)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
	EXPECT_NEAR(actual.z, expected.z, epsilon);
}

/// Level land at 0, or only the sea without any land
class LevelIsland final: public LandIslandInterface
{
public:
	explicit LevelIsland(bool land)
	    : _land(land)
	{
	}

	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { return _cell; }
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2&) const final { return _land; }
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2) const final { return {0, 0, 0, 0}; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
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

private:
	bool _land;
	lnd::LNDCell _cell {};
	std::vector<lnd::LNDCountry> _countries;
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

/// The keys and buttons held, the mouse and the hand, as a test sets them
class FakeActions final: public input::GameActionInterface
{
public:
	[[nodiscard]] bool GetBindable(BindableActionMap action) const final { return (held & static_cast<uint64_t>(action)) != 0; }
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap) const final { return false; }
	[[nodiscard]] bool GetBindableChanged(BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap) const final { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap) const final { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const final { return mouse; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const final { return mouseDelta; }
	[[nodiscard]] float GetMouseWheelDelta() const final { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final { return {hand, std::nullopt}; }
	void Frame() final {}
	void ProcessEvent(const SDL_Event&) final {}

	/// The mouse moved by a pixel delta this frame
	void MoveMouse(glm::ivec2 delta)
	{
		mouseDelta = delta;
		mouse = glm::uvec2(glm::ivec2(mouse) + delta);
	}

	uint64_t held {0};
	std::optional<glm::vec3> hand;
	glm::uvec2 mouse {k_Middle};
	glm::ivec2 mouseDelta {0, 0};
};

/// Where the flight after a drag given up puts the camera: 1000 from the point, heading along the land (yaw pi, from
/// -z) and tilted to the lowest pitch the flight allows, pi / 8
glm::vec3 GivenUpFlightOrigin(glm::vec3 point)
{
	const float pitch = glm::pi<float>() / 8.0f;
	return point + 1000.0f * glm::vec3(0.0f, glm::sin(pitch), -glm::cos(pitch));
}

class WorldCameraLandGrip: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		Locator::windowing::emplace<FakeWindowing>();
		Locator::terrainSystem::emplace<LevelIsland>(true);
		Locator::dynamicsSystem::emplace<LevelDynamics>();
		_help = &Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>().Get();
		Locator::cinematicDirectorSystem::reset();
		_camera.SetProjectionMatrixPerspective(70.0f, static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y), 1.0f,
		                                       65536.0f);
		hp::Reset();
		hp::SetQueries({.paused = [] { return false; }, .scriptWideScreen = [] { return false; }});
	}
	void TearDown() override
	{
		hp::SetQueries({});
		hp::Reset();
	}

	/// The camera at origin, looking at focus on the land; the hand on the land at the focus, under the cursor in the
	/// middle of the screen
	void Place(glm::vec3 origin, glm::vec3 focus)
	{
		_camera.SetOrigin(origin);
		_camera.SetFocus(focus);
		_actions->hand = focus;
	}

	/// A frame of the game: the controls, then the camera's update (the camera itself stays where it was put), then a
	/// turn of the help profile, so that each frame's events count
	void Frame()
	{
		_model.HandleActions(k_Frame);
		std::ignore = _model.Update(k_Frame, _camera);
		hp::Process();
	}

	/// How many frames counted a help event
	[[nodiscard]] static float Counted(hp::Event event) { return hp::TotalEvents(static_cast<int32_t>(event)).value(); }

	[[nodiscard]] uint32_t IdleHints() const
	{
		return camera_drag::IdleTricons(
		    camera_drag::NormalisedCursor(glm::ivec2(k_Middle), k_Screen, camera_drag::ViewHeight(k_Screen, false)), true);
	}

	Camera _camera;
	DefaultWorldCameraModel _model;
	FakeActions* _actions {nullptr};
	camera_help::CameraHelp* _help {nullptr};

private:
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::dynamicsSystem> _restoreDynamics;
	test::RestoreService<Locator::cameraHelpSystem> _restoreHelp;
	test::RestoreService<Locator::cinematicDirectorSystem> _restoreCinematics;
};
} // namespace

TEST_F(WorldCameraLandGrip, AGripTooFarAheadGivesUpAndFliesToTheHand)
{
	// The land under the cursor is sqrt(400^2 + 4000^2) = 4020 ahead, more than 3000
	const glm::vec3 origin {2560.0f, 400.0f, -1500.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2500.0f};
	Place(origin, focus);
	Frame();

	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	// No pan: the camera goes 1000 from the hand's point, at once
	ExpectNear(_model.GetTargetFocus(), focus);
	ExpectNear(_model.GetTargetOrigin(), GivenUpFlightOrigin(focus));
	auto cues = _model.GetHandCues();
	EXPECT_FALSE(cues.dragging);
	EXPECT_EQ(cues.tricons, camera_drag::tricon::k_GivenUp);
	// The flight is reported as the double click's is, once, and nothing is dragged
	EXPECT_EQ(Counted(hp::Event::DoubleClickPos), 1.0f);
	EXPECT_EQ(Counted(hp::Event::Drag), 0.0f);

	// Held on, with a key that would turn the camera: everything is dropped, nothing turns and nothing is dragged
	_actions->held |= static_cast<uint64_t>(BindableActionMap::ROTATE_LEFT);
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	cues = _model.GetHandCues();
	EXPECT_FALSE(cues.dragging);
	EXPECT_EQ(cues.tricons, IdleHints());
	EXPECT_EQ(Counted(hp::Event::DoubleClickPos), 1.0f);
	EXPECT_EQ(Counted(hp::Event::Drag), 0.0f);

	// Once everything is let go, a new press drags again
	_actions->held = 0;
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_model.HandleActions(k_Frame);
	EXPECT_TRUE(_model.GetHandCues().dragging);
}

TEST_F(WorldCameraLandGrip, WithoutTheFlightAGivenUpDragLeavesTheCamera)
{
	const glm::vec3 origin {2560.0f, 400.0f, -1500.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2500.0f};
	Place(origin, focus);
	_help->Enable(0, camera_help::Bit(camera_help::Feature::DoubleClickFly));
	Frame();

	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), origin);
	EXPECT_EQ(_model.GetTargetFocus(), focus);
	EXPECT_EQ(_model.GetHandCues().tricons, camera_drag::tricon::k_GivenUp);
	// No flight, so nothing reported
	EXPECT_EQ(Counted(hp::Event::DoubleClickPos), 0.0f);
}

TEST_F(WorldCameraLandGrip, APanCountsAsADragInEachFrameTheMouseMovesMoreThan2Pixels)
{
	const glm::vec3 origin {2560.0f, 300.0f, 2000.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2500.0f};
	Place(origin, focus);
	Frame();

	// The press grips the land without moving the mouse: not a drag yet
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	ASSERT_TRUE(_model.GetHandCues().dragging);
	EXPECT_EQ(Counted(hp::Event::Drag), 0.0f);

	// 3 pixels across: the land is dragged, and the frame counts
	_actions->MoveMouse({3, 0});
	Frame();
	EXPECT_NE(_model.GetTargetOrigin(), origin);
	EXPECT_EQ(Counted(hp::Event::Drag), 1.0f);

	// 2 pixels across and down: dragged still, but not counted
	_actions->MoveMouse({2, -2});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Drag), 1.0f);

	// Held still: not counted
	_actions->MoveMouse({0, 0});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Drag), 1.0f);

	// 3 pixels down counts again
	_actions->MoveMouse({0, -3});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Drag), 2.0f);
	EXPECT_EQ(Counted(hp::Event::DoubleClickPos), 0.0f);
}

TEST_F(WorldCameraLandGrip, WithoutAGripTheMouseMovingIsNoDrag)
{
	const glm::vec3 origin {2560.0f, 300.0f, 2000.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2500.0f};
	Place(origin, focus);
	Frame();

	// The mouse moves with no button held
	_actions->MoveMouse({5, 5});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Drag), 0.0f);

	// The grab button held with the hand off the land grips nothing
	_actions->hand.reset();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_actions->MoveMouse({5, 5});
	Frame();
	EXPECT_FALSE(_model.GetHandCues().dragging);
	_actions->MoveMouse({-5, 5});
	Frame();
	EXPECT_EQ(Counted(hp::Event::Drag), 0.0f);
}

TEST_F(WorldCameraLandGrip, ZoomingOutTakesTheGripTooFarAhead)
{
	// Gripped sqrt(300^2 + 2970^2) = 2985 ahead: the land is dragged
	const glm::vec3 origin {2560.0f, 300.0f, 0.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2970.0f};
	Place(origin, focus);
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	ASSERT_TRUE(_model.GetHandCues().dragging);
	ExpectNear(_model.GetTargetOrigin(), origin);

	// The zoom out key for a frame, a notch without the wheel: 60 of zoom, by 0.0015 x 3 x 300 = 81 further, past 3000
	_actions->held |= static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	Frame();
	EXPECT_EQ(_model.GetHandCues().tricons, camera_drag::tricon::k_GivenUp);
	ExpectNear(_model.GetTargetFocus(), focus);
	ExpectNear(_model.GetTargetOrigin(), GivenUpFlightOrigin(focus));
}

TEST(WorldCameraPan, TheSeaStopsTheCameraOnlyWithin7500OfTheDrawnCamera)
{
	// Only the sea: the line from the camera at the grip, 100 up, through its new origin, 100 down, meets it half way,
	// at x = 1050. The camera stops 3 / 100 of the move short of it: back by 0.53 of the move
	const LevelIsland sea(false);
	const glm::vec3 originAtGrip {1000.0f, 100.0f, 1000.0f};
	const camera_pan::CameraPlace place {.origin = {1100.0f, -100.0f, 1000.0f}, .focus = {1100.0f, -100.0f, 1500.0f}};
	const glm::vec3 stoppedBack = (place.origin - originAtGrip) * (1.0f - (0.5f - 0.03f));

	// The drawn camera exactly 7500 across from where the sea is met: it counts
	auto stopped = DefaultWorldCameraModel::StopPanShortOfLand(sea, place, originAtGrip, {1050.0f - 7500.0f, 100.0f, 1000.0f});
	ExpectNear(stopped.origin, place.origin - stoppedBack, 1e-3f);
	ExpectNear(stopped.focus, place.focus - stoppedBack, 1e-3f);

	// Any further and the sea is out of reach: the move is not cut, wherever the camera was at the grip
	stopped = DefaultWorldCameraModel::StopPanShortOfLand(sea, place, originAtGrip, {1050.0f - 7501.0f, 100.0f, 1000.0f});
	EXPECT_EQ(stopped.origin, place.origin);
	EXPECT_EQ(stopped.focus, place.focus);
}

TEST(WorldCameraPan, TheLandStopsTheCameraWhereverTheDrawnCameraIs)
{
	// Level land at 0: the line from 100 up to 100 down over 110 across meets it at x = 1055, half way. The stop is 3 /
	// 110 of the move, so the camera comes back by 1 - (55 - 3) / 110 of it, to x = 1110 - 58 and y = -100 + 105.45,
	// with the drawn camera 8000 away
	const LevelIsland land(true);
	const glm::vec3 originAtGrip {1000.0f, 100.0f, 1003.0f};
	const camera_pan::CameraPlace place {.origin = {1110.0f, -100.0f, 1003.0f}, .focus = {1110.0f, -100.0f, 1503.0f}};
	const auto stopped = DefaultWorldCameraModel::StopPanShortOfLand(land, place, originAtGrip, {9000.0f, 100.0f, 1003.0f});
	ExpectNear(stopped.origin, {1052.0f, -100.0f + 200.0f * (58.0f / 110.0f), 1003.0f}, 1e-2f);
	ExpectNear(stopped.focus, {1052.0f, -100.0f + 200.0f * (58.0f / 110.0f), 1503.0f}, 1e-2f);
}
