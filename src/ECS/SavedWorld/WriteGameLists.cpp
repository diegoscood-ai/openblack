/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's global lists whose objects are entities of one kind, each kept newest first, as the game adds every new
// object at the head of its list (docs/wiki/engine/saved-games.md#writing-the-world-file, the global lists)

#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorshipRecords.h>
#include <entt/core/type_info.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/FootpathFinder.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The entities that are a list's objects, in any order
using Members = std::function<std::set<entt::entity>(const ecs::Registry& registry)>;

/// The entities with any of the components, from the storages the registry holds (one it never made holds none)
template <typename... Component>
Members AnyOf()
{
	return [](const ecs::Registry& registry) {
		std::set<entt::entity> found;
		const std::set<entt::id_type> types {entt::type_hash<Component>::value()...};
		registry.EachStorage([&types, &found](entt::id_type id, const auto& storage) {
			if (types.contains(id))
			{
				found.insert(storage.begin(), storage.end());
			}
		});
		return found;
	};
}

/// When an object was made, against the others of its list: the higher, the newer; none when it is not known
using Order = std::function<int64_t(const ecs::Registry& registry, entt::entity entity)>;

/// The game's creation number of an object, -1 for none
int64_t CreationNumber(const ecs::Registry& registry, entt::entity entity)
{
	const auto* index = registry.TryGet<const components::ObjectCreationIndex>(entity);
	return index != nullptr ? static_cast<int64_t>(index->value) : int64_t {-1};
}

/// The stamp a component keeps of the order its kind was made in, from 1; -1 for none
template <typename Component>
int64_t Stamp(const ecs::Registry& registry, entt::entity entity)
{
	const auto stamp = registry.Get<const Component>(entity).creationStamp;
	return stamp != 0 ? static_cast<int64_t>(stamp) : int64_t {-1};
}

/// A list's objects in the game's order: the newest first, as each is added at the list's head when it is made. A
/// loaded game's list keeps its loaded order, without the objects gone since, behind the objects made since, which
/// are all newer. An object whose making's order is not known stops the write, as its place is not known
std::vector<gamesave::ObjectNumber> NewestFirst(WriteContext& context, GlobalList list, const Members& members,
                                                const Order& order = CreationNumber)
{
	const auto& registry = context.GetRegistry();
	std::vector<gamesave::ObjectNumber> numbers;
	// the loaded list, and the entities it names
	std::set<entt::entity> loaded;
	if (const auto* first = context.KeptWorldRecord(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			for (const auto number : game->lists.at(static_cast<size_t>(list)).numbers)
			{
				const auto target = context.TargetOf(number);
				if (target.has_value() && target->entity != entt::null)
				{
					// an object deleted since has left the list, and so has one whose entity is of another class now
					// (a tree felled into a dead tree: the original deletes the tree, which leaves the trees' list)
					if (context.IsGone(number))
					{
						continue;
					}
					loaded.insert(target->entity);
				}
				numbers.push_back(number);
			}
		}
	}
	// the objects made since, newest first, in front of it
	std::vector<std::pair<int64_t, entt::entity>> made;
	for (const auto entity : members(registry))
	{
		if (!loaded.contains(entity))
		{
			made.emplace_back(order(registry, entity), entity);
		}
	}
	std::ranges::sort(made, std::ranges::greater {}, &std::pair<int64_t, entt::entity>::first);
	std::vector<gamesave::ObjectNumber> front;
	front.reserve(made.size());
	for (const auto& [index, entity] : made)
	{
		if (index < 0)
		{
			context.Fail("entity " + std::to_string(entt::to_integral(entity)) + " of the game's list " +
			             std::to_string(static_cast<size_t>(list)) +
			             " has no order of its making (its components: " + context.ComponentsOf(entity) + ")");
			return {};
		}
		front.push_back(context.Reference(entity));
	}
	front.insert(front.end(), numbers.begin(), numbers.end());
	return front;
}

/// The town centres: the abodes of the town centre's kind, and those loaded as town centres
std::set<entt::entity> TownCentres(const ecs::Registry& registry)
{
	std::set<entt::entity> found;
	for (const auto entity : AnyOf<components::Abode>()(registry))
	{
		if (registry.AllOf<components::LoadedRecord<gamesave::TownCentreRecord>>(entity))
		{
			found.insert(entity);
			continue;
		}
		const auto& abode = registry.Get<const components::Abode>(entity);
		if (abode.info != AbodeInfo::None && Locator::infoConstants::has_value() &&
		    Locator::infoConstants::value().abode.at(static_cast<size_t>(abode.info)).abodeType == AbodeType::TownCentre)
		{
			found.insert(entity);
		}
	}
	return found;
}

/// A list openblack makes none of: as the load kept it, else empty
std::vector<gamesave::ObjectNumber> NoneMade(WriteContext& context, GlobalList list)
{
	return NewestFirst(context, list, [](const ecs::Registry& /*registry*/) { return std::set<entt::entity> {}; });
}
} // namespace

