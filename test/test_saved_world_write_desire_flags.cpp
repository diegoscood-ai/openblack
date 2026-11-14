/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns' desire flags written from the entities the load made of them: each record as the load read it, a flag
// made in play from its state, and the frozen Land 1 autosave's flags as read and its whole world file as it was
// (docs/wiki/engine/saved-games.md#the-animals-and-the-towns-desire-flags)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

#include <AnimalRecords.h>
#include <SavedRecords.h>
#include <TownRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/TownDesireFlagArchetype.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/TownDesireFlags.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ObjectNumber;
using gamesave::SavedRecord;
using gamesave::TownDesireFlagsRecord;
using gamesave::TownRecord;
namespace load = openblack::ecs::saved_world_load;

namespace
{
/// The row number every flag of the frozen Land 1 autosave is of, the first prayer site's
constexpr uint32_t k_PrayerSiteRow = 1856;

/// The frozen Land 1 autosave, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenLandOneAutosave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / "_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" /
	                  "Saved Games" / "20" / "20.sav";
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

class DesireFlagWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the flags' writer alone: every other class, the game object too, is written from what its load kept
		RegisterTownDesireFlagWriters(_writers);
		if (!GameData())
		{
			// the mesh and the scale the abodes' desire makes its flag with
			auto& row = Info().townDesire.at(static_cast<size_t>(TownDesireInfo::ForAbodes));
			row.worshipSiteMesh = MeshId::ObjectRockChalk;
			row.worshipSiteScale = 0.75f;
			row.worshipSiteSlot = 2;
		}
	}

	ClassWriters _writers;
};

/// A record kept on the entity, of the class given
template <typename Record>
entt::entity Keeping(const Record& record)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<Record>>(entity, record);
	return entity;
}

/// The record held in a variant of several classes, kept the same way
template <typename Variant>
entt::entity KeepingOne(const Variant& variant)
{
	return std::visit([](const auto& record) { return Keeping(record); }, variant);
}

/// The load's makers: the game's for the towns and their desire flags; every other class keeps its record on an
/// entity of its own, as the game's load keeps what it makes
load::Makers FlagMakers()
{
	return {
	    .footpath = [](const load::PlannedFootpath& footpath) { return Keeping(footpath.record); },
	    .footpathLink = [](const load::PlannedFootpathLink& link,
	                       const std::vector<entt::entity>& /*footpaths*/) { return Keeping(link.record); },
	    .town = load::MakeTownEntity,
	    .tree = [](const load::PlannedTree& tree, uint32_t /*forest*/) { return Keeping(tree.record); },
	    .bigForest = [](const load::PlannedBigForest& big, uint32_t /*forest*/) { return Keeping(big.record); },
	    .pile = [](const load::PlannedPile& pile) { return KeepingOne(pile.record); },
	    .storagePit = [](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                     const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return Keeping(pit.record); },
	    .citadelHeart =
	        [](const load::PlannedCitadelHeart& heart, entt::entity citadel) {
		        const auto entity = Keeping(heart.record);
		        if (citadel == entt::null)
		        {
			        Locator::entitiesRegistry::value().Assign<ecs::components::LoadedRecord<gamesave::CitadelRecord>>(
			            entity, heart.citadel);
		        }
		        return entity;
	        },
	    .abode = [](const load::PlannedAbode& abode, uint32_t /*town*/) { return KeepingOne(abode.record); },
	    .desireFlag = load::MakeDesireFlagEntity,
	    .stream = [](const load::PlannedStream& stream) { return Keeping(stream.record); },
	    .mist = [](const load::PlannedMist& mist) { return Keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	};
}

/// A load clears every object's draw-list mark and the next drawn frame lists the objects in view again; a test draws
/// no frame, so each loaded flag the save had listed is listed as that frame would
void ListAsSaved(const gamesave::WorldRead& world, const LoadedObjects& loaded)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [number, object] : loaded)
	{
		const auto& read = world.objects.at(number - 1);
		const auto* flag = read.has_value() ? std::get_if<TownDesireFlagsRecord>(&*read) : nullptr;
		if (flag != nullptr && registry.Valid(object.entity) && (flag->position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(object.entity);
		}
	}
}

/// The record each loaded flag is written as, against the record the load read. How many were written
size_t ExpectEachFlagAsRead(const gamesave::WorldRead& world, const LoadedObjects& loaded, const ClassWriters& writers,
                            const WorldSource& source)
{
	size_t written = 0;
	WriteContext context(writers, source);
	for (const auto& [number, object] : loaded)
	{
		if (object.type != TownDesireFlagsRecord::k_Type)
		{
			continue;
		}
		EXPECT_EQ(writers.ClaimsOf(source.registry, object.entity), std::vector {object.type}) << number;
		const auto record = (*writers.WriterOf(object.type))(context, object.entity);
		if (!record.has_value())
		{
			ADD_FAILURE() << "no record of " << number << ": " << context.Failure();
			continue;
		}
		EXPECT_TRUE(*record == *world.objects.at(number - 1)) << number;
		++written;
	}
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	return written;
}

/// A place on the synthetic land, `cells` cells in from its corner along x and z
gamesave::MapCoords At(int32_t cellsX, int32_t cellsZ, float altitude)
{
	return {.x = cellsX << 16, .z = cellsZ << 16, .altitude = altitude};
}

