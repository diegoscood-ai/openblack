/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The abodes, the creches, the graveyards and the storage pits written from the entities the load made of them: each
// record as the load read it, an abode made in play from its state, and the frozen Land 1 autosave's records of those
// classes as read and its whole world file as it was, the storage pits' piles as their loads kept them (the pots'
// writer's own tests write them) (docs/wiki/engine/saved-games.md#writing-the-world-file)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include <BuildingRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Footpaths.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/ShowNeeds.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/Wonders.h"
#include "ECS/WaterQueries.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourceManager.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::AbodeRecord;
using gamesave::CrecheRecord;
using gamesave::FieldRecord;
using gamesave::FishFarmRecord;
using gamesave::FoodPileRecord;
using gamesave::GraveyardRecord;
using gamesave::MagicFoodRecord;
using gamesave::ObjectNumber;
using gamesave::SavedRecord;
using gamesave::StoragePitRecord;
using gamesave::TotemStatueRecord;
using gamesave::TownCentreRecord;
using gamesave::TownCentreSpellIconRecord;
using gamesave::TownRecord;
using gamesave::WonderRecord;
using gamesave::WoodPileRecord;
using gamesave::WorkshopRecord;
namespace load = openblack::ecs::saved_world_load;

namespace
{
/// The classes these writers write
const std::set<uint32_t> k_Classes {AbodeRecord::k_Type, CrecheRecord::k_Type, GraveyardRecord::k_Type,
                                    StoragePitRecord::k_Type, FieldRecord::k_Type};

/// The abode rows and the pile rows the synthetic world's records are of
constexpr std::array k_AbodeRows {AbodeInfo::CelticHut, AbodeInfo::CelticCreche, AbodeInfo::CelticGraveyard,
                                  AbodeInfo::CelticStoragePit};
constexpr std::array k_PileRows {PotInfo::StoragePitFoodPile, PotInfo::WoodPile_1, PotInfo::WoodPile_2,
                                 PotInfo::WoodPile_3,         PotInfo::WoodPile_4, PotInfo::WoodPile_5};

/// A world file of a profile of the frozen game, or nothing without the game's data
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

/// The frozen Land 1 autosave, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenLandOneAutosave()
{
	return FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") / "Saved Games" /
	                  "20" / "20.sav");
}

class AbodeWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the abodes' writers alone: every other class, the game object too, is written from what its load kept
		RegisterAbodeWriters(_writers);
		if (!GameData())
		{
			// the rows the synthetic records are of: their kinds, a box each, and the piles' kinds and room
			const std::array kinds {AbodeType::LivingQuarters, AbodeType::Creche, AbodeType::Graveyard, AbodeType::StoragePit};
			for (size_t i = 0; i < k_AbodeRows.size(); ++i)
			{
				auto& row = Info().abode.at(static_cast<size_t>(k_AbodeRows.at(i)));
				row.abodeType = kinds.at(i);
				row.tribeType = Tribe::CELTIC;
				row.meshId = MeshId::BuildingCeltic1;
			}
			for (const auto type : k_PileRows)
			{
				auto& row = Info().pot.at(static_cast<size_t>(type));
				row.potType = type == PotInfo::StoragePitFoodPile ? PotType::PileFood : PotType::PileWood;
				row.maxAmountInPot = 1000;
				row.meshId = MeshId::ObjectRockChalk;
			}
		}
		if (!GameData())
		{
			auto& centre = Info().abode.at(static_cast<size_t>(AbodeInfo::NorseTownCentre));
			centre.abodeType = AbodeType::TownCentre;
			centre.tribeType = Tribe::NORSE;
			centre.meshId = MeshId::BuildingCeltic1;
			auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticField));
			row.abodeType = AbodeType::Field;
			row.tribeType = Tribe::CELTIC;
			row.meshId = MeshId::BuildingCeltic1;
		}
		for (const auto type : k_AbodeRows)
		{
			LoadAbodeMesh(type);
		}
		LoadAbodeMesh(AbodeInfo::CelticField);
		LoadAbodeMesh(AbodeInfo::NorseTownCentre);
		for (const auto type : k_PileRows)
		{
			LoadPileMesh(type);
		}
	}

	/// The model of an abode's row: the game's, or a box 8 m wide and 5 m tall
	void LoadAbodeMesh(AbodeInfo type) const
	{
		LoadMesh(Info().abode.at(static_cast<size_t>(type)).meshId, glm::vec3(-4.0f, 0.0f, -4.0f), glm::vec3(4.0f, 5.0f, 4.0f));
	}
	/// The model of a pile's row: the game's, or a box 2 m wide and 1 m tall
	void LoadPileMesh(PotInfo type) const
	{
		LoadMesh(Info().pot.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	}

	/// The particles the icons' seeds' holder effects start in, which the game's rows have and the synthetic ones do not
	static void UseParticles() { Locator::particleSystem::emplace<test::InertParticleSystem>(); }

	ClassWriters _writers;

private:
	// the particles a test asked for, reset after the registry
	test::RestoreService<Locator::particleSystem> _restoreParticles;
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

/// A load clears every object's draw-list mark and the next drawn frame lists the objects in view again; a test draws
/// no frame, so each loaded object the save had listed is listed as that frame would
void ListAsSaved(const gamesave::WorldRead& world, const LoadedObjects& loaded)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [number, object] : loaded)
	{
		const auto& read = world.objects.at(number - 1);
		if (!read.has_value() || !registry.Valid(object.entity))
		{
			continue;
		}
		std::visit(
		    [&registry, &object](const auto& saved) {
			    uint16_t flags = 0;
			    if constexpr (requires { saved.base.position.flags; })
			    {
				    flags = saved.base.position.flags;
			    }
			    else if constexpr (requires { saved.position.flags; })
			    {
				    flags = saved.position.flags;
			    }
			    if ((flags & position_flags::k_DrawListed) != 0)
			    {
				    registry.AssignOrReplace<ecs::components::DrawListed>(object.entity);
			    }
		    },
		    *read);
	}
}

/// The load's makers: the game's for the towns, the abodes, the creches, the graveyards, the storage pits and their
/// piles; every other class keeps its record on an entity of its own, as the game's load keeps what it makes
load::Makers AbodeMakers()
{
	return {
	    .footpath = [](const load::PlannedFootpath& footpath) { return Keeping(footpath.record); },
	    .footpathLink = [](const load::PlannedFootpathLink& link,
	                       const std::vector<entt::entity>& /*footpaths*/) { return Keeping(link.record); },
	    .town = load::MakeTownEntity,
	    .tree = [](const load::PlannedTree& tree, uint32_t /*forest*/) { return Keeping(tree.record); },
	    .bigForest = [](const load::PlannedBigForest& big, uint32_t /*forest*/) { return Keeping(big.record); },
	    .pile = load::MakePileEntity,
	    .storagePit = load::MakeStoragePitEntity,
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
	    // a town centre is not made here, so its record and its totem statue's are written as read: the original's totem
	    // is in the map's cells, which openblack's is not, so a made one's place's flags would not be written as read
	    .abode =
	        [](const load::PlannedAbode& abode, uint32_t town) {
		        if (std::holds_alternative<TownCentreRecord>(abode.record))
		        {
			        return entt::entity {entt::null};
		        }
		        return load::MakeAbodeEntity(abode, town);
	        },
	    .desireFlag = [](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return Keeping(flag.record); },
	    .stream = [](const load::PlannedStream& stream) { return Keeping(stream.record); },
	    .mist = [](const load::PlannedMist& mist) { return Keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	};
}

/// The record each loaded object of these classes is written as, against the record the load read; with `takeFlags`
/// (a synthetic record), the place's flags the read record is compared with are those of what the entity is now (its
/// map cells, its class: the shared part's, pinned by its own tests). How many of each class were written
std::map<uint32_t, size_t> ExpectEachRecordAsRead(const gamesave::WorldRead& world, const LoadedObjects& loaded,
                                                  const ClassWriters& writers, const WorldSource& source, bool takeFlags)
{
	std::map<uint32_t, size_t> written;
	WriteContext context(writers, source);
	for (const auto& [number, object] : loaded)
	{
		if (!k_Classes.contains(object.type))
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
		auto read = *world.objects.at(number - 1);
		if (takeFlags)
		{
			std::visit(
			    [&](auto& saved) {
				    if constexpr (requires { saved.base.position.flags; })
				    {
					    saved.base.position.flags = PositionFlagsOf(source.registry, object.entity, saved.base.position.flags);
					    EXPECT_NE(saved.base.position.flags & position_flags::k_InMap, 0) << number;
				    }
			    },
			    read);
		}
		EXPECT_TRUE(*record == read) << number;
		++written[object.type];
	}
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	return written;
}

/// A place on the synthetic land, `cells` cells in from its corner along x and z
gamesave::MapCoords At(int32_t cellsX, int32_t cellsZ)
{
	return {.x = cellsX << 16, .z = cellsZ << 16, .altitude = 0.0f};
}

/// The parts of a building of several cells a loaded one writes: its place, its row, creation number, size and angle,
/// built whole
gamesave::MultiMapFixedFields BuildingAt(const gamesave::MapCoords& at, AbodeInfo type, uint32_t creation, float heading)
{
	gamesave::MultiMapFixedFields base;
	base.position.position = at;
	base.object.info = load::AbodeInfoRowOf(type);
	base.object.coords = at;
	base.object.creationIndex = creation;
	base.object.heading = heading;
	base.multi.fixedFlags = static_cast<uint8_t>(ecs::components::Abode::k_Built);
	base.multi.percentBuilt = 1.0f;
	return base;
}

/// An abode's own values as a loaded one writes them, of the town of that number: values whose meanings are not read,
/// villagers and their counts the load does not make, its food and its wood
gamesave::AbodeFields AbodeOf(ObjectNumber town, uint32_t food, uint32_t wood)
{
	gamesave::AbodeFields abode;
	abode.abodeFlags = 1;
	abode.drinkingWater = {.x = 0x123456, .z = 0x654321, .altitude = 2.5f};
	abode.unusedWord = 7;
	abode.town.number = town;
	abode.villagers.numbers = {30, 31};
	abode.adults = 2;
	abode.presentAtHome = 1;
	abode.children = 0;
	abode.index = 4;
	abode.resources.values = {food, wood};
	return abode;
}

/// A pile of the storage pit of that number, as a loaded one writes it
template <typename Record>
Record PileAt(const gamesave::MapCoords& at, PotInfo type, uint32_t creation, ObjectNumber pit, ObjectNumber town,
              uint32_t amount)
{
	Record record;
	record.base.position.position = at;
	record.base.object.info = load::PotInfoRowOf(type);
	record.base.object.coords = at;
	record.base.object.creationIndex = creation;
	record.base.object.heading = 0.25f;
	record.base.mobile.mobileWord = 3;
	record.base.mobileObject.xAngle = 0.5f;
	record.pile.resourceType = type == PotInfo::StoragePitFoodPile ? 0 : 1;
	record.pile.parentTown.number = town;
	record.pile.resourceAmount = amount;
	record.pile.flags = 2;
	record.pile.building.number = pit;
	record.pile.buildingByte = 1;
	record.pile.altitudeZoomer.at(0) = 0x3F;
	return record;
}

/// A loaded game as its file reads: a town with a hut, a creche, a graveyard and a storage pit, the pit's food pile and
/// its five wood piles read inside it
gamesave::WorldRead SyntheticWorld()
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Pit = 6;
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.landNumber = 1;
	world.objects.emplace_back(game);
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.mainStoragePit.number = k_Pit;
	world.objects.emplace_back(town);
	world.objects.emplace_back(AbodeRecord {.thing = {.extra = 1, .flags = 2},
	                                        .base = BuildingAt(At(190, 190), AbodeInfo::CelticHut, 11, 0.5f),
	                                        .abode = AbodeOf(k_Town, 5, 9)});
	world.objects.emplace_back(CrecheRecord {.thing = {.flags = 2},
	                                         .base = BuildingAt(At(210, 190), AbodeInfo::CelticCreche, 12, 1.0f),
	                                         .abode = AbodeOf(k_Town, 0, 0)});
	world.objects.emplace_back(GraveyardRecord {.thing = {},
	                                            .base = BuildingAt(At(190, 210), AbodeInfo::CelticGraveyard, 13, 1.5f),
	                                            .abode = AbodeOf(k_Town, 0, 0),
	                                            .graves = 3});
	StoragePitRecord pit {.thing = {},
	                      .base = BuildingAt(At(210, 210), AbodeInfo::CelticStoragePit, 14, 2.0f),
	                      .abode = AbodeOf(k_Town, 400, 250),
	                      .foodPile = {.numbers = {7}},
	                      .otherPiles = {.numbers = {8, 9, 10, 11, 12}}};
	world.objects.emplace_back(pit);
	auto food = PileAt<FoodPileRecord>(At(211, 210), PotInfo::StoragePitFoodPile, 15, k_Pit, k_Town, 400);
	food.store.number = k_Pit;
	world.objects.emplace_back(food);
	for (size_t i = 1; i < k_PileRows.size(); ++i)
	{
		world.objects.emplace_back(PileAt<WoodPileRecord>(At(212, 205 + static_cast<int32_t>(i)), k_PileRows.at(i),
		                                                  15 + static_cast<uint32_t>(i), k_Pit, k_Town, 50));
	}
	// a field of the town: its kind's row, its food, its town, its turn offset, its crops and its growth
	world.objects.emplace_back(FieldRecord {.thing = {},
	                                        .base = BuildingAt(At(220, 190), AbodeInfo::CelticField, 20, 0.0f),
	                                        .abode = AbodeOf(k_Town, 0, 0),
	                                        .typeInfo = 1763,
	                                        .secondWord = std::bit_cast<uint32_t>(12.5f),
	                                        .town = {k_Town},
	                                        .thirdWord = 7,
	                                        .firstByte = 4,
	                                        .fourthWord = std::bit_cast<uint32_t>(80.0f)});
	// the reads end inside out: the piles inside the pit, the buildings inside the town, it inside the game
	world.loadOrder = {7, 8, 9, 10, 11, 12, 6, 3, 4, 5, 13, 2, 1};
	return world;
}
} // namespace

