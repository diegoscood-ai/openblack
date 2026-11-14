/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "3D/OrientedText.h"
#include "3D/TempleChallengeRoom.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Camera/TempleCameraModel.h"
#include "Common/Zoomer.h"
#include "Locator.h"
#include "support/FakeGameActions.h"
#include "support/RestoreService.h"

#include "../oracle/OracleData.h"

using namespace openblack;

namespace
{
/// A .cam file of a path over a second along x, looking ahead of itself
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

/// A camera model that moves nothing, with or without a lens of its own
class LensModel final: public CameraModel
{
public:
	explicit LensModel(std::optional<Lens> lens)
	    : _lens(lens)
	{
	}
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return glm::vec3(0.0f); }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return glm::vec3(0.0f); }
	[[nodiscard]] std::optional<Lens> GetLens() const override { return _lens; }

private:
	std::optional<Lens> _lens;
};

/// The temple as far as its camera asks of it: the room the player is in, and its doors
class FakeTemple final: public TempleInteriorInterface
{
public:
	TempleRoom room {TempleRoom::Main};

	[[nodiscard]] bool Active() const override { return true; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return room; }
	void SetCurrentRoom(TempleRoom to) override { room = to; }
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return std::nullopt; }
	void SetTransitionRoom(std::optional<TempleRoom>) override {}
	void GoToRoom(TempleRoom) override {}
	void EnterRoom(TempleRoom) override {}
	void LookAtScroll(size_t) override {}
	[[nodiscard]] bool IsRoomDrawn(TempleRoom) const override { return false; }
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override { return std::nullopt; }
	void SetInterface(gui::GameInterface*) override {}
	bool HoldControl(bool, float) override { return false; }
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom) const override { return {}; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override { return nullptr; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _text; }
	[[nodiscard]] uint32_t GetVisits() const override { return 0; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _markers; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _trophies; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return 0.0f; }
	[[nodiscard]] float GetPoolTime() const override { return 0.0f; }
	void Escape() override {}
	void RequestLeave() override {}
	void FadeToWhite() override {}
	void Update(std::chrono::microseconds) override {}
	void ProcessTurn() override {}
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override { return {}; }
	void Activate() override {}
	void Activate(TempleRoom) override {}
	void Deactivate() override {}

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};
} // namespace

TEST(CameraSphere, ACameraModelWithALensOfItsOwnStaysOffTheWorldDisc)
{
	// The temple sits at the origin, about 3620 from the world's centre (2560, 0, 2560), beyond the 3500 the open world's
	// camera is kept within. The world's own model has no lens of its own
	Camera camera;
	EXPECT_FALSE(camera.GetModel().GetLens().has_value());
	const glm::vec3 centre(script_camera::k_DiscCentre, 0.0f, script_camera::k_DiscCentre);
	const glm::vec3 temple(0.0f, 10.0f, 0.0f);
	ASSERT_GT(glm::distance(temple, centre), 3500.0f);

	// with a lens of its own, as the temple's model, the destination stays where it is
	camera.SetModel(std::make_unique<LensModel>(CameraModel::Lens {.horizontalFieldOfView = 60.0f, .nearClip = 1.0f}));
	camera.GetOriginZoomer().SetPosition(temple);
	camera.UpdateZoomers(std::nullopt, 0.016f);
	EXPECT_EQ(camera.GetOriginZoomer().GetDestination(), temple);

	// without one, the same destination is pulled back onto the disc, to 1 / k_DiscScale from its centre
	camera.SetModel(std::make_unique<LensModel>(std::nullopt));
	camera.UpdateZoomers(std::nullopt, 0.016f);
	const auto pulled = camera.GetOriginZoomer().GetDestination();
	EXPECT_NEAR(glm::distance(pulled, centre), 1.0f / script_camera::k_DiscScale, 0.01f);
	EXPECT_NEAR(pulled.x, pulled.z, 1e-3f); // along the line from the centre to the temple
}

TEST(CameraPath, SamplesBetweenItsPointsAndHoldsTheLast)
{
	CameraPath path;
	ASSERT_TRUE(path.LoadFromBuffer(MakeCam(1000, 11)));
	// 10 spans of 100ms: halfway through the third is between the third and fourth points
	const auto middle = path.SampleAt(std::chrono::milliseconds(250));
	EXPECT_NEAR(middle.position.x, 25.0f, 1e-4f);
	EXPECT_NEAR(middle.focus.x, 26.0f, 1e-4f);
	EXPECT_FLOAT_EQ(path.SampleAt(std::chrono::milliseconds(0)).position.x, 0.0f);
	// Past its end, it stays just short of the last point, as the game's temple camera does
	EXPECT_NEAR(path.SampleAt(std::chrono::milliseconds(5000)).position.x, 99.9f, 1e-3f);
}

// A room of pictures' arrow keys: left and right turn by the frame's seconds; forwards and back tilt the lean, taken from
// -1 to 1, by 1.85 a second and move the camera against the tilt by 30 times it
TEST(TempleCamera, APictureRoomsKeysTurnLeanAndRaiseItsAims)
{
	using Aims = TempleCameraModel::PictureAims;
	const Aims start {.yaw = 0.0f, .lean = 0.6f, .height = 6.0f};
	auto aims = TempleCameraModel::PictureKeysStep(start, 0.1f, true, false, false, false);
	EXPECT_FLOAT_EQ(aims.yaw, 0.1f);
	EXPECT_FLOAT_EQ(aims.lean, 0.6f);
	aims = TempleCameraModel::PictureKeysStep(start, 0.1f, false, true, true, false);
	EXPECT_FLOAT_EQ(aims.yaw, -0.1f);
	EXPECT_NEAR(aims.lean, 0.5075f, 1e-6f);
	EXPECT_NEAR(aims.height, 5.955f, 1e-5f);
	aims = TempleCameraModel::PictureKeysStep(start, 0.1f, false, false, false, true);
	EXPECT_NEAR(aims.lean, 0.6925f, 1e-6f);
	EXPECT_NEAR(aims.height, 6.0f - (0.385f * 0.1f * 30.0f), 1e-5f);
}

// Dragging a room of pictures' walls turns from the turn reached by the angle from the press to the cursor, wrapped, and
// raises by the press's height less the cursor's
TEST(TempleCamera, DraggingAPictureRoomsWallsTurnsAndRaisesFromWhereItHasGot)
{
	using Aims = TempleCameraModel::PictureAims;
	const Aims start {.yaw = 5.0f, .lean = 0.4f, .height = 20.0f};
	auto aims = TempleCameraModel::PictureDragAims(start, 1.0f, 12.0f, 0.5f, 15.0f, 0.25f, 10.0f);
	EXPECT_FLOAT_EQ(aims.yaw, 1.25f);
	EXPECT_FLOAT_EQ(aims.height, 17.0f);
	EXPECT_FLOAT_EQ(aims.lean, 0.4f);
	// across the wrap: from just past -pi to just short of pi is a small turn back
	aims = TempleCameraModel::PictureDragAims(start, 0.0f, 0.0f, -3.0f, 0.0f, 3.0f, 0.0f);
	EXPECT_NEAR(aims.yaw, -6.0f + glm::two_pi<float>(), 1e-5f);
}

// Clicking a room of pictures' door walks back from halfway between the camera's aim and the door, at the door's height
TEST(TempleCamera, APictureRoomsWalkBackStartsHalfwayAtTheDoorsHeight)
{
	const TempleCameraModel::Pose aim {{0.0f, 30.0f, 10.0f}, {2.0f, 20.0f, 4.0f}};
	const TempleCameraModel::Pose through {{40.0f, 16.0f, 0.0f}, {20.0f, 10.0f, 0.0f}};
	const auto start = TempleCameraModel::WalkBackStart(aim, through);
	EXPECT_EQ(start.origin, glm::vec3(20.0f, 16.0f, 5.0f));
	EXPECT_EQ(start.focus, glm::vec3(11.0f, 15.0f, 2.0f));
}

