/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The citadels' hearts written from the entities the load made of them: a Land 1 save of a player's profile that holds
// a built temple, its heart's record as read and its whole world file as it was, and a heart made in play, which is not
// written yet (docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <TownRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Temple.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::CitadelEntranceRecord;
using gamesave::CitadelHeartRecord;
using gamesave::CitadelRecord;
using gamesave::SavedRecord;
namespace load = openblack::ecs::saved_world_load;

namespace
{
/// The rows of the citadel heart and the citadel, and the entrance's, which has none
constexpr uint32_t k_CitadelHeartRow = 2411;
constexpr uint32_t k_CitadelRow = 2412;
constexpr uint32_t k_NoRow = 0xFFFFFFFF;

/// A Land 1 save of a player's profile with its temple built, or nothing without the game's data
std::optional<std::vector<uint8_t>> TempleSave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / "_J_a_h_o_v_i_a" / "Saved Games" / "16" / "1" / "16.sav";
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

class HeartWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the hearts' writer alone: every other class, the game object too, is written from what its load kept
		RegisterCitadelHeartWriters(_writers);
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

/// The load's makers: the game's for the citadels' hearts; every other class keeps its record on an
/// entity of its own, as the game's load keeps what it makes
load::Makers HeartMakers()
{
	return {
	    .footpath = [](const load::PlannedFootpath& footpath) { return Keeping(footpath.record); },
	    .footpathLink = [](const load::PlannedFootpathLink& link,
	                       const std::vector<entt::entity>& /*footpaths*/) { return Keeping(link.record); },
	    .town = [](const load::PlannedTown& town) { return Keeping(town.record); },
	    .tree = [](const load::PlannedTree& tree, uint32_t /*forest*/) { return Keeping(tree.record); },
	    .bigForest = [](const load::PlannedBigForest& big, uint32_t /*forest*/) { return Keeping(big.record); },
	    .pile = [](const load::PlannedPile& pile) { return KeepingOne(pile.record); },
	    .storagePit = [](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                     const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return Keeping(pit.record); },
	    .citadelHeart = load::MakeCitadelHeartEntity,
	    .abode = [](const load::PlannedAbode& abode, uint32_t /*town*/) { return KeepingOne(abode.record); },
	    .desireFlag = [](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return Keeping(flag.record); },
	    .stream = [](const load::PlannedStream& stream) { return Keeping(stream.record); },
	    .mist = [](const load::PlannedMist& mist) { return Keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	};
}

/// A load clears every object's draw-list mark and the next drawn frame lists the objects in view again; a test draws
/// no frame, so each loaded heart the save had listed is listed as that frame would
void ListAsSaved(const gamesave::WorldRead& world, const LoadedObjects& loaded)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [number, object] : loaded)
	{
		const auto& read = world.objects.at(number - 1);
		const auto* heart = read.has_value() ? std::get_if<CitadelHeartRecord>(&*read) : nullptr;
		if (heart != nullptr && registry.Valid(object.entity) &&
		    (heart->base.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(object.entity);
		}
	}
}

/// The record each loaded heart is written as, against the record the load read. How many were written
size_t ExpectEachHeartAsRead(const gamesave::WorldRead& world, const LoadedObjects& loaded, const ClassWriters& writers,
                             const WorldSource& source)
{
	size_t written = 0;
	WriteContext context(writers, source);
	for (const auto& [number, object] : loaded)
	{
		if (object.type != CitadelHeartRecord::k_Type)
		{
			continue;
		}
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
} // namespace

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. A Land 1 save with its temple built, its heart made as the game's load makes it and every other object keeping
// its record: the heart's entrance, made with it, stands for the entrance the heart's record names; the heart is
// written as read, and the whole file, the heart by this writer, is the original's byte for byte
TEST_P(HeartWriteWorld, ALandOneSavesHeartIsWrittenAsReadAndTheFileAsItWas)
{
	const auto bytes = TempleSave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the profile's Land 1 save is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), HeartMakers(), &loaded);
	ListAsSaved(world, loaded);
	size_t entrances = 0;
	for (const auto& [number, object] : loaded)
	{
		const auto& read = world.objects.at(number - 1);
		const auto* heart = read.has_value() ? std::get_if<CitadelHeartRecord>(&*read) : nullptr;
		if (heart == nullptr || object.type != CitadelHeartRecord::k_Type)
		{
			continue;
		}
		ASSERT_TRUE(loaded.contains(heart->entrance.number));
		const auto& entrance = loaded.at(heart->entrance.number);
		EXPECT_EQ(entrance.type, CitadelEntranceRecord::k_Type);
		const auto* of = Registry().TryGet<const ecs::components::CitadelEntrance>(entrance.entity);
		ASSERT_NE(of, nullptr);
		EXPECT_TRUE(of->heart == object.entity);
		++entrances;
	}
	EXPECT_EQ(entrances, 1u);
	const auto line = std::ranges::find(census, CitadelEntranceRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, line->saved);

	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	EXPECT_EQ(ExpectEachHeartAsRead(world, loaded, _writers, source), 1u);

	const auto written = WriteWorld(_writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, *bytes);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type == CitadelHeartRecord::k_Type)
		{
			EXPECT_EQ(record.source, RecordSource::Writer) << record.number;
			++byWriter;
		}
	}
	EXPECT_EQ(byWriter, 1u);
}

