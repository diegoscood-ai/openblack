/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedWorldLoad.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <list>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TownDesireFlagArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Footpaths.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedWorld/WriteArenas.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/WalkGoal.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/WeatherThing.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/Citadel.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack::ecs;

namespace
{
/// The numbers of the first tree row and the first big forest row; the others of each follow it downwards
constexpr uint32_t k_FirstTreeRow = 2115;
constexpr uint32_t k_FirstBigForestRow = 369;
constexpr uint32_t k_FirstPotRow = 1876;
constexpr uint32_t k_FirstAbodeRow = 2394;
constexpr uint32_t k_FirstMobileStaticRow = 1937;
constexpr uint32_t k_FirstVillagerRow = 2247;
/// The tribe rows, read before the other tables: the first tribe's number, the others following it downwards
constexpr uint32_t k_FirstTribeRow = 742;
/// The spell seed rows: the first seed's number, the others following it downwards
constexpr uint32_t k_FirstSpellSeedRow = 2578;
/// A loaded villager's state counter when its states load nothing: 1 to 20
constexpr uint32_t k_LoadedCounterRange = 20;
/// A storage pit's first food pile and its five other piles
constexpr size_t k_PitWoodPiles = 5;
/// The game object's lists of every forest and of every town, among its global lists
constexpr size_t k_ForestList = 4;
constexpr size_t k_TownList = 31;
constexpr size_t k_FootpathList = 28;
constexpr size_t k_ClimateList = 38;
constexpr auto k_ReactionList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Reactions);
constexpr auto k_WeatherThingList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::WeatherThings);
constexpr auto k_FireList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::FireEffects);
constexpr auto k_ArenaList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Arenas);
constexpr auto k_DanceList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Dances);
/// The key frame and the action of a dance's part number: the key frame in the high half
constexpr uint32_t k_KeyFrameShift = 16;
constexpr uint32_t k_FirstClimateRow = 327;
constexpr uint32_t k_ClimateRows = 7;
/// A thing that is out of play (held, or in a script's hands) or flown by the particles
constexpr uint8_t k_ThingUnavailable = 1U << 0U;
constexpr uint8_t k_ThingFlownByParticles = 1U << 4U;
/// A thing whose state may not change (held), and one the physics moves
constexpr uint16_t k_PositionUnavailable = 1U << 2U;
constexpr uint16_t k_PositionInPhysics = 1U << 6U;

/// An object the load puts on the map: one that is not held or in flight
template <typename Record>
bool StandsOnTheMap(const Record& object)
{
	return (object.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
	       (object.base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0;
}

/// The row of a table whose first row has that number: the rows are numbered downwards
template <typename Type>
std::optional<Type> RowOf(uint32_t info, uint32_t firstRow)
{
	const auto rows = static_cast<uint32_t>(Type::_COUNT);
	if (info > firstRow || info + rows <= firstRow)
	{
		return std::nullopt;
	}
	return static_cast<Type>(firstRow - info);
}

/// The marks its saved place's flags give an object: held by a script, under a script's control, immovable, not to be
/// picked up and indestructible
void RestorePlaceMarks(entt::entity entity, uint16_t flags)
{
	namespace components = openblack::ecs::components;
	namespace place = openblack::ecs::saved_world_write::position_flags;
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto mark = [&registry, entity, flags]<typename Mark>(uint16_t bit) {
		if ((flags & bit) != 0)
		{
			registry.AssignOrReplace<Mark>(entity);
		}
	};
	mark.template operator()<components::InScript>(place::k_InScript);
	mark.template operator()<components::ScriptControlled>(place::k_ControlledByScript);
	mark.template operator()<components::Immovable>(place::k_Immovable);
	mark.template operator()<components::CannotBePickedUp>(place::k_CannotBePickedUp);
	mark.template operator()<components::Indestructible>(place::k_Indestructible);
}

/// The states that save what the villager works on, the thing it fishes at, fells or looks at
bool TargetsAThing(openblack::gamesave::VillagerStateSave save)
{
	using openblack::gamesave::VillagerStateSave;
	return save == VillagerStateSave::Fishing || save == VillagerStateSave::Foresting ||
	       save == VillagerStateSave::InspectObject;
}

/// Whether a loaded villager's state values are put back: none to put back, the dance states' two words, which
/// nothing of those states reads (kept with the record, and written back as loaded), or a dead villager's cause
bool StateValuesLoaded(uint8_t state, const std::vector<openblack::gamesave::StateValue>& values, uint8_t discipleType)
{
	using openblack::gamesave::VillagerStateSave;
	const auto save = openblack::gamesave::VillagerStateSaveOf(state);
	return values.empty() || openblack::gamesave::StateValueSizes(save, discipleType).empty() ||
	       save == VillagerStateSave::Dance || save == VillagerStateSave::Worship || save == VillagerStateSave::Dead ||
	       TargetsAThing(save);
}

/// The made object keeps its saved creation number, its life (a Life only when not full), its place's marks and its
/// record
template <typename Record>
void KeepSaved(entt::entity entity, const Record& record)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& object = record.base.object;
	RestorePlaceMarks(entity, record.base.position.flags);
	registry.AssignOrReplace<openblack::ecs::components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<openblack::ecs::components::Life>(entity, object.life);
	}
	registry.Assign<openblack::ecs::components::LoadedRecord<Record>>(entity, record);
}

openblack::map_coords::MapCoords CoordsOf(const openblack::gamesave::MapCoords& coords)
{
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}
} // namespace

std::optional<openblack::TreeInfo> saved_world_load::TreeTypeOfInfo(uint32_t info)
{
	return RowOf<TreeInfo>(info, k_FirstTreeRow);
}

std::optional<openblack::BigForestInfo> saved_world_load::BigForestTypeOfInfo(uint32_t info)
{
	return RowOf<BigForestInfo>(info, k_FirstBigForestRow);
}

uint32_t saved_world_load::TreeInfoRowOf(TreeInfo type)
{
	return k_FirstTreeRow - static_cast<uint32_t>(type);
}

uint32_t saved_world_load::BigForestInfoRowOf(BigForestInfo type)
{
	return k_FirstBigForestRow - static_cast<uint32_t>(type);
}

std::optional<openblack::PotInfo> saved_world_load::PotTypeOfInfo(uint32_t info)
{
	return RowOf<PotInfo>(info, k_FirstPotRow);
}

std::optional<openblack::AbodeInfo> saved_world_load::AbodeTypeOfInfo(uint32_t info)
{
	return RowOf<AbodeInfo>(info, k_FirstAbodeRow);
}

std::optional<openblack::VillagerInfo> saved_world_load::VillagerTypeOfInfo(uint32_t info)
{
	return RowOf<VillagerInfo>(info, k_FirstVillagerRow);
}

bool saved_world_load::VillagerCounterDrawnOnLoad(uint8_t topState, uint8_t finalState)
{
	using gamesave::VillagerStateSave;
	return gamesave::VillagerStateSaveOf(topState) == VillagerStateSave::None &&
	       gamesave::VillagerStateSaveOf(finalState) == VillagerStateSave::None;
}

std::optional<openblack::MobileStaticInfo> saved_world_load::MobileStaticTypeOfInfo(uint32_t info)
{
	return RowOf<MobileStaticInfo>(info, k_FirstMobileStaticRow);
}

uint32_t saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo type)
{
	return k_FirstMobileStaticRow - static_cast<uint32_t>(type);
}

