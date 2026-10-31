/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spells in a saved game's world file, in the original's order, written from their entities, and the game's list
// of spells (docs/wiki/engine/saved-games.md#openblack, the spells): the plain spell, the storm and the tornado, the
// healing, the food and wood, the forest, the teleport and the water spells.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellResource.h"
#include "Magic/Spells/SpellWater.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SpellWaterRecord;

namespace
{
/// The two words every spell writes before its first objects
constexpr uint32_t k_FirstMark = 0x12C859;
constexpr uint32_t k_SecondMark = 0x2A1C9A;
/// The places in a spell's record of its six words and its four objects
constexpr size_t k_Chants = 0;
constexpr size_t k_InitialChants = 1;
constexpr size_t k_ClosedDown = 2;
constexpr size_t k_MyInterfaceCasting = 3;
constexpr size_t k_CreatureCasting = 4;
constexpr size_t k_HumanPlayerCasting = 5;
constexpr size_t k_Creator = 0;
constexpr size_t k_Player = 1;
constexpr size_t k_Status = 2;
constexpr size_t k_Seed = 3;
constexpr size_t k_Age = 0;
constexpr size_t k_Magnitude = 1;
constexpr size_t k_StrengthMultiplier = 2;
constexpr size_t k_ManaPathPerTurn = 0;
constexpr size_t k_ManaPathPerEvent = 1;
/// The three bytes after the process info, which are not read
constexpr size_t k_ProcessInfoRead = 57;

std::string NameOf(entt::entity entity)
{
	return "spell " + std::to_string(entt::to_integral(entity));
}

/// A place in metres as the map coordinates the game keeps, with the altitude as it is
gamesave::MapCoords CoordsOf(const glm::vec3& metres)
{
	return {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.z), .altitude = metres.y};
}

/// Map coordinates written as their 12 bytes
gamesave::Point RawPointOf(const gamesave::MapCoords& coords)
{
	return {std::bit_cast<float>(coords.x), std::bit_cast<float>(coords.z), coords.altitude};
}

/// What every spell writes, from its component: its place, its effect's particle interface, its reaction, its
/// movement's direction, the place it is cast at, its direction, its chants and those it started with, whether it is
/// closed down, cast by this machine's interface, by a creature and by a human player, its duration, its process info
/// (the bytes after it as kept, 0 for one made in play), whether it is free, a flag nothing writes (0), its creator,
/// its player, the interface status of the hand that cast it (its creator's player's, for a spell cast from the hand),
/// its seed, its magic type, its age, magnitude and strength multiplier, the place it was first cast at and the two
/// counts of its mana path. None, and the write fails, for a spell with a reaction (the reactions are not written from
/// openblack yet)
std::optional<gamesave::SpellFields> SpellFieldsOf(WriteContext& context, entt::entity entity,
                                                   const gamesave::SpellFields* kept)
{
	const auto& registry = context.GetRegistry();
	const auto& spell = registry.Get<const ecs::components::Spell>(entity);
	if (spell.reaction != 0)
	{
		context.Fail(NameOf(entity) + " has a reaction, whose record is not written from openblack yet");
		return std::nullopt;
	}
	gamesave::SpellFields fields = kept != nullptr ? *kept : gamesave::SpellFields {};
	fields.position.position = CoordsOf(spell.position);
	fields.position.flags = PositionFlagsOf(registry, entity, kept != nullptr ? kept->position.flags : uint16_t {0});
	fields.word = k_FirstMark;
	fields.object = {spell.effect != 0
	                     ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = spell.effect})
	                     : 0};
	fields.secondWord = k_SecondMark;
	fields.other = {0};
	fields.points[0] = {spell.movementDirection.x, spell.movementDirection.y, spell.movementDirection.z};
	fields.points[1] = RawPointOf(CoordsOf(spell.castPos));
	fields.points[2] = {spell.direction.x, spell.direction.y, spell.direction.z};
	fields.words[k_Chants] = std::bit_cast<uint32_t>(spell.chants);
	fields.words[k_InitialChants] = std::bit_cast<uint32_t>(spell.initialChants);
	fields.words[k_ClosedDown] = spell.closedDown ? 1 : 0;
	fields.words[k_MyInterfaceCasting] = spell.isMyInterfaceCasting ? 1 : 0;
	fields.words[k_CreatureCasting] = spell.isCreatureCasting ? 1 : 0;
	fields.words[k_HumanPlayerCasting] = spell.isHumanPlayerCasting ? 1 : 0;
	const auto info = ProcessInfoBytes(spell.processInfo);
	std::copy_n(info.begin(), k_ProcessInfoRead, fields.values.begin());
	fields.value = spell.duration;
	fields.flag = spell.free ? 1 : 0;
	fields.secondFlag = 0;
	using Kind = ecs::components::SpellCreator::Kind;
	switch (spell.creator.kind)
	{
	case Kind::Player:
		fields.objects[k_Creator] = {PlayerObject(context, static_cast<int>(spell.creator.player))};
		break;
	case Kind::None:
		fields.objects[k_Creator] = {0};
		break;
	default:
		fields.objects[k_Creator] = {context.Reference(spell.creator.entity)};
		break;
	}
	fields.objects[k_Player] = {spell.hasPlayer ? PlayerObject(context, static_cast<int>(spell.player)) : 0};
	fields.objects[k_Status] = {spell.castFromInterface
	                                ? context.Reference(SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type,
	                                                               .id = static_cast<uint64_t>(spell.creator.player)})
	                                : 0};
	fields.objects[k_Seed] = {context.Reference(spell.seed)};
	fields.thirdWord = static_cast<uint32_t>(spell.magicType);
	fields.floats[k_Age] = spell.age;
	fields.floats[k_Magnitude] = spell.magnitude;
	fields.floats[k_StrengthMultiplier] = spell.strengthMultiplier;
	fields.place = RawPointOf(CoordsOf(spell.originalCastPos));
	fields.lastFloats[k_ManaPathPerTurn] = spell.manaPathPerTurn;
	fields.lastFloats[k_ManaPathPerEvent] = spell.manaPathPerEvent;
	return fields;
}

