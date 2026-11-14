/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pots' and the piles' records of a saved game's world file, in the original's order, written from their entities:
// the plain pot, the food and wood piles and the magic food and wood (docs/wiki/engine/saved-games.md#openblack, the
// pots and piles).

#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WriteLivingParts.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;
namespace pot_flags = openblack::gamesave::pot_flags;

namespace
{
/// The bits of the flags byte openblack holds; the others are as kept, else 0
constexpr uint8_t k_HeldFlags =
    pot_flags::k_Poisoned | pot_flags::k_ReactionStarted | pot_flags::k_FromBuildingSite | pot_flags::k_SpeedUp;

/// The row of the pot's kind, none without the info constants or a kind
const GPotInfo* RowOf(const components::Pot& pot)
{
	if (!Locator::infoConstants::has_value() || pot.type == PotInfo::_COUNT)
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type));
}

/// The class of a pot entity: that of the record its load kept, else its row's: a plain pot, a food pile (the magic
/// food's row a magic food) or a wood pile (the magic wood's row a magic wood), as the game's making picks it
std::optional<uint32_t> PotClassOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* pot = registry.TryGet<const components::Pot>(entity);
	if (pot == nullptr)
	{
		return std::nullopt;
	}
	if (registry.AllOf<components::LoadedRecord<gamesave::FoodPileRecord>>(entity))
	{
		return gamesave::FoodPileRecord::k_Type;
	}
	if (registry.AllOf<components::LoadedRecord<gamesave::WoodPileRecord>>(entity))
	{
		return gamesave::WoodPileRecord::k_Type;
	}
	const auto* row = RowOf(*pot);
	if (row == nullptr)
	{
		return std::nullopt;
	}
	switch (row->potType)
	{
	case PotType::Pot:
		return gamesave::PotRecord::k_Type;
	case PotType::PileFood:
		return pot->type == PotInfo::MagicFood ? gamesave::MagicFoodRecord::k_Type : gamesave::FoodPileRecord::k_Type;
	case PotType::PileWood:
		return pot->type == PotInfo::MagicWood ? gamesave::MagicWoodRecord::k_Type : gamesave::WoodPileRecord::k_Type;
	}
	return std::nullopt;
}

template <typename Record>
std::optional<Record> KeptOf(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		if (auto* record = std::get_if<Record>(&*kept); record != nullptr)
		{
			return std::move(*record);
		}
	}
	return std::nullopt;
}

/// The flags byte: what openblack holds of the pot, the other bits as kept
uint8_t FlagsOf(const components::Pot& pot, uint8_t kept)
{
	const auto bit = [](bool on, uint8_t flag) { return on ? flag : uint8_t {0}; };
	return static_cast<uint8_t>((kept & ~k_HeldFlags) | bit(pot.poisoned, pot_flags::k_Poisoned) |
	                            bit(pot.reactionStarted, pot_flags::k_ReactionStarted) |
	                            bit(pot.fromBuildingSite, pot_flags::k_FromBuildingSite) |
	                            bit(pot.speedUp, pot_flags::k_SpeedUp));
}

/// The moving object's parts every pot writes first: its place (a pile's height its sink's destination, as its size
/// leaves it), its object's values of its kind's row, the moving thing's word and the moving object's values (as made:
/// carrying nothing, on no path, its x and z angles as it holds them)
gamesave::MobileObjectFields BaseOf(WriteContext& context, entt::entity entity, const components::Pot& pot,
                                    const gamesave::MobileObjectFields* kept)
{
	const auto& registry = context.GetRegistry();
	gamesave::MobileObjectFields base;
	base.position = PositionPartOf(context, entity, kept != nullptr ? &kept->position : nullptr);
	// a pile on the land stands at the height its size gives it
	constexpr uint16_t k_Carried = position_flags::k_InTheHand | position_flags::k_InPhysics;
	if (const auto* sink = registry.TryGet<const components::PileSink>(entity);
	    sink != nullptr && (base.position.flags & k_Carried) == 0)
	{
		base.position.position.altitude = sink->offset.destination;
	}
	base.object = ObjectPartOf(context, entity, kept != nullptr ? &kept->object : nullptr,
	                           kept != nullptr ? kept->object.coords : base.position.position,
	                           ecs::saved_world_load::PotInfoRowOf(pot.type));
	base.mobile = MobilePartOf(kept != nullptr ? &kept->mobile : nullptr);
	if (kept != nullptr)
	{
		base.mobileObject = kept->mobileObject;
	}
	else
	{
		const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
		base.mobileObject = gamesave::MobileObjectPart {.xAngle = angles.x, .zAngle = angles.z};
	}
	return base;
}

