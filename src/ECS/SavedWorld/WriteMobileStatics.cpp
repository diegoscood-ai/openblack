/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rocks', the teleports' stones' and the other mobile statics' records of a saved game's world file, in the original's
// order (docs/wiki/engine/saved-games.md#openblack, the rocks and mobile statics).

#include <cstdint>

#include <bit>
#include <optional>
#include <variant>

#include <EffectRecords.h>
#include <GameTailRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>

#include "ECS/Components/Mobile.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
/// The flag of an object of several cells that is built: every mobile static is made so
constexpr uint8_t k_Constructed = 0x08;

/// A mobile static these writers write: every one but a bonfire and a singing stone's base, which are classes of
/// their own
bool IsPlainMobileStatic(const ecs::Registry& registry, entt::entity entity)
{
	const auto* statics = registry.TryGet<const ecs::components::MobileStatic>(entity);
	return statics != nullptr && statics->type != MobileStaticInfo::Bonfire &&
	       statics->type != MobileStaticInfo::SingingStoneBase;
}

template <typename Record>
[[nodiscard]] std::optional<Record> KeptAs(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return std::nullopt;
}

/// The parts both classes write: an object of several cells' (made whole, as the game makes one), and the owner, the
/// X angle and the Z angle; the Y angle is the object's own
template <typename Record>
Record MobileStaticOf(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& statics = registry.Get<const ecs::components::MobileStatic>(entity);
	const auto kept = KeptAs<Record>(context, entity);
	Record record = kept.value_or(Record {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    ecs::saved_world_load::MobileStaticInfoRowOf(statics.type));
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
	}
	const auto* built = registry.TryGet<const ecs::components::StaticBuiltPercentage>(entity);
	record.base.multi.percentBuilt = built != nullptr ? built->percent : 0.0f;
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	record.mobileStatic.words = {std::bit_cast<uint32_t>(angles.x), std::bit_cast<uint32_t>(angles.z)};
	return record;
}
/// The two words a new rock keeps as it is made (its constructor's 0 and -1.0)
constexpr std::array<uint32_t, 2> k_NewRockWords {0, std::bit_cast<uint32_t>(-1.0f)};

/// A dead or a felled tree, a rock of the dead tree's row: an object of several cells' parts, its player, the X and Z
/// angles, the rock's two words and its player again, then the row of the tree it was and its wood multiplier
template <typename Record>
std::optional<gamesave::SavedRecord> WriteDeadTree(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& dead = registry.Get<const ecs::components::DeadTree>(entity);
	const auto kept = KeptAs<Record>(context, entity);
	Record record = kept.value_or(Record {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::DeadTree));
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
		record.base.multi.percentBuilt = 0.0f;
		record.words = k_NewRockWords;
	}
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	record.mobileStatic.words = {std::bit_cast<uint32_t>(angles.x), std::bit_cast<uint32_t>(angles.z)};
	record.object.number = PlayerReference(context, dead.owner);
	record.info = ecs::saved_world_load::TreeInfoRowOf(dead.type);
	record.word = std::bit_cast<uint32_t>(dead.woodValueMultiplier);
	return record;
}

/// The scale a teleport's stone is shrunk to as it is made: a hundredth of its mobile static's 1.0
constexpr float k_TeleportStoneScale = 0.01f;