saved_world_load::WorldPlan saved_world_load::PlanOf(const gamesave::WorldRead& world)
{
	WorldPlan plan {.census = world.Census()};
	const auto recordOf = [&world](gamesave::ObjectNumber number) -> const gamesave::SavedRecord* {
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			return nullptr;
		}
		return &*world.objects[number - 1];
	};

	// The forests: the game's list holds them newest first, and a load keeps its order
	std::vector<gamesave::ObjectNumber> listed;
	std::vector<gamesave::ObjectNumber> listedTowns;
	std::vector<gamesave::ObjectNumber> listedFootpaths;
	if (const auto* first = recordOf(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			listed = game->lists[k_ForestList].numbers;
			listedTowns = game->lists[k_TownList].numbers;
			listedFootpaths = game->lists[k_FootpathList].numbers;
			// the climates as the list holds them, the newest first
			for (const auto number : game->lists[k_ClimateList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* climate = record != nullptr ? std::get_if<gamesave::ClimateRecord>(record) : nullptr;
				    climate != nullptr)
				{
					plan.climates.push_back({.number = number, .record = *climate});
				}
			}
			// the reactions as the list holds them, the newest first, each with its player's number
			for (const auto number : game->lists[k_ReactionList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* reaction = record != nullptr ? std::get_if<gamesave::ReactionRecord>(record) : nullptr;
				if (reaction == nullptr)
				{
					continue;
				}
				PlannedReaction planned {.number = number, .record = *reaction};
				const auto* playerRecord = recordOf(reaction->third.number);
				if (const auto* player = playerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(playerRecord) : nullptr;
				    player != nullptr && player->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(player->playerNumber);
				}
				plan.reactions.push_back(planned);
			}
			// the things of the weather as the list holds them, the newest first
			for (const auto number : game->lists[k_WeatherThingList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* thing = record != nullptr ? std::get_if<gamesave::WeatherThingRecord>(record) : nullptr;
				    thing != nullptr)
				{
					plan.weatherThings.push_back({.number = number, .record = *thing});
				}
			}
			// the fires as the list holds them, the newest first, each with its player
			// the dances as the list holds them, the newest first, each with its groups
			for (const auto number : game->lists[k_DanceList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* dance = record != nullptr ? std::get_if<gamesave::DanceRecord>(record) : nullptr;
				if (dance == nullptr)
				{
					continue;
				}
				PlannedDance planned {.number = number, .record = *dance};
				for (const auto group : dance->group.danceGroups.numbers)
				{
					const auto* groupRecord = recordOf(group);
					if (const auto* saved =
					        groupRecord != nullptr ? std::get_if<gamesave::DanceGroupRecord>(groupRecord) : nullptr)
					{
						planned.groups.emplace_back(group, *saved);
					}
				}
				for (const auto frame : dance->group.keyFrames.numbers)
				{
					const auto* frameRecord = recordOf(frame);
					if (const auto* saved =
					        frameRecord != nullptr ? std::get_if<gamesave::DanceKeyFrameRecord>(frameRecord) : nullptr)
					{
						plan.keyFrames.emplace(frame, *saved);
					}
				}
				plan.dances.push_back(std::move(planned));
			}
			for (const auto number : game->lists[k_FireList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* fire = record != nullptr ? std::get_if<gamesave::FireEffectRecord>(record) : nullptr;
				if (fire == nullptr)
				{
					continue;
				}
				PlannedFire planned {.number = number, .record = *fire};
				const auto* ownerRecord = recordOf(fire->objects[2].number);
				if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
				    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(owner->playerNumber);
				}
				plan.fires.push_back(planned);
			}
			// the arenas as the list holds them, the newest first
			for (const auto number : game->lists[k_ArenaList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* arena = record != nullptr ? std::get_if<gamesave::ArenaRecord>(record) : nullptr;
				    arena != nullptr)
				{
					plan.arenas.push_back({.number = number, .record = *arena});
				}
			}
			// each player's towns, in the order its list holds them
			for (const auto player : game->players)
			{
				const auto* record = recordOf(player);
				if (const auto* owner = record != nullptr ? std::get_if<gamesave::PlayerRecord>(record) : nullptr;
				    owner != nullptr)
				{
					plan.ownersTowns.insert(plan.ownersTowns.end(), owner->towns.begin(), owner->towns.end());
				}
			}
		}
	}
	const std::unordered_set<gamesave::ObjectNumber> inList(listed.begin(), listed.end());
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr;
		if (forest == nullptr)
		{
			continue;
		}
		// every forest writes the two values of the whole game, and each one read sets them
		plan.lastTreeCreatedTurn = forest->firstGameWord;
		plan.nextForestId = forest->secondGameWord;
		if (!inList.contains(number))
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}
	for (const auto number : listed | std::views::reverse)
	{
		const auto* record = recordOf(number);
		if (const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr; forest != nullptr)
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}

	// The towns, the same way as the forests: the list holds them newest first
	const std::unordered_set<gamesave::ObjectNumber> townsInList(listedTowns.begin(), listedTowns.end());
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* town = record != nullptr ? std::get_if<gamesave::TownRecord>(record) : nullptr;
		if (town != nullptr && !townsInList.contains(number))
		{
			plan.towns.push_back({.number = number, .record = *town});
		}
	}
	for (const auto number : listedTowns | std::views::reverse)
	{
		const auto* record = recordOf(number);
		if (const auto* town = record != nullptr ? std::get_if<gamesave::TownRecord>(record) : nullptr; town != nullptr)
		{
			plan.towns.push_back({.number = number, .record = *town});
		}
	}

	// The footpaths, the same way: the list holds them newest first; each with its nodes in its list's order. Then the
	// footpath links
	const auto footpathOf = [&recordOf](gamesave::ObjectNumber number) -> std::optional<PlannedFootpath> {
		const auto* record = recordOf(number);
		const auto* footpath = record != nullptr ? std::get_if<gamesave::FootpathRecord>(record) : nullptr;
		if (footpath == nullptr)
		{
			return std::nullopt;
		}
		PlannedFootpath planned {.number = number, .record = *footpath};
		for (const auto node : footpath->nodes.numbers)
		{
			const auto* nodeRecord = recordOf(node);
			if (const auto* saved = nodeRecord != nullptr ? std::get_if<gamesave::FootpathNodeRecord>(nodeRecord) : nullptr;
			    saved != nullptr)
			{
				planned.nodes.push_back(*saved);
			}
		}
		return planned;
	};
	const std::unordered_set<gamesave::ObjectNumber> footpathsInList(listedFootpaths.begin(), listedFootpaths.end());
	for (const auto number : world.loadOrder)
	{
		if (footpathsInList.contains(number))
		{
			continue;
		}
		if (auto footpath = footpathOf(number); footpath.has_value())
		{
			plan.footpaths.push_back(std::move(*footpath));
		}
	}
	for (const auto number : listedFootpaths | std::views::reverse)
	{
		if (auto footpath = footpathOf(number); footpath.has_value())
		{
			plan.footpaths.push_back(std::move(*footpath));
		}
	}
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* link = record != nullptr ? std::get_if<gamesave::FootpathLinkRecord>(record) : nullptr; link != nullptr)
		{
			plan.footpathLinks.push_back({.number = number, .record = *link});
		}
	}

	// The rivers and the banks of mist, in the order their links are put back
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* stream = std::get_if<gamesave::StreamRecord>(record); stream != nullptr)
		{
			plan.streams.push_back({.number = number, .record = *stream});
		}
		else if (const auto* mist = std::get_if<gamesave::MistRecord>(record); mist != nullptr)
		{
			plan.mists.push_back({.number = number, .record = *mist});
		}
	}

	// The trees and big forests, in the order their links are put back: the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* tree = std::get_if<gamesave::TreeRecord>(record); tree != nullptr && StandsOnTheMap(*tree))
		{
			if (const auto type = TreeTypeOfInfo(tree->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedTree {.number = number, .type = *type, .record = *tree});
			}
		}
		else if (const auto* big = std::get_if<gamesave::BigForestRecord>(record); big != nullptr && StandsOnTheMap(*big))
		{
			if (const auto type = BigForestTypeOfInfo(big->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedBigForest {.number = number, .type = *type, .record = *big});
			}
		}
		else if (const auto* wood = std::get_if<gamesave::WoodPileRecord>(record); wood != nullptr && StandsOnTheMap(*wood))
		{
			if (const auto type = PotTypeOfInfo(wood->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(
				    PlannedPile {.number = number, .type = *type, .base = wood->base, .pile = wood->pile, .record = *wood});
			}
		}
		else if (const auto* food = std::get_if<gamesave::FoodPileRecord>(record); food != nullptr && StandsOnTheMap(*food))
		{
			if (const auto type = PotTypeOfInfo(food->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(
				    PlannedPile {.number = number, .type = *type, .base = food->base, .pile = food->pile, .record = *food});
			}
		}
		else if (const auto* heart = std::get_if<gamesave::CitadelHeartRecord>(record); heart != nullptr)
		{
			// its citadel's player: the citadel's owner's number
			PlannedCitadelHeart planned {.number = number, .record = *heart};
			if (const auto* citadelRecord = recordOf(heart->citadel.number); citadelRecord != nullptr)
			{
				if (const auto* citadel = std::get_if<gamesave::CitadelRecord>(citadelRecord); citadel != nullptr)
				{
					planned.citadel = *citadel;
					const auto* ownerRecord = recordOf(citadel->container.owner.number);
					if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
					    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
					{
						planned.owner = static_cast<PlayerNames>(owner->playerNumber);
					}
				}
			}
			plan.objects.emplace_back(std::move(planned));
		}
		else if (const auto* flag = std::get_if<gamesave::TownDesireFlagsRecord>(record); flag != nullptr)
		{
			plan.objects.emplace_back(PlannedDesireFlag {.number = number, .record = *flag});
		}
		else if (const auto* abode = std::get_if<gamesave::AbodeRecord>(record); abode != nullptr && StandsOnTheMap(*abode))
		{
			if (const auto type = AbodeTypeOfInfo(abode->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = abode->base, .abode = abode->abode, .record = *abode});
			}
		}
		else if (const auto* creche = std::get_if<gamesave::CrecheRecord>(record); creche != nullptr && StandsOnTheMap(*creche))
		{
			if (const auto type = AbodeTypeOfInfo(creche->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = creche->base, .abode = creche->abode, .record = *creche});
			}
		}
		else if (const auto* graveyard = std::get_if<gamesave::GraveyardRecord>(record);
		         graveyard != nullptr && StandsOnTheMap(*graveyard))
		{
			if (const auto type = AbodeTypeOfInfo(graveyard->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = graveyard->base, .abode = graveyard->abode, .record = *graveyard});
			}
		}
		else if (const auto* pit = std::get_if<gamesave::StoragePitRecord>(record); pit != nullptr && StandsOnTheMap(*pit))
		{
			if (const auto type = AbodeTypeOfInfo(pit->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedStoragePit {.number = number, .type = *type, .record = *pit});
			}
		}
		else if (const auto* rock = std::get_if<gamesave::RockRecord>(record); rock != nullptr && StandsOnTheMap(*rock))
		{
			if (const auto type = MobileStaticTypeOfInfo(rock->base.object.info); type.has_value())
			{
				// its player: its player record's number
				std::optional<PlayerNames> player;
				const auto* ownerRecord = recordOf(rock->object.number);
				if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
				    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					player = static_cast<PlayerNames>(owner->playerNumber);
				}
				plan.objects.emplace_back(PlannedMobileStatic {.number = number,
				                                               .type = *type,
				                                               .base = rock->base,
				                                               .mobileStatic = rock->mobileStatic,
				                                               .player = player,
				                                               .record = *rock});
			}
		}
		else if (const auto* statics = std::get_if<gamesave::MobileStaticRecord>(record);
		         statics != nullptr && StandsOnTheMap(*statics))
		{
			if (const auto type = MobileStaticTypeOfInfo(statics->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedMobileStatic {.number = number,
				                                               .type = *type,
				                                               .base = statics->base,
				                                               .mobileStatic = statics->mobileStatic,
				                                               .record = *statics});
			}
		}
	}
	// the worship sites, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* site = record != nullptr ? std::get_if<gamesave::WorshipSiteRecord>(record) : nullptr)
		{
			PlannedWorshipSite planned {.number = number, .record = *site, .totemNumber = site->totem.number};
			const auto* totem = recordOf(site->totem.number);
			if (const auto* saved = totem != nullptr ? std::get_if<gamesave::WorshipTotemRecord>(totem) : nullptr)
			{
				planned.totem = *saved;
			}
			// its need signs, the visuals its needs' record lists
			const auto* needs = recordOf(site->showNeeds.number);
			if (const auto* shown = needs != nullptr ? std::get_if<gamesave::ShowNeedsRecord>(needs) : nullptr)
			{
				for (size_t i = 0; i < planned.signs.size() && i < shown->visuals.numbers.size(); ++i)
				{
					const auto visual = shown->visuals.numbers[i];
					planned.signs[i].first = visual;
					const auto* sign = recordOf(visual);
					if (const auto* saved = sign != nullptr ? std::get_if<gamesave::ShowNeedsVisualsRecord>(sign) : nullptr)
					{
						planned.signs[i].second = *saved;
					}
				}
			}
			// its spell icons, in its list's order
			for (const auto icon : site->icons.numbers)
			{
				const auto* iconRecord = recordOf(icon);
				if (const auto* saved =
				        iconRecord != nullptr ? std::get_if<gamesave::WorshipSpellIconRecord>(iconRecord) : nullptr)
				{
					planned.icons.emplace_back(icon, *saved);
				}
			}
			plan.worshipSites.push_back(std::move(planned));
		}
	}
	// the villagers, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* villager = std::get_if<gamesave::VillagerRecord>(record); villager != nullptr)
		{
			plan.villagers.push_back({.number = number,
			                          .type = VillagerTypeOfInfo(villager->object.info).value_or(VillagerInfo::None),
			                          .record = *villager});
		}
		else if (const auto* special = std::get_if<gamesave::SpecialVillagerRecord>(record); special != nullptr)
		{
			plan.villagers.push_back({.number = number,
			                          .type = VillagerTypeOfInfo(special->object.info).value_or(VillagerInfo::None),
			                          .record = *special});
		}
	}
	// the fires' two tags, as the last fire read leaves them (each fire's read ends by reading them)
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* fire = record != nullptr ? std::get_if<gamesave::FireEffectRecord>(record) : nullptr; fire != nullptr)
		{
			plan.fireTags = {fire->lastFirstMark, fire->lastSecondMark};
		}
	}
	return plan;
}

