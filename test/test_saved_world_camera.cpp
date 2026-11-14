/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game camera of a saved game's world file: a load takes its focus, position and field of view zoomers, and a save
// writes them, the mode's seconds and the current mode from the camera of the world, the rest as the load kept it; the
// frozen saves' cameras come back and are written as read (docs/wiki/engine/saved-games.md#the-saves-in-play)

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <GameTailRecords.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureFollow.h"
#include "Camera/ScriptCamera.h"
#include "Common/Zoomer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadCamera.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Weather/Atmos.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::CameraRecord;
using gamesave::SavedRecord;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";

/// A zoomer on its way: every value its own, so a value put in the wrong place shows
Zoomer MovingZoomer(float base)
{
	return {.value = base,
	        .destination = base + 1.0f,
	        .destinationSpeed = 0.0f,
	        .speed = base + 2.0f,
	        .time = 0.016f,
	        .duration = 0.3f,
	        .startValue = base + 3.0f,
	        .startSpeed = base + 4.0f,
	        .c2 = base + 5.0f,
	        .c3 = base + 6.0f,
	        .c4 = base + 7.0f};
}

/// A zoomer's saved bytes, to compare zoomers by
std::array<uint8_t, 48> Bytes(const Zoomer& zoomer)
{
	return ecs::saved_zoomer::BytesOf(zoomer);
}

/// The record's 144 bytes of three zoomers, x, y then z
std::array<uint8_t, 144> ThreeZoomers(float base)
{
	std::array<uint8_t, 144> bytes {};
	for (size_t axis = 0; axis < 3; ++axis)
	{
		const auto one = ecs::saved_zoomer::BytesOf(MovingZoomer(base + static_cast<float>(axis) * 100.0f));
		std::ranges::copy(one, bytes.begin() + static_cast<std::ptrdiff_t>(axis * one.size()));
	}
	return bytes;
}

/// A loaded camera: its focus's zoomers from 1000, its position's from 2000, its field of view's from 1.2, its mode's
/// seconds 213.5 and values openblack does not hold
CameraRecord LoadedCamera()
{
	CameraRecord camera;
	camera.point = {2458.5f, 32.5f, 1825.9f};
	camera.words = {std::bit_cast<uint32_t>(213.5f), std::bit_cast<uint32_t>(0.016f), 0, 1};
	camera.eight = {0, 0, 0, 0, 0, 0, 0, 0x31};
	camera.first = ThreeZoomers(1000.0f);
	camera.second = ThreeZoomers(2000.0f);
	camera.zoomer = ecs::saved_zoomer::BytesOf(MovingZoomer(1.2f));
	return camera;
}

/// A loaded world whose game object's tail names the camera, object 2
gamesave::WorldRead KeptWorld()
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.tail.objects[0] = {2};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(LoadedCamera());
	return kept;
}

/// The temple as far as a save asks of it: the island's camera it keeps while the player is inside
class FakeTemple final: public TempleInteriorInterface
{
public:
	std::optional<TempleOutsideCamera> outside;

	[[nodiscard]] std::optional<TempleOutsideCamera> GetOutsideCamera() const override { return outside; }
	[[nodiscard]] bool Active() const override { return outside.has_value(); }
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return TempleRoom::Main; }
	void SetCurrentRoom(TempleRoom) override {}
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

/// The world files of every profile's saved games, or none without the game's data
std::vector<std::filesystem::path> WorldFiles()
{
	std::vector<std::filesystem::path> files;
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (game == nullptr || !std::filesystem::is_directory(std::filesystem::path(game) / "Profiles", error))
	{
		return files;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(game) / "Profiles"))
	{
		const auto name = entry.path().filename().string();
		if (entry.is_regular_file() && entry.path().extension() == ".sav" && !name.starts_with("Script_") &&
		    !name.ends_with("_extra.sav") && entry.path().string().find("Saved Games") != std::string::npos)
		{
			files.push_back(entry.path());
		}
	}
	return files;
}
} // namespace