/// A teleport miracle's stone: an object of several cells' parts of the teleport's row (made whole, as every mobile
/// static is), no owner, the X and Z angles, then the reaction it spreads (none once that has gone), its spell and its
/// player
std::optional<gamesave::SavedRecord> WriteTeleportStone(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& stone = registry.Get<const ecs::components::TeleportStone>(entity);
	const auto kept = KeptAs<gamesave::MagicTeleportRecord>(context, entity);
	gamesave::MagicTeleportRecord record = kept.value_or(gamesave::MagicTeleportRecord {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Teleport));
	// openblack keeps the stone at its scale of 1.0: it has no mesh, so nothing else reads the scale, and a loaded
	// stone's stays as it was read
	record.base.object.scale = kept.has_value() ? kept->base.object.scale : k_TeleportStoneScale;
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
		record.base.multi.percentBuilt = 0.0f;
	}
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	record.mobileStatic.words = {std::bit_cast<uint32_t>(angles.x), std::bit_cast<uint32_t>(angles.z)};
	const bool listed = stone.reaction != 0 &&
	                    (!Locator::reactionSystem::has_value() || ecs::effects::reactions::Find(stone.reaction) != nullptr);
	record.reaction.number =
	    listed ? context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = stone.reaction}) : 0;
	record.spell.number = stone.spell != entt::null && registry.Valid(stone.spell) ? context.Reference(stone.spell) : 0;
	record.player.number = stone.player.has_value() ? PlayerReference(context, *stone.player) : 0;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterMobileStaticWriters(ClassWriters& writers)
{
	// a bonfire: a rock of its row, and a word nothing writes
	writers.Register(
	    gamesave::BonfireRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    const auto* statics = registry.TryGet<const ecs::components::MobileStatic>(entity);
		    return statics != nullptr && statics->type == MobileStaticInfo::Bonfire;
	    },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    auto record = MobileStaticOf<gamesave::BonfireRecord>(context, entity);
		    if (!KeptAs<gamesave::BonfireRecord>(context, entity).has_value())
		    {
			    record.words = k_NewRockWords;
		    }
		    const auto owner = context.GetRegistry().Get<const ecs::components::MobileStatic>(entity).owner;
		    record.object.number = PlayerReference(context, owner);
		    return record;
	    });
	writers.Register(
	    gamesave::DeadTreeRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<ecs::components::DeadTree>(entity) && !registry.AllOf<ecs::components::FelledTree>(entity);
	    },
	    WriteDeadTree<gamesave::DeadTreeRecord>);
	writers.Register(
	    gamesave::FelledTreeRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<ecs::components::DeadTree, ecs::components::FelledTree>(entity);
	    },
	    WriteDeadTree<gamesave::FelledTreeRecord>);
	// a singing stone's base: only an object's parts, of its row, its target where it was made, and the thing's marks
	// of an object out of play and of a hidden name
	writers.Register(
	    gamesave::BaseOnlyRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    const auto* statics = registry.TryGet<const ecs::components::MobileStatic>(entity);
		    return statics != nullptr && statics->type == MobileStaticInfo::SingingStoneBase;
	    },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    constexpr uint8_t k_Unavailable = 0x01;
		    constexpr uint8_t k_NameHidden = 0x40;
		    const auto& registry = context.GetRegistry();
		    const auto kept = KeptAs<gamesave::BaseOnlyRecord>(context, entity);
		    gamesave::BaseOnlyRecord record = kept.value_or(gamesave::BaseOnlyRecord {});
		    if (!kept.has_value())
		    {
			    record.thing.flags =
			        static_cast<uint8_t>((registry.AllOf<ecs::components::Unavailable>(entity) ? k_Unavailable : 0) |
			                             (registry.AllOf<ecs::components::NameHidden>(entity) ? k_NameHidden : 0));
		    }
		    record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
		    record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr,
		                                 kept.has_value() ? kept->object.coords : record.position.position,
		                                 ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::SingingStoneBase));
		    return record;
	    });
	// a rock: its player, and two values the game keeps as they are made
	writers.Register(
	    gamesave::RockRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return IsPlainMobileStatic(registry, entity) && ecs::object_physics::IsRockObject(entity);
	    },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    auto record = MobileStaticOf<gamesave::RockRecord>(context, entity);
		    if (!KeptAs<gamesave::RockRecord>(context, entity).has_value())
		    {
			    record.words = {0, std::bit_cast<uint32_t>(-1.0f)};
		    }
		    const auto owner = context.GetRegistry().Get<const ecs::components::MobileStatic>(entity).owner;
		    record.object.number = PlayerReference(context, owner);
		    return record;
	    });
	writers.Register(
	    gamesave::MagicTeleportRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<ecs::components::TeleportStone>(entity); },
	    WriteTeleportStone);
	writers.Register(
	    gamesave::MobileStaticRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return IsPlainMobileStatic(registry, entity) && !ecs::object_physics::IsRockObject(entity);
	    },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    return MobileStaticOf<gamesave::MobileStaticRecord>(context, entity);
	    });
}
