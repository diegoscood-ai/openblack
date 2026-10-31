/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' interfaces, their statuses and their leashes' statuses, and their computer players with their hands'
// states and attitudes, written from what openblack holds: each by its player's number, a loaded one with the values
// it does not model as the load kept them, one made since as its constructor leaves it, the leash from the leash of
// the creature the player leads, the computer player from what its load put back; and the frozen Land 1 autosave
// writes back byte for byte with them (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <ObjectStream.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Player.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "Locator.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ActionStateRecord;
using gamesave::ComputerAttitudeRecord;
using gamesave::ComputerPlayerRecord;
using gamesave::InterfaceStatusRecord;
using gamesave::LeashStatusRecord;
using gamesave::ObjectNumber;
using gamesave::PlayerInterfaceRecord;
using gamesave::SavedRecord;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";

/// The bytes the game writes for the records given, by their numbers in the file
std::vector<uint8_t> FileOf(const std::map<ObjectNumber, SavedRecord>& records)
{
	auto bytes = gamesave::WriteVanillaFraming(k_Land);
	const auto framing = gamesave::ReadVanillaFraming(bytes);
	gamesave::StreamWriter writer({.objects = 1, .checksum = framing->checksum});
	gamesave::WriteNewObject writeNew;
	writeNew = [&](gamesave::StreamWriter& stream, ObjectNumber number) {
		gamesave::Write(stream, records.at(number), writeNew);
	};
	gamesave::Write(writer, records.at(1), writeNew);
	const auto body = writer.Take();
	bytes.insert(bytes.end(), body.begin(), body.end());
	return bytes;
}

/// A loaded game as its load keeps it: the game object names a player whose first interface's status has a leash,
/// with values openblack does not model
gamesave::WorldRead KeptWorld()
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.players[0] = 2;
	kept.objects.emplace_back(game);
	gamesave::PlayerRecord player;
	player.thing.extra = 3;
	player.playerNumber = 3;
	player.interfaces.assign(18, 0);
	player.interfaces[0] = 3;
	kept.objects.emplace_back(player);
	kept.objects.emplace_back(PlayerInterfaceRecord {.thing = {.extra = 3}, .status = {4}});
	InterfaceStatusRecord status;
	status.thing.extra = 3;
	status.playerNumber = 3;
	status.hand.held.numbers = {0};
	status.cameraPosition = {1.0f, 2.0f, 3.0f};
	status.playerInterface = {3};
	status.leashStatus = {5};
	status.fifthWord = 7;
	kept.objects.emplace_back(status);
	kept.objects.emplace_back(LeashStatusRecord {
	    .thing = {.extra = 3}, .works = 0, .type = 3, .playerNumber = 3, .appliedForce = 6, .length = 9, .status = {4}});
	return kept;
}

/// The game object a new game's writer makes: it names player 2's interface in its first list
void RegisterNewGame(ClassWriters& writers)
{
	writers.Register(
	    gamesave::k_GameType, {}, [](WriteContext& context, entt::entity /*unused*/) -> std::optional<SavedRecord> {
		    gamesave::GameHead game;
		    game.lists[0].numbers = {context.Reference(SourceKey {.type = PlayerInterfaceRecord::k_Type, .id = 2})};
		    return game;
	    });
}

/// The file a new game's writer and the players' writers make: the game object, player 2's interface, its status
/// made as its constructor leaves it, the leash given, and the records after it
std::vector<uint8_t> NewGameFile(const LeashStatusRecord& leash, const std::map<ObjectNumber, SavedRecord>& after = {})
{
	gamesave::GameHead game;
	game.lists[0].numbers = {2};
	InterfaceStatusRecord status;
	status.thing.extra = 2;
	status.playerNumber = 2;
	status.hand.held.numbers = {0};
	status.playerInterface = {2};
	status.leashStatus = {4};
	status.fifthWord = 0xFFFFFFFF;
	std::map<ObjectNumber, SavedRecord> records {
	    {1, game}, {2, PlayerInterfaceRecord {.thing = {.extra = 2}, .status = {3}}}, {3, status}, {4, leash}};
	records.insert(after.begin(), after.end());
	return FileOf(records);
}