/// A pile's values: the pot's, then those of a pot of a structure and the pile's height. The player is the one the pile
/// answered with as it was made, or as a loaded one answered with at its load, as the pot keeps it
gamesave::PotFields PileOf(WriteContext& context, entt::entity entity, const components::Pot& pot, const GPotInfo& row,
                           const gamesave::PotFields* kept)
{
	const auto& registry = context.GetRegistry();
	gamesave::PotFields pile = kept != nullptr ? *kept : gamesave::PotFields {};
	pile.resourceType = static_cast<uint32_t>(row.resourceType);
	pile.parentTown.number = context.Reference(pot.town);
	pile.resourceAmount = pot.amount;
	pile.flags = FlagsOf(pot, kept != nullptr ? kept->flags : uint8_t {0});
	const auto structure = ecs::resource_stores::StructureOfPile(entity);
	pile.building.number = registry.Valid(structure) ? context.Reference(structure) : 0;
	pile.player.number = PlayerReference(context, pot.playerWhenMade);
	pile.buildingByte = pot.reactsWhenMade ? 1 : 0;
	if (const auto* sink = registry.TryGet<const components::PileSink>(entity); sink != nullptr)
	{
		pile.altitudeZoomer = ecs::saved_zoomer::BytesOf(sink->offset);
	}
	return pile;
}

/// The record of a pot of the class, with what openblack holds of it; nothing, and the write fails, for a pot of no
/// kind
template <typename Record>
std::optional<gamesave::SavedRecord> WritePot(WriteContext& context, entt::entity entity)
{
	const auto& pot = context.GetRegistry().Get<const components::Pot>(entity);
	const auto* row = RowOf(pot);
	if (row == nullptr)
	{
		context.Fail(fmt::format("pot {} has no kind's row", entt::to_integral(entity)));
		return std::nullopt;
	}
	const auto kept = KeptOf<Record>(context, entity);
	Record record = kept.value_or(Record {});
	record.base = BaseOf(context, entity, pot, kept.has_value() ? &kept->base : nullptr);
	if constexpr (std::is_same_v<Record, gamesave::PotRecord>)
	{
		record.resourceType = static_cast<uint32_t>(row->resourceType);
		record.parentTown.number = context.Reference(pot.town);
		record.resourceAmount = pot.amount;
		record.flags = FlagsOf(pot, kept.has_value() ? kept->flags : uint8_t {0});
	}
	else
	{
		constexpr bool k_Magic =
		    std::is_same_v<Record, gamesave::MagicFoodRecord> || std::is_same_v<Record, gamesave::MagicWoodRecord>;
		record.pile = PileOf(context, entity, pot, *row, kept.has_value() ? &kept->pile : nullptr);
		if constexpr (k_Magic)
		{
			record.player.number = PlayerReference(context, pot.owner);
		}
		if constexpr (std::is_same_v<Record, gamesave::FoodPileRecord> || std::is_same_v<Record, gamesave::MagicFoodRecord>)
		{
			// its speed-up's spot visual, none once it has gone (a loaded pile's, made again, is the one its load kept)
			record.store.number = SpotVisualObject(context, pot.speedUpVisual);
		}
	}
	return record;
}

/// The moving object rows: the first row's number, the others following it downwards
constexpr uint32_t k_FirstMobileObjectRow = 1970;