TEST(CameraPath, RefusesCamFilesShortOfTheirPoints)
{
	// CameraPath reports the file it couldn't read to the game's log
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_st>("game");
	}
	CameraPath path;
	auto data = MakeCam(1000, 4);
	data.resize(data.size() - 4);
	EXPECT_FALSE(path.LoadFromBuffer(data));
}

TEST(TempleCamera, LooksAtTheMainRoomFromItsOrbit)
{
	// Unturned, it starts behind the pool looking over it at the scroll, leaning a little up
	const auto start = TempleCameraModel::OrbitPose(0.0f, 0.7f);
	EXPECT_NEAR(start.origin.y, 9.92f, 1e-4f);
	EXPECT_NEAR(start.origin.z, 18.32f, 1e-4f);
	EXPECT_NEAR(start.focus.y, 8.4f, 1e-4f);
	// A quarter turn takes it round to the -x side
	const auto turned = TempleCameraModel::OrbitPose(glm::half_pi<float>(), 0.0f);
	EXPECT_NEAR(turned.origin.x, -14.4f, 1e-4f);
	EXPECT_NEAR(turned.origin.z, 0.0f, 1e-4f);
}

// Given back from a scroll it had come all the way to, the main room's camera starts afresh; taken back on its way there,
// it keeps the turn and lean it had
TEST(TempleCamera, TheMainRoomStartsAfreshOnlyAfterALookThatCameAllTheWay)
{
	EXPECT_TRUE(TempleCameraModel::MainRoomStartsAfreshAfterLook(1.0f));
	EXPECT_FALSE(TempleCameraModel::MainRoomStartsAfreshAfterLook(0.999f));
	EXPECT_FALSE(TempleCameraModel::MainRoomStartsAfreshAfterLook(0.0f));
}

// The main room's look still steps in on the frame it is left, as the room's camera blends before it changes state;
// another room's steps out at once
TEST(TempleCamera, TheMainRoomsLookStillStepsInOnTheFrameItIsLeft)
{
	EXPECT_TRUE(TempleCameraModel::LookBlendStepsIn(true, true, false));
	EXPECT_TRUE(TempleCameraModel::LookBlendStepsIn(false, true, true));
	EXPECT_FALSE(TempleCameraModel::LookBlendStepsIn(false, true, false));
	EXPECT_FALSE(TempleCameraModel::LookBlendStepsIn(false, false, true));
}

// Dragging the main room's walls up or down leans by the pixels moved times the time and 0.06, less towards the sides
TEST(TempleCamera, DraggingTheWallsLeansBySixHundredthsAPixelASecond)
{
	EXPECT_EQ(TempleCameraModel::DragLean(10.0f, 0.5f, 0.0f), ((10.0f * 0.5f) * 0.06f) * 1.0f);
	EXPECT_EQ(TempleCameraModel::DragLean(10.0f, 0.5f, -0.5f), ((10.0f * 0.5f) * 0.06f) * 0.5f);
	EXPECT_EQ(TempleCameraModel::DragLean(-4.0f, 0.25f, 1.0f), 0.0f);
}

// Past 0.8 of the way to the top or bottom edge the main room leans away from it, its ramp rising by 0.2 a second;
// over the pool it leans down by 0.1, or by three tenths of a low lean, the ramp rising by 0.1; elsewhere nothing
TEST(TempleCamera, TheMainRoomLeansAwayFromTheScreensEdgesPastEightTenths)
{
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(0.9f, false, 0.7f).speed, (0.9f - 0.8f) * -0.8f);
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(0.9f, false, 0.7f).rampRate, 0.2f);
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(-0.85f, true, 0.7f).speed, (0.85f - 0.8f) * 0.8f);
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(0.8f, true, 0.7f).speed, -0.1f);
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(0.8f, true, 0.7f).rampRate, 0.1f);
	EXPECT_EQ(TempleCameraModel::EdgeLeanOf(0.0f, true, 0.2f).speed, 0.2f * -0.3f);
	EXPECT_FALSE(TempleCameraModel::EdgeLeanOf(0.5f, false, 0.7f).rampRate.has_value());
	// the lean: the speed times ((dt x 1.5) x (1 - cos(ramp x pi)))
	EXPECT_EQ(TempleCameraModel::LeanStep(-0.08f, 0.1f, 0.5f),
	          -0.08f * ((0.1f * 1.5f) * (1.0f - std::cos(0.5f * glm::pi<float>()))));
	EXPECT_EQ(TempleCameraModel::LeanStep(-0.08f, 0.1f, 0.0f), 0.0f);
}

// The main room's camera eases at once, and over a fifth of a second times the blend toward the pool's map
TEST(TempleCamera, TheMainRoomEasesAtOnceButTowardThePoolsMap)
{
	EXPECT_EQ(TempleCameraModel::MainRoomEaseTime(0.0f), 0.0f);
	EXPECT_EQ(TempleCameraModel::MainRoomEaseTime(0.5f), 0.5f * 0.2f);
	EXPECT_EQ(TempleCameraModel::MainRoomEaseTime(1.0f), 0.2f);
}

// Held on the pool's map, the camera looks at the pressed point from 6 away, turned by 6 per screen width moved and
// tilted by 2 per screen height, the tilt held between 0.628 and 1.178
TEST(TempleCamera, DraggingThePoolsMapTurnsAndTiltsAboutThePressedPoint)
{
	const glm::vec3 point(3.0f, 1.0f, -2.0f);
	const auto still = TempleCameraModel::MapLook(point, 0.0f, 0.8f, glm::vec2(0.0f));
	EXPECT_EQ(still.focus, point);
	EXPECT_NEAR(glm::length(still.origin - point), 6.0f, 1e-5f);
	EXPECT_NEAR(still.origin.y - point.y, 6.0f * std::sin(0.8f), 1e-5f);
	// a quarter of the width across turns by 1.5; far down the tilt stops at 1.1780972
	const auto turned = TempleCameraModel::MapLook(point, 0.0f, 0.8f, glm::vec2(0.25f, 1.0f));
	EXPECT_NEAR(turned.origin.y - point.y, 6.0f * std::sin(1.1780972f), 1e-5f);
	EXPECT_NEAR(std::atan2(turned.origin.z - point.z, turned.origin.x - point.x), -1.5f, 1e-5f);
}

// The main room's arrow keys turn by two radians a second and lean by 0.85 a second, the lean held to [0, 1.5]
TEST(TempleCamera, TheMainRoomsArrowKeysTurnAndLean)
{
	const auto left = TempleCameraModel::OrbitKeys({.yaw = 1.0f, .lean = 0.7f}, 0.5f, true, false, true, false);
	EXPECT_EQ(left.yaw, 1.0f + 0.5f * 2.0f);
	EXPECT_EQ(left.lean, 0.7f + 0.5f * 0.85f);
	const auto right = TempleCameraModel::OrbitKeys({.yaw = 1.0f, .lean = 0.2f}, 0.5f, false, true, false, true);
	EXPECT_EQ(right.yaw, 1.0f - 0.5f * 2.0f);
	EXPECT_EQ(right.lean, 0.0f);
	EXPECT_EQ(TempleCameraModel::OrbitKeys({.yaw = 0.0f, .lean = 1.4f}, 1.0f, false, false, true, false).lean, 1.5f);
}

// The creature room zooms in over a second and back in half that
TEST(TempleCamera, TheCreaturesRoomZoomsInOverASecondAndBackInHalf)
{
	EXPECT_EQ(TempleCameraModel::CaveZoomStep(0.0f, 1.0f, 0.25f), 0.25f);
	EXPECT_EQ(TempleCameraModel::CaveZoomStep(0.9f, 1.0f, 0.25f), 1.0f);
	EXPECT_EQ(TempleCameraModel::CaveZoomStep(1.0f, 0.0f, 0.25f), 0.5f);
	EXPECT_EQ(TempleCameraModel::CaveZoomStep(0.2f, 0.0f, 0.25f), 0.0f);
}

