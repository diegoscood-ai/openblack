/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns' records of a saved game's world file, in the original's order: the container's parts, the main storage
// pit, the desires, the town's own values, its lists, its belief, its players' dealings with it and its last values
// (docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town). A loaded town is written from the record
// its load kept, with the values the load put back on it. A town made in play is written from its components; a value
// openblack does not hold is the one a new town starts with (its memory is cleared as it is made, then its constructor
// sets its own values), and the values that change in play without openblack holding them are listed on the wiki page.

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <TownRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::LoadedRecord;
using ecs::components::Town;
using gamesave::SavedRecord;
using gamesave::TownRecord;

namespace
{
/// The number of the town's row among the game's tables (counted from the last row, as the saved games number them):
/// 2081, the one row every town is made with
constexpr uint32_t k_TownRow = 2081;
/// The citadel heart's row, a planned heart's too
constexpr uint32_t k_CitadelHeartRow = 2411;
/// The words the original's belief block, figures block and each player's last attack start with: the same in every
/// save the original writes, as its blocks are written whole from its memory
constexpr uint32_t k_BeliefBlockWord = 0x00999FEC;
constexpr uint32_t k_FiguresBlockWord = 0x0099A00C;
constexpr uint32_t k_AttackBlockWord = 0x008D8BA8;
/// A new town's trust in each player, which only the dealings openblack does not hold move
constexpr float k_NewTownTrust = 1.0f;
/// The players a town keeps its dealings with, the neutral one included, and the bytes of each player's
constexpr size_t k_Players = 8;
constexpr size_t k_PlayerInteractBytes = 0x80;
/// The kinds of harm a town keeps the damage of, by the cause of a death
constexpr size_t k_DeathReasons = 10;
/// The resources a town keeps the last taking of, by player
constexpr uint32_t k_Resources = 2;

/// Writes values one after another into a block written whole, as the original's memory holds them
class BlockWriter
{
public:
	explicit BlockWriter(std::span<uint8_t> block)
	    : _block(block)
	{
	}

	template <typename T>
	void Put(const T& value)
	{
		std::memcpy(_block.data() + _at, &value, sizeof(T));
		_at += sizeof(T);
	}

	template <typename T, size_t N>
	void PutEach(const std::array<T, N>& values)
	{
		for (const auto& value : values)
		{
			Put(value);
		}
	}

	/// Leaves bytes as they are: zero, as the town's memory is cleared as it is made
	void Skip(size_t bytes) { _at += bytes; }