// Each abode, creche, graveyard, storage pit and field the load makes is written as its record was read, the pit
// naming its piles, the field with its kind, food, growth, crops and turn offset
TEST_P(AbodeWriteWorld, EachAbodeTheLoadMadeIsWrittenAsItsRecordWasRead)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), AbodeMakers(), &loaded);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	const auto written = ExpectEachRecordAsRead(world, loaded, _writers, source, true);
	EXPECT_EQ(written, (std::map<uint32_t, size_t> {{AbodeRecord::k_Type, 1},
	                                                {CrecheRecord::k_Type, 1},
	                                                {GraveyardRecord::k_Type, 1},
	                                                {StoragePitRecord::k_Type, 1},
	                                                {FieldRecord::k_Type, 1}}));
}

// A loaded abode is written with the villagers its record listed, without those gone since, and its counts as it holds
// them, as a villager leaves its abode as it is deleted
TEST_P(AbodeWriteWorld, ALoadedAbodeIsWrittenWithoutTheVillagersGoneSinceItsLoad)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), AbodeMakers(), &loaded);
	const auto found =
	    std::ranges::find_if(loaded, [](const auto& object) { return object.second.type == AbodeRecord::k_Type; });
	ASSERT_NE(found, loaded.end());
	const auto entity = found->second.entity;
	auto& registry = Registry();
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	loaded.try_emplace(40, LoadedObject {.entity = alive, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(41, LoadedObject {.entity = dead, .type = gamesave::VillagerRecord::k_Type});
	registry.Get<ecs::components::LoadedRecord<AbodeRecord>>(entity).record.abode.villagers.numbers = {41, 40};
	auto& abode = registry.Get<ecs::components::Abode>(entity);
	abode.adultCount = 1;
	abode.childCount = 0;
	abode.presentAtHome = 1;
	registry.Destroy(dead);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(AbodeRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& fields = std::get<AbodeRecord>(*written).abode;
	EXPECT_EQ(fields.villagers.numbers, (std::vector<gamesave::ObjectNumber> {40}));
	EXPECT_EQ(fields.adults, 1u);
	EXPECT_EQ(fields.children, 0u);
	EXPECT_EQ(fields.presentAtHome, 1u);
}

// A loaded field is written with the farmers its record listed, without those gone since, as a farmer leaves its field
// as its farming ends, which its deletion ends
TEST_P(AbodeWriteWorld, ALoadedFieldIsWrittenWithoutTheFarmersGoneSinceItsLoad)
{
	const auto world = SyntheticWorld();
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), AbodeMakers(), &loaded);
	const auto found =
	    std::ranges::find_if(loaded, [](const auto& object) { return object.second.type == FieldRecord::k_Type; });
	ASSERT_NE(found, loaded.end());
	const auto entity = found->second.entity;
	auto& registry = Registry();
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	loaded.try_emplace(40, LoadedObject {.entity = alive, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(41, LoadedObject {.entity = dead, .type = gamesave::VillagerRecord::k_Type});
	registry.Get<ecs::components::LoadedRecord<FieldRecord>>(entity).record.farmers.numbers = {40, 41};
	registry.Destroy(dead);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(FieldRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<FieldRecord>(*written).farmers.numbers, (std::vector<gamesave::ObjectNumber> {40}));
}

// An abode made in play is written from its state: its row, its place, size and angle, built whole, its town, no
// villagers, its food and its wood, and zero for every value whose meaning is not read
TEST_P(AbodeWriteWorld, AnAbodeMadeInPlayIsWrittenFromItsState)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	const auto hut = ecs::archetypes::AbodeArchetype::Create(3, at, AbodeInfo::CelticHut, 0.5f, 1.0f, 40, 60, false, false);
	ASSERT_TRUE(hut != entt::null);
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(AbodeRecord::k_Type))(context, hut);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	const auto& record = std::get<AbodeRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::CelticHut));
	EXPECT_EQ(record.base.position.position.x, map_coords::ToFixed(at.x));
	EXPECT_EQ(record.base.position.position.z, map_coords::ToFixed(at.z));
	EXPECT_EQ(record.base.object.heading, 0.5f);
	EXPECT_EQ(record.base.object.scale, 1.0f);
	EXPECT_EQ(record.base.multi.percentBuilt, 1.0f);
	EXPECT_EQ(record.abode.town.number, 2u);
	EXPECT_TRUE(record.abode.villagers.numbers.empty());
	EXPECT_EQ(record.abode.adults, 0);
	EXPECT_EQ(record.abode.resources.values, (std::vector<uint32_t> {40, 60}));
	// counted by its town's statistics as it is made whole, and drinking water as its search found it
	const auto& water = Registry().Get<const ecs::water_queries::DrinkingWater>(hut);
	EXPECT_EQ(record.abode.abodeFlags, water.found ? 0x03 : 0x02);
	EXPECT_EQ(record.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(record.abode.unusedWord, 0u);
	EXPECT_EQ(record.abode.index, 0);
	EXPECT_EQ(record.abode.destructionMesh, gamesave::DestructionMeshFlag {});
	EXPECT_EQ(record.thing, gamesave::ThingPart {});
}

// A field made in play is written as an abode of the field row, then its kind's row, its food, its town, its turn
// offset, the crops sown, its growth and its farmers; the words only a load writes are 0
TEST_P(AbodeWriteWorld, AFieldMadeInPlayIsWrittenFromItsState)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	// the abode part of a field of the row, its field's data as the turns left it
	const auto entity = ecs::archetypes::AbodeArchetype::CreatePlain(3, at, AbodeInfo::CelticField, 0.0f, 1.0f, 0, 0, false);
	ASSERT_TRUE(entity != entt::null);
	auto& field = Registry().Assign<ecs::components::Field>(entity, 3);
	field.type = FieldTypeInfo::Corn;
	field.food = 12.5f;
	field.growth = 80.0f;
	field.crops = 4;
	field.turnOffset = 7;
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {FieldRecord::k_Type});
	const auto written = (*_writers.WriterOf(FieldRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<FieldRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::CelticField));
	EXPECT_EQ(record.abode.town.number, 2u);
	// the original's Land 1 makes its fields of kind 0 and kind 2 with rows 1765 and 1763
	EXPECT_EQ(record.typeInfo, 1763u);
	EXPECT_EQ(record.firstWord, 0u);
	EXPECT_EQ(record.secondWord, std::bit_cast<uint32_t>(12.5f));
	EXPECT_EQ(record.town.number, 2u);
	EXPECT_EQ(record.thirdWord, 7);
	EXPECT_EQ(record.firstByte, 4);
	EXPECT_EQ(record.fourthWord, std::bit_cast<uint32_t>(80.0f));
	EXPECT_TRUE(record.farmers.numbers.empty());
	EXPECT_EQ(record.lastByte, 0);
}

// An abode of a field's kind made without its field's data writes no record (the save is refused for it), and reads
// nothing it does not have
TEST_P(AbodeWriteWorld, AFieldsAbodeWithoutItsFieldsDataWritesNoRecord)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	const auto entity = ecs::archetypes::AbodeArchetype::CreatePlain(3, at, AbodeInfo::CelticField, 0.0f, 1.0f, 0, 0, false);
	ASSERT_TRUE(entity != entt::null);
	ASSERT_FALSE(Registry().AllOf<ecs::components::Field>(entity));
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {FieldRecord::k_Type});
	EXPECT_FALSE((*_writers.WriterOf(FieldRecord::k_Type))(context, entity).has_value());
}

