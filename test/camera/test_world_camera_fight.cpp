/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world camera watching a creature fight: the flight to the arena, the orbit round the fighters, the time it eases
// the camera in and its tilt's bounds, each way the watch ends, how far from an arena the camera gives up, the double
// click's flight taken away and no tilt hint from the screen's edge. Worked out from fake controls, a fake level land and
// two fake fighters with radii 2 and 3

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <LNDFile.h>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/LandLine.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Camera/CameraDrag.h"
#include "Camera/CameraFlight.h"
#include "Camera/CameraHelp.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightOrbit.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandLandPoint.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/PickingSystemInterface.h"
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
namespace fo = openblack::fight_orbit;

namespace
{
constexpr glm::ivec2 k_Screen {800, 600};
constexpr auto k_Frame = 33ms;
constexpr glm::uvec2 k_Middle {400, 300};
/// Half way across, near the bottom: past 0.49 of the view's height down, where the hints offer a tilt
constexpr glm::uvec2 k_Bottom {400, 598};

void ExpectNear(glm::vec3 actual, glm::vec3 expected, float epsilon = 1e-2f)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
	EXPECT_NEAR(actual.z, expected.z, epsilon);
}

/// p + d (sin h cos q, sin q, cos h cos q)
glm::vec3 Around(glm::vec3 point, float distance, float heading, float pitch)
{
	return point +
	       distance * glm::vec3(std::sin(heading) * std::cos(pitch), std::sin(pitch), std::cos(heading) * std::cos(pitch));
}

/// Level land at 0
class LevelIsland final: public LandIslandInterface
{
public:
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { return _cell; }
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2&) const final { return true; }
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
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _cell {};
	std::vector<lnd::LNDCountry> _countries;
};

/// Every line of sight going down meets the land at y = 0
class LevelPickingSystem final: public ecs::systems::PickingSystemInterface
{
public:
	// The line test the camera's pan stop reads: the cell walk over the island in the locator, as the game's picking
	[[nodiscard]] std::optional<glm::vec3> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const final
	{
		const auto& island = Locator::terrainSystem::value();
		const auto hit = land_line::LandOrSeaAlong(from, to, camera, land_line::CellsOf(island), island.GetCellsPerSide());
		return hit.has_value() ? std::optional(glm::vec3(hit->x, island.GetHeightAt(*hit), hit->y)) : std::nullopt;
	}
	// The picking the camera doesn't use yet, on the same level land
	[[nodiscard]] std::optional<glm::vec3> LandAlong(glm::vec3 from, glm::vec3 to) const final
	{
		return Level(from, to - from);
	}
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool) const final
	{
		return Level(camera, nearPoint - camera);
	}
	// The level land under a pixel of the test's camera, along the line from its eye through the pixel's near point,
	// moved on as the game's island ray moves it
	[[nodiscard]] std::optional<glm::vec3> LandFrom2DPoint(glm::ivec2 pixel, bool shift, float depth) const final
	{
		const auto eye = camera->GetEye();
		const float nearClip = camera->GetPixelNearClip();
		const auto nearPoint = camera->PixelNearPoint(eye, k_Screen, pixel, nearClip);
		const auto line = shift ? ecs::hand_land_point::ShiftedAlongView(eye, nearPoint, depth, nearClip)
		                        : ecs::hand_land_point::Line {.from = eye, .to = nearPoint};
		return Level(line.from, line.to - line.from);
	}
	void PickUnderCursor(const Frame&) final {}
	[[nodiscard]] const Pick& GetPick() const final { return _pick; }

	/// The test's camera
	const Camera* camera {nullptr};
	[[nodiscard]] std::optional<screen_pick::MeshHit> FeelModel(entt::entity, glm::vec3, glm::vec3) const final
	{
		return std::nullopt;
	}

private:
	[[nodiscard]] static std::optional<glm::vec3> Level(glm::vec3 origin, glm::vec3 direction)
	{
		if (!(direction.y < 0.0f))
		{
			return std::nullopt;
		}
		return origin + direction * (-origin.y / direction.y);
	}
	Pick _pick;
};

