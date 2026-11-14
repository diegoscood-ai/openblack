/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The abodes', the creches', the graveyards', the storage pits' and the spell dispensers' records of a saved game's
// world file, in the original's order (the abode's save, and the graveyard's, the storage pit's and the dispenser's
// after it; docs/wiki/engine/saved-games.md#writing-the-world-file); the storage pits' piles are the pots' writer's.
// Each value openblack holds comes from the entity; every value whose meaning is not read, or that openblack does not
// hold, comes from the record the entity's load kept, and is zero for an object made in play.

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <variant>

#include <BuildingRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/Wonders.h"
#include "ECS/WaterQueries.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::Abode;
using ecs::components::LoadedRecord;
using ecs::components::StoragePit;
using gamesave::AbodeRecord;
using gamesave::CrecheRecord;
using gamesave::FieldRecord;
using gamesave::GraveyardRecord;
using gamesave::SavedRecord;
using gamesave::SpellDispenserRecord;
using gamesave::StoragePitRecord;
using gamesave::TotemStatueRecord;
using gamesave::TownCentreRecord;
using gamesave::WonderRecord;
using gamesave::WorkshopRecord;

namespace
{
template <typename Record>
[[nodiscard]] std::optional<Record> KeptAs(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return std::nullopt;
}

/// The kind of building an abode's row makes; none for an abode made with no row
[[nodiscard]] std::optional<AbodeType> AbodeTypeOf(const Abode& abode)
{
	if (abode.info == AbodeInfo::None || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	return Locator::infoConstants::value().abode.at(static_cast<size_t>(abode.info)).abodeType;
}

/// Whether the abode's load kept the record of one of the seven abode classes written here
[[nodiscard]] bool LoadedAsAnAbodeClass(const ecs::Registry& registry, entt::entity entity)
{
	return registry.AnyOf<LoadedRecord<AbodeRecord>, LoadedRecord<CrecheRecord>, LoadedRecord<GraveyardRecord>,
	                      LoadedRecord<StoragePitRecord>, LoadedRecord<FieldRecord>, LoadedRecord<TownCentreRecord>,
	                      LoadedRecord<WorkshopRecord>>(entity);
}

/// The kinds of building the game makes as a plain abode: every kind its abode maker does not hand to a class of its
/// own (the totem, the storage pit, the windmill, the creche, the workshop, the wonder, the town centre, the graveyard,
/// the football pitch, the field and the spell dispenser), the citadel being no abode the maker makes
[[nodiscard]] bool IsAPlainAbode(AbodeType type)
{
	switch (type)
	{
	case AbodeType::Totem:
	case AbodeType::StoragePit:
	case AbodeType::Windmill:
	case AbodeType::Creche:
	case AbodeType::Workshop:
	case AbodeType::Wonder:
	case AbodeType::TownCentre:
	case AbodeType::Graveyard:
	case AbodeType::FootballPitch:
	case AbodeType::Field:
	case AbodeType::SpellDispenser:
	case AbodeType::Citadel:
		return false;
	default:
		return true;
	}
}

/// An abode is of the class its load kept the record of; one made in play, of the class its row's kind makes
template <typename Record>
[[nodiscard]] ClassClaim AbodeClaim(bool (*ofTheKind)(AbodeType))
{
	return [ofTheKind](const ecs::Registry& registry, entt::entity entity) {
		const auto* abode = registry.TryGet<const Abode>(entity);
		if (abode == nullptr)
		{
			return false;
		}
		if (LoadedAsAnAbodeClass(registry, entity))
		{
			return registry.AllOf<LoadedRecord<Record>>(entity);
		}
		const auto type = AbodeTypeOf(*abode);
		return type.has_value() && ofTheKind(*type);
	};
}

/// The number of the abode's row in the game's tables: its own row's, else the one its record kept; none for an abode
/// made in play with no row
[[nodiscard]] std::optional<uint32_t> AbodeRowOf(const Abode& abode, const gamesave::MultiMapFixedFields* kept)
{
	if (abode.info != AbodeInfo::None)
	{
		return ecs::saved_world_load::AbodeInfoRowOf(abode.info);
	}
	if (kept != nullptr)
	{
		return kept->object.info;
	}
	return std::nullopt;
}

/// The abode's flags of one made in play: whether it found drinking water, whether its town's statistics counted it,
/// whether it is not to be planned again and whether a villager of it is in a hand; it is not yet smashed by a creature
/// nor losing life from neglect
[[nodiscard]] uint8_t AbodeFlagsOf(const ecs::Registry& registry, entt::entity entity, const Abode& abode)
{
	constexpr uint8_t k_HasDrinkingWater = 0x01;
	constexpr uint8_t k_AddedToTownStats = 0x02;
	constexpr uint8_t k_ShouldNotBeAddedToPlanned = 0x04;
	constexpr uint8_t k_VillagerInHand = 0x10;
	const auto* water = registry.TryGet<const ecs::water_queries::DrinkingWater>(entity);
	uint8_t flags = 0;
	flags |= water != nullptr && water->found ? k_HasDrinkingWater : 0;
	flags |= abode.addedToTownStats ? k_AddedToTownStats : 0;
	flags |= abode.shouldNotBeAddedToPlanned ? k_ShouldNotBeAddedToPlanned : 0;
	flags |= abode.heldVillagersHome ? k_VillagerInHand : 0;
	return flags;
}

/// What an abode adds to the parts of a building of several cells, in its save's order: its flags and its drinking
/// water (as kept for a loaded one; from what it holds for one made in play), a word (not read: as kept), its town, its
/// villagers with the adults, those at home and the children, a byte not read (as kept), its food and its wood, and its
/// destruction mesh (as kept: openblack keeps none). A loaded abode writes the villagers its record had without those gone
/// since, and its counts as it holds them; an abode made in play writes those it holds, the head of its list first
[[nodiscard]] gamesave::AbodeFields AbodeFieldsOf(WriteContext& context, entt::entity entity, const gamesave::AbodeFields* kept)
{
	const auto& abode = context.GetRegistry().Get<const Abode>(entity);
	gamesave::AbodeFields fields = kept != nullptr ? *kept : gamesave::AbodeFields {};
	fields.town.number = context.Reference(ecs::town_queries::TownByKey(abode.townId));
	if (kept == nullptr)
	{
		const auto& registry = context.GetRegistry();
		fields.abodeFlags = AbodeFlagsOf(registry, entity, abode);
		if (const auto* water = registry.TryGet<const ecs::water_queries::DrinkingWater>(entity); water != nullptr)
		{
			// in map units with no height, as every drinking water the original's saves hold
			fields.drinkingWater = {
			    .x = map_coords::ToFixed(water->position.x), .z = map_coords::ToFixed(water->position.z), .altitude = 0.0f};
		}
		for (const auto villager : abode.inhabitants)
		{
			fields.villagers.numbers.push_back(context.Reference(villager));
		}
		fields.adults = abode.adultCount;
		fields.presentAtHome = abode.presentAtHome;
		fields.children = abode.childCount;
	}
	else
	{
		// a loaded one's villagers as its record listed them, without those gone since, and its counts as they stand
		// (the load puts them back and a villager's death takes it out of them), as a villager leaves its abode as it
		// is deleted
		context.DropGone(fields.villagers);
		fields.adults = abode.adultCount;
		fields.presentAtHome = abode.presentAtHome;
		fields.children = abode.childCount;
	}
	// the counted array of its two resources, the food first
	fields.resources.values = {abode.foodAmount, abode.woodAmount};
	return fields;
}

/// An abode's record of the class: the parts of a building of several cells, then the abode's own
template <typename Record>
[[nodiscard]] std::optional<Record> AbodeRecordOf(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptAs<Record>(context, entity);
	Record record = kept.value_or(Record {});
	const auto& abode = context.GetRegistry().Get<const Abode>(entity);
	const auto row = AbodeRowOf(abode, kept.has_value() ? &kept->base : nullptr);
	if (!row.has_value())
	{
		context.Fail("abode " + std::to_string(entt::to_integral(entity)) + " was made with no row of the game's tables");
		return std::nullopt;
	}
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr, *row);
	// its build flags: built, under construction, not repaired
	record.base.multi.fixedFlags = static_cast<uint8_t>(abode.buildFlags);
	record.abode = AbodeFieldsOf(context, entity, kept.has_value() ? &kept->abode : nullptr);
	return record;
}

/// An abode, and a creche, which saves as an abode does
template <typename Record>
std::optional<SavedRecord> WriteAbode(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<Record>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	return *record;
}

/// A graveyard: the abode's record, then the dead buried there
std::optional<SavedRecord> WriteGraveyard(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<GraveyardRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	if (const auto* graveyard = context.GetRegistry().TryGet<const ecs::components::Graveyard>(entity))
	{
		record->graves = graveyard->dead;
	}
	return *record;
}

/// The number of the first field kind's row in the game's tables; the other kinds follow it downwards
constexpr uint32_t k_FirstFieldTypeRow = 1765;

/// A field: the abode's record, then its kind's row, a word only a load writes (as kept), its food, its town, its turn
/// offset, the crops sown, its growth, its farmers (the newest first; a loaded field's as kept) and a byte only a load
/// writes (as kept)
std::optional<SavedRecord> WriteField(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<FieldRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	// an abode of a field's kind made without its field's data (no Field component) writes no record, and the save is
	// refused for it, rather than reading data it does not have
	const auto* fieldData = context.GetRegistry().TryGet<const ecs::components::Field>(entity);
	if (fieldData == nullptr)
	{
		return std::nullopt;
	}
	const auto& field = *fieldData;
	record->typeInfo = k_FirstFieldTypeRow - static_cast<uint32_t>(field.type);
	record->secondWord = std::bit_cast<uint32_t>(field.food);
	record->town.number = context.Reference(ecs::town_queries::TownByKey(static_cast<uint32_t>(field.town)));
	record->thirdWord = field.turnOffset;
	record->firstByte = field.crops;
	record->fourthWord = std::bit_cast<uint32_t>(field.growth);
	// a loaded field's farmers as its record names them, as the load makes no farmer's place in it, without those gone
	// since (a farmer leaves its field as its farming ends, which its deletion ends); one made in play's those it holds
	if (!KeptAs<FieldRecord>(context, entity).has_value())
	{
		record->farmers.numbers.clear();
		for (const auto farmer : field.farmers)
		{
			record->farmers.numbers.push_back(context.Reference(farmer));
		}
	}
	else
	{
		context.DropGone(record->farmers);
	}
	return *record;
}

/// A town centre: the abode's record, its totem statue and its six spell icons by their slots
std::optional<SavedRecord> WriteTownCentre(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<TownCentreRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = context.GetRegistry();
	record->totemStatue.number = 0;
	registry.Each<const ecs::components::VillageTotem>(
	    [&context, &record, entity](entt::entity plinth, const ecs::components::VillageTotem& totem) {
		    if (totem.townCentre == entity && totem.onCentre)
		    {
			    record->totemStatue.number = context.Reference(plinth);
		    }
	    });
	constexpr size_t k_Icons = 6;
	record->icons.numbers.assign(k_Icons, 0);
	if (const auto* icons = registry.TryGet<const ecs::components::TownCentreIcons>(entity); icons != nullptr)
	{
		for (size_t slot = 0; slot < k_Icons; ++slot)
		{
			const auto icon = icons->icons.at(slot);
			record->icons.numbers.at(slot) = icon != entt::null && registry.Valid(icon) ? context.Reference(icon) : 0;
		}
	}
	return *record;
}

/// The number of the first tribe's totem statue row in the game's tables; the other tribes' follow it downwards
constexpr uint32_t k_FirstTotemStatueRow = 378;

/// A town centre's totem statue: the parts of a building of several cells of its tribe's row, its town centre, the
/// worship share it rises to, a word only a load writes (as kept, else 0) and the share the hand holds it at
std::optional<SavedRecord> WriteTotemStatue(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& totem = registry.Get<const ecs::components::VillageTotem>(entity);
	const auto kept = KeptAs<TotemStatueRecord>(context, entity);
	TotemStatueRecord record = kept.value_or(TotemStatueRecord {});
	const auto* centre = registry.Valid(totem.townCentre) ? registry.TryGet<const Abode>(totem.townCentre) : nullptr;
	if (!kept.has_value() && (centre == nullptr || centre->info == AbodeInfo::None || !Locator::infoConstants::has_value()))
	{
		context.Fail("totem statue " + std::to_string(entt::to_integral(entity)) + " has no town centre of a tribe");
		return std::nullopt;
	}
	const auto row =
	    kept.has_value()
	        ? kept->base.object.info
	        : k_FirstTotemStatueRow -
	              static_cast<uint32_t>(Locator::infoConstants::value().abode.at(static_cast<size_t>(centre->info)).tribeType);
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr, row);
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = static_cast<uint8_t>(Abode::k_Built);
	}
	record.base.multi.percentBuilt = totem.percentBuilt;
	record.object.number = context.Reference(totem.townCentre);
	const auto* worship = registry.TryGet<const ecs::components::TotemWorship>(entity);
	record.words.at(0) = std::bit_cast<uint32_t>(worship != nullptr ? worship->percentage : 0.0f);
	record.words.at(2) = std::bit_cast<uint32_t>(totem.held);
	return record;
}

