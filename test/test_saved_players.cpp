/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' records: what a load puts back of a player and what a save writes of it, a new game's records as the
// game sets its players up, and the players' writer (docs/wiki/engine/saved-games.md#the-players-records)

#include <cstdint>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <ObjectStream.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatureSlot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"

using namespace openblack;
using namespace openblack::ecs::saved_players;
using openblack::gamesave::ObjectNumber;
using openblack::gamesave::PlayerRecord;
using openblack::gamesave::SavedRecord;
namespace saved_world_write = openblack::ecs::saved_world_write;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";

/// A player's record with a value of its own in every field
PlayerRecord APlayer()
{
	PlayerRecord player = NewRecord(PlayerNames::PLAYER_TWO, 2, u"Player[2]");
	player.thing.flags = 4;
	player.alignment.value = -0.25f;
	player.alignment.change = 0.0095854f;
	for (size_t i = 0; i < player.tribalPower.size(); ++i)
	{
		player.tribalPower.at(i) = 1.0f + static_cast<float>(i);
		player.maxTribalPower.at(i) = 2.0f + static_cast<float>(i);
	}
	player.userId = 963864947;
	player.cheatOn = 1;
	player.windResistance = 2;
	player.allMagicEnabled = 1;
	player.cheatWord = 3;
	player.infiniteInfluence = 4;
	player.computerPlayer = 3;
	player.totalPopulation = 47;
	player.start = {.position = {.x = 0x00DF71B0, .z = 0x013ACEB2, .altitude = 20.0f}, .first = 1, .second = -1};
	player.magicTypeEnabled.at(3) = 2;
	player.magicTypeEverBeenEnabled.at(3) = 1;
	player.magicTypeEverBeenEnabled.at(41) = 1;
	player.gameStats = 13;
	player.citadel = 14;
	player.creature = 15;
	player.towns = {20, 21};
	player.teleports = {22};
	player.influencePower = 2.5f;
	player.averageInfluencePower = 2.0f;
	player.damageFromPlayer.at(7) = 0.75f;
	player.allies.at(0) = 0.5f;
	player.interfaces.at(1) = 30;
	player.hasLost = 1;
	player.lastWord = 7;
	return player;
}

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
} // namespace

// A record's values put back into the same record leave it as it was; values of another player go only where the
// save writes what openblack models, every other field staying the record's
TEST(SavedPlayers, ARecordsValuesGoBackWhereTheyCameFrom)
{
	const auto record = APlayer();
	const auto values = ValuesOf(record);
	EXPECT_EQ(values.alignment.value, -0.25f);
	EXPECT_EQ(values.alignment.pending, 0.0095854f);
	EXPECT_EQ(values.tribalPower.at(8), 9.0f);
	EXPECT_EQ(values.maxTribalPower.at(0), 2.0f);
	EXPECT_EQ(values.type, 2u);
	EXPECT_TRUE(values.allMagic);
	EXPECT_EQ(values.totalPopulation, 47u);
	EXPECT_EQ(values.magicEnabled.at(3), 2);
	EXPECT_TRUE(values.everEnabled.at(3));
	EXPECT_TRUE(values.everEnabled.at(41));
	EXPECT_FALSE(values.everEnabled.at(4));
	EXPECT_EQ(values.influencePower, 2.5f);
	EXPECT_EQ(values.averageInfluencePower, 2.0f);
	EXPECT_EQ(values.damageFromPlayer.at(7), 0.75f);
	EXPECT_EQ(values.allies.at(0), 0.5f);
	EXPECT_EQ(RecordOf(values, record), record);

	PlayerValues other;
	other.alignment = {.value = 0.5f, .pending = 0.125f};
	other.type = 1;
	other.magicEnabled.at(0) = 1;
	const auto written = RecordOf(other, record);
	auto expected = record;
	expected.alignment.value = 0.5f;
	expected.alignment.change = 0.125f;
	expected.tribalPower.assign(9, 0.0f);
	expected.maxTribalPower.assign(9, 0.0f);
	expected.type = 1;
	expected.allMagicEnabled = 0;
	expected.totalPopulation = 0;
	expected.magicTypeEnabled.assign(42, 0);
	expected.magicTypeEnabled.at(0) = 1;
	expected.magicTypeEverBeenEnabled.assign(42, 0);
	expected.influencePower = 0.0f;
	expected.averageInfluencePower = 0.0f;
	expected.damageFromPlayer.assign(8, 0.0f);
	expected.allies.assign(8, 0.0f);
	EXPECT_EQ(written, expected);

	// a list of another count keeps its count
	auto shorter = record;
	shorter.magicTypeEnabled.resize(2);
	EXPECT_EQ(RecordOf(other, shorter).magicTypeEnabled, (std::vector<int32_t> {1, 0}));
	EXPECT_EQ(ValuesOf(shorter).magicEnabled.at(3), 0);
}

