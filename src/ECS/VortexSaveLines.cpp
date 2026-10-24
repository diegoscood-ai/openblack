/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexSaveLines.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAge.h"
#include "LHScriptX/ScriptWriter.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::vortex::crossing;

namespace
{
/// A town's number as the land scripts find it (its script number, or its own when it has none, as
/// town_queries::ScriptIdOf), -1 for none
int32_t TownNumber(const Registry& registry, entt::entity town)
{
	const auto* data = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	return data != nullptr ? static_cast<int32_t>(data->scriptId.value_or(data->id)) : -1;
}

/// The other things of the class and kind of `thing` lying on the land exactly where it stands, at the same height above
/// it: the very same place across the land and the very same height (only those lying on the land are found there,
/// not one flying, held or being taken)
template <typename Class>
std::vector<entt::entity> IdenticalOf(const Registry& registry, entt::entity thing, const vortex_save_lines::World& world)
{
	std::vector<entt::entity> identical;
	const auto* own = registry.Valid(thing) ? registry.TryGet<const Class>(thing) : nullptr;
	if (own == nullptr)
	{
		return identical;
	}
	const auto place = world.placeOf(thing);
	registry.Each<const Class>([&](entt::entity other, const Class& otherOwn) {
		if (other == thing || otherOwn.type != own->type || !world.lyingOnTheLand(other))
		{
			return;
		}
		const auto otherPlace = world.placeOf(other);
		if (otherPlace.x == place.x && otherPlace.z == place.z && otherPlace.altitude == place.altitude)
		{
			identical.push_back(other);
		}
	});
	return identical;
}

/// The angles a thing keeps, as each of the original's classes writes them: the angles it was given, as they are, not
/// angles worked out again from the way it faces (which come out in another order of turns for a thing that leans, and
/// wrapped for one that stands upright)
glm::vec3 AnglesOf(const Registry& registry, entt::entity thing)
{
	return placed_turn::AnglesOf(registry, thing);
}
} // namespace

std::optional<Line> vortex_save_lines::LineFor(const Registry& registry, entt::entity thing, const map_coords::MapCoords& place,
                                               const map_coords::MapCoords& vortex, const World& world)
{
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto offset = place - vortex;
	const float scale = transform->scale.x;
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		const auto town = TownNumber(registry, villager->town);
		const auto age = villager_age::AgeOf(world.turn, villager->birthTurn);
		// one of no tribe is written by its row of the villagers' table
		if (villager->tribe == Tribe::NONE)
		{
			return SpecialVillagerLine {.town = town, .offset = offset, .row = world.villagerRow(thing), .age = age};
		}
		std::string type(k_TribeStrs.at(static_cast<size_t>(villager->tribe)));
		type += '_';
		type += k_VillagerNumberStrs.at(static_cast<size_t>(villager->number));
		return VillagerLine {.town = town, .offset = offset, .villagerType = type, .age = age};
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		const auto* flock = animal->flock != entt::null && registry.Valid(animal->flock)
		                        ? registry.TryGet<const Flock>(animal->flock)
		                        : nullptr;
		return AnimalLine {.offset = offset,
		                   .type = static_cast<int32_t>(animal->type),
		                   .flock = flock != nullptr ? flock->id : -1,
		                   .town = flock != nullptr ? TownNumber(registry, flock->town) : -1,
		                   .age = animal->age};
	}
	if (const auto* pot = registry.TryGet<const Pot>(thing))
	{
		// a pile of a storage pit stays with it
		if (world.ofAStoragePit(thing))
		{
			return std::nullopt;
		}
		return PotLine {.offset = offset,
		                .row = static_cast<int32_t>(pot->type),
		                .resource = world.potResource(pot->type),
		                .amount = pot->amount};
	}
	if (const auto* tree = registry.TryGet<const Tree>(thing))
	{
		return TreeLine {.offset = offset,
		                 .row = static_cast<int32_t>(tree->type),
		                 .nonScenic = tree->isNonScenic,
		                 .yAngle = AnglesOf(registry, thing).y,
		                 .scale = scale,
		                 .maxScale = tree->maxSize};
	}
	if (const auto* globe = registry.TryGet<const OneOffSpellSeed>(thing))
	{
		// written by the miracle its seed casts at its power-up, not by the seed
		return OneOffSpellLine {.offset = offset, .magic = world.magicName(globe->seedType, globe->powerUp)};
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(thing))
	{
		const auto angles = AnglesOf(registry, thing);
		// an artefact of a player stays one, of no town: it joins its player's nearest as it is made again
		std::optional<ArtefactLine> artefact;
		if (const auto* data = registry.TryGet<const TownArtefact>(thing); data != nullptr && data->player.has_value())
		{
			artefact = ArtefactLine {.player = std::string(k_PlayerNamesStrs.at(static_cast<size_t>(*data->player))),
			                         .value = data->value};
		}
		return MobileStaticLine {.offset = offset,
		                         .row = static_cast<int32_t>(still->type),
		                         .xAngle = angles.x,
		                         .yAngle = angles.y,
		                         .zAngle = angles.z,
		                         .scale = scale,
		                         .artefact = artefact};
	}
	if (registry.AllOf<Fragment>(thing))
	{
		// a piece knocked off a building is a rock of the rock's own row, and is written as one: it comes out as a plain
		// rock
		const auto angles = AnglesOf(registry, thing);
		return MobileStaticLine {.offset = offset,
		                         .row = static_cast<int32_t>(MobileStaticInfo::Rock),
		                         .xAngle = angles.x,
		                         .yAngle = angles.y,
		                         .zAngle = angles.z,
		                         .scale = scale};
	}
	if (const auto* scaffold = registry.TryGet<const Scaffold>(thing))
	{
		// a scaffold has a line of its own, with its town; the scaffolds have a single row
		return ScaffoldLine {.town = TownNumber(registry, scaffold->town),
		                     .offset = offset,
		                     .yAngle = AnglesOf(registry, thing).y,
		                     .scale = scale};
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
	{
		return MobileObjectLine {
		    .offset = offset, .row = static_cast<int32_t>(mobile->type), .yAngle = AnglesOf(registry, thing).y, .scale = scale};
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(thing))
	{
		// nobody's dead tree is the neutral player's
		const auto player = world.player(thing).value_or(PlayerNames::NEUTRAL);
		const auto angles = AnglesOf(registry, thing);
		return DeadTreeLine {.offset = offset,
		                     .player = std::string(k_PlayerNamesStrs.at(static_cast<size_t>(player))),
		                     .row = static_cast<int32_t>(dead->type),
		                     .life = world.life(thing),
		                     .xAngle = angles.x,
		                     .yAngle = angles.y,
		                     .zAngle = angles.z};
	}
	return std::nullopt;
}

