/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptContainers.h"

#include <vector>

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::script_containers
{
using components::Abode;
using components::Animal;
using components::Dance;
using components::DanceGroup;
using components::Flock;
using components::Town;
using components::Transform;
using components::Villager;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

void Error(const char* message)
{
	// The original's script error message does nothing: openblack logs what the original would have said
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", message);
}

/// The thing of an id (the entity), entt::null for 0 or a lost one
entt::entity Thing(uint32_t id)
{
	const auto entity = static_cast<entt::entity>(id);
	return id != 0 && Entities().Valid(entity) ? entity : entt::null;
}

uint32_t Id(entt::entity thing)
{
	return thing == entt::null ? 0u : static_cast<uint32_t>(thing);
}

bool IsFlock(entt::entity thing)
{
	return thing != entt::null && Entities().AllOf<Flock>(thing);
}

bool IsTown(entt::entity thing)
{
	return thing != entt::null && Entities().AllOf<Town>(thing);
}

bool IsDance(entt::entity thing)
{
	return dances::IsDance(Entities(), thing);
}

/// A living thing: (approximate) a villager or an animal; a creature is not taken into a flock here
bool IsLiving(entt::entity thing)
{
	return thing != entt::null && Entities().AnyOf<Villager, Animal>(thing);
}

/// OBJECT_TYPE ANIMAL: the members that get a flock of their own when they leave one
bool IsAnimal(entt::entity thing)
{
	return Entities().AllOf<Animal>(thing);
}

/// The town's villager order: its abodes (newest first: town_stats::AbodesOf), each one's inhabitants, then the
/// homeless. `fn` returning true stops it (the villager)
entt::entity WalkTownVillagers(entt::entity town, const std::function<bool(entt::entity)>& fn)
{
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* a = Entities().TryGet<const Abode>(abode);
		if (a == nullptr)
		{
			continue;
		}
		const auto inhabitants = a->inhabitants; // a copy: the callback may move the villager
		for (const auto villager : inhabitants)
		{
			if (fn(villager))
			{
				return villager;
			}
		}
	}
	const auto homeless = town_villagers::Homeless(town);
	for (const auto villager : homeless)
	{
		if (fn(villager))
		{
			return villager;
		}
	}
	return entt::null;
}

/// A living joins a flock: AddLeader or AddMember ("Living already in Flock"), then
/// SetScriptState(living, 27 MOVE_IN_FLOCK) either way
void JoinFlock(entt::entity flock, entt::entity living, bool asLeader)
{
	if (asLeader)
	{
		flocks::AddLeader(flock, living);
	}
	else if (!flocks::AddMember(flock, living))
	{
		Error("Living already in Flock");
	}
	script_held::SetLivingScriptState(living, static_cast<uint32_t>(VillagerStates::MoveInFlock));
}