// A new game's players as the game sets them up before any land: the human, the neutral player and "Player[n]" for
// the others, every tribe's power 1, the lists of the counts the game writes
TEST(SavedPlayers, ANewGamesPlayersAreAsTheGameSetsThemUp)
{
	EXPECT_EQ(NewType(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE), 1u);
	EXPECT_EQ(NewType(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 0u);
	EXPECT_EQ(NewType(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE), 3u);
	EXPECT_EQ(NewName(PlayerNames::PLAYER_ONE, u"ff", u"NEUTRAL"), u"ff");
	EXPECT_EQ(NewName(PlayerNames::PLAYER_TWO, u"ff", u"NEUTRAL"), u"Player[2]");
	EXPECT_EQ(NewName(PlayerNames::PLAYER_SEVEN, u"ff", u"NEUTRAL"), u"Player[7]");
	EXPECT_EQ(NewName(PlayerNames::NEUTRAL, u"ff", u"NEUTRAL"), u"NEUTRAL");

	const auto neutral = NewRecord(PlayerNames::NEUTRAL, k_NeutralPlayerType, u"PEPE NEUTRAL");
	EXPECT_EQ(neutral.thing.extra, 7u);
	EXPECT_EQ(neutral.playerNumber, 7);
	EXPECT_EQ(neutral.alignment.classWord, 0x8CC7F0u);
	EXPECT_EQ(neutral.info, 1743u);
	EXPECT_EQ(neutral.tribalPower, std::vector<float>(9, 1.0f));
	EXPECT_EQ(neutral.maxTribalPower, std::vector<float>(9, 1.0f));
	EXPECT_EQ(neutral.type, 3u);
	ASSERT_EQ(neutral.name.size(), 32u);
	EXPECT_EQ(std::u16string(neutral.name.begin(), neutral.name.begin() + 12), u"PEPE NEUTRAL");
	EXPECT_EQ(neutral.name.at(12), u'\0');
	EXPECT_EQ(neutral.name.back(), u'\0');
	EXPECT_EQ(neutral.magicTypeEnabled.size(), 42u);
	EXPECT_EQ(neutral.magicTypeEverBeenEnabled.size(), 42u);
	EXPECT_EQ(neutral.damageFromPlayer.size(), 8u);
	EXPECT_EQ(neutral.allies.size(), 8u);
	EXPECT_EQ(neutral.interfaces, std::vector<ObjectNumber>(18, 0));
	EXPECT_TRUE(neutral.towns.empty());
	EXPECT_EQ(neutral.computerPlayer, 0u);

	// a name keeps its last character for its end
	const auto longName = NewRecord(PlayerNames::PLAYER_ONE, 1, std::u16string(40, u'a'));
	EXPECT_EQ(longName.name.at(30), u'a');
	EXPECT_EQ(longName.name.at(31), u'\0');
}

// The objects a player's record names: their citadel not marked for deletion, the creature their entity keeps, and
// their towns in the order they joined their list
TEST(SavedPlayers, APlayersObjectsAreTheirCitadelCreatureAndTownsInTheirListsOrder)
{
	ecs::Registry registry;
	const auto gone = registry.Create();
	registry.Assign<ecs::components::Temple>(gone, PlayerNames::PLAYER_TWO);
	registry.Assign<ecs::components::CitadelWorship>(gone);
	registry.Assign<ecs::components::Unavailable>(gone);
	const auto citadel = registry.Create();
	registry.Assign<ecs::components::Temple>(citadel, PlayerNames::PLAYER_TWO);
	registry.Assign<ecs::components::CitadelWorship>(citadel);
	const auto othersCitadel = registry.Create();
	registry.Assign<ecs::components::Temple>(othersCitadel, PlayerNames::PLAYER_ONE);
	registry.Assign<ecs::components::CitadelWorship>(othersCitadel);

	const auto town = [&registry](PlayerNames owner, uint32_t stamp) {
		const auto entity = registry.Create();
		auto& component = registry.Assign<ecs::components::Town>(entity);
		component.owner = owner;
		component.ownerListStamp = stamp;
		return entity;
	};
	const auto later = town(PlayerNames::PLAYER_TWO, 5);
	const auto earlier = town(PlayerNames::PLAYER_TWO, 2);
	town(PlayerNames::NEUTRAL, 3);

	const auto creature = registry.Create();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player, PlayerNames::PLAYER_TWO);
	registry.Assign<ecs::components::PlayerCreatureSlot>(player, std::optional<entt::entity> {creature});

	const auto objects = ObjectsOf(registry, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(objects.citadel == citadel);
	EXPECT_TRUE(objects.creature == creature);
	ASSERT_EQ(objects.towns.size(), 2u);
	EXPECT_TRUE(objects.towns.at(0) == earlier);
	EXPECT_TRUE(objects.towns.at(1) == later);

	// a player with no entity, no citadel and no town names none
	const auto none = ObjectsOf(registry, PlayerNames::PLAYER_FIVE);
	EXPECT_TRUE(none.citadel == entt::null);
	EXPECT_TRUE(none.creature == entt::null);
	EXPECT_TRUE(none.towns.empty());
}

// The load lists each player record the game object names, by its number in the file
TEST(SavedPlayers, TheLoadListsThePlayersTheGameObjectNames)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.players = {3, 2, 0, 0, 0, 0, 0, 9};
	world.objects.emplace_back(game);
	world.objects.emplace_back(NewRecord(PlayerNames::PLAYER_TWO, 0, u"Player[2]"));
	world.objects.emplace_back(NewRecord(PlayerNames::PLAYER_ONE, 1, u"ff"));
	const auto listed = ListPlayers(world);
	ASSERT_EQ(listed.size(), 2u);
	EXPECT_EQ(listed.at(0).number, 3u);
	EXPECT_EQ(listed.at(0).record->playerNumber, 0);
	EXPECT_EQ(listed.at(1).number, 2u);
	EXPECT_EQ(listed.at(1).record->playerNumber, 1);
	EXPECT_TRUE(ListPlayers(gamesave::WorldRead {}).empty());
}

