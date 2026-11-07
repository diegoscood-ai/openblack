/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The arenas' records written from openblack's arenas: a land's arena (made by the land script or by a load) and a
// creature fight's arena, the game's list of them newest first, a loaded arena given back as its record, and the
// frozen Land 1 autosave written back with these writers being the original's file
// (docs/wiki/engine/saved-games.md#writing-the-world-file)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <CreatureRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteArenas.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ArenaRecord;
using gamesave::SavedRecord;

namespace
{
constexpr uint32_t k_One = std::bit_cast<uint32_t>(1.0f);

class ArenaWriteTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the place's flags ask the map cells whether the arena is in the map
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		RegisterArenaWriters(_writers);
		// the creatures are named by their class: a stand-in claim, as the creature's writer will make it
		_writers.Register(
		    gamesave::CreatureRecord::k_Type,
		    [](const ecs::Registry& registry, entt::entity entity) {
			    return registry.AllOf<ecs::components::CreatureFighting>(entity);
		    },
		    [](WriteContext&, entt::entity) { return std::nullopt; });
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A land's arena as the land script makes it
	static entt::entity LandArena(map_coords::MapCoords at, float radius, uint32_t number)
	{
		const auto entity = Reg().Create();
		Reg().Assign<ecs::components::Arena>(entity, glm::vec3(map_coords::ToMetres(at.x), 0.0f, map_coords::ToMetres(at.z)),
		                                     radius, at, number);
		return entity;
	}

	/// Two creatures in a fight, the first having made its arena
	static std::pair<entt::entity, entt::entity> Fight(glm::vec2 centre, float radius, uint32_t serial)
	{
		const auto maker = Reg().Create();
		const auto opponent = Reg().Create();
		const creature_fight::Arena arena {.centre = centre, .radius = radius, .serial = serial};
		Reg().AssignState<ecs::components::CreatureFighting>(
		    maker, ecs::components::CreatureFighting {.opponent = opponent, .arena = arena, .madeArena = true});
		Reg().AssignState<ecs::components::CreatureFighting>(
		    opponent, ecs::components::CreatureFighting {.opponent = maker, .arena = arena, .madeArena = false});
		return {maker, opponent};
	}

	/// The record the arenas' source makes of the arena of that id
	[[nodiscard]] std::optional<ArenaRecord> Written(WriteContext& context, uint64_t id) const
	{
		const auto* writer = _writers.SourceWriterOf(ArenaRecord::k_Type);
		auto record = writer != nullptr ? (*writer)(context, id) : std::nullopt;
		if (!record.has_value())
		{
			return std::nullopt;
		}
		return std::get<ArenaRecord>(*record);
	}
	[[nodiscard]] std::optional<ArenaRecord> Written(uint64_t id) const
	{
		const WorldSource source {.registry = Reg()};
		WriteContext context(_writers, source);
		return Written(context, id);
	}

	ClassWriters _writers;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
};

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / relative, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// An arena the land script made is written as the arena's constructor leaves it: at the script's place in map units
// with its height above the land, no flags, the constructor's first word 1.0, its radius, no fight on, two empty
// fighters' places and an empty last word
TEST_F(ArenaWriteTest, ALandScriptsArenaIsWrittenAsItsConstructorLeavesIt)
{
	const auto arena = LandArena({.x = 14108459, .z = 17634558, .altitude = 0.0f}, 27.453003f, 1);
	const auto record = Written(LandArenaId(arena));
	ASSERT_TRUE(record.has_value());
	EXPECT_EQ(record->thing, gamesave::ThingPart {});
	EXPECT_EQ(record->position, (gamesave::PositionPart {.position = {.x = 14108459, .z = 17634558, .altitude = 0.0f}}));
	EXPECT_EQ(record->words, (std::array<uint32_t, 3> {k_One, std::bit_cast<uint32_t>(27.453003f), 0}));
	EXPECT_EQ(record->objects.numbers, (std::vector<gamesave::ObjectNumber> {0, 0}));
	EXPECT_EQ(record->other, gamesave::Ref {});
}

