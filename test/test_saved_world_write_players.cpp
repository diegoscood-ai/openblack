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
#include <tuple>
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
#include "ECS/SavedWorld/LoadLeashStatus.h"
#include "ECS/SavedWorld/LoadPlayerInterface.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
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
using gamesave::PlayerSubActionArgumentRecord;
using gamesave::PlayerSubActionRecord;
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

// A kept status whose hand holds an object gone since the load holds nothing, with the hand's count and the holding count
// at 0, and a kept leash tied to an object gone since is tied to nothing, as the original's hand and leash checks let go
// of a deleted object each turn; with both objects still there, both keep their numbers and counts
TEST(PlayerInterfaceWrite, AKeptHandAndLeashLetGoOfAnObjectGoneSinceTheLoad)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::handSystem> restoreHand;
	Locator::handSystem::reset();
	auto kept = KeptWorld();
	auto& status = std::get<InterfaceStatusRecord>(*kept.objects[3]);
	status.hand.byte = 1;
	status.hand.held.numbers = {6};
	status.handHoldingSomething = 1;
	std::get<LeashStatusRecord>(*kept.objects[4]).attachedTo = {7};
	ecs::Registry registry;
	ClassWriters writers;
	RegisterPlayerInterfaceWriters(writers);
	for (const bool gone : {true, false})
	{
		const auto held = registry.Create();
		const auto tied = registry.Create();
		ASSERT_TRUE(held != entt::null);
		ASSERT_TRUE(tied != entt::null);
		LoadedObjects loaded;
		KeepPlayerInterfaces(kept, loaded);
		loaded.emplace(6, LoadedObject {.entity = held, .type = gamesave::VillagerRecord::k_Type});
		loaded.emplace(7, LoadedObject {.entity = tied, .type = gamesave::VillagerRecord::k_Type});
		if (gone)
		{
			registry.Destroy(held);
			registry.Destroy(tied);
		}
		const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};
		WriteContext context(writers, source);
		const auto writtenStatus = (*writers.SourceWriterOf(InterfaceStatusRecord::k_Type))(context, 3);
		ASSERT_TRUE(writtenStatus.has_value()) << context.Failure();
		const auto& hand = std::get<InterfaceStatusRecord>(*writtenStatus);
		EXPECT_EQ(hand.hand.held.numbers, (std::vector<ObjectNumber> {gone ? 0u : 6u})) << gone;
		EXPECT_EQ(hand.hand.byte, gone ? 0u : 1u) << gone;
		EXPECT_EQ(hand.handHoldingSomething, gone ? 0u : 1u) << gone;
		EXPECT_EQ(hand.fifthWord, 7u) << gone;
		const auto writtenLeash = (*writers.SourceWriterOf(LeashStatusRecord::k_Type))(context, 3);
		ASSERT_TRUE(writtenLeash.has_value()) << context.Failure();
		const auto& leash = std::get<LeashStatusRecord>(*writtenLeash);
		EXPECT_EQ(leash.attachedTo.number, gone ? 0u : 7u) << gone;
		EXPECT_EQ(leash.type, 3u) << gone;
		EXPECT_EQ(leash.length, 9u) << gone;
	}
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
	EXPECT_EQ(tied.bytes, NewGameFile({.thing = {.extra = 2},
	                                   .activated = 1,
	                                   .works = 0,
	                                   .type = 1,
	                                   .playerNumber = 2,
	                                   .attachedTo = {5},
	                                   .fifthWord = std::bit_cast<uint32_t>(leash.worn->rope.slackLength),
	                                   .length = std::bit_cast<uint32_t>(leash.worn->rope.maxLength),
	                                   .status = {3}},
	                                  {{5, gamesave::WaypointRecord {}}}));

	// no leash on: the leash picked, and whether one put on works
	leash.worn.reset();
	leash.selected = LeashType::Good;
	leash.leashWorks = false;
	const auto off = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(off.Complete()) << off.failure;
	EXPECT_EQ(off.bytes, NewGameFile({.thing = {.extra = 2}, .works = 0, .type = 3, .playerNumber = 2, .status = {3}}));
}