	[[nodiscard]] size_t At() const { return _at; }

private:
	std::span<uint8_t> _block;
	size_t _at {0};
};

/// The number a record names an object by, none (0) for none
[[nodiscard]] gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

/// The objects of a list in its order, each by its number
[[nodiscard]] gamesave::RefList ListOf(WriteContext& context, const std::vector<entt::entity>& entities)
{
	gamesave::RefList list;
	for (const auto entity : entities)
	{
		if (entity != entt::null && context.GetRegistry().Valid(entity))
		{
			list.numbers.push_back(context.Reference(entity));
		}
	}
	return list;
}

template <size_t N>
[[nodiscard]] gamesave::Counted<float> CountedOf(const std::array<float, N>& values)
{
	return {.values = {values.begin(), values.end()}};
}

[[nodiscard]] gamesave::Counted<float> Zeros()
{
	return {.values = std::vector<float>(ecs::components::k_TownDesireCount, 0.0f)};
}

[[nodiscard]] gamesave::Counted<gamesave::DesireSort>
SortsOf(const std::array<ecs::components::DesireSort, ecs::components::k_TownDesireCount>& sorts)
{
	gamesave::Counted<gamesave::DesireSort> counted;
	for (const auto& sort : sorts)
	{
		uint32_t first = 0;
		std::memcpy(&first, &sort.boosts, sizeof(first));
		counted.values.push_back({.first = first, .value = sort.value, .desire = sort.index});
	}
	return counted;
}

/// The desires: what the scripts boost, the desire, the raw desire, the amounts and what is wanted, the two sorted
/// orders, the turns above the alignment's limit and the villagers' states serving each; the base, the modifier, the
/// change and the last turn, which nothing writes in play, are zero
[[nodiscard]] gamesave::TownDesireFields DesiresOf(WriteContext& context, entt::entity entity,
                                                   const ecs::components::TownDesire& desire)
{
	gamesave::TownDesireFields fields;
	fields.base = Zeros();
	fields.modifier = Zeros();
	fields.cheat = CountedOf(desire.boostA);
	fields.boost = CountedOf(desire.boost);
	fields.desire = CountedOf(desire.desire);
	fields.town.number = context.Reference(entity);
	fields.overall = desire.population;
	fields.raw = CountedOf(desire.raw);
	fields.functionMinimum = CountedOf(desire.amount);
	fields.functionMaximum = CountedOf(desire.desired);
	fields.change = Zeros();
	fields.sorts = SortsOf(desire.sorted);
	fields.sortsAgain = SortsOf(desire.sortedRaw);
	fields.alignmentChanges = {.values = {desire.alignmentTurns.begin(), desire.alignmentTurns.end()}};
	fields.previousVillagerStateAmount = CountedOf(desire.doingNowAtStart);
	fields.previousVillagerStateCount = CountedOf(desire.doingNowCountAtStart);
	fields.villagerStateAmount = CountedOf(desire.doingNow);
	fields.villagerStateCount = CountedOf(desire.doingNowCount);
	return fields;
}

/// The figures block: its word, the cleared word after it, then the counts in the original's order. The counts
/// openblack does not keep (the building sites, the plans, the wonders, the space left, the children and abodes added,
/// the last abode's turn and the average food and wood used) are zero, as a new town has them. A loaded town's block
/// starts from the one its load kept: its two words and the counts openblack does not keep stay as loaded, and its
/// counts are the ones it holds, as its load put them back and its process counts them again
[[nodiscard]] std::array<uint8_t, 280> FiguresOf(entt::entity entity, const Town& town,
                                                 const std::array<uint8_t, 280>* kept = nullptr)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto stats = kept != nullptr ? town.stats : ecs::town_stats::AsTheyStand(town.stats, entity);
	const auto* deaths = registry.TryGet<const ecs::components::TownDeaths>(entity);
	const ecs::components::TownDeaths none {};
	const auto& died = deaths != nullptr ? *deaths : none;
	std::array<uint8_t, 280> block = kept != nullptr ? *kept : std::array<uint8_t, 280> {};
	BlockWriter out(block);
	if (kept != nullptr)
	{
		out.Skip(2 * sizeof(uint32_t));
	}
	else
	{
		out.Put(k_FiguresBlockWord);
		out.Put(uint32_t {0});
	}
	out.Put(stats.adults);
	out.Put(stats.children);
	out.Put(stats.abodesWithPlaces);
	out.Skip(4); // the planned buildings
	out.Skip(4); // the building sites
	out.Put(stats.civicBuildings);
	out.Skip(4); // the civic building sites
	out.Put(stats.civicPlans);
	out.Skip(4); // the planned wonders
	out.Skip(4); // the wonder sites
	out.Skip(4); // the total space left in the abodes
	out.Put(stats.adultPlaces);
	out.Put(died.total38);
	out.Skip(4); // the children added
	out.Put(stats.childPlaces);
	out.Skip(4); // the abodes added
	out.Skip(4); // the wonders
	out.Put(stats.freeAdultPlaces);
	out.Skip(4); // the child space left in the abodes
	out.Put(stats.males);
	out.Put(stats.females);
	out.PutEach(died.total5C);
	out.PutEach(died.byReason);
	out.PutEach(died.byPlayer);
	out.PutEach(stats.disciples);
	out.Skip(3); // the padding after the disciples
	out.Put(died.lastDeathTurn);
	out.Put(died.count);
	out.Skip(4); // the turn the last abode was added
	out.Put(stats.foodForDinner);
	out.Put(town.foodUsed);
	out.Put(town.woodUsedForBuilding);
	out.Skip(4); // the average food used
	out.Skip(4); // the average wood used
	out.Put(stats.foodCarried);
	out.Put(stats.woodCarried);
	out.Put(stats.woodAtSites);
	out.Put(town.injured);
	out.PutEach(stats.abodesByNumber);
	return block;
}