// The creature room's camera eases over 0.15 s, and a tenth of a second longer zoomed all the way
TEST(TempleCamera, TheCreaturesRoomEasesLongerTheMoreItIsZoomed)
{
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(0.0f), 0.15f);
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(0.5f), (0.5f * 0.1f) + 0.15f);
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(1.0f), 0.1f + 0.15f);
}

// The creature room's drag moves the look by the lens's width per screen width, on both axes, and its turns are
// brought back by whole turns
TEST(TempleCamera, TheCreaturesRoomDragsByTheLenssWidthAndWrapsItsTurns)
{
	const float across = 2.0f * std::tan(glm::radians(90.0f) * 0.5f);
	const auto look = TempleCameraModel::CaveDragLook({1.0f, 0.2f}, {400.0f, 300.0f}, {300.0f, 350.0f}, 800.0f);
	EXPECT_EQ(look.x, (across * (100.0f / 800.0f)) + 1.0f);
	EXPECT_EQ(look.y, (across * (-50.0f / 800.0f)) + 0.2f);
	EXPECT_EQ(TempleCameraModel::WrapTurn(-1.0f), -1.0f + glm::two_pi<float>());
	EXPECT_EQ(TempleCameraModel::WrapTurn(7.0f), 7.0f - glm::two_pi<float>());
	EXPECT_EQ(TempleCameraModel::WrapTurn(glm::two_pi<float>()), glm::two_pi<float>());
}

// The creature room's cursor keys speed up the look's turn and pitch, which the speeds carry on with and which slow to
// a stop once the keys are let go; the pitch is held short of straight up and down and the turn wrapped
TEST(TempleCamera, TheCreaturesRoomsKeysTurnAndTiltItsLookSlowingToAStop)
{
	using CaveLook = TempleCameraModel::CaveLook;
	constexpr float dt = 0.1f;
	const float slowing = std::exp(-10.0f * dt);
	// left: the turn's speed down by 0.3 dt, the look and the look at the press turned by it, the speed slowed
	auto step = TempleCameraModel::CaveKeysStep({.look = {1.0f, 0.2f}, .atPress = {1.5f, 0.1f}}, dt, true, false, false, false);
	EXPECT_EQ(step.look, glm::vec2(1.0f - (dt * 0.3f), 0.2f));
	EXPECT_EQ(step.atPress, glm::vec2(1.5f - (dt * 0.3f), 0.1f));
	EXPECT_EQ(step.speed, glm::vec2(-(dt * 0.3f) * slowing, 0.0f));
	// right and left together cancel; down adds to the pitch's speed, up takes from it
	step = TempleCameraModel::CaveKeysStep({.look = {1.0f, 0.0f}}, dt, true, true, false, true);
	EXPECT_EQ(step.speed, glm::vec2(0.0f, (dt * 0.3f) * slowing));
	step = TempleCameraModel::CaveKeysStep({.look = {1.0f, 0.0f}}, dt, false, false, true, false);
	EXPECT_EQ(step.look.y, -(dt * 0.3f));
	// both pitches are held within 1.4959966 of level, the speed kept
	step = TempleCameraModel::CaveKeysStep({.look = {1.0f, 1.49f}, .atPress = {1.0f, -1.49f}, .speed = {0.0f, 0.05f}}, dt,
	                                       false, false, false, false);
	EXPECT_EQ(step.look.y, 1.4959966f);
	EXPECT_EQ(step.atPress.y, -1.49f + 0.05f);
	EXPECT_EQ(step.speed.y, 0.05f * slowing);
	step = TempleCameraModel::CaveKeysStep({.look = {1.0f, -1.49f}, .speed = {0.0f, -0.05f}}, dt, false, false, false, false);
	EXPECT_EQ(step.look.y, -1.4959966f);
	// both turns are wrapped by whole turns
	step = TempleCameraModel::CaveKeysStep({.look = {0.01f, 0.0f}, .atPress = {6.28f, 0.0f}, .speed = {-0.02f, 0.0f}}, dt,
	                                       false, false, false, false);
	EXPECT_EQ(step.look.x, (0.01f - 0.02f) + glm::two_pi<float>());
	step = TempleCameraModel::CaveKeysStep({.look = {6.28f, 0.0f}, .speed = {0.02f, 0.0f}}, dt, false, false, false, false);
	EXPECT_EQ(step.look.x, (6.28f + 0.02f) - glm::two_pi<float>());
	// let go, the turn slows to a stop
	CaveLook look {.speed = {0.5f, 0.0f}};
	for (int frame = 0; frame < 60; ++frame)
	{
		look = TempleCameraModel::CaveKeysStep(look, 1.0f / 30.0f, false, false, false, false);
	}
	EXPECT_LT(std::abs(look.speed.x), 1e-4f);
}

// Walking through a door the camera heads through it once 1.5 s have gone, is through past 5 s, and a new press skips
TEST(TempleCamera, ADoorWalkHeadsThroughAfterOneAndAHalfSecondsAndIsThroughAfterFive)
{
	using Stage = TempleCameraModel::DoorStage;
	auto walk = TempleCameraModel::DoorWalkStep(Stage::Approaching, 1.5f, 0.5f, false);
	EXPECT_EQ(walk.stage, Stage::Approaching);
	EXPECT_FALSE(walk.headsThrough);
	walk = TempleCameraModel::DoorWalkStep(walk.stage, walk.time, 0.5f, false);
	EXPECT_EQ(walk.stage, Stage::Entering);
	EXPECT_TRUE(walk.headsThrough);
	EXPECT_EQ(TempleCameraModel::DoorWalkStep(Stage::Entering, 4.75f, 0.5f, false).stage, Stage::Through);
	EXPECT_EQ(TempleCameraModel::DoorWalkStep(Stage::Entering, 4.0f, 0.5f, false).stage, Stage::Entering);
	EXPECT_EQ(TempleCameraModel::DoorWalkStep(Stage::Approaching, 0.0f, 0.1f, true).stage, Stage::Skipped);
}

// The point the depth along the view gives on the cursor's line
TEST(TempleCamera, APointAtADepthAlongTheViewLiesOnTheCursorsLine)
{
	const auto point = TempleCameraModel::PointAtViewDepth({1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 10.0f);
	EXPECT_EQ(point, glm::vec3(1.0f, 12.0f, 13.0f));
}

// The room's cursor point: the mesh picked, else the point kept; the floor over either; the creature over all
TEST(TempleCamera, TheRoomsCursorPointIsThePickTheFloorOrTheCreature)
{
	const glm::vec3 kept(1.0f, 2.0f, 3.0f);
	const glm::vec3 mesh(4.0f, 5.0f, 6.0f);
	const glm::vec3 floor(7.0f, 0.0f, 8.0f);
	const glm::vec3 creature(9.0f, 9.0f, 9.0f);
	EXPECT_EQ(TempleCameraModel::NextRoomCursorPoint(kept, std::nullopt, std::nullopt, std::nullopt), kept);
	EXPECT_EQ(TempleCameraModel::NextRoomCursorPoint(kept, mesh, std::nullopt, std::nullopt), mesh);
	EXPECT_EQ(TempleCameraModel::NextRoomCursorPoint(kept, std::nullopt, floor, std::nullopt), floor);
	EXPECT_EQ(TempleCameraModel::NextRoomCursorPoint(kept, mesh, floor, std::nullopt), floor);
	EXPECT_EQ(TempleCameraModel::NextRoomCursorPoint(kept, mesh, floor, creature), creature);
}

// The creature's room walks back on a new press on its door, or on any press on the door it last walked back through
// (none at first) once the camera is within 25 of the room's cursor point; only near the door's side of the temple
TEST(TempleCamera, TheCreaturesRoomWalksBackOnAPressNearTheDoorsSide)
{
	EXPECT_TRUE(TempleCameraModel::CaveWalksBack(true, true, 0, -1, 78.0f, 60.0f));
	EXPECT_FALSE(TempleCameraModel::CaveWalksBack(false, true, 0, -1, 78.0f, 60.0f));
	// a held press on no door, as at first, with the camera near the point
	EXPECT_TRUE(TempleCameraModel::CaveWalksBack(false, true, -1, -1, 78.0f, 24.0f));
	EXPECT_FALSE(TempleCameraModel::CaveWalksBack(false, true, -1, -1, 78.0f, 25.0f));
	EXPECT_FALSE(TempleCameraModel::CaveWalksBack(false, true, -1, 0, 78.0f, 24.0f));
	// far from the door's side, or with no press, never
	EXPECT_FALSE(TempleCameraModel::CaveWalksBack(true, true, 0, -1, 79.0f, 10.0f));
	EXPECT_FALSE(TempleCameraModel::CaveWalksBack(false, false, -1, -1, 78.0f, 10.0f));
}

TEST(TempleCamera, FindsTheMainRoomsDoorsAroundItsWalls)
{
	// From the room's centre at door height, out along each door's eighth of the room, a little past its start
	for (uint32_t door = 0; door < 8; ++door)
	{
		const float angle = static_cast<float>(door) * glm::quarter_pi<float>() + 0.0f;
		const glm::vec3 direction {std::cos(angle), 0.0f, std::sin(angle)};
		const auto hit =
		    TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), glm::vec3(0.0f, 15.0f, 0.0f) + direction * 0.2f);
		ASSERT_TRUE(hit.has_value());
		EXPECT_FALSE(hit->floor);
		EXPECT_NEAR(hit->distance, TempleCameraModel::k_RoomRadius, 1e-3f);
		const auto found = TempleCameraModel::DoorAt(*hit);
		ASSERT_TRUE(found.has_value()) << "door " << door;
		EXPECT_EQ(*found, door);
	}
	// Between doors, and above them, there are none
	const glm::vec3 between {std::cos(0.4f), 0.0f, std::sin(0.4f)};
	EXPECT_FALSE(TempleCameraModel::DoorAt(
	    *TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), glm::vec3(0.0f, 15.0f, 0.0f) + between)));
	EXPECT_FALSE(
	    TempleCameraModel::DoorAt(*TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(1.0f, 30.0f, 0.0f))));
}