/// The computer players' service as a load and a save use it: what a load puts back of each player is kept, and its
/// mind is the one a save writes
class FakeComputerPlayers final: public ecs::systems::ComputerPlayerSystemInterface
{
public:
	void SwitchOn(PlayerNames /*player*/, bool /*on*/) override {}
	void SetCreatureLike(PlayerNames /*player*/, PlayerNames /*like*/) override {}
	void ProcessTurn() override {}
	[[nodiscard]] const ecs::components::ComputerMind* SavedMind(PlayerNames player) const override
	{
		const auto found = minds.find(player);
		return found != minds.end() ? &found->second : nullptr;
	}
	void Restore(PlayerNames player, const ecs::components::ComputerPlayer& state,
	             const ecs::components::ComputerMind& mind) override
	{
		states[player] = state;
		minds[player] = mind;
	}

	std::map<PlayerNames, ecs::components::ComputerPlayer> states;
	std::map<PlayerNames, ecs::components::ComputerMind> minds;
};

/// A computer player as every player's is made: its speed 90, its seen range 30, the most it keeps seen 50, the whole
/// game's word at -1, no icon counted, no creature-like player, its tree as a new mind's
ComputerPlayerRecord NewComputerPlayer(uint32_t player, ObjectNumber actionState, ObjectNumber firstAttitude)
{
	ComputerPlayerRecord made;
	made.thing.extra = player;
	made.actionState = actionState;
	made.words = {std::bit_cast<uint32_t>(90.0f), std::bit_cast<uint32_t>(30.0f), 50, 0};
	made.gameWord = 0xFFFFFFFF;
	made.table.assign(42, 0);
	made.byte = 0xFF;
	const auto mind = computer_player::MakeMind();
	for (size_t node = 0; node < made.desires.size(); ++node)
	{
		made.desires.at(node) = {.suppression = mind.nodes.at(node).suppression,
		                         .personality = mind.nodes.at(node).personality};
	}
	for (ObjectNumber toward = 0; toward < 8; ++toward)
	{
		made.attitudes.push_back(firstAttitude + toward);
	}
	return made;
}

/// The state of a computer player's hand as it is made: in the middle of the map, asking for a move first, at the
/// computer player's speed
ActionStateRecord NewActionState(uint32_t player, ObjectNumber computerPlayer)
{
	ActionStateRecord made;
	made.thing.extra = player;
	made.position.position = {.x = (128 << 16) | 0x8000, .z = (128 << 16) | 0x8000, .altitude = 0.0f};
	made.computerPlayer = computerPlayer;
	made.words = {1, 0, 0, 0, 0, 0, 1, player};
	made.first = 90.0f;
	return made;
}

/// The game object a new game's writer makes: it names player 1's computer player in its first list
void RegisterNewGameComputerPlayer(ClassWriters& writers)
{
	writers.Register(gamesave::k_GameType, {},
	                 [](WriteContext& context, entt::entity /*unused*/) -> std::optional<SavedRecord> {
		                 gamesave::GameHead game;
		                 game.lists[0].numbers = {context.Reference(SourceKey {.type = ComputerPlayerRecord::k_Type, .id = 1})};
		                 return game;
	                 });
}