/// A moving object of the plain class: one made of a moving object row that is not a poo's, a crop's, a creed's or a
/// Hanoi puzzle's piece (classes of their own), nor a one-off spell's orb or a scaffold
bool IsPlainMobileObject(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mobile = registry.TryGet<const components::MobileObject>(entity);
	if (mobile == nullptr || registry.AnyOf<components::OneOffSpellSeed, components::Scaffold>(entity))
	{
		return false;
	}
	switch (mobile->type)
	{
	case MobileObjectInfo::LumpOfPoo:
	case MobileObjectInfo::Crop:
	case MobileObjectInfo::Creed:
	case MobileObjectInfo::HanoiPuzzleBase:
	case MobileObjectInfo::HanoiPuzzlePart1:
	case MobileObjectInfo::HanoiPuzzlePart2:
	case MobileObjectInfo::HanoiPuzzlePart3:
	case MobileObjectInfo::HanoiPuzzlePart4:
		return false;
	default:
		return true;
	}
}

/// The parts every moving object writes: its place, its object's values of its row (its target its place, as kept for
/// a loaded one), the moving thing's word and the moving object's values (carrying nothing, on no path, its x and z
/// angles as it holds them)
gamesave::MobileObjectFields MovingObjectFieldsOf(WriteContext& context, entt::entity entity, uint32_t row,
                                                  const gamesave::MobileObjectFields* kept)
{
	const auto& registry = context.GetRegistry();
	gamesave::MobileObjectFields base;
	base.position = PositionPartOf(context, entity, kept != nullptr ? &kept->position : nullptr);
	base.object = ObjectPartOf(context, entity, kept != nullptr ? &kept->object : nullptr,
	                           kept != nullptr ? kept->object.coords : base.position.position, row);
	base.mobile = MobilePartOf(kept != nullptr ? &kept->mobile : nullptr);
	if (kept != nullptr)
	{
		base.mobileObject = kept->mobileObject;
	}
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	base.mobileObject.xAngle = angles.x;
	base.mobileObject.zAngle = angles.z;
	return base;
}

/// A plain moving object: its moving object's parts of its row
std::optional<gamesave::SavedRecord> WriteMobileObject(WriteContext& context, entt::entity entity)
{
	const auto& mobile = context.GetRegistry().Get<const components::MobileObject>(entity);
	const auto kept = KeptOf<gamesave::MobileObjectRecord>(context, entity);
	gamesave::MobileObjectRecord record = kept.value_or(gamesave::MobileObjectRecord {});
	record.base = MovingObjectFieldsOf(context, entity, k_FirstMobileObjectRow - static_cast<uint32_t>(mobile.type),
	                                   kept.has_value() ? &kept->base : nullptr);
	return record;
}

/// The places in a Hanoi block's record of the blocks above and below it and of its puzzle
constexpr size_t k_BlockAbove = 0;
constexpr size_t k_BlockBelow = 1;
constexpr size_t k_BlockPuzzle = 2;

/// A Hanoi puzzle's block: its moving object's parts of its row, then its rest place, the blocks above and below it and
/// its puzzle, each none when it has gone
std::optional<gamesave::SavedRecord> WriteHanoiBlock(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& mobile = registry.Get<const components::MobileObject>(entity);
	const auto& block = registry.Get<const components::HanoiBlock>(entity);
	const auto kept = KeptOf<gamesave::HanoiBlockRecord>(context, entity);
	gamesave::HanoiBlockRecord record = kept.value_or(gamesave::HanoiBlockRecord {});
	record.base = MovingObjectFieldsOf(context, entity, k_FirstMobileObjectRow - static_cast<uint32_t>(mobile.type),
	                                   kept.has_value() ? &kept->base : nullptr);
	// one made since that stands where its puzzle put it stands at its rest place, which the game gave it as its place
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	if (!kept.has_value() && transform != nullptr && transform->position == block.placedAt)
	{
		const gamesave::MapCoords home {.x = block.home.x, .z = block.home.z, .altitude = block.home.altitude};
		record.base.position.position = home;
		record.base.object.coords = home;
	}
	// its rest place, in the map coordinates the game keeps it in
	record.point = {std::bit_cast<float>(block.home.x), std::bit_cast<float>(block.home.z), block.home.altitude};
	const auto referenceOrNone = [&context, &registry](entt::entity object) -> gamesave::ObjectNumber {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : 0;
	};
	record.objects.at(k_BlockAbove) = {referenceOrNone(block.above)};
	record.objects.at(k_BlockBelow) = {referenceOrNone(block.below)};
	record.objects.at(k_BlockPuzzle) = {referenceOrNone(block.puzzle)};
	return record;
}

