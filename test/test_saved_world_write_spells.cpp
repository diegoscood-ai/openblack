/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water spell written from its entity: every spell's values from its component (its place and cast places as map
// coordinates, its effect's particle interface, its chants, flags, process info, creator, player, the status of the
// hand that cast it, its seed, magic type, age, magnitude and mana path) and the age it last made a ring at; a loaded
// spell keeps the bytes after its process info, and a spell with a reaction stops the write
// (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellCreature.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellResource.h"
#include "Magic/Spells/SpellShield.h"
#include "Magic/Spells/SpellWater.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SpellWaterRecord;
namespace components = openblack::ecs::components;

namespace
{
class SpellWrite: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		Locator::fireSystem::emplace<test::FakeFires>();
	}

	ecs::Registry& Reg() { return *_registry; }

	/// A water spell cast by a player from the hand
	entt::entity Water()
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
		made.isMyInterfaceCasting = true;
		made.isHumanPlayerCasting = true;
		made.duration = 12.0f;
		made.manaPathPerTurn = 3.0f;
		made.manaPathPerEvent = 4.0f;
		made.creator = {.kind = components::SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_TWO};
		made.player = PlayerNames::PLAYER_TWO;
		made.hasPlayer = true;
		made.castFromInterface = true;
		made.effect = 9;
		made.age = 1.5f;
		made.magnitude = 6.0f;
		made.strengthMultiplier = 2.0f;
		Reg().Assign<magic::water::SpellWaterData>(spell, magic::water::SpellWaterData {.lastRipple = 1.4f});
		return spell;
	}

	/// The record the writer makes of the entity, and the failure when it makes none
	template <typename Record = SpellWaterRecord>
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
	// the map cells the place's flags ask, reset after the registry
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	// the fires a fixed object's writer asks of its burning, reset after the registry
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::Registry* _registry {nullptr};
};

gamesave::Point RawPoint(const glm::vec3& metres)
{
	return {std::bit_cast<float>(map_coords::ToFixed(metres.x)), std::bit_cast<float>(map_coords::ToFixed(metres.z)), metres.y};
}
} // namespace

// A water spell made in play is written from its component: every value in the original's place, the marks every
// spell writes, and the age of its last ring
TEST_F(SpellWrite, AWaterSpellMadeInPlayIsWrittenFromItsComponent)
{
	const auto spell = Water();
	const auto record = Written(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	const auto& fields = record->spell;
	EXPECT_EQ(fields.position.position.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(fields.position.position.z, map_coords::ToFixed(200.0f));
	EXPECT_EQ(fields.position.position.altitude, 2.0f);
	EXPECT_EQ(fields.word, 0x12C859u);
	EXPECT_EQ(fields.secondWord, 0x2A1C9Au);
	EXPECT_EQ(fields.object.number,
	          _context->Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = 9}));
	EXPECT_EQ(fields.other.number, 0u);
	EXPECT_EQ(fields.points[0], (gamesave::Point {0.0f, 0.0f, 1.0f}));
	EXPECT_EQ(fields.points[1], RawPoint(glm::vec3(101.0f, 0.0f, 201.0f)));
	EXPECT_EQ(fields.points[2], (gamesave::Point {0.5f, 0.0f, 0.5f}));
	EXPECT_EQ(fields.words[0], std::bit_cast<uint32_t>(300.0f));
	EXPECT_EQ(fields.words[1], std::bit_cast<uint32_t>(400.0f));
	EXPECT_EQ(fields.words[2], 0u);
	EXPECT_EQ(fields.words[3], 1u);
	EXPECT_EQ(fields.words[4], 0u);
	EXPECT_EQ(fields.words[5], 1u);
	float strength = 0.0f;
	std::memcpy(&strength, fields.values.data() + 48, sizeof(strength));
	EXPECT_EQ(strength, 1.0f);
	EXPECT_EQ(fields.values[56], 1u);
	EXPECT_EQ(fields.value, 12.0f);
	EXPECT_EQ(fields.flag, 0u);
	EXPECT_EQ(fields.secondFlag, 0u);
	const SourceKey two {.type = gamesave::k_PlayerType, .id = 1};
	EXPECT_EQ(fields.objects[0].number, _context->Reference(two));
	EXPECT_EQ(fields.objects[1].number, _context->Reference(two));
	EXPECT_EQ(fields.objects[2].number,
	          _context->Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type, .id = 1}));
	EXPECT_EQ(fields.objects[3].number, 0u);
	EXPECT_EQ(fields.thirdWord, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(fields.floats, (std::array<float, 3> {1.5f, 6.0f, 2.0f}));
	EXPECT_EQ(fields.place, RawPoint(glm::vec3(99.0f, 0.0f, 199.0f)));
	EXPECT_EQ(fields.lastFloats, (std::array<float, 2> {3.0f, 4.0f}));
	EXPECT_EQ(record->word, std::bit_cast<uint32_t>(1.4f));
}