// The record keeps the focus's three zoomers, then the position's, then the field of view's, each value in its place
TEST(SavedCamera, TheRecordsZoomersAreTheFocusThePositionAndTheFieldOfView)
{
	const auto zoomers = ecs::saved_camera::ZoomersOf(LoadedCamera());
	EXPECT_EQ(Bytes(zoomers.focus.axis[0]), Bytes(MovingZoomer(1000.0f)));
	EXPECT_EQ(Bytes(zoomers.focus.axis[2]), Bytes(MovingZoomer(1200.0f)));
	EXPECT_EQ(Bytes(zoomers.origin.axis[1]), Bytes(MovingZoomer(2100.0f)));
	EXPECT_EQ(Bytes(zoomers.fov), Bytes(MovingZoomer(1.2f)));
}

// Written over the record, the zoomers and the mode's seconds change and nothing else does; the zoomers a record gives
// write back the same bytes
TEST(SavedCamera, TheLiveZoomersAndModeSecondsAreWrittenOverTheRecord)
{
	const auto loaded = LoadedCamera();
	auto same = loaded;
	ecs::saved_camera::WriteLive(same, ecs::saved_camera::ZoomersOf(loaded), 213.5f);
	EXPECT_EQ(same, loaded);

	auto moved = loaded;
	auto zoomers = ecs::saved_camera::ZoomersOf(loaded);
	zoomers.origin.axis[0] = MovingZoomer(5.0f);
	zoomers.fov = MovingZoomer(0.9f);
	ecs::saved_camera::WriteLive(moved, zoomers, 0.5f);
	EXPECT_EQ(Bytes(ecs::saved_camera::ZoomersOf(moved).origin.axis[0]), Bytes(MovingZoomer(5.0f)));
	EXPECT_EQ(Bytes(ecs::saved_camera::ZoomersOf(moved).fov), Bytes(MovingZoomer(0.9f)));
	EXPECT_EQ(moved.words[0], std::bit_cast<uint32_t>(0.5f));
	EXPECT_EQ(moved.first, loaded.first);
	moved.words[0] = loaded.words[0];
	moved.second = loaded.second;
	moved.zoomer = loaded.zoomer;
	EXPECT_EQ(moved, loaded);
}

// A loaded camera is kept by its one id and written by its writer: as loaded with no camera of the world, and from the
// camera's zoomers, the script camera's field of view and the mode's seconds with one
TEST(SavedCamera, ALoadedCameraIsWrittenFromTheCameraOfTheWorld)
{
	const auto kept = KeptWorld();
	ASSERT_EQ(ecs::saved_camera::CameraNumber(kept), 2u);
	LoadedObjects loaded;
	KeepCamera(kept, loaded);
	ASSERT_EQ(loaded.size(), 1u);
	EXPECT_EQ(loaded.at(2).type, CameraRecord::k_Type);
	EXPECT_EQ(loaded.at(2).id, std::optional<uint64_t>(ecs::saved_camera::k_CameraId));

	ecs::Registry registry;
	ClassWriters writers;
	RegisterCameraWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};
	{
		const test::RestoreService<Locator::camera> restoreCamera;
		Locator::camera::reset();
		const auto written = WriteWorld(writers, source);
		ASSERT_TRUE(written.Complete()) << written.failure;
		EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));
		ASSERT_EQ(written.records.size(), 2u);
		EXPECT_EQ(written.records[1].source, RecordSource::Writer);
	}

	const test::RestoreService<Locator::camera> restoreCamera;
	auto& camera = Locator::camera::emplace<Camera>();
	auto& script = script_camera::Get();
	const auto fovBefore = script.fov;
	const auto secondsBefore = script.modeSeconds;
	camera.GetOriginZoomer().axis[1] = MovingZoomer(7.0f);
	camera.GetFocusZoomer().axis[2] = MovingZoomer(8.0f);
	script.fov = MovingZoomer(1.1f);
	script.modeSeconds = 4.0f;
	const auto written = WriteWorld(writers, source);
	script.fov = fovBefore;
	script.modeSeconds = secondsBefore;
	ASSERT_TRUE(written.Complete()) << written.failure;
	const auto read = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(read.objects.size(), 2u);
	const auto* record = std::get_if<CameraRecord>(&*read.objects[1]);
	ASSERT_NE(record, nullptr);
	const auto zoomers = ecs::saved_camera::ZoomersOf(*record);
	EXPECT_EQ(Bytes(zoomers.origin.axis[1]), Bytes(MovingZoomer(7.0f)));
	EXPECT_EQ(Bytes(zoomers.focus.axis[2]), Bytes(MovingZoomer(8.0f)));
	EXPECT_EQ(Bytes(zoomers.fov), Bytes(MovingZoomer(1.1f)));
	EXPECT_EQ(record->words[0], std::bit_cast<uint32_t>(4.0f));
	EXPECT_EQ(record->point, LoadedCamera().point);
	EXPECT_EQ(record->eight, LoadedCamera().eight);
}