saved_world_load::Census saved_world_load::Apply(const WorldPlan& plan, const Makers& makers,
                                                 saved_world_write::LoadedObjects* loaded)
{
	// each object made, by its number in the file, for the next save's writer and for the reactions that name it
	std::unordered_map<gamesave::ObjectNumber, entt::entity> entities;
	const auto keep = [loaded, &entities](gamesave::ObjectNumber number, entt::entity entity, uint32_t type) {
		if (entity != entt::null)
		{
			entities.try_emplace(number, entity);
		}
		if (loaded != nullptr && entity != entt::null)
		{
			loaded->try_emplace(number, saved_world_write::LoadedObject {.entity = entity, .type = type});
		}
	};
	// an object no entity stands for, by its class and its id there
	const auto keepSource = [loaded](gamesave::ObjectNumber number, uint32_t type, uint64_t id) {
		if (loaded != nullptr)
		{
			loaded->try_emplace(number, saved_world_write::LoadedObject {.type = type, .id = id});
		}
	};
	const auto typeOf = [](const auto& record) {
		return std::visit([](const auto& saved) { return std::decay_t<decltype(saved)>::k_Type; }, record);
	};
	auto& forests = Locator::forestSystem::value();
	std::unordered_map<gamesave::ObjectNumber, uint32_t> forestKeys; // each saved forest's key
	for (const auto& [number, record] : plan.forests)
	{
		// the forest keeps its saved id, which another forest may share; the counter is put back below
		const auto key = forests.Create(record.id, map_coords::ToWorld(CoordsOf(record.container.position.position)));
		auto& forest = forests.All().at(key);
		forest.id = record.id;
		forest.scenic = record.isScenic != 0;
		forest.saved = record;
		forestKeys.emplace(number, key);
		keepSource(number, gamesave::ForestRecord::k_Type, key);
	}
	if (plan.lastTreeCreatedTurn.has_value())
	{
		forests.SetLastTreeCreatedTurn(*plan.lastTreeCreatedTurn);
	}
	if (plan.nextForestId.has_value())
	{
		forests.SetNextId(*plan.nextForestId);
	}

	const auto keyOf = [&forestKeys](gamesave::ObjectNumber number) {
		const auto found = forestKeys.find(number);
		return found != forestKeys.end() ? found->second : 0U;
	};

	// the footpaths, then the links that list them
	std::unordered_map<gamesave::ObjectNumber, entt::entity> footpathsMade;
	for (const auto& footpath : plan.footpaths)
	{
		if (const auto made = makers.footpath ? makers.footpath(footpath) : entt::entity {entt::null}; made != entt::null)
		{
			footpathsMade.emplace(footpath.number, made);
			keep(footpath.number, made, gamesave::FootpathRecord::k_Type);
		}
	}
	std::unordered_map<gamesave::ObjectNumber, entt::entity> linksMade;
	for (const auto& link : plan.footpathLinks)
	{
		std::vector<entt::entity> listed;
		for (const auto number : link.record.footpaths.numbers)
		{
			if (const auto found = footpathsMade.find(number); found != footpathsMade.end())
			{
				listed.push_back(found->second);
			}
		}
		if (const auto made = makers.footpathLink ? makers.footpathLink(link, listed) : entt::entity {entt::null};
		    made != entt::null)
		{
			linksMade.emplace(link.number, made);
			keep(link.number, made, gamesave::FootpathLinkRecord::k_Type);
		}
	}
	// a thing made gets the footpath link its record names
	const auto giveLink = [&linksMade](entt::entity thing, gamesave::ObjectNumber link) {
		if (const auto found = linksMade.find(link); found != linksMade.end() && Locator::entitiesRegistry::has_value())
		{
			footpaths::SetFootpathLink(thing, found->second);
		}
	};

	// the climates, then the rivers and the banks of mist
	const auto climatesMade = makers.climates ? static_cast<uint32_t>(makers.climates(plan.climates)) : 0U;
	if (climatesMade != 0)
	{
		// each climate by its id
		for (const auto& climate : plan.climates)
		{
			keepSource(climate.number, gamesave::ClimateRecord::k_Type, climate.record.words[2]);
		}
	}
	// the things of the weather, from the list's last to its first, each at the head of the things' list
	uint32_t weatherThingsMade = 0;
	for (const auto& thing : plan.weatherThings | std::views::reverse)
	{
		const auto made = makers.weatherThing ? makers.weatherThing(thing) : entt::entity {entt::null};
		weatherThingsMade += made != entt::null ? 1 : 0;
		keep(thing.number, made, gamesave::WeatherThingRecord::k_Type);
	}
	// the arenas, from the list's last to its first, so that the newest is made last: each a land's arena, among the
	// entities made and, for the writer, the source's object of its entity
	uint32_t arenasMade = 0;
	for (const auto& arena : plan.arenas | std::views::reverse)
	{
		const auto made = makers.arena ? makers.arena(arena) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++arenasMade;
			entities.try_emplace(arena.number, made);
			keepSource(arena.number, gamesave::ArenaRecord::k_Type, saved_world_write::LandArenaId(made));
		}
	}
	uint32_t streamsMade = 0;
	for (const auto& stream : plan.streams)
	{
		const auto made = makers.stream ? makers.stream(stream) : entt::entity {entt::null};
		streamsMade += made != entt::null ? 1 : 0;
		keep(stream.number, made, gamesave::StreamRecord::k_Type);
	}
	uint32_t mistsMade = 0;
	for (const auto& mist : plan.mists)
	{
		const auto made = makers.mist ? makers.mist(mist) : entt::entity {entt::null};
		mistsMade += made != entt::null ? 1 : 0;
		keep(mist.number, made, gamesave::MistRecord::k_Type);
	}

	// the towns, each with its forests in its list's order (the head first)
	std::unordered_map<gamesave::ObjectNumber, entt::entity> towns;
	for (const auto& town : plan.towns)
	{
		const auto made = makers.town ? makers.town(town) : entt::entity {entt::null};
		if (made == entt::null)
		{
			continue;
		}
		towns.emplace(town.number, made);
		keep(town.number, made, gamesave::TownRecord::k_Type);
		if (const auto* component = Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(made)
		                                ? Locator::entitiesRegistry::value().TryGet<components::Town>(made)
		                                : nullptr;
		    component != nullptr)
		{
			auto& list = forests.TownLists()[component->id];
			list.clear();
			for (const auto forest : town.record.forests.numbers)
			{
				if (const auto key = keyOf(forest); key != 0)
				{
					list.push_back(key);
				}
			}
		}
	}
	// each player's list of towns in its saved order: the owner's list stamps follow it
	if (Locator::entitiesRegistry::has_value())
	{
		for (const auto number : plan.ownersTowns)
		{
			if (const auto found = towns.find(number); found != towns.end())
			{
				auto& registry = Locator::entitiesRegistry::value();
				if (auto* component =
				        registry.Valid(found->second) ? registry.TryGet<components::Town>(found->second) : nullptr;
				    component != nullptr)
				{
					component->ownerListStamp = town_queries::NextOwnerListStamp();
				}
			}
		}
	}
	// the towns' keys, which their buildings name them by
	std::unordered_map<gamesave::ObjectNumber, uint32_t> townKeys;
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& [number, made] : towns)
		{
			if (const auto* component = registry.Valid(made) ? registry.TryGet<components::Town>(made) : nullptr;
			    component != nullptr)
			{
				townKeys.emplace(number, component->id);
			}
		}
	}
	const auto townKeyOf = [&townKeys](gamesave::ObjectNumber number) {
		const auto found = townKeys.find(number);
		return found != townKeys.end() ? found->second : components::Abode::k_NoTown;
	};
	uint32_t treesMade = 0;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> bigForests;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> piles;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> storagePits;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> hearts;
	/// the abodes, creches and graveyards made, by class
	std::unordered_map<uint32_t, uint32_t> abodesMade;
	uint32_t desireFlagsMade = 0;
	/// the rocks and other mobile statics made, by class
	std::unordered_map<uint32_t, uint32_t> mobileStaticsMade;
	/// each citadel's first heart, which keeps the citadel
	std::unordered_map<gamesave::ObjectNumber, entt::entity> citadels;
	const auto pileOf = [&piles](gamesave::ObjectNumber number) {
		const auto found = piles.find(number);
		return found != piles.end() ? found->second : entt::entity {entt::null};
	};
	for (const auto& object : plan.objects)
	{
		if (const auto* tree = std::get_if<PlannedTree>(&object); tree != nullptr)
		{
			if (const auto made = makers.tree(*tree, keyOf(tree->record.forest.number)); made != entt::null)
			{
				++treesMade;
				keep(tree->number, made, gamesave::TreeRecord::k_Type);
			}
		}
		else if (const auto* big = std::get_if<PlannedBigForest>(&object); big != nullptr)
		{
			if (const auto made = makers.bigForest(*big, keyOf(big->record.forest.number)); made != entt::null)
			{
				bigForests.emplace(big->number, made);
				keep(big->number, made, gamesave::BigForestRecord::k_Type);
				giveLink(made, big->record.base.multi.footpathLink);
			}
		}
		else if (const auto* pile = std::get_if<PlannedPile>(&object); pile != nullptr && makers.pile)
		{
			if (const auto made = makers.pile(*pile); made != entt::null)
			{
				piles.emplace(pile->number, made);
				keep(pile->number, made, typeOf(pile->record));
			}
		}
		else if (const auto* pit = std::get_if<PlannedStoragePit>(&object); pit != nullptr && makers.storagePit)
		{
			// its piles were read inside it, so they are made already: its food pile, then its five other piles
			archetypes::AbodeArchetype::StoragePitPiles pitPiles;
			if (!pit->record.foodPile.numbers.empty())
			{
				pitPiles.food = pileOf(pit->record.foodPile.numbers.front());
			}
			for (size_t i = 0; i < k_PitWoodPiles && i < pit->record.otherPiles.numbers.size(); ++i)
			{
				pitPiles.wood.at(i) = pileOf(pit->record.otherPiles.numbers.at(i));
			}
			if (const auto made = makers.storagePit(*pit, townKeyOf(pit->record.abode.town.number), pitPiles);
			    made != entt::null)
			{
				storagePits.emplace(pit->number, made);
				keep(pit->number, made, gamesave::StoragePitRecord::k_Type);
				giveLink(made, pit->record.base.multi.footpathLink);
			}
		}
		else if (const auto* abode = std::get_if<PlannedAbode>(&object); abode != nullptr && makers.abode)
		{
			if (const auto made = makers.abode(*abode, townKeyOf(abode->abode.town.number)); made != entt::null)
			{
				++abodesMade[typeOf(abode->record)];
				keep(abode->number, made, typeOf(abode->record));
				giveLink(made, abode->base.multi.footpathLink);
			}
		}
		else if (const auto* flag = std::get_if<PlannedDesireFlag>(&object); flag != nullptr && makers.desireFlag)
		{
			const auto town = towns.find(flag->record.town.number);
			if (town == towns.end())
			{
				continue;
			}
			if (const auto made = makers.desireFlag(*flag, town->second); made != entt::null)
			{
				++desireFlagsMade;
				keep(flag->number, made, gamesave::TownDesireFlagsRecord::k_Type);
			}
		}
		else if (const auto* statics = std::get_if<PlannedMobileStatic>(&object); statics != nullptr && makers.mobileStatic)
		{
			if (const auto made = makers.mobileStatic(*statics); made != entt::null)
			{
				++mobileStaticsMade[typeOf(statics->record)];
				keep(statics->number, made, typeOf(statics->record));
				giveLink(made, statics->base.multi.footpathLink);
			}
		}
		else if (const auto* heart = std::get_if<PlannedCitadelHeart>(&object); heart != nullptr && makers.citadelHeart)
		{
			const auto found = citadels.find(heart->record.citadel.number);
			const auto citadel = found != citadels.end() ? found->second : entt::entity {entt::null};
			if (const auto made = makers.citadelHeart(*heart, citadel); made != entt::null)
			{
				hearts.emplace(heart->number, made);
				keep(heart->number, made, gamesave::CitadelHeartRecord::k_Type);
				// the citadel's first heart keeps the citadel's record too
				if (citadels.try_emplace(heart->record.citadel.number, made).second)
				{
					keep(heart->record.citadel.number, made, gamesave::CitadelRecord::k_Type);
				}
				giveLink(made, heart->record.base.multi.footpathLink);
			}
		}
	}
	// (the citadels' hearts are made in the loop above, each of the citadel its first heart keeps)
	// each town's storage pit, as its record names it
	for (const auto& town : plan.towns)
	{
		const auto made = towns.find(town.number);
		const auto pit = storagePits.find(town.record.mainStoragePit.number);
		if (made != towns.end() && pit != storagePits.end())
		{
			town_stores::SetStoragePit(made->second, pit->second);
		}
	}
	// each forest points at the big forest its own record names, once every big forest is made
	for (const auto& [number, record] : plan.forests)
	{
		if (const auto found = bigForests.find(record.bigForest.number); found != bigForests.end())
		{
			forests.All().at(keyOf(number)).bigForest = found->second;
		}
	}

	// the worship sites, once their citadels' hearts, towns and food pots are made
	uint32_t sitesMade = 0;
	std::vector<std::pair<const PlannedWorshipSite*, entt::entity>> madeSites;
	if (makers.worshipSite)
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		for (const auto& planned : plan.worshipSites)
		{
			const auto made = makers.worshipSite(planned, objectOf);
			if (made != entt::null)
			{
				++sitesMade;
				keep(planned.number, made, gamesave::WorshipSiteRecord::k_Type);
				madeSites.emplace_back(&planned, made);
				// its totem and its need signs, as the site made them
				const auto& component = Locator::entitiesRegistry::value().Get<const components::WorshipSite>(made);
				if (planned.totem.has_value())
				{
					keep(planned.totemNumber, component.totem, gamesave::WorshipTotemRecord::k_Type);
				}
				for (size_t i = 0; i < planned.signs.size(); ++i)
				{
					if (planned.signs[i].second.has_value())
					{
						keep(planned.signs[i].first, component.needsSigns.at(i), gamesave::ShowNeedsVisualsRecord::k_Type);
					}
				}
				// its spell icons, in its list's order
				for (size_t i = 0; i < planned.icons.size() && i < component.icons.size(); ++i)
				{
					keep(planned.icons[i].first, component.icons[i], gamesave::WorshipSpellIconRecord::k_Type);
				}
			}
		}
	}
	// the villagers, in the order their reads ended: each whose states load nothing draws its counter as its read ends,
	// made or not
	std::unordered_map<uint32_t, uint32_t> villagersMade;
	std::vector<std::pair<const PlannedVillager*, entt::entity>> madeVillagers;
	for (const auto& planned : plan.villagers)
	{
		const auto& values =
		    std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; }, planned.record);
		std::optional<uint16_t> counter;
		if (VillagerCounterDrawnOnLoad(values.living.action[0], values.living.action[1]))
		{
			counter = static_cast<uint16_t>(game_random::GameRand(k_LoadedCounterRange) + 1);
		}
		const auto made = makers.villager ? makers.villager(planned, counter) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++villagersMade[typeOf(planned.record)];
			keep(planned.number, made, typeOf(planned.record));
			madeVillagers.emplace_back(&planned, made);
		}
	}
	if (!madeVillagers.empty())
	{
		const auto entityOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		auto& registry = Locator::entitiesRegistry::value();
		// each villager's home, town and mother, as its record names them
		for (const auto& [planned, entity] : madeVillagers)
		{
			const auto& fields = std::visit(
			    [](const auto& record) -> const gamesave::VillagerFields& { return record.values.villager; }, planned->record);
			auto& villager = registry.Get<components::Villager>(entity);
			villager.abode = entityOf(fields.home.number);
			villager.town = entityOf(fields.town.number);
			villager.mother = entityOf(fields.mother.number);
			// what it works on, when one of its states saves it
			const auto& values = std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; },
			                                planned->record);
			for (const auto& [state, saved] : {std::pair {values.living.action[0], &values.topState},
			                                   std::pair {values.living.action[1], &values.finalState}})
			{
				if (TargetsAThing(gamesave::VillagerStateSaveOf(state)) && !saved->empty())
				{
					villager.targetThing = entityOf(saved->front().number);
				}
			}
		}
		// each abode's villagers and counts, as its own record lists them
		const auto giveVillagers = [&](gamesave::ObjectNumber number, const gamesave::AbodeFields& fields) {
			const auto abode = entityOf(number);
			auto* component = abode != entt::null ? registry.TryGet<components::Abode>(abode) : nullptr;
			if (component == nullptr)
			{
				return;
			}
			component->inhabitants.clear();
			for (const auto villager : fields.villagers.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					component->inhabitants.push_back(entity);
				}
			}
			component->adultCount = fields.adults;
			component->presentAtHome = fields.presentAtHome;
			component->childCount = fields.children;
		};
		for (const auto& object : plan.objects)
		{
			if (const auto* abode = std::get_if<PlannedAbode>(&object); abode != nullptr)
			{
				giveVillagers(abode->number, abode->abode);
			}
			else if (const auto* pit = std::get_if<PlannedStoragePit>(&object); pit != nullptr)
			{
				giveVillagers(pit->number, pit->record.abode);
			}
		}
		// each worship site's villagers, and those requesting to go home, sorted as its load sorts them
		for (const auto& [planned, site] : madeSites)
		{
			auto& component = registry.Get<components::WorshipSite>(site);
			component.villagers.clear();
			for (const auto villager : planned->record.worshipping.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					component.villagers.push_back(entity);
				}
			}
			component.goHomeRequests.clear();
			for (const auto villager : planned->record.villagersGoingHome.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					openblack::worship::site::InsertGoHomeRequest(component, entity);
				}
			}
		}
		// each town's homeless villagers, as its record lists them
		for (const auto& town : plan.towns)
		{
			const auto made = towns.find(town.number);
			if (made == towns.end())
			{
				continue;
			}
			auto* component = registry.TryGet<components::Town>(made->second);
			if (component == nullptr)
			{
				continue;
			}
			auto& homeless = component->homelessVillagers;
			homeless.clear();
			for (const auto villager : town.record.homeless.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					homeless.push_back(entity);
				}
			}
		}
	}

	// the dances, from the list's last to its first, once their dancers and what they are danced about are made; each
	// group, key frame and action by its dance and its number there
	uint32_t dancesMade = 0;
	if (makers.dance)
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		for (const auto& planned : plan.dances | std::views::reverse)
		{
			const auto made = makers.dance(planned, objectOf);
			if (made == entt::null)
			{
				continue;
			}
			++dancesMade;
			keep(planned.number, made, gamesave::DanceRecord::k_Type);
			for (size_t group = 0; group < planned.groups.size(); ++group)
			{
				keepSource(planned.groups[group].first, gamesave::DanceGroupRecord::k_Type,
				           saved_world_write::PartId(made, static_cast<uint32_t>(group)));
			}
			const auto& frames = planned.record.group.keyFrames.numbers;
			for (size_t frame = 0; frame < frames.size(); ++frame)
			{
				keepSource(frames[frame], gamesave::DanceKeyFrameRecord::k_Type,
				           saved_world_write::PartId(made, static_cast<uint32_t>(frame)));
				const auto* record = plan.keyFrames.contains(frames[frame]) ? &plan.keyFrames.at(frames[frame]) : nullptr;
				for (size_t action = 0; record != nullptr && action < record->actions.numbers.size(); ++action)
				{
					keepSource(record->actions.numbers[action], gamesave::DanceKeyActionRecord::k_Type,
					           saved_world_write::PartId(made, static_cast<uint32_t>((frame << k_KeyFrameShift) | action)));
				}
			}
		}
	}

	// the fires, once every object they can burn is made
	uint32_t firesMade = 0;
	if (makers.fires && !plan.fires.empty())
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		const auto ids = makers.fires(plan.fires, plan.fireTags, objectOf);
		for (size_t i = 0; i < ids.size() && i < plan.fires.size(); ++i)
		{
			if (ids[i] != 0)
			{
				++firesMade;
				keepSource(plan.fires[i].number, gamesave::FireEffectRecord::k_Type, ids[i]);
			}
		}
	}

	// the reactions, from the list's last to its first, so that the newest is the newest again, each with what its
	// initiator and its dance are now
	uint32_t reactionsMade = 0;
	for (const auto& reaction : plan.reactions | std::views::reverse)
	{
		const auto entityOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		const auto id = makers.reaction ? makers.reaction(reaction, entityOf(reaction.record.object.number),
		                                                  entityOf(reaction.record.second.number))
		                                : 0U;
		if (id != 0)
		{
			++reactionsMade;
			keepSource(reaction.number, gamesave::ReactionRecord::k_Type, id);
		}
	}

	Census census;
	for (const auto& [type, saved] : plan.census)
	{
		uint32_t made = 0;
		if (type == gamesave::TreeRecord::k_Type)
		{
			made = treesMade;
		}
		else if (type == gamesave::ForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(forestKeys.size());
		}
		else if (type == gamesave::BigForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(bigForests.size());
		}
		else if (type == gamesave::TownRecord::k_Type)
		{
			made = static_cast<uint32_t>(towns.size());
		}
		else if (type == gamesave::StoragePitRecord::k_Type)
		{
			made = static_cast<uint32_t>(storagePits.size());
		}
		else if (type == gamesave::ClimateRecord::k_Type)
		{
			made = climatesMade;
		}
		else if (type == gamesave::StreamRecord::k_Type)
		{
			made = streamsMade;
		}
		else if (type == gamesave::WeatherThingRecord::k_Type)
		{
			made = weatherThingsMade;
		}
		else if (type == gamesave::ArenaRecord::k_Type)
		{
			made = arenasMade;
		}
		else if (type == gamesave::ReactionRecord::k_Type)
		{
			made = reactionsMade;
		}
		else if (type == gamesave::MistRecord::k_Type)
		{
			made = mistsMade;
		}
		else if (type == gamesave::TownDesireFlagsRecord::k_Type)
		{
			made = desireFlagsMade;
		}
		else if (abodesMade.contains(type))
		{
			made = abodesMade.at(type);
		}
		else if (mobileStaticsMade.contains(type))
		{
			made = mobileStaticsMade.at(type);
		}
		else if (type == gamesave::FireEffectRecord::k_Type)
		{
			made = firesMade;
		}
		else if (villagersMade.contains(type))
		{
			made = villagersMade.at(type);
		}
		else if (type == gamesave::DanceRecord::k_Type)
		{
			made = dancesMade;
		}
		else if (type == gamesave::WorshipSiteRecord::k_Type)
		{
			made = sitesMade;
		}
		else if (type == gamesave::CitadelHeartRecord::k_Type)
		{
			made = static_cast<uint32_t>(hearts.size());
		}
		else if (type == gamesave::CitadelRecord::k_Type)
		{
			made = static_cast<uint32_t>(citadels.size());
		}
		else if (type == gamesave::FootpathRecord::k_Type)
		{
			made = static_cast<uint32_t>(footpathsMade.size());
		}
		else if (type == gamesave::FootpathLinkRecord::k_Type)
		{
			made = static_cast<uint32_t>(linksMade.size());
		}
		else if (type == gamesave::WoodPileRecord::k_Type || type == gamesave::FoodPileRecord::k_Type)
		{
			made = static_cast<uint32_t>(std::ranges::count_if(plan.objects, [&piles, type](const PlannedObject& object) {
				const auto* pile = std::get_if<PlannedPile>(&object);
				const auto pileType = pile == nullptr ? 0U
				                      : std::holds_alternative<gamesave::WoodPileRecord>(pile->record)
				                          ? gamesave::WoodPileRecord::k_Type
				                          : gamesave::FoodPileRecord::k_Type;
				return pile != nullptr && pileType == type && piles.contains(pile->number);
			}));
		}
		census.push_back({.type = type, .saved = saved, .made = made});
	}
	return census;
}

