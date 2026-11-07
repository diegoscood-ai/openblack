/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reactions written from the game's reactions: made in play, each with its fields and the list newest first; and
// the reactions a profile's saves hold, made again by the load, written as read with their whole files as they were
// (docs/wiki/engine/saved-games.md#the-workshops-and-the-reactions)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ReactionRecord;
namespace load = openblack::ecs::saved_world_load;

namespace
{
/// A profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> ProfileSave(const std::filesystem::path& relative)
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

class ReactionWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		RegisterReactionWriters(_writers);
		if (!Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
		}
	}
	void TearDown() override
	{
		Locator::reactionSystem::reset();
		WorldHarness::TearDown();
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

/// The load's makers: the game's for the reactions; every other class keeps its record on an entity of its own
load::Makers ReactionMakers()
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
	    .desireFlag = [](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return Keeping(flag.record); },
	    .stream = [](const load::PlannedStream& stream) { return Keeping(stream.record); },
	    .mist = [](const load::PlannedMist& mist) { return Keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	    .reaction = load::MakeReaction,
	};
}

/// The players a world's game object names, kept as the players' source by their numbers, and their writer with the
/// values their records hold
void KeepPlayers(const gamesave::WorldRead& world, LoadedObjects& loaded, ClassWriters& writers,
                 std::map<PlayerNames, const gamesave::PlayerRecord*>& players)
{
	for (const auto& [number, record] : ecs::saved_players::ListPlayers(world))
	{
		loaded.insert_or_assign(
		    number, LoadedObject {.entity = entt::null, .type = gamesave::k_PlayerType, .id = record->playerNumber});
		players.emplace(static_cast<PlayerNames>(record->playerNumber), record);
	}
	std::map<gamesave::ObjectNumber, entt::entity> entityOf;
	for (const auto& [number, object] : loaded)
	{
		entityOf.emplace(number, object.entity);
	}
	RegisterPlayerWriters(writers,
	                      {.values = [&players](PlayerNames player) -> std::optional<ecs::saved_players::PlayerValues> {
		                       return ecs::saved_players::ValuesOf(*players.at(player));
	                       },
	                       .objects =
	                           [&players, entityOf](const ecs::Registry& /*registry*/, PlayerNames player) {
		                           const auto at = [&entityOf](gamesave::ObjectNumber number) {
			                           const auto found = entityOf.find(number);
			                           return found != entityOf.end() ? found->second : entt::entity {entt::null};
		                           };
		                           ecs::saved_players::PlayerObjects objects {.citadel = at(players.at(player)->citadel)};
		                           for (const auto town : players.at(player)->towns)
		                           {
			                           objects.towns.push_back(at(town));
		                           }
		                           return objects;
	                           },
	                       .newRecord = {}});
}
} // namespace

// A reaction made in play is written with its type, its start turn, whether it is active and stealthy, its radius
// and no player; the game's list holds the newest first
TEST_P(ReactionWriteWorld, AReactionMadeInPlayIsWrittenFromItsFieldsAndTheListIsNewestFirst)
{
	auto& reactions = Locator::reactionSystem::value();
	const auto first = reactions.Add({.type = openblack::Reaction::ReactToFire, .turnCreated = 12, .radius = 30.0f});
	const auto second = reactions.Add({.type = openblack::Reaction::ReactToFood, .radius = 1.0f, .available = false});
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	EXPECT_EQ(context.Keys(ReactionRecord::k_Type), (std::vector<uint64_t> {second, first}));
	const auto written = (*_writers.SourceWriterOf(ReactionRecord::k_Type))(context, first);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<ReactionRecord>(*written);
	EXPECT_EQ(record.object.number, 0u);
	EXPECT_TRUE(record.livings.numbers.empty());
	EXPECT_EQ(record.firstWord, 0u);
	EXPECT_EQ(record.secondWord, static_cast<uint32_t>(openblack::Reaction::ReactToFire));
	EXPECT_EQ(record.second.number, 0u);
	EXPECT_EQ(record.thirdWord, 12u);
	EXPECT_EQ(record.fourthWord, 1u);
	EXPECT_EQ(record.third.number, 0u);
	EXPECT_EQ(record.lastWord, std::bit_cast<uint32_t>(30.0f));
	const auto other = (*_writers.SourceWriterOf(ReactionRecord::k_Type))(context, second);
	ASSERT_TRUE(other.has_value()) << context.Failure();
	EXPECT_EQ(std::get<ReactionRecord>(*other).fourthWord, 0u);
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 1 autosave and a profile's save with many reactions: each reaction is made again by the load,
// every other object keeping its record and the players kept as their source, and the whole file, the reactions and
// their list by these writers, is the original's byte for byte
TEST_P(ReactionWriteWorld, TheSavesReactionsAreWrittenAsReadAndTheFilesAsTheyWere)
{
	const std::vector<std::filesystem::path> saves {
	    std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") / "Saved Games" / "20" / "20.sav",
	    std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "0" / "0.sav"};
	for (const auto& path : saves)
	{
		const auto bytes = ProfileSave(path);
		if (!GameData() || !bytes.has_value())
		{
			GTEST_SKIP() << "the profiles' saves are the game's data";
		}
		Locator::reactionSystem::value().Reset();
		const auto world = gamesave::ReadWorld(*bytes);
		ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd) << path;
		LoadedObjects loaded;
		std::ignore = load::Apply(load::PlanOf(world), ReactionMakers(), &loaded);
		ClassWriters writers;
		RegisterReactionWriters(writers);
		std::map<PlayerNames, const gamesave::PlayerRecord*> players;
		KeepPlayers(world, loaded, writers, players);
		size_t made = 0;
		for (const auto& [number, object] : loaded)
		{
			made += object.type == ReactionRecord::k_Type ? 1 : 0;
		}
		EXPECT_GT(made, 0u) << path;

		const auto written = WriteWorld(
		    writers, {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
		ASSERT_TRUE(written.Complete()) << path << ": " << written.failure;
		EXPECT_TRUE(written.bytes == *bytes) << path;
		size_t byWriter = 0;
		for (const auto& record : written.records)
		{
			if (record.type == ReactionRecord::k_Type)
			{
				EXPECT_EQ(record.source, RecordSource::Writer) << path << " " << record.number;
				++byWriter;
			}
		}
		EXPECT_EQ(byWriter, made) << path;
	}
}

INSTANTIATE_TEST_SUITE_P(Data, ReactionWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