// The camera's frame time is the frame's whole ms x 0.001 as floats, at most 0.1: the frozen saves' 1, 15, 16 and 31 ms
TEST(SavedCamera, TheFrameTimeIsTheFramesWholeMillisecondsAtMostATenth)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(0)), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(1)), 0x3A83126Fu);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(15)), 0x3C75C290u);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(16)), 0x3C83126Fu);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(31)), 0x3CFDF3B7u);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(100)), 0x3DCCCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(500)), 0x3DCCCCCDu);
}

// A camera made since the load (a new game's) is the player's mode with no word, flags or byte of its own, its zoomers,
// mode's seconds, last frame's seconds, the script's hold of the speed, whether it moved, where it is drawn, the point it
// looks at and the weather at it from what the game holds
TEST(SavedCamera, ANewGamesCameraIsWrittenFromWhatTheGameHolds)
{
	const auto zoomers = ecs::saved_camera::ZoomersOf(LoadedCamera());
	const gamesave::MapCoords drawnAt {.x = 161 << 16, .z = 264 << 16, .altitude = 133.0f};
	const auto record = ecs::saved_camera::NewRecord({.zoomers = zoomers,
	                                                  .modeSeconds = 24.0f,
	                                                  .frameMs = 15,
	                                                  .scriptHoldsSpeed = true,
	                                                  .drawnMoved = true,
	                                                  .drawnAt = drawnAt,
	                                                  .lookedAt = {1730.3f, 12.06f, 2528.9f},
	                                                  .weather = {2, 0x41, 0, 0x34, 0xEE, 0xF8, 0, 0xAF}});
	EXPECT_EQ(record.thing, gamesave::ThingPart {});
	EXPECT_EQ(record.position.position, drawnAt);
	EXPECT_EQ(record.position.flags, 0);
	EXPECT_EQ(record.point, (gamesave::Point {1730.3f, 12.06f, 2528.9f}));
	const std::array<uint32_t, 4> words {std::bit_cast<uint32_t>(24.0f), 0x3C75C290u, 1, 1};
	EXPECT_EQ(record.words, words);
	EXPECT_EQ(record.eight, (std::array<uint8_t, 8> {2, 0x41, 0, 0x34, 0xEE, 0xF8, 0, 0xAF}));
	EXPECT_EQ(record.first, LoadedCamera().first);
	EXPECT_EQ(record.second, LoadedCamera().second);
	EXPECT_EQ(record.zoomer, LoadedCamera().zoomer);
	EXPECT_EQ(record.byte, 0);
	EXPECT_EQ(record.mode, gamesave::CameraModePart {});
}

// With no camera of the world a camera made since the load can't be written: a new game's tail names none, and its
// writer refuses
TEST(SavedCamera, WithNoCameraOfTheWorldANewGamesCameraIsNotWritten)
{
	const test::RestoreService<Locator::camera> restoreCamera;
	Locator::camera::reset();
	EXPECT_FALSE(ecs::saved_camera::NewCameraWritable());
	ecs::Registry registry;
	ClassWriters writers;
	RegisterCameraWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	const auto* write = writers.SourceWriterOf(CameraRecord::k_Type);
	ASSERT_NE(write, nullptr);
	EXPECT_FALSE((*write)(context, ecs::saved_camera::k_CameraId).has_value());
	EXPECT_FALSE(context.Failure().empty());
}