// A leash's status that is on puts the leash back on in its player's hand as it was: its type (also the one picked),
// whether it works, what it is tied to and the turn of that tie, and its two lengths, with the rope laid afresh
TEST(LeashStatusLoad, AStatusThatIsOnPutsTheLeashBackTiedWithItsLengths)
{
	ecs::components::CreatureLeash leash;
	const auto post = static_cast<entt::entity>(7);
	const LeashStatusRecord record {.activated = 1,
	                                .works = 0,
	                                .type = 3,
	                                .playerNumber = 1,
	                                .attachedTo = {40},
	                                .fifthWord = std::bit_cast<uint32_t>(90.0f),
	                                .length = std::bit_cast<uint32_t>(180.0f),
	                                .lastWord = 33123};
	EXPECT_EQ(ecs::saved_leash::Restore(leash, record, PlayerNames::PLAYER_TWO, post), ecs::saved_leash::Restored::Done);
	ASSERT_TRUE(leash.worn.has_value());
	EXPECT_EQ(leash.worn->type, LeashType::Good);
	EXPECT_EQ(leash.worn->holder, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(leash.worn->works);
	EXPECT_EQ(leash.worn->tiedTo, std::optional(post));
	EXPECT_EQ(leash.worn->tiedTurn, 33123u);
	EXPECT_EQ(leash.worn->rope.slackLength, 90.0f);
	EXPECT_EQ(leash.worn->rope.maxLength, 180.0f);
	EXPECT_FALSE(leash.worn->ropeStarted);
	EXPECT_EQ(leash.selected, LeashType::Good);
	EXPECT_FALSE(leash.leashWorks);
	EXPECT_EQ(leash.lastTiedTurn, 33123u);
}

// A status that is off takes any leash off and keeps the player's switch, the leash picked and the last tie's turn;
// one on in the hand is untied; one tied to an object the load did not make leaves the leash as it was
TEST(LeashStatusLoad, AStatusThatIsOffOrInTheHandOrTiedToNothingMade)
{
	ecs::components::CreatureLeash leash;
	leash.worn.emplace();
	const LeashStatusRecord off {.works = 1, .type = 1, .lastWord = 21202};
	EXPECT_EQ(ecs::saved_leash::Restore(leash, off, PlayerNames::PLAYER_ONE, std::nullopt), ecs::saved_leash::Restored::Done);
	EXPECT_FALSE(leash.worn.has_value());
	EXPECT_TRUE(leash.leashWorks);
	EXPECT_EQ(leash.selected, LeashType::Evil);
	EXPECT_EQ(leash.lastTiedTurn, 21202u);

	const LeashStatusRecord inHand {.activated = 1,
	                                .works = 1,
	                                .type = 2,
	                                .fifthWord = std::bit_cast<uint32_t>(43.0f),
	                                .length = std::bit_cast<uint32_t>(122.0f)};
	EXPECT_EQ(ecs::saved_leash::Restore(leash, inHand, PlayerNames::PLAYER_ONE, std::nullopt),
	          ecs::saved_leash::Restored::Done);
	ASSERT_TRUE(leash.worn.has_value());
	EXPECT_FALSE(leash.worn->tiedTo.has_value());
	EXPECT_EQ(leash.worn->tiedTurn, 0u);
	EXPECT_EQ(leash.worn->rope.maxLength, 122.0f);

	const LeashStatusRecord tied {.activated = 1, .works = 1, .type = 3, .attachedTo = {9}};
	EXPECT_EQ(ecs::saved_leash::Restore(leash, tied, PlayerNames::PLAYER_ONE, std::nullopt),
	          ecs::saved_leash::Restored::TiedToNothingMade);
	EXPECT_EQ(leash.worn->type, LeashType::Rope);
	EXPECT_EQ(leash.selected, LeashType::Rope);
}

// The load finds each loaded status's player's creature and what its leash is tied to among the loaded objects, and a
// save straight after writes the status the file had
TEST(LeashStatusLoad, ALoadedStatusComesBackOnThePlayersCreatureAndIsWrittenAsLoaded)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	test::creature_loop_fakes::CallLog log;
	auto& leashes = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	ecs::Registry registry;
	const auto creature = registry.Create();
	const auto post = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(post, gamesave::WaypointRecord {});
	registry.Assign<ecs::components::CreatureLeash>(creature);
	leashes.playersCreature = creature;

	auto kept = KeptWorld();
	auto& status = std::get<LeashStatusRecord>(*kept.objects[4]);
	status.activated = 1;
	status.attachedTo = {6};
	status.fifthWord = std::bit_cast<uint32_t>(20.95f);
	status.length = std::bit_cast<uint32_t>(41.9f);
	status.lastWord = 140666;
	kept.objects.emplace_back(gamesave::WaypointRecord {});
	LoadedObjects loaded;
	KeepPlayerInterfaces(kept, loaded);
	loaded.emplace(6, LoadedObject {.entity = post, .type = gamesave::WaypointRecord::k_Type});

	ecs::saved_leash::Load(registry, leashes, kept, loaded);
	const auto& leash = registry.Get<ecs::components::CreatureLeash>(creature);
	ASSERT_TRUE(leash.worn.has_value());
	EXPECT_EQ(leash.worn->tiedTo, std::optional(post));
	EXPECT_EQ(leash.worn->holder, PlayerNames::PLAYER_FOUR);
	EXPECT_EQ(leash.worn->type, LeashType::Good);

	ClassWriters writers;
	RegisterPlayerInterfaceWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));
}