/// FLOCK_ATTACH with a flock target (or, for two livings, a new flock)
uint32_t AttachToFlock(uint32_t objectId, uint32_t targetId, bool asLeader)
{
	const auto object = Thing(objectId);
	const auto target = Thing(targetId);
	if (object == entt::null || target == entt::null)
	{
		Error("Thing not added to Flock");
		return 0;
	}
	entt::entity flock = entt::null;
	entt::entity living = entt::null;
	entt::entity other = entt::null;
	// the obj a flock, else a living
	if (IsFlock(object))
	{
		flock = object;
	}
	else if (IsLiving(object))
	{
		living = object;
	}
	else
	{
		Error("JONTY-Trying to add non living to flock");
		return 0;
	}
	// the target a flock (two flocks merge), else a living
	if (IsFlock(target))
	{
		if (flock != entt::null)
		{
			// merge into the target, push the target
			flocks::Merge(target, flock);
			return targetId;
		}
		flock = target;
	}
	else if (IsLiving(target))
	{
		if (living == entt::null)
		{
			living = target; // the obj is the flock, the target the living
		}
		else
		{
			other = target; // two livings
		}
	}
	else
	{
		Error("JONTY-Trying to add non living to flock");
		return 0;
	}
	if (flock != entt::null)
	{
		JoinFlock(flock, living, asLeader);
		// push the flock's id, increment the living's
		if (IsFlock(object))
		{
			script_held::IncrementReference(target);
			return objectId;
		}
		script_held::IncrementReference(object);
		return targetId;
	}
	// two livings: a new flock at the target's position
	const auto* transform = Entities().TryGet<const Transform>(other);
	// the default flock info; the original also passes a player ((pending) which)
	const auto fresh = flocks::Create(transform != nullptr ? transform->position : glm::vec3(0.0f), flocks::k_DefaultFlockInfo,
	                                  std::nullopt, flocks::k_ScriptFlockId);
	// a thing created by the script, and a reference to the target
	script_held::AddScriptThing(fresh, true);
	script_held::IncrementReference(other);
	// the id is pushed before the members are added
	if (asLeader)
	{
		// AddLeader, increment the obj; no MOVE_IN_FLOCK state
		flocks::AddLeader(fresh, living);
		script_held::IncrementReference(living);
	}
	else if (flocks::AddMember(fresh, living))
	{
		script_held::SetLivingScriptState(living, static_cast<uint32_t>(VillagerStates::MoveInFlock));
		script_held::IncrementReference(living);
	}
	else
	{
		Error("Living already in Flock");
	}
	// the target too, a second reference for it (literal)
	if (!flocks::AddMember(fresh, other))
	{
		Error("Living already in Flock");
		return Id(fresh);
	}
	script_held::SetLivingScriptState(other, static_cast<uint32_t>(VillagerStates::MoveInFlock));
	script_held::IncrementReference(other);
	return Id(fresh);
}

/// The town attach's loop callback: a villager leaves its town and joins this one
void MoveToTown(entt::entity villager, entt::entity town)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		// (openblack) the original's callback also pushes a 0 onto the script stack here, not reproduced
		Error("JONTY-Trying to add non villager to town");
		return;
	}
	if (v->town != entt::null && Entities().Valid(v->town))
	{
		town_villagers::RemoveVillager(v->town, villager);
	}
	town_villagers::AddVillagerToTown(town, villager);
}

/// Attach to a town: pushes the target (the town's id)
uint32_t AttachToTown(uint32_t objectId, uint32_t targetId, entt::entity town)
{
	const auto object = Thing(objectId);
	if (object == entt::null)
	{
		return 0;
	}
	if (IsContainer(object))
	{
		// the type's loop with MoveToTown on each member
		if (!ForEachMember(object, [town](entt::entity member) {
			    MoveToTown(member, town);
			    return false;
		    }))
		{
			Error("No Loop function for type");
		}
		return targetId;
	}
	// (pending) a spell dispenser's own attach (the town as its argument)
	if (!Entities().AllOf<Villager>(object))
	{
		Error("JONTY-Trying to add non villager to town");
		return 0;
	}
	MoveToTown(object, town);
	return targetId;
}

/// FLOCK_DETACH from a flock
uint32_t DetachFromFlock(uint32_t objectId, entt::entity flock)
{
	const auto object = Thing(objectId);
	if (object != entt::null)
	{
		if (!IsLiving(object))
		{
			Error("Trying to remove non living from flock");
			Error("Thing not removed from Flock");
			return 0;
		}
		// an animal a flock of its own, a villager only removed (argument 0); Decrement; push
		if (IsAnimal(object))
		{
			flocks::SeparateIntoNewFlock(flock, object, false);
		}
		else
		{
			flocks::RemoveLiving(flock, object, false);
		}
		script_held::DecrementReference(object);
		return objectId;
	}
	// no obj: a random member, not the tail when there are two or more
	const auto exclude = flocks::Size(flock) > 1 ? flocks::Leader(flock) : entt::null;
	const auto member = flocks::RandomMember(flock, exclude);
	if (member == entt::null)
	{
		Error("Thing not found from Flock");
		Error("Thing not removed from Flock");
		return 0;
	}
	if (IsAnimal(member))
	{
		flocks::SeparateIntoNewFlock(flock, member, false);
	}
	else
	{
		flocks::RemoveLiving(flock, member, false);
	}
	// its script slot (made when it has none), decrement, push the id
	script_held::AddScriptThing(member, false);
	script_held::DecrementReference(member);
	return Id(member);
}

