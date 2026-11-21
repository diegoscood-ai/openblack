/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The animals' and birds' records, written from what openblack holds: the parts every object and living thing writes,
// then the animal's own values (docs/wiki/engine/saved-games.md#the-animals-and-the-towns-desire-flags). An animal made
// in play is written in its kind's class, with what its constructor leaves where openblack holds nothing.

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <type_traits>
#include <variant>

#include <AnimalRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/ScriptMemory.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WriteLivingParts.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Spells/SpellFlock.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The animal rows: the first, the lion's, numbered 2548, the others following it downwards
constexpr uint32_t k_FirstAnimalRow = 2548;

/// The class an animal of the kind is made in: the cow to the wolf, and the dove to the vulture, as the game's class
/// factory makes them (the spells' and the puzzle games' animals are made by their own makers); none for another kind
std::optional<uint32_t> ClassOfKind(AnimalInfo kind)
{
	switch (kind)
	{
	case AnimalInfo::Cow:
		return gamesave::CowRecord::k_Type;
	case AnimalInfo::Sheep:
		return gamesave::SheepRecord::k_Type;
	case AnimalInfo::Goat:
		return gamesave::GoatRecord::k_Type;
	case AnimalInfo::Horse:
		return gamesave::HorseRecord::k_Type;
	case AnimalInfo::Zebra:
		return gamesave::ZebraRecord::k_Type;
	case AnimalInfo::Pig:
		return gamesave::PigRecord::k_Type;
	case AnimalInfo::Tortoise:
		return gamesave::TortoiseRecord::k_Type;
	case AnimalInfo::Lion:
		return gamesave::LionRecord::k_Type;
	case AnimalInfo::Leopard:
		return gamesave::LeopardRecord::k_Type;
	case AnimalInfo::Tiger:
		return gamesave::TigerRecord::k_Type;
	case AnimalInfo::Wolf:
		return gamesave::WolfRecord::k_Type;
	case AnimalInfo::Dove:
		return gamesave::DoveRecord::k_Type;
	case AnimalInfo::Crow:
		return gamesave::CrowRecord::k_Type;
	case AnimalInfo::Swallow:
		return gamesave::SwallowRecord::k_Type;
	case AnimalInfo::Pigeon:
		return gamesave::PigeonRecord::k_Type;
	case AnimalInfo::Seagull:
		return gamesave::SeagullRecord::k_Type;
	case AnimalInfo::Bat:
		return gamesave::BatRecord::k_Type;
	case AnimalInfo::Vulture:
		return gamesave::VultureRecord::k_Type;
	case AnimalInfo::SpellDove:
		return gamesave::SpellDoveRecord::k_Type;
	case AnimalInfo::SpellBat:
		return gamesave::SpellBatRecord::k_Type;
	case AnimalInfo::SpellWolf:
		return gamesave::SpellWolfRecord::k_Type;
	default:
		return std::nullopt;
	}
}

/// The class of record an animal's load kept on it, none for one made in play
std::optional<uint32_t> KeptClassOf(const ecs::Registry& registry, entt::entity entity)
{
	std::optional<uint32_t> found;
	const auto keptAs = [&registry, entity, &found]<typename Record>() {
		if (registry.AllOf<components::LoadedRecord<Record>>(entity))
		{
			found = Record::k_Type;
		}
	};
	[&keptAs]<typename... Records>(std::variant<Records...>*) {
		(
		    [&keptAs]<typename Record>() {
			    if constexpr (requires(Record record) { record.animal; })
			    {
				    keptAs.template operator()<Record>();
			    }
		    }.template operator()<Records>(),
		    ...);
	}(static_cast<gamesave::SavedRecordVariant*>(nullptr));
	return found;
}

/// What a spell's animal adds to an animal: a dove's or a bat's its fade (its alpha's zoomer); a wolf's its final
/// destination, its corridor (a plane: its normal across the run, 0 for its height, and its offset), the corridor's
/// half width, its fade and its player. Nothing for any other animal
template <typename Record>
void SpellPartsOf(WriteContext& context, entt::entity entity, Record& record)
{
	constexpr bool k_Bird =
	    std::is_same_v<Record, gamesave::SpellDoveRecord> || std::is_same_v<Record, gamesave::SpellBatRecord>;
	constexpr bool k_Wolf = std::is_same_v<Record, gamesave::SpellWolfRecord>;
	if constexpr (k_Bird || k_Wolf)
	{
		const auto& registry = context.GetRegistry();
		const auto* spell = registry.TryGet<const magic::SpellFlockAnimal>(entity);
		const magic::SpellFlockAnimal none {};
		const auto& data = spell != nullptr ? *spell : none;
		if constexpr (k_Bird)
		{
			record.spell = ecs::saved_zoomer::BytesOf(data.fade);
		}
		else
		{
			record.finalDestination = {
			    .x = data.finalDestination.x, .z = data.finalDestination.z, .altitude = data.finalDestination.altitude};
			const std::array<float, 4> plane {data.normal.x, 0.0f, data.normal.y, data.offset};
			record.corridor = std::bit_cast<std::array<uint8_t, 16>>(plane);
			record.halfWidth = data.halfWidth;
			record.fade = ecs::saved_zoomer::BytesOf(data.fade);
			const auto player = registry.Get<const components::Animal>(entity).player;
			record.player.number = player >= 0 ? PlayerReference(context, static_cast<PlayerNames>(player)) : 0;
		}
	}
}