// The load's end switches each loaded player's leash on or off again as its status says, on the creature the player
// leads; a player who leads none has nothing switched
TEST(LeashStatusLoad, TheLoadsEndSwitchesEachLeashAsItsStatusSays)
{
	test::creature_loop_fakes::CallLog log;
	test::creature_loop_fakes::FakeLeash leashes(log);
	const auto creature = static_cast<entt::entity>(12);
	auto kept = KeptWorld();
	LoadedObjects loaded;
	KeepPlayerInterfaces(kept, loaded);

	ecs::saved_leash::SwitchAgain(leashes, kept, loaded);
	EXPECT_TRUE(log.empty());

	leashes.playersCreature = creature;
	ecs::saved_leash::SwitchAgain(leashes, kept, loaded);
	std::get<LeashStatusRecord>(*kept.objects[4]).activated = 1;
	ecs::saved_leash::SwitchAgain(leashes, kept, loaded);
	const test::creature_loop_fakes::CallLog expected {
	    {.name = "leash.SwitchAtLoad", .args = {12.0f, 0.0f}},
	    {.name = "leash.SwitchAtLoad", .args = {12.0f, 1.0f}},
	};
	EXPECT_EQ(log, expected);
}

namespace
{
/// An interface status of the hand's player with a value of its own in every field the hand keeps, its hand holding
/// the object of the number given
InterfaceStatusRecord HandStatus(ObjectNumber held)
{
	InterfaceStatusRecord status;
	status.position.position = {.x = (100 << 16) | 0x4000, .z = (90 << 16) | 0x8000, .altitude = 12.5f};
	for (size_t value = 0; value < status.throwAndHand.size(); ++value)
	{
		status.throwAndHand.at(value) = 0.5f + static_cast<float>(value);
	}
	status.hand = {.byte = 1, .held = {.numbers = {held}}};
	status.handHoldingSomething = 1;
	status.secondWord = 4;
	status.thirdWord = 17;
	status.point = {1000.0f, 20.0f, 1100.0f};
	status.cameraPosition = {900.0f, 300.0f, 950.0f};
	status.cameraFocus = {1010.0f, 25.0f, 1090.0f};
	status.reportedHandPosition = {998.0f, 21.0f, 1101.0f};
	status.secondPoint = {2.0f, -1.0f, -1.0f};
	status.handSpeed = 30.0f;
	status.firstFloat = 5.5f;
	status.secondFloat = -0.25f;
	status.handVelocity = {24.0f, -6.0f, -16.5f};
	status.thirdFloat = 7.5f;
	status.fifthWord = static_cast<uint32_t>(SpellSeedType::Fire);
	return status;
}

/// A loaded game whose player one's status is `status`: its interface 3, the status 4, the object 5 a post, the leash 6
gamesave::WorldRead HandWorld(InterfaceStatusRecord status)
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.players[0] = 2;
	kept.objects.emplace_back(game);
	gamesave::PlayerRecord player;
	player.interfaces.assign(18, 0);
	player.interfaces[0] = 3;
	kept.objects.emplace_back(player);
	kept.objects.emplace_back(PlayerInterfaceRecord {.status = {4}});
	status.playerInterface = {3};
	status.leashStatus = {6};
	kept.objects.emplace_back(status);
	kept.objects.emplace_back(gamesave::WaypointRecord {});
	kept.objects.emplace_back(LeashStatusRecord {.works = 1, .type = 2, .status = {4}});
	return kept;
}
} // namespace

