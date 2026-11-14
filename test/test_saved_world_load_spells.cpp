/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water spell made again by a load: every spell's values read back as the writer wrote them, the spell put back
// in the game's list in its order with its class's own and its seed's link, written back as it was read (a place whose
// map coordinates do not come back from metres the same included), its reaction given once the reactions are made, and
// the frozen Land 2 save's water spell planned with its effect, its creature and the reaction it started
// (docs/wiki/engine/saved-games.md#openblack, the spells)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <EffectRecords.h>
#include <ForestRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadSpells.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellCreature.h"
#include "Magic/Spells/SpellFlock.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellShield.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Magic/Spells/SpellWater.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SpellWaterRecord;
namespace components = openblack::ecs::components;
namespace saved_spells = openblack::ecs::saved_spells;

namespace
{
class SpellLoad: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
		Locator::fireSystem::emplace<test::FakeFires>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		// a physical shield made again goes into the map's cells by its mesh's shape: none here
		auto& shapes =
		    static_cast<test::FakeMapShapeProvider&>(Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>());
		shapes.meshShape = [](entt::entity /*object*/, ecs::map_collide::Shape& /*shape*/, float& /*reach*/) { return false; };
	}

	ecs::Registry& Reg() { return *_registry; }

	/// A water spell cast by a player from the hand, with no effect
	entt::entity Water(float age)
	{
		const auto spell = Reg().Create();
		auto& made = Reg().Assign<components::Spell>(spell);
		made.magicType = MagicType::Water;
		made.spellClass = components::SpellClass::Water;
		made.position = glm::vec3(100.0f, 2.0f, 200.0f);
		made.castPos = glm::vec3(101.0f, 0.0f, 201.0f);
		made.originalCastPos = glm::vec3(99.0f, 0.0f, 199.0f);
		made.movementDirection = glm::vec3(0.0f, 0.0f, 1.0f);
		made.direction = glm::vec3(0.5f, 0.0f, 0.5f);
		made.chants = 300.0f;
		made.initialChants = 400.0f;
		made.closedDown = true;
		made.isMyInterfaceCasting = true;
		made.isHumanPlayerCasting = true;
		made.duration = 12.0f;
		made.free = true;
		made.manaPathPerTurn = 3.0f;
		made.manaPathPerEvent = 4.0f;
		made.creator = {.kind = components::SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_TWO};
		made.player = PlayerNames::PLAYER_TWO;
		made.hasPlayer = true;
		made.castFromInterface = true;
		made.processInfo.power = 0.75f;
		made.processInfo.curl = -1.5f;
		made.age = age;
		made.magnitude = 6.0f;
		made.strengthMultiplier = 2.0f;
		Reg().Assign<magic::water::SpellWaterData>(spell, magic::water::SpellWaterData {.lastRipple = 1.4f});
		return spell;
	}

	/// The record the writer makes of the entity
	template <typename Record = SpellWaterRecord>
	std::optional<Record> Written(entt::entity entity)
	{
		_context.reset();
		_source.reset();
		_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
		_context.emplace(_writers, *_source);
		const auto made = _context->RecordOf(_context->Reference(entity));
		const auto* record = made.record != nullptr ? std::get_if<Record>(made.record.get()) : nullptr;
		return record != nullptr ? std::optional(*record) : std::nullopt;
	}

	ClassWriters _writers = [] {
		ClassWriters writers;
		RegisterSpellWriters(writers);
		return writers;
	}();
	std::optional<WorldSource> _source;
	std::optional<WriteContext> _context;

private:
	// put back in reverse order: the tables, the miracles' service, the animals' shared state, the fires and the map
	// cells the writers ask, then the registry
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	test::RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::mapShapeProvider> _restoreShapes;
	ecs::Registry* _registry {nullptr};
};

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