/// A spell's record of the class: every spell's values, then the class's own, which `own` puts in; none, and the
/// write fails, when either cannot be written
template <typename Record, typename Own>
std::optional<gamesave::SavedRecord> WriteSpellOf(WriteContext& context, entt::entity entity, Own own)
{
	std::optional<Record> kept;
	if (auto record = context.KeptRecordOf(entity, Record::k_Type); record.has_value())
	{
		kept = std::get<Record>(std::move(*record));
	}
	auto spell = SpellFieldsOf(context, entity, kept.has_value() ? &kept->spell : nullptr);
	if (!spell.has_value())
	{
		return std::nullopt;
	}
	Record record = kept.value_or(Record {});
	record.spell = std::move(*spell);
	own(context, entity, record);
	return record;
}

/// Registers the writer of the record for the spells of the class
template <typename Record, typename Own>
void RegisterClass(ClassWriters& writers, ecs::components::SpellClass spellClass, Own own)
{
	writers.Register(
	    Record::k_Type,
	    [spellClass](const ecs::Registry& registry, entt::entity entity) {
		    const auto* spell = registry.TryGet<ecs::components::Spell>(entity);
		    return spell != nullptr && spell->spellClass == spellClass;
	    },
	    [own](WriteContext& context, entt::entity entity) { return WriteSpellOf<Record>(context, entity, own); });
}

/// The classes that write nothing of their own
void NothingOwn(WriteContext& /*context*/, entt::entity /*entity*/, auto& /*record*/) {}

/// The water spell's own: the age it last made a ring at
void WaterOwn(WriteContext& context, entt::entity entity, SpellWaterRecord& record)
{
	const auto* water = context.GetRegistry().TryGet<magic::water::SpellWaterData>(entity);
	record.word = std::bit_cast<uint32_t>(water != nullptr ? water->lastRipple : 0.0f);
}

/// The food and wood spells' own: whether the first grain has landed (it pays the first event's amount)
void ResourceOwn(WriteContext& context, entt::entity entity, gamesave::SpellResourceRecord& record)
{
	const auto* resource = context.GetRegistry().TryGet<magic::SpellResourceData>(entity);
	record.byte = resource != nullptr && resource->firstDone ? 1 : 0;
}

/// The forest spell's own: whether it has made its forest, the forest it made while it stands (by the forest's key)
/// and the most trees it makes
void ForestOwn(WriteContext& context, entt::entity entity, gamesave::SpellForestRecord& record)
{
	const auto* forest = context.GetRegistry().TryGet<magic::SpellForestData>(entity);
	const magic::SpellForestData none {};
	const auto& data = forest != nullptr ? *forest : none;
	record.byte = data.forestCreated ? 1 : 0;
	record.object = {data.forestId != 0 && !data.forestDeleted
	                     ? context.Reference(SourceKey {.type = gamesave::ForestRecord::k_Type, .id = data.forestId})
	                     : 0};
	record.word = static_cast<uint32_t>(data.maxTrees);
}

/// The teleport's own: its objects (its stones), the newest first, as its list holds them
void ObjectsOwn(WriteContext& context, entt::entity entity, gamesave::SpellTeleportRecord& record)
{
	record.objects.numbers.clear();
	for (const auto object : magic::spell_objects::Objects(entity))
	{
		record.objects.numbers.push_back(context.Reference(object));
	}
}
} // namespace

void openblack::ecs::saved_world_write::RegisterSpellWriters(ClassWriters& writers)
{
	using components::SpellClass;
	RegisterClass<gamesave::SpellRecord>(
	    writers, SpellClass::General,
	    [](WriteContext& context, entt::entity entity, gamesave::SpellRecord& record) { NothingOwn(context, entity, record); });
	RegisterClass<gamesave::SpellStormAndTornadoRecord>(
	    writers, SpellClass::StormAndTornado,
	    [](WriteContext& context, entt::entity entity, gamesave::SpellStormAndTornadoRecord& record) {
		    NothingOwn(context, entity, record);
	    });
	RegisterClass<gamesave::SpellHealRecord>(writers, SpellClass::Heal,
	                                         [](WriteContext& context, entt::entity entity, gamesave::SpellHealRecord& record) {
		                                         NothingOwn(context, entity, record);
	                                         });
	RegisterClass<gamesave::SpellResourceRecord>(writers, SpellClass::Resource, ResourceOwn);
	RegisterClass<gamesave::SpellForestRecord>(writers, SpellClass::Forest, ForestOwn);
	RegisterClass<gamesave::SpellTeleportRecord>(writers, SpellClass::Teleport, ObjectsOwn);
	RegisterClass<SpellWaterRecord>(writers, SpellClass::Water, WaterOwn);
	// the game's spells: the live ones, the newest first, then those the load kept, as the loaded file listed them
	writers.RegisterList(GlobalList::Spells, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto spell : magic::Spells())
		{
			numbers.push_back(context.Reference(spell));
		}
		if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
		{
			const auto& kept = game->lists.at(static_cast<size_t>(GlobalList::Spells)).numbers;
			numbers.insert(numbers.end(), kept.begin(), kept.end());
		}
		return numbers;
	});
}
