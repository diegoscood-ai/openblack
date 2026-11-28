/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns written from their entities: a loaded town as its record was read with the values its load put back, a
// town made in play from its components, the values a new town starts with as the original's own save of a new game
// has them, and the frozen Land 1 autosave's towns as read and its whole world file as it was
// (docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <AnimalRecords.h>
#include <EffectRecords.h>
#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MissionaryControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Trees.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ObjectNumber;
using gamesave::SavedRecord;
using gamesave::TownRecord;
namespace load = openblack::ecs::saved_world_load;

namespace
{
/// The row every town of the original's saves is of
constexpr uint32_t k_TownRow = 2081;
/// The words the original's belief block, figures block and each player's last attack start with
constexpr uint32_t k_BeliefBlockWord = 0x00999FEC;
constexpr uint32_t k_FiguresBlockWord = 0x0099A00C;
constexpr uint32_t k_AttackBlockWord = 0x008D8BA8;
/// The bytes of each player's dealings with a town, and where in them the trust and the last attack's block are
constexpr size_t k_PlayerInteractBytes = 0x80;
constexpr size_t k_TrustAt = 0x10;
constexpr size_t k_AttackAt = 0x3C;

template <typename T>
[[nodiscard]] T ValueAt(std::span<const uint8_t> bytes, size_t at)
{
	T value {};
	std::memcpy(&value, bytes.data() + at, sizeof(T));
	return value;
}

/// The file of a save, or nothing without it
std::optional<std::vector<uint8_t>> FileOf(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

constexpr const char* k_Profile = "_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v";

/// The frozen Land 1 autosave, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenLandOneAutosave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return FileOf(std::filesystem::path(game) / "Profiles" / k_Profile / "Saved Games" / "20" / "20.sav");
}

/// The original's own save of a new Land 1 game, made as its temple was built (local reference data beside the frozen
/// copy of the game), or nothing without it
std::optional<std::vector<uint8_t>> OriginalNewLandOneSave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return FileOf(std::filesystem::path(game).parent_path() / "original_saves" / "land1_new_answer3" /
	              (std::string("profile_") + k_Profile) / "Saved Games" / "20" / "20.sav");
}

class TownWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the towns' writer alone: every other class, the game object too, is written from what its load kept
		RegisterTownWriters(_writers);
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

/// The load's makers: the game's for the towns; every other class keeps its record on an entity of its own, as the
/// game's load keeps what it makes
load::Makers TownMakers()
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
	    .desireFlag = [](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return Keeping(flag.record); },
	    .stream = [](const load::PlannedStream& stream) { return Keeping(stream.record); },
	    .mist = [](const load::PlannedMist& mist) { return Keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	};
}

/// The record each loaded town is written as, against the record the load read. How many were written
size_t ExpectEachTownAsRead(const gamesave::WorldRead& world, const LoadedObjects& loaded, const ClassWriters& writers,
                            const WorldSource& source)
{
	size_t written = 0;
	WriteContext context(writers, source);
	for (const auto& [number, object] : loaded)
	{
		if (object.type != TownRecord::k_Type)
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

/// A loaded game as its file reads: a town with values of its own in every part
gamesave::WorldRead SyntheticWorld()
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.landNumber = 1;
	world.objects.emplace_back(game);
	TownRecord town;
	town.container.position.position = {.x = 200 << 16, .z = 200 << 16, .altitude = 0.0f};
	town.container.info = k_TownRow;
	town.id = 4;
	town.tribe = 2;
	town.playerNumber = 7;
	town.worshipPercentage = 0.25f;
	town.influence = 30.0f;
	town.beliefInNeutralPlayer = 0.5f;
	town.uninhabitable = 1;
	town.areaMin = {.x = 190 << 16, .z = 195 << 16, .altitude = 0.0f};
	town.areaMax = {.x = 210 << 16, .z = 205 << 16, .altitude = 0.0f};
	town.stats.at(0) = 0x0C;
	town.belief.at(0) = 0xEC;
	town.playerInteract.at(k_TrustAt) = 1;
	town.balanceBeliefScale = 1.0f;
	town.emptyTownTimer = 12;
	world.objects.emplace_back(town);
	world.loadOrder = {2, 1};
	return world;
}
} // namespace

// Each town the load makes is written as its record was read
TEST_P(TownWriteWorld, EachTownTheLoadMadeIsWrittenAsItsRecordWasRead)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	EXPECT_EQ(ExpectEachTownAsRead(world, loaded, _writers, source), 1u);
}

// A loaded town gets back the things to play with its record lists, the newest first as saved, each as the entity the
// load made of it (footballs, the only things the game gives a town to play with), and is written back as it was read
TEST_P(TownWriteWorld, ALoadedTownGetsItsPlaythingsBackAndWritesThemAsRead)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_OlderBall = 3;
	constexpr ObjectNumber k_NewerBall = 4;
	auto world = SyntheticWorld();
	auto& game = std::get<gamesave::GameHead>(*world.objects.at(0));
	game.lists[static_cast<size_t>(GlobalList::Balls)].numbers = {k_NewerBall, k_OlderBall};
	std::get<TownRecord>(*world.objects.at(k_Town - 1)).playthings.numbers = {k_NewerBall, k_OlderBall};
	gamesave::BallRecord ball;
	ball.base.object.info = 2082;
	ball.town = {k_Town};
	world.objects.emplace_back(ball);
	world.objects.emplace_back(ball);
	world.loadOrder = {k_Town, k_OlderBall, k_NewerBall, 1};
	auto makers = TownMakers();
	makers.ball = [](const load::PlannedBall& planned, entt::entity /*town*/) { return Keeping(planned.record); };
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), makers, &loaded);
	ASSERT_TRUE(loaded.contains(k_Town) && loaded.contains(k_OlderBall) && loaded.contains(k_NewerBall));
	const auto& town = Registry().Get<const ecs::components::Town>(loaded.at(k_Town).entity);
	EXPECT_EQ(town.playthings, (std::vector {loaded.at(k_NewerBall).entity, loaded.at(k_OlderBall).entity}));

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	EXPECT_EQ(ExpectEachTownAsRead(world, loaded, _writers, source), 1u);
}

// A loaded town gets its plans back in its list's order, each of its row, at its place, with its angle, size, creation
// turn and whether it was a building, and its plans writer gives each back as it was read
TEST_P(TownWriteWorld, ALoadedTownGetsItsPlansBack)
{
	auto world = SyntheticWorld();
	gamesave::PlannedAbodeRecord plan;
	plan.planned.position.position = {.x = 201 << 16, .z = 199 << 16, .altitude = 0.0f};
	plan.planned.yAngle = 1.5f;
	plan.planned.scale = 1.25f;
	plan.planned.creationTurn = 40;
	plan.planned.info = load::AbodeInfoRowOf(AbodeInfo::CelticHut);
	plan.town.number = 2;
	world.objects.emplace_back(plan);
	std::get<TownRecord>(*world.objects.at(1)).planned.numbers = {3};
	world.loadOrder = {3, 2, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto town = loaded.at(2).entity;
	const auto& plans = Registry().Get<const ecs::components::Town>(town).plannedAbodes;
	ASSERT_EQ(plans.size(), 1u);
	EXPECT_EQ(plans.at(0).info, AbodeInfo::CelticHut);
	EXPECT_EQ(plans.at(0).yAngleRadians, 1.5f);
	EXPECT_EQ(plans.at(0).scale, 1.25f);
	EXPECT_EQ(plans.at(0).creationTurn, 40u);
	EXPECT_FALSE(plans.at(0).townCentre);
	EXPECT_FALSE(plans.at(0).wasBuilt);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.SourceWriterOf(gamesave::PlannedAbodeRecord::k_Type))(context, PartId(town, 0));
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::PlannedAbodeRecord>(*written).planned, plan.planned);
	// the town names its plans as they stand: the loaded one under its number in the file, and one added since (a
	// building's rebuild plan) after it
	auto& live = Registry().Get<ecs::components::Town>(town).plannedAbodes;
	live.push_back(live.at(0));
	WriteContext again(_writers, source);
	const auto record = (*_writers.WriterOf(TownRecord::k_Type))(again, town);
	ASSERT_TRUE(record.has_value()) << again.Failure();
	const auto& planned = std::get<TownRecord>(*record).planned.numbers;
	ASSERT_EQ(planned.size(), 2u);
	EXPECT_EQ(planned.at(0), 3u);
	EXPECT_EQ(planned.at(1), again.Reference(SourceKey {.type = gamesave::PlannedAbodeRecord::k_Type, .id = PartId(town, 1)}));
}