/// A file with the game object naming the object 2 in its first list, then the records given
std::vector<uint8_t> ListedFile(std::map<ObjectNumber, SavedRecord> records)
{
	gamesave::GameHead game;
	game.lists[0].numbers = {2};
	records.emplace(1, game);
	return FileOf(records);
}

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / relative;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	std::ifstream file(path, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// A loaded player's first interface, its status and its leash are kept by the player's number, and written by their
// writers: each names the others by the numbers the file gives them, and what openblack does not model stays as loaded
TEST(PlayerInterfaceWrite, ALoadedPlayersInterfaceStatusAndLeashAreWrittenByThePlayersNumber)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const auto kept = KeptWorld();
	LoadedObjects loaded;
	KeepPlayerInterfaces(kept, loaded);
	ASSERT_EQ(loaded.size(), 3u);
	EXPECT_EQ(loaded.at(3).type, PlayerInterfaceRecord::k_Type);
	EXPECT_EQ(loaded.at(4).type, InterfaceStatusRecord::k_Type);
	EXPECT_EQ(loaded.at(5).type, LeashStatusRecord::k_Type);
	for (const auto& [number, object] : loaded)
	{
		EXPECT_EQ(object.id, std::optional<uint64_t>(3)) << number;
	}

	ecs::Registry registry;
	ClassWriters writers;
	RegisterPlayerInterfaceWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));
	const std::vector<WrittenRecord> order {
	    {.number = 1, .type = gamesave::k_GameType, .source = RecordSource::KeptWorld},
	    {.number = 2, .type = gamesave::k_PlayerType, .source = RecordSource::KeptWorld},
	    {.number = 3, .type = PlayerInterfaceRecord::k_Type, .source = RecordSource::Writer, .id = 3},
	    {.number = 4, .type = InterfaceStatusRecord::k_Type, .source = RecordSource::Writer, .id = 3},
	    {.number = 5, .type = LeashStatusRecord::k_Type, .source = RecordSource::Writer, .id = 3},
	};
	EXPECT_EQ(written.records, order);
}

// An interface made since the load (a new game's) is written as the original's constructors leave it: the status's
// hand holding nothing and its word after the leash at -1, the leash off, working and the learning leash
TEST(PlayerInterfaceWrite, AnInterfaceMadeSinceTheLoadIsWrittenAsItsConstructorsLeaveIt)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	ecs::Registry registry;
	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterPlayerInterfaceWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, NewGameFile({.thing = {.extra = 2}, .works = 1, .type = 2, .playerNumber = 2, .status = {3}}));
}

// The leash's status says what the leash of the creature the player leads says: whether its player holds it, whether
// it works, its type and what it is tied to; with no leash on, the leash picked and whether a leash put on would work
TEST(PlayerInterfaceWrite, TheLeashIsWrittenFromTheLeashOfTheCreatureThePlayerLeads)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	test::creature_loop_fakes::CallLog log;
	auto& leashes = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	ecs::Registry registry;
	const auto creature = registry.Create();
	const auto post = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(post, gamesave::WaypointRecord {});
	auto& leash = registry.Assign<ecs::components::CreatureLeash>(creature);
	leash.worn.emplace();
	leash.worn->type = LeashType::Evil;
	leash.worn->holder = PlayerNames::PLAYER_THREE;
	leash.worn->works = false;
	leash.worn->tiedTo = post;
	leashes.playersCreature = creature;

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterPlayerInterfaceWriters(writers);
	// on and tied: what it is tied to is the next object of the file, after the leash
	const auto tied = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(tied.Complete()) << tied.failure;
	EXPECT_EQ(
	    tied.bytes,
	    NewGameFile(
	        {.thing = {.extra = 2}, .activated = 1, .works = 0, .type = 1, .playerNumber = 2, .attachedTo = {5}, .status = {3}},
	        {{5, gamesave::WaypointRecord {}}}));

	// no leash on: the leash picked, and whether one put on works
	leash.worn.reset();
	leash.selected = LeashType::Good;
	leash.leashWorks = false;
	const auto off = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(off.Complete()) << off.failure;
	EXPECT_EQ(off.bytes, NewGameFile({.thing = {.extra = 2}, .works = 0, .type = 3, .playerNumber = 2, .status = {3}}));
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's eight interfaces, statuses and leashes are kept by their players' numbers and written by their writers,
// each record the one read, and the file is the original's byte for byte
TEST(PlayerInterfaceWrite, TheFrozenLandOneAutosavesInterfacesAreWrittenByTheirWritersAndTheFileStaysWhole)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::reset();
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());

	LoadedObjects loaded;
	KeepLoadedSources(world, loaded);
	std::map<uint32_t, size_t> kinds;
	for (const auto& entry : loaded)
	{
		++kinds[entry.second.type];
	}
	EXPECT_EQ(kinds[PlayerInterfaceRecord::k_Type], 8u);
	EXPECT_EQ(kinds[InterfaceStatusRecord::k_Type], 8u);
	EXPECT_EQ(kinds[LeashStatusRecord::k_Type], 8u);

	ecs::Registry registry;
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	RegisterPlayerInterfaceWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// each record of the three classes is the writer's, and the one read
	const auto writtenRead = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(writtenRead.stop, gamesave::k_ReadToTheEnd);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type != PlayerInterfaceRecord::k_Type && record.type != InterfaceStatusRecord::k_Type &&
		    record.type != LeashStatusRecord::k_Type)
		{
			continue;
		}
		SCOPED_TRACE(record.number);
		EXPECT_EQ(record.source, RecordSource::Writer);
		byWriter += record.source == RecordSource::Writer ? 1 : 0;
		ASSERT_LE(record.number, world.objects.size());
		ASSERT_LE(record.number, writtenRead.objects.size());
		EXPECT_TRUE(writtenRead.objects[record.number - 1] == world.objects[record.number - 1]);
	}
	EXPECT_EQ(byWriter, 24u);

	// the loads' counter is 0, as the game's save leaves it; every other byte is the original's
	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
}