class FakeWindowing final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const final { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const final { return {}; }
	[[nodiscard]] uint32_t GetID() const final { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const final { return k_Screen; }
	[[nodiscard]] float GetAspectRatio() const final { return static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y); }
	[[nodiscard]] bool IsMinimised() const final { return false; }
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
	[[nodiscard]] glm::ivec2 GetMouseDelta() const final { return {0, 0}; }
	[[nodiscard]] float GetMouseWheelDelta() const final { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final { return {hand, std::nullopt}; }
	void Frame() final {}
	void ProcessEvent(const SDL_Event&) final {}

	uint64_t held {0};
	uint8_t heldUnbindable {0};
	glm::uvec2 mouse {k_Middle};
	std::optional<glm::vec3> hand;
};

class WorldCameraFight: public ::testing::Test
{
protected:
	/// Two fighters 20 apart along x, the arena between them
	static constexpr glm::vec3 k_FighterA {2500.0f, 0.0f, 2500.0f};
	static constexpr glm::vec3 k_FighterB {2520.0f, 0.0f, 2500.0f};
	static constexpr glm::vec3 k_ArenaCentre {2510.0f, 0.0f, 2500.0f};
	static constexpr float k_ArenaRadius = 10.0f;
	/// The fighters' radii, as the camera is given them
	static constexpr float k_RadiusA = 2.0f;
	static constexpr float k_RadiusB = 3.0f;

	void SetUp() override
	{
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		Locator::windowing::emplace<FakeWindowing>();
		Locator::terrainSystem::emplace<LevelIsland>();
		static_cast<LevelPickingSystem&>(Locator::pickingSystem::emplace<LevelPickingSystem>()).camera = &_camera;
		_help = &Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>().Get();
		Locator::cinematicDirectorSystem::reset();
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_fighterA = Fighter(k_FighterA);
		_fighterB = Fighter(k_FighterB);
		_camera.SetProjectionMatrixPerspective(70.0f, static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y), 1.0f,
		                                       65536.0f);
		hp::Reset();
		hp::SetQueries({.paused = [] { return false; }, .scriptWideScreen = [] { return false; }});
		// The camera's mode has just begun, and the scripts let the player leave a fight
		script_camera::Reset();
		player_camera::Reset();
	}
	void TearDown() override
	{
		player_camera::Reset();
		script_camera::Reset();
		hp::SetQueries({});
		hp::Reset();
	}

	entt::entity Fighter(glm::vec3 position)
	{
		const auto entity = _registry->Create();
		_registry->Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	/// The camera at origin, looking at focus on the land; the hand on the land at the focus
	void Place(glm::vec3 origin, glm::vec3 focus)
	{
		_camera.SetOrigin(origin);
		_camera.SetFocus(focus);
		_actions->hand = focus;
	}

	void Watch() { _model.StartFight(_fighterA, _fighterB, k_ArenaCentre, k_ArenaRadius); }

	/// A frame of the game: the controls, then the camera's update (the camera itself stays where it was put), then a
	/// turn of the help profile
	void Frame()
	{
		_model.HandleActions(k_Frame);
		std::ignore = _model.Update(k_Frame, _camera);
		hp::Process();
	}
	void Frames(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Frame();
		}
	}
	/// A frame in which the camera follows the model: what the update gives the camera's zoomers, which then move
	std::optional<CameraModel::CameraInterpolationUpdateInfo> FlownFrame()
	{
		_model.HandleActions(k_Frame);
		auto info = _model.Update(k_Frame, _camera);
		_camera.UpdateZoomers(info, std::chrono::duration<float>(k_Frame).count());
		hp::Process();
		return info;
	}
	/// Frames in which the camera follows the model until the flight to the arena has set its last leg, in 1.5 s
	void FlyToTheArena()
	{
		for (int i = 0; i < 60; ++i)
		{
			if (const auto info = FlownFrame(); info.has_value() && info->duration == 1'500'000us)
			{
				return;
			}
		}
		ADD_FAILURE() << "the flight to the arena never set its last leg";
	}

	/// The hints the cursor's place offers, over no land
	static uint32_t IdleHints(glm::uvec2 cursor)
	{
		return camera_drag::IdleTricons(
		    camera_drag::NormalisedCursor(glm::ivec2(cursor), k_Screen, camera_drag::ViewHeight(k_Screen, false)), false);
	}

