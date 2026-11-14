/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rivers' and the banks of mist's records written from their entities: a loaded one gives back the record it was
// loaded from, one made since is written from what openblack holds, and the frozen Land 1 autosave written back with
// these writers is the original's file (docs/wiki/engine/saved-games.md#openblack)

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Components/Stream.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::MistRecord;
using gamesave::SavedRecord;
using gamesave::StreamRecord;

namespace
{
class RiverAndMistWriteTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		RegisterRiverAndMistWriters(_writers);
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The record the class's writer makes of the entity
	[[nodiscard]] std::optional<SavedRecord> Written(entt::entity entity, uint32_t type) const
	{
		const WorldSource source {.registry = Reg()};
		WriteContext context(_writers, source);
		const auto* writer = _writers.WriterOf(type);
		return writer != nullptr ? (*writer)(context, entity) : std::nullopt;
	}

	ClassWriters _writers;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

std::array<uint8_t, 16> Node(float x, float y, float z, uint32_t link)
{
	std::array<uint8_t, 16> node {};
	const std::array<float, 3> point {x, y, z};
	std::memcpy(node.data(), point.data(), sizeof(point));
	std::memcpy(node.data() + 12, &link, sizeof(link));
	return node;
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
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// A loaded river is written back as its record: its points, each with the word the game wrote after it, its id and its
// last point's index
TEST_F(RiverAndMistWriteTest, ALoadedRiverGivesBackItsRecord)
{
	StreamRecord record;
	record.thing.flags = 3;
	record.nodes.values = {Node(1.0f, 2.0f, 3.0f, 0x1234), Node(4.0f, 5.0f, 6.0f, 0x5678), Node(7.0f, 8.0f, 9.0f, 0)};
	record.words = {7, 2};
	const auto entity = ecs::saved_world_load::MakeStreamEntity({.number = 5, .record = record});
	const auto written = Written(entity, StreamRecord::k_Type);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(std::get<StreamRecord>(*written), record);
}

// A river made since has the points openblack holds, no word after them, its id and its last point's index
TEST_F(RiverAndMistWriteTest, ARiverMadeSinceIsWrittenFromItsPoints)
{
	const auto entity = Reg().Create();
	auto& stream = Reg().Assign<ecs::components::Stream>(entity, 4);
	stream.points = {{10.0f, 1.0f, 20.0f}, {30.0f, 2.0f, 40.0f}};
	const auto written = Written(entity, StreamRecord::k_Type);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<StreamRecord>(*written);
	EXPECT_EQ(record.nodes.values, (std::vector {Node(10.0f, 1.0f, 20.0f, 0), Node(30.0f, 2.0f, 40.0f, 0)}));
	EXPECT_EQ(record.words, (std::array<uint32_t, 2> {4, 1}));
}

// A loaded bank of mist is written back as its record: its place, size, colour and edge-on shrink
TEST_F(RiverAndMistWriteTest, ALoadedMistGivesBackItsRecord)
{
	MistRecord record;
	record.thing.flags = 1;
	record.position = {.position = {.x = 0x0A00123, .z = 0x0B00456, .altitude = 12.5f}, .flags = 2};
	record.words = {std::bit_cast<uint32_t>(0.8f), 0x80FFEEDD, std::bit_cast<uint32_t>(3.0f)};
	const auto entity = ecs::saved_world_load::MakeMistEntity({.number = 6, .record = record});
	const auto written = Written(entity, MistRecord::k_Type);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(std::get<MistRecord>(*written), record);
}

// A bank of mist made since is written at its place, at its altitude over the land, with its size, colour and shrink
TEST_F(RiverAndMistWriteTest, AMistMadeSinceIsWrittenFromItsPlace)
{
	const auto entity = ecs::archetypes::MistArchetype::Create(glm::vec3(100.0f, 0.0f, 200.0f), 15.0f, 0x40112233, 1.5f, 2.0f);
	const auto written = Written(entity, MistRecord::k_Type);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<MistRecord>(*written);
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(record.position.position.z, map_coords::ToFixed(200.0f));
	EXPECT_EQ(record.position.position.altitude, 15.0f);
	EXPECT_EQ(record.words,
	          (std::array<uint32_t, 3> {std::bit_cast<uint32_t>(1.5f), 0x40112233, std::bit_cast<uint32_t>(2.0f)}));
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's rivers and banks of mist made as the load makes them and written by their writers give back the file the
// load kept, byte for byte, each of them written by its writer
TEST_F(RiverAndMistWriteTest, TheFrozenLandOneAutosaveWrittenBackWithTheseWritersIsTheOriginals)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto read = gamesave::ReadWorld(*bytes);
	WorldSource source {.registry = Reg(), .landscape = read.framing.landscape, .kept = &read};
	size_t rivers = 0;
	size_t mists = 0;
	for (size_t i = 0; i < read.objects.size(); ++i)
	{
		const auto number = static_cast<gamesave::ObjectNumber>(i + 1);
		if (!read.objects[i].has_value())
		{
			continue;
		}
		if (const auto* stream = std::get_if<StreamRecord>(&*read.objects[i]))
		{
			source.loaded[number] = {.entity = ecs::saved_world_load::MakeStreamEntity({.number = number, .record = *stream}),
			                         .type = StreamRecord::k_Type};
			++rivers;
		}
		else if (const auto* mist = std::get_if<MistRecord>(&*read.objects[i]))
		{
			source.loaded[number] = {.entity = ecs::saved_world_load::MakeMistEntity({.number = number, .record = *mist}),
			                         .type = MistRecord::k_Type};
			++mists;
		}
	}
	ASSERT_GT(rivers, 0u);
	ASSERT_GT(mists, 0u);
	const auto kept = WriteWorld(ClassWriters {}, source);
	const auto written = WriteWorld(_writers, source);
	EXPECT_EQ(written.failure, kept.failure);
	EXPECT_EQ(written.bytes, kept.bytes);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type == StreamRecord::k_Type || record.type == MistRecord::k_Type)
		{
			EXPECT_EQ(record.source, RecordSource::Writer) << record.number;
			++byWriter;
		}
	}
	EXPECT_EQ(byWriter, rivers + mists);
}