// A loaded spell keeps the three bytes after its process info, which are not read; a spell with a reaction names it by
// its id, as the reactions' writer keys it
TEST_F(SpellWrite, ALoadedSpellKeepsTheBytesAfterItsProcessInfoAndNamesItsReactionByItsId)
{
	const auto spell = Water();
	SpellWaterRecord kept;
	kept.spell.values[57] = 0xCD;
	kept.spell.values[59] = 0xCD;
	Reg().Assign<components::LoadedRecord<SpellWaterRecord>>(spell, kept);
	const auto record = Written(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->spell.values[57], 0xCDu);
	EXPECT_EQ(record->spell.values[58], 0u);
	EXPECT_EQ(record->spell.values[59], 0xCDu);

	Reg().Get<components::Spell>(spell).reaction = 3;
	const auto withReaction = Written(spell);
	ASSERT_TRUE(withReaction.has_value()) << _context->Failure();
	EXPECT_EQ(withReaction->spell.other.number,
	          _context->Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = 3}));
	EXPECT_NE(withReaction->spell.other.number, 0u);
}

// Each class is written by its own record: the plain spell, the storm and the tornado and the healing spell with every
// spell's values only, and their class's number
TEST_F(SpellWrite, ThePlainTheStormAndTheHealingSpellsWriteEverySpellsValuesOnly)
{
	const auto spell = Water();
	auto& made = Reg().Get<components::Spell>(spell);
	made.spellClass = components::SpellClass::General;
	made.magicType = MagicType::Fireball;
	const auto plain = Written<gamesave::SpellRecord>(spell);
	ASSERT_TRUE(plain.has_value()) << _context->Failure();
	EXPECT_EQ(plain->spell.thirdWord, static_cast<uint32_t>(MagicType::Fireball));
	EXPECT_EQ(plain->spell.floats, (std::array<float, 3> {1.5f, 6.0f, 2.0f}));
	made.spellClass = components::SpellClass::StormAndTornado;
	EXPECT_TRUE(Written<gamesave::SpellStormAndTornadoRecord>(spell).has_value()) << _context->Failure();
	made.spellClass = components::SpellClass::Heal;
	EXPECT_TRUE(Written<gamesave::SpellHealRecord>(spell).has_value()) << _context->Failure();
}

// The food and wood spells write whether their first grain has landed
TEST_F(SpellWrite, AResourceSpellWritesWhetherItsFirstGrainHasLanded)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Resource;
	auto first = Written<gamesave::SpellResourceRecord>(spell);
	ASSERT_TRUE(first.has_value()) << _context->Failure();
	EXPECT_EQ(first->byte, 0u);
	Reg().Assign<magic::SpellResourceData>(spell, magic::SpellResourceData {.firstDone = true});
	const auto later = Written<gamesave::SpellResourceRecord>(spell);
	ASSERT_TRUE(later.has_value()) << _context->Failure();
	EXPECT_EQ(later->byte, 1u);
}

