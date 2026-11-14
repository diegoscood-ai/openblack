/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pots and piles written from their entities: the class their row's making picks, the moving object's parts, the
// resource, the town each was made for, the amount and the flags, then a pile's structure, the player it answered with
// as it was made, whether it was made to start its reaction and its sink; a food pile's speed-up visual and a magic
// pile's player. A loaded pile keeps what openblack does not hold (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VanillaSaveFile.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/Zoomer.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::FoodPileRecord;
using gamesave::MagicFoodRecord;
using gamesave::MagicWoodRecord;
using gamesave::PotRecord;
using gamesave::SavedRecord;
using gamesave::WoodPileRecord;
namespace components = openblack::ecs::components;
namespace pot_flags = openblack::gamesave::pot_flags;

namespace
{
constexpr gamesave::StreamStart k_After100 {.objects = 100, .checksum = 0x27000};

/// Whether the record, written to a stream and read back, has the same values and writes the same bytes again
template <typename Record>
bool ReadsBackAsWritten(const Record& record)
{
	gamesave::StreamWriter writer(k_After100);
	gamesave::Write(writer, SavedRecord(record), nullptr);
	const auto bytes = writer.Take();
	gamesave::StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	SavedRecord read;
	if (!gamesave::ReadRecord(reader, type, extra, read, nullptr) || reader.Position() != bytes.size())
	{
		return false;
	}
	const auto* back = std::get_if<Record>(&read);
	if (back == nullptr || Record::FieldsOf(*back) != Record::FieldsOf(record))
	{
		return false;
	}
	gamesave::StreamWriter again(k_After100);
	gamesave::Write(again, read, nullptr);
	return again.Bytes() == bytes;
}

float FloatAt(const std::array<uint8_t, 48>& bytes, size_t offset)
{
	float value = 0.0f;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

class PotWrite: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<test::FakeFires>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		// the rows' kinds and resources as the game's data has them
		auto info = std::make_unique<InfoConstants>();
		const auto row = [&info](PotInfo type, PotType kind, ResourceType resource) {
			auto& pot = info->pot.at(static_cast<size_t>(type));
			pot.potType = kind;
			pot.resourceType = resource;
		};
		row(PotInfo::FoodPot, PotType::Pot, ResourceType::Food);
		row(PotInfo::WoodPile_1, PotType::PileWood, ResourceType::Wood);
		row(PotInfo::MagicWood, PotType::PileWood, ResourceType::Wood);
		row(PotInfo::MagicFood, PotType::PileFood, ResourceType::Food);
		row(PotInfo::HandFood, PotType::PileFood, ResourceType::Food);
		Locator::infoConstants::reset(info.release());
	}

	ecs::Registry& Reg() { return *_registry; }

	/// An entity standing for a loaded object of the class
	template <typename Record>
	entt::entity Loaded()
	{
		const auto entity = Reg().Create();
		Reg().Assign<components::LoadedRecord<Record>>(entity, Record {});
		return entity;
	}

	/// The player of the name, an entity of the players' class
	entt::entity Player(PlayerNames name)
	{
		const auto player = Reg().Create();
		Reg().Assign<components::Player>(player, name);
		return player;
	}