entt::entity saved_world_load::MakeTreeEntity(const PlannedTree& tree, uint32_t forestId)
{
	const auto& record = tree.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::TreeArchetype::Create(
	    forestId, position, tree.type, false, object.heading, record.secondValue, object.scale,
	    {.growing = false, .counter = static_cast<uint16_t>(record.firstValue)});
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeBigForestEntity(const PlannedBigForest& bigForest, uint32_t forestId)
{
	const auto& record = bigForest.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	// the word it keeps after its forest is its wood, a float
	const auto entity = archetypes::BigForestArchetype::Create(position, bigForest.type, object.heading, object.scale, forestId,
	                                                           std::bit_cast<float>(record.word));
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeTownEntity(const PlannedTown& town)
{
	const auto& record = town.record;
	auto& registry = Locator::entitiesRegistry::value();
	// openblack keeps one town per key: a saved id another town has takes the next free key, and keeps the id as the
	// number the scripts find it by
	bool taken = false;
	uint32_t next = 0;
	registry.Each<const components::Town>([&](entt::entity, const components::Town& other) {
		taken = taken || other.id == record.id;
		next = std::max(next, other.id + 1);
	});
	const auto key = taken ? next : record.id;
	const auto owner = record.playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT)
	                       ? static_cast<PlayerNames>(record.playerNumber)
	                       : PlayerNames::NEUTRAL;
	const auto tribe = record.tribe < static_cast<uint32_t>(Tribe::_COUNT) ? static_cast<Tribe>(record.tribe) : Tribe::NONE;
	const auto position = map_coords::ToWorld(CoordsOf(record.container.position.position));
	const auto entity = archetypes::TownArchetype::CreateWithoutDesireFlags(static_cast<int>(key), position, owner, tribe);
	auto& component = registry.Get<components::Town>(entity);
	if (taken)
	{
		component.scriptId = record.id;
	}
	component.belief.beliefInNeutralPlayer = record.beliefInNeutralPlayer;
	component.uninhabitable = record.uninhabitable != 0;
	component.areaMin = {record.areaMin.x, record.areaMin.z};
	component.areaMax = {record.areaMax.x, record.areaMax.z};
	registry.Assign<components::LoadedRecord<gamesave::TownRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakePileEntity(const PlannedPile& pile)
{
	const auto& object = pile.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(pile.base.position.position));
	const auto entity = archetypes::PotArchetype::Create(position, object.heading, pile.type,
	                                                     static_cast<int32_t>(pile.pile.resourceAmount), true);
	if (entity == entt::null)
	{
		return entity;
	}
	// at once at the height of its amount: the rise the save keeps is in its record
	archetypes::PotArchetype::SetSize(entity, false);
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, object.life);
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    pile.record);
	return entity;
}