// The forest spell writes whether it made its forest, the forest while it stands (by its key) and the most trees it
// makes
TEST_F(SpellWrite, AForestSpellWritesItsForestAndTheMostTreesItMakes)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Forest;
	Reg().Assign<magic::SpellForestData>(spell, magic::SpellForestData {.forestId = 12, .forestCreated = true, .maxTrees = 9});
	const auto record = Written<gamesave::SpellForestRecord>(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->byte, 1u);
	EXPECT_EQ(record->object.number, _context->Reference(SourceKey {.type = gamesave::ForestRecord::k_Type, .id = 12}));
	EXPECT_EQ(record->word, 9u);
	// once the forest has gone the spell names none
	Reg().Get<magic::SpellForestData>(spell).forestDeleted = true;
	const auto gone = Written<gamesave::SpellForestRecord>(spell);
	ASSERT_TRUE(gone.has_value()) << _context->Failure();
	EXPECT_EQ(gone->object.number, 0u);
}

// The creature spell writes the creature it was cast on, none once that creature is no longer available, and whether
// the caster's hand has poured onto it
TEST_F(SpellWrite, ACreatureSpellWritesItsCreatureAndWhetherTheHandPouredOntoIt)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Creature;
	const auto creature = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(creature, gamesave::WaypointRecord {});
	Reg().Assign<magic::SpellCreatureData>(spell, magic::SpellCreatureData {.creature = creature, .handPoured = true});
	const auto record = Written<gamesave::SpellCreatureRecord>(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->object.number, _context->Reference(creature));
	EXPECT_EQ(record->byte, 1u);
	Reg().Assign<components::Unavailable>(creature);
	const auto unavailable = Written<gamesave::SpellCreatureRecord>(spell);
	ASSERT_TRUE(unavailable.has_value()) << _context->Failure();
	EXPECT_EQ(unavailable->object.number, 0u);
	// cast at a place, it is on no creature and no hand poured
	Reg().Remove<magic::SpellCreatureData>(spell);
	const auto none = Written<gamesave::SpellCreatureRecord>(spell);
	ASSERT_TRUE(none.has_value()) << _context->Failure();
	EXPECT_EQ(none->object.number, 0u);
	EXPECT_EQ(none->byte, 0u);
}

// The flock spells write their animals, their flock while it is there, the animals made so far, the count to emit and
// where they last let animals out, then 0 for the word only their start sets; the flying one 0 for its own two words
// and byte too
TEST_F(SpellWrite, TheFlockSpellsWriteTheirFlockTheirCountsAndWhereTheyLastLetAnimalsOut)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::FlockFlying;
	const auto flock = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(flock, gamesave::WaypointRecord {});
	Reg().Assign<components::FlockSpell>(
	    spell, components::FlockSpell {
	               .flock = flock, .created = 5, .emitted = 6.0f, .lastSpawn = {65536, 131072}, .lastSpawnHeight = 2.5f});
	const auto flying = Written<gamesave::SpellFlockFlyingRecord>(spell);
	ASSERT_TRUE(flying.has_value()) << _context->Failure();
	EXPECT_EQ(flying->flock.object.number, _context->Reference(flock, gamesave::FlockRecord::k_Type));
	EXPECT_EQ(flying->flock.words[0], 5u);
	EXPECT_EQ(std::bit_cast<float>(flying->flock.words[1]), 6.0f);
	EXPECT_EQ(flying->flock.lastSpawn, (gamesave::MapCoords {.x = 65536, .z = 131072, .altitude = 2.5f}));
	EXPECT_EQ(flying->flock.lastWord, 0u);
	EXPECT_EQ(flying->words, (std::array<uint32_t, 2> {0, 0}));
	EXPECT_EQ(flying->byte, 0u);
	// on the ground, its flock gone: none
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::FlockGround;
	Reg().Destroy(flock);
	const auto ground = Written<gamesave::SpellFlockGroundRecord>(spell);
	ASSERT_TRUE(ground.has_value()) << _context->Failure();
	EXPECT_EQ(ground->flock.object.number, 0u);
	EXPECT_EQ(ground->flock.words[0], 5u);
}