std::optional<std::string> vortex_save_lines::Text(const Line& line)
{
	using lhscriptx::WriteCommand;
	return std::visit(
	    [](const auto& l) -> std::optional<std::string> {
		    using T = std::decay_t<decltype(l)>;
		    if constexpr (std::is_same_v<T, VillagerLine>)
		    {
			    return WriteCommand("CREATE_TOWN_VILLAGER", {l.town, l.offset, l.villagerType, static_cast<int32_t>(l.age)});
		    }
		    else if constexpr (std::is_same_v<T, SpecialVillagerLine>)
		    {
			    return WriteCommand("CREATE_SPECIAL_TOWN_VILLAGER", {l.town, l.offset, l.row, static_cast<int32_t>(l.age)});
		    }
		    else if constexpr (std::is_same_v<T, AnimalLine>)
		    {
			    return WriteCommand("CREATE_NEW_ANIMAL", {l.offset, l.type, l.flock, l.town, static_cast<int32_t>(l.age)});
		    }
		    else if constexpr (std::is_same_v<T, PotLine>)
		    {
			    return WriteCommand("CREATE_POT", {l.offset, l.row, l.resource, static_cast<int32_t>(l.amount)});
		    }
		    else if constexpr (std::is_same_v<T, TreeLine>)
		    {
			    return WriteCommand("CREATE_NEW_TREE", {l.forest, l.offset, l.row, static_cast<int32_t>(l.nonScenic ? 1 : 0),
			                                            l.yAngle, l.scale, l.maxScale});
		    }
		    else if constexpr (std::is_same_v<T, MobileStaticLine>)
		    {
			    auto text = WriteCommand("CREATE_MOBILE_STATIC",
			                             {l.offset, l.row, l.offset.altitude, l.xAngle, l.yAngle, l.zAngle, l.scale});
			    if (text.has_value() && l.artefact.has_value())
			    {
				    const auto artefact =
				        WriteCommand("MAKE_LAST_OBJECT_ARTIFACT", {l.artefact->town, l.artefact->player, l.artefact->value});
				    if (!artefact.has_value())
				    {
					    return std::nullopt;
				    }
				    *text += *artefact;
			    }
			    return text;
		    }
		    else if constexpr (std::is_same_v<T, ScaffoldLine>)
		    {
			    // the angle and the size in thousandths, truncated, as a thing lying about
			    constexpr float k_Thousand = 1000.0f;
			    return WriteCommand("CREATE_SCAFFOLD", {l.town, l.offset, l.row, map_coords::FtoL(l.yAngle * k_Thousand),
			                                            map_coords::FtoL(l.scale * k_Thousand)});
		    }
		    else if constexpr (std::is_same_v<T, OneOffSpellLine>)
		    {
			    return WriteCommand("CREATE_ONE_SHOT_SPELL_PU", {l.offset, l.magic});
		    }
		    else if constexpr (std::is_same_v<T, MobileObjectLine>)
		    {
			    // the angle and the size in thousandths, truncated
			    constexpr float k_Thousand = 1000.0f;
			    return WriteCommand("CREATE_MOBILEOBJECT", {l.offset, l.row, map_coords::FtoL(l.yAngle * k_Thousand),
			                                                map_coords::FtoL(l.scale * k_Thousand)});
		    }
		    else
		    {
			    static_assert(std::is_same_v<T, DeadTreeLine>);
			    return WriteCommand("CREATE_DEAD_TREE", {l.offset, l.player, l.row, l.life, l.xAngle, l.yAngle, l.zAngle});
		    }
	    },
	    line);
}

std::optional<PlayerNames> vortex_save_lines::OwnerOf(const Registry& registry, entt::entity thing)
{
	const auto* dead = thing != entt::null && registry.Valid(thing) ? registry.TryGet<const DeadTree>(thing) : nullptr;
	if (dead == nullptr)
	{
		return std::nullopt;
	}
	return dead->owner;
}

std::vector<entt::entity> vortex_save_lines::IdenticalStatics(const Registry& registry, entt::entity thing, const World& world)
{
	return IdenticalOf<MobileStatic>(registry, thing, world);
}

std::vector<entt::entity> vortex_save_lines::IdenticalMobileObjects(const Registry& registry, entt::entity thing,
                                                                    const World& world)
{
	// a scaffold is not one: it has its own line and its own table
	if (registry.Valid(thing) && registry.AllOf<Scaffold>(thing))
	{
		return {};
	}
	auto identical = IdenticalOf<MobileObject>(registry, thing, world);
	std::erase_if(identical, [&registry](entt::entity other) { return registry.AllOf<Scaffold>(other); });
	return identical;
}