// A loaded town gets its planned citadel heart back, of the citadel heart's row, as the original's load reads it (the
// plan's parts and its town), counted as made, and it is written back as it was read; one of a row the game's tables
// do not hold stays out
TEST_P(TownWriteWorld, ALoadedTownGetsItsPlannedCitadelHeartBack)
{
	auto world = SyntheticWorld();
	gamesave::PlannedCitadelHeartRecord heart;
	heart.planned.position.position = {.x = 205 << 16, .z = 195 << 16, .altitude = 0.0f};
	heart.planned.yAngle = 0.5f;
	heart.planned.scale = 1.0f;
	heart.planned.creationTurn = 12;
	heart.planned.info = 2411;
	heart.town.number = 2;
	world.objects.emplace_back(heart);
	auto other = heart;
	other.planned.info = 2410;
	world.objects.emplace_back(other);
	std::get<TownRecord>(*world.objects.at(1)).planned.numbers = {3, 4};
	world.loadOrder = {4, 3, 2, 1};
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto line = std::ranges::find(census, gamesave::PlannedCitadelHeartRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->saved, 2u);
	EXPECT_EQ(line->made, 1u);
	ASSERT_TRUE(loaded.contains(2));
	const auto town = loaded.at(2).entity;
	const auto& plans = Registry().Get<const ecs::components::Town>(town).plannedAbodes;
	ASSERT_EQ(plans.size(), 1u);
	EXPECT_TRUE(plans.at(0).citadelHeart);
	EXPECT_EQ(plans.at(0).heartInfo, 0u);
	EXPECT_EQ(plans.at(0).info, AbodeInfo::None);
	EXPECT_FALSE(plans.at(0).townCentre);
	EXPECT_EQ(plans.at(0).creationTurn, 12u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.SourceWriterOf(gamesave::PlannedCitadelHeartRecord::k_Type))(context, PartId(town, 0));
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::PlannedCitadelHeartRecord>(*written).planned, heart.planned);
}

// The load's census counts a planned abode as made when its town takes it back as a plan; one no town or scaffold
// takes back stays unmade
TEST_P(TownWriteWorld, TheCensusCountsThePlansATownTakesBackAsMade)
{
	auto world = SyntheticWorld();
	gamesave::PlannedAbodeRecord plan;
	plan.planned.position.position = {.x = 201 << 16, .z = 199 << 16, .altitude = 0.0f};
	plan.planned.info = load::AbodeInfoRowOf(AbodeInfo::CelticHut);
	plan.town.number = 2;
	world.objects.emplace_back(plan);
	world.objects.emplace_back(plan);
	std::get<TownRecord>(*world.objects.at(1)).planned.numbers = {3};
	world.loadOrder = {4, 3, 2, 1};
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto line = std::ranges::find(census, gamesave::PlannedAbodeRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->saved, 2u);
	EXPECT_EQ(line->made, 1u);
}

// A loaded town is written with the values its load put back on it as they stand: its belief in the neutral player,
// whether it may be lived in and its area; everything else as its load kept it
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithTheValuesItsLoadPutBack)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& town = Registry().Get<ecs::components::Town>(entity);
	town.belief.beliefInNeutralPlayer = 0.75f;
	town.uninhabitable = false;
	town.areaMin = {180 << 16, 185 << 16};
	town.areaMax = {220 << 16, 215 << 16};
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	auto expected = std::get<TownRecord>(*world.objects.at(1));
	expected.beliefInNeutralPlayer = 0.75f;
	expected.uninhabitable = 0;
	expected.areaMin.x = 180 << 16;
	expected.areaMin.z = 185 << 16;
	expected.areaMax.x = 220 << 16;
	expected.areaMax.z = 215 << 16;
	EXPECT_TRUE(std::get<TownRecord>(*written) == expected);
}

// A loaded town is written without the buildings destroyed since its load, as a building leaves its town's lists as
// it is deleted: its abodes, building sites and workshops in their saved order without them, and a creche gone replaced
// by the one the town holds now
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithoutTheBuildingsDestroyedSinceItsLoad)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& registry = Registry();
	const auto standing = registry.Create();
	const auto burnt = registry.Create();
	const auto site = registry.Create();
	const auto workshop = registry.Create();
	for (const auto& [number, made] : std::initializer_list<std::pair<gamesave::ObjectNumber, entt::entity>> {
	         {20, standing}, {21, burnt}, {22, site}, {23, workshop}})
	{
		loaded.try_emplace(number, LoadedObject {.entity = made, .type = gamesave::AbodeRecord::k_Type});
	}
	auto& kept = registry.Get<ecs::components::LoadedRecord<TownRecord>>(entity).record;
	kept.abodes.numbers = {21, 20};
	kept.buildingSites.numbers = {22};
	kept.workshops.numbers = {23};
	kept.creche.number = 21;
	kept.graveyard.number = 20;
	kept.mainStoragePit.number = 21;
	kept.townCentre.number = 21;
	kept.worshipSite.number = 21;
	// the standing abode is of the class its kept record gives it, as the town names it
	registry.Assign<ecs::components::LoadedRecord<gamesave::AbodeRecord>>(standing);
	registry.Get<ecs::components::Town>(entity).creche = standing;
	registry.Get<ecs::components::Town>(entity).storagePit = standing;
	registry.Get<ecs::components::Town>(entity).centre = entt::null;
	registry.Destroy(burnt);
	registry.Destroy(site);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& town = std::get<TownRecord>(*written);
	EXPECT_EQ(town.abodes.numbers, (std::vector<gamesave::ObjectNumber> {20}));
	EXPECT_TRUE(town.buildingSites.numbers.empty());
	EXPECT_EQ(town.workshops.numbers, (std::vector<gamesave::ObjectNumber> {23}));
	EXPECT_EQ(town.creche.number, 20u);
	EXPECT_EQ(town.graveyard.number, 20u);
	// a main storage pit and a town centre gone: the ones it holds now (the pit standing, no centre)
	EXPECT_EQ(town.mainStoragePit.number, 20u);
	EXPECT_EQ(town.townCentre.number, 0u);
	// a worship site gone: none
	EXPECT_EQ(town.worshipSite.number, 0u);
}

