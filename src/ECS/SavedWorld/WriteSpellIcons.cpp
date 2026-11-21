/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town centres' and the worship sites' spell icons in a saved game's world file, in the original's order, written
// from their entities (docs/wiki/engine/saved-games.md#openblack, the spell icons).

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <SpellIconRecords.h>

#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::TownCentreSpellIconRecord;
using gamesave::WorshipSpellIconRecord;

namespace
{
/// The spell icon tables' rows, numbered from the first one's down: the worship sites' icon, then the towns'
constexpr uint32_t k_FirstSpellIconRow = 2396;
/// The spell seed tables' rows, numbered from the first one's (the storm's) down
constexpr uint32_t k_FirstSpellSeedRow = 2578;
/// A fixed object of several cells' flag of one made constructed, not as a building site
constexpr uint8_t k_Constructed = 0x08;
/// The power-up levels a town centre's icon writes
constexpr size_t k_PowerUps = 3;

/// The record of the class the entity's load kept on it, if any
template <typename Record>
std::optional<Record> KeptOf(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		if (auto* record = std::get_if<Record>(&*kept); record != nullptr)
		{
			return std::move(*record);
		}
	}
	return std::nullopt;
}

/// What every spell icon writes: the parts of a fixed object of several cells (of its icon table's row), its seed's
/// row (made in play: constructed), the place it was made at (as kept; one made since stands where it was made: nothing moves
/// an icon) and its specular colour (as kept, 0 for one made since). None, and the write fails, for an icon of no seed or with
/// a specular colour of openblack's, whose layout in the original's word is not read
std::optional<gamesave::SpellIconFields> IconFieldsOf(WriteContext& context, entt::entity entity,
                                                      const gamesave::SpellIconFields* kept)
{
	const auto& registry = context.GetRegistry();
	const auto& icon = registry.Get<const ecs::components::SpellIcon>(entity);
	if (icon.seedType == SpellSeedType::None)
	{
		context.Fail("spell icon " + std::to_string(entt::to_integral(entity)) + " has no seed");
		return std::nullopt;
	}
	if (registry.AllOf<ecs::components::SpecularColour>(entity))
	{
		context.Fail("spell icon " + std::to_string(entt::to_integral(entity)) +
		             " has a specular colour, whose word in a save is not read");
		return std::nullopt;
	}
	gamesave::SpellIconFields fields;
	fields.base =
	    MultiMapFixedFieldsOf(context, entity, kept != nullptr ? &kept->base : nullptr, k_FirstSpellIconRow - icon.infoIndex);
	if (kept == nullptr)
	{
		// made in play, not as a building site: constructed, as a fixed object of several cells is made
		fields.base.multi.fixedFlags = k_Constructed;
	}
	fields.info = SpellSeedInfoRow(icon.seedType);
	// the place is map coordinates, two whole numbers and the altitude, written as its 12 bytes
	const auto& at = fields.base.position.position;
	fields.point =
	    kept != nullptr ? kept->point : gamesave::Point {std::bit_cast<float>(at.x), std::bit_cast<float>(at.z), at.altitude};
	fields.word = kept != nullptr ? kept->word : 0;
	return fields;
}

/// A town centre's spell icon: its town, its town centre and the three power-up levels the town holds
std::optional<gamesave::SavedRecord> WriteTownCentreIcon(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptOf<TownCentreSpellIconRecord>(context, entity);
	auto icon = IconFieldsOf(context, entity, kept.has_value() ? &kept->icon : nullptr);
	if (!icon.has_value())
	{
		return std::nullopt;
	}
	const auto& ofTown = context.GetRegistry().Get<const ecs::components::TownCentreSpellIcon>(entity);
	TownCentreSpellIconRecord record = kept.value_or(TownCentreSpellIconRecord {});
	record.icon = std::move(*icon);
	record.town = {context.Reference(ofTown.town)};
	record.object = {context.Reference(ofTown.townCentre)};
	record.words.values.assign(k_PowerUps, 0);
	for (size_t level = 0; level < k_PowerUps; ++level)
	{
		record.words.values.at(level) = ofTown.powerUps.at(level) ? 1 : 0;
	}
	return record;
}

/// A worship site's spell icon: the countdown of its removal, its site, its scale before the removal, the turn it
/// started charging, its slot, whether it charges, the power-up level, the status of the interface that charges it,
/// its charge and the seeds made from it, the newest first
std::optional<gamesave::SavedRecord> WriteWorshipIcon(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptOf<WorshipSpellIconRecord>(context, entity);
	auto icon = IconFieldsOf(context, entity, kept.has_value() ? &kept->icon : nullptr);
	if (!icon.has_value())
	{
		return std::nullopt;
	}
	const auto& ofSite = context.GetRegistry().Get<const ecs::components::WorshipSpellIcon>(entity);
	WorshipSpellIconRecord record = kept.value_or(WorshipSpellIconRecord {});
	record.icon = std::move(*icon);
	record.half = static_cast<uint16_t>(ofSite.removeTimer);
	record.object = {context.Reference(ofSite.site)};
	record.words[0] = std::bit_cast<uint32_t>(ofSite.savedScale);
	record.words[1] = ofSite.chargeStartTurn;
	record.secondHalf = static_cast<uint16_t>(ofSite.slot);
	record.byte = ofSite.charging ? 1 : 0;
	record.thirdWord = static_cast<uint32_t>(ofSite.powerUp);
	record.other = {ofSite.hasChargingInterface ? context.Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type,
	                                                                           .id = static_cast<uint64_t>(ofSite.chargingFor)})
	                                            : 0};
	record.fourthWord = std::bit_cast<uint32_t>(ofSite.chantStore);
	record.objects.numbers.clear();
	for (const auto seed : ofSite.seeds)
	{
		record.objects.numbers.push_back(context.Reference(seed));
	}
	return record;
}
} // namespace

uint32_t openblack::ecs::saved_world_write::SpellSeedInfoRow(SpellSeedType type)
{
	return k_FirstSpellSeedRow - static_cast<uint32_t>(type);
}

void openblack::ecs::saved_world_write::RegisterSpellIconWriters(ClassWriters& writers)
{
	writers.Register(
	    TownCentreSpellIconRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::SpellIcon, components::TownCentreSpellIcon>(entity);
	    },
	    WriteTownCentreIcon);
	writers.Register(
	    WorshipSpellIconRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::SpellIcon, components::WorshipSpellIcon>(entity);
	    },
	    WriteWorshipIcon);
}
