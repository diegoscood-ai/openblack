/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town centres' and the worship sites' spell icons written from their entities: the parts of a fixed object of
// several cells, the seed's row, the place it was made at and the specular colour, then each class's own values; a
// loaded icon keeps what openblack does not model, and an icon of no seed or with a specular colour of openblack's
// stops the write (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <variant>

#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorshipRecords.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Locator.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SavedRecord;
using gamesave::TownCentreSpellIconRecord;
using gamesave::WorshipSpellIconRecord;
namespace components = openblack::ecs::components;

namespace
{
class SpellIconWrite: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<test::FakeFires>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
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

	/// An icon of the table row and seed at a place
	entt::entity Icon(uint8_t infoIndex, SpellSeedType seed)
	{
		const auto icon = Reg().Create();
		Reg().Assign<components::Transform>(icon, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<components::SpellIcon>(icon, components::SpellIcon {.infoIndex = infoIndex, .seedType = seed});
		return icon;
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
		RegisterSpellIconWriters(writers);
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
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// A town centre's icon made in play: the parts of a fixed object of several cells of the towns' icon row, its seed's
// row (the storm's is 2578, the fire's two rows down), the place it was made at as its 12 bytes, no specular colour,
// its town, its town centre and the power-up levels the town holds
TEST_F(SpellIconWrite, ATownCentresIconMadeInPlayIsWrittenFromItsEntity)
{
	const auto town = Loaded<gamesave::TownRecord>();
	const auto centre = Loaded<gamesave::TownCentreRecord>();
	const auto icon = Icon(1, SpellSeedType::Fire);
	Reg().Assign<components::TownCentreSpellIcon>(
	    icon, components::TownCentreSpellIcon {.town = town, .townCentre = centre, .slot = 2, .powerUps = {true, false, true}});
	const auto record = Written<TownCentreSpellIconRecord>(icon);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->icon.base.object.info, 2395u);
	EXPECT_EQ(record->icon.info, 2576u);
	const auto& at = record->icon.base.position.position;
	EXPECT_EQ(std::bit_cast<int32_t>(record->icon.point[0]), at.x);
	EXPECT_EQ(std::bit_cast<int32_t>(record->icon.point[1]), at.z);
	EXPECT_EQ(record->icon.point[2], at.altitude);
	EXPECT_EQ(record->icon.word, 0u);
	EXPECT_EQ(record->town.number, _context->Reference(town));
	EXPECT_EQ(record->object.number, _context->Reference(centre));
	EXPECT_EQ(record->words.values, (std::vector<uint32_t> {1, 0, 1}));
	// the parts are every fixed object's of several cells, made constructed
	auto parts = MultiMapFixedFieldsOf(*_context, icon, nullptr, 2395);
	parts.multi.fixedFlags = 8;
	EXPECT_TRUE(record->icon.base == parts);
}

// A worship site's icon made in play: its removal countdown, site, scale before the removal, the turn it started
// charging, its slot, whether it charges, the power-up level, the status of the interface that charges it (the
// charging player's), its charge and its seeds
TEST_F(SpellIconWrite, AWorshipSitesIconMadeInPlayIsWrittenFromItsEntity)
{
	const auto site = Loaded<gamesave::WorshipSiteRecord>();
	const auto icon = Icon(0, SpellSeedType::Heal);
	Reg().Assign<components::WorshipSpellIcon>(icon, components::WorshipSpellIcon {.site = site,
	                                                                               .removeTimer = 3,
	                                                                               .savedScale = 1.5f,
	                                                                               .charging = true,
	                                                                               .powerUp = 1,
	                                                                               .chargingFor = PlayerNames::PLAYER_TWO,
	                                                                               .hasChargingInterface = true,
	                                                                               .seeds = {},
	                                                                               .chantStore = 700.0f,
	                                                                               .chargeStartTurn = 1582,
	                                                                               .slot = 15});
	const auto record = Written<WorshipSpellIconRecord>(icon);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->icon.base.object.info, 2396u);
	EXPECT_EQ(record->icon.info, 2571u);
	EXPECT_EQ(record->half, 3u);
	EXPECT_EQ(record->object.number, _context->Reference(site));
	EXPECT_EQ(record->words[0], std::bit_cast<uint32_t>(1.5f));
	EXPECT_EQ(record->words[1], 1582u);
	EXPECT_EQ(record->secondHalf, 15u);
	EXPECT_EQ(record->byte, 1u);
	EXPECT_EQ(record->thirdWord, 1u);
	EXPECT_EQ(record->other.number, _context->Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type, .id = 1}));
	EXPECT_EQ(record->fourthWord, std::bit_cast<uint32_t>(700.0f));
	EXPECT_TRUE(record->objects.numbers.empty());
}

// A loaded icon keeps the place it was made at and its specular colour, and the parts' values openblack does not
// model, as its load kept them
TEST_F(SpellIconWrite, ALoadedIconKeepsWhatOpenblackDoesNotModel)
{
	const auto town = Loaded<gamesave::TownRecord>();
	const auto centre = Loaded<gamesave::TownCentreRecord>();
	const auto icon = Icon(1, SpellSeedType::Storm);
	Reg().Assign<components::TownCentreSpellIcon>(icon, components::TownCentreSpellIcon {.town = town, .townCentre = centre});
	TownCentreSpellIconRecord kept;
	kept.icon.point = {1.0f, 2.0f, 3.0f};
	kept.icon.word = 9;
	kept.icon.base.multi.fixedFlags = 8;
	Reg().Assign<components::LoadedRecord<TownCentreSpellIconRecord>>(icon, kept);
	const auto record = Written<TownCentreSpellIconRecord>(icon);
	ASSERT_TRUE(record.has_value()) << _context->Failure();
	EXPECT_EQ(record->icon.point, (gamesave::Point {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(record->icon.word, 9u);
	EXPECT_EQ(record->icon.base.multi.fixedFlags, 8u);
	EXPECT_EQ(record->icon.info, 2578u);
}

// An icon of no seed, or with a specular colour of openblack's (whose word in a save is not read), stops the write
// and says which
TEST_F(SpellIconWrite, AnIconOfNoSeedOrWithASpecularColourStopsTheWrite)
{
	const auto town = Loaded<gamesave::TownRecord>();
	const auto none = Icon(1, SpellSeedType::None);
	Reg().Assign<components::TownCentreSpellIcon>(none, components::TownCentreSpellIcon {.town = town});
	std::string failure;
	EXPECT_FALSE(Written<TownCentreSpellIconRecord>(none, &failure).has_value());
	EXPECT_NE(failure.find("has no seed"), std::string::npos) << failure;

	const auto lit = Icon(1, SpellSeedType::Food);
	Reg().Assign<components::TownCentreSpellIcon>(lit, components::TownCentreSpellIcon {.town = town});
	Reg().Assign<components::SpecularColour>(lit);
	EXPECT_FALSE(Written<TownCentreSpellIconRecord>(lit, &failure).has_value());
	EXPECT_NE(failure.find("specular colour"), std::string::npos) << failure;
}
