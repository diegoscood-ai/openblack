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
// healing, the food and wood, the forest, the teleport, the water and the shield spells, and the physical and the
// spiritual shields' world objects.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Spells/SpellCreature.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellResource.h"
#include "Magic/Spells/SpellShield.h"
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
/// The physical shield's object row and its magic's info, as the original's saves give them (profile D's slot 16 and
/// slot 20 Land 2 saves and the original's own Land 2 save; docs/wiki/engine/saved-games.md, the shields)
constexpr uint32_t k_PhysicalShieldRow = 2083;
constexpr uint32_t k_PhysicalShieldMagicInfo = 2642;
/// The spiritual shield's object row and its magic's info: the rows before the physical shield's in their tables (each
/// table's rows take their ids from its first down; docs/wiki/engine/saved-games.md, the shields)
constexpr uint32_t k_SpiritualShieldRow = k_PhysicalShieldRow + 1;
constexpr uint32_t k_SpiritualShieldMagicInfo = k_PhysicalShieldMagicInfo + 1;
/// The physical shield's word that only its reset writes
constexpr float k_PhysicalShieldAlwaysOne = 1.0f;

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

/// A place a load kept as map coordinates, while it is still the place in metres the load made of it; else the place
/// as map coordinates (metres to map coordinates and back is not always the same coordinates, so a loaded spell
/// writes back the ones it was read with)
gamesave::MapCoords CoordsKeptOr(const glm::vec3& metres, const gamesave::MapCoords* kept)
{
	if (kept != nullptr && map_coords::ToMetres(kept->x) == metres.x && map_coords::ToMetres(kept->z) == metres.z &&
	    kept->altitude == metres.y)
	{
		return *kept;
	}
	return CoordsOf(metres);
}

/// Map coordinates read back from their 12 bytes
gamesave::MapCoords CoordsOfRaw(const gamesave::Point& point)
{
	return {.x = std::bit_cast<int32_t>(point[0]), .z = std::bit_cast<int32_t>(point[1]), .altitude = point[2]};
}

/// What every spell writes, from its component: its place, its effect's particle interface, its reaction, its
/// movement's direction, the place it is cast at, its direction, its chants and those it started with, whether it is
/// closed down, cast by this machine's interface, by a creature and by a human player, its duration, its process info
/// (the bytes after it as kept, 0 for one made in play), whether it is free, a flag nothing writes (0), its creator,
/// its player, the interface status of the hand that cast it (its creator's player's, for a spell cast from the hand),
/// its seed, its magic type, its age, magnitude and strength multiplier, the place it was first cast at and the two
/// counts of its mana path; a loaded spell's places are written as it was read while they stand. Its reaction is
/// named by its id, as the reactions' writer keys it (none once the reaction has gone)
gamesave::Ref ReactionRef(WriteContext& context, uint32_t reaction);
gamesave::SpellFields SpellFieldsOf(WriteContext& context, entt::entity entity, const gamesave::SpellFields* kept)
{
	const auto& registry = context.GetRegistry();
	const auto& spell = registry.Get<const ecs::components::Spell>(entity);
	gamesave::SpellFields fields = kept != nullptr ? *kept : gamesave::SpellFields {};
	fields.position.position = CoordsKeptOr(spell.position, kept != nullptr ? &kept->position.position : nullptr);
	fields.position.flags = PositionFlagsOf(registry, entity, kept != nullptr ? kept->position.flags : uint16_t {0});
	fields.word = k_FirstMark;
	fields.object = {spell.effect != 0
	                     ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = spell.effect})
	                     : 0};
	fields.secondWord = k_SecondMark;
	fields.other = ReactionRef(context, spell.reaction);
	fields.points[0] = {spell.movementDirection.x, spell.movementDirection.y, spell.movementDirection.z};
	const auto keptCast = kept != nullptr ? std::optional(CoordsOfRaw(kept->points[1])) : std::nullopt;
	fields.points[1] = RawPointOf(CoordsKeptOr(spell.castPos, keptCast.has_value() ? &*keptCast : nullptr));
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
	const auto keptPlace = kept != nullptr ? std::optional(CoordsOfRaw(kept->place)) : std::nullopt;
	fields.place = RawPointOf(CoordsKeptOr(spell.originalCastPos, keptPlace.has_value() ? &*keptPlace : nullptr));
	fields.lastFloats[k_ManaPathPerTurn] = spell.manaPathPerTurn;
	fields.lastFloats[k_ManaPathPerEvent] = spell.manaPathPerEvent;
	return fields;
}

