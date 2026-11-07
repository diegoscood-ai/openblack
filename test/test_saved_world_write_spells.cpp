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

#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellResource.h"
#include "Magic/Spells/SpellWater.h"
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

// A loaded spell keeps the three bytes after its process info, which are not read; a spell with a reaction stops the
// write and says which
TEST_F(SpellWrite, ALoadedSpellKeepsTheBytesAfterItsProcessInfoAndOneWithAReactionStops)
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
	std::string failure;
	EXPECT_FALSE(Written(spell, &failure).has_value());
	EXPECT_NE(failure.find("reaction"), std::string::npos) << failure;
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

// The teleport writes its objects, newest first; an object of no class (its stones have no writer yet) stops the write
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
