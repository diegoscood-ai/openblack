/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rocks' and the other mobile statics' records of a saved game's world file, in the original's order
// (docs/wiki/engine/saved-games.md#openblack, the rocks and mobile statics).

#include <cstdint>

#include <bit>
#include <optional>
#include <variant>

#include <EffectRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>

#include "ECS/Components/Mobile.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"

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
} // namespace

void openblack::ecs::saved_world_write::RegisterMobileStaticWriters(ClassWriters& writers)
{
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
	    gamesave::MobileStaticRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return IsPlainMobileStatic(registry, entity) && !ecs::object_physics::IsRockObject(entity);
	    },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    return MobileStaticOf<gamesave::MobileStaticRecord>(context, entity);
	    });
}
