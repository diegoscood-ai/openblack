/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence rings written from their entities: each ring's place as map coordinates, the object it follows while
// that object exists, its player, its radius and its anti word, the game's list of rings with those a load kept after
// the live ones, the file read back as written, and the frozen Land 1 autosave written back byte for byte with the
// rings' writers (docs/wiki/engine/saved-games.md#openblack)

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <ObjectStream.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "Enums.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::InfluenceRingRecord;
using gamesave::ObjectNumber;
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

/// The game object a new game's writer makes: the game's list of rings, by its writer, in its first list; and the
/// players, a source's objects by their numbers
void RegisterNewGame(ClassWriters& writers)
{
	writers.Register(gamesave::k_GameType, {},
	                 [](WriteContext& context, entt::entity /*unused*/) -> std::optional<SavedRecord> {
		                 gamesave::GameHead game;
		                 const auto* list = context.Writers().ListWriterOf(GlobalList::InfluenceRings);
		                 if (list != nullptr)
		                 {
			                 game.lists[0].numbers = (*list)(context);
		                 }
		                 return game;
	                 });
	writers.RegisterSource(
	    gamesave::k_PlayerType,
	    [](const ecs::Registry& /*from*/) {
		    return std::vector<uint64_t> {0, 1};
	    },
	    [](WriteContext& /*context*/, uint64_t id) -> std::optional<SavedRecord> {
		    return gamesave::PlayerRecord {.playerNumber = static_cast<uint8_t>(id)};
	    });
}

/// A ring's record as the game writes it
InfluenceRingRecord RingRecord(const glm::vec3& metres, ObjectNumber attached, ObjectNumber player, float radius, uint32_t anti)
{
	InfluenceRingRecord record;
	record.position.position = {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.z), .altitude = metres.y};
	record.attached = {attached};
	record.player = {player};
	record.radius = radius;
	record.anti = anti;
	return record;
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

// Two rings made in play are written newest first: the one that follows an object names that object, then its player;
// the fixed one names no object, and keeps the anti word its maker gave. Read back, the file gives the same rings. With
// its object gone, the ring that followed it names none
TEST(InfluenceRingWrite, TheRingsMadeInPlayAreWrittenFromTheirEntitiesAndReadBackTheSame)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	const glm::vec3 fixed(100.0f, 3.0f, 200.0f);
	const glm::vec3 followed(300.0f, 0.5f, 400.0f);
	influence::CreateRing(fixed, PlayerNames::PLAYER_TWO, 50.0f, 2);
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, followed, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(object, gamesave::WaypointRecord {});
	influence::CreateRingOnObject(object, PlayerNames::PLAYER_ONE, 30.0f, 0);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterInfluenceRingWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;

	gamesave::GameHead game;
	game.lists[0].numbers = {2, 5};
	const auto onObject = RingRecord(followed, 3, 4, 30.0f, 0);
	const auto alone = RingRecord(fixed, 0, 6, 50.0f, 2);
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, onObject},
	                                 {3, gamesave::WaypointRecord {}},
	                                 {4, gamesave::PlayerRecord {.playerNumber = 0}},
	                                 {5, alone},
	                                 {6, gamesave::PlayerRecord {.playerNumber = 1}}}));

	// the file read back: the two rings, in the order written, with what they were written from
	const auto read = gamesave::ReadWorld(written.bytes);
	ASSERT_EQ(read.stop, gamesave::k_ReadToTheEnd);
	std::vector<InfluenceRingRecord> rings;
	for (const auto& record : read.objects)
	{
		if (const auto* ring = record.has_value() ? std::get_if<InfluenceRingRecord>(&*record) : nullptr; ring != nullptr)
		{
			rings.push_back(*ring);
		}
	}
	ASSERT_EQ(rings.size(), 2u);
	for (size_t i = 0; i < rings.size(); ++i)
	{
		const auto& expected = i == 0 ? onObject : alone;
		EXPECT_EQ(rings[i].position, expected.position);
		EXPECT_EQ(rings[i].attached, expected.attached);
		EXPECT_EQ(rings[i].player, expected.player);
		EXPECT_EQ(rings[i].radius, expected.radius);
		EXPECT_EQ(rings[i].anti, expected.anti);
	}

	// its object gone, the ring that followed it names no object
	registry.Destroy(object);
	const auto without = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(without.Complete()) << without.failure;
	game.lists[0].numbers = {2, 4};
	EXPECT_EQ(without.bytes, FileOf({{1, game},
	                                 {2, RingRecord(followed, 0, 3, 30.0f, 0)},
	                                 {3, gamesave::PlayerRecord {.playerNumber = 0}},
	                                 {4, RingRecord(fixed, 0, 5, 50.0f, 2)},
	                                 {5, gamesave::PlayerRecord {.playerNumber = 1}}}));
}

// The game's list of rings is the live ones, the newest first, then those the load kept, as the loaded file listed
// them
TEST(InfluenceRingWrite, TheGamesListOfRingsIsTheLiveOnesThenTheKeptOnes)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::InfluenceRings)).numbers = {2};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(InfluenceRingRecord {.radius = 5.0f});
	const auto older = influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_ONE, 10.0f, 1);
	const auto newer = influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_ONE, 20.0f, 0);

	ClassWriters writers;
	RegisterInfluenceRingWriters(writers);
	const auto* list = writers.ListWriterOf(GlobalList::InfluenceRings);
	ASSERT_NE(list, nullptr);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
	WriteContext context(writers, source);
	const auto numbers = (*list)(context);
	ASSERT_EQ(numbers.size(), 3u);
	const auto first = context.TargetOf(numbers[0]);
	const auto second = context.TargetOf(numbers[1]);
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_TRUE(first->entity == newer);
	EXPECT_TRUE(second->entity == older);
	EXPECT_EQ(numbers[2], 2u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. With the rings'
// writers, the frozen Land 1 autosave's rings are listed as the load kept them (no load makes them yet), and the file
// is the original's byte for byte
TEST(InfluenceRingWrite, TheFrozenLandOneAutosaveWritesBackByteForByteWithTheRingWriters)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
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
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	RegisterInfluenceRingWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
}