// A creature fight's arena is written at the middle of the two creatures in map units with no height above the land,
// with the first word 1.0, its radius, a fight on, and its fighters: the creature that made it, then its opponent.
// Only the creature that made it names the arena
TEST_F(ArenaWriteTest, AFightsArenaIsWrittenWithItsTwoFighters)
{
	const auto [maker, opponent] = Fight(glm::vec2(2152.78f, 2690.82f), 30.0f, 1);
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	const auto record = Written(context, FightArenaId(maker));
	ASSERT_TRUE(record.has_value());
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	EXPECT_EQ(record->position.position,
	          (gamesave::MapCoords {.x = map_coords::ToFixed(2152.78f), .z = map_coords::ToFixed(2690.82f), .altitude = 0.0f}));
	EXPECT_EQ(record->position.flags, 0);
	EXPECT_EQ(record->words, (std::array<uint32_t, 3> {k_One, std::bit_cast<uint32_t>(30.0f), 1}));
	EXPECT_EQ(record->objects.numbers, (std::vector {context.Reference(maker), context.Reference(opponent)}));
	EXPECT_NE(record->objects.numbers[0], record->objects.numbers[1]);
	EXPECT_EQ(record->other, gamesave::Ref {});

	EXPECT_EQ(ArenaKeyOf(Reg(), maker), (SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(maker)}));
	EXPECT_FALSE(ArenaKeyOf(Reg(), opponent).has_value());
}

// An arena that is gone stops the write, naming it
TEST_F(ArenaWriteTest, AnArenaThatIsGoneStopsTheWrite)
{
	const auto [maker, opponent] = Fight(glm::vec2(10.0f, 20.0f), 5.0f, 1);
	Reg().RemoveState<ecs::components::CreatureFighting>(maker);
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	EXPECT_FALSE(Written(context, FightArenaId(maker)).has_value());
	EXPECT_NE(context.Failure().find("arena"), std::string::npos);
}

// The game's arena list, newest first: the fights' arenas, the newest first, then the land's, the last made first
TEST_F(ArenaWriteTest, TheArenasListIsTheNewestFirst)
{
	const auto first = LandArena({.x = 10, .z = 20}, 5.0f, 1);
	const auto second = LandArena({.x = 30, .z = 40}, 5.0f, 2);
	const auto older = Fight(glm::vec2(1.0f, 2.0f), 5.0f, 1).first;
	const auto newer = Fight(glm::vec2(3.0f, 4.0f), 5.0f, 2).first;
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	EXPECT_EQ(context.Keys(ArenaRecord::k_Type),
	          (std::vector {FightArenaId(newer), FightArenaId(older), LandArenaId(second), LandArenaId(first)}));
	const auto* list = _writers.ListWriterOf(GlobalList::Arenas);
	ASSERT_NE(list, nullptr);
	const auto numbers = (*list)(context);
	EXPECT_EQ(numbers, (std::vector {context.Reference(SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(newer)}),
	                                 context.Reference(SourceKey {.type = ArenaRecord::k_Type, .id = FightArenaId(older)}),
	                                 context.Reference(SourceKey {.type = ArenaRecord::k_Type, .id = LandArenaId(second)}),
	                                 context.Reference(SourceKey {.type = ArenaRecord::k_Type, .id = LandArenaId(first)})}));
}