	/// A pot of the row holding `amount` at a place
	entt::entity Pot(PotInfo type, uint32_t amount)
	{
		const auto pot = Reg().Create();
		Reg().Assign<components::Transform>(pot, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<components::Pot>(pot, components::Pot {.amount = amount, .maxAmount = 1000, .type = type});
		return pot;
	}

	/// The record the writers make of the entity, and the context they made it in
	template <typename Record>
	std::optional<Record> Written(entt::entity entity, std::string* failure = nullptr)
	{
		_context.reset();
		_source.reset();
		_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
		_context.emplace(_writers, *_source);
		const auto made = _context->RecordOf(_context->Reference(entity));
		if (failure != nullptr)
		{
			*failure = _context->Failure();
		}
		if (made.record == nullptr)
		{
			return std::nullopt;
		}
		const auto* record = std::get_if<Record>(made.record.get());
		return record != nullptr ? std::optional(*record) : std::nullopt;
	}

	ClassWriters _writers = [] {
		ClassWriters writers;
		RegisterPotWriters(writers);
		return writers;
	}();
	/// What the context reads, kept as long as the context
	std::optional<WorldSource> _source;
	std::optional<WriteContext> _context;

private:
	// the map cells the place's flags ask, reset after the registry
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	// the fires an object's writer asks of its burning, reset after the registry
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// A field's handful made in play is a food pile: its object names its row of the pot table (1876 less the row), the
// food, the town it was made for, its amount and its flags (poisoned, its reaction started), no structure, the neutral
// player it answered with, its sink as the game keeps it (the place's height the sink's destination) and no speed-up
// visual; it reads back as written
TEST_F(PotWrite, AHandfulMadeInPlayIsWrittenAsAFoodPileWithItsTown)
{
	const auto neutral = Player(PlayerNames::NEUTRAL);
	const auto town = Loaded<gamesave::TownRecord>();
	const auto handful = Pot(PotInfo::HandFood, 25);
	auto& pot = Reg().Get<components::Pot>(handful);
	pot.town = town;
	pot.poisoned = true;
	pot.reactionStarted = true;
	auto& sink = Reg().Assign<components::PileSink>(handful, 5.0f);
	sink.offset.SetPosition(-2.0f);
	sink.offset.SetDestinationWithSpeedAndTime(-0.5f, 0.0f, 1.0f);

	const auto record = Written<FoodPileRecord>(handful);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 1864u);
	EXPECT_EQ(record->pile.resourceType, 0u);
	EXPECT_EQ(record->pile.parentTown.number, _context->Reference(town));
	EXPECT_EQ(record->pile.resourceAmount, 25u);
	EXPECT_EQ(record->pile.flags, pot_flags::k_Poisoned | pot_flags::k_ReactionStarted);
	EXPECT_EQ(record->pile.building.number, 0u);
	EXPECT_EQ(record->pile.player.number, _context->Reference(neutral, gamesave::k_PlayerType));
	EXPECT_EQ(record->pile.buildingByte, 0u);
	EXPECT_EQ(FloatAt(record->pile.altitudeZoomer, 0), -2.0f);
	EXPECT_EQ(FloatAt(record->pile.altitudeZoomer, 4), -0.5f);
	EXPECT_EQ(FloatAt(record->pile.altitudeZoomer, 0x18), 1.0f);
	EXPECT_EQ(record->base.position.position.altitude, -0.5f);
	EXPECT_EQ(record->store.number, 0u);
	EXPECT_EQ(record->base.mobileObject, gamesave::MobileObjectPart {});
	EXPECT_TRUE(ReadsBackAsWritten(*record));
}

// A store's wood pile made in play is a wood pile of its store, no town's, with the player its store answered with as
// it was made (not its store's now), and the marks of a building site's pile and of one made to start its reaction
TEST_F(PotWrite, AStoresPileIsWrittenWithItsStructure)
{
	static_cast<void>(Player(PlayerNames::NEUTRAL));
	const auto three = Player(PlayerNames::PLAYER_THREE);
	const auto pile = Pot(PotInfo::WoodPile_1, 70);
	auto& pot = Reg().Get<components::Pot>(pile);
	pot.playerWhenMade = PlayerNames::PLAYER_THREE;
	pot.fromBuildingSite = true;
	pot.reactsWhenMade = true;
	const auto pit = Loaded<gamesave::StoragePitRecord>();
	auto& store = Reg().Assign<components::StoragePit>(pit);
	store.woodPiles.fill(entt::null);
	store.foodPile = entt::null;
	store.woodPiles.at(0) = pile;

	const auto record = Written<WoodPileRecord>(pile);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 1873u);
	EXPECT_EQ(record->pile.resourceType, 1u);
	EXPECT_EQ(record->pile.parentTown.number, 0u);
	EXPECT_EQ(record->pile.flags, pot_flags::k_FromBuildingSite);
	EXPECT_EQ(record->pile.building.number, _context->Reference(pit));
	EXPECT_EQ(record->pile.player.number, _context->Reference(three, gamesave::k_PlayerType));
	EXPECT_EQ(record->pile.buildingByte, 1u);
	EXPECT_TRUE(ReadsBackAsWritten(*record));
}