TEST(TempleCamera, HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms)
{
	const auto hit = TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 10.0f, 20.0f), glm::vec3(0.0f, 9.8f, 19.95f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_TRUE(hit->floor);
	EXPECT_NEAR(hit->point.z, 17.5f, 1e-3f);

	// The rooms off the main room, around it from +x
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(0), TempleRoom::CreatureCave);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(1), TempleRoom::Options);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(3), TempleRoom::Multi);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(4), TempleRoom::Credits);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(5), TempleRoom::SaveGame);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(7), TempleRoom::Challenge);
	// And the way out
	EXPECT_FALSE(TempleCameraModel::RoomBehindDoor(2).has_value());
	// Through a door, the camera heads out along its eighth
	const auto through = TempleCameraModel::TurnToDoor({{100.0f, 8.0f, 0.0f}, {120.0f, 10.0f, 0.0f}}, 2);
	EXPECT_NEAR(through.origin.x, 0.0f, 1e-3f);
	EXPECT_NEAR(through.origin.z, 100.0f, 1e-3f);
}

TEST(TempleCamera, TheMainRoomLightsTheDoorUnderTheCursorInEveryState)
{
	// The main room's camera finds the door under the cursor whatever it is doing
	EXPECT_EQ(TempleCameraModel::NextHighlightedDoor(TempleRoom::Main, 3u, 5u), 5u);
	EXPECT_EQ(TempleCameraModel::NextHighlightedDoor(TempleRoom::Main, 3u, std::nullopt), std::nullopt);
	// Another room's camera leaves the main room's sign as it was
	EXPECT_EQ(TempleCameraModel::NextHighlightedDoor(TempleRoom::CreatureCave, 3u, 5u), 3u);
	EXPECT_EQ(TempleCameraModel::NextHighlightedDoor(TempleRoom::Challenge, std::nullopt, 5u), std::nullopt);
}

// The temple's camera offers the hand none of the world camera's hints: no edge hints, no drag of the land, no helper
// icons
TEST(TempleCamera, OffersTheHandNoCameraHints)
{
	const TempleCameraModel model(TempleCameraModel::Paths {}, TempleRoom::Main);
	const CameraModel& camera = model;
	const auto cues = camera.GetHandCues();
	EXPECT_EQ(cues.tricons, 0u);
	EXPECT_FALSE(cues.dragging);
	EXPECT_FALSE(cues.dragMode.has_value());
	EXPECT_FALSE(cues.clearViewGrip);
	EXPECT_FALSE(cues.icons.has_value());
	EXPECT_EQ(cues.clearView, 0.0f);
}

// Coming into the temple, the room's camera starts on the start of its path and follows it, with nothing of where the
// camera outside was: the temple's fade from 1.2 covers the cut
TEST(TempleCamera, ComingIntoTheTempleCutsToTheStartOfTheRoomsPath)
{
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::gameActionSystem> restoreActions;
	Locator::temple::emplace<FakeTemple>();
	Locator::gameActionSystem::emplace<test::FakeGameActions>();
	auto path = std::make_shared<CameraPath>();
	ASSERT_TRUE(path->LoadFromBuffer(MakeCam(5000, 11)));
	TempleCameraModel::Paths paths {};
	paths.at(static_cast<size_t>(TempleRoom::Main)) = entt::resource<CameraPath>(path);

	TempleCameraModel model(paths, TempleRoom::Main);
	model.StartIntro(TempleRoom::Main, false);
	const auto start = path->SampleAt(std::chrono::milliseconds::zero());
	EXPECT_EQ(model.GetTargetOrigin(), start.position);
	EXPECT_EQ(model.GetTargetFocus(), start.focus);

	// The camera outside was far from the temple; each frame the room's camera aims at its path's point for the time
	// it has been in, pulled toward nothing else
	Camera outside;
	outside.SetOrigin(glm::vec3(2500.0f, 300.0f, 2500.0f));
	outside.SetFocus(glm::vec3(2600.0f, 0.0f, 2600.0f));
	float seconds = 0.0f;
	for (int frame = 0; frame < 10; ++frame)
	{
		std::ignore = model.Update(std::chrono::milliseconds(100), outside);
		seconds += 0.1f;
		const auto key = path->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(seconds * 1000.0f)));
		SCOPED_TRACE(frame);
		EXPECT_EQ(model.GetTargetOrigin(), key.position);
		EXPECT_EQ(model.GetTargetFocus(), key.focus);
	}
}

// Easing in from another room, the main room's camera starts its path at the tenth of a second, of the first three,
// whose point is nearest the camera; the tenths are counted up in floats
TEST(TempleCamera, TheMainRoomsCameraEasesInFromWhereItsPathPassesNearest)
{
	// Five seconds along x, 20 a second: x = 20 t
	CameraPath path;
	ASSERT_TRUE(path.LoadFromBuffer(MakeCam(5000, 11)));
	const auto tenths = [](int count) {
		float time = 0.0f;
		for (int i = 0; i < count; ++i)
		{
			time += 0.1f;
		}
		return time;
	};
	// 23.4 is nearer 1.2 s's 24 than 1.1 s's 22
	EXPECT_EQ(TempleCameraModel::NearestIntroTime(path, glm::vec3(23.4f, 5.0f, 0.0f)), tenths(12));
	// The start, and nothing past three seconds however far along the camera is: the thirtieth tenth, counted up in
	// floats, is still just under three
	EXPECT_EQ(TempleCameraModel::NearestIntroTime(path, glm::vec3(-50.0f, 5.0f, 0.0f)), 0.0f);
	EXPECT_EQ(TempleCameraModel::NearestIntroTime(path, glm::vec3(90.0f, 5.0f, 0.0f)), tenths(30));
	// Its height and depth count too: off the path to the side, the nearest along it is still kept
	EXPECT_EQ(TempleCameraModel::NearestIntroTime(path, glm::vec3(40.0f, 50.0f, -30.0f)), tenths(20));
}

