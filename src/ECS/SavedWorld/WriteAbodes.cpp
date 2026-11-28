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

#include <algorithm>
#include <bit>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <BuildingRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <VillagerRecords.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FootballPitch.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
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
using ecs::components::BuildingDamage;
using ecs::components::LoadedRecord;
using ecs::components::StoragePit;
using gamesave::AbodeRecord;
using gamesave::CrecheRecord;
using gamesave::FieldRecord;
using gamesave::FootballPitchRecord;
using gamesave::GraveyardRecord;
using gamesave::PuzzleTotemRecord;
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

/// Whether the abode's load kept the record of one of the nine abode classes written here
[[nodiscard]] bool LoadedAsAnAbodeClass(const ecs::Registry& registry, entt::entity entity)
{
	return registry.AnyOf<LoadedRecord<AbodeRecord>, LoadedRecord<CrecheRecord>, LoadedRecord<GraveyardRecord>,
	                      LoadedRecord<StoragePitRecord>, LoadedRecord<FieldRecord>, LoadedRecord<TownCentreRecord>,
	                      LoadedRecord<WorkshopRecord>, LoadedRecord<FootballPitchRecord>, LoadedRecord<PuzzleTotemRecord>>(
	    entity);
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

/// The first abode of the loaded game's list of buildings that holds the villager (none when no loaded abode does):
/// the one the original's load reads the villager's list place from first
entt::entity FirstLoadedHolder(WriteContext& context, entt::entity villager)
{
	const auto* first = context.KeptWorldRecord(1);
	const auto* game = first != nullptr ? std::get_if<gamesave::GameHead>(first) : nullptr;
	if (game == nullptr)
	{
		return entt::null;
	}
	const auto& registry = context.GetRegistry();
	for (const auto number : game->lists.at(static_cast<size_t>(GlobalList::MultiMapFixed)).numbers)
	{
		const auto target = context.TargetOf(number);
		if (!target.has_value() || target->entity == entt::null || !registry.Valid(target->entity))
		{
			continue;
		}
		const auto* holder = registry.TryGet<const Abode>(target->entity);
		if (holder != nullptr && std::ranges::find(holder->inhabitants, villager) != holder->inhabitants.end())
		{
			return target->entity;
		}
	}
	return entt::null;
}

/// What an abode adds to the parts of a building of several cells, in its save's order: its flags and its drinking
/// water (as kept for a loaded one; from what it holds for one made in play), a word (not read: as kept), its town, its
/// villagers with the adults, those at home and the children, a byte not read (as kept), its food and its wood, and its
/// destruction mesh: the damage mesh it is drawn broken from, the one a load put back as its record kept it, none for
/// a building whole. Every abode writes the villagers it holds now: one made in play the
/// head of its list first; a loaded one its record's in their order, less those who moved out or died, with those who
/// moved in since at the head; and its counts as it holds them
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
		// a loaded one's villagers as it holds them now: its record's, in their order, without those gone or moved to
		// another abode since, so the game, whose villagers each have one abode, never finds one in two lists; those
		// who moved in since at the head, as each joins the head of the list
		context.DropGone(fields.villagers);
		// A villager two abodes hold (a save that listed it in two, loaded so) is written in one only: its own abode when
		// that one holds it too, else the first loaded abode of the list that holds it. One held by this abode alone stays
		// in it, whatever abode it names
		const auto& registry = context.GetRegistry();
		const auto holdsIt = [&registry](entt::entity holder, entt::entity villager) {
			const auto* other = holder != entt::null && registry.Valid(holder) ? registry.TryGet<const Abode>(holder) : nullptr;
			return other != nullptr && std::ranges::find(other->inhabitants, villager) != other->inhabitants.end();
		};
		const auto holds = [&abode, &registry, &context, &holdsIt, entity](entt::entity villager) {
			if (std::ranges::find(abode.inhabitants, villager) == abode.inhabitants.end())
			{
				return false;
			}
			const auto* own = registry.Valid(villager) ? registry.TryGet<const ecs::components::Villager>(villager) : nullptr;
			const auto ownAbode = own != nullptr ? own->abode : entt::entity {entt::null};
			if (ownAbode == entity)
			{
				return true;
			}
			// its own abode holds it as well: there only
			if (holdsIt(ownAbode, villager))
			{
				return false;
			}
			// its own abode does not hold it (or it names none): the first loaded abode of the list that holds it
			const auto first = FirstLoadedHolder(context, villager);
			return first == entt::null || first == entity;
		};
		std::vector<entt::entity> listed;
		std::erase_if(fields.villagers.numbers, [&context, &holds, &listed](gamesave::ObjectNumber number) {
			const auto target = context.TargetOf(number);
			if (!target.has_value() || target->entity == entt::null)
			{
				return false;
			}
			listed.push_back(target->entity);
			return !holds(target->entity);
		});
		std::vector<gamesave::ObjectNumber> movedIn;
		for (const auto villager : abode.inhabitants)
		{
			if (registry.Valid(villager) && holds(villager) && std::ranges::find(listed, villager) == listed.end())
			{
				// by its class: a named villager (one born since the load may be) is a special villager's record, any
				// other a villager's
				const bool named = registry.AllOf<ecs::components::NamedVillager>(villager);
				movedIn.push_back(context.Reference(villager, named ? gamesave::SpecialVillagerRecord::k_Type
				                                                    : gamesave::VillagerRecord::k_Type));
			}
		}
		fields.villagers.numbers.insert(fields.villagers.numbers.begin(), movedIn.begin(), movedIn.end());
		fields.adults = abode.adultCount;
		fields.presentAtHome = abode.presentAtHome;
		fields.children = abode.childCount;
	}
	// the counted array of its two resources, the food first
	fields.resources.values = {abode.foodAmount, abode.woodAmount};
	const auto* damage = context.GetRegistry().TryGet<const BuildingDamage>(entity);
	if ((damage == nullptr || !damage->mesh) && kept != nullptr &&
	    context.GetRegistry().AllOf<ecs::components::UnrestoredDestructionMesh>(entity))
	{
		// a mesh its load could not put back: as its record kept it
		fields.destructionMesh = kept->destructionMesh;
	}
	else if (damage == nullptr || !damage->mesh)
	{
		fields.destructionMesh = {};
	}
	else if (kept == nullptr || kept->destructionMesh.present == 0 || damage->mesh.get() != damage->loaded)
	{
		auto mesh = DestructionMeshOf(*damage->mesh);
		fields.destructionMesh = {.present = 1, .words = mesh.words, .pieces = std::move(mesh.pieces)};
	}
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