// A loaded town is written without the villagers gone since its load, as a villager leaves its town as it is deleted:
// out of its homeless villagers; those on their way to its worship site are written as the town holds them, a villager
// gone since out of the list
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithoutTheVillagersGoneSinceItsLoad)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& registry = Registry();
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	const auto walker = registry.Create();
	loaded.try_emplace(30, LoadedObject {.entity = alive, .type = gamesave::SpecialVillagerRecord::k_Type});
	loaded.try_emplace(31, LoadedObject {.entity = dead, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(32, LoadedObject {.entity = walker, .type = gamesave::VillagerRecord::k_Type});
	auto& kept = registry.Get<ecs::components::LoadedRecord<TownRecord>>(entity).record;
	kept.homeless.numbers = {31, 30};
	// the town's homeless as its load left them, the one gone since out with its entity
	registry.Get<ecs::components::Town>(entity).homelessVillagers = {dead, alive};
	// the villagers as their load kept them: a special villager's record is of its own class
	registry.Assign<ecs::components::LoadedRecord<gamesave::SpecialVillagerRecord>>(alive);
	registry.Assign<ecs::components::LoadedRecord<gamesave::VillagerRecord>>(walker);
	auto& magic = registry.Get<ecs::components::TownMagic>(entity);
	magic.onWayVillagers = {walker, alive};
	magic.onWayToWorship = 1;
	registry.Destroy(dead);
	registry.Destroy(walker);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& town = std::get<TownRecord>(*written);
	EXPECT_EQ(town.homeless.numbers, (std::vector<gamesave::ObjectNumber> {30}));
	EXPECT_EQ(town.villagersOnWayToWorshipSite.numbers, (std::vector<gamesave::ObjectNumber> {30}));
	EXPECT_EQ(town.villagersOnWayToWorship, 1u);
}

// A loaded town writes its homeless villagers as it holds them: its record's in their order, without one housed since,
// and one made homeless since (a grown child leaving a crowded home) at the head, as each joins the head of the list;
// before, one made homeless since was written in no list at all
TEST_P(TownWriteWorld, ALoadedTownWritesTheVillagersMadeHomelessSinceItsLoadAtTheHead)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& registry = Registry();
	const auto stays = registry.Create();
	const auto housed = registry.Create();
	const auto grown = registry.Create();
	loaded.try_emplace(30, LoadedObject {.entity = stays, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(31, LoadedObject {.entity = housed, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(32, LoadedObject {.entity = grown, .type = gamesave::VillagerRecord::k_Type});
	for (const auto villager : {stays, housed, grown})
	{
		registry.Assign<ecs::components::LoadedRecord<gamesave::VillagerRecord>>(villager);
	}
	auto& kept = registry.Get<ecs::components::LoadedRecord<TownRecord>>(entity).record;
	kept.homeless.numbers = {31, 30};
	// since the load: the one housed has left the list, the grown child has joined its head
	registry.Get<ecs::components::Town>(entity).homelessVillagers = {grown, stays};
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).homeless.numbers, (std::vector<gamesave::ObjectNumber> {32, 30}));
}

// A loaded town takes back its worship share, those worshipping, those on their way and the share kept through an
// emergency, as the original's town load reads them, and writes them as they stand: before, a loaded town held a share
// of 0 and sent nobody to worship until its player set it again with the totem
TEST_P(TownWriteWorld, ALoadedTownTakesBackItsWorshipShareAndWritesItAsItStands)
{
	auto world = SyntheticWorld();
	auto& saved = std::get<TownRecord>(*world.objects.at(1));
	saved.worshipPercentage = 0.375f;
	saved.worshipCount = 4;
	saved.villagersOnWayToWorship = 2;
	saved.worshipPercentageBeforeEmergency = 0.5f;
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& magic = Registry().Get<ecs::components::TownMagic>(entity);
	EXPECT_FLOAT_EQ(magic.worshipPercentage, 0.375f);
	EXPECT_EQ(magic.worshipping, 4);
	EXPECT_EQ(magic.onWayToWorship, 2);
	auto& town = Registry().Get<ecs::components::Town>(entity);
	EXPECT_FLOAT_EQ(town.savedWorshipPercentage, 0.5f);
	// and its influence's radius, as saved (30 in the synthetic town)
	EXPECT_FLOAT_EQ(Registry().Get<ecs::components::TownInfluence>(entity).radius, 30.0f);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	{
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_TRUE(std::get<TownRecord>(*written) == saved);
	}
	// the totem dragged since, and a worshipper back home
	magic.worshipPercentage = 0.75f;
	magic.worshipping = 3;
	town.savedWorshipPercentage = 0.0f;
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<TownRecord>(*written);
	EXPECT_FLOAT_EQ(record.worshipPercentage, 0.75f);
	EXPECT_EQ(record.worshipCount, 3);
	EXPECT_EQ(record.villagersOnWayToWorship, 2u);
	EXPECT_FLOAT_EQ(record.worshipPercentageBeforeEmergency, 0.0f);
}

// A loaded town is written without the animals, spell icons, artifacts, flocks and fish farms gone since its load, as
// each leaves its town's list as it is deleted
// A loaded town is written without a building site its load did not make (one saved naming no building, which no
// original site is): before, the town's kept list named it and the save wrote the site back, so every later load left
// it out again
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithoutABuildingSiteItsLoadDidNotMake)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& registry = Registry();
	const auto standing = registry.Create();
	registry.Assign<ecs::components::BuildingSite>(standing);
	loaded.try_emplace(30, LoadedObject {.entity = standing, .type = gamesave::StandardBuildingSiteRecord::k_Type});
	loaded.try_emplace(31, LoadedObject {.entity = entt::null, .type = gamesave::StandardBuildingSiteRecord::k_Type});
	registry.Get<ecs::components::LoadedRecord<TownRecord>>(entity).record.buildingSites.numbers = {31, 30};
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).buildingSites.numbers, std::vector<ObjectNumber> {30});
}

TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithoutItsOtherThingsGoneSinceItsLoad)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& registry = Registry();
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	loaded.try_emplace(30, LoadedObject {.entity = alive, .type = gamesave::FlockRecord::k_Type});
	loaded.try_emplace(31, LoadedObject {.entity = dead, .type = gamesave::FlockRecord::k_Type});
	auto& kept = registry.Get<ecs::components::LoadedRecord<TownRecord>>(entity).record;
	for (auto* list : {&kept.animals, &kept.spellIcons, &kept.artifacts, &kept.flocks, &kept.fishFarms})
	{
		list->numbers = {31, 30};
	}
	// its spell icons are written as it holds them, which the load made of the same list
	const auto aliveIcon = registry.Create();
	const auto deadIcon = registry.Create();
	loaded.try_emplace(32, LoadedObject {.entity = aliveIcon, .type = gamesave::TownCentreSpellIconRecord::k_Type});
	loaded.try_emplace(33, LoadedObject {.entity = deadIcon, .type = gamesave::TownCentreSpellIconRecord::k_Type});
	kept.spellIcons.numbers = {33, 32};
	registry.Get<ecs::components::TownMagic>(entity).spellIcons = {deadIcon, aliveIcon};
	registry.Destroy(deadIcon);
	registry.Destroy(dead);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& town = std::get<TownRecord>(*written);
	const std::vector<gamesave::ObjectNumber> left {30};
	EXPECT_EQ(town.animals.numbers, left);
	EXPECT_EQ(town.spellIcons.numbers, std::vector<gamesave::ObjectNumber> {32});
	EXPECT_EQ(town.artifacts.numbers, left);
	EXPECT_EQ(town.flocks.numbers, left);
	EXPECT_EQ(town.fishFarms.numbers, left);
}