// Coming back into the main room, its camera aims along its path from the point nearest where the camera was; another
// room's starts at its path's start
TEST(TempleCamera, ComingBackToTheMainRoomTakesUpItsPathNearestTheCamera)
{
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::gameActionSystem> restoreActions;
	Locator::temple::emplace<FakeTemple>();
	Locator::gameActionSystem::emplace<test::FakeGameActions>();
	auto path = std::make_shared<CameraPath>();
	ASSERT_TRUE(path->LoadFromBuffer(MakeCam(5000, 11)));
	TempleCameraModel::Paths paths {};
	paths.at(static_cast<size_t>(TempleRoom::Main)) = entt::resource<CameraPath>(path);
	paths.at(static_cast<size_t>(TempleRoom::Options)) = entt::resource<CameraPath>(path);
	const Camera camera;

	// The options room's camera, half a second into its path
	TempleCameraModel model(paths, TempleRoom::Options);
	model.StartIntro(TempleRoom::Options, false);
	for (int frame = 0; frame < 5; ++frame)
	{
		std::ignore = model.Update(std::chrono::milliseconds(100), camera);
	}
	const auto from = model.Update(std::chrono::milliseconds::zero(), camera)->origin;
	const float start = TempleCameraModel::NearestIntroTime(*path, from);
	EXPECT_GT(start, 0.0f);

	model.StartIntro(TempleRoom::Main, true);
	// Once the two seconds' ease from the previous camera are over, it aims at its path's point for the time from there
	float time = start;
	for (int frame = 0; frame < 21; ++frame)
	{
		std::ignore = model.Update(std::chrono::milliseconds(100), camera);
		time += 0.1f;
	}
	const auto key = path->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(time * 1000.0f)));
	EXPECT_EQ(model.GetTargetOrigin(), key.position);
	EXPECT_EQ(model.GetTargetFocus(), key.focus);
}

// Easing in from another room, each frame of the first two seconds the room's camera aims between its path's point and
// where the camera it came from was heading, toward the latter by (2 - t) / 2 of the blend's time t, held to 1, and
// eases onto that aim over the frame's half a second times what is left of it; past two seconds it aims at its path
TEST(TempleCamera, ComingThroughADoorTheCameraEasesOffTheWayItWasGoingOverTwoSeconds)
{
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::gameActionSystem> restoreActions;
	Locator::temple::emplace<FakeTemple>();
	Locator::gameActionSystem::emplace<test::FakeGameActions>();
	// The options room's path stands still at one place, ten seconds long; the main room's runs along x
	std::vector<uint8_t> still(3 * sizeof(uint32_t) + 2 * 6 * sizeof(float));
	const uint32_t header[3] {static_cast<uint32_t>(still.size()), 10000, 2};
	std::memcpy(still.data(), header, sizeof(header));
	const float point[6] {300.0f, 40.0f, -20.0f, 310.0f, 35.0f, -20.0f};
	std::memcpy(&still[sizeof(header)], point, sizeof(point));
	std::memcpy(&still[sizeof(header) + sizeof(point)], point, sizeof(point));
	auto options = std::make_shared<CameraPath>();
	ASSERT_TRUE(options->LoadFromBuffer(still));
	// (twenty seconds, so its last second, which eases into the player's control, is never reached)
	auto main = std::make_shared<CameraPath>();
	ASSERT_TRUE(main->LoadFromBuffer(MakeCam(20000, 11)));
	TempleCameraModel::Paths paths {};
	paths.at(static_cast<size_t>(TempleRoom::Main)) = entt::resource<CameraPath>(main);
	paths.at(static_cast<size_t>(TempleRoom::Options)) = entt::resource<CameraPath>(options);
	const Camera camera;

	// The options room's camera settles on its still path
	TempleCameraModel model(paths, TempleRoom::Options);
	model.StartIntro(TempleRoom::Options, false);
	for (int frame = 0; frame < 20; ++frame)
	{
		std::ignore = model.Update(std::chrono::milliseconds(100), camera);
	}
	const auto from = *model.Update(std::chrono::milliseconds::zero(), camera);
	ASSERT_EQ(from.origin, glm::vec3(300.0f, 40.0f, -20.0f));
	ASSERT_EQ(from.focus, glm::vec3(310.0f, 35.0f, -20.0f));

	model.StartIntro(TempleRoom::Main, true);
	float introTime = TempleCameraModel::NearestIntroTime(*main, from.origin);
	float blendTime = 0.0f;
	for (int frame = 0; frame < 25; ++frame)
	{
		std::ignore = model.Update(std::chrono::milliseconds(100), camera);
		introTime += 0.1f;
		blendTime += 0.1f;
		const auto key = main->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(introTime * 1000.0f)));
		const float fromPrevious = (2.0f - blendTime) * 0.5f;
		const float amount = fromPrevious > 0.0f ? std::min(fromPrevious, 1.0f) : 0.0f;
		SCOPED_TRACE(frame);
		EXPECT_EQ(model.GetTargetOrigin(), glm::mix(key.position, from.origin, amount));
		EXPECT_EQ(model.GetTargetFocus(), glm::mix(key.focus, from.focus, amount));
		EXPECT_EQ(model.GetEaseTime(), fromPrevious > 0.0f ? 0.5f * (1.0f - amount) : 0.5f);
	}
}

// Coming into a room, once its way in has run past 0.4 s the room's camera closes the doorway behind it at the rate
// 1.6, every frame, from as far open as it is
TEST(TempleCamera, ComingIntoARoomTheDoorwayClosesBehindTheCameraPastFourTenthsOfASecond)
{
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::gameActionSystem> restoreActions;
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	Locator::gameActionSystem::emplace<test::FakeGameActions>();
	auto path = std::make_shared<CameraPath>();
	ASSERT_TRUE(path->LoadFromBuffer(MakeCam(5000, 11)));
	TempleCameraModel::Paths paths {};
	paths.at(static_cast<size_t>(TempleRoom::Main)) = entt::resource<CameraPath>(path);
	paths.at(static_cast<size_t>(TempleRoom::Options)) = entt::resource<CameraPath>(path);
	const Camera camera;

	// The options room's doorway half open, standing there
	auto& doors = temple.GetDoors();
	doors.Open(TempleDoors::LeafOf(TempleRoom::Options), 0.5f, 0.0f);
	TempleCameraModel model(paths, TempleRoom::Main);
	model.StartIntro(TempleRoom::Options, true);

	// 0.3 s in, the doorway stands as it was
	std::ignore = model.Update(std::chrono::milliseconds(300), camera);
	EXPECT_EQ(doors.GetSwing(), 0.5f);
	// 0.6 s in, it is turned round to close at 1.6 a second, and swings on by the frame
	std::ignore = model.Update(std::chrono::milliseconds(300), camera);
	EXPECT_EQ(doors.GetSwing(), (2.0f - 0.5f) + 1.6f * 0.3f);
	ASSERT_TRUE(doors.GetLeaf().has_value());
	// Asked again the next frame, it goes on closing and is shut
	std::ignore = model.Update(std::chrono::milliseconds(300), camera);
	EXPECT_FALSE(doors.GetLeaf().has_value());
	EXPECT_EQ(doors.GetSwing(), 0.0f);
}

TEST(TempleCamera, GoingToTheRoomThePlayerIsInDoesNothing)
{
	// Not even shut the main room's doorway
	EXPECT_FALSE(TempleCameraModel::ChangesRoom(TempleRoom::Main, TempleRoom::Main));
	EXPECT_FALSE(TempleCameraModel::ChangesRoom(TempleRoom::CreatureCave, TempleRoom::CreatureCave));
	EXPECT_TRUE(TempleCameraModel::ChangesRoom(TempleRoom::Main, TempleRoom::Challenge));
}