entt::entity saved_world_load::MakeStoragePitEntity(const PlannedStoragePit& pit, uint32_t townKey,
                                                    const archetypes::AbodeArchetype::StoragePitPiles& piles)
{
	const auto& record = pit.record;
	const auto& object = record.base.object;
	const auto& amounts = record.abode.resources.values;
	const auto food = amounts.empty() ? 0U : amounts.at(0);
	const auto wood = amounts.size() < 2 ? 0U : amounts.at(1);
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::AbodeArchetype::Create(townKey, position, pit.type, object.heading, object.scale, food,
	                                                       wood, false, false, &piles);
	if (entity != entt::null)
	{
		KeepSaved(entity, record);
	}
	return entity;
}

entt::entity saved_world_load::MakeFootpathEntity(const PlannedFootpath& footpath)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = footpaths::Create();
	auto& made = registry.Get<components::Footpath>(entity);
	made.nodes.reserve(footpath.nodes.size());
	for (const auto& node : footpath.nodes)
	{
		// its place on the land at its map position, raised by its height above it, as the footpath file's nodes
		const auto coords = CoordsOf(node.coords);
		made.nodes.push_back({map_coords::ToWorld(coords), coords, node.flags, made.nextNodeId++});
	}
	made.active = footpath.record.active != 0;
	registry.Assign<components::LoadedRecord<gamesave::FootpathRecord>>(entity, footpath.record);
	return entity;
}

entt::entity saved_world_load::MakeFootpathLinkEntity(const PlannedFootpathLink& link,
                                                      const std::vector<entt::entity>& footpaths)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	std::vector<components::Footpath::Id> ids;
	ids.reserve(footpaths.size());
	for (const auto footpath : footpaths)
	{
		ids.push_back(static_cast<components::Footpath::Id>(footpath));
	}
	registry.Assign<components::FootpathLink>(entity, glm::vec3(0.0f), std::move(ids));
	registry.Assign<components::LoadedRecord<gamesave::FootpathLinkRecord>>(entity, link.record);
	return entity;
}

entt::entity saved_world_load::MakeCitadelHeartEntity(const PlannedCitadelHeart& heart, entt::entity citadel)
{
	const auto& record = heart.record;
	const auto& object = record.base.object;
	const auto& multi = record.base.multi;
	const bool underConstruction = (multi.fixedFlags & components::CitadelPartBuild::k_UnderConstruction) != 0;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::CitadelArchetype::CreateHeart(position, heart.owner, citadel, object.heading, object.scale,
	                                                              object.life, underConstruction, false);
	auto& registry = Locator::entitiesRegistry::value();
	auto& build = registry.Get<components::CitadelPartBuild>(entity);
	build.buildFlags = multi.fixedFlags;
	build.percentBuilt = multi.percentBuilt;
	worship::citadel::SetHeartDrawPercent(entity, build.percentBuilt);
	life::SetLife(entity, object.life);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	registry.Assign<components::LoadedRecord<gamesave::CitadelHeartRecord>>(entity, record);
	if (citadel == entt::null)
	{
		registry.Assign<components::LoadedRecord<gamesave::CitadelRecord>>(entity, heart.citadel);
	}
	return entity;
}

entt::entity saved_world_load::MakeAbodeEntity(const PlannedAbode& abode, uint32_t townKey)
{
	const auto& object = abode.base.object;
	const auto& multi = abode.base.multi;
	const auto& amounts = abode.abode.resources.values;
	const auto food = amounts.empty() ? 0U : amounts.at(0);
	const auto wood = amounts.size() < 2 ? 0U : amounts.at(1);
	const bool underConstruction = (multi.fixedFlags & components::Abode::k_UnderConstruction) != 0;
	const auto position = map_coords::ToWorld(CoordsOf(abode.base.position.position));
	const auto entity = archetypes::AbodeArchetype::Create(townKey, position, abode.type, object.heading, object.scale, food,
	                                                       wood, underConstruction, false);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::Abode>(entity);
	made.buildFlags = multi.fixedFlags;
	made.percentBuilt = multi.percentBuilt;
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, object.life);
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    if constexpr (std::is_same_v<Record, gamesave::GraveyardRecord>)
		    {
			    if (auto* graveyard = registry.TryGet<components::Graveyard>(entity); graveyard != nullptr)
			    {
				    graveyard->dead = record.graves;
				    graveyard->gravesStage = 0;
			    }
		    }
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    abode.record);
	return entity;
}