// A loaded player is written from its kept record with the values and objects the game gives; a loaded town keeps its
// number of the loaded file
TEST(SavedPlayers, ALoadedPlayerIsWrittenFromItsRecordWithTheGamesValues)
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.players = {2, 0, 0, 0, 0, 0, 0, 0};
	kept.objects.emplace_back(game);
	auto player = NewRecord(PlayerNames::PLAYER_ONE, 1, u"ff");
	player.towns = {3};
	player.creature = 0;
	kept.objects.emplace_back(player);
	gamesave::TownRecord town;
	kept.objects.emplace_back(town);

	ecs::Registry registry;
	const auto townEntity = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::TownRecord>>(townEntity, town);
	saved_world_write::LoadedObjects loaded;
	loaded.emplace(2, saved_world_write::LoadedObject {.type = gamesave::k_PlayerType, .id = 0});
	loaded.emplace(3, saved_world_write::LoadedObject {.entity = townEntity, .type = gamesave::TownRecord::k_Type});
	const auto values = [](PlayerNames /*player*/) -> std::optional<PlayerValues> {
		PlayerValues given = ValuesOf(NewRecord(PlayerNames::PLAYER_ONE, 1, u"ff"));
		given.alignment.value = 0.75f;
		given.totalPopulation = 12;
		return given;
	};

	saved_world_write::ClassWriters writers;
	saved_world_write::RegisterGameObjectWriter(writers, {});
	saved_world_write::RegisterPlayerWriters(writers, {.values = values,
	                                                   .objects =
	                                                       [townEntity](const ecs::Registry& /*registry*/, PlayerNames player) {
		                                                       return player == PlayerNames::PLAYER_ONE
		                                                                  ? PlayerObjects {.towns = {townEntity}}
		                                                                  : PlayerObjects {};
	                                                       },
	                                                   .newRecord = {}});
	const auto written = saved_world_write::WriteWorld(
	    writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	auto expected = player;
	expected.alignment.value = 0.75f;
	expected.totalPopulation = 12;
	EXPECT_EQ(written.bytes, FileOf({{1, *kept.objects[0]}, {2, expected}, {3, town}}));
	ASSERT_EQ(written.records.size(), 3u);
	EXPECT_EQ(written.records.at(1).type, gamesave::k_PlayerType);
	EXPECT_EQ(written.records.at(1).source, saved_world_write::RecordSource::Writer);
	EXPECT_TRUE(written.records.at(1).id == std::optional<uint64_t> {0});

	// a creature the loaded player got since its load, which had none, is named; it has no writer yet, and the write
	// says so
	const auto creature = registry.Create();
	saved_world_write::ClassWriters withCreature;
	saved_world_write::RegisterGameObjectWriter(withCreature, {});
	saved_world_write::RegisterPlayerWriters(
	    withCreature, {.values = values,
	                   .objects =
	                       [townEntity, creature](const ecs::Registry& /*registry*/, PlayerNames player) {
		                       return player == PlayerNames::PLAYER_ONE
		                                  ? PlayerObjects {.creature = creature, .towns = {townEntity}}
		                                  : PlayerObjects {};
	                       },
	                   .newRecord = {}});
	const auto failing = saved_world_write::WriteWorld(
	    withCreature, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded});
	EXPECT_FALSE(failing.Complete());
	EXPECT_NE(failing.failure.find("has no writer and no record kept"), std::string::npos) << failing.failure;
}