TEST(TempleCamera, ClickingADoorOnScreenFindsIt)
{
	// The main room as the player takes it over, looking at the scroll through the temple camera's lens
	constexpr glm::vec2 k_Screen {1280.0f, 720.0f};
	const auto pose = TempleCameraModel::OrbitPose(0.0f, 0.7f);
	Camera camera;
	camera.SetProjectionMatrixPerspective(90.0f, k_Screen.x / k_Screen.y, 0.2f, 1000.0f);
	camera.SetOrigin(pose.origin);
	camera.SetFocus(pose.focus);

	// The middle of door 7 at door height, where the mouse is, cast back into the room as the camera's model does
	const float doorAngle = -glm::quarter_pi<float>();
	const glm::vec3 door {TempleCameraModel::k_RoomRadius * std::cos(doorAngle), 15.0f,
	                      TempleCameraModel::k_RoomRadius * std::sin(doorAngle)};
	glm::vec3 screen;
	ASSERT_TRUE(camera.ProjectWorldToScreen(door, glm::vec4(0.0f, 0.0f, k_Screen), screen));
	const auto rayDirection = camera.RayFromEye(glm::vec2(screen) / k_Screen).direction;
	const auto direction = rayDirection * (0.2f / glm::dot(rayDirection, camera.GetForward()));
	const auto hit = TempleCameraModel::RayCastRoom(camera.GetOrigin(), camera.GetOrigin() + direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->height, 15.0f, 0.05f);
	EXPECT_EQ(TempleCameraModel::DoorAt(*hit), 7u);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(7), TempleRoom::Challenge);
}

// The doors back to the main room leave 0.13 clear at each side of their eighth, the main room's own doors 0.1
TEST(TempleCamera, TheDoorsBackLeaveThirteenHundredthsClearAtTheirSides)
{
	// 0.115 into the eighth of door 0, between the two margins
	const TempleCameraModel::RoomHit hit {.distance = 45.0f,
	                                      .angle = 0.115f - 6.4581857f + 8.0f * glm::quarter_pi<float>(),
	                                      .height = 15.0f,
	                                      .floor = false,
	                                      .point = glm::vec3(0.0f),
	                                      .normal = glm::vec3(0.0f)};
	EXPECT_EQ(TempleCameraModel::DoorAt(hit), 0u);
	EXPECT_FALSE(TempleCameraModel::DoorAt(hit, false).has_value());
	auto middle = hit;
	middle.angle = 0.175f - 6.4581857f + 8.0f * glm::quarter_pi<float>();
	EXPECT_EQ(TempleCameraModel::DoorAt(middle, false), 0u);
}

TEST(TempleCamera, PictureRoomsStartLookingAlongTheEndOfTheirPaths)
{
	// A path ending at the options room's centre, looking out along -z
	const TempleCameraModel::Pose end {{87.0f, 10.0f, 87.0f}, {87.0f, 10.0f, 77.0f}};
	const float yaw = TempleCameraModel::PictureStartYaw(end);
	const auto pose = TempleCameraModel::PictureOrbitPose(TempleRoom::Options, yaw, 0.6f, 9.0f, end.origin);
	const auto look = glm::normalize(pose.focus - pose.origin);
	EXPECT_NEAR(look.x, 0.0f, 1e-4f);
	EXPECT_LT(look.z, -0.9f);
	// It stands back from the room's centre, the other way, raised by its height
	EXPECT_NEAR(pose.origin.x, 87.0f, 1e-3f);
	EXPECT_NEAR(pose.origin.z, 107.0f, 1e-3f);
	EXPECT_NEAR(pose.origin.y, 1.4f + 9.0f, 1e-3f);
}

// The library turns about where its path ends, set down on the floor, and not about its room's centre; the other rooms
// of pictures turn about their own centres wherever their paths end
TEST(TempleCamera, TheLibraryTurnsAboutItsPathsEndOnTheFloor)
{
	const glm::vec3 pathEnd {-110.0f, 12.0f, 5.0f};
	const float yaw = 0.3f;
	const auto middle = [&](TempleRoom room) {
		// Half a turn apart, the camera stands on opposite sides of the point it turns about, at the same height
		const auto a = TempleCameraModel::PictureOrbitPose(room, yaw, 0.6f, 6.0f, pathEnd);
		const auto b = TempleCameraModel::PictureOrbitPose(room, yaw + glm::pi<float>(), 0.6f, 6.0f, pathEnd);
		EXPECT_NEAR(a.origin.y, b.origin.y, 1e-4f);
		return std::pair {(a.origin + b.origin) * 0.5f, a.origin.y};
	};
	const auto [library, height] = middle(TempleRoom::Credits);
	EXPECT_NEAR(library.x, -110.0f, 1e-3f);
	EXPECT_NEAR(library.z, 5.0f, 1e-3f);
	// The path end's height is left out: the camera's height is its lean's (0.1 x 0.6 - 0.1 x 0.4) and its own 6
	EXPECT_NEAR(height, 0.02f + 6.0f, 1e-4f);
	const auto [options, optionsHeight] = middle(TempleRoom::Options);
	EXPECT_NEAR(options.x, 87.0f, 1e-3f);
	EXPECT_NEAR(options.z, 87.0f, 1e-3f);
	EXPECT_NEAR(optionsHeight, 1.4f + 6.0f, 1e-4f);
}

TEST(TempleCamera, TheCreaturesRoomLooksOnFromTheEndOfItsPath)
{
	const TempleCameraModel::Pose end {{170.0f, -8.0f, 5.0f}, {213.0f, 4.0f, 19.0f}};
	const auto look = TempleCameraModel::HeadingAndPitch(end);
	const auto pose = TempleCameraModel::LookPose(end.origin, look.x, look.y);
	EXPECT_EQ(pose.origin, end.origin);
	const auto along = glm::normalize(pose.focus - pose.origin);
	const auto expected = glm::normalize(end.focus - end.origin);
	EXPECT_NEAR(glm::dot(along, expected), 1.0f, 1e-5f);
	EXPECT_NEAR(glm::length(pose.focus - pose.origin), 20.0f, 1e-3f);
}

TEST(TempleCamera, CastsAgainstEachRoomsOwnWalls)
{
	// From the challenge room's centre, out along +x, to its walls 37 away
	const auto room = TempleCameraModel::CylinderOf(TempleRoom::Challenge);
	const auto hit = TempleCameraModel::RayCastRoom(room.centre + glm::vec3(0.0f, 10.0f, 0.0f),
	                                                room.centre + glm::vec3(1.0f, 10.0f, 0.0f), room);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->distance, 37.0f, 1e-3f);
	EXPECT_NEAR(hit->point.x, room.centre.x + 37.0f, 1e-3f);
	EXPECT_NEAR(hit->height, 10.0f, 1e-3f);
}

// The original's cast of the cursor against a room, run by the oracle on rooms, eyes and near points of every kind: each
// hit, its distance, angle, height, point and normal, bit for bit
TEST(TempleCamera, TheCursorsCastAgainstARoomIsTheOriginalsBitForBit)
{
	const auto table = openblack::oracle::Table::Load("temple_ray.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&table](const char* name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	const auto vec = [&](size_t row, const char* x, const char* y, const char* z) {
		return glm::vec3(table->Float(row, column(x)), table->Float(row, column(y)), table->Float(row, column(z)));
	};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const TempleCameraModel::Cylinder room {.centre = vec(row, "cx", "cy", "cz"),
		                                        .radius = table->Float(row, column("radius")),
		                                        .floor = table->Bits(row, column("floor")) != 0};
		const auto hit = TempleCameraModel::RayCastRoom(vec(row, "ex", "ey", "ez"), vec(row, "px", "py", "pz"), room);
		bool same = hit.has_value() == (table->Bits(row, column("hit")) != 0);
		if (same && hit.has_value())
		{
			const std::array<std::pair<const char*, float>, 9> outs {{
			    {"distance", hit->distance},
			    {"angle", hit->angle},
			    {"height", hit->height},
			    {"hx", hit->point.x},
			    {"hy", hit->point.y},
			    {"hz", hit->point.z},
			    {"nx", hit->normal.x},
			    {"ny", hit->normal.y},
			    {"nz", hit->normal.z},
			}};
			for (const auto& [name, ours] : outs)
			{
				same = same && std::bit_cast<uint32_t>(ours) == table->Bits(row, column(name));
			}
			same = same && hit->floor == (table->Bits(row, column("onfloor")) != 0);
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row;
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(TempleCamera, APictureRoomTurnsByItselfUnderADialogSpeedingUpOverASecond)
{
	float time = 0.0f;
	// No dialog and no words: the arrow keys turn the room, and the time stays
	EXPECT_FALSE(TempleCameraModel::PictureTurnByItself(time, 0.5f, false, false, false).has_value());
	EXPECT_FLOAT_EQ(time, 0.0f);
	// The first frame of the turn has no speed, then the turn speeds up over a second to a quarter turn a second
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.0f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.5f * 0.25f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.25f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.25f);
}

TEST(TempleCamera, TheLibraryKeepsStillUnderADialog)
{
	float time = 2.0f;
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, true, false), 0.0f);
	EXPECT_FLOAT_EQ(time, 2.5f);
}