// Every spell's values are read back from the record as the writer wrote them from the component
TEST_F(SpellLoad, ASpellsValuesAreReadBackAsTheWriterWroteThem)
{
	const auto spell = Water(1.5f);
	const auto record = Written(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	const auto& original = Reg().Get<const components::Spell>(spell);
	const auto read = saved_spells::SpellOf(record->spell, components::SpellCreator::Kind::Player, PlayerNames::PLAYER_TWO,
	                                        entt::null, entt::null);
	EXPECT_EQ(read.magicType, original.magicType);
	EXPECT_EQ(read.position, original.position);
	EXPECT_EQ(read.movementDirection, original.movementDirection);
	// the places are written as map coordinates, whole steps of a tenth of a 65536th of a metre: read back as those
	const auto onMap = [](const glm::vec3& point) {
		return glm::vec3(map_coords::ToMetres(map_coords::ToFixed(point.x)), point.y,
		                 map_coords::ToMetres(map_coords::ToFixed(point.z)));
	};
	EXPECT_EQ(read.castPos, onMap(original.castPos));
	EXPECT_EQ(read.direction, original.direction);
	EXPECT_EQ(read.chants, original.chants);
	EXPECT_EQ(read.initialChants, original.initialChants);
	EXPECT_EQ(read.closedDown, original.closedDown);
	EXPECT_EQ(read.isMyInterfaceCasting, original.isMyInterfaceCasting);
	EXPECT_EQ(read.isCreatureCasting, original.isCreatureCasting);
	EXPECT_EQ(read.isHumanPlayerCasting, original.isHumanPlayerCasting);
	EXPECT_EQ(read.processInfo.power, original.processInfo.power);
	EXPECT_EQ(read.processInfo.curl, original.processInfo.curl);
	EXPECT_EQ(read.duration, original.duration);
	EXPECT_EQ(read.free, original.free);
	EXPECT_EQ(read.creator.kind, original.creator.kind);
	EXPECT_EQ(read.creator.player, original.creator.player);
	EXPECT_EQ(read.creator.entity, entt::entity {entt::null});
	EXPECT_EQ(read.player, original.player);
	EXPECT_EQ(read.hasPlayer, original.hasPlayer);
	EXPECT_EQ(read.castFromInterface, original.castFromInterface);
	EXPECT_EQ(read.age, original.age);
	EXPECT_EQ(read.magnitude, original.magnitude);
	EXPECT_EQ(read.strengthMultiplier, original.strengthMultiplier);
	EXPECT_EQ(read.originalCastPos, onMap(original.originalCastPos));
	EXPECT_EQ(read.manaPathPerTurn, original.manaPathPerTurn);
	EXPECT_EQ(read.manaPathPerEvent, original.manaPathPerEvent);
	// no player object: no player
	const auto none =
	    saved_spells::SpellOf(record->spell, components::SpellCreator::Kind::None, std::nullopt, entt::null, entt::null);
	EXPECT_FALSE(none.hasPlayer);
	EXPECT_EQ(none.player, PlayerNames::NEUTRAL);
}

// The load makes the water spells again in the list's order, the newest first, each with the age of its last ring, its
// seed's link and the record it was read from, and each is written back as it was read: a place whose map coordinates
// do not come back the same from metres keeps the coordinates it was read with
TEST_F(SpellLoad, WaterSpellsAreMadeAgainInTheirOrderAndWrittenBackAsRead)
{
	auto newer = Written(Water(2.5f));
	auto older = Written(Water(1.5f));
	ASSERT_TRUE(newer.has_value());
	ASSERT_TRUE(older.has_value());
	// 8090858 in metres and back is 8090857
	ASSERT_NE(map_coords::ToFixed(map_coords::ToMetres(8090858)), 8090858);
	newer->spell.position.position.x = 8090858;
	const auto seedAgain = Reg().Create();
	Reg().Assign<components::SpellSeed>(seedAgain);
	newer->spell.objects[3] = {77};
	std::vector<saved_spells::PlannedSpell> planned {{.number = 10,
	                                                  .record = *newer,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO},
	                                                 {.number = 11,
	                                                  .record = *older,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	std::unordered_map<gamesave::ObjectNumber, entt::entity> kept;
	const auto made = saved_spells::MakeSpells(
	    planned, {.entityOf = [&](gamesave::ObjectNumber number) { return number == 77 ? seedAgain : entt::null; }}, {},
	    [&kept](gamesave::ObjectNumber number, entt::entity entity, uint32_t type) {
		    EXPECT_EQ(type, SpellWaterRecord::k_Type);
		    kept.emplace(number, entity);
	    });
	ASSERT_EQ(made.size(), 2u);
	EXPECT_EQ(kept, made);
	ASSERT_EQ(magic::Spells().size(), 2u);
	EXPECT_EQ(magic::Spells()[0], made.at(10));
	EXPECT_EQ(magic::Spells()[1], made.at(11));
	const auto& spell = Reg().Get<const components::Spell>(made.at(10));
	EXPECT_EQ(spell.spellClass, components::SpellClass::Water);
	EXPECT_EQ(spell.seed, seedAgain);
	EXPECT_EQ(Reg().Get<const components::SpellSeed>(seedAgain).spell, made.at(10));
	EXPECT_EQ(Reg().Get<const magic::water::SpellWaterData>(made.at(10)).lastRipple, 1.4f);
	EXPECT_EQ(spell.effect, 0u);

	// written back as read (the seed is named by its number in this write)
	const auto again = Written(made.at(11));
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(*again, *older);
	auto newerAgain = Written(made.at(10));
	ASSERT_TRUE(newerAgain.has_value()) << _context->Failure();
	EXPECT_EQ(newerAgain->spell.position.position.x, 8090858);
	newerAgain->spell.objects[3] = {77};
	EXPECT_EQ(*newerAgain, *newer);

	// once moved, its place is written from metres
	Reg().Get<components::Spell>(made.at(10)).position.x = 101.0f;
	const auto moved = Written(made.at(10));
	ASSERT_TRUE(moved.has_value());
	EXPECT_EQ(moved->spell.position.position.x, map_coords::ToFixed(101.0f));

	// the reaction each started, once the reactions are made, by the id it was made again with
	newer->spell.other = {40};
	planned[0].record = *newer;
	saved_spells::LinkReactions(planned, made, {{40, 6}});
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(10)).reaction, 6u);
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(11)).reaction, 0u);
}