	Camera _camera;
	/// The second fighter's radius, which a test may take away
	float _radiusB {k_RadiusB};
	DefaultWorldCameraModel _model {
	    [this](entt::entity fighter) { return fighter == _fighterA ? k_RadiusA : (fighter == _fighterB ? _radiusB : 0.0f); }};
	FakeActions* _actions {nullptr};
	camera_help::CameraHelp* _help {nullptr};
	ecs::Registry* _registry {nullptr};
	entt::entity _fighterA {entt::null};
	entt::entity _fighterB {entt::null};

private:
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::pickingSystem> _restoreDynamics;
	test::RestoreService<Locator::cameraHelpSystem> _restoreHelp;
	test::RestoreService<Locator::cinematicDirectorSystem> _restoreCinematics;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

// The maths

TEST(FightOrbit, TheCameraIsTooFarFromTheArenaPastItsRadii)
{
	const glm::vec3 arena {0.0f, 0.0f, 0.0f};
	// Its focus past 3.2 radii and itself past 4.2
	EXPECT_TRUE(fo::WantToQuit(arena, 10.0f, {50.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 0.0f}, 1.0f));
	// Its focus near: only past 6 radii
	EXPECT_FALSE(fo::WantToQuit(arena, 10.0f, {50.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, 1.0f));
	EXPECT_TRUE(fo::WantToQuit(arena, 10.0f, {0.0f, 0.0f, 61.0f}, {5.0f, 0.0f, 0.0f}, 1.0f));
	// Itself within 4.2 radii, its focus far
	EXPECT_FALSE(fo::WantToQuit(arena, 10.0f, {41.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 1.0f));
	// The heights don't count
	EXPECT_FALSE(fo::WantToQuit(arena, 10.0f, {41.0f, 5000.0f, 0.0f}, {5.0f, -300.0f, 0.0f}, 1.0f));
	// At three quarters of the radius, 46 is past 6 x 7.5
	EXPECT_FALSE(fo::WantToQuit(arena, 10.0f, {46.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, 1.0f));
	EXPECT_TRUE(fo::WantToQuit(arena, 10.0f, {46.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, fo::k_DragAwayShare));
}

TEST(FightOrbit, TheArenaLingeredOverTakesInWhereTheCameraIsAndWhereItLooks)
{
	// the first arena, in the list's order, whose radius takes in both points across the land, with both fighters
	const std::array arenas {
	    fo::ArenaInView {.centre = {0.0f, 0.0f}, .radius = 50.0f, .bothFighters = false},
	    fo::ArenaInView {.centre = {10.0f, 0.0f}, .radius = 50.0f, .bothFighters = true},
	    fo::ArenaInView {.centre = {0.0f, 0.0f}, .radius = 50.0f, .bothFighters = true},
	};
	EXPECT_EQ(fo::ArenaUnderView(arenas, {20.0f, 20.0f}, {0.0f, 10.0f}), std::optional<size_t>(1));
	// the camera outside the radius, or on its edge (strictly inside only): none
	EXPECT_EQ(fo::ArenaUnderView(arenas, {80.0f, 0.0f}, {0.0f, 10.0f}), std::nullopt);
	EXPECT_EQ(fo::ArenaUnderView(std::span(arenas).subspan(2), {50.0f, 0.0f}, {0.0f, 0.0f}), std::nullopt);
	EXPECT_EQ(fo::k_ArenaLingerMs, 1000);
}

TEST(FightOrbit, TheArenaDoubleClickedOntoIsTheFirstWhoseRadiusReachesThePoint)
{
	// whoever is in it; the radius strictly beyond the distance across the land
	const std::array arenas {
	    fo::ArenaInView {.centre = {100.0f, 0.0f}, .radius = 20.0f, .bothFighters = true},
	    fo::ArenaInView {.centre = {0.0f, 0.0f}, .radius = 50.0f, .bothFighters = false},
	    fo::ArenaInView {.centre = {10.0f, 0.0f}, .radius = 50.0f, .bothFighters = true},
	};
	EXPECT_EQ(fo::ArenaAtPoint(arenas, {-30.0f, 40.0f}), std::nullopt); // on the second's edge, beyond the third
	EXPECT_EQ(fo::ArenaAtPoint(arenas, {20.0f, 30.0f}), std::optional<size_t>(1));
	EXPECT_EQ(fo::ArenaAtPoint(arenas, {110.0f, 0.0f}), std::optional<size_t>(0));
	EXPECT_EQ(fo::ArenaAtPoint(std::span(arenas).subspan(0, 1), {0.0f, 0.0f}), std::nullopt);
}

TEST(FightOrbit, WrapAngleKeepsWithinPiAndWrapsTheRest)
{
	EXPECT_EQ(fo::WrapAngle(1.0f), 1.0f);
	EXPECT_EQ(fo::WrapAngle(-3.0f), -3.0f);
	EXPECT_NEAR(fo::WrapAngle(4.0f), 4.0f - 2.0f * glm::pi<float>(), 1e-5f);
	EXPECT_NEAR(fo::WrapAngle(-4.0f), 2.0f * glm::pi<float>() - 4.0f, 1e-5f);
	// 10 is a turn and 3.7168 more, past pi
	EXPECT_NEAR(fo::WrapAngle(10.0f), 10.0f - 4.0f * glm::pi<float>(), 1e-4f);
}

TEST(FightOrbit, TheTurnFollowsTheFightersTheShortWayRound)
{
	// From 3 to -3 the short way is past pi, by 2 pi - 6
	EXPECT_NEAR(fo::TurnTowards(3.0f, -3.0f), 3.0f + 2.0f * glm::pi<float>() - 6.0f, 1e-5f);
	EXPECT_NEAR(fo::TurnTowards(0.5f, 1.0f), 1.0f, 1e-6f);
	// The line from the first fighter to the second: 0 towards -z, a quarter turn towards +x
	EXPECT_NEAR(fo::FightersHeading({0.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, 2.0f), glm::half_pi<float>(), 1e-6f);
	EXPECT_NEAR(fo::FightersHeading({0.0f, 0.0f, 0.0f}, {0.0f, 3.0f, -20.0f}, 2.0f), 0.0f, 1e-6f);
	// Within a hundredth of each other the turn stays
	EXPECT_EQ(fo::FightersHeading({0.0f, 0.0f, 0.0f}, {0.005f, 4.0f, -0.005f}, 2.0f), 2.0f);
}

TEST(FightOrbit, TheDistanceCountsTheSecondFightersRadii)
{
	// 10 apart, radii 2 and 4: 10 + 4 x 5.5 + 2 = 34
	EXPECT_FLOAT_EQ(fo::OrbitDistance(5.5f, 10.0f, 2.0f, 4.0f), 34.0f);
	// 20 of zoom moves the camera 6 further: 1.5 more of the second fighter's radii
	EXPECT_FLOAT_EQ(fo::Zoom(5.5f, 20.0f, 10.0f, 2.0f, 4.0f), 7.0f);
	EXPECT_FLOAT_EQ(fo::Zoom(5.5f, -20.0f, 10.0f, 2.0f, 4.0f), 4.0f);
	EXPECT_EQ(fo::ClampDistance(50.0f), fo::k_MaxDistance);
	EXPECT_EQ(fo::ClampDistance(-1.0f), 0.0f);
	EXPECT_EQ(fo::ClampDistance(20.0f), 20.0f);
	EXPECT_EQ(fo::ClampPitch(0.1f), fo::k_MinPitch);
	EXPECT_EQ(fo::ClampPitch(2.0f), fo::k_MaxPitch);
	EXPECT_EQ(fo::ClampPitch(0.5f), 0.5f);
	// Not a number and infinities, as the original's comparisons take them
	EXPECT_EQ(fo::ClampDistance(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_EQ(fo::ClampDistance(std::numeric_limits<float>::infinity()), fo::k_MaxDistance);
	EXPECT_EQ(fo::ClampDistance(-std::numeric_limits<float>::infinity()), 0.0f);
	EXPECT_EQ(fo::ClampPitch(std::numeric_limits<float>::quiet_NaN()), fo::k_MinPitch);
	EXPECT_EQ(fo::Zoom(5.5f, 20.0f, 10.0f, 2.0f, 0.0f), std::numeric_limits<float>::infinity());
	EXPECT_EQ(fo::Zoom(5.5f, -20.0f, 10.0f, 2.0f, 0.0f), -std::numeric_limits<float>::infinity());
	EXPECT_EQ(fo::Middle({0.0f, 2.0f, 10.0f}, {20.0f, 4.0f, -10.0f}), glm::vec3(10.0f, 3.0f, 0.0f));
}

TEST(FightOrbit, TheOrbitEasesInOverFiveSecondsAfterAModeChangeAndTwoOnceTheModeIsSettled)
{
	EXPECT_EQ(fo::EaseSeconds(0.0f), 5.0f);
	EXPECT_EQ(fo::EaseSeconds(0.5f), 4.25f);
	EXPECT_EQ(fo::EaseSeconds(1.0f), 3.5f);
	// At 2 itself the mode is not yet past its 2 seconds: 2.5 x (2 - 1.2), within a rounding of 2
	EXPECT_FLOAT_EQ(fo::EaseSeconds(2.0f), 2.0f);
	EXPECT_EQ(fo::EaseSeconds(2.0001f), 2.0f);
	EXPECT_EQ(fo::EaseSeconds(30.0f), 2.0f);
}

TEST(FightOrbit, WatchingTakesOnlyTheDoubleClicksFlightAway)
{
	EXPECT_EQ(camera_help::DuringFight(camera_help::k_NormalFeatures), camera_help::k_NormalFeatures & ~0x10);
	EXPECT_EQ(camera_help::DuringFight(0x10), 0);
	EXPECT_EQ(camera_help::DuringFight(0x6F), 0x6F);
}

// The world camera

TEST_F(WorldCameraFight, OnlyThePlayersCameraWatchesFights)
{
	EXPECT_EQ(_model.GetFightWatch(), &_model);
	EXPECT_FALSE(_model.IsWatchingFight());
	Watch();
	EXPECT_TRUE(_model.IsWatchingFight());
	// Not without both fighters
	_model.EndFightNow();
	_model.StartFight(_fighterA, entt::null, k_ArenaCentre, k_ArenaRadius);
	EXPECT_FALSE(_model.IsWatchingFight());
}

// Watching a fight in an arena, the player can go on drawing a gesture as the camera turns round the fighters; not
// before the watch, nor once it has ended
TEST_F(WorldCameraFight, ThePlayerCanGestureWhileTheCameraWatchesAFight)
{
	EXPECT_FALSE(_model.LetsPlayerGestureWhileMoving());
	Watch();
	EXPECT_TRUE(_model.LetsPlayerGestureWhileMoving());
	_model.EndFightNow();
	EXPECT_FALSE(_model.LetsPlayerGestureWhileMoving());
}

TEST_F(WorldCameraFight, ItTurnsRoundTheFightersMiddle)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	// A quarter turn from their line, which runs along x: the camera looks along -z, (20 + 3 x 5.5) + 2 = 38.5 out (their
	// spacing, 5.5 of the second fighter's radii and the first one's radius)
	ExpectNear(_model.GetTargetFocus(), k_ArenaCentre);
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_StartPitch));

	// Tilted up for a while, it stops at its lowest tilt
	_actions->held = static_cast<uint64_t>(BindableActionMap::TILT_UP);
	Frames(10);
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_MinPitch));
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, TheWatchFliesToTheArenaFirst)
{
	const glm::vec3 origin {2510.0f, 300.0f, 2000.0f};
	Place(origin, {2510.0f, 0.0f, 2500.0f});
	Frame();
	Watch();
	// The view of the arena: from its centre as a map position holds it, on the land, to its rim half a radius up; the
	// flight's middle point with no rise
	const LevelIsland level;
	const glm::vec3 centre {map_coords::Quantise(k_ArenaCentre.x), 0.0f, map_coords::Quantise(k_ArenaCentre.z)};
	const auto view = camera_flight::ViewOfPoint(level, centre, camera_flight::ArenaLookPoint(centre, k_ArenaRadius));
	const auto via = CameraModel::CharterFlight(level, view.origin, view.focus, origin, 0.0f).midpoint;
	ASSERT_TRUE(via.has_value());

	// The first leg, to the middle point in 1.35 s
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	ExpectNear(info->origin, *via, 1e-3f);
	ExpectNear(info->focus, view.focus, 1e-3f);
	EXPECT_NEAR(static_cast<double>(info->duration.count()), 1'350'000.0, 1.0);
	// Then nothing reaches the camera, the orbit's place neither, until the zoomers are past 0.75 s: the last leg, to
	// the view in 1.5 s
	int frames = 0;
	for (info = FlownFrame(); !info.has_value() && frames < 60; info = FlownFrame())
	{
		++frames;
	}
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(frames, 22);
	ExpectNear(info->origin, view.origin, 1e-3f);
	ExpectNear(info->focus, view.focus, 1e-3f);
	EXPECT_EQ(info->duration, 1'500'000us);
	// From the next frame on the orbit's place replaces it, eased into over 5 s as the camera's mode is new
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->origin, _model.GetTargetOrigin());
	EXPECT_EQ(info->focus, _model.GetTargetFocus());
	ExpectNear(info->focus, k_ArenaCentre, 1.0f);
	EXPECT_EQ(info->duration, 5'000'000us);
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, TheOrbitEasesInOverTwoSecondsOnceTheModeIsSettledAndFiveAfterAModeChange)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Frame();
	Watch();
	FlyToTheArena();
	// The camera's mode has run more than 2 s
	script_camera::Get().modeSeconds = 2.5f;
	auto info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->duration, 2'000'000us);
	// A second into it, 3.5 s
	script_camera::Get().modeSeconds = 1.0f;
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->duration, 3'500'000us);
	// A script's camera taken and given back is a change of mode: 5 s again. Its end makes the player's camera again,
	// which lets the watch go, and the fight's side watches the fight again
	script_camera::Get().modeSeconds = 2.5f;
	ASSERT_TRUE(script_camera::Begin({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f}));
	ASSERT_TRUE(script_camera::End());
	_model.Restart();
	EXPECT_FALSE(_model.IsWatchingFight());
	Watch();
	FlyToTheArena();
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->duration, 5'000'000us);
	EXPECT_TRUE(_model.IsWatchingFight());
	// The watch over, the camera takes its usual time again
	_model.EndFightNow();
	info = FlownFrame();
	ASSERT_TRUE(info.has_value());
	EXPECT_LE(info->duration, 1'500'000us);
}

