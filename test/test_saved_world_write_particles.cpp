/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spot visuals' containers, with their effects' particle interfaces and managers, written from the particle
// engine: a live container and its effect as the game saves them, the game's list of containers with those a load
// kept after the live ones, and the frozen Land 1 autosave, whose two bonfires' containers no load makes yet, writes
// back byte for byte (docs/wiki/engine/saved-games.md#openblack)

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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <ObjectStream.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ObjectNumber;
using gamesave::ParticleContainerRecord;
using gamesave::ParticleInterfaceRecord;
using gamesave::ParticleManagerRecord;
using gamesave::SavedRecord;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";
constexpr uint32_t k_Effect = 7;

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

/// The game object a new game's writer makes: the game's list of containers, by its writer, in its first list
void RegisterNewGame(ClassWriters& writers)
{
	writers.Register(gamesave::k_GameType, {},
	                 [](WriteContext& context, entt::entity /*unused*/) -> std::optional<SavedRecord> {
		                 gamesave::GameHead game;
		                 const auto* list = context.Writers().ListWriterOf(GlobalList::ParticleContainers);
		                 if (list != nullptr)
		                 {
			                 game.lists[0].numbers = (*list)(context);
		                 }
		                 return game;
	                 });
}

/// A spot visual's container hanging on a thing, its effect started from a file of no parts at a point, drawn as one
void StartContainer(particles::manager::State& state, entt::entity thing)
{
	auto file = std::make_shared<particles::File>();
	file->name = "SF_Bonfire";
	auto& running = state.effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	running.inContainer = true;
	running.path = particles::DrawPath::Queued;
	state.containers.push_front({.effect = k_Effect,
	                             .object = entt::null,
	                             .owner = thing,
	                             .turns = -1,
	                             .hadOwner = true,
	                             .coords = {.x = 0x1000000, .z = 0x2000000, .altitude = 27.5f},
	                             .placed = glm::vec3(0.0f),
	                             .particleType = ParticleType::Bonfire});
}

/// The manager a new spot visual's effect has, at the point, for the file of the particle type
ParticleManagerRecord NewManager()
{
	ParticleManagerRecord manager;
	const float strength = 1.0f;
	std::memcpy(manager.processInfo.data() + 48, &strength, sizeof(strength));
	manager.processInfo[56] = 1;
	manager.point = {1803.5f, 27.5f, 2645.5f};
	manager.byte = 255;
	manager.points[2] = {1.0f, 0.0f, 0.0f};
	manager.words[1] = std::bit_cast<uint32_t>(1.0f);
	manager.bytes = {1, 1};
	manager.persistent = {.first = static_cast<uint32_t>(ParticleType::Bonfire), .second = 0xFFFFFFFF};
	return manager;
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

// A live spot visual's container is written with the thing it hangs on, its effect's interface and its manager: the
// container's place, its effect drawn as one, the turns it has left and its strength; the manager's origin,
// alpha, age, start direction along x, magnitude, synced flag and its file named by the spot visual's particle type
TEST(ParticleWrite, ALiveContainerIsWrittenWithItsEffectsInterfaceAndManager)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto bonfire = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(bonfire, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), bonfire);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;

	gamesave::GameHead game;
	game.lists[0].numbers = {2};
	ParticleContainerRecord container;
	container.position.position = {.x = 0x1000000, .z = 0x2000000, .altitude = 27.5f};
	container.byte = 1;
	container.object = {3};
	container.system = {4};
	container.word = 0xFFFFFFFF;
	container.lastWord = std::bit_cast<uint32_t>(1.0f);
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, container},
	                                 {3, gamesave::WaypointRecord {}},
	                                 {4, ParticleInterfaceRecord {.system = {5}}},
	                                 {5, NewManager()}}));

	// gone with its thing, the container hangs on nothing; closing down, its manager says so
	registry.Destroy(bonfire);
	particles.GetState().effects[k_Effect].effect->CloseDown();
	const auto alone = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(alone.Complete()) << alone.failure;
	container.object = {0};
	container.system = {3};
	auto closing = NewManager();
	closing.secondWord = 1;
	EXPECT_EQ(alone.bytes, FileOf({{1, game}, {2, container}, {3, ParticleInterfaceRecord {.system = {4}}}, {4, closing}}));

	// a container whose spot visual's particle type is not known cannot name its effect's file: the write stops and
	// says which
	particles.GetState().containers.front().particleType = ParticleType::None;
	const auto unknown = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	EXPECT_FALSE(unknown.Complete());
	EXPECT_NE(unknown.failure.find("particle effect 7"), std::string::npos) << unknown.failure;
}

// The game's list of containers is the live ones, the newest first, then those the load kept, as the loaded file
// listed them
TEST(ParticleWrite, TheGamesListOfContainersIsTheLiveOnesThenTheKeptOnes)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers = {2};
	kept.objects.emplace_back(game);
	ParticleContainerRecord loaded;
	loaded.word = 5;
	kept.objects.emplace_back(loaded);
	ecs::Registry registry;
	StartContainer(particles.GetState(), entt::null);

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const auto* list = writers.ListWriterOf(GlobalList::ParticleContainers);
	ASSERT_NE(list, nullptr);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
	WriteContext context(writers, source);
	const auto numbers = (*list)(context);
	ASSERT_EQ(numbers.size(), 2u);
	EXPECT_EQ(numbers[1], 2u);
	const auto live = context.TargetOf(numbers[0]);
	ASSERT_TRUE(live.has_value());
	EXPECT_EQ(live->type, ParticleContainerRecord::k_Type);
	EXPECT_EQ(live->id, std::optional<uint64_t>(k_Effect));
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's two bonfires' containers are listed as the load kept them (no load makes them yet), and with the particle
// writers the file is the original's byte for byte
TEST(ParticleWrite, TheFrozenLandOneAutosaveWritesBackByteForByteWithTheParticleWriters)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
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
	ecs::Registry registry;
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	RegisterParticleWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
	size_t containers = 0;
	for (const auto& record : written.records)
	{
		containers += record.type == ParticleContainerRecord::k_Type ? 1 : 0;
	}
	EXPECT_EQ(containers, 2u);
}
