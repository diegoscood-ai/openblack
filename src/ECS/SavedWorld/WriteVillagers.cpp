/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers' and the special villagers' records of a saved game's world file, in the original's order
// (docs/wiki/engine/saved-games.md#openblack, the villagers).

#include <cstdint>

#include <optional>
#include <type_traits>
#include <variant>

#include <SavedRecords.h>
#include <VillagerRecords.h>
#include <fmt/format.h>

#include "ECS/Components/AtHome.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteLivingParts.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/WalkStateOf.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The number of the first villager row; the others follow it downwards
constexpr uint32_t k_FirstVillagerRow = 2247;
/// The flag bits the villager component holds, and the others: awake, at home, an into or out-of clip plays, an
/// out-of clip plays first, has been to bed
constexpr uint16_t k_HeldFlags = 0x2 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x200 | 0x400 | 0xC000;
constexpr uint16_t k_FlagWoken = 0x1;
constexpr uint16_t k_FlagAtHome = 0x4;
constexpr uint16_t k_FlagsTransition = 0x800 | 0x1000;
constexpr uint16_t k_FlagBeenToBed = 0x2000;
/// The only bit the game has that openblack does not hold: as kept
constexpr uint16_t k_KeptFlags = 0x100;
/// The carried object a villager made in play has until its first clip is chosen
constexpr uint8_t k_NoCarriedObjectYet = 0;
/// The states a villager made in play is in before it first moves: created, and drowning on water
constexpr uint8_t k_CreatedState = 85;
constexpr uint8_t k_DrowningState = 16;

/// The villager's flags word, from what openblack holds and, for the one bit it does not, as kept
uint16_t FlagsOf(const ecs::Registry& registry, entt::entity entity, const components::Villager& villager, uint16_t kept)
{
	auto flags = static_cast<uint16_t>((villager.flags & k_HeldFlags) | (kept & k_KeptFlags));
	if (villager.woken)
	{
		flags |= k_FlagWoken;
	}
	if (const auto* home = registry.TryGet<const components::AtHome>(entity))
	{
		flags |= k_FlagAtHome;
		if (home->beenToBed)
		{
			flags |= k_FlagBeenToBed;
		}
	}
	if (const auto* animation = registry.TryGet<const components::SkeletalAnimation>(entity))
	{
		flags |= static_cast<uint16_t>(animation->transitionFlags & k_FlagsTransition);
	}
	return flags;
}