TEST_F(WorldCameraFight, NotWhileTheLandAloneIsDragged)
{
	const glm::vec3 origin {2510.0f, 300.0f, 2000.0f};
	Place(origin, {2510.0f, 0.0f, 2500.0f});
	Watch();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frame();
	ExpectNear(_model.GetTargetOrigin(), origin);
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, AMovementKeyDuringAWatchTiltsRoundTheFightAndTheOrbitGoesOn)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_StartPitch));
	// Backwards, nothing gripped: the watch keeps the turn modifier on, so the key tilts as the tilt up key does and is
	// no move over the land; the orbit keeps the camera, which stops at its lowest tilt
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE_BACKWARDS);
	Frames(10);
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_MinPitch));
	EXPECT_TRUE(_model.IsWatchingFight());
	// Let go, the orbit stays where the key left it
	_actions->held = 0;
	Frame();
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_MinPitch));
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, AWatchStartingStartsTheCamerasEasingAgain)
{
	// The camera has run past its first 1.5 s, so it eases at its shortest
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Frames(50);
	Watch();
	Frame();
	// The land gripped drops the flight to the arena before its last leg, and the land alone dragged leaves the orbit
	// out: the camera takes the model's own time, which starts again with the watch: two frames in, still near the
	// 1.5 s a new camera starts at, not its shortest
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	_model.HandleActions(k_Frame);
	const auto info = _model.Update(k_Frame, _camera);
	ASSERT_TRUE(info.has_value());
	EXPECT_TRUE(_model.IsWatchingFight());
	EXPECT_GT(info->duration, 1'400'000us);
	EXPECT_LE(info->duration, 1'500'000us);
}