/// A storage pit: the abode's record, then its food pile as an array of one and its five wood piles as an array of
/// five
std::optional<SavedRecord> WriteStoragePit(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<StoragePitRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto* pit = context.GetRegistry().TryGet<const StoragePit>(entity);
	if (pit == nullptr)
	{
		context.Fail("storage pit " + std::to_string(entt::to_integral(entity)) + " has no piles");
		return std::nullopt;
	}
	record->foodPile.numbers = {context.Reference(pit->foodPile)};
	record->otherPiles.numbers.clear();
	for (const auto pile : pit->woodPiles)
	{
		record->otherPiles.numbers.push_back(context.Reference(pile));
	}
	return *record;
}

/// A workshop: the abode's record, then its wood pile and its sign of the wood it wants, the turns left for the
/// scaffold being made and the scaffolds it owns, its three scaffold slots, and the scaffolds it holds, the head of its
/// list first
std::optional<SavedRecord> WriteWorkshop(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto* workshop = registry.TryGet<const ecs::components::Workshop>(entity);
	if (workshop == nullptr)
	{
		context.Fail("workshop " + std::to_string(entt::to_integral(entity)) + " has no workshop's values");
		return std::nullopt;
	}
	auto record = AbodeRecordOf<WorkshopRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto referenceOrNone = [&context, &registry](entt::entity object) {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : gamesave::ObjectNumber {0};
	};
	record->first.number = referenceOrNone(workshop->woodPile);
	record->second.number = referenceOrNone(workshop->showNeeds);
	record->firstWord = static_cast<uint32_t>(workshop->countdown);
	record->secondWord = static_cast<uint32_t>(workshop->owned);
	record->bytes.values.assign(workshop->slots.begin(), workshop->slots.end());
	record->objects.numbers.clear();
	for (const auto scaffold : workshop->scaffolds)
	{
		record->objects.numbers.push_back(referenceOrNone(scaffold));
	}
	return *record;
}