// A town centre made in play is written as an abode of its row, then its totem statue and its six icons by their
// slots; its totem statue of its tribe's row (378 less the tribe: the original's Norse and Aztec Land 1 totems are
// rows 371 and 376), built, with its town centre, the worship share it rises to, 0 and the share the hand holds it at
TEST_P(AbodeWriteWorld, ATownCentreAndItsTotemMadeInPlayAreWrittenFromTheirState)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	const auto centre =
	    ecs::archetypes::AbodeArchetype::Create(3, at, AbodeInfo::NorseTownCentre, 0.0f, 1.0f, 0, 0, false, false);
	ASSERT_TRUE(centre != entt::null);
	auto plinth = entt::entity {entt::null};
	Registry().Each<const ecs::components::VillageTotem>(
	    [&plinth, centre](entt::entity entity, const ecs::components::VillageTotem& totem) {
		    plinth = totem.townCentre == centre ? entity : plinth;
	    });
	if (plinth == entt::null)
	{
		plinth = Registry().Create();
		Registry().Assign<ecs::components::Transform>(plinth, at + glm::vec3(0.0f, 4.5f, 0.0f), glm::mat3(1.0f),
		                                              glm::vec3(1.0f));
		Registry().Assign<ecs::components::ObjectCreationIndex>(plinth, ecs::components::ObjectCreationIndex {.value = 63});
		Registry().Assign<ecs::components::VillageTotem>(plinth, centre);
	}
	Registry().Get<ecs::components::VillageTotem>(plinth).held = 0.5f;
	Registry().AssignOrReplace<ecs::components::TotemWorship>(plinth).percentage = 0.25f;
	const auto icon = Registry().Create();
	auto& icons = Registry().AssignOrReplace<ecs::components::TownCentreIcons>(centre);
	icons.icons = {entt::null, entt::null, icon, entt::null, entt::null, entt::null};
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), centre), std::vector {TownCentreRecord::k_Type});
	const auto written = (*_writers.WriterOf(TownCentreRecord::k_Type))(context, centre);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<TownCentreRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::NorseTownCentre));
	EXPECT_EQ(record.totemStatue.number, context.Reference(plinth));
	EXPECT_EQ(record.icons.numbers, (std::vector<ObjectNumber> {0, 0, context.Reference(icon), 0, 0, 0}));

	const auto totemWritten = (*_writers.WriterOf(TotemStatueRecord::k_Type))(context, plinth);
	ASSERT_TRUE(totemWritten.has_value()) << context.Failure();
	const auto& totem = std::get<TotemStatueRecord>(*totemWritten);
	EXPECT_EQ(totem.base.object.info, 371u);
	EXPECT_EQ(totem.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(totem.base.multi.percentBuilt, 1.0f);
	EXPECT_EQ(totem.object.number, context.Reference(centre));
	EXPECT_EQ(totem.words, (std::array<uint32_t, 3> {std::bit_cast<uint32_t>(0.25f), 0, std::bit_cast<uint32_t>(0.5f)}));
}

// A town centre of the save is made as an abode of its row, of its town, but not made functional: no totem and no icon
// is made with it, as their own records make them, and its town has it as its centre as the town's record names it; it
// is written back as it was read
TEST_P(AbodeWriteWorld, ASavedTownCentreIsMadeWithoutItsTotemAndIconsAndWrittenBackAsRead)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Centre = 3;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.townCentre.number = k_Centre;
	world.objects.emplace_back(town);
	TownCentreRecord centre {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::NorseTownCentre, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .totemStatue = {0},
	                         .icons = {.numbers = {0, 0, 0, 0, 0, 0}}};
	world.objects.emplace_back(centre);
	world.loadOrder = {3, 2, 1};
	LoadedObjects loaded;
	const auto census =
	    load::Apply(load::PlanOf(world), {.town = load::MakeTownEntity, .abode = load::MakeAbodeEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(k_Centre));
	ASSERT_TRUE(loaded.contains(k_Town));
	const auto entity = loaded.at(k_Centre).entity;
	const auto* abode = Registry().TryGet<const ecs::components::Abode>(entity);
	ASSERT_NE(abode, nullptr);
	EXPECT_EQ(abode->info, AbodeInfo::NorseTownCentre);
	size_t totems = 0;
	Registry().Each<const ecs::components::VillageTotem>([&totems](const ecs::components::VillageTotem&) { ++totems; });
	EXPECT_EQ(totems, 0u);
	EXPECT_FALSE(Registry().AllOf<ecs::components::TownCentreIcons>(entity));
	EXPECT_TRUE(Registry().Get<const ecs::components::Town>(loaded.at(k_Town).entity).centre == entity);
	const auto line = std::ranges::find(census, TownCentreRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 1u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {TownCentreRecord::k_Type});
	const auto written = (*_writers.WriterOf(TownCentreRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	centre.base.position.flags = PositionFlagsOf(Registry(), entity, centre.base.position.flags);
	EXPECT_TRUE(std::get<TownCentreRecord>(*written) == centre);
}

// A town centre's totem statue of the save is made as its load makes it: its tribe's plinth at its saved place, of its
// town centre, which has it as its totem, built as saved and standing at the share it was saved rising to, with the
// hand's share the same; the centre and the totem are written back as they were read, but for the hand's share, which
// the original's resolve sets to the share the totem stands at
TEST_P(AbodeWriteWorld, ASavedTotemStatueIsMadeOnItsTownCentreAndWrittenBackAsRead)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Centre = 3;
	constexpr ObjectNumber k_Totem = 4;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.townCentre.number = k_Centre;
	world.objects.emplace_back(town);
	TownCentreRecord centre {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::NorseTownCentre, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .totemStatue = {k_Totem},
	                         .icons = {.numbers = {0, 0, 0, 0, 0, 0}}};
	world.objects.emplace_back(centre);
	// the Norse totem's row (378 less the Norse tribe, 7), 4.5 m above the land, three quarters built
	TotemStatueRecord totem {.thing = {}, .base = BuildingAt(At(206, 201), AbodeInfo::NorseTownCentre, 22, 0.5f)};
	totem.base.object.info = 371;
	totem.base.position.position.altitude = 4.5f;
	totem.base.multi.percentBuilt = 0.75f;
	totem.object.number = k_Centre;
	totem.words = {std::bit_cast<uint32_t>(0.25f), 7, std::bit_cast<uint32_t>(0.5f)};
	world.objects.emplace_back(totem);
	// the totem is read inside its town centre, the centre inside its town
	world.loadOrder = {4, 3, 2, 1};
	LoadedObjects loaded;
	const auto census = load::Apply(
	    load::PlanOf(world),
	    {.town = load::MakeTownEntity, .abode = load::MakeAbodeEntity, .totemStatue = load::MakeTotemStatueEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(k_Totem));
	const auto plinth = loaded.at(k_Totem).entity;
	const auto made = loaded.at(k_Centre).entity;
	const auto* statue = Registry().TryGet<const ecs::components::VillageTotem>(plinth);
	ASSERT_NE(statue, nullptr);
	EXPECT_TRUE(statue->townCentre == made);
	EXPECT_TRUE(statue->onCentre);
	EXPECT_EQ(statue->percentBuilt, 0.75f);
	EXPECT_EQ(statue->held, 0.25f);
	const auto& worship = Registry().Get<const ecs::components::TotemWorship>(plinth);
	EXPECT_EQ(worship.percentage, 0.25f);
	EXPECT_EQ(worship.rise.value, 0.25f);
	EXPECT_EQ(worship.rise.destination, 0.25f);
	EXPECT_FALSE(worship.rising);
	const auto& saved = totem.base.position.position;
	const auto at = map_coords::ToWorld({.x = saved.x, .z = saved.z, .altitude = saved.altitude});
	EXPECT_EQ(Registry().Get<const ecs::components::Transform>(plinth).position, at);
	EXPECT_EQ(statue->restY, at.y);
	EXPECT_EQ(Registry().Get<const ecs::components::ObjectCreationIndex>(plinth).value, 22u);
	const auto line = std::ranges::find(census, TotemStatueRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 1u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), plinth), std::vector {TotemStatueRecord::k_Type});
	const auto writtenTotem = (*_writers.WriterOf(TotemStatueRecord::k_Type))(context, plinth);
	ASSERT_TRUE(writtenTotem.has_value()) << context.Failure();
	totem.base.position.flags = PositionFlagsOf(Registry(), plinth, totem.base.position.flags);
	totem.words.at(2) = totem.words.at(0);
	EXPECT_TRUE(std::get<TotemStatueRecord>(*writtenTotem) == totem);
	const auto writtenCentre = (*_writers.WriterOf(TownCentreRecord::k_Type))(context, made);
	ASSERT_TRUE(writtenCentre.has_value()) << context.Failure();
	centre.base.position.flags = PositionFlagsOf(Registry(), made, centre.base.position.flags);
	EXPECT_TRUE(std::get<TownCentreRecord>(*writtenCentre) == centre);
}

// A saved totem statue gets back the footpath link its record names, as every building's load puts its link back, so
// a save writes the link again; before, the loaded totem had none and its link was left out of the next save
TEST_P(AbodeWriteWorld, ASavedTotemStatueKeepsItsFootpathLink)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Centre = 3;
	constexpr ObjectNumber k_Totem = 4;
	constexpr ObjectNumber k_Link = 5;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.townCentre.number = k_Centre;
	world.objects.emplace_back(town);
	TownCentreRecord centre {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::NorseTownCentre, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .totemStatue = {k_Totem},
	                         .icons = {.numbers = {0, 0, 0, 0, 0, 0}}};
	world.objects.emplace_back(centre);
	TotemStatueRecord totem {.thing = {}, .base = BuildingAt(At(206, 201), AbodeInfo::NorseTownCentre, 22, 0.5f)};
	totem.base.object.info = 371;
	totem.base.multi.footpathLink = k_Link;
	totem.object.number = k_Centre;
	world.objects.emplace_back(totem);
	world.objects.emplace_back(gamesave::FootpathLinkRecord {});
	// the link is read inside the totem, the totem inside its town centre, the centre inside its town
	world.loadOrder = {5, 4, 3, 2, 1};
	LoadedObjects loaded;
	load::Apply(load::PlanOf(world),
	            {.footpathLink = load::MakeFootpathLinkEntity,
	             .town = load::MakeTownEntity,
	             .abode = load::MakeAbodeEntity,
	             .totemStatue = load::MakeTotemStatueEntity},
	            &loaded);
	ASSERT_TRUE(loaded.contains(k_Totem));
	ASSERT_TRUE(loaded.contains(k_Link));
	const auto plinth = loaded.at(k_Totem).entity;
	EXPECT_TRUE(ecs::footpaths::GetFootpathLink(plinth) == loaded.at(k_Link).entity);
}