// Inside the citadel the camera of the world shows the rooms: a save writes the island's camera the citadel keeps, its
// zoomers as the player left them going in
TEST(SavedCamera, InsideTheCitadelASaveWritesTheIslandsCameraTheCitadelKeeps)
{
	const auto kept = KeptWorld();
	LoadedObjects loaded;
	KeepCamera(kept, loaded);
	ecs::Registry registry;
	ClassWriters writers;
	RegisterCameraWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};

	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::temple> restoreTemple;
	auto& camera = Locator::camera::emplace<Camera>();
	camera.GetOriginZoomer().axis[0] = MovingZoomer(-50.0f);
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	TempleOutsideCamera outside;
	outside.origin.axis[0] = MovingZoomer(3000.0f);
	outside.focus.axis[1] = MovingZoomer(40.0f);
	temple.outside = outside;
	const auto sequenceBefore = game_clock::SequenceMode();
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
	const auto written = WriteWorld(writers, source);
	game_clock::SetSequenceMode(sequenceBefore);
	ASSERT_TRUE(written.Complete()) << written.failure;
	const auto read = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(read.objects.size(), 2u);
	const auto* record = std::get_if<CameraRecord>(&*read.objects[1]);
	ASSERT_NE(record, nullptr);
	const auto zoomers = ecs::saved_camera::ZoomersOf(*record);
	EXPECT_EQ(Bytes(zoomers.origin.axis[0]), Bytes(MovingZoomer(3000.0f)));
	EXPECT_EQ(Bytes(zoomers.focus.axis[1]), Bytes(MovingZoomer(40.0f)));
}

// The rooms' camera inside the citadel is held 1 m over the land under it as the island camera's update reads it
TEST(SavedCamera, InsideTheCitadelTheDrawnCameraIsHeldOneMetreOverTheLand)
{
	const auto held = ecs::saved_camera::HeldOverLand({5.0f, 3.0f, 7.0f}, 20.1f);
	EXPECT_EQ(held.y, (20.1f + 1.0f) - 3.0f + 3.0f);
	EXPECT_EQ(held.x, 5.0f);
	EXPECT_EQ(ecs::saved_camera::HeldOverLand({5.0f, 40.0f, 7.0f}, 20.1f).y, 40.0f);
}

// A new game's camera saved inside the citadel: the zoomers the citadel keeps and the point they look at; where it is
// drawn, whether it moved and the weather from the rooms' camera of the frame before, held over the land
TEST(SavedCamera, ANewGamesCameraSavedInsideTheCitadelIsTheKeptIslandCameraAndTheRoomsDrawnCamera)
{
	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::temple> restoreTemple;
	const test::RestoreService<Locator::terrainSystem> restoreLand;
	Locator::camera::emplace<Camera>();
	constexpr uint8_t k_Altitude = 30;
	Locator::terrainSystem::emplace<test::FlatDryIsland>(k_Altitude);
	const float ground = test::FlatDryIsland::HeightOf(k_Altitude);
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	TempleOutsideCamera outside;
	outside.origin.SetPosition({1000.0f, 200.0f, 1000.0f});
	outside.focus.SetPosition({1000.0f, ground, 1100.0f});
	temple.outside = outside;
	auto& script = script_camera::Get();
	const auto before = script.drawnBefore;
	const glm::vec3 room {5.0f, 3.0f, 7.0f};
	script.drawnBefore = room;
	const auto sequenceBefore = game_clock::SequenceMode();
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);

	ecs::Registry registry;
	ClassWriters writers;
	RegisterCameraWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	EXPECT_TRUE(ecs::saved_camera::NewCameraWritable());
	const auto made = (*writers.SourceWriterOf(CameraRecord::k_Type))(context, ecs::saved_camera::k_CameraId);
	game_clock::SetSequenceMode(sequenceBefore);
	script.drawnBefore = before;
	ASSERT_TRUE(made.has_value()) << context.Failure();
	const auto& record = std::get<CameraRecord>(*made);
	const auto zoomers = ecs::saved_camera::ZoomersOf(record);
	EXPECT_EQ(Bytes(zoomers.origin.axis[1]), Bytes(outside.origin.axis[1]));
	EXPECT_EQ(Bytes(zoomers.focus.axis[2]), Bytes(outside.focus.axis[2]));
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(room.x));
	EXPECT_EQ(record.position.position.z, map_coords::ToFixed(room.z));
	EXPECT_EQ(record.position.position.altitude, room.y - static_cast<float>(k_Altitude) * LandIslandInterface::k_HeightUnit);
	EXPECT_EQ(record.words[3], 1u);
	const auto held = ecs::saved_camera::HeldOverLand(room, ground);
	const auto weatherBytes = std::bit_cast<std::array<uint8_t, 8>>(weather::atmos::GetWeatherSmooth(held, true));
	EXPECT_EQ(record.eight, weatherBytes);
	EXPECT_EQ(record.point[1], ground);
	EXPECT_NEAR(record.point[0], 1000.0f, 1.0f);
	EXPECT_NEAR(record.point[2], 1100.0f, 1.0f);
	EXPECT_EQ(record.mode, gamesave::CameraModePart {});
}