// The status's values become the hand's: the synced place, the throw's four parts, the hand's unlock and still turns,
// the synced hand, the camera the sync gave, the hand at the last turn's end and its move, speed, heading, turning
// rate, velocity and sideways acceleration; written back with the object's number and the last seed type, they make
// the record they came from
TEST(HandStatusLoad, TheStatusValuesBecomeTheHandsAndAreWrittenBackAsTheyWere)
{
	const auto record = HandStatus(40);
	const auto post = static_cast<entt::entity>(7);
	const auto status = ecs::saved_hand::StatusOf(record, post);
	EXPECT_EQ(status.syncedPlace,
	          (map_coords::MapCoords {.x = (100 << 16) | 0x4000, .z = (90 << 16) | 0x8000, .altitude = 12.5f}));
	EXPECT_EQ(status.throwVelocity, glm::vec3(0.5f, 1.5f, 2.5f));
	EXPECT_EQ(status.throwAngular, glm::vec3(3.5f, 4.5f, 5.5f));
	EXPECT_EQ(status.throwHandPosition, glm::vec3(6.5f, 7.5f, 8.5f));
	EXPECT_EQ(status.throwAngles, glm::vec3(9.5f, 10.5f, 11.5f));
	EXPECT_EQ(status.unlockTurnsHeld, 4);
	EXPECT_EQ(status.stillTurns, 17u);
	EXPECT_EQ(status.syncedHand, glm::vec3(1000.0f, 20.0f, 1100.0f));
	EXPECT_EQ(status.cameraPosition, glm::vec3(900.0f, 300.0f, 950.0f));
	EXPECT_EQ(status.cameraFocus, glm::vec3(1010.0f, 25.0f, 1090.0f));
	EXPECT_EQ(status.turnHand, glm::vec3(998.0f, 21.0f, 1101.0f));
	EXPECT_EQ(status.turnDelta, glm::vec3(2.0f, -1.0f, -1.0f));
	EXPECT_EQ(status.turnSpeed, 30.0f);
	EXPECT_EQ(status.turnHeading, 5.5f);
	EXPECT_EQ(status.turnRate, -0.25f);
	EXPECT_EQ(status.turnVelocity, glm::vec3(24.0f, -6.0f, -16.5f));
	EXPECT_EQ(status.turnSideAcceleration, 7.5f);
	EXPECT_EQ(status.held, std::optional(post));

	// written over a status as its constructor leaves it, but for the values the hand does not keep
	InterfaceStatusRecord written;
	written.hand.held.numbers = {0};
	written.fifthWord = 0xFFFFFFFF;
	ecs::saved_hand::WriteLive(written, status, 40, static_cast<int32_t>(SpellSeedType::Fire));
	EXPECT_EQ(written, record);

	// an empty hand: nothing held, no hand holding; with no gestures, the last seed type stays as the record has it
	ecs::saved_hand::WriteLive(written, ecs::saved_hand::StatusOf(record, std::nullopt), 0, std::nullopt);
	EXPECT_EQ(written.hand.byte, 0);
	EXPECT_EQ(written.hand.held.numbers, std::vector<ObjectNumber> {0});
	EXPECT_EQ(written.handHoldingSomething, 0);
	EXPECT_EQ(written.fifthWord, static_cast<uint32_t>(SpellSeedType::Fire));
}