// A loaded town writes its forests as it holds them: a forest a villager deleted since its load (found empty) has left
// its list, as it leaves every town's list as it is deleted. Before, the town's record kept the forest and the save
// was refused for a forest that was gone
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithoutTheForestsDeletedSinceItsLoad)
{
	auto world = SyntheticWorld();
	gamesave::ForestRecord forest;
	forest.container.position.position = {.x = 210 << 16, .z = 200 << 16, .altitude = 0.0f};
	forest.id = 20;
	world.objects.emplace_back(forest);
	forest.id = 21;
	world.objects.emplace_back(forest);
	std::get<gamesave::GameHead>(*world.objects.at(0)).lists[4].numbers = {3, 4};
	std::get<TownRecord>(*world.objects.at(1)).forests.numbers = {4, 3};
	world.loadOrder = {2, 3, 4, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	{
		// as loaded: its forests in their saved order
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_EQ(std::get<TownRecord>(*written).forests.numbers, (std::vector<ObjectNumber> {4, 3}));
	}
	ASSERT_TRUE(loaded.at(4).id.has_value());
	ecs::DeleteForest(static_cast<uint32_t>(*loaded.at(4).id));
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).forests.numbers, std::vector<ObjectNumber> {3});
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A loaded town writes its town centre's spell icons as it holds them: a centre made functional since its load gave it
// icons, which it writes, so a worship site made after the next load takes them. Before, the town's record kept its
// loaded list (none), and the player's Land 2 site, made after a save and a load, got no miracle
TEST_P(TownWriteWorld, ALoadedTownIsWrittenWithTheSpellIconsItGotSinceItsLoad)
{
	auto world = SyntheticWorld();
	world.loadOrder = {2, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	const auto icon = Registry().Create();
	Registry().Get<ecs::components::TownMagic>(entity).spellIcons.insert(
	    Registry().Get<ecs::components::TownMagic>(entity).spellIcons.begin(), icon);
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).spellIcons.numbers,
	          std::vector<ObjectNumber> {context.Reference(icon, gamesave::TownCentreSpellIconRecord::k_Type)});
	EXPECT_NE(std::get<TownRecord>(*written).spellIcons.numbers.front(), 0u);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// The repair of saves written before a loaded town wrote its spell icons as it held them: a town whose list is empty
// gets its centre's icons, the last slot first as each was put at the head of the list; a town with a list, or with a
// centre naming none, is left as it is
TEST(TownSpellIconsRepair, AnEmptyTownListTakesItsCentresIconsTheLastSlotFirst)
{
	gamesave::WorldRead world;
	world.ResizeObjects(4);
	TownRecord town;
	town.townCentre.number = 2;
	gamesave::PutRecord(world.objects[0], town);
	gamesave::TownCentreRecord centre;
	centre.icons.numbers = {5, 0, 7, 0, 0, 0};
	gamesave::PutRecord(world.objects[1], centre);
	TownRecord listed = town;
	listed.spellIcons.numbers = {9};
	gamesave::PutRecord(world.objects[2], listed);
	EXPECT_EQ(load::RepairTownSpellIcons(world), std::vector<ObjectNumber> {1});
	EXPECT_EQ(std::get<TownRecord>(*world.objects[0]).spellIcons.numbers, (std::vector<ObjectNumber> {7, 5}));
	EXPECT_EQ(std::get<TownRecord>(*world.objects[2]).spellIcons.numbers, std::vector<ObjectNumber> {9});
	EXPECT_TRUE(load::RepairTownSpellIcons(world).empty());

	// a town naming no centre takes the centre whose record names it, and the worship site whose towns name it
	gamesave::WorldRead later;
	later.ResizeObjects(3);
	gamesave::PutRecord(later.objects[0], TownRecord {});
	gamesave::TownCentreRecord named;
	named.abode.town.number = 1;
	named.icons.numbers = {4, 0, 0, 0, 0, 0};
	gamesave::PutRecord(later.objects[1], named);
	gamesave::WorshipSiteRecord site;
	site.towns.numbers = {1};
	gamesave::PutRecord(later.objects[2], site);
	EXPECT_EQ(load::RepairTownSpellIcons(later), std::vector<ObjectNumber> {1});
	const auto& repaired = std::get<TownRecord>(*later.objects[0]);
	EXPECT_EQ(repaired.spellIcons.numbers, std::vector<ObjectNumber> {4});
	EXPECT_EQ(repaired.townCentre.number, 2u);
	EXPECT_EQ(repaired.worshipSite.number, 3u);
}

// A town with a missionary preaching to it is written with its list of missionary controls, and each control as a
// record of its own class: its place, its missionary and the reaction it spreads, as the original saves them. Before,
// no writer claimed a control, so a town with a missionary stopped the whole save
TEST_P(TownWriteWorld, ATownWithAMissionaryIsWrittenWithItsMissionaryControls)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto town = loaded.at(2).entity;
	auto& registry = Registry();
	const auto missionary = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::VillagerRecord>>(missionary);
	loaded.try_emplace(30, LoadedObject {.entity = missionary, .type = gamesave::VillagerRecord::k_Type});
	const auto control = registry.Create();
	registry.Assign<ecs::components::Transform>(control, glm::vec3(200.0f, 0.0f, 210.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::MissionaryControl>(control, ecs::components::MissionaryControl {.missionary = missionary});
	registry.Get<ecs::components::Town>(town).missionaries = {control};
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, town);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<TownRecord>(*written);
	ASSERT_EQ(record.missionaries.numbers.size(), 1u);
	EXPECT_NE(record.missionaries.numbers.front(), 0u);
	const auto* writer = _writers.WriterOf(gamesave::MissionaryControlRecord::k_Type);
	ASSERT_NE(writer, nullptr);
	const auto controlWritten = (*writer)(context, control);
	ASSERT_TRUE(controlWritten.has_value()) << context.Failure();
	const auto& saved = std::get<gamesave::MissionaryControlRecord>(*controlWritten);
	EXPECT_EQ(saved.missionary.number, 30u);
	EXPECT_EQ(saved.reaction.number, 0u);
}

// A loaded town takes back its missionary controls in its list's order, each with its missionary, as the original's
// town load reads its list
TEST_P(TownWriteWorld, ALoadedTownTakesBackItsMissionaryControls)
{
	auto world = SyntheticWorld();
	gamesave::MissionaryControlRecord control;
	control.position.position = {.x = 200 << 16, .z = 210 << 16, .altitude = 0.0f};
	world.objects.emplace_back(control);
	std::get<TownRecord>(*world.objects.at(1)).missionaries.numbers = {3};
	auto makers = TownMakers();
	makers.missionaryControl = load::MakeMissionaryControlEntity;
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), makers, &loaded);
	const auto& missionaries = Registry().Get<ecs::components::Town>(loaded.at(2).entity).missionaries;
	ASSERT_EQ(missionaries.size(), 1u);
	ASSERT_TRUE(Registry().AllOf<ecs::components::MissionaryControl>(missionaries.front()));
	EXPECT_TRUE(Registry().Get<ecs::components::MissionaryControl>(missionaries.front()).missionary == entt::null);
}

// A loaded town takes back whether a script forbids it a worship site, as the original's town load reads it, and writes
// it as it stands: a script that allows it since writes it allowed
TEST_P(TownWriteWorld, ALoadedTownTakesBackWhetherItMayBuildAWorshipSite)
{
	auto world = SyntheticWorld();
	std::get<TownRecord>(*world.objects.at(1)).cannotBuildWorshipSite = 1;
	world.loadOrder = {2, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& magic = Registry().Get<ecs::components::TownMagic>(entity);
	EXPECT_TRUE(magic.forbidWorshipSite);
	magic.forbidWorshipSite = false;
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).cannotBuildWorshipSite, 0u);
}