/// A poo: a moving object of the poo's row, not an orb nor a scaffold
bool IsPoo(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mobile = registry.TryGet<const components::MobileObject>(entity);
	return mobile != nullptr && mobile->type == MobileObjectInfo::LumpOfPoo &&
	       !registry.AnyOf<components::OneOffSpellSeed, components::Scaffold>(entity);
}

/// A poo: its moving object's parts of the poo's row alone, as the plain moving object writes them
std::optional<gamesave::SavedRecord> WritePoo(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptOf<gamesave::PooRecord>(context, entity);
	gamesave::PooRecord record = kept.value_or(gamesave::PooRecord {});
	record.base =
	    MovingObjectFieldsOf(context, entity, k_FirstMobileObjectRow - static_cast<uint32_t>(MobileObjectInfo::LumpOfPoo),
	                         kept.has_value() ? &kept->base : nullptr);
	return record;
}

/// A one-off spell's orb: its moving object's parts of the orb's row, then its power-up, its seed and its multiplier
std::optional<gamesave::SavedRecord> WriteOrb(WriteContext& context, entt::entity entity)
{
	const auto& orb = context.GetRegistry().Get<const components::OneOffSpellSeed>(entity);
	const auto kept = KeptOf<gamesave::OneOffSpellSeedRecord>(context, entity);
	gamesave::OneOffSpellSeedRecord record = kept.value_or(gamesave::OneOffSpellSeedRecord {});
	record.base =
	    MovingObjectFieldsOf(context, entity, k_FirstMobileObjectRow - static_cast<uint32_t>(MobileObjectInfo::OneOffSpellSeed),
	                         kept.has_value() ? &kept->base : nullptr);
	record.words = {static_cast<uint32_t>(orb.powerUp), static_cast<uint32_t>(orb.seedType),
	                std::bit_cast<uint32_t>(orb.scale)};
	return record;
}

/// The scaffolds' row: the game's one scaffold kind, which every scaffold of the original's saves names
constexpr uint32_t k_ScaffoldRow = 1938;
/// A plan that offers no building names no row
constexpr uint32_t k_NoRow = 0xFFFFFFFF;

/// The number by which a scaffold names one of its two plans: none for none. A plan deleted while the scaffold still
/// points at it is refused: the original's pointer would name an object that is gone
gamesave::ObjectNumber ScaffoldPlanReference(WriteContext& context, entt::entity entity,
                                             const std::optional<components::ScaffoldPlan>& plan, uint32_t index)
{
	if (!plan.has_value())
	{
		return 0;
	}
	if (!plan->available)
	{
		context.Fail(fmt::format("scaffold {} points at a deleted plan", entt::to_integral(entity)));
		return 0;
	}
	return context.Reference(SourceKey {.type = gamesave::PlannedAbodeRecord::k_Type, .id = PartId(entity, index)});
}

/// A scaffold: its moving object's parts of the scaffold's row, carrying the workshop that owns it; then its building
/// site, its plan and its candidate (its own planned abodes, in no town's list), the cross-fade's timer, the player who
/// holds or dropped it, the phantom's angle and scale, its flags, its town and the only building it may offer
std::optional<gamesave::SavedRecord> WriteScaffold(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& scaffold = registry.Get<const components::Scaffold>(entity);
	const auto referenceOrNone = [&context, &registry](entt::entity object) {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : gamesave::ObjectNumber {0};
	};
	const auto kept = KeptOf<gamesave::ScaffoldRecord>(context, entity);
	gamesave::ScaffoldRecord record = kept.value_or(gamesave::ScaffoldRecord {});
	record.base = MovingObjectFieldsOf(context, entity, k_ScaffoldRow, kept.has_value() ? &kept->base : nullptr);
	record.base.mobileObject.object = referenceOrNone(scaffold.owner);
	// a loaded scaffold whose workshop the load did not make names it as its record did: that workshop is written as
	// the load kept it, its list naming this scaffold
	if (scaffold.owner == entt::null && kept.has_value() && kept->base.mobileObject.object != 0)
	{
		const auto target = context.TargetOf(kept->base.mobileObject.object);
		if (target.has_value() && target->entity == entt::null && !target->id.has_value())
		{
			record.base.mobileObject.object = kept->base.mobileObject.object;
		}
	}
	record.objects[0].number = referenceOrNone(scaffold.site);
	record.objects[1].number = ScaffoldPlanReference(context, entity, scaffold.plan, 0);
	record.objects[2].number = ScaffoldPlanReference(context, entity, scaffold.candidate, 1);
	record.firstWord = static_cast<uint32_t>(scaffold.fade);
	record.other.number = scaffold.holder.has_value() ? PlayerReference(context, *scaffold.holder) : 0;
	record.secondWord = std::bit_cast<uint32_t>(scaffold.phantomAngle);
	record.thirdWord = std::bit_cast<uint32_t>(scaffold.phantomScale);
	record.half = scaffold.flags;
	record.last.number = referenceOrNone(scaffold.town);
	record.lastWord = static_cast<uint32_t>(scaffold.limit);
	return record;
}