/// The sexes a dance's group must take for a living to join it: a villager's own, anything else either
uint32_t DanceSex(entt::entity living)
{
	const auto* villager = Entities().TryGet<const Villager>(living);
	if (villager == nullptr)
	{
		return DanceGroup::k_AnySex;
	}
	return villager->sex == Villager::Sex::FEMALE ? DanceGroup::k_Women : DanceGroup::k_Men;
}

/// A living joins a dance: out of any dance it is in, into the group whose turn it is that takes it (or none), then
/// the IN_DANCE script state either way
void JoinDance(entt::entity living, entt::entity dance)
{
	dances::RemoveDancer(Entities(), living);
	static_cast<void>(dances::AddDancer(Entities(), dance, living, DanceSex(living)));
	script_held::SetLivingScriptState(living, static_cast<uint32_t>(VillagerStates::InDance));
}

/// FLOCK_ATTACH with a dance target: a container's living members each join it, controlled by the script and holding a
/// reference, and the target is pushed; a living joins it (as what it is danced about when `asLeader`), holding a
/// reference; anything else "JONTY-Trying to add non living to dance" and 0
uint32_t AttachToDance(uint32_t objectId, uint32_t targetId, entt::entity dance, bool asLeader)
{
	const auto object = Thing(objectId);
	if (object == entt::null)
	{
		Error("JONTY-Trying to add non living to dance");
		return 0;
	}
	if (IsContainer(object))
	{
		// the type's loop: a member that is not living is passed over
		if (!ForEachMember(object, [dance](entt::entity member) {
			    if (IsLiving(member))
			    {
				    JoinDance(member, dance);
				    script_held::SetControlledByScript(member, true);
				    script_held::IncrementReference(member);
			    }
			    return false;
		    }))
		{
			Error("No Loop function for type");
		}
		return targetId;
	}
	if (!IsLiving(object))
	{
		Error("JONTY-Trying to add non living to dance");
		return 0;
	}
	if (asLeader)
	{
		Entities().Get<Dance>(dance).owner = object;
	}
	JoinDance(object, dance);
	script_held::IncrementReference(object);
	return targetId;
}

/// FLOCK_DETACH from a dance: the living given leaves it, letting go of its reference; with none given, the first
/// dancer of its file's groups, passing over what it is danced about while it has more than one dancer, which is
/// handed to the script. The id it pushes (0 for none)
uint32_t DetachFromDance(uint32_t objectId, entt::entity dance)
{
	const auto object = Thing(objectId);
	if (object != entt::null)
	{
		if (!IsLiving(object))
		{
			Error("Trying to remove non living from dance");
			Error("Thing not removed from Dance");
			return 0;
		}
		dances::RemoveDancer(Entities(), object);
		script_held::DecrementReference(object);
		return objectId;
	}
	const auto& data = Entities().Get<const Dance>(dance);
	const auto exclude = data.dancers == 1 ? entt::null : data.owner;
	const auto dancer = dances::FirstDancer(Entities(), dance, exclude);
	if (dancer == entt::null)
	{
		Error("Thing not removed from Dance");
		return 0;
	}
	dances::RemoveDancer(Entities(), dancer);
	// its script slot (made when it has none), decrement, push the id
	script_held::AddScriptThing(dancer, false);
	script_held::DecrementReference(dancer);
	return Id(dancer);
}

/// FLOCK_DETACH from a town; pushes 0 either way
uint32_t DetachFromTown(uint32_t objectId, entt::entity town)
{
	const auto object = Thing(objectId);
	const auto* villager = object != entt::null ? Entities().TryGet<const Villager>(object) : nullptr;
	if (villager != nullptr)
	{
		if (villager->town == town)
		{
			town_villagers::RemoveVillager(town, object);
			return 0;
		}
		Error("Wrong Town");
	}
	Error("Thing not removed from Town");
	return 0;
}
} // namespace

bool IsContainer(entt::entity thing)
{
	// Towns, dances and flocks are containers
	return IsFlock(thing) || IsTown(thing) || IsDance(thing);
}