// A loaded computer player, its hand's state and its eight attitudes are kept by the player's number; the load puts back
// its switches and its mind (the icons counted, the tree's nodes and the attitudes), and the save writes them from there
TEST(ComputerPlayerWrite, ALoadedComputerPlayerComesBackAndIsWrittenFromWhatTheLoadPutBack)
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.players[0] = 2;
	kept.objects.emplace_back(game);
	gamesave::PlayerRecord player;
	player.thing.extra = 3;
	player.playerNumber = 3;
	player.computerPlayer = 3;
	kept.objects.emplace_back(player);
	auto computer = NewComputerPlayer(3, 4, 5);
	computer.player = 2;
	computer.numbers = {1, 1, 0};
	computer.words[3] = 1;
	computer.byte = 5;
	computer.table.at(7) = 2;
	computer.tableWord = 6;
	computer.desires.at(3) = {.suppression = 0.25f, .personality = 0.5f};
	kept.objects.emplace_back(computer);
	auto state = NewActionState(3, 3);
	state.first = 45.0f;
	state.block = {1, 2, 3};
	kept.objects.emplace_back(state);
	for (size_t toward = 0; toward < 8; ++toward)
	{
		kept.objects.emplace_back(ComputerAttitudeRecord {.attitude = toward == 2 ? 0.5f : 1.0f});
	}

	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	auto& computers = static_cast<FakeComputerPlayers&>(Locator::computerPlayerSystem::emplace<FakeComputerPlayers>());
	LoadedObjects loaded;
	LoadComputerPlayers(kept, loaded);
	ASSERT_EQ(loaded.size(), 10u);
	EXPECT_EQ(loaded.at(3).type, ComputerPlayerRecord::k_Type);
	EXPECT_EQ(loaded.at(3).id, std::optional<uint64_t>(3));
	EXPECT_EQ(loaded.at(4).type, ActionStateRecord::k_Type);
	EXPECT_EQ(loaded.at(4).id, std::optional<uint64_t>(3));
	EXPECT_EQ(loaded.at(7).type, ComputerAttitudeRecord::k_Type);
	EXPECT_EQ(loaded.at(7).id, std::optional<uint64_t>(3 * 8 + 2));

	// what the load put back
	ASSERT_EQ(computers.states.count(PlayerNames::PLAYER_FOUR), 1u);
	const auto restored = computers.states.at(PlayerNames::PLAYER_FOUR);
	EXPECT_TRUE(restored.switchedOn);
	EXPECT_TRUE(restored.started);
	EXPECT_TRUE(restored.paused);
	EXPECT_EQ(restored.creatureLike, std::optional(PlayerNames::PLAYER_SIX));
	const auto& mind = computers.minds.at(PlayerNames::PLAYER_FOUR);
	EXPECT_EQ(mind.iconsFound.at(7), 2u);
	EXPECT_EQ(mind.nodes.at(3).suppression, 0.25f);
	EXPECT_EQ(mind.nodes.at(3).personality, 0.5f);
	EXPECT_EQ(mind.attitude.at(2), 0.5f);
	EXPECT_EQ(mind.attitude.at(1), 1.0f);

	// the player's entity has the switches, as the computer players' service puts them there
	ecs::Registry registry;
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Player>(entity, PlayerNames::PLAYER_FOUR);
	registry.Assign<ecs::components::ComputerPlayer>(entity, restored);
	ClassWriters writers;
	RegisterComputerPlayerWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		byWriter += record.source == RecordSource::Writer ? 1 : 0;
	}
	EXPECT_EQ(byWriter, 10u);
}