TEST_F(WorldCameraFight, WithoutTheLandGrabAMovementKeyStillTiltsRoundTheFight)
{
	// The turn modifier's tilt does not need the land grab, which only the move over the land does
	_help->Enable(0, camera_help::Bit(camera_help::Feature::GrabLand));
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE_BACKWARDS);
	Frames(10);
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_MinPitch));
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, ZoomingOutPast40EndsTheWatch)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	// The zoom key is a notch a frame: 60 x 0.0015 x the zoom scale, 3 x the camera's 300 above its focus, = 81 of zoom,
	// which moves the camera 0.3 x 81 = 24.3, 8.1 of the second fighter's radius 3. From 5.5: 13.6, 21.7, 29.8 and 37.9
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	Frames(4);
	EXPECT_TRUE(_model.IsWatchingFight());
	// The fifth takes it to 46, past 40
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, ZoomingOutPast40KeepsTheWatchWhileTheScriptsKeepThePlayerInTheFight)
{
	player_camera::Get().fightExit = false;
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	// The fifth notch would take it to 46: it stays at the furthest, 40, and the watch goes on
	Frames(8);
	EXPECT_TRUE(_model.IsWatchingFight());
	// Let out again, the next notch past the furthest ends it
	player_camera::Get().fightExit = true;
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, ZoomingInStopsAtNoDistance)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	// 8.1 nearer from 5.5 is below 0: the camera is the fighters' spacing and the first one's radius out, 22
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_IN);
	Frame();
	EXPECT_TRUE(_model.IsWatchingFight());
	_actions->held = 0;
	Frame();
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 22.0f, 0.0f, fo::k_StartPitch));
}

