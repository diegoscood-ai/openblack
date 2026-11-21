/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A spell's seed (the miracle in a hand) in a saved game's world file, in the original's order, written from its
// entity (docs/wiki/engine/saved-games.md#openblack, the spell seeds).

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <EffectRecords.h>
#include <InterfaceRecords.h>
#include <SavedRecords.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SpellSeedRecord;

namespace
{
/// The places in the seed's record of its four objects, its two bytes, its seven words and its two last words
constexpr size_t k_Icon = 0;
constexpr size_t k_Creator = 1;
constexpr size_t k_Spell = 2;
constexpr size_t k_Status = 3;
constexpr size_t k_HasCast = 0;
constexpr size_t k_LearnedFrom = 1;
constexpr size_t k_ChantStore = 0;
constexpr size_t k_ChantStoreCopy = 1;
constexpr size_t k_StoredChants = 2;
constexpr size_t k_StoredAge = 3;
constexpr size_t k_StoredMaxObjects = 4;
constexpr size_t k_Power = 5;
constexpr size_t k_CastMultiplier = 6;
constexpr size_t k_TurnsInHand = 0;
constexpr size_t k_LastMagic = 1;

std::string NameOf(entt::entity entity)
{
	return "spell seed " + std::to_string(entt::to_integral(entity));
}

/// A spell's seed: the parts of an object (its row is its seed's), its power-up level, its flags, its seed's row, the
/// worship icon that charged it, its creator (the player or the icon), the spell cast from it, the status of the
/// interface whose hand holds it, whether it has cast, whether a creature learnt from it, its charge and its copy, the
/// chants, age and object count it kept from its spell, its power, the multiplier of its cast, whether it is ready,
/// its turns in the hand, the magic of its last cast and whether a one-shot orb made it. The object part's map
/// coordinates are the place its maker made it at (as kept for a loaded seed). None, and the write fails,
/// for a seed of no seed type or of a creator whose objects in the record are not read (a creature, another thing)
std::optional<gamesave::SavedRecord> WriteSeed(WriteContext& context, entt::entity entity)
{
	const auto& seed = context.GetRegistry().Get<const ecs::components::SpellSeed>(entity);
	if (seed.seedType == SpellSeedType::None)
	{
		context.Fail(NameOf(entity) + " has no seed type");
		return std::nullopt;
	}
	using Kind = ecs::components::SpellCreator::Kind;
	gamesave::ObjectNumber creator = 0;
	switch (seed.creator.kind)
	{
	case Kind::Player:
		creator = PlayerObject(context, static_cast<int>(seed.creator.player));
		break;
	case Kind::WorshipSpellIcon:
		creator = context.Reference(seed.creator.entity);
		break;
	case Kind::None:
		break;
	default:
		context.Fail(NameOf(entity) + " has a creator whose objects in a save are not read");
		return std::nullopt;
	}
	std::optional<SpellSeedRecord> kept;
	if (auto record = context.KeptRecordOf(entity, SpellSeedRecord::k_Type); record.has_value())
	{
		kept = std::get<SpellSeedRecord>(std::move(*record));
	}
	SpellSeedRecord record = kept.value_or(SpellSeedRecord {});
	const auto row = SpellSeedInfoRow(seed.seedType);
	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	record.object = ObjectPartOf(
	    context, entity, kept.has_value() ? &kept->object : nullptr,
	    kept.has_value() ? kept->object.coords
	                     : gamesave::MapCoords {.x = seed.madeAt.x, .z = seed.madeAt.z, .altitude = seed.madeAt.altitude},
	    row);
	record.word = static_cast<uint32_t>(seed.powerUp);
	record.byte = seed.flags;
	record.info = row;
	record.objects[k_Icon] = {context.Reference(seed.icon)};
	record.objects[k_Creator] = {creator};
	record.objects[k_Spell] = {context.Reference(seed.spell)};
	record.objects[k_Status] = {context.Reference(
	    SourceKey {.type = gamesave::InterfaceStatusRecord::k_Type, .id = static_cast<uint64_t>(seed.creator.player)})};
	record.bytes[k_HasCast] = seed.hasCast ? 1 : 0;
	record.bytes[k_LearnedFrom] = seed.learnedFrom ? 1 : 0;
	record.words[k_ChantStore] = std::bit_cast<uint32_t>(seed.chantStore);
	record.words[k_ChantStoreCopy] = std::bit_cast<uint32_t>(seed.chantStoreCopy);
	record.words[k_StoredChants] = std::bit_cast<uint32_t>(seed.storedChants);
	record.words[k_StoredAge] = std::bit_cast<uint32_t>(seed.storedAge);
	record.words[k_StoredMaxObjects] = static_cast<uint32_t>(seed.storedMaxObjects);
	record.words[k_Power] = std::bit_cast<uint32_t>(seed.power);
	record.words[k_CastMultiplier] = std::bit_cast<uint32_t>(seed.castMultiplier);
	record.flag = seed.ready ? 1 : 0;
	record.lastWords[k_TurnsInHand] = static_cast<uint32_t>(seed.turnsInHand);
	record.lastWords[k_LastMagic] = static_cast<uint32_t>(seed.lastMagic);
	record.lastFlag = seed.fromOneShot ? 1 : 0;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterSpellSeedWriters(ClassWriters& writers)
{
	writers.Register(
	    SpellSeedRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::SpellSeed>(entity); }, WriteSeed);
}