// A computer player made since the load (a new game's) is written as every player's is made: its player first, then
// its hand's state and its eight attitudes after it
TEST(ComputerPlayerWrite, AComputerPlayerMadeSinceTheLoadIsWrittenAsEveryPlayersIsMade)
{
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::reset();
	ecs::Registry registry;
	ClassWriters writers;
	RegisterNewGameComputerPlayer(writers);
	RegisterComputerPlayerWriters(writers);
	// the players are a source's objects, by their numbers, as the players' writer registers them
	writers.RegisterSource(
	    gamesave::k_PlayerType, [](const ecs::Registry& /*from*/) { return std::vector<uint64_t> {1}; },
	    [](WriteContext& /*context*/, uint64_t /*id*/) -> std::optional<SavedRecord> {
		    return gamesave::PlayerRecord {.playerNumber = 1};
	    });
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;
	auto computer = NewComputerPlayer(1, 4, 5);
	computer.player = 3;
	std::map<ObjectNumber, SavedRecord> records {
	    {2, computer}, {3, gamesave::PlayerRecord {.playerNumber = 1}}, {4, NewActionState(1, 2)}};
	for (ObjectNumber toward = 0; toward < 8; ++toward)
	{
		records.emplace(5 + toward, ComputerAttitudeRecord {});
	}
	EXPECT_EQ(written.bytes, ListedFile(records));
}

// The computer player's switches, its hand's and its mind's values come from the player's entity: whether it is on, has
// started, is paused, the player its creature is like, whether a script holds its hand, the player's speed and the
// steps' speed, the icons its mind counted, its tree's nodes and its attitudes; its player is the players' source of
// that number
TEST(ComputerPlayerWrite, TheSwitchesTheHandAndTheMindAreWrittenFromThePlayersEntity)
{
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::reset();
	ecs::Registry registry;
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Player>(entity, PlayerNames::PLAYER_TWO);
	registry.Assign<ecs::components::ComputerPlayer>(entity, ecs::components::ComputerPlayer {
	                                                             .switchedOn = true,
	                                                             .started = true,
	                                                             .paused = false,
	                                                             .creatureLike = PlayerNames::PLAYER_ONE,
	                                                         });
	auto& hand = registry.Assign<ecs::components::ComputerHand>(entity);
	hand.held = true;
	hand.playerSpeed = 45.0f;
	hand.speed = 60.0f;
	auto mind = computer_player::MakeMind();
	mind.attitude.at(0) = 0.25f;
	mind.nodes.at(0).suppression = 0.5f;
	mind.iconsFound.at(3) = 4;
	registry.Assign<ecs::components::ComputerMind>(entity, mind);

	ClassWriters writers;
	RegisterNewGameComputerPlayer(writers);
	RegisterComputerPlayerWriters(writers);
	// the players are a source's objects, by their numbers: the player's record is named first in the computer player's
	writers.RegisterSource(
	    gamesave::k_PlayerType, [](const ecs::Registry& /*from*/) { return std::vector<uint64_t> {1}; },
	    [](WriteContext& /*context*/, uint64_t /*id*/) -> std::optional<SavedRecord> {
		    return gamesave::PlayerRecord {.playerNumber = 1};
	    });
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;

	auto computer = NewComputerPlayer(1, 4, 5);
	computer.player = 3;
	computer.numbers = {1, 0, 1};
	computer.words[0] = std::bit_cast<uint32_t>(45.0f);
	computer.words[3] = 1;
	computer.byte = 0;
	computer.table.at(3) = 4;
	computer.desires.at(0).suppression = 0.5f;
	auto state = NewActionState(1, 2);
	state.first = 60.0f;
	std::map<ObjectNumber, SavedRecord> records {{2, computer}, {3, gamesave::PlayerRecord {.playerNumber = 1}}, {4, state}};
	for (ObjectNumber toward = 0; toward < 8; ++toward)
	{
		records.emplace(5 + toward, ComputerAttitudeRecord {.attitude = toward == 0 ? 0.25f : 1.0f});
	}
	EXPECT_EQ(written.bytes, ListedFile(records));
}

