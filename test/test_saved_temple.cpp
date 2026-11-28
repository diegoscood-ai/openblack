/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a saved game keeps of the temple: whether its rooms are open and the main room's map buttons, written from the
// temple as it is in play and put back by a load (saved_game_save::TempleOf, saved_game_load::PutBackTemple), and the
// challenge room's records, written from the records made in play and put back by a load
// (saved_game_save::ChallengePartsOf, saved_game_load::PutBackChallengeRecords).
// docs/wiki/engine/saved-games.md#openblack

#include <cstdint>

#include <array>
#include <bit>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <SavedRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedScriptObjects.h"

using namespace openblack;
namespace saved_game_load = openblack::ecs::saved_game_load;
namespace saved_game_save = openblack::ecs::saved_game_save;
namespace records = openblack::ecs::challenge_records;
namespace snapshots = openblack::script::challenge_snapshots;
using openblack::ecs::components::ChallengeRecord;

namespace
{
/// The temple as far as a save and a load ask of it: whether its rooms are open and its map buttons
class FakeTemple final: public TempleInteriorInterface
{
public:
	bool roomsOpened {false};
	std::array<bool, k_MapButtonCount> mapButtons {true, true, true, true, true};

	[[nodiscard]] bool RoomsOpened() const override { return roomsOpened; }
	void SetRoomsOpened(bool opened) override { roomsOpened = opened; }
	[[nodiscard]] std::array<bool, k_MapButtonCount> GetMapButtons() const override { return mapButtons; }
	void SetMapButtons(const std::array<bool, k_MapButtonCount>& shown) override { mapButtons = shown; }

	[[nodiscard]] bool Active() const override { return false; }
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
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom) override {}
	void ForgetKeptRoom() override {}
	void BeginLandChange() override {}
	void EndLandChange() override {}
	void Deactivate() override {}

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

/// A save of a temple, read back as a load reads the world file
saved_game_load::LoadedGameState Loaded(const gamesave::GameHead& head)
{
	gamesave::WorldRead world;
	world.objects.emplace_back(head);
	auto state = saved_game_load::StateOf(world);
	EXPECT_TRUE(state.has_value());
	return state.value_or(saved_game_load::LoadedGameState {});
}

/// The scripts' object table of the save: the scripts' value of each place, place 1 first
const std::vector<uint32_t> k_Places = {11, 22, 77};
constexpr uint32_t k_Chest = 77;
constexpr uint32_t k_ChestPlace = 3;
constexpr uint32_t k_LostBrother = 19;
constexpr uint32_t k_Shepherd = 0x10022;

snapshots::Value ValueOf(uint32_t value, openblack::lhvm::DataType type)
{
	snapshots::Value result;
	result.value.uintVal = value;
	result.type = type;
	return result;
}

snapshots::Snapshot SnapshotOf(uint32_t challenge, float success, bool quest)
{
	snapshots::Snapshot snapshot;
	snapshot.challenge = challenge;
	snapshot.title = 2433;
	snapshot.reminderScript = "StandardReminder";
	snapshot.arguments = {ValueOf(k_Chest, openblack::lhvm::DataType::Object), ValueOf(4255, openblack::lhvm::DataType::Int)};
	snapshot.success = success;
	snapshot.alignment = -0.25f;
	snapshot.position = glm::vec3 {1.0f, 2.0f, 3.0f};
	snapshot.focus = glm::vec3 {4.0f, 5.0f, 6.0f};
	snapshot.quest = quest;
	return snapshot;
}

saved_game_save::PlaceOfThing PlaceInTheTable()
{
	return [](uint32_t value) { return openblack::ecs::saved_script_objects::PlaceOf(k_Places, value); };
}
} // namespace