/// A reward chest's row of the reward table runs down from this one
constexpr uint32_t k_FirstRewardRow = 313;
/// A new chest's last drawn frame: none
constexpr uint32_t k_NoDrawnFrame = 0xFFFFFFFF;

/// A reward chest: its moving object's parts of its row of the reward table (while it falls, at the place it was given
/// at and its y angle as given: the fall turns only its model), its player and its town; then its words: the scripts'
/// alignment, its opened mark, a word nothing reads, its state, whether it has landed, whether it goes into the map's
/// cells at the next turn, its clock, the last frame it was drawn at, its making's unread number and its dust time. The
/// alignment, the unread words and the drawn frame, which openblack does not hold, are as kept, else as a new chest's.
/// An opened chest's town's miracle above it is refused: openblack draws it with no record, where the original writes
/// its own object
std::optional<gamesave::SavedRecord> WriteReward(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& chest = registry.Get<const components::Reward>(entity);
	if (chest.townSeed != entt::null && registry.Valid(chest.townSeed))
	{
		context.Fail(fmt::format("reward chest {} is open with its town's miracle above it, which has no record",
		                         entt::to_integral(entity)));
		return std::nullopt;
	}
	const auto kept = KeptOf<gamesave::RewardRecord>(context, entity);
	gamesave::RewardRecord record = kept.value_or(gamesave::RewardRecord {
	    .words = {0, 0, 0, 0, 0, 0, 0, k_NoDrawnFrame, 0, 0},
	});
	record.base = MovingObjectFieldsOf(context, entity, k_FirstRewardRow - static_cast<uint32_t>(chest.type),
	                                   kept.has_value() ? &kept->base : nullptr);
	if (chest.state == components::Reward::State::Falling)
	{
		const auto given = map_coords::FromWorld(chest.madeAt);
		auto& place = record.base.position.position;
		place = kept.has_value() ? kept->base.position.position
		                         : gamesave::MapCoords {.x = given.x, .z = given.z, .altitude = given.altitude};
		record.base.object.heading = kept.has_value() ? kept->base.object.heading : 0.0f;
		if (!kept.has_value())
		{
			record.base.object.coords = place;
		}
	}
	const auto referenceOrNone = [&context, &registry](entt::entity object) {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : gamesave::ObjectNumber {0};
	};
	record.objects[0].number = 0;
	record.objects[1].number = chest.player.has_value() ? PlayerReference(context, *chest.player) : 0;
	record.objects[2].number = referenceOrNone(chest.town);
	auto& words = record.words;
	words[1] = chest.opened ? 1 : 0;
	words[3] = static_cast<uint32_t>(chest.state);
	words[4] = chest.state != components::Reward::State::Falling ? 1 : 0;
	words[5] = chest.goesIntoMap ? 1 : 0;
	words[6] = std::bit_cast<uint32_t>(chest.seconds);
	words[9] = static_cast<uint32_t>(std::max(chest.dustMilliseconds, 0.0f));
	return record;
}

/// The football's row of the game's tables: the one after the map shields' two
constexpr uint32_t k_BallRow = 2082;