TEST(TempleCamera, TheFutureRoomTurnsByItselfWhileItShowsItsWords)
{
	float time = 0.0f;
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.25f, false, false, true), 0.0f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.25f, false, false, true), 0.25f * 0.25f * 0.25f);
	EXPECT_FLOAT_EQ(time, 0.5f);
}

TEST(TempleCamera, APressLeavesTheCreatureZoomedTo)
{
	// A press zooms the creature's room back from every target, but stays on the creature, so that the tattoo editor its
	// zoom opened is still open and still closed by the camera
	using CreatureCaveTargets::Target;
	EXPECT_EQ(TempleCameraModel::ZoomedToAfterPress(Target::Creature), Target::Creature);
	EXPECT_EQ(TempleCameraModel::ZoomedToAfterPress(Target::Belts), std::nullopt);
	EXPECT_EQ(TempleCameraModel::ZoomedToAfterPress(Target::Medals), std::nullopt);
	EXPECT_EQ(TempleCameraModel::ZoomedToAfterPress(Target::Exit), std::nullopt);
	EXPECT_EQ(TempleCameraModel::ZoomedToAfterPress(std::nullopt), std::nullopt);
}

namespace
{
/// A blend toward a close look, and the aim at it: the origin's x, y and z, then the focus's, as their bits
struct CloseLookAim
{
	uint32_t blend;
	std::array<uint32_t, 6> aim;
};

[[nodiscard]] std::array<uint32_t, 6> AimBits(const TempleCameraModel::Pose& pose)
{
	return {std::bit_cast<uint32_t>(pose.origin.x), std::bit_cast<uint32_t>(pose.origin.y),
	        std::bit_cast<uint32_t>(pose.origin.z), std::bit_cast<uint32_t>(pose.focus.x),
	        std::bit_cast<uint32_t>(pose.focus.y),  std::bit_cast<uint32_t>(pose.focus.z)};
}

const TempleCameraModel::Pose k_RoomView {{10.0f, 12.5f, -30.0f}, {0.0f, 1.0f, 0.0f}};
const TempleCameraModel::Pose k_CloseLook {{3.0f, 20.25f, 7.0f}, {1.5f, 18.0f, 33.0f}};
const TempleCameraModel::Pose k_OtherRoomView {{-40.0f, 9.0f, 16.0f}, {-2.0f, 3.0f, 4.0f}};
const TempleCameraModel::Pose k_OtherCloseLook {{-12.75f, 14.0f, 41.5f}, {-12.75f, 14.0f, 16.5f}};
} // namespace

TEST(TempleCamera, APictureRoomsCameraAimsAlongHalfACosineTowardACloseLook)
{
	// The aims the original's room of pictures' camera gives at these blends, from these views of the room toward these
	// close looks
	constexpr std::array<CloseLookAim, 8> k_Aims {{
	    {0x3C449BA6, {0x411FF5D0, 0x41480B47, 0xC1EFE514, 0x3A0BB205, 0x3F80C5E7, 0x3C4014C7}},
	    {0x3DCCCCCD, {0x411D4259, 0x414B08D5, 0xC1E8C1A1, 0x3D165AB9, 0x3FB54022, 0x3F4EBCBF}},
	    {0x3E800000, {0x410F9915, 0x415A28CD, 0xC1C4A6DC, 0x3E60F127, 0x405F557C, 0x409AA5CB}},
	    {0x3EAAA64C, {0x4104014D, 0x4166FE8F, 0xC1A60370, 0x3EBFF715, 0x40A7F9AF, 0x4103F9DE}},
	    {0x3F19999A, {0x40AD63DC, 0x418C945C, 0xC0B90FD4, 0x3F7B54D0, 0x414206BE, 0x41ACCA4F}},
	    {0x3F400000, {0x4080CDD5, 0x4198EB9A, 0x3FCA6DD0, 0x3FA3E1DC, 0x41782AA2, 0x41E1568E}},
	    {0x3F666666, {0x404AF69E, 0x41A07B96, 0x40C30680, 0x3FBB4D2A, 0x418CABFE, 0x4200C50D}},
	    {0x3F7D70A4, {0x40401C4C, 0x41A1FC16, 0x40DFB538, 0x3FBFF3E0, 0x418FF769, 0x4203F7AA}},
	}};
	for (const auto& row : k_Aims)
	{
		const float blend = std::bit_cast<float>(row.blend);
		EXPECT_EQ(AimBits(TempleCameraModel::TowardCloseLook(k_RoomView, k_CloseLook, blend)), row.aim) << blend;
	}
	constexpr std::array<CloseLookAim, 4> k_OtherAims {{
	    {0x3C449BA6, {0xC21FF616, 0x41100747, 0x4180128E, 0xC0003E92, 0x40404007, 0x40802461}},
	    {0x3E800000, {0xC210098E, 0x411BB73A, 0x419DE007, 0xC064C15A, 0x40938C99, 0x40BA9422}},
	    {0x3F19999A, {0xC1B1512F, 0x41445C56, 0x4202C287, 0xC1109352, 0x4123318A, 0x4142E6D7}},
	    {0x3F7D70A4, {0xC14C1B8A, 0x415FFAF2, 0x4225F98F, 0xC14BF523, 0x415FF4E2, 0x4183F9AF}},
	}};
	for (const auto& row : k_OtherAims)
	{
		const float blend = std::bit_cast<float>(row.blend);
		EXPECT_EQ(AimBits(TempleCameraModel::TowardCloseLook(k_OtherRoomView, k_OtherCloseLook, blend)), row.aim) << blend;
	}
}

TEST(TempleCamera, ACloseLooksAimIsTheRoomsViewWithNoBlendAndTheLookPastAFullOne)
{
	EXPECT_EQ(TempleCameraModel::CloseLookAlong(0.0f), 0.0f);
	EXPECT_EQ(TempleCameraModel::CloseLookAlong(0.5f), 0.5f);
	EXPECT_EQ(TempleCameraModel::CloseLookAlong(1.0f), 1.0f);
	for (const float none : {0.0f, -0.1f, std::numeric_limits<float>::quiet_NaN()})
	{
		EXPECT_EQ(AimBits(TempleCameraModel::TowardCloseLook(k_RoomView, k_CloseLook, none)), AimBits(k_RoomView));
	}
	for (const float full : {1.0f, 1.2f})
	{
		EXPECT_EQ(AimBits(TempleCameraModel::TowardCloseLook(k_RoomView, k_CloseLook, full)), AimBits(k_CloseLook));
	}
	// Half a second to ease onto the aim at the full blend, none once back with the room
	EXPECT_EQ(TempleCameraModel::PictureEaseTime(1.0f), 0.5f);
	EXPECT_EQ(TempleCameraModel::PictureEaseTime(0.5f), 0.25f);
	EXPECT_EQ(TempleCameraModel::PictureEaseTime(0.0f), 0.0f);
}