// A loaded town takes back its belief as its block holds it, after the block's two words: each player's belief, the
// recent belief, the turns, the cap, what was added this period, the accumulator, the pending belief, the boredom and
// the desires' thresholds, and the scale of its pending belief; it is written back as it stands. Without it the town
// believed in no player and went to the neutral player at its first fold, leaving its worship site and its icons
TEST_P(TownWriteWorld, ALoadedTownTakesBackItsBeliefAndWritesItAsItStands)
{
	auto world = SyntheticWorld();
	auto& saved = std::get<TownRecord>(*world.objects.at(1));
	std::array<float, 8> belief {};
	belief.at(1) = 0.875f;
	belief.at(7) = 0.5f;
	std::array<float, 8> cap {};
	cap.fill(10.0f);
	std::array<float, 41> boredom {};
	boredom.fill(1.0f);
	boredom.at(3) = 0.25f;
	std::array<float, 17> threshold {};
	threshold.at(16) = 0.75f;
	const uint32_t lastAdded = 1234;
	std::memcpy(saved.belief.data() + 8, belief.data(), sizeof(belief));
	std::memcpy(saved.belief.data() + 8 + 2 * 32, &lastAdded, sizeof(lastAdded));
	std::memcpy(saved.belief.data() + 8 + 3 * 32, cap.data(), sizeof(cap));
	std::memcpy(saved.belief.data() + 8 + 7 * 32, boredom.data(), sizeof(boredom));
	std::memcpy(saved.belief.data() + 8 + 7 * 32 + sizeof(boredom), threshold.data(), sizeof(threshold));
	saved.belief.at(4) = 0x12; // the second word, kept as read
	saved.balanceBeliefScale = 0.5f;
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& town = Registry().Get<ecs::components::Town>(entity);
	EXPECT_EQ(town.belief.belief, belief);
	EXPECT_EQ(town.belief.lastAddedTurn.at(0), lastAdded);
	EXPECT_EQ(town.belief.cap, cap);
	EXPECT_EQ(town.belief.boredom, boredom);
	EXPECT_EQ(town.belief.desireThreshold, threshold);
	EXPECT_EQ(town.belief.beliefScale, 0.5f);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	{
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_TRUE(std::get<TownRecord>(*written) == saved);
	}
	town.belief.belief.at(1) = 0.25f;
	town.belief.beliefScale = 2.0f;
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<TownRecord>(*written);
	float first = 0.0f;
	std::memcpy(&first, record.belief.data() + 8 + sizeof(float), sizeof(first));
	EXPECT_EQ(first, 0.25f);
	EXPECT_EQ(record.belief.at(4), 0x12);
	EXPECT_EQ(record.balanceBeliefScale, 2.0f);
}

// A loaded town is written with its two temporary pots as they stand: one deleted since the load is not named, as the
// original writes the slots it holds
TEST_P(TownWriteWorld, ALoadedTownWritesItsTemporaryPotsAsTheyStand)
{
	auto world = SyntheticWorld();
	std::get<TownRecord>(*world.objects.at(1)).temporaryStores.numbers = {0, 0};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& town = Registry().Get<ecs::components::Town>(entity);
	const auto food = Keeping(gamesave::FoodPileRecord {});
	const auto wood = Keeping(gamesave::WoodPileRecord {});
	town.temporaryPots = {food, wood};
	Registry().Destroy(wood);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).temporaryStores.numbers, (std::vector<ObjectNumber> {context.Reference(food), 0}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A loaded town takes back its figures as its block holds them, after the block's two words: its adults and children,
// its deaths, the food it used, its injured and the abodes by their number; it writes them as it holds them, with the
// counts openblack does not keep (the planned buildings) as loaded
TEST_P(TownWriteWorld, ALoadedTownTakesBackItsFiguresAndWritesThemAsItHoldsThem)
{
	auto world = SyntheticWorld();
	auto& saved = std::get<TownRecord>(*world.objects.at(1));
	const auto put = [&saved](size_t at, const auto& value) { std::memcpy(saved.stats.data() + at, &value, sizeof(value)); };
	put(8, uint32_t {12});   // the adults
	put(12, uint32_t {5});   // the children
	put(20, uint32_t {3});   // the planned buildings, not kept
	put(56, uint32_t {7});   // the deaths' total
	put(220, uint32_t {9});  // the deaths' count
	put(232, 1.5f);          // the food used
	put(260, int32_t {2});   // the injured
	saved.stats.at(264) = 4; // the abodes of the first number
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& town = Registry().Get<ecs::components::Town>(entity);
	EXPECT_EQ(town.stats.adults, 12u);
	EXPECT_EQ(town.stats.children, 5u);
	EXPECT_EQ(town.stats.abodesByNumber.at(0), 4);
	EXPECT_EQ(town.foodUsed, 1.5f);
	EXPECT_EQ(town.injured, 2);
	const auto& died = Registry().Get<const ecs::components::TownDeaths>(entity);
	EXPECT_EQ(died.total38, 7u);
	EXPECT_EQ(died.count, 9u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	{
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_TRUE(std::get<TownRecord>(*written) == saved);
	}
	town.stats.adults = 10;
	town.injured = 0;
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& stats = std::get<TownRecord>(*written).stats;
	uint32_t adults = 0;
	uint32_t planned = 0;
	int32_t injured = -1;
	std::memcpy(&adults, stats.data() + 8, sizeof(adults));
	std::memcpy(&planned, stats.data() + 20, sizeof(planned));
	std::memcpy(&injured, stats.data() + 260, sizeof(injured));
	EXPECT_EQ(adults, 10u);
	EXPECT_EQ(planned, 3u);
	EXPECT_EQ(injured, 0);
}

// A loaded town takes back the magic types it holds, as saved, and writes them as they stand
TEST_P(TownWriteWorld, ALoadedTownTakesBackTheMagicItHolds)
{
	auto world = SyntheticWorld();
	auto& saved = std::get<TownRecord>(*world.objects.at(1));
	saved.magicTypesHeld.at(3) = 1;
	saved.magicTypesHeld.at(17) = 1;
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& magic = Registry().Get<ecs::components::TownMagic>(entity);
	for (size_t type = 0; type < magic.held.size(); ++type)
	{
		EXPECT_EQ(magic.held.at(type), type == 3 || type == 17) << type;
	}

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	{
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_TRUE(std::get<TownRecord>(*written) == saved);
	}
	magic.held.at(3) = false;
	magic.held.at(5) = true;
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& held = std::get<TownRecord>(*written).magicTypesHeld;
	EXPECT_EQ(held.at(3), 0);
	EXPECT_EQ(held.at(5), 1);
	EXPECT_EQ(held.at(17), 1);
}

// A loaded town takes back the countdown to its being empty, as saved, and writes it as it stands
TEST_P(TownWriteWorld, ALoadedTownTakesBackItsEmptyCountdown)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	const auto entity = loaded.at(2).entity;
	auto& town = Registry().Get<ecs::components::Town>(entity);
	EXPECT_EQ(town.emptyCountdown, 12u);
	town.emptyCountdown = 7;
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).emptyTownTimer, 7u);
}