TEST_F(WorldCameraFight, ASecondFighterWithNoRadiusIsWatchedAtTheSpacingAndTheFirstOnesRadius)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	_radiusB = 0.0f;
	Watch();
	Frames(3);
	// (20 + 0 x 5.5) + 2
	EXPECT_TRUE(_model.IsWatchingFight());
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 22.0f, 0.0f, fo::k_StartPitch));
}

TEST_F(WorldCameraFight, ASecondFighterWithNoRadiusEndsTheWatchOnAZoomOut)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	_radiusB = 0.0f;
	Watch();
	Frame();
	// 24.3 / 0 is +infinity, past 40, on the very frame
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_OUT);
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, ASecondFighterWithNoRadiusStaysWatchedOnAZoomIn)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	_radiusB = 0.0f;
	Watch();
	Frame();
	// -24.3 / 0 is -infinity, clamped to 0: the camera stays 20 + 2 out
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_IN);
	Frame();
	EXPECT_TRUE(_model.IsWatchingFight());
	_actions->held = 0;
	Frame();
	EXPECT_TRUE(_model.IsWatchingFight());
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 22.0f, 0.0f, fo::k_StartPitch));
}

TEST_F(WorldCameraFight, ARestartedCameraLetsTheWatchGoAndAWatchAfterStartsAfresh)
{
	const glm::vec3 origin {2510.0f, 300.0f, 2000.0f};
	Place(origin, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	_actions->held = static_cast<uint64_t>(BindableActionMap::TILT_UP);
	Frames(10);
	_actions->held = 0;
	ASSERT_TRUE(_model.IsWatchingFight());

	_model.Restart();
	EXPECT_FALSE(_model.IsWatchingFight());
	EXPECT_EQ(_model.GetWatchedFighter(), std::nullopt);
	// Watched again, the orbit starts as a first watch does: a quarter turn round, tilted down, not where the tilt had
	// taken the last one
	Place(origin, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	ExpectNear(_model.GetTargetFocus(), k_ArenaCentre);
	ExpectNear(_model.GetTargetOrigin(), Around(k_ArenaCentre, 38.5f, 0.0f, fo::k_StartPitch));
}

TEST_F(WorldCameraFight, EndFightNowEndsAtOnce)
{
	Watch();
	_model.EndFightNow();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, EndFightLingersThreeSecondsOnlyOnce)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	_model.EndFight();
	// 33 ms a frame: 60 frames in, told again, which changes nothing
	Frames(60);
	EXPECT_TRUE(_model.IsWatchingFight());
	_model.EndFight();
	// After 90 frames 30 ms are left; the 91st runs them out
	Frames(30);
	EXPECT_TRUE(_model.IsWatchingFight());
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
	// Over: told again, it stays over
	_model.EndFight();
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, AFighterGoneEndsTheWatchAfterTheLinger)
{
	Place({2510.0f, 300.0f, 2000.0f}, {2510.0f, 0.0f, 2500.0f});
	Watch();
	Frame();
	_registry->Destroy(_fighterB);
	Frames(91);
	EXPECT_TRUE(_model.IsWatchingFight());
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, DraggingAwayEndsTheWatchFourAndAHalfSecondsAfterTheLastFlight)
{
	// Far from the arena, the land gripped and held: no orbit, and 137 frames of 33 ms come to more than 4.5 s
	Place({2560.0f, 300.0f, 1000.0f}, {2560.0f, 0.0f, 1500.0f});
	Watch();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frames(137);
	EXPECT_TRUE(_model.IsWatchingFight());
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, DraggingAwayKeepsTheWatchWhileTheScriptsKeepThePlayerInTheFight)
{
	player_camera::Get().fightExit = false;
	Place({2560.0f, 300.0f, 1000.0f}, {2560.0f, 0.0f, 1500.0f});
	Watch();
	_actions->held = static_cast<uint64_t>(BindableActionMap::MOVE);
	Frames(150);
	EXPECT_TRUE(_model.IsWatchingFight());
	player_camera::Get().fightExit = true;
	Frame();
	EXPECT_FALSE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, WithoutAGripTheWatchGoesOn)
{
	Place({2560.0f, 300.0f, 1000.0f}, {2560.0f, 0.0f, 1500.0f});
	Watch();
	Frames(150);
	EXPECT_TRUE(_model.IsWatchingFight());
}

TEST_F(WorldCameraFight, TheCameraIsTooFarFromWhereItIsAndLooks)
{
	// No land at the screen's middle yet: never too far
	EXPECT_FALSE(_model.WantToQuitFight({0.0f, 0.0f, 0.0f}, 1.0f));
	// 500 across from its focus on the land
	Place({2560.0f, 300.0f, 2000.0f}, {2560.0f, 0.0f, 2500.0f});
	Frame();
	// An arena at the focus: the camera is 500 away, within 6 x 100 but not 6 x 50
	EXPECT_FALSE(_model.WantToQuitFight({2560.0f, 0.0f, 2500.0f}, 100.0f));
	EXPECT_TRUE(_model.WantToQuitFight({2560.0f, 0.0f, 2500.0f}, 50.0f));
	// One under the camera, its focus 500 away: within 4.2 x 50 of the camera
	EXPECT_FALSE(_model.WantToQuitFight({2560.0f, 0.0f, 2000.0f}, 50.0f));
}

TEST_F(WorldCameraFight, NoDoubleClickFlightWhileWatching)
{
	Place({2560.0f, 300.0f, 2000.0f}, {2560.0f, 0.0f, 2500.0f});
	Frame();
	_actions->heldUnbindable = static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK);
	Watch();
	Frame();
	EXPECT_EQ(hp::TotalEvents(static_cast<int32_t>(hp::Event::DoubleClickPos)).value(), 0.0f);
	_model.EndFightNow();
	Frame();
	EXPECT_EQ(hp::TotalEvents(static_cast<int32_t>(hp::Event::DoubleClickPos)).value(), 1.0f);
}