// A magic pile is of its own class, with its maker as its player twice; a plain pot writes the pot's values only
TEST_F(PotWrite, MagicPilesAndAPlainPotAreOfTheirOwnClasses)
{
	const auto two = Player(PlayerNames::PLAYER_TWO);
	const auto food = Pot(PotInfo::MagicFood, 300);
	Reg().Get<components::Pot>(food).owner = PlayerNames::PLAYER_TWO;
	Reg().Get<components::Pot>(food).playerWhenMade = PlayerNames::PLAYER_TWO;
	const auto magicFood = Written<MagicFoodRecord>(food);
	ASSERT_TRUE(magicFood.has_value()) << _context->Failure();
	EXPECT_EQ(magicFood->base.object.info, 1866u);
	EXPECT_EQ(magicFood->player.number, _context->Reference(two, gamesave::k_PlayerType));
	EXPECT_EQ(magicFood->pile.player.number, magicFood->player.number);
	EXPECT_TRUE(ReadsBackAsWritten(*magicFood));

	const auto wood = Pot(PotInfo::MagicWood, 40);
	Reg().Get<components::Pot>(wood).owner = PlayerNames::PLAYER_TWO;
	const auto magicWood = Written<MagicWoodRecord>(wood);
	ASSERT_TRUE(magicWood.has_value()) << _context->Failure();
	EXPECT_EQ(magicWood->base.object.info, 1867u);
	EXPECT_EQ(magicWood->pile.resourceType, 1u);
	EXPECT_EQ(magicWood->player.number, _context->Reference(two, gamesave::k_PlayerType));
	EXPECT_TRUE(ReadsBackAsWritten(*magicWood));

	const auto town = Loaded<gamesave::TownRecord>();
	const auto plain = Pot(PotInfo::FoodPot, 12);
	auto& pot = Reg().Get<components::Pot>(plain);
	pot.town = town;
	pot.speedUp = true;
	const auto record = Written<PotRecord>(plain);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 1876u);
	EXPECT_EQ(record->resourceType, 0u);
	EXPECT_EQ(record->parentTown.number, _context->Reference(town));
	EXPECT_EQ(record->resourceAmount, 12u);
	EXPECT_EQ(record->flags, pot_flags::k_SpeedUp);
	EXPECT_TRUE(ReadsBackAsWritten(*record));
}

// A loaded pile is of its record's class whatever its row, writes what openblack holds (its amount, the flags it holds
// and the player it answers with, as its load set it) and keeps what it does not: the flags' other bits; with no
// sparkle running it names none
TEST_F(PotWrite, ALoadedPileKeepsWhatOpenblackDoesNotHold)
{
	const auto three = Player(PlayerNames::PLAYER_THREE);
	const auto pile = Pot(PotInfo::MagicFood, 40);
	Reg().Get<components::Pot>(pile).speedUp = true;
	Reg().Get<components::Pot>(pile).playerWhenMade = PlayerNames::PLAYER_THREE;
	FoodPileRecord kept;
	kept.pile.resourceAmount = 900;
	kept.pile.flags = pot_flags::k_Poisoned | 0x04;
	kept.pile.player.number = 7;
	kept.store.number = 9;
	Reg().Assign<components::LoadedRecord<FoodPileRecord>>(pile, kept);

	const auto record = Written<FoodPileRecord>(pile);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->pile.resourceAmount, 40u);
	EXPECT_EQ(record->pile.flags, 0x04u | pot_flags::k_SpeedUp);
	EXPECT_EQ(record->pile.player.number, _context->Reference(three, gamesave::k_PlayerType));
	EXPECT_EQ(record->store.number, 0u);
}

// A pot of no kind is of no class: naming it stops the write
TEST_F(PotWrite, APotOfNoKindStopsTheWrite)
{
	const auto pot = Pot(PotInfo::_COUNT, 5);
	std::string failure;
	EXPECT_FALSE(Written<PotRecord>(pot, &failure).has_value());
	EXPECT_NE(failure.find("no class"), std::string::npos) << failure;
}