/// A spell dispenser: the abode's record, then the orb it made while it has one, the turns it has counted, the turns
/// between its orbs, whether it is active and the miracle it gives
std::optional<SavedRecord> WriteSpellDispenser(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(entity);
	if (dispenser == nullptr)
	{
		context.Fail("spell dispenser " + std::to_string(entt::to_integral(entity)) + " has no dispenser's values");
		return std::nullopt;
	}
	auto record = AbodeRecordOf<SpellDispenserRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto orb = dispenser->orb;
	record->object.number = orb != entt::null && registry.Valid(orb) ? context.Reference(orb) : gamesave::ObjectNumber {0};
	record->words = {dispenser->timer.tick, dispenser->timer.period, dispenser->timer.active ? 1U : 0U,
	                 static_cast<uint32_t>(dispenser->magicType)};
	return *record;
}

/// A wonder: the abode's record, then its power
std::optional<SavedRecord> WriteWonder(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<WonderRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	record->word = std::bit_cast<uint32_t>(ecs::wonders::GetPower(entity));
	return *record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterAbodeWriters(ClassWriters& writers)
{
	writers.Register(AbodeRecord::k_Type, AbodeClaim<AbodeRecord>(IsAPlainAbode), WriteAbode<AbodeRecord>);
	writers.Register(CrecheRecord::k_Type, AbodeClaim<CrecheRecord>([](AbodeType type) { return type == AbodeType::Creche; }),
	                 WriteAbode<CrecheRecord>);
	writers.Register(GraveyardRecord::k_Type,
	                 AbodeClaim<GraveyardRecord>([](AbodeType type) { return type == AbodeType::Graveyard; }), WriteGraveyard);
	writers.Register(FieldRecord::k_Type, AbodeClaim<FieldRecord>([](AbodeType type) { return type == AbodeType::Field; }),
	                 WriteField);
	writers.Register(TownCentreRecord::k_Type,
	                 AbodeClaim<TownCentreRecord>([](AbodeType type) { return type == AbodeType::TownCentre; }),
	                 WriteTownCentre);
	writers.Register(
	    TotemStatueRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) {
		    return registry.AllOf<ecs::components::VillageTotem>(entity);
	    },
	    WriteTotemStatue);
	writers.Register(WorkshopRecord::k_Type,
	                 AbodeClaim<WorkshopRecord>([](AbodeType type) { return type == AbodeType::Workshop; }), WriteWorkshop);
	writers.Register(StoragePitRecord::k_Type,
	                 AbodeClaim<StoragePitRecord>([](AbodeType type) { return type == AbodeType::StoragePit; }),
	                 WriteStoragePit);
	writers.Register(SpellDispenserRecord::k_Type,
	                 AbodeClaim<SpellDispenserRecord>([](AbodeType type) { return type == AbodeType::SpellDispenser; }),
	                 WriteSpellDispenser);
	writers.Register(WonderRecord::k_Type, AbodeClaim<WonderRecord>([](AbodeType type) { return type == AbodeType::Wonder; }),
	                 WriteWonder);
}