/// The belief block: its word, the cleared word after it, then the belief's values by player, the boredom and the
/// desires' thresholds
[[nodiscard]] std::array<uint8_t, 464> BeliefOf(const ecs::components::TownBelief& belief)
{
	std::array<uint8_t, 464> block {};
	BlockWriter out(block);
	out.Put(k_BeliefBlockWord);
	out.Put(uint32_t {0});
	out.PutEach(belief.belief);
	out.PutEach(belief.recent);
	out.PutEach(belief.lastAddedTurn);
	out.PutEach(belief.cap);
	out.PutEach(belief.addedThisPeriod);
	out.PutEach(belief.reduceAccumulator);
	out.PutEach(belief.pending);
	out.PutEach(belief.boredom);
	out.PutEach(belief.desireThreshold);
	return block;
}

/// Each player's dealings with the town: the aggression held against it, the turn of its last attack, the trust (as a
/// new town has it), the damage done (none held), its last attack's burn, crush and hit, and the turn the town last
/// gave it belief for an artefact
[[nodiscard]] std::array<uint8_t, k_Players * k_PlayerInteractBytes> PlayerInteractOf(entt::entity entity, const Town& town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* gifts = registry.TryGet<const ecs::components::TownArtefactGifts>(entity);
	std::array<uint8_t, k_Players * k_PlayerInteractBytes> block {};
	for (size_t player = 0; player < k_Players; ++player)
	{
		BlockWriter out(std::span(block).subspan(player * k_PlayerInteractBytes, k_PlayerInteractBytes));
		const auto& effect = town.aggression.lastEffects.at(player);
		out.Skip(4);
		out.Put(town.aggression.aggression.at(player));
		out.Put(town.aggression.lastTurns.at(player));
		out.Skip(4);
		out.Put(k_NewTownTrust);
		out.Skip(4 * k_DeathReasons); // the damage done, by cause
		// the last attack's values: its word, the cleared word after it, then its burn, crush and hit
		out.Put(k_AttackBlockWord);
		out.Put(uint32_t {0});
		out.Put(effect.burn);
		out.Put(effect.crush);
		out.Put(effect.hit);
		out.Skip(4 * 4 + 4 + 4 + 12 + 4 + 4); // its other kinds, its scale, who applied it, its place and its player
		out.Put(gifts != nullptr ? gifts->lastGift.at(player) : uint32_t {0});
	}
	return block;
}

/// The town's forests, head first, by their keys
[[nodiscard]] gamesave::RefList ForestsOf(WriteContext& context, const Town& town)
{
	gamesave::RefList list;
	if (!Locator::forestSystem::has_value())
	{
		return list;
	}
	auto& lists = Locator::forestSystem::value().TownLists();
	const auto found = lists.find(town.id);
	if (found == lists.end())
	{
		return list;
	}
	for (const auto key : found->second)
	{
		list.numbers.push_back(
		    context.Reference(ecs::saved_world_write::SourceKey {.type = gamesave::ForestRecord::k_Type, .id = key}));
	}
	return list;
}