// The computer players' service puts a loaded mind on its player's entity's mind when it has one, else keeps it for the
// entity to take up, as a mind kept from the land before; the switches go on the player's entity, and a player whose
// switches are a new one's gets no computer player of its own
TEST(ComputerPlayerWrite, TheServicePutsALoadedComputerPlayerWhereItsPlayerTakesItUp)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto two = registry.Create();
	registry.Assign<ecs::components::Player>(two, PlayerNames::PLAYER_TWO);
	const auto four = registry.Create();
	registry.Assign<ecs::components::Player>(four, PlayerNames::PLAYER_FOUR);
	registry.Assign<ecs::components::ComputerMind>(four, computer_player::MakeMind());
	ecs::systems::ComputerPlayerSystem computers;
	EXPECT_EQ(computers.SavedMind(PlayerNames::PLAYER_TWO), nullptr);

	auto mind = computer_player::MakeMind();
	mind.attitude.at(1) = 0.5f;
	mind.nodes.at(5).suppression = 0.25f;
	computers.Restore(PlayerNames::PLAYER_TWO, {.switchedOn = true, .started = true}, mind);
	ASSERT_NE(computers.SavedMind(PlayerNames::PLAYER_TWO), nullptr);
	EXPECT_EQ(computers.SavedMind(PlayerNames::PLAYER_TWO)->attitude.at(1), 0.5f);
	EXPECT_EQ(computers.SavedMind(PlayerNames::PLAYER_TWO)->nodes.at(5).suppression, 0.25f);
	EXPECT_FALSE(registry.AllOf<ecs::components::ComputerMind>(two));
	ASSERT_TRUE(registry.AllOf<ecs::components::ComputerPlayer>(two));
	EXPECT_TRUE(registry.Get<ecs::components::ComputerPlayer>(two).switchedOn);
	EXPECT_TRUE(registry.Get<ecs::components::ComputerPlayer>(two).started);

	// a player with a mind of its own takes the loaded one at once; switches as a new one's make no computer player
	computers.Restore(PlayerNames::PLAYER_FOUR, {}, mind);
	EXPECT_EQ(registry.Get<ecs::components::ComputerMind>(four).attitude.at(1), 0.5f);
	EXPECT_FALSE(registry.AllOf<ecs::components::ComputerPlayer>(four));
	// a player with no entity on the land keeps its mind for it, and none is made
	computers.Restore(PlayerNames::PLAYER_SIX, {}, mind);
	ASSERT_NE(computers.SavedMind(PlayerNames::PLAYER_SIX), nullptr);
	size_t players = 0;
	registry.Each<const ecs::components::Player>(
	    [&players](entt::entity /*entity*/, const ecs::components::Player& /*player*/) { ++players; });
	EXPECT_EQ(players, 2u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's eight computer players, their hands' states and their 64 attitudes are kept by their players' numbers,
// come back through the computer players' service and are written from there by their writers, each record the one
// read, and the file is the original's byte for byte
TEST(ComputerPlayerWrite, TheFrozenLandOneAutosavesComputerPlayersComeBackAndAreWrittenByTheirWriters)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	auto& computers = static_cast<FakeComputerPlayers&>(Locator::computerPlayerSystem::emplace<FakeComputerPlayers>());
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());

	LoadedObjects loaded;
	KeepLoadedSources(world, loaded);
	EXPECT_EQ(computers.minds.size(), 8u);
	std::map<uint32_t, size_t> kinds;
	for (const auto& entry : loaded)
	{
		++kinds[entry.second.type];
	}
	EXPECT_EQ(kinds[ComputerPlayerRecord::k_Type], 8u);
	EXPECT_EQ(kinds[ActionStateRecord::k_Type], 8u);
	EXPECT_EQ(kinds[ComputerAttitudeRecord::k_Type], 64u);

	ecs::Registry registry;
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	RegisterPlayerInterfaceWriters(writers);
	RegisterComputerPlayerWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;

	const auto writtenRead = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(writtenRead.stop, gamesave::k_ReadToTheEnd);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type != ComputerPlayerRecord::k_Type && record.type != ActionStateRecord::k_Type &&
		    record.type != ComputerAttitudeRecord::k_Type)
		{
			continue;
		}
		SCOPED_TRACE(record.number);
		EXPECT_EQ(record.source, RecordSource::Writer);
		byWriter += record.source == RecordSource::Writer ? 1 : 0;
		ASSERT_LE(record.number, world.objects.size());
		ASSERT_LE(record.number, writtenRead.objects.size());
		EXPECT_TRUE(writtenRead.objects[record.number - 1] == world.objects[record.number - 1]);
	}
	EXPECT_EQ(byWriter, 80u);

	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
}