// The script's mode saves as a follow: its heading, pitch, distance and time factor as floats, "behind" as 1, its offsets
// not in use and written as 0, the thing its position follows, and no clear view; the file keeps the part as it is
TEST(SavedCamera, TheScriptsModeIsSavedAsAFollowOfItsView)
{
	const auto part = ecs::saved_camera::FollowPartOf(
	    {.heading = 0.3927f, .pitch = 0.5f, .distance = 3.0f, .timeFactor = 0.2f, .behind = true, .thing = 7});
	EXPECT_EQ(part.mode, ecs::saved_camera::k_FollowMode);
	const std::array<uint32_t, 6> words {std::bit_cast<uint32_t>(0.3927f),
	                                     std::bit_cast<uint32_t>(0.5f),
	                                     std::bit_cast<uint32_t>(3.0f),
	                                     std::bit_cast<uint32_t>(0.2f),
	                                     1,
	                                     0};
	EXPECT_EQ(part.words, words);
	EXPECT_EQ(part.points[0], gamesave::Point {});
	EXPECT_EQ(part.points[1], gamesave::Point {});
	EXPECT_EQ(part.object.number, 7u);
	EXPECT_EQ(part.lastWord, 0u);
	EXPECT_EQ(ecs::saved_camera::FollowPartOf({.behind = false}).words[4], 0u);

	auto kept = KeptWorld();
	auto follow = part;
	follow.object.number = 0;
	std::get<CameraRecord>(*kept.objects[1]).mode = follow;
	const auto read = gamesave::ReadWorld(gamesave::WriteWorld(kept));
	ASSERT_EQ(read.objects.size(), 2u);
	const auto* record = std::get_if<CameraRecord>(&*read.objects[1]);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->mode, follow);
}

// A load makes the player's camera of a plain mode and of a follow of nothing, which goes at the first turn; a follow of
// a thing is made once the things are back; a heading and a free mode, which only a load makes, are not made again
TEST(SavedCamera, ALoadMakesThePlayersCameraOfAPlainModeOrAFollowOfNothing)
{
	using ecs::saved_camera::LoadedMode;
	EXPECT_EQ(ecs::saved_camera::LoadedModeOf({}), LoadedMode::Player);
	gamesave::CameraModePart mode;
	mode.mode = ecs::saved_camera::k_FollowMode;
	EXPECT_EQ(ecs::saved_camera::LoadedModeOf(mode), LoadedMode::Player);
	mode.object.number = 5;
	EXPECT_EQ(ecs::saved_camera::LoadedModeOf(mode), LoadedMode::FollowOfAThing);
	mode.object.number = 0;
	mode.mode = ecs::saved_camera::k_HeadingMode;
	EXPECT_EQ(ecs::saved_camera::LoadedModeOf(mode), LoadedMode::NotMade);
	mode.mode = ecs::saved_camera::k_FreeMode;
	EXPECT_EQ(ecs::saved_camera::LoadedModeOf(mode), LoadedMode::NotMade);
}