// A town centre's spell icons of the save are made as their load makes them: each the town's icon model at its saved
// place, of its seed, its town and its town centre, in the slot the centre's record names it in, with its saved power-up
// levels; the town lists them in its record's order, and no worship site's icon is made. Each is written back as it was
// read, and so is the centre naming them by their slots
TEST_P(AbodeWriteWorld, ASavedTownCentresSpellIconsAreMadeInTheirSlotsAndWrittenBackAsRead)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Centre = 3;
	constexpr ObjectNumber k_Fire = 4;
	constexpr ObjectNumber k_Storm = 5;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.townCentre.number = k_Centre;
	// the town's list, the newest first
	town.spellIcons.numbers = {k_Storm, k_Fire};
	world.objects.emplace_back(town);
	TownCentreRecord centre {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::NorseTownCentre, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .totemStatue = {0},
	                         .icons = {.numbers = {0, k_Fire, 0, k_Storm, 0, 0}}};
	world.objects.emplace_back(centre);
	// the towns' icon row (2396 less 1), the seed's row (2578 less the seed: the storm's 2578, the fire's 2576), the
	// power-up levels the town holds
	const auto iconAt = [](int32_t x, uint32_t creation, uint32_t seedRow, std::vector<uint32_t> levels) {
		gamesave::TownCentreSpellIconRecord icon;
		icon.icon.base = BuildingAt(At(x, 202), AbodeInfo::NorseTownCentre, creation, 0.5f);
		icon.icon.base.object.info = 2395;
		icon.icon.info = seedRow;
		icon.icon.point = {std::bit_cast<float>(x << 16), std::bit_cast<float>(202 << 16), 0.0f};
		icon.town.number = k_Town;
		icon.object.number = k_Centre;
		icon.words.values = std::move(levels);
		return icon;
	};
	auto fire = iconAt(204, 23, 2576, {1, 0, 1});
	auto storm = iconAt(206, 24, 2578, {0, 0, 0});
	world.objects.emplace_back(fire);
	world.objects.emplace_back(storm);
	// the icons are read inside their town centre, the centre inside its town
	world.loadOrder = {4, 5, 3, 2, 1};
	UseParticles();
	RegisterSpellIconWriters(_writers);
	LoadedObjects loaded;
	const auto census = load::Apply(
	    load::PlanOf(world),
	    {.town = load::MakeTownEntity, .abode = load::MakeAbodeEntity, .townCentreIcon = load::MakeTownCentreSpellIconEntity},
	    &loaded);
	ASSERT_TRUE(loaded.contains(k_Fire));
	ASSERT_TRUE(loaded.contains(k_Storm));
	const auto made = loaded.at(k_Centre).entity;
	const auto fireIcon = loaded.at(k_Fire).entity;
	const auto stormIcon = loaded.at(k_Storm).entity;
	const auto& slots = Registry().Get<const ecs::components::TownCentreIcons>(made);
	EXPECT_TRUE(slots.icons.at(1) == fireIcon);
	EXPECT_TRUE(slots.icons.at(3) == stormIcon);
	const auto& ofTown = Registry().Get<const ecs::components::TownCentreSpellIcon>(fireIcon);
	EXPECT_TRUE(ofTown.town == loaded.at(k_Town).entity);
	EXPECT_TRUE(ofTown.townCentre == made);
	EXPECT_EQ(ofTown.slot, 1);
	EXPECT_EQ(ofTown.powerUps, (std::array {true, false, true}));
	EXPECT_EQ(Registry().Get<const ecs::components::SpellIcon>(fireIcon).seedType, SpellSeedType::Fire);
	EXPECT_EQ(Registry().Get<const ecs::components::SpellIcon>(stormIcon).seedType, SpellSeedType::Storm);
	EXPECT_EQ(Registry().Get<const ecs::components::TownMagic>(loaded.at(k_Town).entity).spellIcons,
	          (std::vector {stormIcon, fireIcon}));
	size_t siteIcons = 0;
	Registry().Each<const ecs::components::WorshipSpellIcon>(
	    [&siteIcons](const ecs::components::WorshipSpellIcon&) { ++siteIcons; });
	EXPECT_EQ(siteIcons, 0u);
	const auto line = std::ranges::find(census, gamesave::TownCentreSpellIconRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 2u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	for (auto [icon, read] : {std::pair {fireIcon, fire}, std::pair {stormIcon, storm}})
	{
		EXPECT_EQ(_writers.ClaimsOf(Registry(), icon), std::vector {gamesave::TownCentreSpellIconRecord::k_Type});
		const auto written = (*_writers.WriterOf(gamesave::TownCentreSpellIconRecord::k_Type))(context, icon);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		read.icon.base.position.flags = PositionFlagsOf(Registry(), icon, read.icon.base.position.flags);
		EXPECT_TRUE(std::get<gamesave::TownCentreSpellIconRecord>(*written) == read);
	}
	const auto writtenCentre = (*_writers.WriterOf(TownCentreRecord::k_Type))(context, made);
	ASSERT_TRUE(writtenCentre.has_value()) << context.Failure();
	centre.base.position.flags = PositionFlagsOf(Registry(), made, centre.base.position.flags);
	EXPECT_TRUE(std::get<TownCentreRecord>(*writtenCentre) == centre);
}

// The original's saves name no town centre in a centre's icons: each such icon is in the slot its centre's record names
// it in, as the centre's load reads its six icons, still naming no centre, and both are written back as read (the
// slots were left empty, so the town centre showed none of its icons)
TEST_P(AbodeWriteWorld, ATownCentresIconsThatNameNoCentreAreInTheSlotsItsRecordNames)
{
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Centre = 3;
	constexpr ObjectNumber k_Fire = 4;
	constexpr ObjectNumber k_Storm = 5;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.townCentre.number = k_Centre;
	town.spellIcons.numbers = {k_Storm, k_Fire};
	world.objects.emplace_back(town);
	TownCentreRecord centre {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::NorseTownCentre, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .totemStatue = {0},
	                         .icons = {.numbers = {k_Storm, 0, k_Fire, 0, 0, 0}}};
	world.objects.emplace_back(centre);
	const auto iconAt = [](int32_t x, uint32_t creation, uint32_t seedRow) {
		gamesave::TownCentreSpellIconRecord icon;
		icon.icon.base = BuildingAt(At(x, 202), AbodeInfo::NorseTownCentre, creation, 0.5f);
		icon.icon.base.object.info = 2395;
		icon.icon.info = seedRow;
		icon.icon.point = {std::bit_cast<float>(x << 16), std::bit_cast<float>(202 << 16), 0.0f};
		icon.town.number = k_Town;
		icon.object.number = 0;
		icon.words.values = {0, 0, 0};
		return icon;
	};
	auto fire = iconAt(204, 23, 2576);
	auto storm = iconAt(206, 24, 2578);
	world.objects.emplace_back(fire);
	world.objects.emplace_back(storm);
	world.loadOrder = {4, 5, 3, 2, 1};
	UseParticles();
	RegisterSpellIconWriters(_writers);
	LoadedObjects loaded;
	std::ignore = load::Apply(
	    load::PlanOf(world),
	    {.town = load::MakeTownEntity, .abode = load::MakeAbodeEntity, .townCentreIcon = load::MakeTownCentreSpellIconEntity},
	    &loaded);
	ASSERT_TRUE(loaded.contains(k_Fire) && loaded.contains(k_Storm) && loaded.contains(k_Centre));
	const auto made = loaded.at(k_Centre).entity;
	const auto fireIcon = loaded.at(k_Fire).entity;
	const auto stormIcon = loaded.at(k_Storm).entity;
	const auto* slots = Registry().TryGet<const ecs::components::TownCentreIcons>(made);
	ASSERT_NE(slots, nullptr);
	EXPECT_TRUE(slots->icons.at(0) == stormIcon);
	EXPECT_TRUE(slots->icons.at(2) == fireIcon);
	EXPECT_EQ(Registry().Get<const ecs::components::TownCentreSpellIcon>(fireIcon).slot, 2);
	EXPECT_TRUE(Registry().Get<const ecs::components::TownCentreSpellIcon>(fireIcon).townCentre == entt::null);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	for (auto [icon, read] : {std::pair {fireIcon, fire}, std::pair {stormIcon, storm}})
	{
		const auto written = (*_writers.WriterOf(gamesave::TownCentreSpellIconRecord::k_Type))(context, icon);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		read.icon.base.position.flags = PositionFlagsOf(Registry(), icon, read.icon.base.position.flags);
		EXPECT_TRUE(std::get<gamesave::TownCentreSpellIconRecord>(*written) == read);
	}
	const auto writtenCentre = (*_writers.WriterOf(TownCentreRecord::k_Type))(context, made);
	ASSERT_TRUE(writtenCentre.has_value()) << context.Failure();
	centre.base.position.flags = PositionFlagsOf(Registry(), made, centre.base.position.flags);
	EXPECT_TRUE(std::get<TownCentreRecord>(*writtenCentre) == centre);
}

// A workshop made in play is written as an abode of its row, then its wood pile, its sign of the wood it wants, the
// turns left for the scaffold being made, the scaffolds it owns, its three slots and its scaffolds, each by its number
TEST_P(AbodeWriteWorld, AWorkshopMadeInPlayIsWrittenFromItsState)
{
	RegisterPotWriters(_writers);
	RegisterWorshipSiteWriters(_writers);
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticWorkshop));
		row.abodeType = AbodeType::Workshop;
		row.tribeType = Tribe::CELTIC;
	}
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	// an abode made as a hut is, then of the workshop's row with a workshop's values and its sign (the workshop's own
	// making needs its ghost's shipped model, which the tests do not load)
	const auto entity = ecs::archetypes::AbodeArchetype::Create(3, at, AbodeInfo::CelticHut, 0.0f, 1.0f, 0, 0, false, false);
	ASSERT_TRUE(entity != entt::null);
	Registry().Get<ecs::components::Abode>(entity).info = AbodeInfo::CelticWorkshop;
	auto& workshop = Registry().AssignOrReplace<ecs::components::Workshop>(entity);
	workshop.showNeeds = ecs::show_needs::Create(at, entity, ecs::show_needs::k_WorkshopInfo);
	ASSERT_TRUE(workshop.showNeeds != entt::null);
	workshop.countdown = 7;
	workshop.owned = 1;
	workshop.slots = {2, 0, 1};
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {WorkshopRecord::k_Type});
	const auto written = (*_writers.WriterOf(WorkshopRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<WorkshopRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::CelticWorkshop));
	EXPECT_EQ(record.abode.town.number, 2u);
	EXPECT_EQ(record.first.number, workshop.woodPile != entt::null ? context.Reference(workshop.woodPile) : 0u);
	EXPECT_EQ(record.second.number, context.Reference(workshop.showNeeds));
	EXPECT_EQ(record.firstWord, 7u);
	EXPECT_EQ(record.secondWord, 1u);
	EXPECT_EQ(record.bytes.values, (std::vector<uint8_t> {2, 0, 1}));
	EXPECT_TRUE(record.objects.numbers.empty());
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	// a scaffold it holds is named in its list, as any other object
	const auto scaffold = Registry().Create();
	Registry().Assign<ecs::components::LoadedRecord<gamesave::ScaffoldRecord>>(scaffold, gamesave::ScaffoldRecord {});
	workshop.scaffolds.push_back(scaffold);
	WriteContext holding(_writers, source);
	const auto withScaffold = (*_writers.WriterOf(WorkshopRecord::k_Type))(holding, entity);
	ASSERT_TRUE(withScaffold.has_value()) << holding.Failure();
	EXPECT_EQ(std::get<WorkshopRecord>(*withScaffold).objects.numbers, std::vector {holding.Reference(scaffold)});
	EXPECT_TRUE(holding.Failure().empty()) << holding.Failure();
}