// The load puts player one's status back into the hand, with the object in the hand the load made; a save straight
// after, from the hand, writes the file the load read. An object the load did not make leaves the hand empty
TEST(HandStatusLoad, ALoadedStatusComesBackIntoTheHandAndASaveStraightAfterWritesIt)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	Locator::magicSystem::reset();
	const test::RestoreService<Locator::worshipState> restoreWorship;
	Locator::worshipState::reset();
	const test::RestoreService<Locator::handSystem> restoreHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	ecs::Registry registry;
	const auto post = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(post, gamesave::WaypointRecord {});

	const auto kept = HandWorld(HandStatus(5));
	LoadedObjects loaded;
	KeepPlayerInterfaces(kept, loaded);
	ASSERT_EQ(loaded.size(), 3u);
	loaded.emplace(5, LoadedObject {.entity = post, .type = gamesave::WaypointRecord::k_Type});

	const auto restored = ecs::saved_hand::Load(registry, hand, kept, loaded);
	EXPECT_TRUE(restored.found);
	EXPECT_EQ(restored.heldNumber, 5u);
	EXPECT_TRUE(restored.heldMade);
	EXPECT_EQ(restored.lastSeedType, static_cast<int32_t>(SpellSeedType::Fire));
	ASSERT_TRUE(hand.restoredStatus.has_value());
	EXPECT_EQ(*hand.restoredStatus, ecs::saved_hand::StatusOf(std::get<InterfaceStatusRecord>(*kept.objects[3]), post));

	// the save reads the hand as the load left it
	hand.savedStatus = hand.restoredStatus;
	ClassWriters writers;
	RegisterPlayerInterfaceWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));

	// the object not made: the hand comes back empty, and the readout says so
	hand.restoredStatus.reset();
	loaded.erase(5);
	const auto empty = ecs::saved_hand::Load(registry, hand, kept, loaded);
	EXPECT_EQ(empty.heldNumber, 5u);
	EXPECT_FALSE(empty.heldMade);
	ASSERT_TRUE(hand.restoredStatus.has_value());
	EXPECT_FALSE(hand.restoredStatus->held.has_value());
	EXPECT_EQ(ecs::saved_hand::ReadoutOf(empty), "held object 5 (not made, left out), last seed type 2");
}