// A save made while the script's mode has the camera, as in a cut scene, writes that mode as a follow of its view and
// the zoomers it moves; once the player's mode has the camera again, the camera of the world's zoomers and no mode
TEST(SavedCamera, ASaveInACutSceneWritesTheScriptsModeAndTheZoomersItMoves)
{
	const auto kept = KeptWorld();
	LoadedObjects loaded;
	KeepCamera(kept, loaded);
	ecs::Registry registry;
	ClassWriters writers;
	RegisterCameraWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};

	const test::RestoreService<Locator::camera> restoreCamera;
	auto& camera = Locator::camera::emplace<Camera>();
	camera.GetOriginZoomer().axis[0] = MovingZoomer(7.0f);
	auto& script = script_camera::Get();
	const auto positionBefore = script.position;
	const auto focusBefore = script.focus;
	script_camera::ClearModes();
	ASSERT_TRUE(script_camera::Begin({100.0f, 50.0f, 100.0f}, {200.0f, 0.0f, 200.0f}));
	script.position.axis[0] = MovingZoomer(60.0f);
	script.focus.axis[2] = MovingZoomer(70.0f);
	script.heading = 0.5f;
	script.pitch = 0.3f;
	script.distance = 40.0f;
	const auto inCutScene = WriteWorld(writers, source);
	script_camera::ClearModes();
	const auto afterIt = WriteWorld(writers, source);
	script.position = positionBefore;
	script.focus = focusBefore;

	ASSERT_TRUE(inCutScene.Complete()) << inCutScene.failure;
	const auto read = gamesave::ReadWorld(inCutScene.bytes);
	ASSERT_EQ(read.objects.size(), 2u);
	const auto* record = std::get_if<CameraRecord>(&*read.objects[1]);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->mode, ecs::saved_camera::FollowPartOf({.heading = 0.5f,
	                                                         .pitch = 0.3f,
	                                                         .distance = 40.0f,
	                                                         .timeFactor = script_camera::k_ScriptFollowTimeFactor,
	                                                         .behind = true}));
	const auto zoomers = ecs::saved_camera::ZoomersOf(*record);
	EXPECT_EQ(Bytes(zoomers.origin.axis[0]), Bytes(MovingZoomer(60.0f)));
	EXPECT_EQ(Bytes(zoomers.focus.axis[2]), Bytes(MovingZoomer(70.0f)));

	ASSERT_TRUE(afterIt.Complete()) << afterIt.failure;
	const auto readAfter = gamesave::ReadWorld(afterIt.bytes);
	ASSERT_EQ(readAfter.objects.size(), 2u);
	const auto* after = std::get_if<CameraRecord>(&*readAfter.objects[1]);
	ASSERT_NE(after, nullptr);
	EXPECT_EQ(after->mode, gamesave::CameraModePart {});
	EXPECT_EQ(Bytes(ecs::saved_camera::ZoomersOf(*after).origin.axis[0]), Bytes(MovingZoomer(7.0f)));
}

// A saved follow reads back as it was written, every value in its place; another mode keeps no follow
TEST(SavedCamera, AFollowReadsBackFromItsModePart)
{
	const ecs::saved_camera::Follow follow {.heading = 2.5f,
	                                        .pitch = 0.4f,
	                                        .distance = 75.0f,
	                                        .timeFactor = 1.0f,
	                                        .behind = true,
	                                        .offsets = true,
	                                        .thing = 9,
	                                        .clearView = true};
	const auto part = ecs::saved_camera::FollowPartOf(follow);
	EXPECT_EQ(part.words[5], 1u);
	EXPECT_EQ(part.lastWord, 1u);
	const auto read = ecs::saved_camera::FollowOf(part);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->heading, 2.5f);
	EXPECT_EQ(read->pitch, 0.4f);
	EXPECT_EQ(read->distance, 75.0f);
	EXPECT_EQ(read->timeFactor, 1.0f);
	EXPECT_TRUE(read->behind);
	EXPECT_TRUE(read->offsets);
	EXPECT_EQ(read->thing, 9u);
	EXPECT_TRUE(read->clearView);
	EXPECT_FALSE(ecs::saved_camera::FollowOf({}).has_value());
	gamesave::CameraModePart free;
	free.mode = ecs::saved_camera::k_FreeMode;
	EXPECT_FALSE(ecs::saved_camera::FollowOf(free).has_value());
}

// Creature Mode's follow is its view of the creature at the time factor 1, not behind it, with no offsets and the clear
// view looked for, the style a load makes it again with; a follow made to keep offsets has none
TEST(SavedCamera, CreatureModesFollowIsItsViewAtThePlainPaceWithTheClearView)
{
	const auto follow = ecs::saved_camera::CreatureModeFollow({.yaw = 1.5f, .pitch = 0.6f, .distance = 90.0f}, 4);
	const auto part = ecs::saved_camera::FollowPartOf(follow);
	const std::array<uint32_t, 6> words {std::bit_cast<uint32_t>(1.5f),
	                                     std::bit_cast<uint32_t>(0.6f),
	                                     std::bit_cast<uint32_t>(90.0f),
	                                     std::bit_cast<uint32_t>(1.0f),
	                                     0,
	                                     0};
	EXPECT_EQ(part.words, words);
	EXPECT_EQ(part.object.number, 4u);
	EXPECT_EQ(part.lastWord, 1u);
	const auto style = ecs::saved_camera::StyleOf(follow);
	ASSERT_TRUE(style.has_value());
	EXPECT_EQ(style->timeFactor, 1.0f);
	EXPECT_FALSE(style->behind);
	EXPECT_TRUE(style->clearView);
	auto offsets = follow;
	offsets.offsets = true;
	EXPECT_FALSE(ecs::saved_camera::StyleOf(offsets).has_value());
}