/// A spell's record of the class: every spell's values, then the class's own, which `own` puts in
template <typename Record, typename Own>
std::optional<gamesave::SavedRecord> WriteSpellOf(WriteContext& context, entt::entity entity, Own own)
{
	std::optional<Record> kept;
	if (auto record = context.KeptRecordOf(entity, Record::k_Type); record.has_value())
	{
		kept = std::get<Record>(std::move(*record));
	}
	Record record = kept.value_or(Record {});
	record.spell = SpellFieldsOf(context, entity, kept.has_value() ? &kept->spell : nullptr);
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

/// A creature spell's own: the creature it was cast on, none once that creature is no longer available (its turn then
/// lets it go), and whether the caster's hand has poured onto it
void CreatureOwn(WriteContext& context, entt::entity entity, gamesave::SpellCreatureRecord& record)
{
	const auto* creature = context.GetRegistry().TryGet<magic::SpellCreatureData>(entity);
	const magic::SpellCreatureData none {};
	const auto& data = creature != nullptr ? *creature : none;
	const auto& registry = context.GetRegistry();
	const bool available = registry.Valid(data.creature) && !registry.AllOf<ecs::components::Unavailable>(data.creature);
	record.object = {available ? context.Reference(data.creature) : 0};
	record.byte = data.handPoured ? 1 : 0;
}

/// A spell with objects' own: its objects (the teleport's stones, the shield's world object), the newest first, as its
/// list holds them
template <typename Record>
void ObjectsOwn(WriteContext& context, entt::entity entity, Record& record)
{
	record.objects.numbers.clear();
	for (const auto object : magic::spell_objects::Objects(entity))
	{
		record.objects.numbers.push_back(context.Reference(object));
	}
}

/// A flock spell's own: its objects (its animals, the newest first), its flock (none once gone), the animals it has made,
/// its count of animals to emit, where it last let them out, and the word only its start sets (0)
template <typename Record>
void FlockOwn(WriteContext& context, entt::entity entity, Record& record)
{
	ObjectsOwn(context, entity, record);
	const auto& registry = context.GetRegistry();
	const auto* flock = registry.TryGet<ecs::components::FlockSpell>(entity);
	const ecs::components::FlockSpell none {};
	const auto& data = flock != nullptr ? *flock : none;
	record.flock.object.number = data.flock != entt::null && registry.Valid(data.flock)
	                                 ? context.Reference(data.flock, gamesave::FlockRecord::k_Type)
	                                 : 0;
	record.flock.words = {static_cast<uint32_t>(data.created), std::bit_cast<uint32_t>(data.emitted)};
	record.flock.lastSpawn = {.x = data.lastSpawn.x, .z = data.lastSpawn.y, .altitude = data.lastSpawnHeight};
	record.flock.lastWord = 0;
}

/// The flying flock spell's own: the flock's, then two words and a byte only its start sets (0)
void FlockFlyingOwn(WriteContext& context, entt::entity entity, gamesave::SpellFlockFlyingRecord& record)
{
	FlockOwn(context, entity, record);
	record.words = {0, 0};
	record.byte = 0;
}

/// The number of a reaction by its id, none for 0 or for a reaction that has gone: the game holds a miracle's reactions
/// by pointers that a reaction's deletion sets to none
gamesave::Ref ReactionRef(WriteContext& context, uint32_t reaction)
{
	const bool listed =
	    reaction != 0 && (!Locator::reactionSystem::has_value() || ecs::effects::reactions::Find(reaction) != nullptr);
	return {listed ? context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = reaction}) : 0};
}