/// A football pitch: the abode's record, then its match as the pitch holds it: whether it is its town's, what the match
/// is doing and its clocks, the tallies, the places round it taken and the wave, the referee, both sides and their
/// rankings, the nearest attackers and defenders, the ball's fetcher, the watchers in the 40 places, the ball's heading
/// and the ball. A script's cup final's slots stay none (no town match fills them), and the values nothing but the
/// match's start writes stay as loaded, a new pitch's 0, as the game's zero-filled one
std::optional<SavedRecord> WriteFootballPitch(WriteContext& context, entt::entity entity)
{
	auto record = AbodeRecordOf<FootballPitchRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto* pitch = context.GetRegistry().TryGet<const ecs::components::FootballPitch>(entity);
	if (pitch == nullptr)
	{
		return *record;
	}
	const auto refOf = [&context](entt::entity thing) { return gamesave::Ref {.number = context.Reference(thing)}; };
	const auto listOf = [&context](const std::vector<entt::entity>& things) {
		gamesave::RefList list;
		for (const auto thing : things)
		{
			list.numbers.push_back(context.Reference(thing));
		}
		return list;
	};
	record->functional = pitch->functional ? 1 : 0;
	record->playState = static_cast<uint32_t>(pitch->playState);
	record->kickOffReady = pitch->kickOffReady ? 1 : 0;
	record->turns = pitch->waitingTurns;
	record->deadBallTurns = pitch->deadBallTurns;
	record->waveRest = pitch->waveRest;
	record->homeTally = pitch->homeScore;
	record->awayTally = pitch->awayScore;
	record->placesTaken = pitch->wavePlacesTaken;
	record->waveIndex = pitch->waveIndex;
	record->referee = refOf(pitch->referee);
	record->homeTeam = listOf(pitch->homeTeam);
	record->awayTeam = listOf(pitch->awayTeam);
	record->homeRanked = listOf(pitch->ranked[0]);
	record->awayRanked = listOf(pitch->ranked[1]);
	for (size_t side = 0; side < record->nearestAttackers.size(); ++side)
	{
		record->nearestAttackers.at(side) = refOf(pitch->nearestAttacker.at(side));
		record->nearestDefenders.at(side) = refOf(pitch->nearestDefender.at(side));
	}
	record->ballChaser = refOf(pitch->ballChaser);
	for (size_t place = 0; place < record->places.size() && place < pitch->wave.size(); ++place)
	{
		record->places.at(place) = refOf(pitch->wave.at(place));
	}
	record->ballHeading = {pitch->ballHeading.x, pitch->ballHeading.y, pitch->ballHeading.z};
	record->ball = refOf(pitch->ball);
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

/// A puzzle totem: the abode's record, then where the hand last pulled it to, where it was made, its highest step and
/// its height. Where it was made is its place with the altitude it stood at with all its height; where the hand pulled
/// it to is its place at the altitude the hand holds it at, once the hand has taken it (a loaded one the hand has not
/// taken since keeps the place its record had, a new one none)
std::optional<SavedRecord> WritePuzzleTotem(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptAs<PuzzleTotemRecord>(context, entity);
	auto record = AbodeRecordOf<PuzzleTotemRecord>(context, entity);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = context.GetRegistry();
	const auto& totem = registry.Get<const ecs::components::PuzzleTotem>(entity);
	const auto& coords = record->base.position.position;
	const float land = Locator::terrainSystem::has_value()
	                       ? Locator::terrainSystem::value().GetHeightAt(
	                             glm::vec2(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)))
	                       : 0.0f;
	record->madeAt = kept.has_value()
	                     ? kept->madeAt
	                     : gamesave::MapCoords {.x = coords.x, .z = coords.z, .altitude = land + totem.restAltitude};
	if (totem.held || totem.handAltitude != 0.0f)
	{
		record->pulledTo = {.x = coords.x, .z = coords.z, .altitude = land + totem.handAltitude};
	}
	else
	{
		record->pulledTo = kept.has_value() ? kept->pulledTo : gamesave::MapCoords {};
	}
	record->maxHeight = totem.maxHeight;
	record->height = totem.height;
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
	// each building waiting on the dead list is written with its thing's unavailable mark
	writers.Register(AbodeRecord::k_Type, AbodeClaim<AbodeRecord>(IsAPlainAbode),
	                 MarkedWhenUnavailable(WriteAbode<AbodeRecord>));
	writers.Register(CrecheRecord::k_Type, AbodeClaim<CrecheRecord>([](AbodeType type) { return type == AbodeType::Creche; }),
	                 MarkedWhenUnavailable(WriteAbode<CrecheRecord>));
	writers.Register(GraveyardRecord::k_Type,
	                 AbodeClaim<GraveyardRecord>([](AbodeType type) { return type == AbodeType::Graveyard; }),
	                 MarkedWhenUnavailable(WriteGraveyard));
	writers.Register(
	    PuzzleTotemRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) {
		    return registry.AllOf<Abode, ecs::components::PuzzleTotem>(entity);
	    },
	    MarkedWhenUnavailable(WritePuzzleTotem));
	writers.Register(FootballPitchRecord::k_Type,
	                 AbodeClaim<FootballPitchRecord>([](AbodeType type) { return type == AbodeType::FootballPitch; }),
	                 MarkedWhenUnavailable(WriteFootballPitch));
	writers.Register(FieldRecord::k_Type, AbodeClaim<FieldRecord>([](AbodeType type) { return type == AbodeType::Field; }),
	                 MarkedWhenUnavailable(WriteField));
	writers.Register(TownCentreRecord::k_Type,
	                 AbodeClaim<TownCentreRecord>([](AbodeType type) { return type == AbodeType::TownCentre; }),
	                 MarkedWhenUnavailable(WriteTownCentre));
	writers.Register(
	    TotemStatueRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) {
		    return registry.AllOf<ecs::components::VillageTotem>(entity);
	    },
	    MarkedWhenUnavailable(WriteTotemStatue));
	writers.Register(WorkshopRecord::k_Type,
	                 AbodeClaim<WorkshopRecord>([](AbodeType type) { return type == AbodeType::Workshop; }),
	                 MarkedWhenUnavailable(WriteWorkshop));
	writers.Register(StoragePitRecord::k_Type,
	                 AbodeClaim<StoragePitRecord>([](AbodeType type) { return type == AbodeType::StoragePit; }),
	                 MarkedWhenUnavailable(WriteStoragePit));
	writers.Register(SpellDispenserRecord::k_Type,
	                 AbodeClaim<SpellDispenserRecord>([](AbodeType type) { return type == AbodeType::SpellDispenser; }),
	                 MarkedWhenUnavailable(WriteSpellDispenser));
	writers.Register(WonderRecord::k_Type, AbodeClaim<WonderRecord>([](AbodeType type) { return type == AbodeType::Wonder; }),
	                 MarkedWhenUnavailable(WriteWonder));
}
