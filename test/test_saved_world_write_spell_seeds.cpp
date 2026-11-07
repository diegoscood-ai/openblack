/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A spell's seed written from its entity: an object's parts of its seed's row, then its own values; its creator the
// player or the worship icon, its holder's interface status; a loaded seed keeps the place it was made at, and a seed
// of no seed type or of a creator whose objects are not read stops the write (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <string>
#include <variant>

#include <EffectRecords.h>
#include <InterfaceRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SpellSeedRecord;
namespace components = openblack::ecs::components;

namespace
{
class SpellSeedWrite: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<test::FakeFires>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
	}

	ecs::Registry& Reg() { return *_registry; }

	/// A seed of the type in the hand of the creator's player
	entt::entity Seed(SpellSeedType type, components::SpellCreator creator)
	{
		const auto seed = Reg().Create();
		Reg().Assign<components::Transform>(seed, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& made = Reg().Assign<components::SpellSeed>(seed);
		made.seedType = type;
		made.creator = creator;
		return seed;
	}

	/// The record the writer makes of the entity, and the failure when it makes none
	std::optional<SpellSeedRecord> Written(entt::entity entity, std::string* failure = nullptr)
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
		const auto* record = made.record != nullptr ? std::get_if<SpellSeedRecord>(made.record.get()) : nullptr;
		return record != nullptr ? std::optional(*record) : std::nullopt;
	}

	ClassWriters _writers = [] {
		ClassWriters writers;
		RegisterSpellSeedWriters(writers);
		return writers;
	}();
	std::optional<WorldSource> _source;
	std::optional<WriteContext> _context;

private:
	// the map cells the place's flags ask, reset after the registry
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	// the fires an object's writer asks of its burning, reset after the registry
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// A seed in a player's hand: its object part of its seed's row, its power-up level, flags, row, the player as its
// creator, its holder's interface status, its cast, learnt and one-shot flags, its charges, what it kept from its
// spell, its power, cast multiplier, readiness, turns in the hand and the magic of its last cast
TEST_F(SpellSeedWrite, ASeedInAPlayersHandIsWrittenFromItsEntity)
{
	const auto entity =
	    Seed(SpellSeedType::Heal, {.kind = components::SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_TWO});
	auto& seed = Reg().Get<components::SpellSeed>(entity);
	seed.flags = 1;
	seed.powerUp = 1;
	seed.hasCast = true;
	seed.learnedFrom = true;
	seed.fromOneShot = true;
	seed.chantStore = 700.0f;
	seed.chantStoreCopy = 700.0f;
	seed.storedChants = -1.0f;
	seed.storedMaxObjects = -1;
	seed.storedAge = 2.5f;
	seed.castMultiplier = 7.0f;
	seed.power = 0.5f;
	seed.ready = true;
	seed.turnsInHand = 28;
	seed.lastMagic = MagicType::Heal;
	seed.madeAt = {.x = 0x10000, .z = 0x20000, .altitude = 0.0f};

	const auto record = Written(entity);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->object.info, 2571u);
	EXPECT_EQ(record->info, 2571u);
	EXPECT_EQ(record->object.coords, (gamesave::MapCoords {.x = 0x10000, .z = 0x20000, .altitude = 0.0f}));
	EXPECT_EQ(record->word, 1u);
	EXPECT_EQ(record->byte, 1u);
	EXPECT_EQ(record->objects[0].number, 0u);
	EXPECT_EQ(record->objects[1].number, _context->Reference(SourceKey {.type = gamesave::k_PlayerType, .id = 1}));
	EXPECT_EQ(record->objects[2].number, 0u);
	EXPECT_EQ(record->objects[3].number,
	          _context->Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type, .id = 1}));
	EXPECT_EQ(record->bytes, (std::array<uint8_t, 2> {1, 1}));
	EXPECT_EQ(record->words[0], std::bit_cast<uint32_t>(700.0f));
	EXPECT_EQ(record->words[1], std::bit_cast<uint32_t>(700.0f));
	EXPECT_EQ(record->words[2], std::bit_cast<uint32_t>(-1.0f));
	EXPECT_EQ(record->words[3], std::bit_cast<uint32_t>(2.5f));
	EXPECT_EQ(record->words[4], 0xFFFFFFFFu);
	EXPECT_EQ(record->words[5], std::bit_cast<uint32_t>(0.5f));
	EXPECT_EQ(record->words[6], std::bit_cast<uint32_t>(7.0f));
	EXPECT_EQ(record->flag, 1u);
	EXPECT_EQ(record->lastWords[0], 28u);
	EXPECT_EQ(record->lastWords[1], static_cast<uint32_t>(MagicType::Heal));
	EXPECT_EQ(record->lastFlag, 1u);
}

// A seed a worship icon made has the icon as its creator as well as its icon; a loaded seed keeps the place it was made
// at
TEST_F(SpellSeedWrite, AnIconsSeedNamesItsIconAsItsCreatorAndALoadedOneKeepsItsPlace)
{
	const auto icon = Reg().Create();
	Reg().Assign<components::LoadedRecord<gamesave::WorshipSpellIconRecord>>(icon, gamesave::WorshipSpellIconRecord {});
	const auto entity =
	    Seed(SpellSeedType::Storm,
	         {.kind = components::SpellCreator::Kind::WorshipSpellIcon, .player = PlayerNames::PLAYER_ONE, .entity = icon});
	Reg().Get<components::SpellSeed>(entity).icon = icon;
	SpellSeedRecord kept;
	kept.object.coords = {.x = 0x10000, .z = 0x20000, .altitude = 0.0f};
	Reg().Assign<components::LoadedRecord<SpellSeedRecord>>(entity, kept);
	const auto record = Written(entity);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->info, 2578u);
	EXPECT_EQ(record->objects[0].number, _context->Reference(icon));
	EXPECT_EQ(record->objects[1].number, _context->Reference(icon));
	EXPECT_EQ(record->object.coords, kept.object.coords);
}

// A seed of no seed type, or whose creator is a creature or another thing (whose objects in the record are not read),
// stops the write and says which
TEST_F(SpellSeedWrite, ASeedOfNoTypeOrOfAnUnreadCreatorStopsTheWrite)
{
	std::string failure;
	const auto none = Seed(SpellSeedType::None, {.kind = components::SpellCreator::Kind::Player});
	EXPECT_FALSE(Written(none, &failure).has_value());
	EXPECT_NE(failure.find("no seed type"), std::string::npos) << failure;
	const auto creature = Seed(SpellSeedType::Food, {.kind = components::SpellCreator::Kind::Creature});
	EXPECT_FALSE(Written(creature, &failure).has_value());
	EXPECT_NE(failure.find("creator"), std::string::npos) << failure;
}

// A seed made into the hand from the hand's place (the one-shot orb's) is made at the hand's place on the map: x and z
// times 6553.6 truncated toward zero, at altitude 0, whatever the hand's height
TEST(SpellSeedMadeAt, ASeedMadeIntoTheHandIsAtTheHandsPlaceAtAltitudeZero)
{
	const auto at = magic::seed::MadeAtHand(glm::vec3(1.0f, 7.0f, -1.0f));
	EXPECT_EQ(at.x, 6553);
	EXPECT_EQ(at.z, -6553);
	EXPECT_EQ(at.altitude, 0.0f);
	const auto far = magic::seed::MadeAtHand(glm::vec3(100.05f, 3.0f, 200.3f));
	EXPECT_EQ(far.x, map_coords::ToFixed(100.05f));
	EXPECT_EQ(far.z, map_coords::ToFixed(200.3f));
}