/// A loaded flag of the town of that number: its place (over the ground by `altitude`), where it was made, its row,
/// creation number and scale, its slot, desire and mesh
TownDesireFlagsRecord FlagAt(const gamesave::MapCoords& at, const gamesave::MapCoords& made, uint32_t creation,
                             ObjectNumber town, int32_t slot, TownDesireInfo desire, float level)
{
	TownDesireFlagsRecord record;
	record.thing = {.extra = 0, .flags = 0};
	record.position.position = at;
	record.position.flags = 0x8001;
	record.object.info = k_PrayerSiteRow;
	record.object.coords = made;
	record.object.creationIndex = creation;
	record.object.life = 1.0f;
	record.object.scale = 0.75f;
	record.town.number = town;
	record.slot = slot;
	record.desire = level;
	record.desireType = static_cast<uint32_t>(desire);
	record.mesh = 7;
	return record;
}

/// A loaded game as its file reads: a town with two flags, one moved to a store and up 1.5 m over the ground, one still
/// where it was made
gamesave::WorldRead SyntheticWorld()
{
	constexpr ObjectNumber k_Town = 2;
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.landNumber = 1;
	world.objects.emplace_back(game);
	TownRecord town;
	town.container.position.position = At(200, 200, 0.0f);
	town.id = 1;
	world.objects.emplace_back(town);
	world.objects.emplace_back(FlagAt(At(205, 201, 1.5f), At(203, 200, 0.0f), 31, k_Town, 2, TownDesireInfo::ForAbodes, 0.25f));
	world.objects.emplace_back(FlagAt(At(196, 199, 0.0f), At(196, 199, 0.0f), 32, k_Town, 0, TownDesireInfo::ForFood, 0.0f));
	// the reads end inside out: the flags inside the town, it inside the game
	world.loadOrder = {3, 4, 2, 1};
	return world;
}
} // namespace

// Each flag the load makes is written as its record was read
TEST_P(DesireFlagWriteWorld, EachFlagTheLoadMadeIsWrittenAsItsRecordWasRead)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), FlagMakers(), &loaded);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	EXPECT_EQ(ExpectEachFlagAsRead(world, loaded, _writers, source), 2u);
}

// A flag made in play is written from its state: the first prayer site's row, where it was made as its place and its
// target place, its desire's scale and mesh, its town, its slot (its point among the store's less the first flag's),
// its desire not yet taken, and zero for every value whose meaning is not read
TEST_P(DesireFlagWriteWorld, AFlagMadeInPlayIsWrittenFromItsState)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	const auto& row = Info().townDesire.at(static_cast<size_t>(TownDesireInfo::ForAbodes));
	const auto point = ecs::town_desire_flags::PointOf(row.worshipSiteSlot);
	ASSERT_TRUE(point.has_value());
	const auto flag = ecs::archetypes::TownDesireFlagArchetype::Create(town, TownDesireInfo::ForAbodes, *point,
	                                                                   row.worshipSiteMesh, row.worshipSiteScale, at);
	ASSERT_TRUE(flag != entt::null);
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(source.registry, flag), std::vector {TownDesireFlagsRecord::k_Type});
	const auto written = (*_writers.WriterOf(TownDesireFlagsRecord::k_Type))(context, flag);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	const auto& record = std::get<TownDesireFlagsRecord>(*written);
	EXPECT_EQ(record.object.info, k_PrayerSiteRow);
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(at.x));
	EXPECT_EQ(record.position.position.z, map_coords::ToFixed(at.z));
	EXPECT_EQ(record.position.position.altitude, 0.0f);
	EXPECT_EQ(record.object.coords.x, map_coords::ToFixed(at.x));
	EXPECT_EQ(record.object.coords.z, map_coords::ToFixed(at.z));
	EXPECT_EQ(record.object.scale, row.worshipSiteScale);
	EXPECT_EQ(record.town.number, 2u);
	EXPECT_EQ(record.slot, row.worshipSiteSlot);
	EXPECT_EQ(record.desire, 0.0f);
	EXPECT_EQ(record.desireType, static_cast<uint32_t>(TownDesireInfo::ForAbodes));
	EXPECT_EQ(record.mesh, static_cast<uint32_t>(row.worshipSiteMesh));
	EXPECT_EQ(record.thing, gamesave::ThingPart {});
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 1 autosave with its towns and their desire flags made as the game's load makes them, and every
// other object keeping its record: each flag is written as read, and the whole file, the flags by this writer, is the
// original's byte for byte
TEST_P(DesireFlagWriteWorld, TheFrozenLandOneAutosavesFlagsAreWrittenAsReadAndTheFileAsItWas)
{
	const auto bytes = FrozenLandOneAutosave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), FlagMakers(), &loaded);
	ListAsSaved(world, loaded);

	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	const auto each = ExpectEachFlagAsRead(world, loaded, _writers, source);
	EXPECT_EQ(each, 42u);

	const auto written = WriteWorld(_writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, *bytes);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type == TownDesireFlagsRecord::k_Type)
		{
			EXPECT_EQ(record.source, RecordSource::Writer) << record.number;
			++byWriter;
		}
	}
	EXPECT_EQ(byWriter, each);
}

INSTANTIATE_TEST_SUITE_P(Data, DesireFlagWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