// A loaded town gets back the nearest town its record names, as saved and not worked out again (the first names the
// second though the third is nearer), none when its record names none; each is written back as it was read,
// and a town whose nearest town has changed since its load writes the one it has now
TEST_P(TownWriteWorld, ALoadedTownGetsItsNearestTownBackAsSavedAndWritesItAsItStands)
{
	constexpr ObjectNumber k_First = 2;
	constexpr ObjectNumber k_Second = 3;
	constexpr ObjectNumber k_Third = 4;
	auto world = SyntheticWorld();
	auto second = std::get<TownRecord>(*world.objects.at(k_First - 1));
	second.id = 5;
	second.container.position.position = {.x = 400 << 16, .z = 200 << 16, .altitude = 0.0f};
	auto third = second;
	third.id = 6;
	third.container.position.position = {.x = 210 << 16, .z = 200 << 16, .altitude = 0.0f};
	std::get<TownRecord>(*world.objects.at(k_First - 1)).nearestTown.number = k_Second;
	second.nearestTown.number = k_First;
	third.nearestTown.number = 0;
	world.objects.emplace_back(second);
	world.objects.emplace_back(third);
	world.loadOrder = {k_First, k_Second, k_Third, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);
	ASSERT_TRUE(loaded.contains(k_First) && loaded.contains(k_Second) && loaded.contains(k_Third));
	const auto first = loaded.at(k_First).entity;
	EXPECT_EQ(Registry().Get<const ecs::components::Town>(first).nearestTown, loaded.at(k_Second).entity);
	EXPECT_EQ(Registry().Get<const ecs::components::Town>(loaded.at(k_Second).entity).nearestTown, first);
	EXPECT_EQ(Registry().Get<const ecs::components::Town>(loaded.at(k_Third).entity).nearestTown, entt::entity {entt::null});

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	EXPECT_EQ(ExpectEachTownAsRead(world, loaded, _writers, source), 3u);

	Registry().Get<ecs::components::Town>(first).nearestTown = loaded.at(k_Third).entity;
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, first);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*written).nearestTown.number, context.Reference(loaded.at(k_Third).entity));
}

// Two towns made in play each name the other as its nearest town
TEST_P(TownWriteWorld, TownsMadeInPlayAreWrittenWithTheirNearestTown)
{
	const auto first = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(5, glm::vec3(1500.0f, k_Ground, 1500.0f),
	                                                                            PlayerNames::NEUTRAL, Tribe::NORSE);
	const auto second = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(6, glm::vec3(1700.0f, k_Ground, 1500.0f),
	                                                                             PlayerNames::NEUTRAL, Tribe::NORSE);
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto firstWritten = (*_writers.WriterOf(TownRecord::k_Type))(context, first);
	const auto secondWritten = (*_writers.WriterOf(TownRecord::k_Type))(context, second);
	ASSERT_TRUE(firstWritten.has_value() && secondWritten.has_value()) << context.Failure();
	EXPECT_EQ(std::get<TownRecord>(*firstWritten).nearestTown.number, context.Reference(second));
	EXPECT_EQ(std::get<TownRecord>(*secondWritten).nearestTown.number, context.Reference(first));
}

// A town made in play is written from its components in the original's layout: the town's row and its owner, its
// script number, tribe and player, its desires with seventeen values in each, the figures block, the belief block
// and each player's dealings, each starting with the original's word, the trust and the scales a new town starts
// with, and its last values
TEST_P(TownWriteWorld, ATownMadeInPlayIsWrittenFromItsComponents)
{
	const auto entity = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(5, glm::vec3(1500.0f, k_Ground, 1500.0f),
	                                                                             PlayerNames::NEUTRAL, Tribe::NORSE);
	auto& town = Registry().Get<ecs::components::Town>(entity);
	town.belief.belief.at(0) = 2.5f;
	town.belief.beliefInNeutralPlayer = 0.5f;
	town.desire.desire.at(3) = 0.125f;
	town.desire.sorted.at(1) = {.boosts = 1.0f, .value = 0.5f, .index = 6};
	town.stats.adults = 9;
	town.stats.males = 4;
	town.stats.foodForDinner = 18.0f;
	town.injured = 2;
	town.aggression.aggression.at(1) = 3.0f;
	town.aggression.lastTurns.at(1) = 77;
	town.aggression.lastEffects.at(1) = {.burn = 0.5f, .crush = 0.25f, .hit = 0.125f};
	town.aggression.lastTurn = 77;
	town.resourceLastRemovedTurn.at(2).at(1) = 41;
	town.emptyCountdown = 50;
	auto& magic = Registry().Get<ecs::components::TownMagic>(entity);
	magic.worshipPercentage = 0.4f;
	magic.held.at(3) = true;

	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(source.registry, entity), std::vector {TownRecord::k_Type});
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<TownRecord>(*written);
	EXPECT_EQ(record.container.info, k_TownRow);
	EXPECT_EQ(record.id, 5u);
	EXPECT_EQ(record.tribe, static_cast<uint32_t>(Tribe::NORSE));
	EXPECT_EQ(record.playerNumber, 7u);
	EXPECT_EQ(record.worshipPercentage, 0.4f);
	EXPECT_EQ(record.beliefInNeutralPlayer, 0.5f);
	for (const auto* values : {&record.desires.base.values, &record.desires.modifier.values, &record.desires.cheat.values,
	                           &record.desires.boost.values, &record.desires.desire.values, &record.desires.raw.values,
	                           &record.desires.change.values, &record.desires.villagerStateCount.values})
	{
		EXPECT_EQ(values->size(), 17u);
	}
	EXPECT_EQ(record.desires.desire.values.at(3), 0.125f);
	EXPECT_EQ(record.desires.sorts.values.at(1).first, 0x3F800000u);
	EXPECT_EQ(record.desires.sorts.values.at(1).desire, 6u);
	EXPECT_EQ(record.desires.town.number, context.Reference(entity));
	EXPECT_EQ(record.temporaryStores.numbers.size(), 2u);

	const std::span<const uint8_t> figures(record.stats);
	EXPECT_EQ(ValueAt<uint32_t>(figures, 0x0), k_FiguresBlockWord);
	EXPECT_EQ(ValueAt<uint32_t>(figures, 0x4), 0u);
	EXPECT_EQ(ValueAt<uint32_t>(figures, 0x8), 9u);  // the adults
	EXPECT_EQ(ValueAt<uint32_t>(figures, 0x54), 4u); // the males
	EXPECT_EQ(ValueAt<float>(figures, 0xE4), 18.0f); // the food for dinner
	EXPECT_EQ(ValueAt<int32_t>(figures, 0x104), 2);  // the wounded

	const std::span<const uint8_t> belief(record.belief);
	EXPECT_EQ(ValueAt<uint32_t>(belief, 0x0), k_BeliefBlockWord);
	EXPECT_EQ(ValueAt<uint32_t>(belief, 0x4), 0u);
	EXPECT_EQ(ValueAt<float>(belief, 0x8), 2.5f);
	EXPECT_EQ(ValueAt<float>(belief, 0x8 + 3 * 32), 10.0f); // the first cap
	EXPECT_EQ(ValueAt<float>(belief, 0x8 + 7 * 32), 1.0f);  // the first boredom

	const std::span<const uint8_t> interact(record.playerInteract);
	for (size_t player = 0; player < 8; ++player)
	{
		const auto at = player * k_PlayerInteractBytes;
		EXPECT_EQ(ValueAt<float>(interact, at + k_TrustAt), 1.0f) << player;
		EXPECT_EQ(ValueAt<uint32_t>(interact, at + k_AttackAt), k_AttackBlockWord) << player;
	}
	EXPECT_EQ(ValueAt<float>(interact, k_PlayerInteractBytes + 0x4), 3.0f);
	EXPECT_EQ(ValueAt<uint32_t>(interact, k_PlayerInteractBytes + 0x8), 77u);
	EXPECT_EQ(ValueAt<float>(interact, k_PlayerInteractBytes + k_AttackAt + 0x8), 0.5f);
	EXPECT_EQ(ValueAt<float>(interact, k_PlayerInteractBytes + k_AttackAt + 0x10), 0.125f);

	EXPECT_EQ(record.magicTypesHeld.at(3), 1);
	EXPECT_EQ(record.lastAttackedTurn, 77u);
	EXPECT_EQ(record.otherPlayersAggressorScale, 1.0f);
	EXPECT_EQ(record.ownerAggressorScale, 1.0f);
	EXPECT_EQ(record.resourceLastRemovedTurn.rows, 8u);
	EXPECT_EQ(record.resourceLastRemovedTurn.columns, 2u);
	EXPECT_EQ(record.resourceLastRemovedTurn.values.at(2 * 2 + 1), 41u);
	EXPECT_EQ(record.desireFlags.numbers.size(), 17u);
	EXPECT_EQ(record.balanceBeliefScale, 1.0f);
	EXPECT_EQ(record.emptyTownTimer, 50u);
}