/// The town's desire flags, by the desire each flies for; none where it has none
[[nodiscard]] gamesave::RefList DesireFlagsOf(WriteContext& context, entt::entity entity)
{
	std::array<entt::entity, ecs::components::k_TownDesireCount> flags {};
	flags.fill(entt::null);
	context.GetRegistry().Each<const ecs::components::TownDesireFlag>(
	    [&flags, entity](entt::entity flag, const ecs::components::TownDesireFlag& each) {
		    const auto desire = static_cast<size_t>(each.desire);
		    if (each.town == entity && desire < flags.size())
		    {
			    flags.at(desire) = flag;
		    }
	    });
	gamesave::RefList list;
	for (const auto flag : flags)
	{
		list.numbers.push_back(ReferenceOrNone(context, flag));
	}
	return list;
}

/// The class of a town's plan: a planned town centre, a planned citadel heart or a planned abode
[[nodiscard]] uint32_t PlanTypeOf(const ecs::components::PlannedAbode& plan)
{
	if (plan.citadelHeart)
	{
		return gamesave::PlannedCitadelHeartRecord::k_Type;
	}
	return plan.townCentre ? gamesave::PlannedTownCentreRecord::k_Type : gamesave::PlannedAbodeRecord::k_Type;
}

/// The town's plans in its list's order, each by its number
[[nodiscard]] gamesave::RefList PlansOf(WriteContext& context, entt::entity entity, const Town& town)
{
	gamesave::RefList list;
	for (uint32_t index = 0; index < town.plannedAbodes.size(); ++index)
	{
		list.numbers.push_back(
		    context.Reference(SourceKey {.type = PlanTypeOf(town.plannedAbodes.at(index)), .id = PartId(entity, index)}));
	}
	return list;
}