/// A villager's record of either class: its place, its object's values, the moving thing's word, its walk and living
/// values, its own values and its two states' values. Nothing, and the write fails, for one whose move state or
/// states' values openblack cannot give
template <typename Record>
std::optional<gamesave::SavedRecord> WriteVillagerRecord(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& villager = registry.Get<const components::Villager>(entity);
	std::optional<Record> kept;
	if (auto record = context.KeptRecordOf(entity, Record::k_Type); record.has_value())
	{
		kept = std::get<Record>(std::move(*record));
	}
	if (villager.infoRow == VillagerInfo::None)
	{
		context.Fail(fmt::format("villager {} has no row", entt::to_integral(entity)));
		return std::nullopt;
	}
	const auto row = static_cast<size_t>(villager.infoRow);
	Record record = kept.value_or(Record {});
	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr,
	                             kept.has_value() ? kept->object.coords : record.position.position,
	                             k_FirstVillagerRow - static_cast<uint32_t>(row));
	record.mobile = MobilePartOf(kept.has_value() ? &kept->mobile : nullptr);

	// as its constructor leaves it: the row's speed and strength, and its state counter, which openblack holds
	const auto made = LivingConstructorParts(Locator::infoConstants::value().villager.at(row));
	auto held = WallHugHeldOf(registry, entity);
	if (const auto state = ecs::wall_hug::WalkStateOf(registry, entity); state.has_value())
	{
		held.moveState = static_cast<uint8_t>(*state);
	}
	else if (!kept.has_value())
	{
		// no move state tag: the 0 it was made with, while it has not moved yet; after a walk the game's state (the
		// arrival's, or none) is not held
		const auto* action = registry.TryGet<const components::LivingAction>(entity);
		const auto top = action != nullptr ? action->states[0] : k_CreatedState;
		if (top != k_CreatedState && top != k_DrowningState)
		{
			context.Fail(fmt::format("villager {} made in play holds no move state", entt::to_integral(entity)));
			return std::nullopt;
		}
		held.moveState = 0;
	}
	const auto move = WallHugPartOf(context, entity, kept.has_value() ? &kept->values.move : nullptr, made.move, held);
	const auto living = LivingFieldsOf(context, entity, kept.has_value() ? &kept->values.living : nullptr, made.living,
	                                   LivingHeldOf(registry, entity));
	if (!move.has_value() || !living.has_value())
	{
		return std::nullopt;
	}
	record.values.move = *move;
	record.values.living = *living;

	// its own values
	auto& fields = record.values.villager;
	fields.flags = FlagsOf(registry, entity, villager, kept.has_value() ? kept->values.villager.flags : uint16_t {0});
	fields.food = villager.food;
	fields.lastCheckTurn = static_cast<int32_t>(villager.lastCheckTurn);
	fields.foodSpeedUp = villager.foodSpeedUp;
	const auto* animation = registry.TryGet<const components::SkeletalAnimation>(entity);
	fields.carriedObjectType = animation != nullptr ? static_cast<uint8_t>(animation->carriedObject) : k_NoCarriedObjectYet;
	fields.discipleType = villager.discipleType;
	fields.foodHeld = villager.resourceHeld[0];
	fields.woodHeld = villager.resourceHeld[1];
	fields.pregnant = villager.pregnancy;
	fields.home.number = context.Reference(villager.abode);
	fields.town.number = context.Reference(villager.town);
	fields.mother.number = context.Reference(villager.mother);
	fields.reserved = {};

	// its two states' values: none for a state that saves none; else as kept while it is in the states its load kept
	const auto& states = record.values.living.action;
	const auto valuesOf = [&](uint8_t state, const std::vector<gamesave::StateValue>* keptValues,
	                          uint8_t keptState) -> std::optional<std::vector<gamesave::StateValue>> {
		if (gamesave::StateValueSizes(gamesave::VillagerStateSaveOf(state), fields.discipleType).empty())
		{
			return std::vector<gamesave::StateValue> {};
		}
		// what it works on, as it holds it
		if (const auto save = gamesave::VillagerStateSaveOf(state); save == gamesave::VillagerStateSave::Fishing ||
		                                                            save == gamesave::VillagerStateSave::Foresting ||
		                                                            save == gamesave::VillagerStateSave::InspectObject)
		{
			gamesave::StateValue target {.size = gamesave::k_StateReference};
			target.number = context.Reference(villager.targetThing);
			return std::vector<gamesave::StateValue> {target};
		}
		// a dead villager's cause, as it holds it
		if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Dead)
		{
			gamesave::StateValue cause {.size = 1};
			cause.bytes[0] = static_cast<uint8_t>(villager.deathReason);
			return std::vector<gamesave::StateValue> {cause};
		}
		if (keptValues != nullptr && keptState == state)
		{
			return *keptValues;
		}
		// a dance's state, entered in play: its two words are what a slot the villager's other states share last held,
		// which nothing of the dance's states reads; openblack writes the 0 and 0 a villager is made with
		if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Dance)
		{
			return std::vector<gamesave::StateValue> {{.size = 4}, {.size = 4}};
		}
		// a worship state, entered in play: its word (requesting to go home) is that same slot; nothing in the game
		// sets it, so it is the 0 a villager is made with
		if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Worship)
		{
			return std::vector<gamesave::StateValue> {{.size = 4}};
		}
		context.Fail(
		    fmt::format("villager {} is in state {}, whose values openblack does not hold", entt::to_integral(entity), state));
		return std::nullopt;
	};
	const auto top = valuesOf(states[0], kept.has_value() ? &kept->values.topState : nullptr,
	                          kept.has_value() ? kept->values.living.action[0] : uint8_t {0});
	const auto final = valuesOf(states[1], kept.has_value() ? &kept->values.finalState : nullptr,
	                            kept.has_value() ? kept->values.living.action[1] : uint8_t {0});
	if (!top.has_value() || !final.has_value())
	{
		return std::nullopt;
	}
	record.values.topState = *top;
	record.values.finalState = *final;

	if constexpr (std::is_same_v<Record, gamesave::SpecialVillagerRecord>)
	{
		record.infoIndex = registry.Get<const components::NamedVillager>(entity).row;
	}
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterVillagerWriters(ClassWriters& writers)
{
	// a special villager is one with a name of the special villagers' table
	writers.Register(
	    gamesave::VillagerRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::Villager>(entity) && !registry.AllOf<components::NamedVillager>(entity);
	    },
	    WriteVillagerRecord<gamesave::VillagerRecord>);
	writers.Register(
	    gamesave::SpecialVillagerRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::Villager, components::NamedVillager>(entity);
	    },
	    WriteVillagerRecord<gamesave::SpecialVillagerRecord>);
}