/// The shield spell's own: its objects (its world shield), its anti-influence rings, the newest first, then its struck
/// reaction and its shield reaction
void ShieldOwn(WriteContext& context, entt::entity entity, gamesave::SpellShieldRecord& record)
{
	ObjectsOwn(context, entity, record);
	const auto* shield = context.GetRegistry().TryGet<magic::SpellShieldData>(entity);
	const magic::SpellShieldData none {};
	const auto& data = shield != nullptr ? *shield : none;
	record.shields.numbers.clear();
	for (const auto ring : data.rings)
	{
		record.shields.numbers.push_back(context.Reference(ring));
	}
	record.others[0] = ReactionRef(context, data.struckReaction);
	record.others[1] = ReactionRef(context, data.shieldReaction);
}

/// A physical shield's matrix as its 52 bytes: its rows, its translation and its scale, each cell a float
std::array<uint8_t, 52> ShieldMatrixBytes(const ecs::components::ShieldMatrix& matrix)
{
	std::array<float, 13> cells {};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t column = 0; column < 3; ++column)
		{
			cells.at(static_cast<size_t>(row * 3 + column)) = matrix.rows[row][column];
		}
	}
	cells[9] = matrix.translation.x;
	cells[10] = matrix.translation.y;
	cells[11] = matrix.translation.z;
	cells[12] = matrix.scale;
	std::array<uint8_t, 52> bytes {};
	std::memcpy(bytes.data(), cells.data(), bytes.size());
	return bytes;
}

/// A physical shield: a fixed object's parts at its place (its height above the land the place's altitude, its
/// object's scale the last rescale's, its heading 0), its spell, its magic's info, the word only its reset writes, its
/// creation turn, this turn's matrix and the last one's, its start and final scale, its start and end spin, its angle,
/// its bob, whether it is dying, its alpha, its die time and its effect's particle interface
std::optional<gamesave::SavedRecord> WritePhysicalShield(WriteContext& context, entt::entity entity)
{
	const auto& shield = context.GetRegistry().Get<const ecs::components::MagicShield>(entity);
	std::optional<gamesave::PhysicalShieldRecord> kept;
	if (auto record = context.KeptRecordOf(entity, gamesave::PhysicalShieldRecord::k_Type); record.has_value())
	{
		kept = std::get<gamesave::PhysicalShieldRecord>(std::move(*record));
	}
	gamesave::PhysicalShieldRecord record = kept.value_or(gamesave::PhysicalShieldRecord {});
	record.base = FixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr, k_PhysicalShieldRow);
	const auto place = CoordsKeptOr(shield.position, kept.has_value() ? &kept->base.position.position : nullptr);
	record.base.position.position = place;
	if (!kept.has_value())
	{
		// the place it was made at, the cast's on the land (altitude 0): only the place's altitude follows its height
		record.base.object.coords = {.x = place.x, .z = place.z, .altitude = 0.0f};
	}
	// the object's scale is the one its last rescale gave it, and it is never turned as an object: the matrix turns it
	record.base.object.scale = shield.objectScale;
	record.base.object.heading = 0.0f;
	record.owner = {context.Reference(shield.spell)};
	record.info = k_PhysicalShieldMagicInfo;
	record.words = {std::bit_cast<uint32_t>(k_PhysicalShieldAlwaysOne), shield.creationTurn};
	record.first = ShieldMatrixBytes(shield.matrix);
	record.second = ShieldMatrixBytes(shield.previousMatrix);
	record.secondWords = {std::bit_cast<uint32_t>(shield.startScale), std::bit_cast<uint32_t>(shield.finalScale),
	                      std::bit_cast<uint32_t>(shield.startSpin),  std::bit_cast<uint32_t>(shield.endSpin),
	                      std::bit_cast<uint32_t>(shield.angle),      std::bit_cast<uint32_t>(shield.bob)};
	record.bytes = {static_cast<uint8_t>(shield.dying ? 1 : 0), shield.alpha};
	record.lastWord = std::bit_cast<uint32_t>(shield.dieTime);
	record.object = {
	    shield.fx != 0 ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = shield.fx}) : 0};
	return record;
}