// A hand's steps and their arguments are written from the hand, after its state: each step's kind and its arguments,
// each argument's kind, thing, number, amount and point, its bytes zero for one made in play
TEST(ComputerPlayerWrite, AHandsStepsAndTheirArgumentsAreWrittenFromTheHand)
{
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::reset();
	ecs::Registry registry;
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Player>(entity, PlayerNames::PLAYER_TWO);
	const auto post = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(post, gamesave::WaypointRecord {});
	auto& hand = registry.Assign<ecs::components::ComputerHand>(entity);
	using ecs::components::HandArgument;
	using ecs::components::HandSubAction;
	ecs::components::HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = post});
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = 150.0f});
	hand.steps.push_back(pickUp);
	ecs::components::HandStep move {.type = HandSubAction::MoveHandToPos, .arguments = {}};
	move.arguments.push_back({.kind = HandArgument::Kind::Point, .point = glm::vec3(1.0f, 2.0f, 3.0f)});
	hand.steps.push_back(move);

	ClassWriters writers;
	RegisterNewGameComputerPlayer(writers);
	RegisterComputerPlayerWriters(writers);
	// the players are a source's objects, by their numbers, as the players' writer registers them
	writers.RegisterSource(
	    gamesave::k_PlayerType, [](const ecs::Registry& /*from*/) { return std::vector<uint64_t> {1}; },
	    [](WriteContext& /*context*/, uint64_t /*id*/) -> std::optional<SavedRecord> {
		    return gamesave::PlayerRecord {.playerNumber = 1};
	    });
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// the computer player, its player, its hand's state with its two steps and their arguments (the first one's thing
	// after it), then its eight attitudes
	auto computer = NewComputerPlayer(1, 4, 11);
	computer.player = 3;
	auto state = NewActionState(1, 2);
	state.objects = {5, 9};
	const auto argument = [](uint32_t kind, ObjectNumber thing, float amount, gamesave::Point point) {
		gamesave::PlayerSubActionArgumentRecord made;
		made.word = kind;
		made.object = {thing};
		made.bytes.values.assign(64, 0);
		made.thirdWord = std::bit_cast<uint32_t>(amount);
		made.point = point;
		return made;
	};
	std::map<ObjectNumber, SavedRecord> records {
	    {2, computer},
	    {3, gamesave::PlayerRecord {.playerNumber = 1}},
	    {4, state},
	    {5, gamesave::PlayerSubActionRecord {.word = 0, .arguments = {.numbers = {6, 8}}}},
	    {6, argument(0, 7, 0.0f, {})},
	    {7, gamesave::WaypointRecord {}},
	    {8, argument(5, 0, 150.0f, {})},
	    {9, gamesave::PlayerSubActionRecord {.word = 1, .arguments = {.numbers = {10}}}},
	    {10, argument(1, 0, 0.0f, {1.0f, 2.0f, 3.0f})},
	};
	for (ObjectNumber toward = 0; toward < 8; ++toward)
	{
		records.emplace(11 + toward, ComputerAttitudeRecord {});
	}
	EXPECT_EQ(written.bytes, ListedFile(records));
}