/// A town made in play, from its components
std::optional<SavedRecord> WriteNewTown(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& town = registry.Get<const Town>(entity);
	const auto& magic = registry.Get<const ecs::components::TownMagic>(entity);
	const auto* influence = registry.TryGet<const ecs::components::TownInfluence>(entity);
	const auto* tribe = registry.TryGet<const Tribe>(entity);

	TownRecord record;
	record.container.position = PositionPartOf(context, entity, nullptr);
	record.container.info = k_TownRow;
	record.container.owner.number = PlayerReference(context, town.owner);
	record.mainStoragePit.number = ReferenceOrNone(context, town.storagePit);
	record.desires = DesiresOf(context, entity, town.desire);
	record.id = ecs::town_queries::ScriptIdOf(entity);
	record.tribe = static_cast<uint32_t>(tribe != nullptr ? *tribe : Tribe::NONE);
	record.playerNumber = static_cast<uint8_t>(town.owner);
	record.worshipPercentage = magic.worshipPercentage;
	record.worshipCount = magic.worshipping;
	record.influence = influence != nullptr ? influence->radius : 0.0f;
	record.villagersOnWayToWorship = static_cast<uint32_t>(magic.onWayToWorship);
	record.beliefInNeutralPlayer = town.belief.beliefInNeutralPlayer;
	record.buildingRequested = town.requestedPlanThisTurn ? 1 : 0;
	record.desireDirty = town.buildPulse;
	record.desireDirtyTimer = town.buildPulsePrevious;
	record.cannotBuildWorshipSite = magic.forbidWorshipSite ? 1 : 0;
	record.uninhabitable = town.uninhabitable ? 1 : 0;
	for (const auto pot : town.temporaryPots)
	{
		record.temporaryStores.numbers.push_back(ReferenceOrNone(context, pot));
	}
	record.forests = ForestsOf(context, town);
	record.stats = FiguresOf(entity, town);
	record.areaMin = {.x = town.areaMin.x, .z = town.areaMin.y, .altitude = 0.0f};
	record.areaMax = {.x = town.areaMax.x, .z = town.areaMax.y, .altitude = 0.0f};
	record.creche.number = ReferenceOrNone(context, town.creche);
	record.graveyard.number = ReferenceOrNone(context, town.graveyard);
	record.workshops = ListOf(context, town.workshops);
	record.abodes = ListOf(context, ecs::town_stats::AbodesOf(entity));
	record.homeless = ListOf(context, town.homelessVillagers);
	record.spellIcons = ListOf(context, magic.spellIcons);
	record.fields = ListOf(context, ecs::fields::TownFields(entity));
	record.fishFarms = ListOf(context, ecs::fish_farms::TownFishFarms(entity));
	record.buildingSites = ListOf(context, town.buildingSites);
	record.artifacts.numbers = TownArtifactsOf(context, entity);
	record.belief = BeliefOf(town.belief);
	record.playthings = ListOf(context, town.playthings);
	record.animals = ListOf(context, town.animals);
	record.worshipSite.number = ReferenceOrNone(context, magic.worshipSite);
	record.townCentre.number = ReferenceOrNone(context, town.centre);
	record.planned = PlansOf(context, entity, town);
	record.playerInteract = PlayerInteractOf(entity, town);
	record.villagersOnWayToWorshipSite = ListOf(context, magic.onWayVillagers);
	std::ranges::transform(magic.held, record.magicTypesHeld.begin(), [](bool held) { return held ? 1 : 0; });
	record.footballPitch.number = ReferenceOrNone(context, town.footballPitch);
	record.lastAttackingPlayer.number =
	    town.aggression.lastAggressor.has_value() ? PlayerReference(context, *town.aggression.lastAggressor) : 0;
	record.lastAttackedTurn = town.aggression.lastTurn;
	record.otherPlayersAggressorScale = town.aggression.protectionMultiplier;
	record.ownerAggressorScale = town.aggression.mercyMultiplier;
	record.ownerInteractionTotal = town.aggression.mercy;
	record.otherInteractionTotal = town.aggression.protection;
	record.worshipPercentageBeforeEmergency = town.savedWorshipPercentage;
	record.resourceLastRemovedTurn.rows = static_cast<uint32_t>(k_Players);
	record.resourceLastRemovedTurn.columns = k_Resources;
	for (const auto& player : town.resourceLastRemovedTurn)
	{
		record.resourceLastRemovedTurn.values.insert(record.resourceLastRemovedTurn.values.end(), player.begin(), player.end());
	}
	// the script adds each flock at the end of openblack's list; the original's adds it at the head
	const std::vector<entt::entity> flocks(town.flocks.rbegin(), town.flocks.rend());
	record.flocks = ListOf(context, flocks);
	record.desireFlags = DesireFlagsOf(context, entity);
	record.missionaries = ListOf(context, town.missionaries);
	record.balanceBeliefScale = town.belief.beliefScale;
	record.emptyTownTimer = town.emptyCountdown;
	return record;
}