/// A spiritual shield: a fixed object's parts at its place (on the land, as it is made), its object's scale (0.017 of
/// its radius), its heading 0, its spell and its magic's info
std::optional<gamesave::SavedRecord> WriteSpiritualShield(WriteContext& context, entt::entity entity)
{
	const auto& shield = context.GetRegistry().Get<const ecs::components::MagicShield>(entity);
	std::optional<gamesave::SpiritualShieldRecord> kept;
	if (auto record = context.KeptRecordOf(entity, gamesave::SpiritualShieldRecord::k_Type); record.has_value())
	{
		kept = std::get<gamesave::SpiritualShieldRecord>(std::move(*record));
	}
	gamesave::SpiritualShieldRecord record = kept.value_or(gamesave::SpiritualShieldRecord {});
	record.base = FixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr, k_SpiritualShieldRow);
	const auto place = CoordsKeptOr(shield.position, kept.has_value() ? &kept->base.position.position : nullptr);
	record.base.position.position = place;
	if (!kept.has_value())
	{
		record.base.object.coords = {.x = place.x, .z = place.z, .altitude = 0.0f};
	}
	record.base.object.scale = shield.objectScale;
	record.base.object.heading = 0.0f;
	record.owner = {context.Reference(shield.spell)};
	record.info = k_SpiritualShieldMagicInfo;
	return record;
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
	RegisterClass<gamesave::SpellCreatureRecord>(writers, SpellClass::Creature, CreatureOwn);
	RegisterClass<gamesave::SpellTeleportRecord>(writers, SpellClass::Teleport, ObjectsOwn<gamesave::SpellTeleportRecord>);
	RegisterClass<SpellWaterRecord>(writers, SpellClass::Water, WaterOwn);
	RegisterClass<gamesave::SpellFlockFlyingRecord>(writers, SpellClass::FlockFlying, FlockFlyingOwn);
	RegisterClass<gamesave::SpellFlockGroundRecord>(writers, SpellClass::FlockGround,
	                                                FlockOwn<gamesave::SpellFlockGroundRecord>);
	RegisterClass<gamesave::SpellShieldRecord>(writers, SpellClass::Shield, ShieldOwn);
	// the physical and the spiritual shields' world objects, each a class of its own
	writers.Register(
	    gamesave::PhysicalShieldRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    const auto* shield = registry.TryGet<components::MagicShield>(entity);
		    return shield != nullptr && shield->kind == components::MagicShield::Kind::Physical;
	    },
	    WritePhysicalShield);
	writers.Register(
	    gamesave::SpiritualShieldRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    const auto* shield = registry.TryGet<components::MagicShield>(entity);
		    return shield != nullptr && shield->kind == components::MagicShield::Kind::Spiritual;
	    },
	    WriteSpiritualShield);
	// the game's spells: the live ones, the newest first, then those the load kept, as the loaded file listed them; a
	// kept spell the load made again and deleted since has left the list, as the original's spell leaves the game's
	// spells as it is deleted
	writers.RegisterList(GlobalList::Spells, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto spell : magic::Spells())
		{
			numbers.push_back(context.Reference(spell));
		}
		if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
		{
			// a kept spell the load made again is among the live ones already
			const auto live = static_cast<std::ptrdiff_t>(numbers.size());
			for (const auto number : game->lists.at(static_cast<size_t>(GlobalList::Spells)).numbers)
			{
				if (!context.IsGone(number) &&
				    std::find(numbers.begin(), numbers.begin() + live, number) == numbers.begin() + live)
				{
					numbers.push_back(number);
				}
			}
		}
		return numbers;
	});
}