// The game's record of the drawn hand puts it back holding the status's object, held as it was: its hold type, how far
// it hangs and whether the object was drawn sorted before the hand; a drawn hand in any other state, naming another
// object, or with the status's object not made, holds nothing
TEST(HandStatusLoad, TheDrawnHandHoldsTheStatussObjectAsTheGamesRecordKeepsIt)
{
	gamesave::GameTail tail;
	tail.handWord = 4;
	tail.handObject = {5};
	tail.handWords = {static_cast<uint32_t>(HoldType::Side), 1, 0, std::bit_cast<uint32_t>(0.7f)};
	const auto pot = static_cast<entt::entity>(9);
	const auto drawn = ecs::saved_hand::DrawnHandOf(tail, 5, pot);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(*drawn, (ecs::systems::SavedDrawnHand {
	                      .held = pot, .holdType = HoldType::Side, .lowering = 0.7f, .hadNeedsSorting = true}));
	EXPECT_FALSE(ecs::saved_hand::DrawnHandOf(tail, 5, std::nullopt).has_value());
	EXPECT_FALSE(ecs::saved_hand::DrawnHandOf(tail, 6, pot).has_value());
	EXPECT_FALSE(ecs::saved_hand::DrawnHandOf(tail, 0, pot).has_value());
	tail.handWord = 1;
	EXPECT_FALSE(ecs::saved_hand::DrawnHandOf(tail, 5, pot).has_value());

	// through the load: the status's object made, the drawn hand holds it after the status is back
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	Locator::magicSystem::reset();
	const test::RestoreService<Locator::worshipState> restoreWorship;
	Locator::worshipState::reset();
	const test::RestoreService<Locator::handSystem> restoreHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	ecs::Registry registry;
	const auto post = registry.Create();
	auto kept = HandWorld(HandStatus(5));
	auto& game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.tail.handWord = 4;
	game.tail.handObject = {5};
	game.tail.handWords = {static_cast<uint32_t>(HoldType::Above), 0, 0, std::bit_cast<uint32_t>(-0.3f)};
	LoadedObjects loaded;
	KeepPlayerInterfaces(kept, loaded);
	loaded.emplace(5, LoadedObject {.entity = post, .type = gamesave::WaypointRecord::k_Type});
	const auto restored = ecs::saved_hand::Load(registry, hand, kept, loaded);
	EXPECT_TRUE(restored.drawnHolding);
	ASSERT_TRUE(hand.restoredDrawn.has_value());
	EXPECT_EQ(*hand.restoredDrawn,
	          (ecs::systems::SavedDrawnHand {
	              .held = post, .holdType = HoldType::Above, .lowering = -0.3f, .hadNeedsSorting = false}));

	// the object not made: the drawn hand is not put back
	hand.restoredDrawn.reset();
	loaded.erase(5);
	EXPECT_FALSE(ecs::saved_hand::Load(registry, hand, kept, loaded).drawnHolding);
	EXPECT_FALSE(hand.restoredDrawn.has_value());
}

// The game's record names the drawn hand's object and the spell seed whose effect plays in the hand as the live hand
// has them now, not as the load left them: another object picked up since, nothing (0) once the hand let go or the
// thing is gone, and a seed in the hand in both. With no hand the record keeps the load's; the particles' place stays
TEST(HandStatusLoad, TheGamesRecordNamesTheDrawnHandsObjectAndSeedFromTheLiveHand)
{
	const test::RestoreService<Locator::handSystem> restoreHand;
	Locator::handSystem::reset();
	ecs::Registry registry;
	const auto post = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(post, gamesave::WaypointRecord {});
	const auto other = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(other, gamesave::WaypointRecord {});
	const auto seed = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(seed, gamesave::WaypointRecord {});
	const auto gone = registry.Create();
	registry.Destroy(gone);

	auto kept = HandWorld(HandStatus(5));
	auto& game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.tail.handWord = 4;
	game.tail.handObject = {5};
	game.tail.handObjects = {gamesave::Ref {7}, gamesave::Ref {5}};
	ClassWriters writers;
	RegisterGameObjectWriter(writers, {});
	const auto write = [&] {
		const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
		WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
		EXPECT_TRUE(written.has_value()) << context.Failure();
		const auto tail = written.has_value() ? std::get<gamesave::GameHead>(*written).tail : gamesave::GameTail {};
		return std::tuple {tail.handObject.number, tail.handObjects.at(0).number, tail.handObjects.at(1).number,
		                   context.Reference(other), context.Reference(seed)};
	};

	// no hand: as the load kept them
	{
		const auto [object, particles, spell, otherNumber, seedNumber] = write();
		EXPECT_EQ(object, 5u);
		EXPECT_EQ(particles, 7u);
		EXPECT_EQ(spell, 5u);
	}
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	// another object in the drawn hand, no seed
	hand.renderHeld = other;
	{
		const auto [object, particles, spell, otherNumber, seedNumber] = write();
		ASSERT_TRUE(otherNumber != 0u);
		EXPECT_EQ(object, otherNumber);
		EXPECT_EQ(particles, 7u);
		EXPECT_EQ(spell, 0u);
	}
	// a seed in the hand: the drawn hand's object and the spell in the hand
	hand.renderHeld = seed;
	hand.spellInHand = seed;
	{
		const auto [object, particles, spell, otherNumber, seedNumber] = write();
		ASSERT_TRUE(seedNumber != 0u);
		EXPECT_EQ(object, seedNumber);
		EXPECT_EQ(particles, 7u);
		EXPECT_EQ(spell, seedNumber);
	}
	// the hand empty, then holding things gone since
	hand.renderHeld.reset();
	hand.spellInHand.reset();
	{
		const auto [object, particles, spell, otherNumber, seedNumber] = write();
		EXPECT_EQ(object, 0u);
		EXPECT_EQ(spell, 0u);
	}
	hand.renderHeld = gone;
	hand.spellInHand = gone;
	{
		const auto [object, particles, spell, otherNumber, seedNumber] = write();
		EXPECT_EQ(object, 0u);
		EXPECT_EQ(spell, 0u);
	}
}