// A plain spell (a fireball's, a lightning bolt's) is planned from the game's list of spells and made again of the plain
// class with its values, kept by its number as its own class, and written back as read
TEST_F(SpellLoad, APlainSpellIsMadeAgainAndWrittenBackAsRead)
{
	const auto cast = Water(2.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::Fireball;
	values.spellClass = components::SpellClass::General;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	const auto plain = Written<gamesave::SpellRecord>(cast);
	ASSERT_TRUE(plain.has_value()) << _context->Failure();

	gamesave::WorldRead world;
	world.ResizeObjects(2);
	gamesave::PutRecord(world.objects[1], *plain);
	const auto plannedList = saved_spells::PlanSpells(world, {2}, {});
	ASSERT_EQ(plannedList.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<gamesave::SpellRecord>(plannedList[0].record));

	std::vector<saved_spells::PlannedSpell> planned {{.number = 12,
	                                                  .record = *plain,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	std::vector<uint32_t> types;
	const auto made = saved_spells::MakeSpells(
	    planned, {}, {},
	    [&types](gamesave::ObjectNumber /*number*/, entt::entity /*entity*/, uint32_t type) { types.push_back(type); });
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(types, std::vector<uint32_t> {gamesave::SpellRecord::k_Type});
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(12)).spellClass, components::SpellClass::General);
	const auto again = Written<gamesave::SpellRecord>(made.at(12));
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(*again, *plain);
}

// A healing spell is planned from the game's list of spells and made again of the healing class with its values, kept
// by its number as its own class, and written back as read
TEST_F(SpellLoad, AHealingSpellIsMadeAgainAndWrittenBackAsRead)
{
	const auto cast = Water(3.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::Heal;
	values.spellClass = components::SpellClass::Heal;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	const auto heal = Written<gamesave::SpellHealRecord>(cast);
	ASSERT_TRUE(heal.has_value()) << _context->Failure();

	gamesave::WorldRead world;
	world.ResizeObjects(2);
	gamesave::PutRecord(world.objects[1], *heal);
	const auto plannedList = saved_spells::PlanSpells(world, {2}, {});
	ASSERT_EQ(plannedList.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<gamesave::SpellHealRecord>(plannedList[0].record));

	std::vector<saved_spells::PlannedSpell> planned {{.number = 13,
	                                                  .record = *heal,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	std::vector<uint32_t> types;
	const auto made = saved_spells::MakeSpells(
	    planned, {}, {},
	    [&types](gamesave::ObjectNumber /*number*/, entt::entity /*entity*/, uint32_t type) { types.push_back(type); });
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(types, std::vector<uint32_t> {gamesave::SpellHealRecord::k_Type});
	ASSERT_EQ(magic::Spells().size(), 1u);
	EXPECT_EQ(magic::Spells()[0], made.at(13));
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(13)).spellClass, components::SpellClass::Heal);
	const auto again = Written<gamesave::SpellHealRecord>(made.at(13));
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(*again, *heal);
}

// The storm and tornado spells are planned from the game's list of spells and made again of their class with their
// values, in the list's order; they join the storm spells in their objects' order, each at the head, with no cast swirl
// and no water reaction, and each is written back as read
TEST_F(SpellLoad, StormSpellsAreMadeAgainJoinTheStormSpellsAndAreWrittenBackAsRead)
{
	const auto cast = Water(4.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::StormWindRain;
	values.spellClass = components::SpellClass::StormAndTornado;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	const auto storm = Written<gamesave::SpellStormAndTornadoRecord>(cast);
	ASSERT_TRUE(storm.has_value()) << _context->Failure();
	values.magicType = MagicType::Tornado;
	const auto tornado = Written<gamesave::SpellStormAndTornadoRecord>(cast);
	ASSERT_TRUE(tornado.has_value()) << _context->Failure();

	gamesave::WorldRead world;
	world.ResizeObjects(2);
	gamesave::PutRecord(world.objects[1], *storm);
	const auto plannedList = saved_spells::PlanSpells(world, {2}, {});
	ASSERT_EQ(plannedList.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<gamesave::SpellStormAndTornadoRecord>(plannedList[0].record));

	// the list names the storm first, the tornado's object comes later in the file
	std::vector<saved_spells::PlannedSpell> planned {{.number = 15,
	                                                  .record = *storm,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO},
	                                                 {.number = 20,
	                                                  .record = *tornado,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	std::vector<uint32_t> types;
	const auto made = saved_spells::MakeSpells(
	    planned, {}, {},
	    [&types](gamesave::ObjectNumber /*number*/, entt::entity /*entity*/, uint32_t type) { types.push_back(type); });
	ASSERT_EQ(made.size(), 2u);
	EXPECT_EQ(types, std::vector<uint32_t>(2, gamesave::SpellStormAndTornadoRecord::k_Type));
	ASSERT_EQ(magic::Spells().size(), 2u);
	EXPECT_EQ(magic::Spells()[0], made.at(15));
	EXPECT_EQ(magic::Spells()[1], made.at(20));
	ASSERT_EQ(magic::spell_storm::Spells().size(), 2u);
	EXPECT_EQ(magic::spell_storm::Spells()[0], made.at(20));
	EXPECT_EQ(magic::spell_storm::Spells()[1], made.at(15));
	for (const auto& [number, entity] : made)
	{
		EXPECT_EQ(Reg().Get<const components::Spell>(entity).spellClass, components::SpellClass::StormAndTornado);
		const auto& own = Reg().Get<const magic::SpellStormData>(entity);
		EXPECT_EQ(own.castEffect, 0u);
		EXPECT_EQ(own.waterReaction, 0u);
	}
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(20)).magicType, MagicType::Tornado);
	const auto stormAgain = Written<gamesave::SpellStormAndTornadoRecord>(made.at(15));
	ASSERT_TRUE(stormAgain.has_value()) << _context->Failure();
	EXPECT_EQ(*stormAgain, *storm);
	const auto tornadoAgain = Written<gamesave::SpellStormAndTornadoRecord>(made.at(20));
	ASSERT_TRUE(tornadoAgain.has_value()) << _context->Failure();
	EXPECT_EQ(*tornadoAgain, *tornado);
}

// A forest spell is planned from the game's list of spells and made again of the forest class with its values, whether it
// has made its forest, the forest the load made of the one it names and the most trees it makes, and written back as read;
// one naming a forest the load did not make is kept as read, not made
TEST_F(SpellLoad, AForestSpellIsMadeAgainWithItsForestAndOneNamingAForestNotMadeIsKept)
{
	const auto cast = Water(5.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::Forest;
	values.spellClass = components::SpellClass::Forest;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	Reg().Assign<magic::SpellForestData>(cast, magic::SpellForestData {.forestId = 12, .forestCreated = true, .maxTrees = 9});
	const auto forest = Written<gamesave::SpellForestRecord>(cast);
	ASSERT_TRUE(forest.has_value()) << _context->Failure();
	const auto forestNumber = forest->object.number;
	ASSERT_NE(forestNumber, 0u);

	gamesave::WorldRead world;
	world.ResizeObjects(2);
	gamesave::PutRecord(world.objects[1], *forest);
	const auto plannedList = saved_spells::PlanSpells(world, {2}, {});
	ASSERT_EQ(plannedList.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<gamesave::SpellForestRecord>(plannedList[0].record));

	std::vector<saved_spells::PlannedSpell> planned {{.number = 14,
	                                                  .record = *forest,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	// the forest it names was not made: the spell is not made
	const auto notMade =
	    saved_spells::MakeSpells(planned, {.forestOf = [](gamesave::ObjectNumber /*number*/) { return 0U; }}, {}, {});
	EXPECT_TRUE(notMade.empty());
	EXPECT_TRUE(magic::Spells().empty());

	// the load made it again as the forest of key 30
	std::vector<uint32_t> types;
	const auto made = saved_spells::MakeSpells(
	    planned, {.forestOf = [forestNumber](gamesave::ObjectNumber number) { return number == forestNumber ? 30U : 0U; }}, {},
	    [&types](gamesave::ObjectNumber /*number*/, entt::entity /*entity*/, uint32_t type) { types.push_back(type); });
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(types, std::vector<uint32_t> {gamesave::SpellForestRecord::k_Type});
	ASSERT_EQ(magic::Spells().size(), 1u);
	EXPECT_EQ(magic::Spells()[0], made.at(14));
	EXPECT_EQ(Reg().Get<const components::Spell>(made.at(14)).spellClass, components::SpellClass::Forest);
	const auto& own = Reg().Get<const magic::SpellForestData>(made.at(14));
	EXPECT_EQ(own.forestId, 30u);
	EXPECT_TRUE(own.forestCreated);
	EXPECT_EQ(own.maxTrees, 9);
	EXPECT_FALSE(own.forestDeleted);

	// written back as read (the forest is named by its number in this write)
	auto again = Written<gamesave::SpellForestRecord>(made.at(14));
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(again->object.number, _context->Reference(SourceKey {.type = gamesave::ForestRecord::k_Type, .id = 30}));
	again->object = forest->object;
	EXPECT_EQ(*again, *forest);
}

// A flock spell is made again with its flock, the animals it has made, its count to emit and where it last let animals
// out, and its animals the load made, each naming it as its spell (one it did not make is left out); it is written
// back as read
TEST_F(SpellLoad, AFlockSpellIsMadeAgainWithItsFlockItsCountsAndItsAnimalsAndWrittenBackAsRead)
{
	const auto cast = Water(2.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::FlockGround;
	values.spellClass = components::SpellClass::FlockGround;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	const auto flock = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(flock, gamesave::WaypointRecord {});
	Reg().Assign<components::FlockSpell>(
	    cast, components::FlockSpell {
	              .flock = flock, .created = 7, .emitted = 7.2f, .lastSpawn = {65536, 131072}, .lastSpawnHeight = 0.0f});
	auto record = Written<gamesave::SpellFlockGroundRecord>(cast);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	record->flock.object = {80};
	record->objects.numbers = {81, 82};
	const auto newer = Reg().Create();
	Reg().Assign<magic::SpellFlockAnimal>(newer).wolf = true;
	std::vector<saved_spells::PlannedSpell> planned {{.number = 12,
	                                                  .record = *record,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	const auto made =
	    saved_spells::MakeSpells(planned,
	                             {.entityOf =
	                                  [&](gamesave::ObjectNumber number) {
		                                  return number == 80 ? flock : number == 81 ? newer : entt::entity {entt::null};
	                                  }},
	                             {}, {});
	ASSERT_EQ(made.size(), 1u);
	const auto spell = made.at(12);
	EXPECT_EQ(Reg().Get<const components::Spell>(spell).spellClass, components::SpellClass::FlockGround);
	const auto& data = Reg().Get<const components::FlockSpell>(spell);
	EXPECT_EQ(data.flock, flock);
	EXPECT_EQ(data.created, 7);
	EXPECT_EQ(data.emitted, 7.2f);
	EXPECT_EQ(data.lastSpawn, glm::ivec2(65536, 131072));
	EXPECT_EQ(magic::spell_objects::Objects(spell), (std::vector<entt::entity> {newer}));
	EXPECT_EQ(Reg().Get<const magic::SpellFlockAnimal>(newer).spell, spell);

	// written back as read, but for the animal the load did not make
	auto again = Written<gamesave::SpellFlockGroundRecord>(spell);
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(again->objects.numbers, (std::vector<gamesave::ObjectNumber> {_context->Reference(newer)}));
	EXPECT_EQ(again->flock.words, record->flock.words);
	EXPECT_EQ(again->flock.lastSpawn, record->flock.lastSpawn);
	EXPECT_EQ(again->spell.words, record->spell.words);
}

// A creature spell is made again with the creature it was cast on and whether the hand poured onto it, and written back
// as read; one on a creature the load did not make stays as the load read it, not made
TEST_F(SpellLoad, ACreatureSpellIsMadeAgainWithItsCreatureAndOneOnACreatureNotMadeIsKept)
{
	const auto cast = Water(2.0f);
	auto& values = Reg().Get<components::Spell>(cast);
	values.magicType = MagicType::CreatureSpellFreeze;
	values.spellClass = components::SpellClass::Creature;
	Reg().Remove<magic::water::SpellWaterData>(cast);
	const auto creature = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(creature, gamesave::WaypointRecord {});
	Reg().Assign<magic::SpellCreatureData>(cast, magic::SpellCreatureData {.creature = creature, .handPoured = true});
	auto record = Written<gamesave::SpellCreatureRecord>(cast);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	record->object = {90};
	auto other = *record;
	other.object = {91};
	std::vector<saved_spells::PlannedSpell> planned {{.number = 13,
	                                                  .record = *record,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO},
	                                                 {.number = 14,
	                                                  .record = other,
	                                                  .creatorKind = components::SpellCreator::Kind::Player,
	                                                  .player = PlayerNames::PLAYER_TWO}};
	const auto made = saved_spells::MakeSpells(
	    planned,
	    {.entityOf = [&](gamesave::ObjectNumber number) { return number == 90 ? creature : entt::entity {entt::null}; }}, {},
	    {});
	ASSERT_EQ(made.size(), 1u);
	ASSERT_TRUE(made.contains(13));
	const auto spell = made.at(13);
	EXPECT_EQ(Reg().Get<const components::Spell>(spell).spellClass, components::SpellClass::Creature);
	const auto& data = Reg().Get<const magic::SpellCreatureData>(spell);
	EXPECT_EQ(data.creature, creature);
	EXPECT_TRUE(data.handPoured);
	const auto again = Written<gamesave::SpellCreatureRecord>(spell);
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_EQ(again->object.number, _context->Reference(creature));
	EXPECT_EQ(again->byte, 1u);
	EXPECT_EQ(again->spell.words, record->spell.words);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The Land 2 save of
// Jahovia's slot 20 holds one water spell in its list of spells, with its effect's interface and manager, cast by a
// creature for the second player, and the reaction it started (to look at a nice spell); its shield spell is no water
// spell, so it is not planned
TEST(SpellLoadFrozen, TheLandTwoSaveOfSlotTwentyPlansItsWaterSpellWithItsEffectAndItsReaction)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no Land 2 save in Jahovia's slot 20";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto playerOf = [&world](gamesave::ObjectNumber number) -> std::optional<PlayerNames> {
		const auto& object = world.objects.at(number - 1);
		const auto* player = object.has_value() ? std::get_if<gamesave::PlayerRecord>(&*object) : nullptr;
		return player != nullptr ? std::optional(static_cast<PlayerNames>(player->playerNumber)) : std::nullopt;
	};
	const auto planned =
	    saved_spells::PlanSpells(world, game->lists.at(static_cast<size_t>(GlobalList::Spells)).numbers, playerOf);
	// the water spell and the shield spell, each by its number
	ASSERT_EQ(planned.size(), 2u);
	const auto find = [&planned](gamesave::ObjectNumber number) {
		return std::ranges::find(planned, number, &saved_spells::PlannedSpell::number);
	};
	const auto water = find(7382);
	ASSERT_NE(water, planned.end());
	ASSERT_TRUE(std::holds_alternative<SpellWaterRecord>(water->record));
	ASSERT_TRUE(water->effect.has_value());
	EXPECT_EQ(water->effect->interfaceNumber, 7383u);
	EXPECT_EQ(water->effect->managerNumber, 7384u);
	EXPECT_EQ(water->creatorKind, components::SpellCreator::Kind::Creature);
	EXPECT_EQ(water->player, playerOf(2));
	EXPECT_EQ(std::get<SpellWaterRecord>(water->record).spell.other.number, 7392u);
	const auto shield = find(7393);
	ASSERT_NE(shield, planned.end());
	ASSERT_TRUE(std::holds_alternative<gamesave::SpellShieldRecord>(shield->record));
	const auto& shieldRecord = std::get<gamesave::SpellShieldRecord>(shield->record);
	EXPECT_FALSE(shield->effect.has_value());
	EXPECT_EQ(shield->creatorKind, components::SpellCreator::Kind::Player);
	EXPECT_EQ(shieldRecord.objects.numbers, (std::vector<gamesave::ObjectNumber> {7395}));
	EXPECT_EQ(shieldRecord.shields.numbers, (std::vector<gamesave::ObjectNumber> {7526, 7527}));
	EXPECT_EQ(shieldRecord.others[1].number, 7528u);
	EXPECT_EQ(shieldRecord.spell.objects[3].number, 7394u);

	// its physical shield, with its effect, and its matrix as the shield's turn makes it of its angle and scale, bit for
	// bit
	const auto shields =
	    saved_spells::PlanPhysicalShields(world, game->lists.at(static_cast<size_t>(GlobalList::MapShields)).numbers);
	ASSERT_EQ(shields.size(), 1u);
	EXPECT_EQ(shields.front().number, 7395u);
	ASSERT_TRUE(shields.front().effect.has_value());
	EXPECT_EQ(shields.front().effect->interfaceNumber, 7396u);
	EXPECT_EQ(shields.front().effect->managerNumber, 7397u);
	const auto values = saved_spells::PhysicalShieldOf(shields.front().record);
	EXPECT_EQ(values.matrix.rows, magic::map_shield::TurnedRows(values.angle, values.matrix.scale));
	EXPECT_EQ(values.position.y, shields.front().record.base.position.position.altitude);
}

// A physical shield's values are read back from its record as its writer wrote them, and the load makes it again with
// the newest serial, written back as it was read; a shield spell made again keeps it as its world shield and names
// itself as its spell, with its rings, and is given its struck and shield reactions once the reactions are made
TEST_F(SpellLoad, APhysicalShieldAndItsSpellAreMadeAgainAndWrittenBackAsRead)
{
	const auto spell = Water(1.5f);
	{
		auto& component = Reg().Get<components::Spell>(spell);
		component.magicType = MagicType::PhysicalShield;
		component.spellClass = components::SpellClass::Shield;
	}
	const auto original = Reg().Create();
	auto& shield = Reg().Assign<components::MagicShield>(original);
	shield.kind = components::MagicShield::Kind::Physical;
	shield.magicType = MagicType::PhysicalShield;
	shield.position = glm::vec3(3609.5f, -0.0625f, 3457.5f);
	shield.objectScale = 0.25f;
	shield.creationTurn = 12649;
	shield.matrix = {.rows = magic::map_shield::TurnedRows(5.369f, 0.11f), .translation = {1.0f, 2.0f, 3.0f}, .scale = 0.11f};
	shield.previousMatrix = {
	    .rows = magic::map_shield::TurnedRows(5.2f, 0.1f), .translation = {1.0f, 2.5f, 3.0f}, .scale = 0.1f};
	shield.startScale = 0.001f;
	shield.finalScale = 0.1f;
	shield.startSpin = 3.0f;
	shield.endSpin = 0.15f;
	shield.angle = 5.369f;
	shield.bob = 3.0f;
	shield.dying = true;
	shield.alpha = 166;
	shield.dieTime = 0.5f;
	Reg().Assign<components::ObjectCreationIndex>(original, 1234u);
	const auto record = Written<gamesave::PhysicalShieldRecord>(original);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	const auto read = saved_spells::PhysicalShieldOf(*record);
	EXPECT_EQ(read.position, shield.position);
	EXPECT_EQ(read.objectScale, shield.objectScale);
	EXPECT_EQ(read.creationTurn, shield.creationTurn);
	EXPECT_EQ(read.matrix, shield.matrix);
	EXPECT_EQ(read.previousMatrix, shield.previousMatrix);
	EXPECT_EQ(read.startScale, shield.startScale);
	EXPECT_EQ(read.finalScale, shield.finalScale);
	EXPECT_EQ(read.startSpin, shield.startSpin);
	EXPECT_EQ(read.endSpin, shield.endSpin);
	EXPECT_EQ(read.angle, shield.angle);
	EXPECT_EQ(read.bob, shield.bob);
	EXPECT_EQ(read.dying, shield.dying);
	EXPECT_EQ(read.alpha, shield.alpha);
	EXPECT_EQ(read.dieTime, shield.dieTime);

	// made again, with the newest serial and the record it was read from, written back as read
	Reg().Destroy(original);
	std::unordered_map<gamesave::ObjectNumber, entt::entity> entities;
	const auto keep = [&entities](gamesave::ObjectNumber number, entt::entity entity, uint32_t /*type*/) {
		entities.emplace(number, entity);
	};
	const auto entityOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	const std::vector<saved_spells::PlannedPhysicalShield> plannedShields {{.number = 30, .record = *record}};
	const auto shields = saved_spells::MakePhysicalShields(plannedShields, {.entityOf = entityOf}, {}, keep);
	ASSERT_EQ(shields.size(), 1u);
	const auto made = shields.at(30);
	EXPECT_EQ(magic::map_shield::Shields().front(), made);
	// its creation number is its record's, not a new one
	EXPECT_EQ(Reg().Get<components::ObjectCreationIndex>(made).value, 1234u);
	EXPECT_EQ(record->base.object.creationIndex, 1234u);
	const auto again = Written<gamesave::PhysicalShieldRecord>(made);
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	// made again, it stands in the map's cells and counts as fixed, as its place's flags say (the shield written first
	// was in none); once the object draw list has listed it, its flags carry the listed mark too, as the frozen saves'
	// physical shields have them (0x8101)
	constexpr uint16_t k_InTheMap = position_flags::k_CountsAsFixed | position_flags::k_InMap;
	EXPECT_EQ(again->base.position.flags, k_InTheMap);
	auto inTheMap = *record;
	inTheMap.base.position.flags = k_InTheMap;
	EXPECT_EQ(*again, inTheMap);
	Reg().Assign<components::DrawListed>(made);
	const auto listed = Written<gamesave::PhysicalShieldRecord>(made);
	ASSERT_TRUE(listed.has_value()) << _context->Failure();
	EXPECT_EQ(listed->base.position.flags, k_InTheMap | position_flags::k_DrawListed);

	// the shield spell that keeps it, with a ring
	const auto ring = Reg().Create();
	entities.emplace(31, ring);
	auto shieldSpell = Written<gamesave::SpellShieldRecord>(spell);
	ASSERT_TRUE(shieldSpell.has_value()) << _context->Failure();
	shieldSpell->objects.numbers = {30};
	shieldSpell->shields.numbers = {31};
	shieldSpell->others = {gamesave::Ref {40}, gamesave::Ref {41}};
	const std::vector<saved_spells::PlannedSpell> planned {{.number = 32,
	                                                        .record = *shieldSpell,
	                                                        .creatorKind = components::SpellCreator::Kind::Player,
	                                                        .player = PlayerNames::PLAYER_TWO}};
	const auto spells = saved_spells::MakeSpells(planned, {.entityOf = entityOf}, {}, keep);
	ASSERT_EQ(spells.size(), 1u);
	const auto madeSpell = spells.at(32);
	EXPECT_EQ(magic::spell_objects::Objects(madeSpell), (std::vector<entt::entity> {made}));
	EXPECT_EQ(Reg().Get<const components::MagicShield>(made).spell, madeSpell);
	const auto& data = Reg().Get<const magic::SpellShieldData>(madeSpell);
	EXPECT_EQ(data.rings, (std::vector<entt::entity> {ring}));
	EXPECT_EQ(data.town, entt::entity {entt::null});
	saved_spells::LinkReactions(planned, spells, {{40, 5}, {41, 6}});
	EXPECT_EQ(data.struckReaction, 5u);
	EXPECT_EQ(data.shieldReaction, 6u);
}

// The teleports' stones are planned from the game's list of objects of several cells with the player their records name
// and their place in that player's list; made again, each player's list keeps its order (the newest first), each stone
// with its creation number, written back as it was read; the teleport spell made again keeps its stones in its list's
// order, and each stone is given the spell its record names and the reaction it spreads once the reactions are made
TEST_F(SpellLoad, TeleportStonesAndTheirSpellAreMadeAgainInTheirOrderAndWrittenBackAsRead)
{
	const test::RestoreService<Locator::teleportSystem> restoreTeleports;
	Locator::teleportSystem::emplace<ecs::systems::TeleportSystem>();
	RegisterMobileStaticWriters(_writers);
	const auto spell = Water(1.5f);
	{
		auto& component = Reg().Get<components::Spell>(spell);
		component.magicType = MagicType::Teleport;
		component.spellClass = components::SpellClass::Teleport;
	}
	Reg().Remove<magic::water::SpellWaterData>(spell);
	const auto stoneAt = [this, spell](glm::vec3 position, uint32_t serial, uint32_t reaction) {
		const auto entity = Reg().Create();
		ecs::object_index::Assign(entity);
		Reg().Assign<components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& stone = Reg().Assign<components::TeleportStone>(entity);
		stone.player = PlayerNames::PLAYER_TWO;
		stone.serial = serial;
		stone.spell = spell;
		stone.reaction = reaction;
		return entity;
	};
	const auto older = stoneAt(glm::vec3(2510.0f, 0.0f, 2520.0f), 1, 7);
	const auto newer = stoneAt(glm::vec3(2530.0f, 0.0f, 2520.0f), 2, 8);
	magic::spell_objects::Add(spell, older);
	magic::spell_objects::Add(spell, newer);
	const auto olderRecord = Written<gamesave::MagicTeleportRecord>(older);
	ASSERT_TRUE(olderRecord.has_value()) << _context->Failure();
	const auto newerRecord = Written<gamesave::MagicTeleportRecord>(newer);
	ASSERT_TRUE(newerRecord.has_value()) << _context->Failure();
	auto teleport = Written<gamesave::SpellTeleportRecord>(spell);
	ASSERT_TRUE(teleport.has_value()) << _context->Failure();

	// planned with their place in their player's list, and none for a stone no list names
	{
		gamesave::WorldRead world;
		world.ResizeObjects(4);
		gamesave::PlayerRecord player;
		player.teleports = {3, 2};
		gamesave::PutRecord(world.objects[0], player);
		auto first = *olderRecord;
		first.player.number = 1;
		gamesave::PutRecord(world.objects[1], first);
		auto second = *newerRecord;
		second.player.number = 1;
		gamesave::PutRecord(world.objects[2], second);
		auto alone = *olderRecord;
		alone.player.number = 0;
		gamesave::PutRecord(world.objects[3], alone);
		const auto playerOf = [](gamesave::ObjectNumber number) {
			return number == 1 ? std::optional(PlayerNames::PLAYER_TWO) : std::nullopt;
		};
		const auto planned = saved_spells::PlanTeleportStones(world, {2, 3, 4, 1}, playerOf);
		ASSERT_EQ(planned.size(), 3u);
		EXPECT_EQ(planned[0].number, 2u);
		EXPECT_EQ(planned[0].player, PlayerNames::PLAYER_TWO);
		EXPECT_EQ(planned[0].place, std::optional<size_t>(1));
		EXPECT_EQ(planned[1].place, std::optional<size_t>(0));
		EXPECT_FALSE(planned[2].player.has_value());
		EXPECT_FALSE(planned[2].place.has_value());
	}

	// made again: the player's list as it was, the newest first, each with its creation number and no travellers
	Reg().Destroy(older);
	Reg().Destroy(newer);
	std::unordered_map<gamesave::ObjectNumber, entt::entity> entities;
	const auto keep = [&entities](gamesave::ObjectNumber number, entt::entity entity, uint32_t /*type*/) {
		entities.emplace(number, entity);
	};
	const auto entityOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	const std::vector<saved_spells::PlannedTeleportStone> plannedStones {
	    {.number = 20, .record = *olderRecord, .player = PlayerNames::PLAYER_TWO, .place = 1},
	    {.number = 21, .record = *newerRecord, .player = PlayerNames::PLAYER_TWO, .place = 0}};
	const auto stones = saved_spells::MakeTeleportStones(plannedStones, keep);
	ASSERT_EQ(stones.size(), 2u);
	EXPECT_EQ(magic::teleport::StonesOf(PlayerNames::PLAYER_TWO), (std::vector<entt::entity> {stones.at(21), stones.at(20)}));
	EXPECT_EQ(ecs::object_index::Of(stones.at(20)), static_cast<int64_t>(olderRecord->base.object.creationIndex));
	EXPECT_TRUE(Reg().Get<const components::TeleportStone>(stones.at(20)).travellers.empty());

	// the spell that keeps them, in its list's order
	const auto writtenObjects = teleport->objects;
	teleport->objects.numbers = {21, 20};
	const std::vector<saved_spells::PlannedSpell> planned {{.number = 22,
	                                                        .record = *teleport,
	                                                        .creatorKind = components::SpellCreator::Kind::Player,
	                                                        .player = PlayerNames::PLAYER_TWO}};
	const auto spells = saved_spells::MakeSpells(planned, {.entityOf = entityOf}, {}, keep);
	ASSERT_EQ(spells.size(), 1u);
	const auto madeSpell = spells.at(22);
	EXPECT_EQ(Reg().Get<const components::Spell>(madeSpell).spellClass, components::SpellClass::Teleport);
	EXPECT_EQ(magic::spell_objects::Objects(madeSpell), (std::vector<entt::entity> {stones.at(21), stones.at(20)}));
	// each stone, the spell its record names and its reaction by the id it was made again with
	auto named = plannedStones;
	named[0].record.spell.number = 22;
	named[0].record.reaction.number = 40;
	named[1].record.spell.number = 0;
	named[1].record.reaction.number = 0;
	saved_spells::LinkTeleportStones(named, stones, spells, {{40, 5}});
	EXPECT_EQ(Reg().Get<const components::TeleportStone>(stones.at(20)).spell, madeSpell);
	EXPECT_EQ(Reg().Get<const components::TeleportStone>(stones.at(20)).reaction, 5u);
	EXPECT_EQ(Reg().Get<const components::TeleportStone>(stones.at(21)).spell, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const components::TeleportStone>(stones.at(21)).reaction, 0u);

	// written back as read (its spell the one made again), at the scale it was read with, its place's flags those of
	// a stone in the map's cells
	Reg().Get<components::TeleportStone>(stones.at(21)).spell = madeSpell;
	Reg().Get<components::TeleportStone>(stones.at(21)).reaction = 8;
	const auto again = Written<gamesave::MagicTeleportRecord>(stones.at(21));
	ASSERT_TRUE(again.has_value()) << _context->Failure();
	EXPECT_NE(again->base.position.flags & position_flags::k_InMap, 0);
	EXPECT_EQ(again->base.object.scale, 0.01f);
	auto inTheMap = *newerRecord;
	inTheMap.base.position.flags = again->base.position.flags;
	inTheMap.spell = again->spell;
	EXPECT_EQ(*again, inTheMap);
	const auto spellAgain = Written<gamesave::SpellTeleportRecord>(madeSpell);
	ASSERT_TRUE(spellAgain.has_value()) << _context->Failure();
	EXPECT_EQ(spellAgain->objects, writtenObjects);
}

// A spiritual shield is written as the original writes its object: a fixed object's parts of its row at its place, at
// its scale, its spell and its magic's info; planned from the game's list of map shields, it is made again with the
// physical shields in the list's order, with its values and its creation number, written back as it was read, and the
// shield spell made again keeps it as its world shield and names itself as its spell
TEST_F(SpellLoad, ASpiritualShieldIsWrittenMadeAgainInItsListsOrderAndWrittenBackAsRead)
{
	const auto spell = Water(1.5f);
	{
		auto& component = Reg().Get<components::Spell>(spell);
		component.magicType = MagicType::Shield;
		component.spellClass = components::SpellClass::Shield;
	}
	Reg().Remove<magic::water::SpellWaterData>(spell);
	const auto original = Reg().Create();
	auto& shield = Reg().Assign<components::MagicShield>(original);
	shield.kind = components::MagicShield::Kind::Spiritual;
	shield.magicType = MagicType::Shield;
	shield.spell = spell;
	shield.position = glm::vec3(3609.5f, 0.0f, 3457.5f);
	shield.objectScale = 0.85f;
	Reg().Assign<components::ObjectCreationIndex>(original, 4321u);
	const auto record = Written<gamesave::SpiritualShieldRecord>(original);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.object.info, 2084u);
	EXPECT_EQ(record->info, 2643u);
	EXPECT_NE(record->owner.number, 0u);
	EXPECT_EQ(record->base.position.position.x, map_coords::ToFixed(3609.5f));
	EXPECT_EQ(record->base.position.position.altitude, 0.0f);
	EXPECT_EQ(record->base.object.coords.altitude, 0.0f);
	EXPECT_EQ(record->base.object.scale, 0.85f);
	EXPECT_EQ(record->base.object.heading, 0.0f);
	EXPECT_EQ(record->base.object.creationIndex, 4321u);
	const auto read = saved_spells::SpiritualShieldOf(*record);
	EXPECT_EQ(read.kind, components::MagicShield::Kind::Spiritual);
	EXPECT_EQ(read.position, shield.position);
	EXPECT_EQ(read.objectScale, shield.objectScale);

	// planned with its place in the game's list of map shields, after a physical shield that is newer
	gamesave::WorldRead world;
	world.ResizeObjects(2);
	gamesave::PutRecord(world.objects[0], gamesave::PhysicalShieldRecord {});
	gamesave::PutRecord(world.objects[1], *record);
	const auto plannedSpiritual = saved_spells::PlanSpiritualShields(world, {1, 2});
	ASSERT_EQ(plannedSpiritual.size(), 1u);
	EXPECT_EQ(plannedSpiritual[0].number, 2u);
	EXPECT_EQ(plannedSpiritual[0].place, 1u);
	const auto plannedPhysical = saved_spells::PlanPhysicalShields(world, {1, 2});
	ASSERT_EQ(plannedPhysical.size(), 1u);
	EXPECT_EQ(plannedPhysical[0].place, 0u);

	// made again in the list's order: the physical shield, first in the list, the newest
	Reg().Destroy(original);
	std::unordered_map<gamesave::ObjectNumber, entt::entity> entities;
	const auto keep = [&entities](gamesave::ObjectNumber number, entt::entity entity, uint32_t /*type*/) {
		entities.emplace(number, entity);
	};
	const auto entityOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	const auto shields = saved_spells::MakePhysicalShields(plannedPhysical, {.entityOf = entityOf}, {}, keep, plannedSpiritual);
	ASSERT_EQ(shields.size(), 2u);
	const auto made = shields.at(2);
	EXPECT_EQ(magic::map_shield::Shields(), (std::vector<entt::entity> {shields.at(1), made}));
	EXPECT_EQ(Reg().Get<const components::MagicShield>(made).kind, components::MagicShield::Kind::Spiritual);
	EXPECT_EQ(Reg().Get<const components::MagicShield>(made).objectScale, 0.85f);
	EXPECT_EQ(Reg().Get<const components::Transform>(made).scale, glm::vec3(0.85f));
	EXPECT_EQ(ecs::object_index::Of(made), 4321);

	// the shield spell that keeps it
	auto shieldSpell = Written<gamesave::SpellShieldRecord>(spell);
	ASSERT_TRUE(shieldSpell.has_value()) << _context->Failure();
	shieldSpell->objects.numbers = {2};
	shieldSpell->shields.numbers = {};
	const std::vector<saved_spells::PlannedSpell> planned {{.number = 32,
	                                                        .record = *shieldSpell,
	                                                        .creatorKind = components::SpellCreator::Kind::Player,
	                                                        .player = PlayerNames::PLAYER_TWO}};
	const auto spells = saved_spells::MakeSpells(planned, {.entityOf = entityOf}, {}, keep);
	ASSERT_EQ(spells.size(), 1u);
	EXPECT_EQ(magic::spell_objects::Objects(spells.at(32)), (std::vector<entt::entity> {made}));
	EXPECT_EQ(Reg().Get<const components::MagicShield>(made).spell, spells.at(32));

	// written back as read, its place's flags those of a shield in the map's cells
	const auto written = Written<gamesave::SpiritualShieldRecord>(made);
	ASSERT_TRUE(written.has_value()) << _context->Failure();
	auto inTheMap = *record;
	inTheMap.base.position.flags = written->base.position.flags;
	EXPECT_EQ(*written, inTheMap);
}