// A loaded arena is a land's arena at its saved place, with its saved radius, the next in the arenas' order, and is
// written back as the record it was loaded from: its first word, whether a fight is on, its fighters and its last word
// as the record has them
TEST_F(ArenaWriteTest, ALoadedArenaGivesBackItsRecord)
{
	ArenaRecord record;
	record.thing.flags = 1;
	record.position = {.position = {.x = 0x0A00123, .z = 0x0B00456, .altitude = 2.5f}, .flags = 0};
	record.words = {std::bit_cast<uint32_t>(0.75f), std::bit_cast<uint32_t>(40.0f), 1};
	record.objects.numbers = {5, 6};
	record.other = {.number = 9};
	constexpr gamesave::ObjectNumber k_Number = 3;

	const auto older = ecs::saved_world_load::MakeArenaEntity({.number = 2, .record = ArenaRecord {}});
	const auto entity = ecs::saved_world_load::MakeArenaEntity({.number = k_Number, .record = record});
	const auto& arena = Reg().Get<const ecs::components::Arena>(entity);
	EXPECT_EQ(arena.at, (map_coords::MapCoords {.x = 0x0A00123, .z = 0x0B00456, .altitude = 2.5f}));
	EXPECT_EQ(arena.position, glm::vec3(map_coords::ToMetres(0x0A00123), 0.0f, map_coords::ToMetres(0x0B00456)));
	EXPECT_EQ(arena.radius, 40.0f);
	EXPECT_EQ(Reg().Get<const ecs::components::Arena>(older).number, 1u);
	EXPECT_EQ(arena.number, 2u);

	gamesave::WorldRead kept;
	kept.objects.resize(k_Number);
	kept.objects[k_Number - 1] = SavedRecord {record};
	WorldSource source {.registry = Reg(), .kept = &kept};
	source.loaded[k_Number] = {.type = ArenaRecord::k_Type, .id = LandArenaId(entity)};
	WriteContext context(_writers, source);
	const auto written = Written(context, LandArenaId(entity));
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(*written, record);
}

// Saved and loaded again, an arena the land script made comes back as the same arena and is written as the same record
TEST_F(ArenaWriteTest, ALandScriptsArenaComesBackFromItsRecord)
{
	const map_coords::MapCoords at {.x = 16559834, .z = 20353056, .altitude = 0.0f};
	const auto made = LandArena(at, 27.453003f, 1);
	const auto record = Written(LandArenaId(made));
	ASSERT_TRUE(record.has_value());
	const auto& before = Reg().Get<const ecs::components::Arena>(made);

	const auto loaded = ecs::saved_world_load::MakeArenaEntity({.number = 4, .record = *record});
	const auto& after = Reg().Get<const ecs::components::Arena>(loaded);
	EXPECT_EQ(after.at, before.at);
	EXPECT_EQ(after.position, before.position);
	EXPECT_EQ(after.radius, before.radius);
	Reg().Destroy(made);
	EXPECT_EQ(Written(LandArenaId(loaded)), record);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's three arenas, the land script's, are planned in the list's order and made as the load makes them; the
// arenas' list written from them is the save's, and the file written back with the arenas' writers is the original's,
// byte for byte, each arena written by its writer
TEST_F(ArenaWriteTest, TheFrozenLandOneAutosavesArenasWriteBackByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto read = gamesave::ReadWorld(*bytes);
	const auto plan = ecs::saved_world_load::PlanOf(read);
	ASSERT_EQ(plan.arenas.size(), 3u);
	WorldSource source {.registry = Reg(), .landscape = read.framing.landscape, .kept = &read};
	for (const auto& arena : plan.arenas | std::views::reverse)
	{
		const auto entity = ecs::saved_world_load::MakeArenaEntity(arena);
		source.loaded[arena.number] = {.type = ArenaRecord::k_Type, .id = LandArenaId(entity)};
	}

	ASSERT_TRUE(read.objects.front().has_value());
	const auto& game = std::get<gamesave::GameHead>(*read.objects.front());
	WriteContext context(_writers, source);
	const auto* list = _writers.ListWriterOf(GlobalList::Arenas);
	ASSERT_NE(list, nullptr);
	EXPECT_EQ((*list)(context), game.lists[static_cast<size_t>(GlobalList::Arenas)].numbers);

	const auto kept = WriteWorld(ClassWriters {}, source);
	const auto written = WriteWorld(_writers, source);
	EXPECT_EQ(written.failure, kept.failure);
	EXPECT_EQ(written.bytes, kept.bytes);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		byWriter += record.type == ArenaRecord::k_Type && record.source == RecordSource::Writer ? 1 : 0;
	}
	EXPECT_EQ(byWriter, 3u);
}
