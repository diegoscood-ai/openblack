/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "3D/TempleChallengeRoom.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Camera/TempleCameraModel.h"
#include "Common/Zoomer.h"

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

TEST(TempleCamera, FindsTheMainRoomsDoorsAroundItsWalls)
{
	// From the room's centre at door height, out along each door's eighth of the room, a little past its start
	for (uint32_t door = 0; door < 8; ++door)
	{
		const float angle = static_cast<float>(door) * glm::quarter_pi<float>() + 0.0f;
		const glm::vec3 direction {std::cos(angle), 0.0f, std::sin(angle)};
		const auto hit = TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), direction * 0.2f);
		ASSERT_TRUE(hit.has_value());
		EXPECT_FALSE(hit->floor);
		EXPECT_NEAR(hit->distance, TempleCameraModel::k_RoomRadius, 1e-3f);
		const auto found = TempleCameraModel::DoorAt(*hit);
		ASSERT_TRUE(found.has_value()) << "door " << door;
		EXPECT_EQ(*found, door);
	}
	// Between doors, and above them, there are none
	const glm::vec3 between {std::cos(0.4f), 0.0f, std::sin(0.4f)};
	EXPECT_FALSE(TempleCameraModel::DoorAt(*TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), between)));
	EXPECT_FALSE(TempleCameraModel::DoorAt(*TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(1, 0, 0))));
}

TEST(TempleCamera, HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms)
{
	const auto hit = TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 10.0f, 20.0f), glm::vec3(0.0f, -0.2f, -0.05f));
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
	const auto hit = TempleCameraModel::RayCastRoom(camera.GetOrigin(), direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->height, 15.0f, 0.05f);
	EXPECT_EQ(TempleCameraModel::DoorAt(*hit), 7u);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(7), TempleRoom::Challenge);
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
	const auto hit =
	    TempleCameraModel::RayCastRoom(room.centre + glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), room);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->distance, 37.0f, 1e-3f);
	EXPECT_NEAR(hit->point.x, room.centre.x + 37.0f, 1e-3f);
	EXPECT_NEAR(hit->height, 10.0f, 1e-3f);
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