// A town made in play names its plans in its list's order, each a planned building of its class written from the
// plan: where it is to stand in whole map units with no height, its angle and scale, whether it rebuilds, the turn it
// was planned, its row and its town
TEST_P(TownWriteWorld, ATownMadeInPlayNamesItsPlansWrittenFromThem)
{
	const auto entity = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(6, glm::vec3(1500.0f, k_Ground, 1500.0f),
	                                                                             PlayerNames::NEUTRAL, Tribe::NORSE);
	auto& town = Registry().Get<ecs::components::Town>(entity);
	town.plannedAbodes.push_back({.info = AbodeInfo::CelticHut,
	                              .position = glm::vec3(1510.0f, k_Ground, 1490.0f),
	                              .yAngleRadians = 0.5f,
	                              .scale = 1.25f,
	                              .townCentre = false,
	                              .creationTurn = 12});
	town.plannedAbodes.push_back({.info = AbodeInfo::CelticTownCentre,
	                              .position = glm::vec3(1500.0f, k_Ground, 1500.0f),
	                              .yAngleRadians = 0.0f,
	                              .scale = 1.0f,
	                              .townCentre = true});
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& planned = std::get<TownRecord>(*written).planned.numbers;
	ASSERT_EQ(planned.size(), 2u);
	const auto abodeKey = SourceKey {.type = gamesave::PlannedAbodeRecord::k_Type, .id = PartId(entity, 0)};
	const auto centreKey = SourceKey {.type = gamesave::PlannedTownCentreRecord::k_Type, .id = PartId(entity, 1)};
	EXPECT_EQ(planned.at(0), context.Reference(abodeKey));
	EXPECT_EQ(planned.at(1), context.Reference(centreKey));

	const auto plan = (*_writers.SourceWriterOf(gamesave::PlannedAbodeRecord::k_Type))(context, abodeKey.id);
	ASSERT_TRUE(plan.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::PlannedAbodeRecord>(*plan);
	EXPECT_EQ(record.planned.position.position.x, map_coords::ToFixed(1510.0f));
	EXPECT_EQ(record.planned.position.position.z, map_coords::ToFixed(1490.0f));
	EXPECT_EQ(record.planned.position.position.altitude, 0.0f);
	EXPECT_EQ(record.planned.yAngle, 0.5f);
	EXPECT_EQ(record.planned.scale, 1.25f);
	EXPECT_EQ(record.planned.wasConstructed, 0u);
	EXPECT_EQ(record.planned.creationTurn, 12);
	EXPECT_EQ(record.planned.info, load::AbodeInfoRowOf(AbodeInfo::CelticHut));
	EXPECT_EQ(record.town.number, context.Reference(entity, TownRecord::k_Type));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A town's planned citadel heart is written of the citadel heart's row, at its place with no height, its angle and
// scale and its town; one of a row the tables do not hold is refused
TEST_P(TownWriteWorld, APlannedCitadelHeartIsWrittenOfTheHeartsRow)
{
	const auto entity = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(6, glm::vec3(1500.0f, k_Ground, 1500.0f),
	                                                                             PlayerNames::NEUTRAL, Tribe::NORSE);
	auto& town = Registry().Get<ecs::components::Town>(entity);
	ecs::components::PlannedAbode heart {.info = AbodeInfo::None,
	                                     .position = glm::vec3(1520.0f, k_Ground, 1480.0f),
	                                     .yAngleRadians = 0.25f,
	                                     .scale = 1.0f,
	                                     .townCentre = false};
	heart.citadelHeart = true;
	town.plannedAbodes.push_back(heart);
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto key = PartId(entity, 0);
	const auto plan = (*_writers.SourceWriterOf(gamesave::PlannedCitadelHeartRecord::k_Type))(context, key);
	ASSERT_TRUE(plan.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::PlannedCitadelHeartRecord>(*plan);
	EXPECT_EQ(record.planned.info, 2411u);
	EXPECT_EQ(record.planned.position.position.x, map_coords::ToFixed(1520.0f));
	EXPECT_EQ(record.planned.position.position.altitude, 0.0f);
	EXPECT_EQ(record.planned.yAngle, 0.25f);
	EXPECT_EQ(record.town.number, context.Reference(entity, TownRecord::k_Type));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	town.plannedAbodes.at(0).heartInfo = 1;
	WriteContext refused(_writers, source);
	EXPECT_FALSE((*_writers.SourceWriterOf(gamesave::PlannedCitadelHeartRecord::k_Type))(refused, key).has_value());
	EXPECT_FALSE(refused.Failure().empty());
}

// Integration test: needs the game's data; skipped without it. The planned citadel heart of the frozen _f_f profile's
// Land 1 save is of the citadel heart's row, with no height, and names a town
TEST_P(TownWriteWorld, TheFrozenLandOnePlannedCitadelHeartIsOfTheHeartsRow)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!GameData() || game == nullptr)
	{
		GTEST_SKIP() << "the frozen profile's save is the game's data";
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / "_f_f" / "Saved Games" / "20" / "20.sav", std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no save in the _f_f profile";
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
	const auto world = gamesave::ReadWorld(bytes);
	size_t hearts = 0;
	for (const auto& object : world.objects)
	{
		const auto* plan = object.has_value() ? std::get_if<gamesave::PlannedCitadelHeartRecord>(&*object) : nullptr;
		if (plan == nullptr)
		{
			continue;
		}
		++hearts;
		EXPECT_EQ(plan->planned.info, 2411u);
		EXPECT_EQ(plan->planned.position.position.altitude, 0.0f);
		const auto town = plan->town.number;
		ASSERT_TRUE(town != 0 && town <= world.objects.size() && world.objects.at(town - 1).has_value());
		EXPECT_TRUE(std::holds_alternative<TownRecord>(*world.objects.at(town - 1)));
	}
	EXPECT_EQ(hearts, 1u);
	// its town takes it back as its plan: the load's census counts it made
	const auto census = load::Apply(load::PlanOf(world), TownMakers());
	const auto line = std::ranges::find(census, gamesave::PlannedCitadelHeartRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->saved, 1u);
	EXPECT_EQ(line->made, 1u);
}

// A town made in play names the artefacts that joined it, the newest first, each written from its object: the object,
// its town, its worth, its dance, its glow (its spot visual's container, none once it has gone), whether its dance
// started and the turn it did
TEST_P(TownWriteWorld, ATownMadeInPlayNamesItsArtefactsWrittenFromThem)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	auto& state = particles.GetState();
	RegisterTownArtifactWriters(_writers);
	_writers.Register(
	    gamesave::RockRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) {
		    return registry.AllOf<ecs::components::TownArtefact>(entity);
	    },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<SavedRecord> { return std::nullopt; });
	const auto entity = ecs::archetypes::TownArchetype::CreateWithoutDesireFlags(7, glm::vec3(1500.0f, k_Ground, 1500.0f),
	                                                                             PlayerNames::NEUTRAL, Tribe::NORSE);
	const auto rock = Registry().Create();
	auto& artefact = Registry().Assign<ecs::components::TownArtefact>(rock);
	artefact.town = entity;
	artefact.inTown = true;
	artefact.joined = 3;
	artefact.value = 1.5f;
	artefact.dancing = true;
	artefact.danceStarted = 40;
	// its glow: a spot visual, whose object no class claims
	constexpr uint32_t k_Glow = 7;
	artefact.effect = Registry().Create();
	state.effects[k_Glow].effect = std::make_unique<particles::Effect>(std::make_shared<particles::File>(), glm::vec3(0.0f),
	                                                                   1.0f, game_random::particles::NetGameType::Synced);
	state.containers.push_front({.effect = k_Glow, .object = artefact.effect});
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TownRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto number = context.Reference(rock, gamesave::TownArtifactRecord::k_Type);
	EXPECT_EQ(std::get<TownRecord>(*written).artifacts.numbers, std::vector {number});
	const auto record = (*_writers.WriterOf(gamesave::TownArtifactRecord::k_Type))(context, rock);
	ASSERT_TRUE(record.has_value()) << context.Failure();
	const auto& saved = std::get<gamesave::TownArtifactRecord>(*record);
	EXPECT_EQ(saved.objects.at(0).number, context.Reference(rock));
	EXPECT_EQ(saved.objects.at(1).number, context.Reference(entity, TownRecord::k_Type));
	EXPECT_EQ(saved.word, std::bit_cast<uint32_t>(1.5f));
	EXPECT_EQ(saved.words.at(0), 1u);
	EXPECT_EQ(saved.words.at(1), 40u);
	EXPECT_EQ(saved.others.at(1).number,
	          context.Reference(SourceKey {.type = gamesave::ParticleContainerRecord::k_Type, .id = k_Glow}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	// the glow gone: none
	state.effects.erase(k_Glow);
	const auto gone = (*_writers.WriterOf(gamesave::TownArtifactRecord::k_Type))(context, rock);
	ASSERT_TRUE(gone.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::TownArtifactRecord>(*gone).others.at(1).number, 0u);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// Integration test: needs the original's own save of a new Land 1 game; skipped without it. Its planned buildings hold
// what the plans' writer gives: no place flags, no height, a 0 word after whether it rebuilds, and a town
TEST_P(TownWriteWorld, TheOriginalsNewGamePlansHoldWhatThePlansWriterGives)
{
	const auto bytes = OriginalNewLandOneSave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the original's save of a new Land 1 game is local reference data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	size_t plans = 0;
	for (const auto& object : world.objects)
	{
		const auto* plan = object.has_value() ? std::get_if<gamesave::PlannedAbodeRecord>(&*object) : nullptr;
		const auto* centre = object.has_value() ? std::get_if<gamesave::PlannedTownCentreRecord>(&*object) : nullptr;
		const auto* fields = plan != nullptr ? &plan->planned : (centre != nullptr ? &centre->planned : nullptr);
		if (fields == nullptr)
		{
			continue;
		}
		++plans;
		EXPECT_EQ(fields->position.flags, 0u) << plans;
		EXPECT_EQ(fields->position.position.altitude, 0.0f) << plans;
		EXPECT_EQ(fields->word, 0u) << plans;
		EXPECT_EQ(fields->wasConstructed, 0u) << plans;
		EXPECT_NE((plan != nullptr ? plan->town : centre->town).number, 0u) << plans;
	}
	EXPECT_EQ(plans, 6u);
}

// Integration test: needs the original game's data and the original's own save of a new Land 1 game (local reference
// data beside the game's folder); skipped without them. Every town of a new game holds the values the writer gives a
// town made in play where nothing in play has moved them yet: the town's row, the three blocks' words and the cleared
// words after them, the full trust in every player, both aggressor scales and the belief scale at 1, and no villager
// in the town's own list of villagers nor among its extra villagers
TEST_P(TownWriteWorld, TheOriginalsNewGameTownsHoldWhatANewTownStartsWith)
{
	const auto bytes = OriginalNewLandOneSave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the original's save of a new Land 1 game is local reference data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	size_t towns = 0;
	for (const auto& object : world.objects)
	{
		const auto* town = object.has_value() ? std::get_if<TownRecord>(&*object) : nullptr;
		if (town == nullptr)
		{
			continue;
		}
		++towns;
		EXPECT_EQ(town->container.info, k_TownRow) << town->id;
		const std::span<const uint8_t> figures(town->stats);
		EXPECT_EQ(ValueAt<uint32_t>(figures, 0x0), k_FiguresBlockWord) << town->id;
		EXPECT_EQ(ValueAt<uint32_t>(figures, 0x4), 0u) << town->id;
		const std::span<const uint8_t> belief(town->belief);
		EXPECT_EQ(ValueAt<uint32_t>(belief, 0x0), k_BeliefBlockWord) << town->id;
		EXPECT_EQ(ValueAt<uint32_t>(belief, 0x4), 0u) << town->id;
		const std::span<const uint8_t> interact(town->playerInteract);
		for (size_t player = 0; player < 8; ++player)
		{
			const auto at = player * k_PlayerInteractBytes;
			EXPECT_EQ(ValueAt<float>(interact, at + k_TrustAt), 1.0f) << town->id << " " << player;
			EXPECT_EQ(ValueAt<uint32_t>(interact, at + k_AttackAt), k_AttackBlockWord) << town->id << " " << player;
			EXPECT_EQ(ValueAt<uint32_t>(interact, at + k_AttackAt + 4), 0u) << town->id << " " << player;
		}
		EXPECT_EQ(town->otherPlayersAggressorScale, 1.0f) << town->id;
		EXPECT_EQ(town->ownerAggressorScale, 1.0f) << town->id;
		EXPECT_EQ(town->balanceBeliefScale, 1.0f) << town->id;
		EXPECT_TRUE(town->villagers.numbers.empty()) << town->id;
		EXPECT_TRUE(town->extraVillagers.numbers.empty()) << town->id;
		EXPECT_EQ(town->temporaryStores.numbers.size(), 2u) << town->id;
		EXPECT_EQ(town->desireFlags.numbers.size(), 17u) << town->id;
		EXPECT_EQ(town->desires.base.values, std::vector<float>(17, 0.0f)) << town->id;
		EXPECT_EQ(town->desires.modifier.values, std::vector<float>(17, 0.0f)) << town->id;
		EXPECT_EQ(town->desires.change.values, std::vector<float>(17, 0.0f)) << town->id;
		EXPECT_EQ(town->desires.lastProcessTurn, 0u) << town->id;
		EXPECT_EQ(town->promiscuity, 0.0f) << town->id;
	}
	EXPECT_EQ(towns, 6u);
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 1 autosave with its towns made as the game's load makes them, and every other object keeping its
// record: each town is written as read, and the whole file, the towns by this writer, is the original's byte for byte
TEST_P(TownWriteWorld, TheFrozenLandOneAutosavesTownsAreWrittenAsReadAndTheFileAsItWas)
{
	const auto bytes = FrozenLandOneAutosave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), TownMakers(), &loaded);

	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	const auto each = ExpectEachTownAsRead(world, loaded, _writers, source);
	EXPECT_GT(each, 0u);

	const auto written = WriteWorld(_writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, *bytes);
	size_t byWriter = 0;
	for (const auto& record : written.records)
	{
		if (record.type == TownRecord::k_Type)
		{
			EXPECT_EQ(record.source, RecordSource::Writer) << record.number;
			++byWriter;
		}
	}
	EXPECT_EQ(byWriter, each);
}

INSTANTIATE_TEST_SUITE_P(Data, TownWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