TEST(SavedTemple, TheRoomsOpenAndTheMapButtonsAreWrittenFromPlayAndPutBackByALoad)
{
	// in play the main room was drawn, which opened the others, and the creatures' and the influence buttons turned off
	FakeTemple played;
	played.roomsOpened = true;
	played.mapButtons = {true, false, true, false, true};
	const saved_game_save::GameValues values {.temple = saved_game_save::TempleOf(played)};
	// a new game's record, as every save starts from one: its other values (the camera's zones among them) are a game's
	const auto start = saved_game_save::NewGameHead();
	const auto head = saved_game_save::HeadOf(values, start);
	// after the audio: the rooms' flag, then the five buttons, each 1 for on
	EXPECT_EQ(head.tail.roomsAvailable, 1u);
	EXPECT_EQ(head.tail.buttonConfig, (std::array<uint32_t, 5> {1, 0, 1, 0, 1}));

	// a load into a temple whose rooms are closed and whose buttons are all on puts them back as saved
	const auto state = Loaded(head);
	FakeTemple loaded;
	saved_game_load::PutBackTemple(loaded, state);
	EXPECT_TRUE(loaded.roomsOpened);
	EXPECT_EQ(loaded.mapButtons, (std::array {true, false, true, false, true}));
	// and a save right after the load writes them back as read, from the load's values and from the temple alike
	EXPECT_EQ(saved_game_save::HeadOf(saved_game_save::ValuesOf(state), start), head);
	EXPECT_EQ(saved_game_save::HeadOf({.temple = saved_game_save::TempleOf(loaded)}, start), head);
}

TEST(SavedTemple, ASaveMadeBeforeTheMainRoomWasDrawnClosesTheRoomsAgain)
{
	// a game saved before the player first went into the temple: only the main room open, every button on
	FakeTemple played;
	const auto head = saved_game_save::HeadOf({.temple = saved_game_save::TempleOf(played)}, {});
	EXPECT_EQ(head.tail.roomsAvailable, 0u);
	EXPECT_EQ(head.tail.buttonConfig, (std::array<uint32_t, 5> {1, 1, 1, 1, 1}));
	// loaded into a game whose rooms were open, the rooms close, and the buttons turned off come back on
	FakeTemple loaded;
	loaded.roomsOpened = true;
	loaded.mapButtons = {false, false, false, false, false};
	saved_game_load::PutBackTemple(loaded, Loaded(head));
	EXPECT_FALSE(loaded.roomsOpened);
	EXPECT_EQ(loaded.mapButtons, (std::array {true, true, true, true, true}));
}

TEST(SavedTemple, WithNoTempleTheRecordKeepsWhatItHeld)
{
	gamesave::GameHead base;
	base.tail.roomsAvailable = 1;
	base.tail.buttonConfig = {0, 1, 0, 1, 0};
	EXPECT_EQ(saved_game_save::HeadOf({}, base), base);
}