// A spell dispenser made in play (a new Land 3's has one) is written as an abode of its row, then the orb it made, the
// turns it has counted, the turns between its orbs, whether it is active and its miracle; a quick save over it was
// refused before, no writer claiming it
TEST_P(AbodeWriteWorld, ASpellDispenserMadeInPlayIsWrittenFromItsState)
{
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser));
		row.abodeType = AbodeType::SpellDispenser;
		row.tribeType = Tribe::CELTIC;
	}
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	// an abode made as a hut is, then of the dispenser's row with a dispenser's values
	const auto entity = ecs::archetypes::AbodeArchetype::Create(3, at, AbodeInfo::CelticHut, 0.0f, 1.0f, 0, 0, false, false);
	ASSERT_TRUE(entity != entt::null);
	Registry().Get<ecs::components::Abode>(entity).info = AbodeInfo::CelticSpellDispenser;
	auto& dispenser = Registry().AssignOrReplace<ecs::components::SpellDispenser>(entity);
	dispenser.magicType = MagicType::Heal;
	dispenser.timer = {.tick = 42, .period = 300, .active = true};
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {gamesave::SpellDispenserRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::SpellDispenserRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::SpellDispenserRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::CelticSpellDispenser));
	EXPECT_EQ(record.abode.town.number, 2u);
	EXPECT_EQ(record.object.number, 0u);
	EXPECT_EQ(record.words, (std::array<uint32_t, 4> {42, 300, 1, static_cast<uint32_t>(MagicType::Heal)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	// the orb it made is named by its number, as any other object
	const auto orb = Registry().Create();
	Registry().Assign<ecs::components::LoadedRecord<gamesave::OneOffSpellSeedRecord>>(orb, gamesave::OneOffSpellSeedRecord {});
	dispenser.orb = orb;
	WriteContext holding(_writers, source);
	const auto withOrb = (*_writers.WriterOf(gamesave::SpellDispenserRecord::k_Type))(holding, entity);
	ASSERT_TRUE(withOrb.has_value()) << holding.Failure();
	EXPECT_EQ(std::get<gamesave::SpellDispenserRecord>(*withOrb).object.number, holding.Reference(orb));
}

// A spell dispenser of the save is made again as an abode of its row with its count, period, activity and magic as
// saved, keeps its record, and is written back with the same values
TEST_P(AbodeWriteWorld, ASavedSpellDispenserIsMadeWithItsValuesAndWrittenBack)
{
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser));
		row.abodeType = AbodeType::SpellDispenser;
		row.tribeType = Tribe::CELTIC;
		row.meshId = MeshId::BuildingCeltic1;
	}
	LoadAbodeMesh(AbodeInfo::CelticSpellDispenser);
	UseParticles();
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	gamesave::SpellDispenserRecord record;
	record.base = BuildingAt(At(150, 149), AbodeInfo::CelticSpellDispenser, 101, 0.0f);
	record.abode.town.number = 2;
	record.words = {42, 300, 1, static_cast<uint32_t>(MagicType::Heal)};
	const auto entity = load::MakeAbodeEntity(
	    {.number = 9, .type = AbodeInfo::CelticSpellDispenser, .base = record.base, .abode = record.abode, .record = record},
	    3);
	ASSERT_TRUE(entity != entt::null);
	const auto* dispenser = Registry().TryGet<const ecs::components::SpellDispenser>(entity);
	ASSERT_NE(dispenser, nullptr);
	EXPECT_EQ(dispenser->magicType, MagicType::Heal);
	EXPECT_EQ(dispenser->timer.tick, 42u);
	EXPECT_EQ(dispenser->timer.period, 300u);
	EXPECT_TRUE(dispenser->timer.active);
	EXPECT_EQ(dispenser->orb, entt::entity {entt::null});
	EXPECT_TRUE(Registry().AllOf<ecs::components::LoadedRecord<gamesave::SpellDispenserRecord>>(entity));

	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}},
	                                     {9, LoadedObject {.entity = entity, .type = gamesave::SpellDispenserRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {gamesave::SpellDispenserRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::SpellDispenserRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::SpellDispenserRecord>(*written).words, record.words);
}

// A spell dispenser's effect on the land under it starts as its creation starts it, for a dispenser saved available;
// one saved not available (in a hand or held by a script) is made without it
TEST_P(AbodeWriteWorld, ASavedSpellDispenserStartsItsEffectOnlyWhenItWasSavedAvailable)
{
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser));
		row.abodeType = AbodeType::SpellDispenser;
		row.tribeType = Tribe::CELTIC;
		row.meshId = MeshId::BuildingCeltic1;
	}
	LoadAbodeMesh(AbodeInfo::CelticSpellDispenser);
	/// Every effect started is the next number, from 1
	class CountedParticles: public test::InertParticleSystem
	{
	public:
		EffectId Start(std::string_view /*file*/, glm::vec3 /*origin*/, float /*magnitude*/, bool /*synced*/) override
		{
			return ++_started;
		}

	private:
		EffectId _started {0};
	};
	Locator::particleSystem::emplace<CountedParticles>();
	TownRecord townRecord;
	townRecord.id = 3;
	std::ignore = load::MakeTownEntity({.number = 2, .record = townRecord});
	const auto made = [](gamesave::MapCoords at, gamesave::ObjectNumber number, uint8_t thingFlags) {
		gamesave::SpellDispenserRecord record;
		record.thing.flags = thingFlags;
		record.base = BuildingAt(at, AbodeInfo::CelticSpellDispenser, number, 0.0f);
		record.abode.town.number = 2;
		return load::MakeAbodeEntity({.number = number,
		                              .type = AbodeInfo::CelticSpellDispenser,
		                              .base = record.base,
		                              .abode = record.abode,
		                              .record = record},
		                             3);
	};
	const auto available = made(At(150, 149), 9, 0);
	const auto held = made(At(170, 149), 10, 1);
	ASSERT_TRUE(available != entt::null);
	ASSERT_TRUE(held != entt::null);
	EXPECT_EQ(Registry().Get<const ecs::components::SpellDispenser>(available).effect, 1u);
	EXPECT_EQ(Registry().Get<const ecs::components::SpellDispenser>(held).effect, 0u);
}