void openblack::ecs::saved_world_write::RegisterGameListWriters(ClassWriters& writers)
{
	const auto newest = [&writers](GlobalList list, Members members) {
		writers.RegisterList(
		    list, [list, members = std::move(members)](WriteContext& context) { return NewestFirst(context, list, members); });
	};
	newest(GlobalList::StreetLanterns, AnyOf<components::StreetLantern>());
	newest(GlobalList::Rewards, AnyOf<components::Reward>());
	newest(GlobalList::Balls, AnyOf<components::Ball>());
	newest(GlobalList::Livings, AnyOf<components::Villager, components::Animal, components::Creature>());
	newest(GlobalList::Fields, AnyOf<components::Field>());
	newest(GlobalList::FishFarms, AnyOf<components::FishFarm>());
	newest(GlobalList::Fireballs, AnyOf<components::MagicFireBall>());
	newest(GlobalList::MapShields, AnyOf<components::MagicShield>());
	newest(GlobalList::ScriptHighlights, AnyOf<components::ScriptHighlight>());
	newest(GlobalList::AnimatedStatics, AnyOf<components::AnimatedStatic>());
	newest(GlobalList::Trees, AnyOf<components::Tree>());
	newest(GlobalList::BigForests, AnyOf<components::BigForest>());
	newest(GlobalList::TownCentres, TownCentres);
	newest(GlobalList::Whales, AnyOf<components::Shark>());
	newest(GlobalList::Fragments, AnyOf<components::Fragment>());
	// every object of several map cells joins the head of the game's list of them as its constructor runs: the abodes
	// (the fields, the stores, the creches, the graveyards, the town centres and the pitches among them), the citadels'
	// pieces, the big forests, the mobile statics (the rocks and the bonfires), the dead trees, the fish farms, the
	// features, the animated statics, the town centres' totems, the spell icons, the flowers and the teleports' stones
	newest(GlobalList::MultiMapFixed,
	       AnyOf<components::Abode, components::CitadelHeart, components::WorshipSite, components::WorshipTotem,
	             components::BigForest, components::MobileStatic, components::DeadTree, components::FishFarm,
	             components::Feature, components::AnimatedStatic, components::VillageTotem, components::SpellIcon,
	             components::Flowers, components::TeleportStone>());
	// the lists of things that are not objects, each kept in the order its kind was made
	const auto stamped = [&writers](GlobalList list, Members members, Order order) {
		writers.RegisterList(list, [list, members = std::move(members), order = std::move(order)](WriteContext& context) {
			return NewestFirst(context, list, members, order);
		});
	};
	stamped(GlobalList::Footpaths, AnyOf<components::Footpath>(), Stamp<components::Footpath>);
	stamped(GlobalList::Towns, AnyOf<components::Town>(), Stamp<components::Town>);
	stamped(GlobalList::FootpathFinders, AnyOf<components::FootpathFinder>(), Stamp<components::FootpathFinder>);
	stamped(GlobalList::Mist, AnyOf<components::Mist>(), Stamp<components::Mist>);
	stamped(GlobalList::Streams, AnyOf<components::Stream>(), Stamp<components::Stream>);
	stamped(GlobalList::BuildingSites, AnyOf<components::BuildingSite>(), Stamp<components::BuildingSite>);
	// the puzzle games: each joins the head of the game's list as it is made
	stamped(GlobalList::PuzzleGame, AnyOf<components::PuzzleGame>(), Stamp<components::PuzzleGame>);
	// the classes openblack makes none of, empty in every save read: the street lights, the waterfalls and the game's
	// own list of objects
	for (const auto list : {GlobalList::StreetLights, GlobalList::Waterfalls, GlobalList::Objects})
	{
		writers.RegisterList(list, [list](WriteContext& context) { return NoneMade(context, list); });
	}
	// the towns' and the worship sites' artifacts, which openblack keeps on the artefact's object: each joins the head
	// of the game's list of them as it is made, those made since the load in front of the loaded list
	writers.RegisterList(GlobalList::TownArtifacts, [](WriteContext& context) {
		const auto& registry = context.GetRegistry();
		std::vector<std::pair<uint64_t, entt::entity>> made;
		registry.Each<const components::TownArtefact>(
		    [&made](entt::entity entity, const components::TownArtefact& each) { made.emplace_back(each.made, entity); });
		std::ranges::sort(made, std::ranges::greater {}, &std::pair<uint64_t, entt::entity>::first);
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto& [stamp, entity] : made)
		{
			numbers.push_back(context.Reference(entity, gamesave::TownArtifactRecord::k_Type));
		}
		const auto kept = NoneMade(context, GlobalList::TownArtifacts);
		numbers.insert(numbers.end(), kept.begin(), kept.end());
		return numbers;
	});
	// the villagers without a town, which a villager joins at the head as it loses its town (or is made with none):
	// the town state's list as it stands, then those of the loaded list that are still without a town
	writers.RegisterList(GlobalList::VillagersWithoutTown, [](WriteContext& context) {
		const auto& registry = context.GetRegistry();
		std::vector<gamesave::ObjectNumber> numbers;
		std::set<entt::entity> listed;
		if (Locator::townStateSystem::has_value())
		{
			for (const auto villager : Locator::townStateSystem::value().Vagrants())
			{
				if (registry.Valid(villager) && listed.insert(villager).second)
				{
					numbers.push_back(context.Reference(villager));
				}
			}
		}
		if (const auto* first = context.KeptWorldRecord(1); first != nullptr)
		{
			if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
			{
				for (const auto number : game->lists.at(static_cast<size_t>(GlobalList::VillagersWithoutTown)).numbers)
				{
					const auto target = context.TargetOf(number);
					if (!target.has_value() || target->entity == entt::null)
					{
						numbers.push_back(number);
						continue;
					}
					const auto* villager =
					    registry.Valid(target->entity) ? registry.TryGet<const components::Villager>(target->entity) : nullptr;
					if (villager != nullptr && villager->town == entt::null && listed.insert(target->entity).second)
					{
						numbers.push_back(number);
					}
				}
			}
		}
		return numbers;
	});
}