entt::entity saved_world_load::MakeWorshipSiteEntity(const PlannedWorshipSite& planned,
                                                     const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto citadel = objectOf(record.part.citadel.number);
	auto* worship = citadel != entt::null ? registry.TryGet<components::CitadelWorship>(citadel) : nullptr;
	const auto tribeIndex = static_cast<int64_t>(k_FirstTribeRow) - static_cast<int64_t>(record.tribeInfo);
	if (worship == nullptr || record.slot >= components::CitadelWorship::k_Sites ||
	    worship->sites.at(record.slot) != entt::null || tribeIndex < 0 || tribeIndex >= static_cast<int64_t>(Tribe::_COUNT))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: worship site {} not made: no citadel, a taken slot or no tribe",
		                   planned.number);
		return entt::null;
	}
	// the citadel's own record names its six sites, by slot, as the original's load sets them: it should name this one
	if (const auto* kept = registry.TryGet<const components::LoadedRecord<gamesave::CitadelRecord>>(citadel);
	    kept != nullptr &&
	    (record.slot >= kept->record.parts.numbers.size() || kept->record.parts.numbers.at(record.slot) != planned.number))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: worship site {} is not its citadel's site in slot {}", planned.number,
		                   record.slot);
	}
	const auto& base = record.part.base;
	const auto entity =
	    openblack::worship::site::CreateInSlot(citadel, static_cast<Tribe>(tribeIndex), record.slot, base.object.heading);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, base.object.creationIndex);
	RestorePlaceMarks(entity, base.position.flags);
	if (base.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, base.object.life);
	}
	registry.AssignOrReplace<components::CitadelPartBuild>(
	    entity, components::CitadelPartBuild {.buildFlags = base.multi.fixedFlags, .percentBuilt = base.multi.percentBuilt});
	auto& site = registry.Get<components::WorshipSite>(entity);
	for (const auto number : record.towns.numbers)
	{
		const auto town = objectOf(number);
		if (town == entt::null)
		{
			continue;
		}
		site.towns.push_back(town);
		if (auto* magic = registry.TryGet<components::TownMagic>(town))
		{
			magic->worshipSite = entity;
		}
	}
	site.foodPot = objectOf(record.foodPot.number);
	site.villagersAtSite = static_cast<int32_t>(record.worshippers);
	site.foodOnTheWay =
	    record.resourcesOnTheWay.values.empty() ? 0 : static_cast<int32_t>(record.resourcesOnTheWay.values.front());
	site.battery = record.chants[0];
	site.sacrificed = record.chants[1];
	site.available = record.chants[2];
	site.used = record.chants[3];
	site.requested = record.chants[4];
	site.chantDamage = record.chants[5];
	site.infiniteChants = record.infiniteChants != 0;
	site.freeMaintenance = record.freeMaintenance != 0;
	site.iconTookChants = record.isSupplyingChants != 0;
	site.strain = record.overloaded;
	site.strainPhase = record.overloadPulsePhase;
	site.strainPulse = record.overloadPulse;
	// its totem and need signs, made as the game makes them, then given their saved creation numbers, a sign its saved
	// desire, and their records
	site.totem = openblack::worship::site::CreateTotemOf(entity);
	if (site.totem != entt::null && planned.totem.has_value())
	{
		registry.AssignOrReplace<components::ObjectCreationIndex>(site.totem, planned.totem->part.base.object.creationIndex);
		RestorePlaceMarks(site.totem, planned.totem->part.base.position.flags);
		registry.AssignOrReplace<components::LoadedRecord<gamesave::WorshipTotemRecord>>(site.totem, *planned.totem);
		// the altar's own build state, as its site's above
		const auto& multi = planned.totem->part.base.multi;
		registry.AssignOrReplace<components::CitadelPartBuild>(
		    site.totem, components::CitadelPartBuild {.buildFlags = multi.fixedFlags, .percentBuilt = multi.percentBuilt});
		abodes::RedrawConstruction(site.totem);
	}
	for (size_t i = 0; i < planned.signs.size(); ++i)
	{
		const auto sign = site.needsSigns.at(i);
		const auto& saved = planned.signs[i].second;
		if (sign == entt::null || !saved.has_value())
		{
			continue;
		}
		registry.AssignOrReplace<components::ObjectCreationIndex>(sign, saved->object.creationIndex);
		if (auto* visuals = registry.TryGet<components::ShowNeedsVisuals>(sign))
		{
			visuals->desire = std::bit_cast<float>(saved->lastWord);
		}
		registry.AssignOrReplace<components::LoadedRecord<gamesave::ShowNeedsVisualsRecord>>(sign, *saved);
	}
	// its spell icons, each made at its saved place as the game makes one, at the head of the site's list, so that the
	// last of the saved list made first leaves them in its order; then their saved values
	for (const auto& [number, saved] : planned.icons | std::views::reverse)
	{
		const auto seedRow = static_cast<int64_t>(k_FirstSpellSeedRow) - static_cast<int64_t>(saved.icon.info);
		const auto icon = openblack::worship::icon::Create(map_coords::ToWorld(CoordsOf(saved.icon.base.position.position)),
		                                                   static_cast<SpellSeedType>(seedRow), entity,
		                                                   static_cast<int16_t>(saved.secondHalf));
		if (icon == entt::null)
		{
			continue;
		}
		registry.AssignOrReplace<components::ObjectCreationIndex>(icon, saved.icon.base.object.creationIndex);
		RestorePlaceMarks(icon, saved.icon.base.position.flags);
		auto& values = registry.Get<components::WorshipSpellIcon>(icon);
		values.removeTimer = static_cast<int16_t>(saved.half);
		values.savedScale = std::bit_cast<float>(saved.words[0]);
		values.chargeStartTurn = saved.words[1];
		values.charging = saved.byte != 0;
		values.powerUp = static_cast<int>(saved.thirdWord);
		values.chantStore = std::bit_cast<float>(saved.fourthWord);
		openblack::worship::icon::UpdatePowerUpGraphics(icon);
		if (saved.other.number != 0 || !saved.objects.numbers.empty())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: spell icon {}: its charging hand and its seeds are not put back yet",
			                   number);
		}
		registry.Assign<components::LoadedRecord<gamesave::WorshipSpellIconRecord>>(icon, saved);
	}
	registry.Assign<components::LoadedRecord<gamesave::WorshipSiteRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeDanceEntity(const PlannedDance& planned,
                                               const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	// its dancers first: one not made leaves the dance unmade
	std::vector<std::vector<entt::entity>> dancers;
	for (const auto& [number, group] : planned.groups)
	{
		auto& members = dancers.emplace_back();
		for (const auto member : group.members.numbers)
		{
			const auto entity = objectOf(member);
			if (entity == entt::null)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: dance {} not made: its dancer {} is not", planned.number,
				                   member);
				return entt::null;
			}
			members.push_back(entity);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	// it comes after every dance there is
	uint32_t made = 0;
	registry.Each<const components::Dance>(
	    [&made](entt::entity, const components::Dance& other) { made = std::max(made, other.made + 1); });
	const auto entity = registry.Create();
	auto& dance = registry.Assign<components::Dance>(entity);
	dance.made = made;
	dance.type = static_cast<DanceInfo>(record.type);
	const auto& group = record.group;
	dance.place = {
	    .x = group.position.position.x, .z = group.position.position.z, .altitude = group.position.position.altitude};
	dance.clock = group.time;
	dance.loopLength = group.length;
	dance.dancers = group.dancers;
	dance.groups.round = group.proportionCounter;
	dance.groups.roundLength = group.denominator;
	dance.rate = group.speed;
	dance.dancingRate = group.lastSpeed;
	dance.madeByScript = group.flags[1] != 0;
	dance.speed = record.worshipSpeed;
	dance.owner = objectOf(record.target.number);
	dance.state = record.words[0] == 0 ? components::Dance::State::Dancing : components::Dance::State::Stopped;
	dance.duration = record.words[1];
	dance.startTurn = record.words[2];
	if (record.words[3] != 0)
	{
		dance.firstDancerTurn = record.words[4];
	}
	dance.onTheirWay = record.words[5];
	if (Locator::infoConstants::has_value() && record.type < Locator::infoConstants::value().dance.size())
	{
		dance.autostart = Locator::infoConstants::value().dance.at(record.type).startsAutomatically != 0;
	}
	if (Locator::danceSystem::has_value())
	{
		dance.file = Locator::danceSystem::value().FileOf(dance.type);
	}
	// its groups, as their records say, in its order
	const auto indexOf = [&planned](gamesave::ObjectNumber number) {
		const auto found = std::ranges::find(planned.groups, number, [](const auto& entry) { return entry.first; });
		return static_cast<size_t>(found - planned.groups.begin());
	};
	for (size_t index = 0; index < planned.groups.size(); ++index)
	{
		const auto& saved = planned.groups[index].second;
		auto& part = dance.groups.all.emplace_back();
		part.name = std::string(saved.text.data(), strnlen(saved.text.data(), saved.text.size()));
		part.dancers = dancers[index];
		part.flag = saved.words[1] != 0;
		part.formation = saved.words[2];
		part.shape = saved.words[3];
		std::memcpy(&part.offset, saved.eightBytes.data(), saved.eightBytes.size());
		part.radius = std::bit_cast<float>(saved.secondWords[0]);
		part.rotation = std::bit_cast<float>(saved.secondWords[2]);
		std::array<uint32_t, 4> move {};
		std::memcpy(move.data(), saved.sixteenBytes.data(), saved.sixteenBytes.size());
		part.move = {.action = move[0], .first = move[1], .second = move[2], .end = move[3]};
		part.spin = saved.value;
		if (saved.other.number != 0)
		{
			part.parent = indexOf(saved.other.number);
		}
		part.indexInParent = saved.byte;
		part.limitedDancers = saved.thirdWord;
		part.moved = saved.firstInt != 0;
		part.danceType = saved.danceType;
		part.inFormation = saved.fourthWord;
		std::array<uint32_t, 3> proportion {};
		std::memcpy(proportion.data(), saved.proportion.data(), saved.proportion.size());
		part.limited = proportion[0] != 0;
		part.quota = proportion[1];
		part.weight = proportion[2];
		part.children.assign(saved.stack.values.begin(), saved.stack.values.end());
		part.sexes = saved.villagerType;
		// each dancer in its place in the dance
		for (const auto dancer : part.dancers)
		{
			registry.AssignOrReplace<components::Dancer>(dancer, components::Dancer {.dance = entity, .group = index});
		}
	}
	for (const auto number : group.fixedGroups.numbers)
	{
		dance.groups.limited.push_back(indexOf(number));
	}
	for (const auto number : group.proportionGroups.numbers)
	{
		dance.groups.shared.push_back(indexOf(number));
	}
	registry.Assign<components::LoadedRecord<gamesave::DanceRecord>>(entity, record);
	// a worship site's dance is the site's own again, facing the way the site does
	if (auto* site = dance.owner != entt::null ? registry.TryGet<components::WorshipSite>(dance.owner) : nullptr;
	    site != nullptr)
	{
		site->dance = entity;
		registry.Get<components::Dance>(entity).angle = site->yAngle;
	}
	return entity;
}