bool ForEachMember(entt::entity container, const std::function<bool(entt::entity)>& fn)
{
	if (IsFlock(container))
	{
		for (const auto member : flocks::MembersFromHead(container))
		{
			if (fn(member))
			{
				break;
			}
		}
		return true;
	}
	if (IsTown(container))
	{
		WalkTownVillagers(container, fn);
		return true;
	}
	if (IsDance(container))
	{
		// group by group, each in the order its dancers joined (a copy: the callback may take one out)
		for (const auto dancer : dances::Dancers(Entities(), container))
		{
			if (fn(dancer))
			{
				break;
			}
		}
		return true;
	}
	return false;
}

entt::entity Find(entt::entity container, script::ObjectType type, uint32_t subtype, bool excludingScripted)
{
	// not in a script and matching when excluding, else matching only
	const auto filter = [type, subtype, excludingScripted](entt::entity thing) {
		return (!excludingScripted || !script_held::IsInScript(thing)) && script_type::Matches(thing, type, subtype);
	};
	if (IsFlock(container))
	{
		return flocks::FindLiving(container, filter);
	}
	if (IsDance(container))
	{
		// the first dancer that passes, group by group
		for (const auto dancer : dances::Dancers(Entities(), container))
		{
			if (filter(dancer))
			{
				return dancer;
			}
		}
		return entt::null;
	}
	if (IsTown(container))
	{
		switch (type)
		{
		case script::ObjectType::Villager:
		case script::ObjectType::VillagerChild:
			return WalkTownVillagers(container, filter);
		case script::ObjectType::Store:
			return town_queries::GetStoragePit(container); // no filter
		case script::ObjectType::Animal:
			// (pending) the town's animals, not kept by openblack's towns
			return entt::null;
		default:
			Error("Looking for strange type in Town");
			return entt::null;
		}
	}
	Error("Cannot look in object");
	return entt::null;
}

entt::entity CreateFlock(const glm::vec3& position)
{
	// map coordinates (x and z through 16.16; (approximate) y kept as the script gave it); then a thing created by the script
	const glm::vec3 coords(map_coords::Quantise(position.x), position.y, map_coords::Quantise(position.z));
	// the default flock info; the original also passes a player ((pending) which)
	const auto flock = flocks::Create(coords, flocks::k_DefaultFlockInfo, std::nullopt, flocks::k_ScriptFlockId);
	script_held::AddScriptThing(flock, true);
	return flock;
}

uint32_t Attach(uint32_t object, uint32_t target, bool asLeader)
{
	const auto obj = Thing(object);
	const auto container = Thing(target);
	if (obj == entt::null)
	{
		Error("Id deleted to attach");
		return 0;
	}
	if (container == entt::null)
	{
		Error("Id deleted to attach to");
		return 0;
	}
	// a dance: controlled by the script, then attached
	if (IsDance(container))
	{
		script_held::SetControlledByScript(container, true);
		return AttachToDance(object, target, container, asLeader);
	}
	if (IsFlock(container))
	{
		script_held::SetControlledByScript(container, true);
		return AttachToFlock(object, target, asLeader);
	}
	if (IsTown(container))
	{
		return AttachToTown(object, target, container);
	}
	Error("Thing not added to id");
	return 0;
}

uint32_t Detach(uint32_t container, uint32_t object)
{
	const auto thing = Thing(container);
	if (thing == entt::null)
	{
		// "From thing dead!" and no push at all; (openblack) 0, so that the script's stack stays whole
		Error("From thing dead!");
		return 0;
	}
	if (IsFlock(thing))
	{
		return DetachFromFlock(object, thing);
	}
	// a dance: detach from it
	if (IsDance(thing))
	{
		return DetachFromDance(object, thing);
	}
	if (IsTown(thing))
	{
		return DetachFromTown(object, thing);
	}
	if (IsLiving(thing))
	{
		// a living stands for its flock
		if (const auto flock = flocks::FlockOf(thing); flock != entt::null)
		{
			return DetachFromFlock(object, flock);
		}
		Error("No flock for living so what the...");
		return 0;
	}
	Error("Not living - confused");
	return 0;
}