namespace
{
/// A thing the camera's follow names, which the fake writer below writes as an object of no other class
struct FollowedThing
{
};

/// Creature Mode as far as a load and a save ask of it: what it is locked onto, its view, and the restores asked for
class FakeCreatureMode final: public ecs::systems::CreatureModeSystemInterface
{
public:
	std::optional<entt::entity> creature;
	std::optional<creature_follow::View> view;
	std::optional<creature_follow::Style> style;
	struct Restored
	{
		entt::entity thing;
		creature_follow::View view;
		creature_follow::Style style;
	};
	std::vector<Restored> restored;

	void Update(std::chrono::microseconds, const Frame&) override {}
	void PressCreatureKey() override {}
	bool Enter(entt::entity) override { return false; }
	bool Restore(entt::entity thing, const creature_follow::View& saved, const creature_follow::Style& savedStyle) override
	{
		restored.push_back({thing, saved, savedStyle});
		return true;
	}
	void Leave() override {}
	void LockOnNext(entt::entity) override {}
	[[nodiscard]] bool IsActive() const override { return creature.has_value(); }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return creature; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override { return creature; }
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override { return view; }
	[[nodiscard]] std::optional<creature_follow::Style> GetStyle() const override { return style; }
	void ClearView() override {}
};
} // namespace

// A save while Creature Mode has the camera writes its follow of the creature with its view; the creature is the object
// the follow names
TEST(SavedCamera, ASaveInCreatureModeWritesItsFollowOfTheCreature)
{
	const auto kept = KeptWorld();
	LoadedObjects loaded;
	KeepCamera(kept, loaded);
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.AssignState<FollowedThing>(creature);
	ClassWriters writers;
	RegisterCameraWriters(writers);
	writers.Register(
	    gamesave::BaseOnlyRecord::k_Type,
	    [](const ecs::Registry& from, entt::entity entity) { return from.AllOf<FollowedThing>(entity); },
	    [](WriteContext&, entt::entity) -> std::optional<SavedRecord> { return gamesave::BaseOnlyRecord {}; });
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};

	const test::RestoreService<Locator::camera> restoreCamera;
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	Locator::camera::emplace<Camera>();
	auto& mode = static_cast<FakeCreatureMode&>(Locator::creatureModeSystem::emplace<FakeCreatureMode>());
	script_camera::ClearModes();
	mode.creature = creature;
	mode.view = creature_follow::View {.yaw = 0.25f, .pitch = 0.7f, .distance = 55.0f};
	mode.style = creature_follow::Style {};
	const auto written = WriteWorld(writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	const auto read = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(read.objects.size(), 3u);
	const auto* record = std::get_if<CameraRecord>(&*read.objects[1]);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->mode, ecs::saved_camera::FollowPartOf(ecs::saved_camera::CreatureModeFollow(*mode.view, 3)));
	// a follow a load made again writes its own style
	mode.style = creature_follow::Style {.timeFactor = 0.2f, .behind = true, .clearView = false};
	const auto again = WriteWorld(writers, source);
	ASSERT_TRUE(again.Complete()) << again.failure;
	const auto readAgain = gamesave::ReadWorld(again.bytes);
	ASSERT_EQ(readAgain.objects.size(), 3u);
	EXPECT_EQ(std::get<CameraRecord>(*readAgain.objects[1]).mode,
	          ecs::saved_camera::FollowPartOf(ecs::saved_camera::PlayersFollow(*mode.view, *mode.style, 3)));

	// with the camera not Creature Mode's (no view), no mode
	mode.view.reset();
	const auto plain = WriteWorld(writers, source);
	ASSERT_TRUE(plain.Complete()) << plain.failure;
	const auto readPlain = gamesave::ReadWorld(plain.bytes);
	ASSERT_GE(readPlain.objects.size(), 2u);
	EXPECT_EQ(std::get<CameraRecord>(*readPlain.objects[1]).mode, gamesave::CameraModePart {});
}

