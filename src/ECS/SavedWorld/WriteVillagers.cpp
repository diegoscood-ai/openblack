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

#include "ECS/SavedWorld/WriteVillagers.h"

#include <concepts>
#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>
#include <VillagerRecords.h>
#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicTreeWatch.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptMemory.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/ShepherdWork.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Components/VillagerInspection.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Components/WallHug.h"
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
/// The thing's mark of a name hidden for good (a villager a script made)
constexpr uint8_t k_ThingNameHidden = 0x40;
/// The carried object a villager made in play has until its first clip is chosen
constexpr uint8_t k_NoCarriedObjectYet = 0;

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

/// A villager's script memory as its script's reminder: the thing its walk followed, the walk's destination with its
/// height, the word only cleared and the pointer only saved as its load kept them (0 for one made in play), the
/// script's place in whole map x and z, its number, the pair of whole numbers, the clip its state last put on and the
/// state to take up; a place, number and pair no script gave are 0
std::optional<gamesave::SavedRecord> WriteScriptReminder(WriteContext& context, uint64_t id)
{
	const auto& registry = context.GetRegistry();
	const auto villager = PartOf(id).owner;
	const auto* memory = registry.Valid(villager) ? registry.TryGet<const components::ScriptMemory>(villager) : nullptr;
	if (memory == nullptr)
	{
		context.Fail(fmt::format("living {} has no script memory for its reminder", entt::to_integral(villager)));
		return std::nullopt;
	}
	std::optional<gamesave::ScriptReminderRecord> kept;
	if (auto record = context.KeptRecordOf(SourceKey {.type = gamesave::ScriptReminderRecord::k_Type, .id = id});
	    record.has_value())
	{
		kept = std::get<gamesave::ScriptReminderRecord>(std::move(*record));
	}
	auto record = kept.value_or(gamesave::ScriptReminderRecord {});
	const auto put = [](auto& bytes, size_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, sizeof(value)); };
	record.object.number =
	    memory->target != entt::null && registry.Valid(memory->target) ? context.Reference(memory->target) : 0;
	put(record.firstValue, 0, std::bit_cast<uint32_t>(memory->destination.x));
	put(record.firstValue, sizeof(uint32_t), std::bit_cast<uint32_t>(memory->destination.z));
	put(record.firstValue, 2 * sizeof(uint32_t), std::bit_cast<uint32_t>(memory->destination.altitude));
	const auto place = memory->arguments.has_value() ? memory->arguments->place : glm::vec2 {0.0f};
	put(record.thirdValue, 0, std::bit_cast<uint32_t>(map_coords::ToFixed(place.x)));
	put(record.thirdValue, sizeof(uint32_t), std::bit_cast<uint32_t>(map_coords::ToFixed(place.y)));
	record.words = {std::bit_cast<uint32_t>(memory->arguments.has_value() ? memory->arguments->number : 0.0f),
	                memory->clipAndPlays.has_value() ? static_cast<uint32_t>(memory->clipAndPlays->clip) : 0U,
	                memory->clipAndPlays.has_value() ? memory->clipAndPlays->playsLeft : 0U,
	                std::bit_cast<uint32_t>(memory->stateClip), static_cast<uint32_t>(memory->state)};
	return record;
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
	// the thing's mark of a hidden name from what openblack holds, its other marks as kept
	record.thing.flags = static_cast<uint8_t>((record.thing.flags & ~k_ThingNameHidden) |
	                                          (registry.AllOf<components::NameHidden>(entity) ? k_ThingNameHidden : 0));
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
	else if (const auto* after = registry.TryGet<const components::WalkStateAfterWalk>(entity); after != nullptr)
	{
		// its walk's tags dropped: the state the game keeps after the walk
		held.moveState = after->state;
	}
	else if (!kept.has_value())
	{
		// no move state tag and no walk yet: the 0 it was made with, which only a walk's set-up changes
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
	// its script's reminder: the one its script memory writes, none without one
	record.values.living.scriptReminder.number =
	    registry.AllOf<components::ScriptMemory>(entity)
	        ? context.Reference(SourceKey {.type = gamesave::ScriptReminderRecord::k_Type, .id = PartId(entity, 0)})
	        : gamesave::ObjectNumber {0};

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
	// its home, its town and its mother while they are there: a deleted abode puts its villagers out, a deleted mother
	// orphans her children and a deleted town lets its villagers go, as the game does it
	const auto standing = [&context](entt::entity entity) {
		return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
	};
	fields.home.number = standing(villager.abode);
	fields.town.number = standing(villager.town);
	fields.mother.number = standing(villager.mother);
	fields.reserved = {};

	// its two states' values: none for a state that saves none; those openblack holds; else as kept while it is in the
	// states its load kept
	const auto& states = record.values.living.action;
	const auto valuesOf = [&](uint8_t state, const std::vector<gamesave::StateValue>* keptValues,
	                          uint8_t keptState) -> std::optional<std::vector<gamesave::StateValue>> {
		if (gamesave::StateValueSizes(gamesave::VillagerStateSaveOf(state), fields.discipleType).empty())
		{
			return std::vector<gamesave::StateValue> {};
		}
		if (auto held = HeldVillagerStateValues(context, entity, state, record.values.living); held.has_value())
		{
			return held;
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

/// A reference among a state's values
gamesave::StateValue ReferenceValue(gamesave::ObjectNumber number)
{
	gamesave::StateValue value {.size = gamesave::k_StateReference};
	value.number = number;
	return value;
}

/// One value of a state's save, its bytes as the game holds them
template <typename T>
gamesave::StateValue PlainValue(const T& held)
{
	static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 12);
	gamesave::StateValue value {.size = static_cast<uint8_t>(sizeof(T))};
	std::memcpy(value.bytes.data(), &held, sizeof(T));
	return value;
}

/// A place the game keeps as whole map x and z
gamesave::StateValue WholeXzValue(int32_t x, int32_t z)
{
	return PlainValue(std::array<int32_t, 2> {x, z});
}

/// The place another object stands at, as its own record writes it: its kept record's while it stands there
gamesave::MapCoords PlaceOf(WriteContext& context, entt::entity entity)
{
	std::optional<gamesave::PositionPart> kept;
	if (const auto type = context.ClassOf(entity); type.has_value())
	{
		if (auto record = context.KeptRecordOf(entity, *type); record.has_value())
		{
			std::visit(
			    [&kept](const auto& saved) {
				    if constexpr (requires { requires std::same_as<decltype(saved.base.position), gamesave::PositionPart>; })
				    {
					    kept = saved.base.position;
				    }
				    else if constexpr (requires { requires std::same_as<decltype(saved.position), gamesave::PositionPart>; })
				    {
					    kept = saved.position;
				    }
			    },
			    *record);
		}
	}
	return PositionPartOf(context, entity, kept.has_value() ? &*kept : nullptr).position;
}
} // namespace

std::optional<std::vector<gamesave::StateValue>>
openblack::ecs::saved_world_write::HeldVillagerStateValues(WriteContext& context, entt::entity entity, uint8_t state,
                                                           const gamesave::LivingFields& living)
{
	using Save = gamesave::VillagerStateSave;
	using Values = std::vector<gamesave::StateValue>;
	const auto& registry = context.GetRegistry();
	const auto& villager = registry.Get<const components::Villager>(entity);
	switch (gamesave::VillagerStateSaveOf(state))
	{
	// what it works on: the thing it fishes at, fells or looks at
	case Save::Fishing:
	case Save::Foresting:
	case Save::InspectObject:
		return Values {ReferenceValue(context.Reference(villager.targetThing))};
	// a dead villager's cause
	case Save::Dead:
		return Values {PlainValue(static_cast<uint8_t>(villager.deathReason))};
	// the building site and its place there
	case Save::Building:
		return Values {ReferenceValue(context.Reference(villager.buildingSite)), PlainValue(villager.buildPosIndex)};
	// the work point in its field, then the field
	case Save::Farming:
		return Values {PlainValue(villager.workPos), ReferenceValue(context.Reference(villager.targetThing))};
	// the partner (none while it wanders), then the spot it wanders about
	case Save::Breeder:
		return Values {ReferenceValue(context.Reference(villager.targetThing)),
		               WholeXzValue(villager.wanderArea.x, villager.wanderArea.z)};
	// the resource it trades and the town it trades with; a trader's nothing to do writes the same
	case Save::Trader:
	case Save::DiscipleNothingToDo:
		return Values {PlainValue(static_cast<int32_t>(villager.tradeResource)),
		               ReferenceValue(context.Reference(villager.tradeTown))};
	// the disciple it had as the hand picked it up
	case Save::InHand:
		return Values {PlainValue(static_cast<uint32_t>(villager.discipleOnPickUp))};
	// the footpath it is on and its node there, as its living part writes them
	case Save::FootPath:
		return Values {ReferenceValue(living.footpath.number), ReferenceValue(living.footpathNode.number)};
	// whether it chooses its substate again, the turns in it, its place among its position's players, then the pitch
	case Save::Football:
		return Values {PlainValue(static_cast<uint8_t>(villager.footballChooseAgain ? 1 : 0)),
		               PlainValue(villager.footballSubstateTurns), PlainValue(villager.footballNumber),
		               ReferenceValue(context.Reference(villager.footballPitch))};
	// the turns it waits, then the state it goes on to (none before its first wait: 0 and 0, as it is made)
	case Save::Shepherd:
	{
		const auto* work = registry.TryGet<const components::ShepherdWork>(entity);
		const auto wait = work != nullptr ? work->waitTurns : int16_t {0};
		const auto pending = work != nullptr ? static_cast<uint16_t>(work->pending) : uint16_t {0};
		return Values {PlainValue(wait), PlainValue(pending)};
	}
	// the place a script gave it in whole map x and z, the number it gave (how far to wander), then the pair of whole
	// numbers it gave (a clip and its plays, or the fewest and the most turns at each stop); 0 for those no script gave
	// yet, as it is made
	case Save::InScript:
	case Save::ScriptPos:
	{
		const auto* arguments = registry.TryGet<const components::ScriptStateArguments>(entity);
		const auto place = arguments != nullptr ? arguments->place : glm::vec2 {0.0f};
		const auto number = arguments != nullptr ? arguments->number : 0.0f;
		const auto* pair = registry.TryGet<const components::ScriptAnimation>(entity);
		const auto first = pair != nullptr ? static_cast<uint32_t>(pair->clip) : uint32_t {0};
		const auto second = pair != nullptr ? pair->playsLeft : uint32_t {0};
		return Values {WholeXzValue(map_coords::ToFixed(place.x), map_coords::ToFixed(place.y)), PlainValue(number),
		               PlainValue(first), PlainValue(second)};
	}
	// the place it goes round the fire to, in whole map x and z, then the fire it flees or fights
	case Save::Fire:
	{
		const auto* fire = registry.TryGet<const components::VillagerFireState>(entity);
		const auto to = fire != nullptr ? fire->savedDestination : glm::vec2 {0.0f};
		const auto effect = fire != nullptr && fire->fire != 0
		                        ? context.Reference(SourceKey {.type = gamesave::FireEffectRecord::k_Type, .id = fire->fire})
		                        : gamesave::ObjectNumber {0};
		return Values {WholeXzValue(map_coords::ToFixed(to.x), map_coords::ToFixed(to.y)), ReferenceValue(effect)};
	}
	// the turns it has hidden or looked about, then the place of the building it hides in, x and z
	case Save::HideInBuilding:
	{
		const auto* hiding = registry.TryGet<const components::VillagerHiding>(entity);
		const auto turns = hiding != nullptr ? hiding->turns : uint32_t {0};
		const auto place =
		    hiding != nullptr && hiding->building != entt::null ? PlaceOf(context, hiding->building) : gamesave::MapCoords {};
		return Values {PlainValue(turns), WholeXzValue(place.x, place.z)};
	}
	// the point it looks at, x and z
	case Save::ShieldReaction:
	{
		const auto* shield = registry.TryGet<const components::VillagerShieldState>(entity);
		const auto point = shield != nullptr ? shield->lookAt : glm::ivec2 {0};
		return Values {WholeXzValue(point.x, point.y)};
	}
	// the distance it looks from and the turns before it walks closer
	case Save::InspectionReaction:
	{
		const auto* inspection = registry.TryGet<const components::VillagerInspection>(entity);
		const auto distance = inspection != nullptr ? inspection->distance : 0.0f;
		const auto turns = inspection != nullptr ? inspection->turnsUntilApproach : uint32_t {0};
		return Values {PlainValue(distance), PlainValue(turns)};
	}
	// the number of the next tree it looks at, then how many it has looked at
	case Save::MagicTreeReaction:
	{
		const auto* watch = registry.TryGet<const components::MagicTreeWatch>(entity);
		const auto next = watch != nullptr ? watch->next : uint32_t {0};
		const auto looked = watch != nullptr ? watch->looked : uint32_t {0};
		return Values {PlainValue(next), PlainValue(looked)};
	}
	default:
		return std::nullopt;
	}
}

void openblack::ecs::saved_world_write::RegisterVillagerWriters(ClassWriters& writers)
{
	// a villager's script memory is its script's reminder, keyed by the villager
	writers.RegisterSource(
	    gamesave::ScriptReminderRecord::k_Type,
	    [](const Registry& registry) {
		    std::vector<uint64_t> keys;
		    registry.Each<const components::Villager, const components::ScriptMemory>(
		        [&keys](entt::entity entity, const components::Villager& /*villager*/,
		                const components::ScriptMemory& /*memory*/) { keys.push_back(PartId(entity, 0)); });
		    // and an animal's, which only a load gives it, kept to be written again
		    registry.Each<const components::Animal, const components::ScriptMemory>(
		        [&keys](entt::entity entity, const components::Animal& /*animal*/, const components::ScriptMemory& /*memory*/) {
			        keys.push_back(PartId(entity, 0));
		        });
		    return keys;
	    },
	    WriteScriptReminder);
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