// A heart made in play is written with its citadel and its entrance: the heart's row, its part's influence, its
// citadel and its entrance; the citadel at the heart's place, of the citadel's row and the heart's player, with its
// heart, six empty worship site slots, the heart as its one piece, its influence and the other lists empty; the
// entrance at the heart's place, with no row, the creation number after the heart's and counting as fixed
TEST_P(HeartWriteWorld, AHeartMadeInPlayIsWrittenWithItsCitadelAndItsEntrance)
{
	const glm::vec3 at(1500.0f, k_Ground, 1500.0f);
	const auto next = ecs::object_index::Peek();
	const auto heart =
	    ecs::archetypes::CitadelArchetype::CreateHeart(at, PlayerNames::PLAYER_ONE, entt::null, 0.0f, 1.0f, 1.0f, false, false);
	ASSERT_TRUE(heart != entt::null);
	auto& registry = Registry();
	// the heart takes the first of the five numbers its creation takes, as the game's counter gives them
	EXPECT_EQ(ecs::object_index::Of(heart), static_cast<int64_t>(next));
	EXPECT_EQ(ecs::object_index::Peek(), next + 5);
	const auto& made = registry.Get<const ecs::components::CitadelHeart>(heart);
	ASSERT_EQ(made.citadel, heart);
	ASSERT_TRUE(made.entrance != entt::null);
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(source.registry, heart), std::vector {CitadelHeartRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(source.registry, made.entrance), std::vector {CitadelEntranceRecord::k_Type});
	const auto heartNumber = context.Reference(heart, CitadelHeartRecord::k_Type);

	const auto writtenHeart = (*_writers.WriterOf(CitadelHeartRecord::k_Type))(context, heart);
	ASSERT_TRUE(writtenHeart.has_value()) << context.Failure();
	const auto& heartRecord = std::get<CitadelHeartRecord>(*writtenHeart);
	EXPECT_EQ(heartRecord.base.object.info, k_CitadelHeartRow);
	EXPECT_EQ(heartRecord.base.position.position.altitude, 0.0f);
	EXPECT_EQ(heartRecord.influence, Info().citadelHeart.influence);
	EXPECT_EQ(heartRecord.citadel.number, context.Reference(heart, CitadelRecord::k_Type));
	EXPECT_EQ(heartRecord.entrance.number, context.Reference(made.entrance));
	EXPECT_EQ(heartRecord.other.number, 0u);
	EXPECT_EQ(heartRecord.specularColour, 0u);

	const auto writtenCitadel = (*_writers.WriterOf(CitadelRecord::k_Type))(context, heart);
	ASSERT_TRUE(writtenCitadel.has_value()) << context.Failure();
	const auto& citadel = std::get<CitadelRecord>(*writtenCitadel);
	EXPECT_EQ(citadel.container.info, k_CitadelRow);
	EXPECT_EQ(citadel.container.position.position, heartRecord.base.position.position);
	EXPECT_EQ(citadel.container.position.flags, 0u);
	EXPECT_EQ(citadel.heart.number, heartNumber);
	EXPECT_EQ(citadel.parts.numbers, std::vector<gamesave::ObjectNumber>(6, 0));
	EXPECT_EQ(citadel.firstList.numbers, std::vector {heartNumber});
	EXPECT_TRUE(citadel.secondList.numbers.empty());
	EXPECT_TRUE(citadel.thirdList.numbers.empty());
	EXPECT_TRUE(citadel.fourthList.numbers.empty());
	// written before the influence's turn has stored it: the influence its first heart fixes, the same as stored after
	EXPECT_FALSE(registry.AllOf<ecs::components::CitadelInfluence>(heart));
	std::ignore = influence::CitadelRadius(heart);
	EXPECT_EQ(std::bit_cast<float>(citadel.firstWord), registry.Get<const ecs::components::CitadelInfluence>(heart).reach);
	EXPECT_EQ(citadel.thirdWord, 0);

	const auto writtenEntrance = (*_writers.WriterOf(CitadelEntranceRecord::k_Type))(context, made.entrance);
	ASSERT_TRUE(writtenEntrance.has_value()) << context.Failure();
	const auto& entrance = std::get<CitadelEntranceRecord>(*writtenEntrance);
	EXPECT_EQ(entrance.position.position, heartRecord.base.position.position);
	EXPECT_EQ(entrance.position.flags, position_flags::k_CountsAsFixed);
	EXPECT_EQ(entrance.object.info, k_NoRow);
	EXPECT_EQ(entrance.object.coords, entrance.position.position);
	EXPECT_EQ(heartRecord.base.object.creationIndex, next);
	EXPECT_EQ(entrance.object.creationIndex, next + 1);
	EXPECT_EQ(entrance.object.life, 1.0f);
	EXPECT_EQ(entrance.object.scale, 1.0f);
	EXPECT_EQ(entrance.object.heading, 0.0f);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// Integration test: needs the original game's data and the original's own save of a new Land 1 game (local reference
// data beside the game's folder); skipped without them. Its citadel, heart and entrance hold what the writer gives a
// heart made in play: the citadel at the heart's place with no place flags, of the citadel's row, the heart its one
// piece, the other three lists empty and its worship site slots empty; the heart of its row with its part's influence
// and no specular colour; the entrance at the heart's place, counting as fixed, with no row, the heart's creation
// number plus one, whole, at scale 1 and turned 0
TEST_P(HeartWriteWorld, TheOriginalsNewGameCitadelHoldsWhatANewHeartStartsWith)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!GameData() || game == nullptr)
	{
		GTEST_SKIP() << "the original's save of a new Land 1 game is local reference data";
	}
	std::ifstream file(std::filesystem::path(game).parent_path() / "original_saves" / "land1_new_answer3" /
	                       "profile__e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" / "Saved Games" / "20" /
	                       "20.sav",
	                   std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no save of a new Land 1 game made by the original";
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
	const auto world = gamesave::ReadWorld(bytes);
	const auto recordAt = [&world](gamesave::ObjectNumber number) -> const gamesave::SavedRecord* {
		return number != 0 && number <= world.objects.size() && world.objects.at(number - 1).has_value()
		           ? &*world.objects.at(number - 1)
		           : nullptr;
	};
	size_t citadels = 0;
	for (size_t index = 0; index < world.objects.size(); ++index)
	{
		const auto* citadel =
		    world.objects.at(index).has_value() ? std::get_if<CitadelRecord>(&*world.objects.at(index)) : nullptr;
		if (citadel == nullptr)
		{
			continue;
		}
		++citadels;
		const auto* heartRead = recordAt(citadel->heart.number);
		ASSERT_NE(heartRead, nullptr);
		const auto& heart = std::get<CitadelHeartRecord>(*heartRead);
		EXPECT_EQ(citadel->container.info, k_CitadelRow);
		EXPECT_EQ(citadel->container.position.position, heart.base.position.position);
		EXPECT_EQ(citadel->container.position.flags, 0u);
		EXPECT_EQ(citadel->parts.numbers, std::vector<gamesave::ObjectNumber>(6, 0));
		EXPECT_EQ(citadel->firstList.numbers, std::vector {citadel->heart.number});
		EXPECT_TRUE(citadel->secondList.numbers.empty());
		EXPECT_TRUE(citadel->thirdList.numbers.empty());
		EXPECT_TRUE(citadel->fourthList.numbers.empty());
		EXPECT_EQ(citadel->secondWord, 0u);
		EXPECT_EQ(citadel->thirdWord, 0);
		EXPECT_EQ(heart.base.object.info, k_CitadelHeartRow);
		EXPECT_EQ(heart.base.position.position.altitude, 0.0f);
		EXPECT_EQ(heart.influence, Info().citadelHeart.influence);
		EXPECT_EQ(heart.other.number, 0u);
		EXPECT_EQ(heart.specularColour, 0u);
		const auto* entranceRead = recordAt(heart.entrance.number);
		ASSERT_NE(entranceRead, nullptr);
		const auto& entrance = std::get<CitadelEntranceRecord>(*entranceRead);
		EXPECT_EQ(entrance.thing.extra, 0u);
		EXPECT_EQ(entrance.thing.destroyed, 0u);
		EXPECT_EQ(entrance.thing.flags, 0u);
		EXPECT_EQ(entrance.position.position, heart.base.position.position);
		EXPECT_EQ(entrance.position.flags, position_flags::k_CountsAsFixed);
		EXPECT_EQ(entrance.object.info, k_NoRow);
		EXPECT_EQ(entrance.object.coords, entrance.position.position);
		EXPECT_EQ(entrance.object.creationIndex, heart.base.object.creationIndex + 1);
		EXPECT_EQ(entrance.object.life, 1.0f);
		EXPECT_EQ(entrance.object.scale, 1.0f);
		EXPECT_EQ(entrance.object.heading, 0.0f);
		EXPECT_FALSE(entrance.object.physics.has_value());
	}
	EXPECT_EQ(citadels, 1u);
}

INSTANTIATE_TEST_SUITE_P(Data, HeartWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