// The teleport writes its objects, newest first; an object of no class stops the write
TEST_F(SpellWrite, ATeleportWritesItsObjectsAndAStoneOfNoClassStopsTheWrite)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Teleport;
	const auto object = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(object, gamesave::WaypointRecord {});
	magic::spell_objects::Add(spell, object);
	const auto record = Written<gamesave::SpellTeleportRecord>(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->objects.numbers, (std::vector<gamesave::ObjectNumber> {_context->Reference(object)}));

	const auto stone = Reg().Create();
	magic::spell_objects::Add(spell, stone);
	std::string failure;
	std::ignore = Written<gamesave::SpellTeleportRecord>(spell, &failure);
	EXPECT_NE(failure.find("of no class"), std::string::npos) << failure;
}

// The shield spell writes every spell's values, then its world shield (its objects), its anti-influence rings, the
// newest first, and its struck and shield reactions by their ids
TEST_F(SpellWrite, AShieldSpellWritesItsWorldShieldItsRingsAndItsTwoReactions)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Shield;
	const auto shield = Reg().Create();
	Reg().Assign<components::MagicShield>(shield, components::MagicShield {.kind = components::MagicShield::Kind::Physical});
	magic::spell_objects::Add(spell, shield);
	const auto newer = Reg().Create();
	const auto older = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(newer, gamesave::WaypointRecord {});
	Reg().Assign<components::LoadedRecord<gamesave::WaypointRecord>>(older, gamesave::WaypointRecord {});
	Reg().Assign<magic::SpellShieldData>(
	    spell, magic::SpellShieldData {.struckReaction = 0, .shieldReaction = 7, .rings = {newer, older}});
	const auto record = Written<gamesave::SpellShieldRecord>(spell);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->spell.thirdWord, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(record->objects.numbers, (std::vector<gamesave::ObjectNumber> {_context->Reference(shield)}));
	EXPECT_EQ(record->shields.numbers,
	          (std::vector<gamesave::ObjectNumber> {_context->Reference(newer), _context->Reference(older)}));
	EXPECT_EQ(record->others[0].number, 0u);
	EXPECT_EQ(record->others[1].number, _context->Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = 7}));
}