/// A town: a loaded one as its load kept it, with the values the load put back on it; one made in play from its
/// components
std::optional<SavedRecord> WriteTown(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto* loaded = registry.TryGet<const LoadedRecord<TownRecord>>(entity);
	if (loaded == nullptr)
	{
		return WriteNewTown(context, entity);
	}
	const auto& town = registry.Get<const Town>(entity);
	TownRecord record = loaded->record;
	record.beliefInNeutralPlayer = town.belief.beliefInNeutralPlayer;
	// its belief as it stands, after the block's two leading words as kept
	const auto belief = BeliefOf(town.belief);
	std::copy(belief.begin() + 2 * sizeof(uint32_t), belief.end(), record.belief.begin() + 2 * sizeof(uint32_t));
	record.balanceBeliefScale = town.belief.beliefScale;
	// its counts, its deaths, the food and wood it used and its injured, as it holds them
	record.stats = FiguresOf(entity, town, &loaded->record.stats);
	record.emptyTownTimer = town.emptyCountdown;
	// the magic types it holds as they stand, a value that says the same kept as loaded
	if (const auto* magic = registry.TryGet<const ecs::components::TownMagic>(entity); magic != nullptr)
	{
		for (size_t type = 0; type < magic->held.size(); ++type)
		{
			if ((record.magicTypesHeld.at(type) != 0) != magic->held.at(type))
			{
				record.magicTypesHeld.at(type) = magic->held.at(type) ? 1 : 0;
			}
		}
	}
	// its two temporary pots as they stand: one deleted since the load has left its slot
	for (size_t slot = 0; slot < town.temporaryPots.size() && slot < record.temporaryStores.numbers.size(); ++slot)
	{
		record.temporaryStores.numbers.at(slot) = ReferenceOrNone(context, town.temporaryPots.at(slot));
	}
	// its plans as they stand: a building destroyed since leaves its rebuild plan there, a plan built is gone
	record.planned = PlansOf(context, entity, town);
	// its things to play with as it holds them: the load put its record's back, and a football taken since is in it
	record.playthings = ListOf(context, town.playthings);
	// the buildings destroyed since the load have left it, as a building leaves its town's lists as it is deleted:
	// its abodes, building sites and workshops in their saved order without them, and a creche or graveyard gone
	// replaced by the one the town holds now
	context.DropGone(record.abodes);
	context.DropGone(record.buildingSites);
	context.DropGone(record.workshops);
	// the villagers gone since have left it too, as a villager leaves its town as it is deleted: out of its homeless
	// villagers and of those on their way to its worship site, whose count goes down with each
	context.DropGone(record.homeless);
	const auto onWay = record.villagersOnWayToWorshipSite.numbers.size();
	context.DropGone(record.villagersOnWayToWorshipSite);
	const auto left = static_cast<uint32_t>(onWay - record.villagersOnWayToWorshipSite.numbers.size());
	record.villagersOnWayToWorship -= left;
	// and the other things that leave their town's lists as they are deleted: its animals, its spell icons, its
	// artifacts, its flocks and its fish farms
	context.DropGone(record.animals);
	context.DropGone(record.spellIcons);
	context.DropGone(record.artifacts);
	context.DropGone(record.flocks);
	context.DropGone(record.fishFarms);
	if (context.IsGone(record.creche.number))
	{
		record.creche.number = ReferenceOrNone(context, town.creche);
	}
	if (context.IsGone(record.graveyard.number))
	{
		record.graveyard.number = ReferenceOrNone(context, town.graveyard);
	}
	// its main storage pit and town centre gone since: the one it holds now, as a deleted pit or centre hands the
	// town's link to another functional one of its abodes, or none; its worship site gone since: none, as a deleted site
	// clears its towns' link (its football pitch's link stays as kept: a deleted pitch leaves it)
	if (context.IsGone(record.mainStoragePit.number))
	{
		record.mainStoragePit.number = ReferenceOrNone(context, town.storagePit);
	}
	if (context.IsGone(record.townCentre.number))
	{
		record.townCentre.number = ReferenceOrNone(context, town.centre);
	}
	record.worshipSite.number = context.UnlessGone(record.worshipSite.number);
	record.uninhabitable = town.uninhabitable ? 1 : 0;
	record.areaMin.x = town.areaMin.x;
	record.areaMin.z = town.areaMin.y;
	record.areaMax.x = town.areaMax.x;
	record.areaMax.z = town.areaMax.y;
	return record;
}
/// The plans of a class, by their towns' ids and places in their lists
[[nodiscard]] std::vector<uint64_t> PlanKeys(const ecs::Registry& registry, uint32_t type)
{
	std::vector<uint64_t> keys;
	registry.Each<const Town>([&keys, type](entt::entity entity, const Town& town) {
		for (uint32_t index = 0; index < town.plannedAbodes.size(); ++index)
		{
			if (PlanTypeOf(town.plannedAbodes.at(index)) == type)
			{
				keys.push_back(PartId(entity, index));
			}
		}
	});
	return keys;
}