TEST(SavedChallengeRecords, TheRecordsAreWrittenFromPlayAndPutBackByALoad)
{
	// in play: a quest started at turn 100 and done at turn 400, then a challenge started at turn 200, halfway
	openblack::ecs::Registry played;
	records::Record(played, SnapshotOf(k_LostBrother, 0.0f, true), snapshots::Call::Start, 100, {});
	records::Record(played, SnapshotOf(k_Shepherd, 0.5f, false), snapshots::Call::Start, 200, {});
	records::Record(played, SnapshotOf(k_LostBrother, 1.0f, false), snapshots::Call::Update, 400, {});
	const auto parts = saved_game_save::ChallengePartsOf(played, PlaceInTheTable());
	ASSERT_EQ(parts.size(), 2u);

	// the first started first: its challenge, its reminder script in 64 bytes, its title, success and alignment, the
	// camera, the turn it started twice, the turn it was done on and its count, its values (the chest by its place in
	// the table) and their types, and whether it is a quest
	const auto& done = parts[0];
	EXPECT_EQ(done.half, k_LostBrother);
	ASSERT_EQ(done.name.values.size(), 64u);
	EXPECT_EQ(std::string(done.name.values.begin(), done.name.values.begin() + 16), "StandardReminder");
	EXPECT_EQ(done.name.values[16], 0u);
	EXPECT_EQ(done.words, (std::array<uint32_t, 3> {2433, std::bit_cast<uint32_t>(1.0f), std::bit_cast<uint32_t>(-0.25f)}));
	EXPECT_EQ(done.points[0], (gamesave::Point {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(done.points[1], (gamesave::Point {4.0f, 5.0f, 6.0f}));
	EXPECT_EQ(done.secondWords, (std::array<uint32_t, 4> {100, 100, 400, 2}));
	EXPECT_EQ(done.first.values, (std::vector<uint32_t> {k_ChestPlace, 4255}));
	EXPECT_EQ(done.second.values, (std::vector<uint32_t> {4, 1}));
	EXPECT_EQ(done.lastWord, 1u);
	// the second keeps only its challenge's low 16 bits, and is not done yet
	EXPECT_EQ(parts[1].half, 0x22u);
	EXPECT_EQ(parts[1].secondWords, (std::array<uint32_t, 4> {200, 200, 0, 2}));
	EXPECT_EQ(parts[1].lastWord, 0u);

	// a load puts them back in their order, the chest the loaded object of its place
	gamesave::GameHead head;
	head.tail.challenges.elements = parts;
	const auto state = Loaded(head);
	openblack::ecs::Registry loaded;
	saved_game_load::PutBackChallengeRecords(loaded, state, k_Places);
	const auto order = records::InOrder(loaded);
	ASSERT_EQ(order.size(), 2u);
	const auto& first = loaded.Get<ChallengeRecord>(order[0]);
	EXPECT_EQ(first.challenge, k_LostBrother);
	EXPECT_EQ(first.reminderScript, "StandardReminder");
	EXPECT_TRUE(records::IsDone(first));
	EXPECT_EQ(first.doneTurn, 400u);
	EXPECT_EQ(first.startedTurn, 100u);
	EXPECT_TRUE(first.quest);
	ASSERT_EQ(first.arguments.size(), 2u);
	EXPECT_EQ(first.arguments[0].value.uintVal, k_Chest);
	EXPECT_EQ(first.arguments[0].type, openblack::lhvm::DataType::Object);
	EXPECT_EQ(first.arguments[1].value.uintVal, 4255u);
	EXPECT_EQ(records::List(loaded).completed, 1);
	EXPECT_EQ(records::List(loaded).discovered, 2);
	EXPECT_FALSE(loaded.Get<ChallengeRecord>(order[1]).doneTurn.has_value());
	// and a save right after the load writes them back as read
	EXPECT_EQ(saved_game_save::ChallengePartsOf(loaded, PlaceInTheTable()), parts);
}

TEST(SavedChallengeRecords, TheTurnItStartedIsReadTwiceAndOnlyADoneRecordKeepsItsDoneTurn)
{
	gamesave::ChallengePart part;
	part.half = 7;
	part.words = {1, std::bit_cast<uint32_t>(0.5f), 0};
	part.secondWords = {5, 9, 30, 1};
	part.first.values = {k_ChestPlace, 8};
	part.second.values = {4, 1};
	// the second read of the turn it started is kept, a record not done has no done turn, and only its count of values
	// is taken
	const auto record = saved_game_load::ChallengeRecordOf(part, 0);
	EXPECT_EQ(record.startedTurn, 9u);
	EXPECT_FALSE(record.doneTurn.has_value());
	ASSERT_EQ(record.arguments.size(), 1u);
	EXPECT_EQ(record.arguments[0].value.uintVal, k_ChestPlace);
	EXPECT_TRUE(record.reminderScript.empty());
	// a record never given a camera writes it as 0
	const ChallengeRecord noCamera {.challenge = 7};
	const auto written = saved_game_save::ChallengePartOf(noCamera, PlaceInTheTable());
	EXPECT_EQ(written.points[0], (gamesave::Point {}));
	EXPECT_EQ(written.points[1], (gamesave::Point {}));
	EXPECT_TRUE(written.first.values.empty());
}

TEST(SavedChallengeRecords, ALoadAddsTheRecordsAfterAnyTheGameHolds)
{
	openblack::ecs::Registry registry;
	records::Record(registry, SnapshotOf(k_LostBrother, 0.0f, true), snapshots::Call::Start, 100, {});
	saved_game_load::LoadedGameState state;
	state.challenges = {ChallengeRecord {.challenge = 3}};
	saved_game_load::PutBackChallengeRecords(registry, state, k_Places);
	const auto order = records::InOrder(registry);
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(registry.Get<ChallengeRecord>(order[1]).challenge, 3u);
}