std::vector<uint32_t> saved_world_load::MakeFires(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
                                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	std::vector<uint32_t> ids(fires.size(), 0);
	std::unordered_map<gamesave::ObjectNumber, fire::FireEffect*> made;
	// from the list's last to its first, each at the head of the fires' list
	for (size_t i = fires.size(); i-- > 0;)
	{
		const auto& record = fires[i].record;
		auto* restored = fire::Restore({.object = objectOf(record.objects[0].number),
		                                .source = objectOf(record.objects[1].number),
		                                .player = fires[i].player,
		                                .temperature = std::bit_cast<float>(record.words[0]),
		                                .previous = std::bit_cast<float>(record.words[1]),
		                                .charring = std::bit_cast<float>(record.word),
		                                .tag = record.byte,
		                                .flags = record.secondByte});
		if (restored != nullptr)
		{
			ids[i] = restored->id;
			made.emplace(fires[i].number, restored);
		}
	}
	// each in its group, with its root's firemen
	for (const auto& planned : fires)
	{
		const auto found = made.find(planned.number);
		if (found == made.end())
		{
			continue;
		}
		const auto& record = planned.record;
		const auto next = made.find(record.others[0].number);
		const auto root = made.find(record.others[1].number);
		found->second->next = next != made.end() ? next->second : nullptr;
		found->second->root = root != made.end() ? root->second : found->second;
		for (const auto fireman : record.burning.numbers)
		{
			if (const auto entity = objectOf(fireman); entity != entt::null)
			{
				found->second->firemen.push_back(entity);
			}
		}
	}
	auto& system = Locator::fireSystem::value();
	system.SetProcessTag(tags[0]);
	system.SetCreateTag(tags[1]);
	return ids;
}

entt::entity saved_world_load::MakeVillagerEntity(const PlannedVillager& planned, std::optional<uint16_t> counter)
{
	const auto& base =
	    std::visit([](const auto& record) { return std::tie(record.thing, record.position, record.object, record.values); },
	               planned.record);
	const auto& [thing, position, object, values] = base;
	const auto& move = values.move;
	const auto& living = values.living;
	const auto& fields = values.villager;
	// what the load does not make yet
	const char* why = nullptr;
	if (planned.type == VillagerInfo::None)
	{
		why = "no villager row";
	}
	else if ((thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) != 0 ||
	         (position.flags & (k_PositionUnavailable | k_PositionInPhysics)) != 0)
	{
		why = "held or in flight";
	}
	else if (living.footpath.number != 0 || move.footpath.number != 0 || living.dataPath.number != 0 ||
	         living.flock.number != 0 || living.reaction.number != 0 || living.otherObject.number != 0 ||
	         !living.reactionsDone.empty())
	{
		why = "on a footpath or a data path, in a flock or a reaction";
	}
	else if (move.moveState != 0 || move.circle.has_value() || move.target.number != 0)
	{
		why = "walking";
	}
	else if (!StateValuesLoaded(living.action[0], values.topState, fields.discipleType) ||
	         !StateValuesLoaded(living.action[1], values.finalState, fields.discipleType))
	{
		why = "in a state that loads values";
	}
	if (why != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: villager {} not made: {}", planned.number, why);
		return entt::null;
	}
	const bool child = (fields.flags & components::Villager::k_FlagChild) != 0;
	const auto entity = archetypes::VillagerArchetype::Restore(map_coords::ToWorld(CoordsOf(position.position)), planned.type,
	                                                           object.heading, object.scale, child);
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	RestorePlaceMarks(entity, position.flags);
	auto& villager = registry.Get<components::Villager>(entity);
	villager.life = object.life;
	villager.flags = static_cast<uint16_t>(fields.flags & 0xC6FAU);
	villager.woken = (fields.flags & 0x1U) != 0;
	villager.food = fields.food;
	villager.lastCheckTurn = static_cast<uint32_t>(fields.lastCheckTurn);
	villager.foodSpeedUp = fields.foodSpeedUp;
	villager.discipleType = fields.discipleType;
	villager.resourceHeld = {fields.foodHeld, fields.woodHeld};
	villager.pregnancy = fields.pregnant;
	villager.birthTurn = living.birthTurn;
	villager.status = static_cast<uint16_t>(living.status & 0x0471U);
	villager.flockOrder = living.secondByte;
	villager.crecheWalk = std::bit_cast<int32_t>(living.secondWord);
	// a dead villager's cause, its state's one byte
	for (const auto& [state, saved] :
	     {std::pair {living.action[0], &values.topState}, std::pair {living.action[1], &values.finalState}})
	{
		if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Dead && !saved->empty())
		{
			villager.deathReason = static_cast<DeathReason>(saved->front().bytes[0]);
		}
	}
	if ((living.status & 0x0002U) != 0)
	{
		registry.Assign<components::Poisoned>(entity);
	}
	if ((living.status & 0x0080U) != 0)
	{
		registry.Assign<components::DownedVillager>(entity);
	}
	if ((fields.flags & 0x4U) != 0)
	{
		registry.Assign<components::AtHome>(entity).beenToBed = (fields.flags & 0x2000U) != 0;
	}
	if (living.specular != std::array<uint8_t, 4> {})
	{
		registry.Assign<components::SpecularColour>(
		    entity, components::SpecularColour {.colour = {living.specular[2], living.specular[1], living.specular[0]}});
	}
	auto& action = registry.Get<components::LivingAction>(entity);
	action.states = {living.action[0], living.action[1], living.action[2]};
	action.turnsSinceStateChange = static_cast<uint16_t>(living.action[4] | (living.action[5] << 8U));
	action.turnsUntilStateChange = counter.value_or(std::bit_cast<uint16_t>(move.turnsToStateChange));
	auto& wallHug = registry.Get<components::WallHug>(entity);
	wallHug.speed = map_coords::ToMetres(static_cast<int32_t>(move.speed));
	wallHug.gameAngle = move.angle;
	walk_goal::Set(entity, wallHug.goal, {map_coords::ToMetres(move.goal.x), map_coords::ToMetres(move.goal.z)});
	walk_goal::SetAltitude(entity, move.goal.altitude);
	wallHug.step = {move.step.x, move.step.z};
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    if constexpr (std::is_same_v<Record, gamesave::SpecialVillagerRecord>)
		    {
			    registry.Assign<components::NamedVillager>(entity).row = static_cast<uint8_t>(record.infoIndex);
		    }
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    planned.record);
	archetypes::VillagerArchetype::PlaceRestored(entity);
	// the clips it plays as it was saved, and what it carries
	if (auto* animation = registry.TryGet<components::SkeletalAnimation>(entity))
	{
		animation->transitionFlags = static_cast<uint16_t>(fields.flags & 0x1800U);
		animation->carriedObject = fields.carriedObjectType;
	}
	return entity;
}

entt::entity saved_world_load::MakeMobileStaticEntity(const PlannedMobileStatic& object)
{
	const auto& base = object.base;
	const auto& coords = base.position.position;
	// on the land under its place, raised by its altitude, at its X, Y and Z angles
	const auto ground = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	const auto entity = archetypes::MobileStaticArchetype::Create(
	    ground, object.type, coords.altitude, std::bit_cast<float>(object.mobileStatic.words[0]), base.object.heading,
	    std::bit_cast<float>(object.mobileStatic.words[1]), base.object.scale);
	auto& registry = Locator::entitiesRegistry::value();
	if (object.player.has_value())
	{
		registry.Get<components::MobileStatic>(entity).owner = *object.player;
	}
	if (base.multi.percentBuilt != 0.0f)
	{
		registry.AssignOrReplace<components::StaticBuiltPercentage>(entity, base.multi.percentBuilt);
	}
	std::visit([entity](const auto& record) { KeepSaved(entity, record); }, object.record);
	return entity;
}

entt::entity saved_world_load::MakeDesireFlagEntity(const PlannedDesireFlag& flag, entt::entity town)
{
	const auto& record = flag.record;
	const auto& object = record.object;
	if (record.desireType >= components::k_TownDesireCount)
	{
		return entt::null;
	}
	const auto point = town_desire_flags::PointOf(record.slot);
	const auto position = map_coords::ToWorld(CoordsOf(record.position.position));
	const auto entity =
	    archetypes::TownDesireFlagArchetype::Create(town, static_cast<TownDesireInfo>(record.desireType), point.value_or(0),
	                                                static_cast<MeshId>(record.mesh), object.scale, position);
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::TownDesireFlag>(entity);
	// where it was made: the object's own coordinates, which the game sets once
	made.madeAt = map_coords::ToWorld(CoordsOf(object.coords));
	made.overGround = record.position.position.altitude;
	made.storedDesire = record.desire;
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	registry.Assign<components::LoadedRecord<gamesave::TownDesireFlagsRecord>>(entity, record);
	// its town keeps it by its desire
	if (registry.Valid(town))
	{
		if (!registry.AllOf<components::TownDesireFlags>(town))
		{
			components::TownDesireFlags none;
			none.flags.fill(entt::null);
			registry.AssignState<components::TownDesireFlags>(town, none);
		}
		registry.Get<components::TownDesireFlags>(town).flags.at(record.desireType) = entity;
	}
	return entity;
}