// A new game's eight players are written in their numbers' order, each from a new game's record; the first one's
// computer player has no writer yet, and the write says so
TEST(SavedPlayers, ANewGamesPlayersAreWrittenAndTheirComputerPlayersAreNamed)
{
	ecs::Registry registry;
	saved_world_write::ClassWriters writers;
	saved_world_write::RegisterGameObjectWriter(writers, {});
	for (size_t list = 0; list < static_cast<size_t>(saved_world_write::GlobalList::_COUNT); ++list)
	{
		writers.RegisterList(static_cast<saved_world_write::GlobalList>(list),
		                     [](saved_world_write::WriteContext& /*context*/) { return std::vector<ObjectNumber> {}; });
	}
	std::vector<PlayerNames> made;
	saved_world_write::RegisterPlayerWriters(writers, {.values = {}, .objects = {}, .newRecord = [&made](PlayerNames player) {
		                                                   made.push_back(player);
		                                                   return NewRecord(player, 0, u"name");
	                                                   }});
	const auto written = saved_world_write::WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	EXPECT_FALSE(written.Complete());
	EXPECT_NE(written.failure.find("has no writer and no record kept"), std::string::npos) << written.failure;
	ASSERT_EQ(written.records.size(), 2u);
	EXPECT_EQ(written.records.at(1).type, gamesave::k_PlayerType);
	EXPECT_TRUE(written.records.at(1).id == std::optional<uint64_t> {0});
	ASSERT_FALSE(made.empty());
	EXPECT_TRUE(made.front() == PlayerNames::PLAYER_ONE);
}