/// A football: its moving object's parts of its row, its point as its pitch's turn last took it and the one that turn
/// before, no pitch's game and no villagers (nothing in the game sets them), the town a villager's pick-up gave it to
/// (none for none), and its place and word as kept, else as a new ball's. Whether its town has taken it is not written,
/// as the game does not write it
std::optional<gamesave::SavedRecord> WriteBall(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& ball = registry.Get<const components::Ball>(entity);
	const auto kept = KeptOf<gamesave::BallRecord>(context, entity);
	gamesave::BallRecord record = kept.value_or(gamesave::BallRecord {});
	record.base = MovingObjectFieldsOf(context, entity, k_BallRow, kept.has_value() ? &kept->base : nullptr);
	record.point = {ball.point.x, ball.point.y, ball.point.z};
	record.previousPoint = {ball.previousPoint.x, ball.previousPoint.y, ball.previousPoint.z};
	record.pitch.number = 0;
	record.villager.number = 0;
	record.lastVillager.number = 0;
	record.town.number = ball.town != entt::null && registry.Valid(ball.town) ? context.Reference(ball.town) : 0;
	return record;
}

/// A game list of pots: those made since the load, the newest first (each pot's making puts it at its list's head),
/// then the list the load kept, less what has gone since. `ofList` says which pots' classes the list holds
std::vector<gamesave::ObjectNumber> PotList(WriteContext& context, GlobalList list, bool (*ofList)(uint32_t type))
{
	const auto& registry = context.GetRegistry();
	std::vector<std::pair<uint32_t, entt::entity>> made;
	const auto add = [&](entt::entity entity, uint32_t type) {
		if (!ofList(type) || context.KeptRecordOf(entity, type).has_value())
		{
			return;
		}
		const auto* index = registry.TryGet<const components::ObjectCreationIndex>(entity);
		made.emplace_back(index != nullptr ? index->value : 0, entity);
	};
	registry.Each<const components::Pot>([&](entt::entity entity, const components::Pot& /*pot*/) {
		if (const auto type = PotClassOf(registry, entity); type.has_value())
		{
			add(entity, *type);
		}
	});
	// the game's list of moving objects holds the plain ones, the orbs and the scaffolds too, each at its head as it is
	// made
	if (list == GlobalList::MobileObjects)
	{
		registry.Each<const components::MobileObject>([&](entt::entity entity, const components::MobileObject& /*mobile*/) {
			if (IsPlainMobileObject(registry, entity))
			{
				add(entity, gamesave::MobileObjectRecord::k_Type);
			}
			else if (IsPoo(registry, entity))
			{
				add(entity, gamesave::PooRecord::k_Type);
			}
		});
		registry.Each<const components::OneOffSpellSeed>([&](entt::entity entity, const components::OneOffSpellSeed& /*orb*/) {
			add(entity, gamesave::OneOffSpellSeedRecord::k_Type);
		});
		registry.Each<const components::Scaffold>([&](entt::entity entity, const components::Scaffold& /*scaffold*/) {
			add(entity, gamesave::ScaffoldRecord::k_Type);
		});
		registry.Each<const components::HanoiBlock>([&](entt::entity entity, const components::HanoiBlock& /*block*/) {
			add(entity, gamesave::HanoiBlockRecord::k_Type);
		});
		// and the reward chests, a moving object's class
		registry.Each<const components::Reward>(
		    [&](entt::entity entity, const components::Reward& /*chest*/) { add(entity, gamesave::RewardRecord::k_Type); });
		// and the footballs, another
		registry.Each<const components::Ball>(
		    [&](entt::entity entity, const components::Ball& /*ball*/) { add(entity, gamesave::BallRecord::k_Type); });
	}
	std::ranges::stable_sort(made, std::ranges::greater {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<gamesave::ObjectNumber> numbers;
	numbers.reserve(made.size());
	for (const auto& [index, entity] : made)
	{
		numbers.push_back(context.Reference(entity));
	}
	if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
	{
		for (const auto number : game->lists.at(static_cast<size_t>(list)).numbers)
		{
			const auto target = context.TargetOf(number);
			if (target.has_value() && target->entity != entt::null && !registry.Valid(target->entity))
			{
				continue;
			}
			numbers.push_back(number);
		}
	}
	return numbers;
}

bool IsFoodPileClass(uint32_t type)
{
	return type == gamesave::FoodPileRecord::k_Type || type == gamesave::MagicFoodRecord::k_Type;
}

bool IsAnyPotClass(uint32_t /*type*/)
{
	return true;
}

template <typename Record>
void RegisterPot(ClassWriters& writers)
{
	writers.Register(
	    Record::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return PotClassOf(registry, entity) == Record::k_Type; },
	    WritePot<Record>);
}
} // namespace