/// A town's plan: where it is to stand in whole map units, its angle and scale, whether it is a building's rebuild,
/// the turn it was planned, its footpath link, its row and its town
template <typename Record>
std::optional<SavedRecord> WritePlan(WriteContext& context, uint64_t id)
{
	const auto& registry = context.GetRegistry();
	const auto [owner, index] = PartOf(id);
	const auto* town = registry.Valid(owner) ? registry.TryGet<const Town>(owner) : nullptr;
	if (town == nullptr || index >= town->plannedAbodes.size())
	{
		context.Fail("plan " + std::to_string(index) + " of town entity " + std::to_string(entt::to_integral(owner)) +
		             " is gone");
		return std::nullopt;
	}
	const auto& plan = town->plannedAbodes.at(index);
	if (plan.citadelHeart && plan.heartInfo != 0)
	{
		// the game's tables hold one citadel heart row
		context.Fail("town " + std::to_string(town->id) + " plans a citadel heart of the row " +
		             std::to_string(plan.heartInfo) + ", which the game's tables do not hold");
		return std::nullopt;
	}
	// a loaded plan from its record, its thing part and its place's height and flags as kept
	Record record;
	if (auto kept = context.KeptRecordOf(SourceKey {.type = Record::k_Type, .id = id}); kept.has_value())
	{
		if (auto* saved = std::get_if<Record>(&*kept); saved != nullptr)
		{
			record = *saved;
		}
	}
	// its place in whole map units: a kept one's while the plan stands where its record put it (the metres it holds
	// do not always give back the same units)
	auto& at = record.planned.position.position;
	if (map_coords::ToMetres(at.x) != plan.position.x || map_coords::ToMetres(at.z) != plan.position.z)
	{
		at.x = map_coords::ToFixed(plan.position.x);
		at.z = map_coords::ToFixed(plan.position.z);
	}
	record.planned.yAngle = plan.yAngleRadians;
	record.planned.scale = plan.scale;
	record.planned.wasConstructed = plan.wasBuilt ? 1 : 0;
	record.planned.creationTurn = static_cast<int32_t>(plan.creationTurn);
	record.planned.footpathLink.number =
	    plan.footpathLink != entt::null && registry.Valid(plan.footpathLink) ? context.Reference(plan.footpathLink) : 0;
	// a planned citadel heart is of the citadel heart's row
	record.planned.info = plan.citadelHeart ? k_CitadelHeartRow : ecs::saved_world_load::AbodeInfoRowOf(plan.info);
	record.town.number = context.Reference(owner, TownRecord::k_Type);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterTownWriters(ClassWriters& writers)
{
	// a planned abode is a town's plan or a scaffold's own plan, told apart by what holds it
	writers.RegisterSource(
	    gamesave::PlannedAbodeRecord::k_Type,
	    [](const Registry& registry) { return PlanKeys(registry, gamesave::PlannedAbodeRecord::k_Type); },
	    [](WriteContext& context, uint64_t id) {
		    const auto& registry = context.GetRegistry();
		    if (const auto owner = PartOf(id).owner; registry.Valid(owner) && registry.AllOf<ecs::components::Scaffold>(owner))
		    {
			    return WriteScaffoldPlan(context, id);
		    }
		    return WritePlan<gamesave::PlannedAbodeRecord>(context, id);
	    });
	writers.RegisterSource(
	    gamesave::PlannedTownCentreRecord::k_Type,
	    [](const Registry& registry) { return PlanKeys(registry, gamesave::PlannedTownCentreRecord::k_Type); },
	    WritePlan<gamesave::PlannedTownCentreRecord>);
	writers.RegisterSource(
	    gamesave::PlannedCitadelHeartRecord::k_Type,
	    [](const Registry& registry) { return PlanKeys(registry, gamesave::PlannedCitadelHeartRecord::k_Type); },
	    WritePlan<gamesave::PlannedCitadelHeartRecord>);
	writers.Register(
	    TownRecord::k_Type, [](const Registry& registry, entt::entity entity) { return registry.AllOf<Town>(entity); },
	    WriteTown);
}