// The game's record's hand part written from the drawn hand, the inverse of what the load reads: a game loaded with an
// empty hand, then the hand picking another object up, writes a record the load reads back as holding it, held as the
// hand holds it. The shadow flag stays as loaded; with nothing held the hold's words stay too, and with no effect in
// the hand its bone's byte
TEST(HandStatusLoad, TheGamesRecordsDrawnHandIsWrittenFromTheDrawnHandAndReadsBackAsIt)
{
	using ecs::systems::DrawnHandNow;
	const test::RestoreService<Locator::handSystem> restoreHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	ecs::Registry registry;
	const auto other = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(other, gamesave::WaypointRecord {});

	auto kept = HandWorld(HandStatus(0));
	auto& game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.tail.handWord = 1;
	game.tail.handObject = {0};
	game.tail.handWords = {static_cast<uint32_t>(HoldType::Above), 0, 5, std::bit_cast<uint32_t>(-0.3f)};
	game.tail.handPoint = {1.0f, 2.0f, 3.0f};
	game.tail.handObjects = {gamesave::Ref {7}, gamesave::Ref {0}};
	game.tail.handByte = 1;
	ClassWriters writers;
	RegisterGameObjectWriter(writers, {});
	hand.renderHeld = other;
	hand.drawnNow = DrawnHandNow {.state = 4,
	                              .held = other,
	                              .holdType = HoldType::Side,
	                              .lowering = 0.7f,
	                              .hadNeedsSorting = true,
	                              .place = glm::vec3(10.0f, 20.0f, 30.0f)};
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& tail = std::get<gamesave::GameHead>(*written).tail;
	const auto number = context.Reference(other);
	ASSERT_TRUE(number != 0u);
	EXPECT_EQ(tail.handWord, 4u);
	EXPECT_EQ(tail.handObject.number, number);
	EXPECT_EQ(tail.handWords,
	          (std::array<uint32_t, 4> {static_cast<uint32_t>(HoldType::Side), 1, 5, std::bit_cast<uint32_t>(0.7f)}));
	EXPECT_EQ(tail.handPoint, (gamesave::Point {10.0f, 20.0f, 30.0f}));
	EXPECT_EQ(tail.handObjects.at(0).number, 7u);
	EXPECT_EQ(tail.handObjects.at(1).number, 0u);
	EXPECT_EQ(tail.handByte, 1);
	const auto drawn = ecs::saved_hand::DrawnHandOf(tail, number, other);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(*drawn, (ecs::systems::SavedDrawnHand {
	                      .held = other, .holdType = HoldType::Side, .lowering = 0.7f, .hadNeedsSorting = true}));

	// let go, a seed's effect in the hand not at the bone, no place yet: the state and the byte, the hold's words kept
	auto empty = game.tail;
	ecs::saved_hand::WriteDrawnHand(empty, DrawnHandNow {.state = 1, .effectAtBone = false}, 0, 9);
	EXPECT_EQ(empty.handWord, 1u);
	EXPECT_EQ(empty.handObject.number, 0u);
	EXPECT_EQ(empty.handWords, game.tail.handWords);
	EXPECT_EQ(empty.handPoint, game.tail.handPoint);
	EXPECT_EQ(empty.handObjects.at(0).number, 7u);
	EXPECT_EQ(empty.handObjects.at(1).number, 9u);
	EXPECT_EQ(empty.handByte, 0);
	EXPECT_FALSE(ecs::saved_hand::DrawnHandOf(empty, number, other).has_value());
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
// A computer player's hand comes back from its saved state when it has steps: its place, where its step goes, the speeds,
// the first step's stage and whether it was looked at for a move in front, a wait's turns, a gesture's origin and
// progress and whether a script holds it, and each step with its arguments, a thing as the load made it again; with no
// steps it is not made, as a new hand is made when a script first gives it some
TEST(ComputerPlayerWrite, ALoadedHandComesBackWithItsStepsAndTheirArguments)
{
	gamesave::WorldRead world;
	auto state = NewActionState(3, 1);
	const std::array<float, 3> place {2500.0f, 30.0f, 2600.0f};
	state.block = std::bit_cast<std::array<uint8_t, 12>>(place);
	state.point = {2550.0f, 31.0f, 2650.0f};
	state.first = 45.0f;
	state.words[0] = 0;
	state.words[1] = 1;
	state.words[2] = 1;
	state.words[5] = 7;
	state.second = 0.25f;
	state.otherPoint = {1.0f, 2.0f, 3.0f};
	state.objects = {3};
	world.objects.emplace_back(state);
	auto computer = NewComputerPlayer(3, 1, 0);
	computer.numbers = {1, 0, 1};
	computer.words[0] = std::bit_cast<uint32_t>(90.0f);
	world.objects.emplace_back(computer);
	PlayerSubActionRecord step {.word = 23};
	step.arguments.numbers = {4, 5};
	world.objects.emplace_back(step);
	world.objects.emplace_back(PlayerSubActionArgumentRecord {.word = 0, .object = {.number = 9}});
	world.objects.emplace_back(
	    PlayerSubActionArgumentRecord {.word = 5, .thirdWord = std::bit_cast<uint32_t>(1.5f), .point = {4.0f, 5.0f, 6.0f}});
	ecs::Registry registry;
	const auto thing = registry.Create();
	LoadedObjects loaded;
	loaded.emplace(9, LoadedObject {.entity = thing, .type = 0x2B});
	const auto hand = HandOfRecord(world, loaded, state, computer);
	ASSERT_TRUE(hand.has_value());
	EXPECT_EQ(hand->point, glm::vec3(2500.0f, 30.0f, 2600.0f));
	EXPECT_EQ(hand->target, glm::vec3(2550.0f, 31.0f, 2650.0f));
	EXPECT_EQ(hand->speed, 45.0f);
	EXPECT_EQ(hand->playerSpeed, 90.0f);
	EXPECT_FALSE(hand->checkPrerequisite);
	EXPECT_TRUE(hand->prerequisiteInserted);
	EXPECT_EQ(hand->stage, 1);
	EXPECT_EQ(hand->waitTurns, 7);
	EXPECT_EQ(hand->gestureProgress, 0.25f);
	EXPECT_EQ(hand->gestureOrigin, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(hand->held);
	ASSERT_EQ(hand->steps.size(), 1u);
	EXPECT_EQ(hand->steps[0].type, static_cast<ecs::components::HandSubAction>(23));
	ASSERT_EQ(hand->steps[0].arguments.size(), 2u);
	EXPECT_EQ(hand->steps[0].arguments[0].kind, ecs::components::HandArgument::Kind::Thing);
	EXPECT_EQ(hand->steps[0].arguments[0].thing, thing);
	EXPECT_EQ(hand->steps[0].arguments[1].kind, ecs::components::HandArgument::Kind::Float);
	EXPECT_EQ(hand->steps[0].arguments[1].floatValue, 1.5f);
	EXPECT_EQ(hand->steps[0].arguments[1].point, glm::vec3(4.0f, 5.0f, 6.0f));
	state.objects.clear();
	EXPECT_FALSE(HandOfRecord(world, loaded, state, computer).has_value());
}

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