void openblack::ecs::saved_world_write::RegisterPotWriters(ClassWriters& writers)
{
	RegisterPot<gamesave::PotRecord>(writers);
	RegisterPot<gamesave::FoodPileRecord>(writers);
	RegisterPot<gamesave::WoodPileRecord>(writers);
	RegisterPot<gamesave::MagicFoodRecord>(writers);
	RegisterPot<gamesave::MagicWoodRecord>(writers);
	// the game's food piles (each food pile's making puts it there) and its mobile objects (each pot is one)
	writers.Register(gamesave::MobileObjectRecord::k_Type, IsPlainMobileObject, WriteMobileObject);
	writers.Register(gamesave::PooRecord::k_Type, IsPoo, WritePoo);
	writers.Register(
	    gamesave::OneOffSpellSeedRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::OneOffSpellSeed>(entity); },
	    WriteOrb);
	writers.Register(
	    gamesave::ScaffoldRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Scaffold>(entity); },
	    WriteScaffold);
	writers.Register(
	    gamesave::HanoiBlockRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::HanoiBlock, components::MobileObject>(entity);
	    },
	    WriteHanoiBlock);
	writers.Register(
	    gamesave::RewardRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Reward>(entity); }, WriteReward);
	writers.Register(
	    gamesave::BallRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Ball>(entity); }, WriteBall);
	writers.RegisterList(GlobalList::FoodPiles,
	                     [](WriteContext& context) { return PotList(context, GlobalList::FoodPiles, IsFoodPileClass); });
	writers.RegisterList(GlobalList::MobileObjects,
	                     [](WriteContext& context) { return PotList(context, GlobalList::MobileObjects, IsAnyPotClass); });
}

std::optional<gamesave::SavedRecord> openblack::ecs::saved_world_write::WriteScaffoldPlan(WriteContext& context, uint64_t id)
{
	const auto& registry = context.GetRegistry();
	const auto [owner, index] = PartOf(id);
	const auto* scaffold = registry.Valid(owner) ? registry.TryGet<const components::Scaffold>(owner) : nullptr;
	const auto* plan = scaffold == nullptr ? nullptr
	                   : index == 0        ? (scaffold->plan.has_value() ? &*scaffold->plan : nullptr)
	                   : index == 1        ? (scaffold->candidate.has_value() ? &*scaffold->candidate : nullptr)
	                                       : nullptr;
	if (plan == nullptr)
	{
		context.Fail(fmt::format("plan {} of scaffold {} is gone", index, entt::to_integral(owner)));
		return std::nullopt;
	}
	// where it is to stand, its angle and scale, the turn it was made, never built, on no footpath and in no town: a
	// scaffold's plans are never put in a town's list
	gamesave::PlannedAbodeRecord record;
	record.planned.position.position = {.x = plan->position.x, .z = plan->position.z, .altitude = plan->position.altitude};
	record.planned.yAngle = plan->yAngle;
	record.planned.scale = plan->scale;
	record.planned.creationTurn = static_cast<int32_t>(plan->creationTurn);
	record.planned.info = k_NoRow;
	if (plan->info != nullptr)
	{
		const auto* infos = Locator::infoConstants::has_value() ? &Locator::infoConstants::value().abode : nullptr;
		if (infos == nullptr || plan->info < &infos->front() || plan->info > &infos->back())
		{
			context.Fail(
			    fmt::format("plan {} of scaffold {} offers a building of no abode row", index, entt::to_integral(owner)));
			return std::nullopt;
		}
		record.planned.info =
		    ecs::saved_world_load::AbodeInfoRowOf(static_cast<AbodeInfo>(static_cast<int32_t>(plan->info - &infos->front())));
	}
	return record;
}