// A physical shield writes its place with its height above the land as the place's altitude (the place it was made
// at, on the land, as its target), its spell, its magic's info, its values and its two matrices as the game keeps
// them. The matrix of profile D's slot 20 Land 2 shield, from its saved angle, scale and translation, is the saved
// one bit for bit (the cosine a float, the sine unrounded)
TEST_F(SpellWrite, APhysicalShieldWritesItsValuesAndItsMatricesAsTheGameKeepsThem)
{
	const auto spell = Water();
	Reg().Get<components::Spell>(spell).spellClass = components::SpellClass::Shield;
	const auto entity = Reg().Create();
	auto& shield = Reg().Assign<components::MagicShield>(entity);
	shield.kind = components::MagicShield::Kind::Physical;
	shield.spell = spell;
	shield.magicType = MagicType::PhysicalShield;
	shield.position = glm::vec3(3609.5f, -0.0625f, 3457.5f);
	shield.creationTurn = 12649;
	shield.alpha = 166;
	shield.angle = std::bit_cast<float>(0x40ABD3B2u);
	const float scale = std::bit_cast<float>(0x3DE5FF17u);
	shield.matrix = {.rows = magic::map_shield::TurnedRows(shield.angle, scale),
	                 .translation = {std::bit_cast<float>(0x4561985Du), std::bit_cast<float>(0x41057660u),
	                                 std::bit_cast<float>(0x45581813u)},
	                 .scale = scale};
	shield.previousMatrix = {
	    .rows = magic::map_shield::TurnedRows(0.0f, 2.0f), .translation = {1.0f, 2.0f, 3.0f}, .scale = 2.0f};
	shield.startScale = 0.001f;
	shield.finalScale = 0.1f;
	shield.startSpin = 3.0f;
	shield.endSpin = 0.15f;
	shield.bob = 3.0f;
	shield.dieTime = 0.5f;
	shield.objectScale = 0.25f;
	shield.fx = 4;
	const auto record = Written<gamesave::PhysicalShieldRecord>(entity);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->base.position.position.x, map_coords::ToFixed(3609.5f));
	EXPECT_EQ(record->base.position.position.z, map_coords::ToFixed(3457.5f));
	EXPECT_EQ(record->base.position.position.altitude, -0.0625f);
	EXPECT_EQ(record->base.object.info, 2083u);
	EXPECT_EQ(record->base.object.coords.x, map_coords::ToFixed(3609.5f));
	EXPECT_EQ(record->base.object.coords.altitude, 0.0f);
	EXPECT_EQ(record->base.object.scale, 0.25f);
	EXPECT_EQ(record->base.object.heading, 0.0f);
	EXPECT_EQ(record->owner.number, _context->Reference(spell));
	EXPECT_EQ(record->info, 2642u);
	EXPECT_EQ(record->words, (std::array<uint32_t, 2> {std::bit_cast<uint32_t>(1.0f), 12649u}));
	constexpr std::array<uint32_t, 13> k_Saved {0x3D8C8182, 0x00000000, 0xBDB616D2, 0x00000000, 0x3DE5FF17,
	                                            0x00000000, 0x3DB616D2, 0x00000000, 0x3D8C8182, 0x4561985D,
	                                            0x41057660, 0x45581813, 0x3DE5FF17};
	std::array<uint32_t, 13> first {};
	std::memcpy(first.data(), record->first.data(), record->first.size());
	EXPECT_EQ(first, k_Saved);
	// the last turn's: at angle 0 its scale on the diagonal, its translation and its scale
	std::array<float, 13> second {};
	std::memcpy(second.data(), record->second.data(), record->second.size());
	EXPECT_EQ(second, (std::array<float, 13> {2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 2.0f, 3.0f, 2.0f}));
	EXPECT_EQ(record->secondWords, (std::array<uint32_t, 6> {std::bit_cast<uint32_t>(0.001f), std::bit_cast<uint32_t>(0.1f),
	                                                         std::bit_cast<uint32_t>(3.0f), std::bit_cast<uint32_t>(0.15f),
	                                                         0x40ABD3B2u, std::bit_cast<uint32_t>(3.0f)}));
	EXPECT_EQ(record->bytes, (std::array<uint8_t, 2> {0, 166}));
	EXPECT_EQ(record->lastWord, std::bit_cast<uint32_t>(0.5f));
	EXPECT_EQ(record->object.number,
	          _context->Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = 4}));
}

// A spell the load made again and deleted since has left the game's spells list, as the original's spell leaves the
// list as it is deleted; a spell the load kept as read stays, in the loaded file's order
TEST_F(SpellWrite, AKeptSpellDeletedSinceTheLoadLeavesTheGamesSpellsList)
{
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	Locator::magicSystem::emplace<test::InertMagicSystem>();
	gamesave::WorldRead kept;
	gamesave::GameHead game;
	game.lists[static_cast<size_t>(GlobalList::Spells)].numbers = {2, 3};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(SpellWaterRecord {});
	kept.objects.emplace_back(SpellWaterRecord {});
	// object 2 was made again and has been deleted since; object 3 was kept as read
	const auto deleted = Water();
	Reg().Destroy(deleted);
	const WorldSource source {.registry = Reg(),
	                          .landscape = "Land1.lnd",
	                          .kept = &kept,
	                          .loaded = {{2, {.entity = deleted, .type = SpellWaterRecord::k_Type}}}};
	WriteContext context(_writers, source);
	EXPECT_TRUE(context.IsGone(2));
	const auto listed = (*_writers.ListWriterOf(GlobalList::Spells))(context);
	EXPECT_EQ(listed, (std::vector<gamesave::ObjectNumber> {3}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}