TEST_F(WorldCameraFight, AWatchedFightOffersTheTiltAndTurnIconsAsTheTurnKeyDoes)
{
	const auto keyHints = [this] { return _model.GetHandCues().icons.value_or(hand_tricons::WorldCameraFrame {}).keyHints; };
	_model.HandleActions(k_Frame);
	EXPECT_EQ(keyHints(), 0U);
	// No key held: the watched fight keeps the turn modifier on, which offers tilting and turning
	Watch();
	_model.HandleActions(k_Frame);
	EXPECT_EQ(keyHints(), camera_drag::tricon::k_Pitch | camera_drag::tricon::k_Rotate);
	// The zoom key wins: zooming and turning
	_actions->held = static_cast<uint64_t>(BindableActionMap::ZOOM_ON);
	_model.HandleActions(k_Frame);
	EXPECT_EQ(keyHints(), camera_drag::tricon::k_Zoom | camera_drag::tricon::k_Rotate);
	_actions->held = 0;
	_model.EndFightNow();
	_model.HandleActions(k_Frame);
	EXPECT_EQ(keyHints(), 0U);
}

TEST_F(WorldCameraFight, NoTiltHintFromTheScreensEdgeWhileWatching)
{
	const auto hints = IdleHints(k_Bottom);
	ASSERT_NE(hints & camera_drag::tricon::k_Pitch, 0U);
	_actions->mouse = k_Bottom;
	Watch();
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, hints & ~camera_drag::tricon::k_Pitch);
	_model.EndFightNow();
	_model.HandleActions(k_Frame);
	EXPECT_EQ(_model.GetHandCues().tricons, hints);
}