// A load of a follow of a thing makes the player's follow of the loaded thing with the saved view and style: Creature
// Mode's of a creature, and a script's of any thing; a follow of a thing the load did not make, or one made to keep
// offsets, takes nothing
TEST(SavedCamera, ALoadedCreatureModeFollowLocksOntoTheCreatureAgain)
{
	// the follow's load logs what it makes: run alone, no other test has made the game's logger
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto creature = registry.Create();
	registry.AssignState<ecs::components::Creature>(creature);
	auto& mode = static_cast<FakeCreatureMode&>(Locator::creatureModeSystem::emplace<FakeCreatureMode>());

	auto world = KeptWorld();
	auto& camera = std::get<CameraRecord>(*world.objects[1]);
	const creature_follow::View view {.yaw = 3.0f, .pitch = 0.5f, .distance = 120.0f};
	camera.mode = ecs::saved_camera::FollowPartOf(ecs::saved_camera::CreatureModeFollow(view, 3));
	LoadedObjects loaded;
	loaded.emplace(3, LoadedObject {.entity = creature});

	ecs::saved_camera::LoadFollow(world, loaded);
	ASSERT_EQ(mode.restored.size(), 1u);
	EXPECT_EQ(mode.restored[0].thing, creature);
	EXPECT_EQ(mode.restored[0].view.yaw, 3.0f);
	EXPECT_EQ(mode.restored[0].view.pitch, 0.5f);
	EXPECT_EQ(mode.restored[0].view.distance, 120.0f);
	EXPECT_EQ(mode.restored[0].style.timeFactor, 1.0f);
	EXPECT_FALSE(mode.restored[0].style.behind);
	EXPECT_TRUE(mode.restored[0].style.clearView);

	// the creature not made by the load
	ecs::saved_camera::LoadFollow(world, {});
	EXPECT_EQ(mode.restored.size(), 1u);
	// a script's follow of a thing, saved in a cut scene: the player's follow of it, with its style
	const auto thing = registry.Create();
	loaded.emplace(4, LoadedObject {.entity = thing});
	camera.mode = ecs::saved_camera::FollowPartOf(
	    {.distance = 40.0f, .timeFactor = script_camera::k_ScriptFollowTimeFactor, .behind = true, .thing = 4});
	ecs::saved_camera::LoadFollow(world, loaded);
	ASSERT_EQ(mode.restored.size(), 2u);
	EXPECT_EQ(mode.restored[1].thing, thing);
	EXPECT_EQ(mode.restored[1].view.distance, 40.0f);
	EXPECT_EQ(mode.restored[1].style.timeFactor, script_camera::k_ScriptFollowTimeFactor);
	EXPECT_TRUE(mode.restored[1].style.behind);
	EXPECT_FALSE(mode.restored[1].style.clearView);
	// a follow made to keep offsets, which no follow in play keeps, and a plain mode
	camera.mode = ecs::saved_camera::FollowPartOf({.offsets = true, .thing = 3});
	ecs::saved_camera::LoadFollow(world, loaded);
	camera.mode = {};
	ecs::saved_camera::LoadFollow(world, loaded);
	EXPECT_EQ(mode.restored.size(), 2u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. Every saved game's
// camera is named by its game object, its zoomers write back the bytes read, and the player's mode is the one saved
TEST(SavedCamera, EveryFrozenSavesCameraComesBackAndIsWrittenAsRead)
{
	const auto files = WorldFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no saved games";
	}
	size_t cameras = 0;
	for (const auto& path : files)
	{
		SCOPED_TRACE(path.string());
		std::ifstream stream(path, std::ios::binary);
		const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		const auto world = gamesave::ReadWorld(bytes);
		if (world.stop != gamesave::k_ReadToTheEnd)
		{
			continue;
		}
		const auto* camera = ecs::saved_camera::CameraOf(world);
		ASSERT_NE(camera, nullptr);
		++cameras;
		EXPECT_EQ(camera->mode.mode, ecs::saved_camera::k_PlainMode);
		auto written = *camera;
		ecs::saved_camera::WriteLive(written, ecs::saved_camera::ZoomersOf(*camera), std::bit_cast<float>(camera->words[0]));
		EXPECT_EQ(written, *camera);
	}
	EXPECT_GT(cameras, 0u);
}