/// An animal of the class: its parts as an object and a living thing, from what openblack holds, the rest as its load
/// kept it or as made; then its town, its needs, its sleeping cell, why it died and its tilt from its components, and
/// its other values as kept, or for one made in play as its constructor leaves them (all 0): the word +0xEC and the
/// physical object's place only its constructors write; the food place, the thing it pounced on and the surroundings'
/// place the hunts, the grazing and the search for its surroundings write, none of which openblack holds yet
template <typename Record>
std::optional<gamesave::SavedRecord> WriteAnimalRecord(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& animal = registry.Get<const components::Animal>(entity);
	// one that has had no turn yet is written with the values it was made with, as the original's holds them from its
	// making
	const auto brain = AnimalBrainOf(registry, entity);
	const bool madeOnly = !registry.AllOf<components::AnimalBrain>(entity);
	if (!brain.has_value())
	{
		context.Fail(fmt::format("animal {} has no place: no brain can be made for it", entt::to_integral(entity)));
		return std::nullopt;
	}
	std::optional<Record> kept;
	if (auto keptRecord = context.KeptRecordOf(entity, Record::k_Type); keptRecord.has_value())
	{
		kept = std::get<Record>(std::move(*keptRecord));
	}
	Record record = kept.value_or(Record {});
	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	// its place's map coordinates as loaded while it stands where its load put it: openblack holds its place in metres
	if (const auto* transform = registry.TryGet<const components::Transform>(entity);
	    kept.has_value() && transform != nullptr &&
	    map_coords::ToWorld(
	        {.x = kept->position.position.x, .z = kept->position.position.z, .altitude = kept->position.position.altitude}) ==
	        transform->position)
	{
		record.position.position = kept->position.position;
	}
	record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr,
	                             kept.has_value() ? kept->object.coords : record.position.position,
	                             k_FirstAnimalRow - static_cast<uint32_t>(animal.type));
	record.mobile = MobilePartOf(kept.has_value() ? &kept->mobile : nullptr);
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal.type));
	// and the life of its row, which openblack gives it at its first turn
	if (madeOnly && !registry.AllOf<components::Life>(entity))
	{
		record.object.life = info.life;
	}
	LivingParts keptParts;
	if (kept.has_value())
	{
		keptParts = {.mobile = kept->mobile, .move = kept->move, .living = kept->living};
	}
	const auto parts = LivingPartsOf(context, entity, kept.has_value() ? &keptParts : nullptr, LivingConstructorParts(info));
	if (!parts.has_value())
	{
		return std::nullopt;
	}
	record.mobile = parts->mobile;
	record.move = parts->move;
	record.living = parts->living;
	// its script's reminder: the one its script memory writes (only a load gives an animal one), none without one
	record.living.scriptReminder.number =
	    registry.AllOf<components::ScriptMemory>(entity)
	        ? context.Reference(SourceKey {.type = gamesave::ScriptReminderRecord::k_Type, .id = PartId(entity, 0)})
	        : gamesave::ObjectNumber {0};
	auto& fields = record.animal;
	fields.object.number = animal.town != entt::null && registry.Valid(animal.town) ? context.Reference(animal.town) : 0;
	fields.hunger = brain->hunger;
	fields.tiredness = brain->sleep;
	fields.breedingUrge = brain->breed;
	// its sleeping cell, two half words
	fields.sleepPosition = static_cast<uint32_t>(brain->sleepCell.x) | (static_cast<uint32_t>(brain->sleepCell.y) << 16U);
	fields.deathReason = static_cast<uint8_t>(animal.deathReason);
	// its tilt into a turn, as its brain holds it (a loaded animal's is cleared as its load makes it)
	fields.angleZoomer = ecs::saved_zoomer::BytesOf(brain->bank);
	SpellPartsOf(context, entity, record);
	return record;
}

/// Registers the writer of one class: the animals whose load kept a record of that class
template <typename Record>
void RegisterAnimalClass(ClassWriters& writers)
{
	writers.Register(
	    Record::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) {
		    if (!registry.AllOf<components::Animal>(entity))
		    {
			    return false;
		    }
		    const auto kept = KeptClassOf(registry, entity);
		    return (kept.has_value() ? kept : ClassOfKind(registry.Get<const components::Animal>(entity).type)) ==
		           Record::k_Type;
	    },
	    WriteAnimalRecord<Record>);
}
} // namespace

void openblack::ecs::saved_world_write::RegisterAnimalWriters(ClassWriters& writers)
{
	[&writers]<typename... Records>(std::variant<Records...>*) {
		(
		    [&writers]<typename Record>() {
			    if constexpr (requires(Record record) { record.animal; })
			    {
				    RegisterAnimalClass<Record>(writers);
			    }
		    }.template operator()<Records>(),
		    ...);
	}(static_cast<gamesave::SavedRecordVariant*>(nullptr));
}