entt::entity saved_world_load::MakeStreamEntity(const PlannedStream& stream)
{
	const auto& record = stream.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// its id, then its points: each node keeps its place as a point (x, the ground's height, z) and a word of the game's
	// memory
	const auto id = static_cast<components::Stream::Id>(record.words[0]);
	auto& made = registry.Assign<components::Stream>(entity, id);
	made.points.reserve(record.nodes.values.size());
	for (const auto& node : record.nodes.values)
	{
		std::array<float, 3> point {};
		std::memcpy(point.data(), node.data(), sizeof(point));
		made.points.emplace_back(point[0], point[1], point[2]);
	}
	registry.Context().streams.insert_or_assign(id, entity);
	registry.Assign<components::LoadedRecord<gamesave::StreamRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeWeatherThingEntity(const PlannedWeatherThing& thing)
{
	const auto& record = thing.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// at its place on the map, as a thing made by a script stands
	const auto& coords = record.position.position;
	registry.Assign<components::Transform>(entity,
	                                       glm::vec3(map_coords::ToMetres(coords.x), 0.0f, map_coords::ToMetres(coords.z)),
	                                       glm::mat3(1.0f), glm::vec3(1.0f));
	auto& made = registry.Assign<components::WeatherThing>(entity);
	made.descriptor = StormDescriptorOf(record.descriptor);
	made.affectedByWind = record.affectedByWind != 0;
	// its storm: made from its saved descriptor (at the head of the storms' list), then its age, speed and target
	if (record.storm.storm.has_value())
	{
		const auto& saved = *record.storm.storm;
		made.storm = weather::storms::Create(StormDescriptorOf(saved.descriptor));
		if (auto* storm = weather::storms::Find(made.storm))
		{
			storm->age = std::bit_cast<float>(saved.age);
			storm->speed = std::bit_cast<float>(saved.speed);
			storm->target = glm::vec3(saved.target[0], saved.target[1], saved.target[2]);
		}
	}
	weather::weather_thing::AddLoaded(entity);
	registry.Assign<components::LoadedRecord<gamesave::WeatherThingRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeArenaEntity(const PlannedArena& arena)
{
	const auto& record = arena.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// its record's second word is its radius; it is the next in the arenas' order
	const auto& coords = record.position.position;
	const auto x = map_coords::ToMetres(coords.x);
	const auto z = map_coords::ToMetres(coords.z);
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	const auto number = static_cast<uint32_t>(registry.Size<components::Arena>()) + 1;
	registry.Assign<components::Arena>(entity, glm::vec3(x, ground, z), std::bit_cast<float>(record.words[1]),
	                                   map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = coords.altitude},
	                                   number);
	return entity;
}

entt::entity saved_world_load::MakeMistEntity(const PlannedMist& mist)
{
	const auto& record = mist.record;
	const auto& coords = record.position.position;
	// its three words: its size, its colour and its edge-on shrink
	const auto entity = archetypes::MistArchetype::Create(
	    glm::vec3(map_coords::ToMetres(coords.x), 0.0f, map_coords::ToMetres(coords.z)), coords.altitude, record.words[1],
	    std::bit_cast<float>(record.words[0]), std::bit_cast<float>(record.words[2]));
	Locator::entitiesRegistry::value().Assign<components::LoadedRecord<gamesave::MistRecord>>(entity, record);
	return entity;
}

std::optional<uint32_t> saved_world_load::ClimateTypeOfInfo(uint32_t info)
{
	if (info > k_FirstClimateRow || info + k_ClimateRows <= k_FirstClimateRow)
	{
		return std::nullopt;
	}
	return k_FirstClimateRow - info;
}

namespace
{
/// Each value of a storm's descriptor with its place in the 80 bytes the game writes
template <typename Descriptor, typename Function>
void EachDescriptorValue(Descriptor& descriptor, Function value)
{
	// the weather it adds: eight bytes
	static_assert(sizeof(openblack::weather::WeatherInfo) == 8);
	value(0x00, descriptor.position.x);
	value(0x04, descriptor.position.y);
	value(0x08, descriptor.position.z);
	value(0x0C, descriptor.innerRadius);
	value(0x10, descriptor.outerRadius);
	value(0x14, descriptor.fadeInTime);
	value(0x18, descriptor.lifeTime);
	value(0x1C, descriptor.strength);
	value(0x20, descriptor.numClouds);
	value(0x24, descriptor.blackness);
	value(0x28, descriptor.elevation);
	value(0x2C, descriptor.fallSpeed);
	value(0x30, descriptor.sheetMin);
	value(0x34, descriptor.sheetMax);
	value(0x38, descriptor.forkMin);
	value(0x3C, descriptor.forkMax);
	value(0x40, descriptor.snowCoverRate);
	value(0x44, descriptor.unknown0x44);
	value(0x48, descriptor.weather);
}
} // namespace

uint32_t saved_world_load::ClimateInfoRowOf(uint32_t type)
{
	return k_FirstClimateRow - type;
}

openblack::weather::storms::StormDescriptor saved_world_load::StormDescriptorOf(const std::array<uint8_t, 80>& bytes)
{
	weather::storms::StormDescriptor descriptor;
	EachDescriptorValue(descriptor,
	                    [&bytes]<typename T>(size_t at, T& into) { std::memcpy(&into, bytes.data() + at, sizeof(T)); });
	return descriptor;
}

std::array<uint8_t, 80> saved_world_load::StormDescriptorBytes(const weather::storms::StormDescriptor& descriptor)
{
	std::array<uint8_t, 80> bytes {};
	EachDescriptorValue(descriptor,
	                    [&bytes]<typename T>(size_t at, const T& from) { std::memcpy(bytes.data() + at, &from, sizeof(T)); });
	return bytes;
}

size_t saved_world_load::MakeClimates(const std::vector<PlannedClimate>& climates)
{
	if (climates.empty())
	{
		return 0;
	}
	// the land's climates go first, with their storms, before the saved ones' storms are made
	weather::climate::Reset();
	std::list<weather::climate::Climate> made;
	weather::climate::Counters counters;
	for (const auto& planned : climates)
	{
		const auto& record = planned.record;
		weather::climate::Climate climate;
		// its place: the map coordinates' two words and the altitude
		climate.x = std::bit_cast<int32_t>(record.point[0]);
		climate.z = std::bit_cast<int32_t>(record.point[1]);
		climate.y = record.point[2];
		climate.innerRadius = std::bit_cast<float>(record.words[0]);
		climate.outerRadius = std::bit_cast<float>(record.words[1]);
		climate.id = static_cast<int32_t>(record.words[2]);
		climate.info = static_cast<int32_t>(ClimateTypeOfInfo(record.words[4]).value_or(0));
		// the rain: its desire, its two day counts and its flags
		std::memcpy(&climate.rain.desire, record.sixteen.data(), sizeof(float));
		std::memcpy(&climate.rain.dryDays, record.sixteen.data() + 4, sizeof(int32_t));
		std::memcpy(&climate.rain.rainingDays, record.sixteen.data() + 8, sizeof(int32_t));
		climate.rain.flags = record.sixteen[12];
		std::memcpy(&climate.temperature, record.eight.data(), sizeof(float));
		std::memcpy(&climate.targetTemperature, record.eight.data() + 4, sizeof(float));
		climate.windX = record.secondPoint[0];
		climate.windZ = record.secondPoint[1];
		climate.windAngle = record.secondPoint[2];
		climate.maxStorms = static_cast<int32_t>(record.word);
		climate.world = record.byte != 0;
		climate.stormElevation = static_cast<int32_t>(record.lastWords[0]);
		climate.fallSpeed = std::bit_cast<float>(record.lastWords[1]);
		climate.unknown0x70 = std::bit_cast<float>(record.lastWords[2]);
		climate.sheetMin = std::bit_cast<float>(record.lastWords[3]);
		climate.sheetMax = std::bit_cast<float>(record.lastWords[4]);
		climate.forkMin = std::bit_cast<float>(record.lastWords[5]);
		climate.forkMax = std::bit_cast<float>(record.lastWords[6]);
		climate.lightning = record.lastByte;
		climate.saved = record;
		// its natural storms, in their saved order, each at the head of its list
		for (const auto& area : record.areas.elements)
		{
			const auto id = weather::storms::Create(StormDescriptorOf(area.values));
			if (auto* storm = weather::storms::Find(id))
			{
				storm->age = std::bit_cast<float>(area.words[0]);
				storm->speed = std::bit_cast<float>(area.words[1]);
				storm->target = glm::vec3(area.second[0], area.second[1], area.second[2]);
			}
			climate.storms.push_front(id);
		}
		made.push_back(std::move(climate));
		// every climate writes the counters of the whole system: the next id, the last day, the season and the last
		// season
		counters = {.nextId = static_cast<int32_t>(record.words[3]),
		            .lastDay = static_cast<int32_t>(record.words[5]),
		            .season = record.words[6],
		            .lastSeason = record.words[7]};
	}
	const auto count = made.size();
	weather::climate::Restore(std::move(made), counters);
	return count;
}

saved_world_load::Makers saved_world_load::GameMakers()
{
	return {.footpath = MakeFootpathEntity,
	        .footpathLink = MakeFootpathLinkEntity,
	        .town = MakeTownEntity,
	        .tree = MakeTreeEntity,
	        .bigForest = MakeBigForestEntity,
	        .pile = MakePileEntity,
	        .storagePit = MakeStoragePitEntity,
	        .citadelHeart = MakeCitadelHeartEntity,
	        .abode = MakeAbodeEntity,
	        .desireFlag = MakeDesireFlagEntity,
	        .mobileStatic = MakeMobileStaticEntity,
	        .stream = MakeStreamEntity,
	        .mist = MakeMistEntity,
	        .climates = MakeClimates,
	        .weatherThing = MakeWeatherThingEntity,
	        .worshipSite = MakeWorshipSiteEntity,
	        .villager = MakeVillagerEntity,
	        .dance = MakeDanceEntity,
	        .fires = MakeFires,
	        .arena = MakeArenaEntity,
	        .reaction = MakeReaction};
}

uint32_t saved_world_load::MakeReaction(const PlannedReaction& reaction, entt::entity initiator, entt::entity dance)
{
	if (!Locator::reactionSystem::has_value())
	{
		return 0;
	}
	const auto& record = reaction.record;
	return Locator::reactionSystem::value().Add({.initiator = initiator,
	                                             .type = static_cast<openblack::Reaction>(record.secondWord),
	                                             .player = reaction.player,
	                                             .turnCreated = record.thirdWord,
	                                             .radius = std::bit_cast<float>(record.lastWord),
	                                             .available = record.fourthWord != 0,
	                                             .stealth = record.firstWord != 0,
	                                             .dance = dance});
}