TEST(TempleCamera, APictureRoomsCameraEasesToACloseLookAndBackAsTheOriginals)
{
	// The camera's x at 30 frames a second, every fifth frame, as the original's room of pictures' camera gives it:
	// 45 frames looking, then 50 back with the room
	constexpr float k_Seconds = 1.0f / 30.0f;
	constexpr std::array<uint32_t, 9> k_Looking {0x411FC3B4, 0x411CE342, 0x4114809A, 0x410618DF, 0x40E67707,
	                                             0x40BD44CE, 0x4096DDAD, 0x40724400, 0x405049C2};
	constexpr std::array<uint32_t, 10> k_Back {0x40453134, 0x4052E9FB, 0x40804991, 0x40A65D3F, 0x40D59AF0,
	                                           0x41025DC0, 0x41150719, 0x411E6F99, 0x41200000, 0x41200000};
	Zoomer x;
	x.SetPosition(k_RoomView.origin.x);
	float blend = 0.0f;
	for (size_t frame = 1; frame <= 45; ++frame)
	{
		// Looking, the camera is given half a second to ease onto its aim at the blend it had, which then rises
		const auto aim = TempleCameraModel::TowardCloseLook(k_RoomView, k_CloseLook, blend);
		x.SetDestinationWithSpeedAndTime(aim.origin.x, 0.0f, 0.5f);
		x.Update(k_Seconds);
		blend = temple_challenge_room::BlendStep(blend, true, k_Seconds);
		if (frame % 5 == 0)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(x.value), k_Looking.at(frame / 5 - 1)) << frame;
		}
	}
	EXPECT_EQ(blend, 1.0f);
	for (size_t frame = 1; frame <= 50; ++frame)
	{
		// Back with the room, the blend falls first, and the camera eases onto its aim at it the quicker the lower it is
		blend = temple_challenge_room::BlendStep(blend, false, k_Seconds);
		const auto aim = TempleCameraModel::TowardCloseLook(k_RoomView, k_CloseLook, blend);
		x.SetDestinationWithSpeedAndTime(aim.origin.x, 0.0f, TempleCameraModel::PictureEaseTime(blend));
		x.Update(k_Seconds);
		if (frame % 5 == 0)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(x.value), k_Back.at(frame / 5 - 1)) << frame;
		}
	}
	// Once the blend is gone, the camera is on the room's view
	EXPECT_EQ(blend, 0.0f);
	EXPECT_EQ(x.value, k_RoomView.origin.x);
}

TEST(TempleCamera, UnderTheCinemaBarsTheRoomsTakeNoPressAndOneHeldThroughThemWaitsForItsRelease)
{
	bool waits = false;
	// a press outside the bars is a press
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 1, false), 1);
	// under the bars none, a double click neither
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 1, true), 0);
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 2, true), 0);
	// held on after them: still none until let go
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 1, false), 0);
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 0, false), 0);
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 1, false), 1);
	// let go under the bars: the next press after them is one
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 0, true), 0);
	EXPECT_EQ(TempleCameraModel::PressUnderCinemaBars(waits, 1, false), 1);
}

TEST(TempleCamera, ADoubleClickOnTheMapAsksToLeaveForThePressesPointOnce)
{
	constexpr glm::vec3 k_Focus {3.0f, -1.0f, 4.0f};
	EXPECT_EQ(TempleCameraModel::MapDoubleClickLeave(2, false, k_Focus), std::optional(k_Focus));
	// a plain press, or a double click once the temple has taken a leave, asks for nothing
	EXPECT_FALSE(TempleCameraModel::MapDoubleClickLeave(1, false, k_Focus).has_value());
	EXPECT_FALSE(TempleCameraModel::MapDoubleClickLeave(2, true, k_Focus).has_value());
}

TEST(TempleCamera, TheDoorwayFollowsTheWalkThroughADoorAsItStoodAtTheFramesStart)
{
	using Stage = TempleCameraModel::DoorStage;
	using Kind = TempleCameraModel::DoorwayCall::Kind;
	// Not walking through a door, the walk asks nothing of the doorway
	EXPECT_FALSE(TempleCameraModel::DoorwayAtFrameStart(false, Stage::Entering, TempleRoom::Main, TempleRoom::Options));
	// Heading to the door, it is shut at once
	const auto approaching =
	    TempleCameraModel::DoorwayAtFrameStart(true, Stage::Approaching, TempleRoom::Main, TempleRoom::Options);
	ASSERT_TRUE(approaching.has_value());
	EXPECT_EQ(approaching->kind, Kind::FastClose);
	// Heading through from the main room, the doorway ahead waits half a swing, then opens at 0.4 a second
	const auto ahead = TempleCameraModel::DoorwayAtFrameStart(true, Stage::Entering, TempleRoom::Main, TempleRoom::Options);
	ASSERT_TRUE(ahead.has_value());
	EXPECT_EQ(ahead->kind, Kind::Open);
	EXPECT_EQ(ahead->leaf, TempleDoors::LeafOf(TempleRoom::Options));
	EXPECT_EQ(ahead->from, -0.5f);
	EXPECT_EQ(ahead->rate, 0.4f);
	// The way out swings with no leaf of its own
	const auto out = TempleCameraModel::DoorwayAtFrameStart(true, Stage::Entering, TempleRoom::Main, std::nullopt);
	ASSERT_TRUE(out.has_value());
	EXPECT_EQ(out->leaf, std::optional<uint32_t>(TempleDoors::k_ExitLeaf));
	// Back from another room, the main room's doorway to it opens at once, at 2 a second
	const auto back = TempleCameraModel::DoorwayAtFrameStart(true, Stage::Entering, TempleRoom::Challenge, TempleRoom::Main);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->leaf, TempleDoors::LeafOf(TempleRoom::Challenge));
	EXPECT_EQ(back->from, 0.0f);
	EXPECT_EQ(back->rate, 2.0f);
	// Through, or skipped, nothing
	EXPECT_FALSE(TempleCameraModel::DoorwayAtFrameStart(true, Stage::Through, TempleRoom::Main, TempleRoom::Options));
	EXPECT_FALSE(TempleCameraModel::DoorwayAtFrameStart(true, Stage::Skipped, TempleRoom::Main, TempleRoom::Options));
}

// Looking at a scroll in the main room, a press elsewhere (held from its press to its release) or an arrow key sends the
// camera back; with neither, it keeps looking
TEST(TempleCamera, APressOrAnArrowKeyTakesTheCameraBackFromAScroll)
{
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::gameActionSystem> restoreActions;
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	auto& actions = static_cast<test::FakeGameActions&>(Locator::gameActionSystem::emplace<test::FakeGameActions>());
	auto path = std::make_shared<CameraPath>();
	ASSERT_TRUE(path->LoadFromBuffer(MakeCam(5000, 11)));
	TempleCameraModel::Paths paths {};
	paths.at(static_cast<size_t>(TempleRoom::Main)) = entt::resource<CameraPath>(path);
	paths.at(static_cast<size_t>(TempleRoom::Options)) = entt::resource<CameraPath>(path);
	const Camera camera;

	constexpr std::array k_Leaves {input::BindableActionMap::MOVE_LEFT,     input::BindableActionMap::MOVE_RIGHT,
	                               input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS,
	                               input::BindableActionMap::ACTION,        input::BindableActionMap::MOVE};
	for (const auto leave : k_Leaves)
	{
		SCOPED_TRACE(static_cast<uint64_t>(leave));
		actions.held = 0;
		// From the options room straight to the main room, which the temple then stands in
		temple.room = TempleRoom::Options;
		TempleCameraModel model(paths, TempleRoom::Options);
		model.GoToRoom(TempleRoom::Main);
		temple.room = TempleRoom::Main;
		ASSERT_TRUE(model.IsInControl());
		model.LookAtSubMesh(glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f, 5.0f, 0.0f));
		for (int frame = 0; frame < 3; ++frame)
		{
			std::ignore = model.Update(std::chrono::milliseconds(100), camera);
		}
		EXPECT_TRUE(model.IsLookingAtSubMesh());
		actions.held = static_cast<uint64_t>(leave);
		std::ignore = model.Update(std::chrono::milliseconds(100), camera);
		EXPECT_FALSE(model.IsLookingAtSubMesh());
	}
}