// The game's lists of food piles and of mobile objects hold the pots made since the load, the newest first, then the
// lists the load kept, less what has gone: the food piles' list only the food piles and the magic food
// A moving object made in play is written as a plain moving object of its row (1970 less the row) at its place, with
// its x and z angles; an orb as a one-off spell's orb with its power-up, seed and multiplier; a poo as a poo of the
// poo's row, its moving object's parts alone; and the game's list of moving objects holds them with the pots, the newest
// first
TEST_F(PotWrite, AMovingObjectAndAnOrbMadeInPlayAreWrittenAndListed)
{
	const auto made = [this](uint32_t index) {
		const auto entity = Reg().Create();
		Reg().Assign<components::Transform>(entity, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::ObjectCreationIndex>(entity, index);
		return entity;
	};
	const auto barrel = made(4);
	Reg().Assign<components::MobileObject>(barrel, MobileObjectInfo::EgyptBarrel);
	const auto poo = made(5);
	Reg().Assign<components::MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	const auto orb = made(6);
	Reg().Assign<components::OneOffSpellSeed>(
	    orb, components::OneOffSpellSeed {.seedType = static_cast<SpellSeedType>(3), .scale = 1.5f, .powerUp = 2});
	const auto pot = Pot(PotInfo::FoodPot, 10);
	Reg().Assign<ecs::components::ObjectCreationIndex>(pot, uint32_t {2});
	EXPECT_EQ(_writers.ClaimsOf(Reg(), barrel), std::vector<uint32_t> {gamesave::MobileObjectRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(Reg(), poo), std::vector<uint32_t> {gamesave::PooRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(Reg(), orb), std::vector<uint32_t> {gamesave::OneOffSpellSeedRecord::k_Type});
	const auto record = Written<gamesave::MobileObjectRecord>(barrel);
	ASSERT_TRUE(record.has_value());
	EXPECT_EQ(record->base.object.info, 1970u);
	EXPECT_EQ(record->base.object.creationIndex, 4u);
	const auto pooWritten = Written<gamesave::PooRecord>(poo);
	ASSERT_TRUE(pooWritten.has_value());
	EXPECT_EQ(pooWritten->base.object.info, 1965u);
	EXPECT_EQ(pooWritten->base.object.creationIndex, 5u);
	const auto written = Written<gamesave::OneOffSpellSeedRecord>(orb);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(written->base.object.info, 1945u);
	EXPECT_EQ(written->words, (std::array<uint32_t, 3> {2, 3, std::bit_cast<uint32_t>(1.5f)}));
	const WorldSource source {.registry = Reg(), .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto list = (*_writers.ListWriterOf(GlobalList::MobileObjects))(context);
	EXPECT_EQ(list, (std::vector<gamesave::ObjectNumber> {context.Reference(orb), context.Reference(poo),
	                                                      context.Reference(barrel), context.Reference(pot)}));
}

// A scaffold made in play is of its own class, of the scaffold's row, carrying its workshop: then its site, its plan and
// its candidate (each its own planned abode, written as the original writes the two a new scaffold makes: at the
// scaffold's place, of no row, angle 0, scale 1, the turn it was made, never built, on no footpath, in no town), the
// cross-fade's timer, its holder, the phantom's angle and scale, its flags, its town and the building it may offer; it
// goes in the game's list of moving objects with the others, newest first. A plan deleted while still named is refused
TEST_F(PotWrite, AScaffoldMadeInPlayIsWrittenWithItsTwoPlansAndListed)
{
	RegisterTownWriters(_writers);
	const auto two = Player(PlayerNames::PLAYER_TWO);
	const auto town = Loaded<gamesave::TownRecord>();
	const auto workshop = Loaded<gamesave::WorkshopRecord>();
	const auto site = Loaded<gamesave::StandardBuildingSiteRecord>();
	const auto entity = Reg().Create();
	Reg().Assign<components::Transform>(entity, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(0.7f));
	Reg().Assign<ecs::components::ObjectCreationIndex>(entity, uint32_t {4391});
	Reg().Assign<components::MobileObject>(entity, MobileObjectInfo::OldScaffold);
	const map_coords::MapCoords at {.x = 17371936, .z = 12718964, .altitude = 0.0f};
	auto& scaffold = Reg().Assign<components::Scaffold>(entity);
	scaffold.owner = workshop;
	scaffold.site = site;
	scaffold.plan = components::ScaffoldPlan {.position = at, .creationTurn = 17485};
	scaffold.candidate = components::ScaffoldPlan {.position = at, .yAngle = 0.5f, .scale = 1.5f, .creationTurn = 17486};
	scaffold.fade = 300;
	scaffold.holder = PlayerNames::PLAYER_TWO;
	scaffold.phantomAngle = 0.78f;
	scaffold.phantomScale = 0.7f;
	scaffold.flags = 0x77;
	scaffold.town = town;
	EXPECT_EQ(_writers.ClaimsOf(Reg(), entity), std::vector<uint32_t> {gamesave::ScaffoldRecord::k_Type});
	const auto record = Written<gamesave::ScaffoldRecord>(entity);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 1938u);
	EXPECT_EQ(record->base.object.creationIndex, 4391u);
	EXPECT_EQ(record->base.mobileObject.object, _context->Reference(workshop));
	EXPECT_EQ(record->objects[0].number, _context->Reference(site));
	const auto plan = _context->Reference(SourceKey {.type = gamesave::PlannedAbodeRecord::k_Type, .id = PartId(entity, 0)});
	const auto candidate =
	    _context->Reference(SourceKey {.type = gamesave::PlannedAbodeRecord::k_Type, .id = PartId(entity, 1)});
	EXPECT_EQ(record->objects[1].number, plan);
	EXPECT_EQ(record->objects[2].number, candidate);
	EXPECT_NE(plan, candidate);
	EXPECT_EQ(record->firstWord, 300u);
	EXPECT_EQ(record->other.number, _context->Reference(two, gamesave::k_PlayerType));
	EXPECT_EQ(record->secondWord, std::bit_cast<uint32_t>(0.78f));
	EXPECT_EQ(record->thirdWord, std::bit_cast<uint32_t>(0.7f));
	EXPECT_EQ(record->half, 0x77);
	EXPECT_EQ(record->last.number, _context->Reference(town));
	EXPECT_EQ(record->lastWord, 0xFFFFFFFFu);
	EXPECT_TRUE(ReadsBackAsWritten(*record));
	const auto planRecord = _context->RecordOf(plan);
	ASSERT_NE(planRecord.record, nullptr) << _context->Failure();
	const auto& planned = std::get<gamesave::PlannedAbodeRecord>(*planRecord.record);
	EXPECT_EQ(planned.planned.position.position, (gamesave::MapCoords {.x = 17371936, .z = 12718964, .altitude = 0.0f}));
	EXPECT_EQ(planned.planned.position.flags, 0u);
	EXPECT_EQ(planned.planned.yAngle, 0.0f);
	EXPECT_EQ(planned.planned.scale, 1.0f);
	EXPECT_EQ(planned.planned.wasConstructed, 0u);
	EXPECT_EQ(planned.planned.creationTurn, 17485);
	EXPECT_EQ(planned.planned.footpathLink.number, 0u);
	EXPECT_EQ(planned.planned.info, 0xFFFFFFFFu);
	EXPECT_EQ(planned.town.number, 0u);
	const auto candidateRecord = _context->RecordOf(candidate);
	ASSERT_NE(candidateRecord.record, nullptr) << _context->Failure();
	const auto& second = std::get<gamesave::PlannedAbodeRecord>(*candidateRecord.record);
	EXPECT_EQ(second.planned.yAngle, 0.5f);
	EXPECT_EQ(second.planned.scale, 1.5f);
	EXPECT_EQ(second.planned.creationTurn, 17486);
	EXPECT_TRUE(_context->Failure().empty()) << _context->Failure();

	const auto barrel = Reg().Create();
	Reg().Assign<components::Transform>(barrel, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::ObjectCreationIndex>(barrel, uint32_t {4000});
	Reg().Assign<components::MobileObject>(barrel, MobileObjectInfo::EgyptBarrel);
	const WorldSource source {.registry = Reg(), .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	EXPECT_EQ((*_writers.ListWriterOf(GlobalList::MobileObjects))(context),
	          (std::vector<gamesave::ObjectNumber> {context.Reference(entity), context.Reference(barrel)}));

	scaffold.plan->available = false;
	std::string failure;
	std::ignore = Written<gamesave::ScaffoldRecord>(entity, &failure);
	EXPECT_FALSE(failure.empty());
}

TEST_F(PotWrite, TheGamesListsOfPotsAreTheNewOnesNewestFirstThenTheKeptOnes)
{
	const auto pot = [this](PotInfo type, uint32_t index) {
		const auto entity = Pot(type, 10);
		Reg().Assign<ecs::components::ObjectCreationIndex>(entity, index);
		return entity;
	};
	const auto older = pot(PotInfo::HandFood, 5);
	const auto wood = pot(PotInfo::WoodPile_1, 7);
	const auto newer = pot(PotInfo::MagicFood, 9);
	const auto plain = pot(PotInfo::FoodPot, 3);
	// loaded: a pile the load made (2), one gone since (3), and a pot no entity stands for (4)
	const auto loaded = pot(PotInfo::HandFood, 1);
	Reg().Assign<components::LoadedRecord<FoodPileRecord>>(loaded, FoodPileRecord {});
	const auto gone = Reg().Create();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming("Land1.lnd"));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::FoodPiles)).numbers = {2, 3, 4};
	game.lists.at(static_cast<size_t>(GlobalList::MobileObjects)).numbers = {4, 2};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(FoodPileRecord {});
	kept.objects.emplace_back(FoodPileRecord {});
	kept.objects.emplace_back(MagicFoodRecord {});
	Reg().Destroy(gone);
	const WorldSource source {.registry = Reg(),
	                          .landscape = "Land1.lnd",
	                          .kept = &kept,
	                          .loaded = {{2, LoadedObject {.entity = loaded, .type = FoodPileRecord::k_Type}},
	                                     {3, LoadedObject {.entity = gone, .type = FoodPileRecord::k_Type}}}};
	WriteContext context(_writers, source);
	const auto* food = _writers.ListWriterOf(GlobalList::FoodPiles);
	const auto* mobile = _writers.ListWriterOf(GlobalList::MobileObjects);
	ASSERT_NE(food, nullptr);
	ASSERT_NE(mobile, nullptr);
	const auto foodPiles = (*food)(context);
	const auto mobileObjects = (*mobile)(context);
	EXPECT_EQ(foodPiles, (std::vector<gamesave::ObjectNumber> {context.Reference(newer), context.Reference(older), 2, 4}));
	EXPECT_EQ(mobileObjects, (std::vector<gamesave::ObjectNumber> {context.Reference(newer), context.Reference(wood),
	                                                               context.Reference(older), context.Reference(plain), 4, 2}));
}

// A reward chest made in play is of its own class, of its row of the reward table (313 less the row), with its player
// and its town (none here), then its words: no alignment, its opened mark, a word nothing reads, its state, landed,
// whether it goes into the map's cells at the next turn, its clock, no drawn frame (as a new chest's), its making's
// unread number and its dust time; it reads back as written and goes in the game's list of moving objects. Loaded, it
// keeps the words openblack does not hold, and written again from its record it gives the same record
TEST_F(PotWrite, ARewardChestIsWrittenWithItsWordsAndKeepsWhatOpenblackDoesNotHold)
{
	const auto two = Player(PlayerNames::PLAYER_TWO);
	const auto chest = Reg().Create();
	Reg().Assign<components::Transform>(chest, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::ObjectCreationIndex>(chest, uint32_t {2663});
	Reg().Assign<components::Reward>(chest, components::Reward {.type = RewardObjectInfo::RewardObjectToyBall,
	                                                            .player = PlayerNames::PLAYER_TWO,
	                                                            .state = components::Reward::State::Landed,
	                                                            .seconds = 17.28f});
	EXPECT_EQ(_writers.ClaimsOf(Reg(), chest), std::vector<uint32_t> {gamesave::RewardRecord::k_Type});
	const auto record = Written<gamesave::RewardRecord>(chest);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 255u);
	EXPECT_EQ(record->base.object.creationIndex, 2663u);
	EXPECT_EQ(record->objects[0].number, 0u);
	EXPECT_EQ(record->objects[1].number, _context->Reference(two, gamesave::k_PlayerType));
	EXPECT_EQ(record->objects[2].number, 0u);
	EXPECT_EQ(record->words, (std::array<uint32_t, 10> {0, 0, 0, 1, 1, 0, std::bit_cast<uint32_t>(17.28f), 0xFFFFFFFF, 0, 0}));
	EXPECT_TRUE(ReadsBackAsWritten(*record));
	const WorldSource source {.registry = Reg(), .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto list = (*_writers.ListWriterOf(GlobalList::MobileObjects))(context);
	EXPECT_EQ(list, (std::vector<gamesave::ObjectNumber> {context.Reference(chest)}));

	// loaded with the scripts' alignment, the unread words and a drawn frame, which it keeps
	auto kept = *record;
	kept.words[0] = std::bit_cast<uint32_t>(0.5f);
	kept.words[2] = 3;
	kept.words[7] = 75667;
	kept.words[8] = std::bit_cast<uint32_t>(2.0f);
	Reg().Assign<components::LoadedRecord<gamesave::RewardRecord>>(chest, kept);
	const auto again = Written<gamesave::RewardRecord>(chest);
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(again->words, kept.words);
	EXPECT_EQ(gamesave::RewardRecord::FieldsOf(*again), gamesave::RewardRecord::FieldsOf(kept));
}

// A chest falling from the sky is written at the place it was given at and its y angle as given, not where its model
// is drawn in its fall, not landed; an opened chest with its town's miracle above it is refused
TEST_F(PotWrite, AFallingChestIsWrittenWhereItWasGivenAndAMiracleAboveAChestIsRefused)
{
	const auto chest = Reg().Create();
	Reg().Assign<components::Transform>(chest, glm::vec3(100.0f, 150.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<components::Reward>(chest, components::Reward {.type = RewardObjectInfo::SmallFood,
	                                                            .state = components::Reward::State::Falling,
	                                                            .madeAt = glm::vec3(100.0f, 5.0f, 200.0f),
	                                                            .dustMilliseconds = 2500.0f});
	const auto record = Written<gamesave::RewardRecord>(chest);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.position.position.altitude, 5.0f);
	EXPECT_EQ(record->base.object.coords, record->base.position.position);
	EXPECT_EQ(record->objects[1].number, 0u);
	EXPECT_EQ(record->words[3], 0u);
	EXPECT_EQ(record->words[4], 0u);
	EXPECT_EQ(record->words[9], 2500u);

	auto& opened = Reg().Get<components::Reward>(chest);
	opened.state = components::Reward::State::Opened;
	opened.townSeed = Reg().Create();
	std::string failure;
	EXPECT_FALSE(Written<gamesave::RewardRecord>(chest, &failure).has_value());
	EXPECT_NE(failure.find("miracle"), std::string::npos) << failure;
}

// A football made in play is of its own class, of its row of the game's tables (2082, the one after the map shields'
// two), with its two points and the town a villager's pick-up gave it to, no pitch's game and no villagers, and its
// place and word as a new ball's; whether its town has taken it is not written. It reads back as written and goes in
// the game's list of moving objects. One of no town names none
TEST_F(PotWrite, AFootballIsWrittenWithItsPointsAndItsTown)
{
	const auto town = Loaded<gamesave::TownRecord>();
	const auto ball = Reg().Create();
	Reg().Assign<components::Transform>(ball, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::ObjectCreationIndex>(ball, uint32_t {2700});
	Reg().Assign<components::Ball>(ball, components::Ball {.point = glm::vec3(100.0f, 5.0f, 200.0f),
	                                                       .previousPoint = glm::vec3(99.0f, 5.5f, 201.0f),
	                                                       .owned = true,
	                                                       .town = town});
	EXPECT_EQ(_writers.ClaimsOf(Reg(), ball), std::vector<uint32_t> {gamesave::BallRecord::k_Type});
	const auto record = Written<gamesave::BallRecord>(ball);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 2082u);
	EXPECT_EQ(record->base.object.creationIndex, 2700u);
	EXPECT_EQ(record->point, (gamesave::Point {100.0f, 5.0f, 200.0f}));
	EXPECT_EQ(record->previousPoint, (gamesave::Point {99.0f, 5.5f, 201.0f}));
	EXPECT_EQ(record->pitch.number, 0u);
	EXPECT_EQ(record->villager.number, 0u);
	EXPECT_EQ(record->lastVillager.number, 0u);
	EXPECT_EQ(record->town.number, _context->Reference(town));
	EXPECT_TRUE(record->place == gamesave::MapCoords {});
	EXPECT_EQ(record->word, 0u);
	EXPECT_TRUE(ReadsBackAsWritten(*record));
	const WorldSource source {.registry = Reg(), .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto list = (*_writers.ListWriterOf(GlobalList::MobileObjects))(context);
	EXPECT_EQ(list, (std::vector<gamesave::ObjectNumber> {context.Reference(ball)}));

	auto& untaken = Reg().Get<components::Ball>(ball);
	untaken.owned = false;
	untaken.town = entt::null;
	const auto none = Written<gamesave::BallRecord>(ball);
	ASSERT_TRUE(none.has_value()) << _context->Failure();
	EXPECT_EQ(none->town.number, 0u);
}

// A football of the save is made again at its place with its two points and the town its record names, as the load
// made that town, and free to be picked up, as the game saves no word of its town having taken it; with its creation
// number. Written back, it gives the record it was read from: its points, its town and the place and word openblack does
// not hold
TEST_F(PotWrite, ASavedFootballIsMadeAgainWithItsTownAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	const test::RestoreService<Locator::objectCreationIndexSystem> restoreIndex;
	Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	constexpr gamesave::ObjectNumber k_Town = 2;
	constexpr gamesave::ObjectNumber k_Ball = 3;
	gamesave::BallRecord saved;
	saved.base.position.position = {.x = map_coords::ToFixed(100.0f), .z = map_coords::ToFixed(200.0f), .altitude = 0.0f};
	saved.base.object.coords = saved.base.position.position;
	saved.base.object.info = 2082;
	saved.base.object.creationIndex = 2700;
	saved.base.object.scale = 1.0f;
	saved.base.object.life = 1.0f;
	saved.point = {100.0f, 3.0f, 200.0f};
	saved.previousPoint = {99.0f, 3.5f, 201.0f};
	saved.town = {k_Town};
	saved.place = {.x = 7, .z = 8, .altitude = 9.0f};
	saved.word = 10;
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.lists[static_cast<size_t>(GlobalList::Balls)].numbers = {k_Ball};
	world.objects.emplace_back(game);
	world.objects.emplace_back(gamesave::TownRecord {});
	world.objects.emplace_back(saved);
	world.loadOrder = {k_Town, k_Ball};
	// a town the load made, with the record it was read from, so a write can name it
	const auto town = Loaded<gamesave::TownRecord>();
	LoadedObjects loaded;
	std::ignore = load::Apply(
	    load::PlanOf(world),
	    load::Makers {.town = [town](const load::PlannedTown& /*planned*/) { return town; }, .ball = load::MakeBallEntity},
	    &loaded);
	ASSERT_TRUE(loaded.contains(k_Ball));
	const auto entity = loaded.at(k_Ball).entity;
	ASSERT_TRUE(entity != entt::null);
	const auto& ball = Reg().Get<const components::Ball>(entity);
	EXPECT_TRUE(ball.town == town);
	EXPECT_FALSE(ball.owned);
	EXPECT_EQ(ball.point, glm::vec3(100.0f, 3.0f, 200.0f));
	EXPECT_EQ(ball.previousPoint, glm::vec3(99.0f, 3.5f, 201.0f));
	EXPECT_EQ(ecs::object_index::Of(entity), 2700);

	const WorldSource source {.registry = Reg(), .landscape = "Land1.lnd", .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::BallRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::BallRecord>(*written);
	EXPECT_EQ(record.base.object.info, 2082u);
	EXPECT_EQ(record.base.object.creationIndex, 2700u);
	EXPECT_EQ(record.point, saved.point);
	EXPECT_EQ(record.previousPoint, saved.previousPoint);
	EXPECT_NE(record.town.number, 0u);
	EXPECT_EQ(record.town.number, context.Reference(town));
	EXPECT_TRUE(record.place == saved.place);
	EXPECT_EQ(record.word, saved.word);
}