// A workshop of the save is made as its load reads it: of its row, with no sign and pile of its own making, its turns
// left, the scaffolds it owns and its slots; its sign made from the sign's own record, of the workshop, with its
// creation number and desire; in its town's list. Both are written back as they were read; before, a loaded game made
// no workshop
TEST_P(AbodeWriteWorld, ASavedWorkshopIsMadeWithItsSignAndWrittenBackAsRead)
{
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticWorkshop));
		row.abodeType = AbodeType::Workshop;
		row.tribeType = Tribe::CELTIC;
		row.meshId = MeshId::BuildingCeltic1;
	}
	LoadAbodeMesh(AbodeInfo::CelticWorkshop);
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Workshop = 3;
	constexpr ObjectNumber k_Sign = 4;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	town.workshops.numbers = {k_Workshop};
	world.objects.emplace_back(town);
	WorkshopRecord workshop {.thing = {},
	                         .base = BuildingAt(At(205, 200), AbodeInfo::CelticWorkshop, 21, 0.5f),
	                         .abode = AbodeOf(k_Town, 0, 0),
	                         .first = {0},
	                         .second = {k_Sign},
	                         .firstWord = 7,
	                         .secondWord = 1,
	                         .bytes = {.values = {2, 0, 1}},
	                         .objects = {}};
	world.objects.emplace_back(workshop);
	gamesave::ShowNeedsVisualsRecord sign;
	sign.position.position = At(205, 200);
	sign.object.info = 1855 - ecs::show_needs::k_WorkshopInfo;
	sign.object.creationIndex = 33;
	sign.other.number = k_Workshop;
	sign.lastWord = std::bit_cast<uint32_t>(0.5f);
	world.objects.emplace_back(sign);
	world.loadOrder = {4, 3, 2, 1};
	LoadedObjects loaded;
	const auto census =
	    load::Apply(load::PlanOf(world), {.town = load::MakeTownEntity, .abode = load::MakeAbodeEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(k_Workshop));
	ASSERT_TRUE(loaded.contains(k_Sign));
	const auto entity = loaded.at(k_Workshop).entity;
	const auto* made = Registry().TryGet<const ecs::components::Workshop>(entity);
	ASSERT_NE(made, nullptr);
	EXPECT_EQ(made->countdown, 7);
	EXPECT_EQ(made->owned, 1);
	EXPECT_EQ(made->slots, (std::array<uint8_t, 3> {2, 0, 1}));
	EXPECT_TRUE(made->woodPile == entt::null);
	const auto signEntity = loaded.at(k_Sign).entity;
	EXPECT_TRUE(made->showNeeds == signEntity);
	const auto& visuals = Registry().Get<const ecs::components::ShowNeedsVisuals>(signEntity);
	EXPECT_TRUE(visuals.owner == entity);
	EXPECT_EQ(visuals.infoIndex, ecs::show_needs::k_WorkshopInfo);
	EXPECT_EQ(visuals.desire, 0.5f);
	EXPECT_EQ(Registry().Get<const ecs::components::ObjectCreationIndex>(signEntity).value, 33u);
	// the only signs are the saved one: the workshop made none of its own
	size_t signs = 0;
	Registry().Each<const ecs::components::ShowNeedsVisuals>([&signs](const ecs::components::ShowNeedsVisuals&) { ++signs; });
	EXPECT_EQ(signs, 1u);
	EXPECT_EQ(Registry().Get<const ecs::components::Town>(loaded.at(k_Town).entity).workshops, std::vector {entity});
	const auto line = std::ranges::find(census, WorkshopRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 1u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {WorkshopRecord::k_Type});
	const auto written = (*_writers.WriterOf(WorkshopRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	workshop.base.position.flags = PositionFlagsOf(Registry(), entity, workshop.base.position.flags);
	EXPECT_TRUE(std::get<WorkshopRecord>(*written) == workshop);
	EXPECT_EQ(context.Reference(signEntity), k_Sign);
}

// A fish farm of the save is made as a script makes it, of the town its record names, with its stock and creation
// number, and is written back as it was read, its fishermen as the record names them
TEST_P(AbodeWriteWorld, ASavedFishFarmIsMadeAndWrittenBackAsRead)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.landNumber = 1;
	world.objects.emplace_back(game);
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	world.objects.emplace_back(town);
	FishFarmRecord farm;
	farm.base = BuildingAt(At(150, 149), AbodeInfo::CelticHut, 101, 0.0f);
	farm.base.object.info = 1759;
	farm.base.position.flags = 0x8003;
	farm.villagers.numbers = {};
	farm.town.number = 2;
	farm.first = 1.0f;
	farm.second = 900.0f;
	world.objects.emplace_back(farm);
	world.loadOrder = {3, 2, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world),
	                          load::Makers {.town = load::MakeTownEntity, .fishFarm = load::MakeFishFarmEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(3));
	const auto entity = loaded.at(3).entity;
	const auto& made = Registry().Get<const ecs::components::FishFarm>(entity);
	EXPECT_EQ(made.town, loaded.at(2).entity);
	EXPECT_EQ(made.food, 900.0f);
	EXPECT_EQ(ecs::object_index::Of(entity), 101);
	ClassWriters writers;
	RegisterFishFarmWriters(writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(FishFarmRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<FishFarmRecord>(*written);
	EXPECT_EQ(record.base.object, farm.base.object);
	EXPECT_EQ(record.base.multi, farm.base.multi);
	EXPECT_EQ(record.town.number, 2u);
	EXPECT_EQ(record.first, 1.0f);
	EXPECT_EQ(record.second, 900.0f);

	// its saved fishermen without those gone since, as a fisherman leaves its farm as its fishing ends
	const auto alive = Registry().Create();
	const auto dead = Registry().Create();
	loaded.try_emplace(40, LoadedObject {.entity = alive, .type = gamesave::VillagerRecord::k_Type});
	loaded.try_emplace(41, LoadedObject {.entity = dead, .type = gamesave::VillagerRecord::k_Type});
	Registry().Get<ecs::components::LoadedRecord<FishFarmRecord>>(entity).record.villagers.numbers = {41, 40};
	Registry().Destroy(dead);
	const WorldSource after {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext again(writers, after);
	const auto rewritten = (*writers.WriterOf(FishFarmRecord::k_Type))(again, entity);
	ASSERT_TRUE(rewritten.has_value()) << again.Failure();
	EXPECT_EQ(std::get<FishFarmRecord>(*rewritten).villagers.numbers, (std::vector<gamesave::ObjectNumber> {40}));
}

// A fish farm made in play: a whole building of the fish farm's row, no fishermen yet, its town, the constructor's 1
// and its stock
TEST_P(AbodeWriteWorld, AFishFarmMadeInPlayIsWrittenFromItsState)
{
	TownRecord townRecord;
	townRecord.id = 3;
	const auto town = load::MakeTownEntity({.number = 2, .record = townRecord});
	const auto entity = Registry().Create();
	Registry().Assign<ecs::components::Transform>(entity, glm::vec3(1505.0f, k_Ground, 1495.0f), glm::mat3(1.0f),
	                                              glm::vec3(1.0f));
	Registry().Assign<ecs::components::ObjectCreationIndex>(entity, ecs::components::ObjectCreationIndex {.value = 101});
	auto& farm = Registry().Assign<ecs::components::FishFarm>(entity);
	farm.town = town;
	farm.food = 1400.0f;
	ClassWriters writers;
	RegisterFishFarmWriters(writers);
	const WorldSource source {.registry = Registry(),
	                          .loaded = {{2, LoadedObject {.entity = town, .type = TownRecord::k_Type}}}};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(FishFarmRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<FishFarmRecord>(*written);
	// the row every fish farm of the original's new Land 1 game has
	EXPECT_EQ(record.base.object.info, 1759u);
	EXPECT_EQ(record.base.object.creationIndex, 101u);
	EXPECT_EQ(record.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(record.base.multi.percentBuilt, 1.0f);
	EXPECT_TRUE(record.villagers.numbers.empty());
	EXPECT_EQ(record.town.number, 2u);
	EXPECT_EQ(record.first, 1.0f);
	EXPECT_EQ(record.second, 1400.0f);
}

namespace
{
/// The abodes' makers, and the game's for the scaffolds (every other moving object keeps its record) and the building
/// sites
load::Makers ScaffoldMakers()
{
	auto makers = AbodeMakers();
	makers.mobileObject = [](const load::PlannedMobileObject& object) {
		if (std::holds_alternative<gamesave::ScaffoldRecord>(object.record))
		{
			return load::MakeMobileObjectEntity(object);
		}
		return std::visit([](const auto& record) { return Keeping(record); }, object.record);
	};
	makers.buildingSite = load::MakeBuildingSiteEntity;
	return makers;
}

/// The record the writers make of an object, by its number
std::optional<SavedRecord> WrittenOf(WriteContext& context, ObjectNumber number)
{
	auto made = context.RecordOf(number);
	return made.record != nullptr ? std::optional(*made.record) : std::nullopt;
}

/// Whether a scaffold's record is written as it was read, but for the numbers of its two plans, which the write gives
void ExpectScaffoldAsRead(const gamesave::ScaffoldRecord& written, const gamesave::ScaffoldRecord& read, ObjectNumber number)
{
	EXPECT_EQ(written.thing, read.thing) << number;
	EXPECT_EQ(written.base, read.base) << number;
	EXPECT_EQ(written.objects[0], read.objects[0]) << number;
	EXPECT_EQ(written.objects[1].number != 0, read.objects[1].number != 0) << number;
	EXPECT_EQ(written.objects[2].number != 0, read.objects[2].number != 0) << number;
	EXPECT_EQ(written.firstWord, read.firstWord) << number;
	EXPECT_EQ(written.other, read.other) << number;
	EXPECT_EQ(written.secondWord, read.secondWord) << number;
	EXPECT_EQ(written.thirdWord, read.thirdWord) << number;
	EXPECT_EQ(written.half, read.half) << number;
	EXPECT_EQ(written.last, read.last) << number;
	EXPECT_EQ(written.lastWord, read.lastWord) << number;
}
} // namespace

// A saved scaffold is made again as a moving object of its place, size and angle, before its building site: a site's
// with no plan, in its site's list, its phantom its site's building's; a workshop's with its plan and candidate as their
// records hold them (where, angle, scale, turn made, the building offered), its phantom its plan's building's (none
// here); each with its cross-fade's timer, the phantom's angle and scale, its flags and the building it may offer as
// saved, its site, workshop and town as its record names them, counting as placed. Each is written back as it was
// read, its two plans too
TEST_P(AbodeWriteWorld, ASavedScaffoldIsMadeBeforeItsSiteWithItsPlansAndWrittenBackAsRead)
{
	constexpr ObjectNumber k_Town = 2;
	if (!GameData())
	{
		Info().scaffold.meshId = MeshId::ObjectRockChalk;
		// a scaffold is of the moving objects' type, as the game's row
		Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).type = ObjectType::MobileObject;
		Info().scaffold.type = ObjectType::MobileObject;
	}
	LoadMesh(Info().scaffold.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	LoadMesh(Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 1.0f, 1.0f));
	const auto scaffoldAt = [](const gamesave::MapCoords& at, uint32_t creation) {
		gamesave::ScaffoldRecord record;
		record.base.position.position = at;
		record.base.position.flags = position_flags::k_InMap;
		record.base.object.info = 1938;
		record.base.object.coords = at;
		record.base.object.creationIndex = creation;
		record.base.object.scale = 0.7f;
		record.base.object.heading = 0.5f;
		record.last.number = k_Town;
		record.lastWord = 0xFFFFFFFF;
		return record;
	};
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.landNumber = 2;
	world.objects.emplace_back(game);
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	world.objects.emplace_back(town);
	world.objects.emplace_back(AbodeRecord {
	    .thing = {}, .base = BuildingAt(At(190, 190), AbodeInfo::CelticHut, 11, 0.5f), .abode = AbodeOf(k_Town, 0, 0)});
	gamesave::StandardBuildingSiteRecord site;
	site.rootBuilding.number = 3;
	site.scaffolds.numbers = {5};
	world.objects.emplace_back(site);
	auto onSite = scaffoldAt(At(191, 190), 21);
	onSite.objects[0].number = 4;
	onSite.firstWord = 300;
	onSite.secondWord = std::bit_cast<uint32_t>(0.78f);
	onSite.thirdWord = std::bit_cast<uint32_t>(0.7f);
	onSite.half = 0x77;
	world.objects.emplace_back(onSite);
	// the workshop: an abode of the town here, which a workshop's scaffold names as what it carries
	world.objects.emplace_back(AbodeRecord {
	    .thing = {}, .base = BuildingAt(At(210, 190), AbodeInfo::CelticHut, 12, 0.0f), .abode = AbodeOf(k_Town, 0, 0)});
	auto stored = scaffoldAt(At(211, 191), 22);
	stored.base.mobileObject.object = 6;
	stored.objects[1].number = 8;
	stored.objects[2].number = 9;
	stored.half = 0x40;
	world.objects.emplace_back(stored);
	gamesave::PlannedAbodeRecord plan;
	plan.planned.position.position = At(211, 191);
	plan.planned.scale = 1.0f;
	plan.planned.creationTurn = 14875;
	plan.planned.info = 0xFFFFFFFF;
	world.objects.emplace_back(plan);
	auto candidate = plan;
	candidate.planned.yAngle = 0.25f;
	candidate.planned.info = load::AbodeInfoRowOf(AbodeInfo::CelticHut);
	world.objects.emplace_back(candidate);
	world.loadOrder = {5, 4, 3, 8, 9, 7, 6, 2, 1};

	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), ScaffoldMakers(), &loaded);
	const auto line = std::ranges::find(census, gamesave::ScaffoldRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 2u);
	ASSERT_TRUE(loaded.contains(4) && loaded.contains(5) && loaded.contains(7));
	auto& registry = Registry();
	const auto siteEntity = loaded.at(4).entity;
	const auto first = loaded.at(5).entity;
	const auto second = loaded.at(7).entity;
	EXPECT_EQ(registry.Get<const ecs::components::BuildingSite>(siteEntity).scaffolds, std::vector {first});
	const auto& madeOnSite = registry.Get<const ecs::components::Scaffold>(first);
	EXPECT_TRUE(madeOnSite.site == siteEntity);
	EXPECT_TRUE(madeOnSite.town == loaded.at(k_Town).entity);
	EXPECT_FALSE(madeOnSite.plan.has_value());
	EXPECT_FALSE(madeOnSite.candidate.has_value());
	EXPECT_EQ(madeOnSite.fade, 300);
	EXPECT_EQ(madeOnSite.phantomAngle, 0.78f);
	EXPECT_EQ(madeOnSite.phantomScale, 0.7f);
	EXPECT_EQ(madeOnSite.flags, 0x77);
	EXPECT_EQ(madeOnSite.limit, -1);
	EXPECT_EQ(madeOnSite.placedTurn, 1u);
	EXPECT_EQ(madeOnSite.phantomMesh,
	          resources::HashIdentifier(Info().abode.at(static_cast<size_t>(AbodeInfo::CelticHut)).meshId));
	const auto& madeStored = registry.Get<const ecs::components::Scaffold>(second);
	EXPECT_TRUE(madeStored.owner == loaded.at(6).entity);
	EXPECT_TRUE(madeStored.site == entt::entity {entt::null});
	ASSERT_TRUE(madeStored.plan.has_value());
	EXPECT_EQ(madeStored.plan->info, nullptr);
	EXPECT_EQ(madeStored.plan->creationTurn, 14875u);
	EXPECT_EQ(madeStored.plan->scale, 1.0f);
	ASSERT_TRUE(madeStored.candidate.has_value());
	EXPECT_EQ(madeStored.candidate->info, &Info().abode.at(static_cast<size_t>(AbodeInfo::CelticHut)));
	EXPECT_EQ(madeStored.candidate->yAngle, 0.25f);
	EXPECT_EQ(madeStored.phantomMesh, 0u);
	EXPECT_EQ(registry.Get<const ecs::components::ObjectCreationIndex>(second).value, 22u);

	RegisterPotWriters(_writers);
	RegisterTownWriters(_writers);
	RegisterBuildingSiteWriters(_writers);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, first), std::vector {gamesave::ScaffoldRecord::k_Type});
	const auto writtenOnSite = WrittenOf(context, 5);
	ASSERT_TRUE(writtenOnSite.has_value()) << context.Failure();
	EXPECT_TRUE(*writtenOnSite == SavedRecord(onSite));
	const auto writtenStored = WrittenOf(context, 7);
	ASSERT_TRUE(writtenStored.has_value()) << context.Failure();
	const auto& storedBack = std::get<gamesave::ScaffoldRecord>(*writtenStored);
	ExpectScaffoldAsRead(storedBack, stored, 7);
	const auto writtenPlan = WrittenOf(context, storedBack.objects[1].number);
	ASSERT_TRUE(writtenPlan.has_value()) << context.Failure();
	EXPECT_TRUE(*writtenPlan == SavedRecord(plan));
	const auto writtenCandidate = WrittenOf(context, storedBack.objects[2].number);
	ASSERT_TRUE(writtenCandidate.has_value()) << context.Failure();
	EXPECT_TRUE(*writtenCandidate == SavedRecord(candidate));
	const auto writtenSite = WrittenOf(context, 4);
	ASSERT_TRUE(writtenSite.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::StandardBuildingSiteRecord>(*writtenSite).scaffolds.numbers, std::vector<ObjectNumber> {5});
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 2 autosave's twelve scaffolds,
// six on building sites (two of them still naming their workshop) and six in workshops with their plans, are made again
// and each is written as it was read, its plans too: the workshops are not made, so their scaffolds name them as their
// records do
TEST_P(AbodeWriteWorld, TheFrozenLandTwoAutosavesScaffoldsAreMadeAndWrittenAsRead)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 2 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto plan = load::PlanOf(world);
	for (const auto& object : plan.objects)
	{
		if (const auto* abode = std::get_if<load::PlannedAbode>(&object); abode != nullptr)
		{
			LoadAbodeMesh(abode->type);
		}
		else if (const auto* pit = std::get_if<load::PlannedStoragePit>(&object); pit != nullptr)
		{
			LoadAbodeMesh(pit->type);
		}
		else if (const auto* pile = std::get_if<load::PlannedPile>(&object); pile != nullptr)
		{
			LoadPileMesh(pile->type);
		}
	}
	LoadMesh(Info().scaffold.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	LoadMesh(Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 1.0f, 1.0f));
	LoadedObjects loaded;
	std::ignore = load::Apply(plan, ScaffoldMakers(), &loaded);
	ListAsSaved(world, loaded);
	RegisterPotWriters(_writers);
	RegisterTownWriters(_writers);
	RegisterBuildingSiteWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	size_t scaffolds = 0;
	size_t plans = 0;
	size_t onSites = 0;
	for (const auto& [number, object] : loaded)
	{
		if (object.type != gamesave::ScaffoldRecord::k_Type)
		{
			continue;
		}
		const auto& read = std::get<gamesave::ScaffoldRecord>(*world.objects.at(number - 1));
		const auto written = WrittenOf(context, number);
		ASSERT_TRUE(written.has_value()) << number << ": " << context.Failure();
		const auto& back = std::get<gamesave::ScaffoldRecord>(*written);
		ExpectScaffoldAsRead(back, read, number);
		onSites += read.objects[0].number != 0 ? 1 : 0;
		for (size_t i = 1; i < back.objects.size(); ++i)
		{
			if (read.objects.at(i).number == 0 || back.objects.at(i).number == 0)
			{
				continue;
			}
			const auto writtenPlan = WrittenOf(context, back.objects.at(i).number);
			ASSERT_TRUE(writtenPlan.has_value()) << number << ": " << context.Failure();
			const auto& readPlan = std::get<gamesave::PlannedAbodeRecord>(*world.objects.at(read.objects.at(i).number - 1));
			const auto& backPlan = std::get<gamesave::PlannedAbodeRecord>(*writtenPlan);
			EXPECT_EQ(backPlan.planned, readPlan.planned) << number;
			EXPECT_EQ(backPlan.town, readPlan.town) << number;
			EXPECT_EQ(backPlan.thing.flags, readPlan.thing.flags) << number;
			++plans;
		}
		++scaffolds;
	}
	EXPECT_EQ(scaffolds, 12u);
	EXPECT_EQ(onSites, 6u);
	EXPECT_EQ(plans, 12u);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

namespace
{
/// Every player's most tribal power of each tribe
std::vector<std::array<float, ecs::components::PlayerMagic::k_Tribes>> MostTribalPowers()
{
	std::vector<std::array<float, ecs::components::PlayerMagic::k_Tribes>> powers;
	for (size_t player = 0; player < static_cast<size_t>(PlayerNames::_COUNT); ++player)
	{
		powers.push_back(magic::players::ReadMagicOf(static_cast<PlayerNames>(player)).maxTribalPower);
	}
	return powers;
}

/// The trees and big forests of a frozen save keep their records on entities of their own, as the abodes' makers keep
/// them: the load makes every planned one through its maker
entt::entity KeepTree(const load::PlannedTree& tree, uint32_t /*forest*/)
{
	return Keeping(tree.record);
}
entt::entity KeepBigForest(const load::PlannedBigForest& big, uint32_t /*forest*/)
{
	return Keeping(big.record);
}
} // namespace

// A wonder of the save is made as an abode of its row, of its town, with the power its record keeps, and leaves every
// player's most tribal power as it was (the players' records hold it); the census counts it made, and it is written back
// as it was read; its power, changed since, is written as it is now
TEST_P(AbodeWriteWorld, ASavedWonderIsMadeWithItsPowerAndWrittenBackAsRead)
{
	test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticWonder));
		row.abodeType = AbodeType::Wonder;
		row.tribeType = Tribe::CELTIC;
		row.meshId = MeshId::BuildingCeltic1;
	}
	LoadAbodeMesh(AbodeInfo::CelticWonder);
	constexpr ObjectNumber k_Town = 2;
	constexpr ObjectNumber k_Wonder = 3;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	TownRecord town;
	town.container.position.position = At(200, 200);
	town.id = 1;
	world.objects.emplace_back(town);
	WonderRecord wonder {.thing = {.flags = 2},
	                     .base = BuildingAt(At(205, 200), AbodeInfo::CelticWonder, 31, 0.5f),
	                     .abode = AbodeOf(k_Town, 0, 0),
	                     .word = std::bit_cast<uint32_t>(2.5f)};
	world.objects.emplace_back(wonder);
	world.loadOrder = {3, 2, 1};
	const auto before = MostTribalPowers();
	LoadedObjects loaded;
	const auto census =
	    load::Apply(load::PlanOf(world), {.town = load::MakeTownEntity, .wonder = load::MakeWonderEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(k_Wonder));
	EXPECT_EQ(loaded.at(k_Wonder).type, WonderRecord::k_Type);
	const auto entity = loaded.at(k_Wonder).entity;
	const auto* abode = Registry().TryGet<const ecs::components::Abode>(entity);
	ASSERT_NE(abode, nullptr);
	EXPECT_EQ(abode->info, AbodeInfo::CelticWonder);
	EXPECT_TRUE(ecs::town_queries::TownByKey(abode->townId) == loaded.at(k_Town).entity);
	EXPECT_EQ(ecs::wonders::GetPower(entity), 2.5f);
	EXPECT_TRUE(Registry().AllOf<ecs::components::LoadedRecord<WonderRecord>>(entity));
	EXPECT_FALSE(Registry().AllOf<ecs::components::LoadedRecord<AbodeRecord>>(entity));
	EXPECT_EQ(MostTribalPowers(), before);
	const auto line = std::ranges::find(census, WonderRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->saved, 1u);
	EXPECT_EQ(line->made, 1u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {WonderRecord::k_Type});
	const auto written = (*_writers.WriterOf(WonderRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	wonder.base.position.flags = PositionFlagsOf(Registry(), entity, wonder.base.position.flags);
	EXPECT_TRUE(std::get<WonderRecord>(*written) == wonder);

	ecs::wonders::SetPower(entity, 3.0f);
	const auto changed = (*_writers.WriterOf(WonderRecord::k_Type))(context, entity);
	ASSERT_TRUE(changed.has_value()) << context.Failure();
	EXPECT_EQ(std::get<WonderRecord>(*changed).word, std::bit_cast<uint32_t>(3.0f));
}

// A wonder made in play, of no record, is claimed by the wonders' writer alone and written with its row and its power
TEST_P(AbodeWriteWorld, AWonderMadeInPlayIsWrittenWithItsPower)
{
	test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	if (!GameData())
	{
		auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticWonder));
		row.abodeType = AbodeType::Wonder;
		row.tribeType = Tribe::CELTIC;
		row.meshId = MeshId::BuildingCeltic1;
	}
	LoadAbodeMesh(AbodeInfo::CelticWonder);
	const glm::vec3 at(1505.0f, k_Ground, 1495.0f);
	const auto entity = ecs::archetypes::AbodeArchetype::Create(ecs::components::Abode::k_NoTown, at, AbodeInfo::CelticWonder,
	                                                            0.5f, 1.25f, 0, 0, false, false, nullptr, false);
	ASSERT_NE(entity, entt::entity {entt::null});
	ASSERT_TRUE(ecs::wonders::IsWonder(entity));
	const LoadedObjects none;
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = nullptr, .loaded = none};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {WonderRecord::k_Type});
	const auto written = (*_writers.WriterOf(WonderRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<WonderRecord>(*written);
	EXPECT_EQ(record.base.object.info, load::AbodeInfoRowOf(AbodeInfo::CelticWonder));
	EXPECT_EQ(record.word, std::bit_cast<uint32_t>(ecs::wonders::GetPower(entity)));
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 3 saves hold three wonders each, which the load plans; those of the first are made, each with its
// power as saved, and written back as they were read
TEST_P(AbodeWriteWorld, TheFrozenLandThreeSavesWondersAreMadeAndWrittenAsRead)
{
	test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	const std::filesystem::path games = std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games";
	const std::array saves {games / "0" / "0.sav", games / "15" / "3" / "15.sav", games / "16" / "3" / "16.sav",
	                        games / "17" / "3" / "17.sav"};
	if (!GameData() || !FrozenSave(saves.front()).has_value())
	{
		GTEST_SKIP() << "the frozen Land 3 saves are the game's data";
	}
	for (const auto& path : saves)
	{
		const auto bytes = FrozenSave(path);
		ASSERT_TRUE(bytes.has_value()) << path.string();
		EXPECT_EQ(load::PlanOf(gamesave::ReadWorld(*bytes)).wonders.size(), 3u) << path.string();
	}
	const auto bytes = FrozenSave(saves.front());
	const auto world = gamesave::ReadWorld(*bytes);
	const auto plan = load::PlanOf(world);
	// the models of the rows the save's buildings, piles and scaffolds are of: the load makes them all, the footpath links
	// and the building sites the wonders name with them
	for (const auto& object : plan.objects)
	{
		if (const auto* abode = std::get_if<load::PlannedAbode>(&object); abode != nullptr)
		{
			LoadAbodeMesh(abode->type);
		}
		else if (const auto* pit = std::get_if<load::PlannedStoragePit>(&object); pit != nullptr)
		{
			LoadAbodeMesh(pit->type);
		}
		else if (const auto* pile = std::get_if<load::PlannedPile>(&object); pile != nullptr)
		{
			LoadPileMesh(pile->type);
		}
	}
	for (const auto& wonder : plan.wonders)
	{
		LoadAbodeMesh(wonder.type);
	}
	LoadMesh(Info().scaffold.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	LoadMesh(Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 1.0f, 1.0f));
	auto makers = ScaffoldMakers();
	makers.wonder = load::MakeWonderEntity;
	LoadedObjects loaded;
	const auto census = load::Apply(plan, makers, &loaded);
	const auto line = std::ranges::find(census, WonderRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 3u);
	ListAsSaved(world, loaded);
	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	for (const auto& wonder : plan.wonders)
	{
		ASSERT_TRUE(loaded.contains(wonder.number)) << wonder.number;
		const auto entity = loaded.at(wonder.number).entity;
		EXPECT_EQ(ecs::wonders::GetPower(entity), std::bit_cast<float>(wonder.record.word)) << wonder.number;
		EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {WonderRecord::k_Type}) << wonder.number;
		const auto written = (*_writers.WriterOf(WonderRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		// field by field, so that a failure names the field
		const auto& back = std::get<WonderRecord>(*written);
		const auto& read = wonder.record;
		EXPECT_EQ(back.thing, read.thing) << wonder.number;
		EXPECT_EQ(back.base.position.position, read.base.position.position) << wonder.number;
		EXPECT_EQ(back.base.position.flags, read.base.position.flags) << wonder.number;
		EXPECT_EQ(back.base.object.info, read.base.object.info) << wonder.number;
		EXPECT_EQ(back.base.object.coords, read.base.object.coords) << wonder.number;
		EXPECT_EQ(back.base.object.creationIndex, read.base.object.creationIndex) << wonder.number;
		EXPECT_EQ(back.base.object.life, read.base.object.life) << wonder.number;
		EXPECT_EQ(back.base.object.scale, read.base.object.scale) << wonder.number;
		EXPECT_EQ(back.base.object.heading, read.base.object.heading) << wonder.number;
		EXPECT_EQ(back.base.object.fire, read.base.object.fire) << wonder.number;
		EXPECT_EQ(back.base.object.physics.has_value(), read.base.object.physics.has_value()) << wonder.number;
		EXPECT_EQ(back.base.fixed, read.base.fixed) << wonder.number;
		EXPECT_EQ(back.base.multi.fixedFlags, read.base.multi.fixedFlags) << wonder.number;
		EXPECT_EQ(back.base.multi.percentBuilt, read.base.multi.percentBuilt) << wonder.number;
		EXPECT_EQ(back.base.multi.footpathLink, read.base.multi.footpathLink) << wonder.number;
		EXPECT_EQ(back.base.multi.buildingSite, read.base.multi.buildingSite) << wonder.number;
		EXPECT_EQ(back.abode.abodeFlags, read.abode.abodeFlags) << wonder.number;
		EXPECT_EQ(back.abode.drinkingWater, read.abode.drinkingWater) << wonder.number;
		EXPECT_EQ(back.abode.unusedWord, read.abode.unusedWord) << wonder.number;
		EXPECT_EQ(back.abode.town.number, read.abode.town.number) << wonder.number;
		EXPECT_EQ(back.abode.villagers.numbers, read.abode.villagers.numbers) << wonder.number;
		EXPECT_EQ(back.abode.adults, read.abode.adults) << wonder.number;
		EXPECT_EQ(back.abode.presentAtHome, read.abode.presentAtHome) << wonder.number;
		EXPECT_EQ(back.abode.children, read.abode.children) << wonder.number;
		EXPECT_EQ(back.abode.index, read.abode.index) << wonder.number;
		EXPECT_EQ(back.abode.resources.values, read.abode.resources.values) << wonder.number;
		EXPECT_EQ(back.abode.destructionMesh, read.abode.destructionMesh) << wonder.number;
		EXPECT_EQ(back.word, read.word) << wonder.number;
	}
}

// A pile of magic food of the save is made as a food pile of its row with its amount and the player its record names,
// its record kept and not a food pile's; the census counts it made, and the pots' writer writes it back as it was read
// but for its sink, which the load starts again buried at the depth of its height, rising to its amount's height in 1 s,
// and so its place's height
TEST_P(AbodeWriteWorld, ASavedMagicFoodIsMadeWithItsPlayerAndWrittenBackAsRead)
{
	RegisterPotWriters(_writers);
	if (!GameData())
	{
		auto& row = Info().pot.at(static_cast<size_t>(PotInfo::MagicFood));
		row.type = ObjectType::Pot;
		row.potType = PotType::PileFood;
		row.resourceType = ResourceType::Food;
		row.maxAmountInPot = 1000;
		row.meshId = MeshId::ObjectRockChalk;
	}
	LoadPileMesh(PotInfo::MagicFood);
	constexpr ObjectNumber k_Player = 2;
	constexpr ObjectNumber k_Food = 3;
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.players.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = k_Player;
	world.objects.emplace_back(game);
	world.objects.emplace_back(gamesave::PlayerRecord {.playerNumber = static_cast<uint8_t>(PlayerNames::PLAYER_TWO)});
	auto food = PileAt<MagicFoodRecord>(At(205, 200), PotInfo::MagicFood, 41, 0, 0, 300);
	food.pile.resourceType = 0;
	food.pile.altitudeZoomer = {};
	food.pile.player.number = k_Player;
	food.player.number = k_Player;
	world.objects.emplace_back(food);
	world.loadOrder = {3, 2, 1};
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), {.magicFood = load::MakeMagicFoodEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(k_Food));
	EXPECT_EQ(loaded.at(k_Food).type, MagicFoodRecord::k_Type);
	const auto entity = loaded.at(k_Food).entity;
	const auto* pot = Registry().TryGet<const ecs::components::Pot>(entity);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::MagicFood);
	EXPECT_EQ(pot->amount, 300u);
	EXPECT_TRUE(pot->owner == PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(Registry().AllOf<ecs::components::LoadedRecord<MagicFoodRecord>>(entity));
	EXPECT_FALSE(Registry().AllOf<ecs::components::LoadedRecord<FoodPileRecord>>(entity));
	const auto line = std::ranges::find(census, MagicFoodRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->saved, 1u);
	EXPECT_EQ(line->made, 1u);

	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {MagicFoodRecord::k_Type});
	const auto written = (*_writers.WriterOf(MagicFoodRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	food.base.position.flags = PositionFlagsOf(Registry(), entity, food.base.position.flags);
	const float height = ecs::object::GetHeight(entity);
	EXPECT_GT(height, 0.0f);
	Zoomer rising;
	rising.SetPosition(-height);
	rising.SetDestinationWithSpeedAndTime((ecs::object::GetProportionRaised(entity) - 1.0f) * height, 0.0f, 1.0f);
	food.pile.altitudeZoomer = ecs::saved_zoomer::BytesOf(rising);
	// and its place's height the height of its amount, where its sink goes
	food.base.position.position.altitude = rising.destination;
	const auto& back = std::get<MagicFoodRecord>(*written);
	EXPECT_EQ(back.pile.altitudeZoomer, food.pile.altitudeZoomer);
	EXPECT_EQ(back.base.position.position.altitude, food.base.position.position.altitude);
	EXPECT_TRUE(back == food);
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 2 saves hold piles of magic food; those the load plans are made, each with its amount and player,
// and the pots' writer writes each one's pile, sparkle and player as read
TEST_P(AbodeWriteWorld, TheFrozenLandTwoSavesMagicFoodIsMadeAndWrittenAsRead)
{
	RegisterPotWriters(_writers);
	const std::filesystem::path games = std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games";
	const std::array saves {games / "15" / "2" / "15.sav", games / "16" / "2" / "16.sav", games / "20" / "20.sav"};
	if (!GameData() || !FrozenSave(saves.front()).has_value())
	{
		GTEST_SKIP() << "the frozen Land 2 saves are the game's data";
	}
	size_t planned = 0;
	for (const auto& path : saves)
	{
		const auto bytes = FrozenSave(path);
		ASSERT_TRUE(bytes.has_value()) << path.string();
		planned += load::PlanOf(gamesave::ReadWorld(*bytes)).magicFoods.size();
	}
	EXPECT_GT(planned, 0u);
	const auto bytes = FrozenSave(saves.back());
	const auto world = gamesave::ReadWorld(*bytes);
	const auto plan = load::PlanOf(world);
	ASSERT_FALSE(plan.magicFoods.empty());
	for (const auto& food : plan.magicFoods)
	{
		LoadPileMesh(food.type);
	}
	LoadedObjects loaded;
	const auto census = load::Apply(
	    plan,
	    {.town = load::MakeTownEntity, .tree = KeepTree, .bigForest = KeepBigForest, .magicFood = load::MakeMagicFoodEntity},
	    &loaded);
	const auto line = std::ranges::find(census, MagicFoodRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, plan.magicFoods.size());
	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	for (const auto& food : plan.magicFoods)
	{
		ASSERT_TRUE(loaded.contains(food.number)) << food.number;
		const auto entity = loaded.at(food.number).entity;
		EXPECT_EQ(Registry().Get<const ecs::components::Pot>(entity).amount, food.record.pile.resourceAmount) << food.number;
		EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {MagicFoodRecord::k_Type}) << food.number;
		const auto written = (*_writers.WriterOf(MagicFoodRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		const auto& record = std::get<MagicFoodRecord>(*written);
		EXPECT_TRUE(record.pile == food.record.pile) << food.number;
		EXPECT_EQ(record.store.number, food.record.store.number) << food.number;
		EXPECT_EQ(record.player.number, food.record.player.number) << food.number;
		EXPECT_EQ(record.base.object.info, food.record.base.object.info) << food.number;
		EXPECT_EQ(record.base.object.creationIndex, food.record.base.object.creationIndex) << food.number;
	}
}

// Integration test: needs the original game's data (OPENBLACK_GAME_PATH and OPENBLACK_TEST_GAME_PATH); skipped without
// it. The frozen Land 1 autosave with its towns, abodes, creches, graveyards, storage pits and piles made as the game's
// load makes them, and every other object keeping its record: each record of those classes is written as read, and the
// whole file, those classes by these writers, is the original's byte for byte
TEST_P(AbodeWriteWorld, TheFrozenLandOneAutosavesAbodesAreWrittenAsReadAndTheFileAsItWas)
{
	const auto bytes = FrozenLandOneAutosave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto plan = load::PlanOf(world);
	// the models of the rows the save's buildings and piles are of
	for (const auto& object : plan.objects)
	{
		if (const auto* abode = std::get_if<load::PlannedAbode>(&object); abode != nullptr)
		{
			LoadAbodeMesh(abode->type);
		}
		else if (const auto* pit = std::get_if<load::PlannedStoragePit>(&object); pit != nullptr)
		{
			LoadAbodeMesh(pit->type);
		}
		else if (const auto* pile = std::get_if<load::PlannedPile>(&object); pile != nullptr)
		{
			LoadPileMesh(pile->type);
		}
	}
	LoadedObjects loaded;
	std::ignore = load::Apply(plan, AbodeMakers(), &loaded);
	ListAsSaved(world, loaded);

	const WorldSource source {.registry = Registry(), .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	const auto each = ExpectEachRecordAsRead(world, loaded, _writers, source, false);
	size_t ofTheClasses = 0;
	for (const auto& [type, count] : each)
	{
		ofTheClasses += count;
	}
	EXPECT_GT(each.count(AbodeRecord::k_Type), 0u);
	EXPECT_GT(each.count(StoragePitRecord::k_Type), 0u);

	const auto written = WriteWorld(_writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, *bytes);
	size_t byWriters = 0;
	for (const auto& record : written.records)
	{
		if (k_Classes.contains(record.type))
		{
			EXPECT_EQ(record.source, RecordSource::Writer) << record.number;
			++byWriters;
		}
	}
	EXPECT_EQ(byWriters, ofTheClasses);
}

INSTANTIATE_TEST_SUITE_P(Data, AbodeWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