void Disband(uint32_t container)
{
	const auto thing = Thing(container);
	if (thing == entt::null)
	{
		Error("Disbanding a NULL object");
		Error("Bad Id for Disband");
		return;
	}
	if (IsFlock(thing))
	{
		// from the head, the next one read before each removal
		for (const auto member : flocks::MembersFromHead(thing))
		{
			// an animal, with a game flag clear ((pending) the flag)
			if (IsAnimal(member))
			{
				flocks::SeparateIntoNewFlock(thing, member, false);
			}
			else
			{
				flocks::RemoveLiving(thing, member, false);
			}
			script_held::DecrementReference(member);
			if (script_held::IsControlledByScript(member))
			{
				script_held::SetLivingScriptState(member, static_cast<uint32_t>(VillagerStates::InScript));
			}
		}
		return;
	}
	if (IsDance(thing))
	{
		// while it has dancers: the first of its file's groups' leaves it and lets go of its reference, a controlled one
		// gets the IN_SCRIPT state
		while (Entities().Get<const Dance>(thing).dancers > 0)
		{
			const auto dancer = dances::FirstDancer(Entities(), thing, entt::null);
			if (dancer == entt::null)
			{
				// (openblack) the game asks again for ever: here the loop ends
				Error("Should never happen");
				return;
			}
			dances::RemoveDancer(Entities(), dancer);
			script_held::DecrementReference(dancer);
			if (script_held::IsControlledByScript(dancer))
			{
				script_held::SetLivingScriptState(dancer, static_cast<uint32_t>(VillagerStates::InScript));
			}
		}
		return;
	}
	// a town or an abode, nothing
	if (IsTown(thing) || Entities().AllOf<Abode>(thing))
	{
		return;
	}
	Error("Bad Id for Disband");
}

std::optional<float> Size(uint32_t container)
{
	const auto thing = Thing(container);
	// the count as an unsigned 32-bit value, converted to float
	if (IsFlock(thing))
	{
		return static_cast<float>(flocks::Size(thing));
	}
	if (IsTown(thing))
	{
		// the town's adults + children
		const auto& stats = Entities().Get<const Town>(thing).stats;
		return static_cast<float>(static_cast<uint64_t>(stats.adults + stats.children));
	}
	if (IsDance(thing))
	{
		// its dancers
		return static_cast<float>(dances::Size(Entities(), thing));
	}
	// (pending) a football's size
	Error("Cannot Find Flock/Dance/Town Size");
	return std::nullopt;
}

void ChangeInnerOuter(uint32_t object, float inner, float outer, float calm)
{
	const auto thing = Thing(object);
	if (!IsFlock(thing))
	{
		return; // (pending) a weather thing's own version
	}
	auto& flock = Entities().Get<Flock>(thing);
	// 0 keeps the field; else truncated into the u16
	if (outer != 0.0f)
	{
		flock.domainRadius = static_cast<uint16_t>(map_coords::FtoL(outer));
	}
	if (inner != 0.0f)
	{
		flock.flockDistance = static_cast<uint16_t>(map_coords::FtoL(inner));
	}
	flock.calm = map_coords::FtoL(calm);
}

std::optional<glm::vec3> PlaceOf(entt::entity container)
{
	const auto& registry = Entities();
	if (!registry.Valid(container))
	{
		return std::nullopt;
	}
	if (const auto* dance = registry.TryGet<const Dance>(container))
	{
		return glm::vec3(map_coords::ToMetres(dance->place.x), dance->place.altitude, map_coords::ToMetres(dance->place.z));
	}
	if (const auto* flock = registry.TryGet<const Flock>(container))
	{
		// its centre keeps the height it was given; the game holds that as a height above the land at its x and z
		const auto& centre = flock->domainCentre;
		const float ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({centre.x, centre.z}) : 0.0f;
		return glm::vec3(centre.x, centre.y - ground, centre.z);
	}
	return std::nullopt;
}

bool TakePopulated(entt::entity container, entt::entity living)
{
	if (IsDance(container))
	{
		JoinDance(living, container);
	}
	else if (IsFlock(container))
	{
		JoinFlock(container, living, false);
	}
	else
	{
		Error("Not implemented - Thing not added to id");
		return false;
	}
	script_held::IncrementReference(living);
	return true;
}

} // namespace openblack::ecs::script_containers