// What the mind has seen comes back with the load, each thing as the load made it (one it made nothing of with its
// turn alone), and is written from the mind, the newest first: a thing the load made by its number, one it did not as
// the file had it
TEST(ComputerPlayerWrite, WhatTheMindHasSeenComesBackWithALoadAndIsWrittenFromTheMind)
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.players[0] = 2;
	kept.objects.emplace_back(game);
	gamesave::PlayerRecord player;
	player.playerNumber = 3;
	player.computerPlayer = 3;
	kept.objects.emplace_back(player);
	auto computer = NewComputerPlayer(3, 4, 9);
	computer.player = 2;
	computer.lists[1] = {5, 7};
	kept.objects.emplace_back(computer);
	kept.objects.emplace_back(NewActionState(3, 3));
	kept.objects.emplace_back(gamesave::ComputerSeenRecord {.object = {6}, .word = 500});
	gamesave::WaypointRecord tree;
	tree.position.position = {.x = 0x10000, .z = 0x20000, .altitude = 1.0f};
	kept.objects.emplace_back(tree);
	kept.objects.emplace_back(gamesave::ComputerSeenRecord {.object = {8}, .word = 7});
	kept.objects.emplace_back(gamesave::WaypointRecord {});
	for (size_t toward = 0; toward < 8; ++toward)
	{
		kept.objects.emplace_back(ComputerAttitudeRecord {});
	}

	// the load made the first thing seen (as the tree its maker made)
	ecs::Registry registry;
	const auto made = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(made, tree);
	LoadedObjects loaded {{6, {.entity = made, .type = gamesave::WaypointRecord::k_Type}}};
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	auto& computers = static_cast<FakeComputerPlayers&>(Locator::computerPlayerSystem::emplace<FakeComputerPlayers>());
	LoadComputerPlayers(kept, loaded);
	EXPECT_EQ(loaded.at(5).type, gamesave::ComputerSeenRecord::k_Type);
	EXPECT_EQ(loaded.at(7).type, gamesave::ComputerSeenRecord::k_Type);
	EXPECT_NE(loaded.at(5).id, loaded.at(7).id);
	const auto& mind = computers.minds.at(PlayerNames::PLAYER_FOUR);
	ASSERT_EQ(mind.seen.size(), 2u);
	EXPECT_TRUE(mind.seen[0].thing == made);
	EXPECT_EQ(mind.seen[0].turn, 500u);
	EXPECT_FALSE(mind.seen[0].forest.has_value());
	EXPECT_TRUE(mind.seen[1].thing == entt::null);
	EXPECT_EQ(mind.seen[1].turn, 7u);

	ClassWriters writers;
	RegisterComputerPlayerWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));

	// a turn the mind keeps differently is the one written
	computers.minds.at(PlayerNames::PLAYER_FOUR).seen[0].turn = 600;
	const auto again =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(again.Complete()) << again.failure;
	const auto read = gamesave::ReadWorld(again.bytes);
	ASSERT_GE(read.objects.size(), 5u);
	ASSERT_TRUE(read.objects[4].has_value());
	const auto* first = std::get_if<gamesave::ComputerSeenRecord>(&*read.objects[4]);
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(first->word, 600u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. A frozen save whose
// computer players' hands have steps (profile D's Land 2 autosave, slot 15): its six steps and nine arguments are kept
// by their players and places and written by their writers, each record the one read, and the file is the original's
// byte for byte
TEST(ComputerPlayerWrite, AFrozenSavesHandStepsAreWrittenByTheirWritersAndTheFileStaysWhole)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "15" / "2" / "15.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen profile D";
	}
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::emplace<FakeComputerPlayers>();
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);

	LoadedObjects loaded;
	KeepLoadedSources(world, loaded);
	ecs::Registry registry;
	ClassWriters writers;
	RegisterPlayerInterfaceWriters(writers);
	RegisterComputerPlayerWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;

	std::map<uint32_t, size_t> byWriter;
	for (const auto& record : written.records)
	{
		if (record.source == RecordSource::Writer)
		{
			++byWriter[record.type];
		}
	}
	EXPECT_EQ(byWriter[gamesave::PlayerSubActionRecord::k_Type], 6u);
	EXPECT_EQ(byWriter[gamesave::PlayerSubActionArgumentRecord::k_Type], 9u);
	EXPECT_EQ(byWriter[ComputerPlayerRecord::k_Type], 8u);

	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
}