void openblack::ecs::saved_world_write::RegisterFireFlyWriters(ClassWriters& writers, std::function<FireFlyWorld()> world)
{
	// the firefly's object row, and the raw words of its map coordinates as the record's points hold them
	constexpr uint32_t k_FireFlyRow = 2090;
	const auto pointOf = [](const map_coords::MapCoords& coords) {
		return gamesave::Point {std::bit_cast<float>(coords.x), std::bit_cast<float>(coords.z), coords.altitude};
	};
	const auto coordsOf = [](const map_coords::MapCoords& coords) {
		return gamesave::MapCoords {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
	};
	const auto bitsOf = [](const std::vector<float>& values) {
		std::vector<uint32_t> bits;
		bits.reserve(values.size());
		for (const auto value : values)
		{
			bits.push_back(std::bit_cast<uint32_t>(value));
		}
		return bits;
	};
	writers.RegisterSource(
	    gamesave::FireFlyRecord::k_Type,
	    [world](const ecs::Registry& /*registry*/) {
		    std::vector<uint64_t> keys;
		    for (const auto& fly : world().fireFlies)
		    {
			    keys.push_back(fly.creationIndex);
		    }
		    return keys;
	    },
	    [world, pointOf, coordsOf, bitsOf](WriteContext& context, uint64_t id) -> std::optional<gamesave::SavedRecord> {
		    const auto now = world();
		    const auto fly = std::ranges::find(now.fireFlies, static_cast<uint32_t>(id), &ecs::SavedFireFly::creationIndex);
		    if (fly == now.fireFlies.end())
		    {
			    context.Fail("firefly " + std::to_string(id) + " is gone");
			    return std::nullopt;
		    }
		    std::optional<gamesave::FireFlyRecord> kept;
		    if (auto record = context.KeptRecordOf(SourceKey {.type = gamesave::FireFlyRecord::k_Type, .id = id});
		        record.has_value())
		    {
			    kept = std::get<gamesave::FireFlyRecord>(std::move(*record));
		    }
		    auto record = kept.value_or(gamesave::FireFlyRecord {});
		    // a map object at where it is; its object of the firefly's row where it was made, with its creation number
		    record.position.position = coordsOf(fly->coords);
		    // the place's flags from its object in the map (the object draw list's listed mark among them); with no
		    // object, as kept, else a map object's
		    const auto keptFlags = kept.has_value() ? kept->position.flags : position_flags::k_InMap;
		    record.position.flags = context.GetRegistry().Valid(fly->object)
		                                ? PositionFlagsOf(context.GetRegistry(), fly->object, keptFlags)
		                                : keptFlags;
		    record.object.info = k_FireFlyRow;
		    record.object.coords = coordsOf(fly->madeAt);
		    record.object.creationIndex = fly->creationIndex;
		    record.word = static_cast<uint32_t>(fly->state);
		    record.points = {pointOf(fly->home), pointOf(fly->light), pointOf(fly->last),
		                     gamesave::Point {fly->drawn.x, fly->drawn.y, fly->drawn.z}};
		    record.words = {std::bit_cast<uint32_t>(fly->clock),    std::bit_cast<uint32_t>(fly->drift),
		                    std::bit_cast<uint32_t>(fly->progress), std::bit_cast<uint32_t>(fly->duration),
		                    std::bit_cast<uint32_t>(fly->speedA),   std::bit_cast<uint32_t>(fly->speedB)};
		    record.floats = fly->phases;
		    record.byte = fly->flags;
		    // every firefly writes the land's reward table and whether the next nightfall tops them up
		    record.first.values = bitsOf(now.sums);
		    record.second.values = bitsOf(now.probabilities);
		    record.flag = now.maySpawn ? 1 : 0;
		    return record;
	    });
	writers.RegisterList(GlobalList::Fireflies, [world = std::move(world)](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto& fly : world().fireFlies)
		{
			numbers.push_back(context.Reference(SourceKey {.type = gamesave::FireFlyRecord::k_Type, .id = fly.creationIndex}));
		}
		return numbers;
	});
}
