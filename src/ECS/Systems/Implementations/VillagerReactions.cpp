/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerReactions.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MissionaryControl.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/CreatureReactionFacts.h"
#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Effects/PredatorFlee.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/InTheAir.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownCreatureAttitude.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerBreeding.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMissionary.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Impressiveness.h"
#include "Physics/LivingRules.h"
#include "VillagerCover.h"
#include "VillagerCreature.h"
#include "VillagerFire.h"
#include "VillagerResourceReactions.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
namespace reactions = effects::reactions;
using villager_reactions::SlotType;

/// The villager type table for the slot types: 7 and 12
const SlotType k_ReactToFood {
    .sameTypeSwitch = false,
    .setup = &villager_resource_reactions::SetupReactToFood,
    .priority = &villager_resource_reactions::ReactToFoodPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};
const SlotType k_ReactToWood {
    .sameTypeSwitch = false,
    .setup = &villager_resource_reactions::SetupReactToWood,
    .priority = &villager_resource_reactions::ReactToWoodPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::TurnsBeforeReactingToWoodAgain,
};

void SetupFleeFromObject(entt::entity villager, entt::entity object, uint32_t reaction);
void SetupLookAtObject(entt::entity villager, entt::entity object, uint32_t reaction);

/// Every living thing takes a reaction to run from a thing (0) at the table's own priority for it, whatever the thing
uint8_t FleeFromObjectPriority([[maybe_unused]] entt::entity villager, [[maybe_unused]] uint32_t reaction,
                               [[maybe_unused]] uint32_t other)
{
	return Locator::infoConstants::value().reaction.at(0).priority;
}

/// The row of a thing to run from, 0: the villager runs from it as from a frightening miracle
const SlotType k_FleeFromObject {
    .sameTypeSwitch = false,
    .setup = &SetupFleeFromObject,
    .priority = &FleeFromObjectPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};

/// The rows of the miracles: 3 runs from a frightening one; 21 (a nice one) and 37 (an impressive one) share one row,
/// the villager stops to watch
const SlotType k_FleeFromSpell {
    .sameTypeSwitch = false,
    .setup = &SetupFleeFromObject,
    .priority = &villager_reactions::FleeFromSpellPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};
const SlotType k_LookAtNiceSpell {
    .sameTypeSwitch = false,
    .setup = &SetupLookAtObject,
    .priority = &villager_reactions::LookAtNiceSpellPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};

void SetupReactToFlyingObject(entt::entity villager, entt::entity object, uint32_t reaction);

/// The row of 9, a thing thrown from the hand: the villager runs from it or points at it
const SlotType k_ReactToFlyingObject {
    .sameTypeSwitch = false,
    .setup = &SetupReactToFlyingObject,
    .priority = &villager_reactions::ReactToFlyingObjectPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};

/// The row of 36, a shield's fall: the villager hides in a building of its town nearby, or panics in the open
/// (VillagerCover.cpp)
const SlotType k_ReactToMagicShieldDestroyed {
    .sameTypeSwitch = false,
    .setup = &villager_cover::SetupReactToMagicShieldDestroyed,
    .priority = &villager_reactions::ReactToMagicShieldDestroyedPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};

/// The row of 30, a breeder's call: one villager of the other sex at a time answers it and goes to the breeder
/// (ECS/Villager/VillagerBreeding.cpp)
const SlotType k_ReactToBreeder {
    .sameTypeSwitch = false,
    .setup = &villager::SetupReactToBreeder,
    .priority = &villager::ReactToBreederPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};

void SetupFleeFromPredator(entt::entity villager, entt::entity predator, uint32_t reaction);

/// The row of 28, a predator about: the villager runs from it, watches it or hides from it in a building of its town.
/// The type table lets a villager switch to another reaction of this same type
const SlotType k_FleeFromPredator {
    .sameTypeSwitch = true,
    .setup = &SetupFleeFromPredator,
    .priority = &villager_reactions::FleeFromPredatorPriority,
    .turnsToReact = &villager_reactions::TurnsToReactToPredator,
    .turnsBeforeAgain = &villager_reactions::TurnsBeforeReactingAgainToPredator,
};

/// The row of 6, a creature about: the villager inspects it, respects it or runs from it (VillagerCreature.cpp)
const SlotType k_ReactToCreature {
    .sameTypeSwitch = false,
    .setup = &villager_creature::SetupReactToCreature,
    .priority = &villager_creature::ReactToCreaturePriority,
    .turnsToReact = &villager_creature::TurnsToReactToCreature,
    .turnsBeforeAgain = &villager_creature::TurnsBeforeReactingAgainToCreature,
};

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint8_t TypeByte(openblack::Reaction type)
{
	return static_cast<uint8_t>(static_cast<int>(type));
}

/// The reaction a villager follows, whatever keeps it: the slot, else the miracle maps in ReactionValidate's
/// order (fire, teleport, shield, death). (inferred) the fire's, the shield's and the mourning's reaction is the one of
/// that type their object started (their maps keep no public id; one reaction of the type per object); the
/// teleport's is villager_teleport::CurrentReaction
struct Current
{
	uint32_t id {0};
	openblack::Reaction type {openblack::Reaction::None};
	[[nodiscard]] bool Any() const { return type != openblack::Reaction::None; }
};

Current CurrentOf(entt::entity villager)
{
	if (const auto* slot = Entities().TryGet<const LivingReaction>(villager); slot != nullptr && slot->reaction != 0)
	{
		return {slot->reaction, slot->type};
	}
	if (villager_fire::IsReacting(villager))
	{
		return {
		    reactions::GetReactionOfTypeInitiatedBy(villager_fire::ReactionObject(villager), openblack::Reaction::ReactToFire),
		    openblack::Reaction::ReactToFire};
	}
	if (villager_teleport::IsReacting(villager))
	{
		return {villager_teleport::CurrentReaction(villager), openblack::Reaction::ReactToTeleport};
	}
	if (villager_shield::IsReacting(villager))
	{
		return {reactions::GetReactionOfTypeInitiatedBy(villager_shield::ReactionObject(villager),
		                                                openblack::Reaction::ReactToMagicShield),
		        openblack::Reaction::ReactToMagicShield};
	}
	if (villager_mourning::IsReacting(villager))
	{
		return {reactions::GetReactionOfTypeInitiatedBy(villager_mourning::ReactionObject(villager),
		                                                openblack::Reaction::ReactToDeath),
		        openblack::Reaction::ReactToDeath};
	}
	return {};
}

/// The object of the reaction a villager follows, whatever keeps it: the slot, else the miracle maps (none: null)
entt::entity ReactionObjectOf(entt::entity villager)
{
	if (villager_reactions::SlotHeld(villager))
	{
		return villager_reactions::SlotObject(villager);
	}
	for (const auto object : {villager_fire::ReactionObject(villager), villager_teleport::ReactionObject(villager),
	                          villager_shield::ReactionObject(villager), villager_mourning::ReactionObject(villager)})
	{
		if (object != entt::null)
		{
			return object;
		}
	}
	return entt::null;
}

/// Where a reaction's object stands on the map (x, z in whole map units): a thing's position, or a miracle's own (a
/// spell has no Transform)
glm::ivec2 ObjectCoords(entt::entity object)
{
	auto& registry = Entities();
	if (!registry.AllOf<Transform>(object))
	{
		if (const auto* spell = registry.TryGet<const components::Spell>(object); spell != nullptr)
		{
			return {map_coords::ToFixed(spell->position.x), map_coords::ToFixed(spell->position.z)};
		}
	}
	return town_queries::PosOf(object);
}

/// The same as a point in the world, with its height: the ground there plus its altitude above it
glm::vec3 WorldPointOf(entt::entity entity)
{
	auto& registry = Entities();
	if (const auto* transform = registry.TryGet<const Transform>(entity); transform != nullptr)
	{
		return magic::ToWorld(magic::ToMap(transform->position));
	}
	if (const auto* spell = registry.TryGet<const components::Spell>(entity); spell != nullptr)
	{
		return magic::ToWorld(spell->position);
	}
	return glm::vec3(0.0f);
}

namespace living_rules = openblack::physics::living;

/// The physics body of a thing, none when it is not in the physics
const PhysicsEntry* PhysicsEntryOf(entt::entity object)
{
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(object) : nullptr;
}

/// The way a reaction's object is going: a creature's ahead of its body, in the physics too; a miracle's last movement;
/// a thing in the physics, its body's velocity (none without a body); an animal along its last step while it walks;
/// nothing for anything else. (pending) a villager that walks: no reaction a villager runs from has one as its object
glm::vec3 MovementOf(entt::entity object)
{
	if (creature_reaction_facts::IsCreature(object))
	{
		return creature_reaction_facts::MovementOf(object);
	}
	if (const auto* spell = Entities().TryGet<const components::Spell>(object); spell != nullptr)
	{
		return spell->movementDirection;
	}
	if (Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(object))
	{
		const auto* entry = PhysicsEntryOf(object);
		return entry != nullptr ? entry->body.velocity : glm::vec3(0.0f);
	}
	if (Entities().AllOf<components::Animal>(object))
	{
		return animal_ai::MovementDirection(object);
	}
	return glm::vec3(0.0f);
}

/// The land balance's entry for impressiveness
constexpr size_t k_ImpressiveBalance = 2;
/// The land balance a missionary's impressiveness is multiplied by
constexpr size_t k_MissionaryBalance = 3;
/// The land balance an object's impressiveness is multiplied by
constexpr size_t k_ObjectImpressiveBalance = 1;

/// How much an object's reaction impresses a villager of `town`: the object's own value from its info by the land's
/// balance, the reaction's multiplier, the distance's share, the town's weariness and the artefact modifier
float ObjectImpressiveValue(entt::entity villager, entt::entity town, const reactions::Reaction& reaction,
                            const ReactionInfo& info)
{
	std::optional<float> desire;
	if (info.correspondingTownDesire != TownDesireInfo::None)
	{
		desire = town_desire::GetDesire(town, info.correspondingTownDesire);
	}
	const auto* objectInfo = fire::traits::InfoOf(reaction.initiator);
	const float artefact =
	    Locator::artefactSystem::has_value() ? Locator::artefactSystem::value().ImpressiveModifier(reaction.initiator) : 1.0f;
	return magic::ObjectImpressiveValue({
	    .landBalance = land_balance::Get(k_ObjectImpressiveBalance),
	    .impressiveValue = objectInfo != nullptr ? objectInfo->impressiveValue : 0.0f,
	    .reactionMultiplier = magic::ReactionMultiplier(reaction.type, info.defaultReactionImpressiveMultiplier, desire),
	    .distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(reaction.initiator)),
	    .maxDistance = info.maxReactionDistance,
	    .power = 1.0f,
	    .boredom = town_belief::GetBoredomMultiplier(town, static_cast<size_t>(reaction.type)),
	    .artefactModifier = artefact,
	});
}

/// How much a scaffold's reaction impresses a villager of `town`: how many scaffolds it stands for, by its value as an
/// object, by what its site's town wants of the building
float ScaffoldImpressiveValue(entt::entity villager, entt::entity town, const reactions::Reaction& reaction,
                              const ReactionInfo& info)
{
	return magic::ScaffoldImpressiveValue(scaffolds::GetValue(reaction.initiator),
	                                      ObjectImpressiveValue(villager, town, reaction, info),
	                                      villager_reactions::ScaffoldSiteWant(reaction.initiator));
}

/// How much a creature's reaction impresses a villager of `town`: its size share, the town's weariness, the distance's
/// share, what it last carried out to its end by the reaction's multiplier (the town rules' one-off belief while the
/// town has not judged it yet), by the land's balance
float CreatureImpressiveValue(entt::entity villager, entt::entity town, const reactions::Reaction& reaction,
                              const ReactionInfo& info)
{
	const auto& registry = Entities();
	const auto& infos = Locator::infoConstants::value();
	const auto creature = reaction.initiator;
	const auto& body = registry.Get<const components::Creature>(creature);
	const auto row = creature::InfoRow(body.species);
	const auto* species = row < infos.creature.size() ? &infos.creature.at(row) : nullptr;
	const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
	const auto action = mind != nullptr ? mind->lastCompletedAction : 0;
	const float actionValue = action < infos.creatureAction.size() ? infos.creatureAction.at(action).impressiveValue : 0.0f;
	std::optional<float> desire;
	if (info.correspondingTownDesire != TownDesireInfo::None)
	{
		desire = town_desire::GetDesire(town, info.correspondingTownDesire);
	}
	float done = magic::ReactionMultiplier(reaction.type, info.defaultReactionImpressiveMultiplier, desire) * actionValue;
	if (town_creature_attitude::AttitudeOf(town, creature) == town_creature_attitude::k_New)
	{
		done = infos.town.oneOffBeliefForCreature;
	}
	return magic::CreatureImpressiveValue({
	    .drawnSize = creature_pose::DrawnSize(registry, creature),
	    .sizeAddition = species != nullptr ? species->impressiveSizeAddition : 0.0f,
	    .sizeMultiplier = species != nullptr ? species->impressiveSizeMultiplier : 0.0f,
	    .boredom = town_belief::GetBoredomMultiplier(town, static_cast<size_t>(reaction.type)),
	    .power = 1.0f,
	    .distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(creature)),
	    .maxDistance = info.maxReactionDistance,
	    .done = done,
	    .landBalance = land_balance::Get(k_ObjectImpressiveBalance),
	});
}

/// How much a reaction's miracle impresses a villager of `town` (the miracle's own value; a shield's overrides)
float MiracleImpressiveValue(entt::entity villager, entt::entity town, const reactions::Reaction& reaction,
                             const components::Spell& spell, const ReactionInfo& info)
{
	// the reaction's multiplier: for food and wood the town's want of what it shows, else the table's
	std::optional<float> desire;
	if (info.correspondingTownDesire != TownDesireInfo::None)
	{
		desire = town_desire::GetDesire(town, info.correspondingTownDesire);
	}
	const float multiplier = magic::ReactionMultiplier(reaction.type, info.defaultReactionImpressiveMultiplier, desire);
	// the distance in the plane from the villager to the miracle where it is now
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(reaction.initiator));
	const float value = magic::ImpressiveValue({
	    .landBalance = land_balance::Get(k_ImpressiveBalance),
	    .impressiveValue = magic::EffectInfoOf(reaction.initiator).impressiveValue,
	    .reactionMultiplier = multiplier,
	    .distance = distance,
	    .maxDistance = info.maxReactionDistance,
	    .power = 1.0f,
	    .boredom = town_belief::GetBoredomMultiplier(town, static_cast<size_t>(reaction.type)),
	});
	if (spell.spellClass != SpellClass::Shield)
	{
		return value;
	}
	// a shield: whether the town's last attacker is the shield's player. (approximate) a town that has never been
	// attacked keeps turn 0 and no attacker; an attack in turn 0 itself would read as none
	const auto& record = Entities().Get<const Town>(town).aggression;
	const bool attackedByShieldPlayer = record.lastTurn != 0 && record.lastAggressor == spell.player;
	return magic::ShieldImpressiveValue(reaction.type, attackedByShieldPlayer, value);
}

/// How much a missionary's reaction impresses the villager: the land's fourth balance by the missionary's own value by
/// the reaction's multiplier, then the boredom by the distance's share
float MissionaryValue(entt::entity villager, entt::entity town, const reactions::Reaction& reaction,
                      const components::MissionaryControl& control, const ReactionInfo& info)
{
	std::optional<float> desire;
	if (info.correspondingTownDesire != TownDesireInfo::None)
	{
		desire = town_desire::GetDesire(town, info.correspondingTownDesire);
	}
	const float multiplier = magic::ReactionMultiplier(reaction.type, info.defaultReactionImpressiveMultiplier, desire);
	const float own =
	    Entities().AllOf<Villager>(control.missionary) ? villager::InfoOf(control.missionary).missionaryImpressiveValue : 0.0f;
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(reaction.initiator));
	return villager::MissionaryImpressiveValue(land_balance::Get(k_MissionaryBalance), own, multiplier,
	                                           gutils::DistanceChangeToBelief(distance, info.maxReactionDistance),
	                                           town_belief::GetBoredomMultiplier(town, static_cast<size_t>(reaction.type)));
}

void SetupFleeFromObject(entt::entity villager, entt::entity object, uint32_t reaction)
{
	// it runs, and keeps what it runs from
	villager_reactions::AddReaction(villager, reaction, VillagerStates::FleeingFromObjectReaction);
	villager_reactions::SetSlotObject(villager, object);
}

void SetupLookAtObject(entt::entity villager, entt::entity object, uint32_t reaction)
{
	// it stops to watch, and keeps what it watches
	villager_reactions::AddReaction(villager, reaction, VillagerStates::LookingAtObjectReaction);
	villager_reactions::SetSlotObject(villager, object);
}

void SetupFleeFromPredator(entt::entity villager, entt::entity predator, uint32_t reaction)
{
	// On its way to hide already, it only counts its turns inside afresh
	if (!effects::flee::VillagerStartsFleeingFromPredator(villager::GetFinalState(villager)))
	{
		if (auto* hidingIn = Entities().TryGet<components::VillagerHiding>(villager); hidingIn != nullptr)
		{
			hidingIn->turns = 0;
		}
		return;
	}
	// else it starts running from it, and keeps it
	villager_reactions::AddReaction(villager, reaction, VillagerStates::FleeingFromPredatorReaction);
	villager_reactions::SetSlotObject(villager, predator);
}

/// A predator's speed in metres a second and whether it is coming at the villager: none for anything not an animal
struct PredatorMotion
{
	bool animal {false};
	float speed {0.0f};
	bool comingAt {false};
};
PredatorMotion MotionOf(entt::entity villager, entt::entity predator)
{
	if (!Entities().AllOf<components::Animal>(predator))
	{
		return {};
	}
	return {.animal = true,
	        .speed = animal_ai::SpeedInMetresPerSecond(predator),
	        .comingAt = effects::flee::ComingTowards(WorldPointOf(villager), WorldPointOf(predator), MovementOf(predator))};
}

/// Whether a villager runs from a thrown thing or points at it, from how far it is across the map and how fast it flies
VillagerStates FlyingObjectState(entt::entity villager, entt::entity object, const PhysicsEntry& entry)
{
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(object));
	const float speed = living_rules::FlyingSpeed(entry.body.velocity);
	return living_rules::RespondToFlyingObject(distance, speed) == living_rules::FlyingObjectResponse::Run
	           ? VillagerStates::FleeingFromObjectReaction
	           : VillagerStates::PointAtFlyingObjectReaction;
}

void SetupReactToFlyingObject(entt::entity villager, entt::entity object, uint32_t reaction)
{
	// a thing with no physics body: nothing
	const auto* entry = PhysicsEntryOf(object);
	if (entry == nullptr)
	{
		return;
	}
	// it runs (6) or points (162; the state's clip is chosen as it enters it), and keeps the thing
	const auto state = FlyingObjectState(villager, object, *entry);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("flying object {}: speed {:.3f} -> {}", static_cast<uint32_t>(object),
		                                      living_rules::FlyingSpeed(entry->body.velocity), static_cast<int>(state)));
	}
	villager_reactions::AddReaction(villager, reaction, state);
	villager_reactions::SetSlotObject(villager, object);
}

/// The reaction a villager follows with its type, its object, the object's place and the distance to it
struct Followed
{
	openblack::Reaction type {openblack::Reaction::None};
	entt::entity object {entt::null};
	glm::ivec2 at {0};
	float distance {0.0f};
};

/// Nothing when the object is gone or no longer available, or the reaction is gone
std::optional<Followed> FollowedBy(entt::entity villager)
{
	const auto object = ReactionObjectOf(villager);
	if (object == entt::null || !fire::traits::IsAvailable(object))
	{
		return std::nullopt;
	}
	const auto current = CurrentOf(villager);
	if (reactions::Find(current.id) == nullptr)
	{
		return std::nullopt;
	}
	const auto at = ObjectCoords(object);
	return Followed {current.type, object, at, town_queries::GetDistanceInMetres(town_queries::PosOf(villager), at)};
}

/// The miracle maps: one of them is held
bool IsReactingToMiracle(entt::entity villager)
{
	return villager_fire::IsReacting(villager) || villager_teleport::IsReacting(villager) ||
	       villager_shield::IsReacting(villager) || villager_mourning::IsReacting(villager);
}

/// The priority function of a type for a villager: the slot table's, the miracle functions' (the original's
/// arguments: ReactToFirePriority is the only one that reads `other`), 0 for the others
uint8_t PriorityOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, uint32_t other)
{
	if (const auto* row = villager_reactions::SlotTypeOf(type); row != nullptr)
	{
		return row->priority(villager, reaction, other);
	}
	switch (type)
	{
	case openblack::Reaction::ReactToFire:
		return villager_fire::ReactToFirePriority(villager, reaction, other);
	case openblack::Reaction::ReactToTeleport:
		return villager_teleport::ReactToTeleportPriority(villager, reaction);
	case openblack::Reaction::ReactToMagicShield:
		return villager_shield::ReactToMagicShieldPriority(villager, reaction);
	case openblack::Reaction::ReactToDeath:
		return villager_mourning::ReactToDeathPriority(villager, reaction);
	default:
		return 0;
	}
}

/// The switch from the reaction a villager follows (`cur`) to `reaction` at distance d. (approximate) a current miracle
/// reaction no longer in the list scores 0 (the original's shut-down would have stopped the villager first)
bool SwitchAllowed(entt::entity villager, const Current& cur, const reactions::Reaction& reaction, float d)
{
	const auto* current = reactions::Find(cur.id);
	// the current one's distance to its cell; its score with the new one as `other`
	const float dc = current != nullptr ? reactions::DistanceToReactionCell(villager, *current) : 0.0f;
	const float curScore = current != nullptr
	                           ? static_cast<float>(villager_reactions::ScoreOf(villager, cur.id, cur.type, dc, reaction.id))
	                           : 0.0f;
	// the new one's
	const auto newScore = static_cast<float>(villager_reactions::ScoreOf(villager, reaction.id, reaction.type, d, 0));
	// the same type: only with the table's sameTypeSwitch, and never to the reaction it follows
	if (reaction.type == cur.type)
	{
		if (!reactions::SameTypeSwitch(TypeByte(reaction.type)) || cur.id == reaction.id)
		{
			return false;
		}
	}
	// (now - RecordTurn(current type)) / (1000 / ms per turn), both unsigned divisions. (openblack, guard) never a
	// division by 0
	const uint32_t perSecond = std::max<uint32_t>(1000u / std::max<uint32_t>(game_clock::MsPerTurn(), 1u), 1u);
	const uint32_t seconds = (villager::CurrentTurn() - reactions::RecordTurn(villager, TypeByte(cur.type))) / perSecond;
	// the seconds as a float
	return reactions::MaySwitch(curScore, newScore, static_cast<float>(seconds), TypeByte(cur.type));
}

/// The body that applies a reaction to one villager, for a slot type
void ApplySlotReaction(entt::entity villager, const reactions::Reaction& reaction, float d)
{
	auto& registry = Entities();
	const auto* row = villager_reactions::SlotTypeOf(reaction.type);
	const auto t = TypeByte(reaction.type);
	// a Living, available for the reaction; the villager that started it is not turned away
	if (row == nullptr || !registry.AllOf<Villager, LivingAction>(villager) ||
	    !villager::IsAvailableForReaction(villager, reaction.type))
	{
		return;
	}
	// again = the table's turnsBeforeAgain (initiator, t, d)
	const auto again = row->turnsBeforeAgain(villager, reaction.initiator, t, d);
	// a villager only impressed by it has been taken by ImpressOnly before this
	const auto now = villager::CurrentTurn();
	const auto cur = CurrentOf(villager);
	if (!cur.Any())
	{
		// Score(r)(me, d, 0) > 0 and Records(t, again)
		const auto score = villager_reactions::ScoreOf(villager, reaction.id, reaction.type, d, 0);
		if (score == 0 || !reactions::Records(villager, t, again, now))
		{
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager,
				                fmt::format("react {}: {} score {} -> skip ({})", t, static_cast<uint32_t>(reaction.initiator),
				                            score, score == 0 ? "score" : "again"));
			}
			return;
		}
		// the reaction's start turn = now if not set; then starting to react is the table's setup
		reactions::MarkStarted(reaction.id, now);
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager,
			                fmt::format("react {}: {} score {} -> setup", t, static_cast<uint32_t>(reaction.initiator), score));
		}
		row->setup(villager, reaction.initiator, reaction.id);
		return;
	}
	if (!SwitchAllowed(villager, cur, reaction, d))
	{
		return;
	}
	// SetReactionDoneWhen(t); out of the current reaction's followers (AddReaction drops the slot or the miracle map);
	// then the setup
	reactions::SetReactionDoneWhen(villager, t, now);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("react {}: {} switch from {} -> setup", t,
		                                      static_cast<uint32_t>(reaction.initiator), static_cast<int>(cur.type)));
	}
	row->setup(villager, reaction.initiator, reaction.id);
}

/// The dispatch of the miracle types, as before the slot
void DispatchMiracleReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	switch (reaction.type)
	{
	case openblack::Reaction::ReactToFire:
		villager_fire::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToTeleport:
		villager_teleport::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToMagicShield:
		villager_shield::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToDeath:
		// ECS/Villager/VillagerMourning.h
		villager_mourning::ApplyReaction(villager, reaction);
		break;
	default:
		break;
	}
}

/// How fast a thing goes, in metres a second: a walker's (a villager's or an animal's) speed a turn in metres times
/// ten (ten turns a second, villager_reactions::WalkerSpeedInMetresPerSecond); 0 for anything else
float SpeedInMetresPerSecond(entt::entity thing)
{
	auto& registry = Entities();
	if (!registry.Valid(thing))
	{
		return 0.0f;
	}
	if (registry.AllOf<components::Animal>(thing))
	{
		return villager_reactions::WalkerSpeedInMetresPerSecond(animal_ai::GetSpeedInMetres(thing));
	}
	if (registry.AllOf<Villager>(thing))
	{
		return villager_reactions::WalkerSpeedInMetresPerSecond(villager::SpeedInMetres(thing));
	}
	return 0.0f;
}

/// The type table's turns before a villager reacts to a type again, at the spread's distance d: a slot type's own, the
/// shield's, the burning thing in the hand's, else the standard ones
uint32_t TurnsBeforeReactingAgain(entt::entity villager, const reactions::Reaction& reaction, float d)
{
	const auto t = TypeByte(reaction.type);
	if (const auto* row = villager_reactions::SlotTypeOf(reaction.type); row != nullptr)
	{
		return row->turnsBeforeAgain(villager, reaction.initiator, t, d);
	}
	switch (reaction.type)
	{
	case openblack::Reaction::ReactToMagicShield:
		return villager_shield::TurnsBeforeReactingToShieldAgain(villager, reaction.initiator, d);
	case openblack::Reaction::ReactToBurningObjectInHand:
	{
		// its own distance to the thing, not the spread's
		const float own = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(reaction.initiator));
		const auto& row = Locator::infoConstants::value().reaction.at(t);
		return villager_reactions::BurningObjectTurnsBeforeReactingAgain(own, row.minDistanceToRunAwayFromObject,
		                                                                 SpeedInMetresPerSecond(reaction.initiator),
		                                                                 reactions::StandardTurnsBeforeReactingAgain(t, d));
	}
	default:
		return reactions::StandardTurnsBeforeReactingAgain(t, d);
	}
}

/// A villager only impressed by a reaction: one of the reactions that only impress (the watchers of a fire put out by
/// water or a storm, a missionary, a fight won), or any reaction reaching a villager that cannot be reached (at home,
/// held back from a change of state, hiding in a building). Available for the reaction, it is impressed once per the
/// type's turns before reacting again and takes up nothing, and it ends the cell's walk, impressed or not. Nothing
/// (std::nullopt) when the reaction takes its own course with this villager
std::optional<reactions::CellWalk> ImpressOnly(entt::entity villager, const reactions::Reaction& reaction, float d)
{
	auto& registry = Entities();
	const auto t = TypeByte(reaction.type);
	const bool beliefOnly = reactions::BeliefButNotReaction(t);
	if (!registry.AllOf<Villager, LivingAction>(villager) || !villager::IsAvailableForReaction(villager, reaction.type))
	{
		// not available: a reaction that only impresses goes on to the next Living, any other keeps its own course
		if (beliefOnly)
		{
			return reactions::CellWalk::Next;
		}
		return std::nullopt;
	}
	if (!villager_reactions::IsAvailableForBeliefButNotReaction(beliefOnly, villager::IsReachable(villager)))
	{
		return std::nullopt;
	}
	const auto again = TurnsBeforeReactingAgain(villager, reaction, d);
	const bool recorded = reactions::Records(villager, t, again, villager::CurrentTurn());
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("react {}: {} -> {}", t, static_cast<uint32_t>(reaction.initiator),
		                                      recorded ? "impressed only" : "impressed only (again)"));
	}
	if (recorded)
	{
		villager_reactions::UpdateHowImpressed(villager, reaction.id, true);
	}
	return reactions::CellWalk::Leave;
}

reactions::CellWalk VillagerReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float distance)
{
	// a reaction that only impresses, or a villager that cannot be reached: it takes nothing up
	if (const auto walk = ImpressOnly(villager, reaction, distance); walk.has_value())
	{
		return *walk;
	}
	// a slot type: the original's per-villager body
	if (villager_reactions::SlotTypeOf(reaction.type) != nullptr)
	{
		ApplySlotReaction(villager, reaction, distance);
		return reactions::CellWalk::Next;
	}
	// the only path before the slot: no slot reaction held -> the miracle dispatch, unchanged
	if (!villager_reactions::SlotHeld(villager))
	{
		DispatchMiracleReaction(villager, reaction);
		return reactions::CellWalk::Next;
	}
	// a miracle type reaches a villager that follows a slot reaction: first IsAvailableForReaction(type) (whole: the
	// life test lets only type 7 through), as for every type in the original, then the switch rule with the slot as
	// the current one (the miracle types are not in the villager type table yet: their score and setup are theirs as
	// today)
	if (!villager::IsAvailableForReaction(villager, reaction.type))
	{
		return reactions::CellWalk::Next;
	}
	const auto cur = CurrentOf(villager);
	if (!SwitchAllowed(villager, cur, reaction, distance))
	{
		return reactions::CellWalk::Next;
	}
	// the slot is unlinked (no record refresh, as the original's switch) and the miracle ApplyReaction runs with its own
	// gates and setup; it took the villager -> SetReactionDoneWhen (after their Records: idempotent), else the slot is
	// put back
	auto& registry = Entities();
	const auto saved = registry.Get<const LivingReaction>(villager);
	registry.Remove<LivingReaction>(villager);
	DispatchMiracleReaction(villager, reaction);
	if (IsReactingToMiracle(villager))
	{
		reactions::SetReactionDoneWhen(villager, TypeByte(reaction.type), villager::CurrentTurn());
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("react {}: switch from slot {} -> {}", static_cast<int>(saved.type),
			                                      saved.reaction, static_cast<int>(reaction.type)));
		}
	}
	else if (registry.Valid(villager))
	{
		registry.AssignOrReplace<LivingReaction>(villager, saved);
	}
	return reactions::CellWalk::Next;
}

/// The reaction's shut-down for the villagers: while the reaction has followers, the head one's (the newest)
/// StopReactingAndSetState. Only the slot holders keep the follower link (the miracle maps: unchanged)
void ShutDownSlotReaction(uint32_t reaction)
{
	auto& registry = Entities();
	std::vector<std::pair<uint32_t, entt::entity>> followers;
	registry.Each<const LivingReaction>([&followers, reaction](entt::entity villager, const LivingReaction& slot) {
		if (slot.reaction == reaction)
		{
			followers.emplace_back(slot.joinOrder, villager);
		}
	});
	std::sort(followers.begin(), followers.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& [order, villager] : followers)
	{
		// one StopReactingAndSetState may move another follower off it: only the ones still following
		const auto* slot = registry.Valid(villager) ? registry.TryGet<const LivingReaction>(villager) : nullptr;
		if (slot == nullptr || slot->reaction != reaction)
		{
			continue;
		}
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager,
			                fmt::format("react {}: shut down {} (follower {})", static_cast<int>(slot->type), reaction, order));
		}
		villager_reactions::StopReactingAndSetState(villager);
		// (openblack, guard) the original loops until the list is empty: StopReacting always unlinks
		if (registry.Valid(villager))
		{
			if (const auto* still = registry.TryGet<const LivingReaction>(villager);
			    still != nullptr && still->reaction == reaction)
			{
				registry.Remove<LivingReaction>(villager);
			}
		}
	}
}
} // namespace

void villager_reactions::RegisterHandlers()
{
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
	effects::reactions::SetLivingShutDownHandler(effects::reactions::LivingClass::Villager, &ShutDownSlotReaction);
}

void villager_reactions::Register()
{
	RegisterHandlers();
	Locator::reactionSystem::value().ResetJoinOrder();
}

effects::reactions::CellWalk villager_reactions::HandleReaction(entt::entity villager,
                                                                const effects::reactions::Reaction& reaction, float distance)
{
	return VillagerReaction(villager, reaction, distance);
}

bool villager_reactions::IsAvailableForBeliefButNotReaction(bool beliefButNotReaction, bool reachable)
{
	return beliefButNotReaction || !reachable;
}

float villager_reactions::WalkerSpeedInMetresPerSecond(float metresPerTurn)
{
	constexpr float k_TurnsPerSecond = 10.0f;
	return metresPerTurn * k_TurnsPerSecond;
}

uint32_t villager_reactions::BurningObjectTurnsBeforeReactingAgain(float distance, float minDistanceToRunAway, float speed,
                                                                   uint32_t standard)
{
	constexpr float k_BurningObjectStillSpeed = 3.0f;
	return distance >= minDistanceToRunAway && speed <= k_BurningObjectStillSpeed ? standard : 0u;
}

const villager_reactions::SlotType* villager_reactions::SlotTypeOf(openblack::Reaction type)
{
	switch (type)
	{
	case openblack::Reaction::ReactToFood:
		return &k_ReactToFood;
	case openblack::Reaction::ReactToWood:
		return &k_ReactToWood;
	case openblack::Reaction::FleeFromObject:
		return &k_FleeFromObject;
	case openblack::Reaction::FleeFromSpell:
		return &k_FleeFromSpell;
	case openblack::Reaction::LookAtNiceSpell:
	case openblack::Reaction::ReactToImpressiveSpell:
		return &k_LookAtNiceSpell;
	case openblack::Reaction::ReactToFlyingObject:
		return &k_ReactToFlyingObject;
	case openblack::Reaction::ReactToMagicShieldDestroyed:
		return &k_ReactToMagicShieldDestroyed;
	case openblack::Reaction::ReactToBreeder:
		return &k_ReactToBreeder;
	case openblack::Reaction::FleeFromPredator:
		return &k_FleeFromPredator;
	case openblack::Reaction::ReactToCreature:
		return &k_ReactToCreature;
	default:
		return nullptr;
	}
}

villager_reactions::FollowedReaction villager_reactions::FollowedReactionOf(entt::entity villager)
{
	return {.object = ReactionObjectOf(villager), .kind = CurrentOf(villager).type};
}

bool villager_reactions::SlotHeld(entt::entity villager)
{
	return SlotReaction(villager) != 0;
}

uint32_t villager_reactions::SlotReaction(entt::entity villager)
{
	const auto* slot = Entities().TryGet<const LivingReaction>(villager);
	return slot != nullptr ? slot->reaction : 0;
}

entt::entity villager_reactions::SlotObject(entt::entity villager)
{
	const auto* slot = Entities().TryGet<const LivingReaction>(villager);
	return slot != nullptr ? slot->object : entt::entity(entt::null);
}

void villager_reactions::SetSlotObject(entt::entity villager, entt::entity object)
{
	if (auto* slot = Entities().TryGet<LivingReaction>(villager); slot != nullptr)
	{
		slot->object = object;
	}
}

uint32_t villager_reactions::ScoreOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, float distance,
                                     uint32_t other)
{
	const auto t = TypeByte(type);
	const auto& infos = Locator::infoConstants::value();
	if (t >= infos.reaction.size())
	{
		return 0;
	}
	// isReacting[t] of the villager's info and d <= maxReactionDistance: only then the priority function
	const auto& info = villager::InfoOf(villager);
	const bool reacts = ReactsTo(info.isReacting, t);
	if (!reacts || distance > infos.reaction.at(t).maxReactionDistance)
	{
		return 0;
	}
	return reactions::Score(t, true, PriorityOf(villager, reaction, type, other), distance);
}

uint8_t villager_reactions::FleeFromSpellPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	// the table's priority for running from a miracle, whatever the reaction's own type
	const auto priority = Locator::infoConstants::value().reaction.at(TypeByte(openblack::Reaction::FleeFromSpell)).priority;
	const auto* found = reactions::Find(reaction);
	// (openblack, guard) a reaction always has its initiator here
	if (found == nullptr || !Entities().Valid(found->initiator))
	{
		return 0;
	}
	// more urgent the nearer it is, by the fast distance from the villager to the miracle
	const auto me = town_queries::PosOf(villager);
	const auto at = ObjectCoords(found->initiator);
	const auto fast = gutils::FastDistance({me.x, me.y, 0.0f}, {at.x, at.y, 0.0f});
	return effects::flee::FleeFromSpellPriority(priority, fast);
}

uint8_t villager_reactions::LookAtNiceSpellPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	// a villager does not stop to watch a miracle it made itself
	if (const auto* found = reactions::Find(reaction); found != nullptr && Entities().Valid(found->initiator))
	{
		if (const auto* spell = Entities().TryGet<const components::Spell>(found->initiator);
		    spell != nullptr && spell->creator.entity == villager)
		{
			return 0;
		}
	}
	// the table's priority for a nice miracle, for the impressive ones too
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(openblack::Reaction::LookAtNiceSpell));
	return static_cast<uint8_t>(info.priority);
}

uint32_t villager_reactions::Fleeing(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// the object or the reaction gone: back to what it was doing
	const auto followed = FollowedBy(villager);
	if (!followed.has_value())
	{
		StopReactingAndSetState(villager);
		return 1;
	}
	// far enough to be safe: it gives up
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(followed->type));
	if (!(followed->distance <= info.maxDistanceToRunAwayFromObject))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("flee: {:.3f} m -> gave up", followed->distance));
		}
		StopReactingAndSetState(villager);
		return 1;
	}
	// beyond the near distance and not coming at it it turns and watches it, else it runs, and watches from there
	FleeFromObjectIfComingTowardsMe(villager, followed->object, followed->type,
	                                VillagerStates::FleeingAndLookingAtObjectReaction,
	                                VillagerStates::FleeingAndLookingAtObjectReaction);
	return 1;
}

void villager_reactions::FleeFromObjectIfComingTowardsMe(entt::entity villager, entt::entity object, openblack::Reaction type,
                                                         VillagerStates lookState, VillagerStates fleeState)
{
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(type));
	const auto at = ObjectCoords(object);
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), at);
	// beyond the near distance and not coming at it: it turns and watches it
	const auto movement = MovementOf(object);
	const bool coming = effects::flee::ComingTowards(WorldPointOf(villager), WorldPointOf(object), movement);
	if (!(distance <= info.minDistanceToRunAwayFromObject) && !coming)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("flee: {:.3f} m, not coming -> watch", distance));
		}
		SetTopState(villager, lookState);
		villager::LookAtPos(villager, at, 1);
		return;
	}
	// else it runs ten metres: across the way of a moving thing (two draws, x first), straight away from a still one
	const auto me = town_queries::ToMetres(town_queries::PosOf(villager));
	const auto from = town_queries::ToMetres(at);
	glm::vec2 goal;
	if (glm::length(movement) > 0.0f)
	{
		const float randomX = villager::GameFloatRand(effects::flee::k_FleeJitter);
		const float randomZ = villager::GameFloatRand(effects::flee::k_FleeJitter);
		const glm::vec2 way(movement.x, movement.z);
		goal = effects::flee::FleeingPositionFromMoving(me, from, way, effects::flee::k_FleeStep, randomX, randomZ);
	}
	else
	{
		goal = effects::flee::FleeingPositionFromStill(me, from, effects::flee::k_FleeStep);
	}
	// a point off the map (its cell against the map's size, not the land's): it stays
	if (map_coords::InBounds(map_coords::FromMetres(goal)))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("flee: run to {:.3f}, {:.3f}", goal.x, goal.y));
		}
		villager::SetupMoveToWithHug(villager, goal, fleeState);
	}
}

uint32_t villager_reactions::Watching(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// the object or the reaction gone: back to what it was doing
	const auto followed = FollowedBy(villager);
	if (!followed.has_value())
	{
		StopReactingAndSetState(villager);
		return 1;
	}
	// too far to see it: it gives up
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(followed->type));
	if (!(followed->distance <= info.maxDistanceToRunAwayFromObject))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("watch: {:.3f} m -> gave up", followed->distance));
		}
		StopReactingAndSetState(villager);
		return 1;
	}
	// it turns to face it, a sixteenth of a turn a turn at most
	villager::LookAtPos(villager, followed->at, 1);
	return 1;
}

uint8_t villager_reactions::ReactToMagicShieldDestroyedPriority([[maybe_unused]] entt::entity villager,
                                                                [[maybe_unused]] uint32_t reaction,
                                                                [[maybe_unused]] uint32_t other)
{
	const auto& row =
	    Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToMagicShieldDestroyed));
	return static_cast<uint8_t>(row.priority & 0xFFu);
}

uint8_t villager_reactions::FleeFromPredatorPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* found = reactions::Find(reaction);
	// (openblack, guard) a reaction always has its initiator here
	if (found == nullptr || !Entities().Valid(found->initiator))
	{
		return 0;
	}
	const auto predator = found->initiator;
	const auto motion = MotionOf(villager, predator);
	effects::flee::VillagerPredatorFacts facts {.initiatorIsAnimal = motion.animal};
	if (motion.animal)
	{
		const auto& row = Locator::infoConstants::value().reaction.at(TypeByte(openblack::Reaction::FleeFromPredator));
		const auto& action = Entities().Get<const LivingAction>(villager);
		facts.distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(predator));
		facts.minDistanceToRunAway = row.minDistanceToRunAwayFromObject;
		facts.atHome = villager::IsAtHome(villager);
		facts.topState = static_cast<VillagerStates>(action.states[static_cast<size_t>(LivingAction::Index::Top)]);
		facts.storedFinalState = static_cast<VillagerStates>(action.states[static_cast<size_t>(LivingAction::Index::Final)]);
		facts.finalState = villager::GetFinalState(villager);
		facts.turnsSinceStateChange = action.turnsSinceStateChange;
		facts.predatorTopState = static_cast<uint8_t>(animal_ai::TopState(predator));
		facts.turnReactionStarted = reactions::RecordTurn(villager, TypeByte(openblack::Reaction::FleeFromPredator));
		facts.predatorComingTowards = motion.comingAt;
		facts.predatorSpeed = motion.speed;
		facts.predatorWalkingSpeed = animal_ai::WalkingSpeed(predator);
		facts.priority = row.priority;
	}
	return effects::flee::VillagerFleeFromPredatorPriority(facts);
}

uint32_t villager_reactions::TurnsToReactToPredator(entt::entity villager, entt::entity object, uint8_t type, float distance)
{
	const auto motion = MotionOf(villager, object);
	const auto top = Entities().Get<const LivingAction>(villager).states[static_cast<size_t>(LivingAction::Index::Top)];
	const auto living = effects::flee::LivingTurnsToReactToPredator(motion.animal, top, motion.speed, motion.comingAt,
	                                                                reactions::StandardTurnsToReact(type, distance));
	return effects::flee::VillagerTurnsToReactToPredator(villager::GetFinalState(villager), living);
}

uint32_t villager_reactions::TurnsBeforeReactingAgainToPredator(entt::entity villager, entt::entity object, uint8_t type,
                                                                float distance)
{
	const auto motion = MotionOf(villager, object);
	const auto& row = Locator::infoConstants::value().reaction.at(type);
	// its own distance to the predator, not the reaction's spread distance
	const float own = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(object));
	const auto living = effects::flee::LivingTurnsBeforeReactingAgainToPredator(
	    motion.animal, motion.speed, own, row.minDistanceToRunAwayFromObject,
	    reactions::StandardTurnsBeforeReactingAgain(type, distance));
	return effects::flee::VillagerTurnsBeforeReactingAgainToPredator(villager::GetFinalState(villager), living);
}

uint32_t villager_reactions::FleeingFromPredator(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// the predator or the reaction gone: back to what it was doing
	const auto followed = FollowedBy(villager);
	if (!followed.has_value())
	{
		StopReactingAndSetState(villager);
		return 1;
	}
	// only an animal frightens it
	const auto motion = MotionOf(villager, followed->object);
	if (!motion.animal)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(followed->type));
	const bool lookingIfSafe = villager::GetFinalState(villager) == VillagerStates::LookToSeeIfItIsSafe;
	const bool placeThere = villager_cover::BuildingHiddenIn(villager) != entt::null;
	switch (effects::flee::VillagerPredatorFleeStep(followed->distance, info.maxDistanceToRunAwayFromObject, lookingIfSafe,
	                                                placeThere, motion.speed, motion.comingAt))
	{
	case effects::flee::PredatorFleeStep::Stop:
		StopReactingAndSetState(villager);
		return 1;
	case effects::flee::PredatorFleeStep::HideAgain:
		villager_cover::HideAgain(villager, false);
		return 1;
	case effects::flee::PredatorFleeStep::LookForHidingPlace:
		if (villager_cover::SetupGoAndHideInNearbyBuilding(villager, followed->object, followed->type))
		{
			return 1;
		}
		break;
	case effects::flee::PredatorFleeStep::Draw:
		// one in six hides, the draw made only here
		if (villager::GameRand(effects::flee::k_HideChanceRange) == 0 &&
		    villager_cover::SetupGoAndHideInNearbyBuilding(villager, followed->object, followed->type))
		{
			return 1;
		}
		break;
	}
	// else it runs from it if it is coming at it, or watches it
	FleeFromObjectIfComingTowardsMe(villager, followed->object, followed->type, VillagerStates::LookingAtObjectReaction,
	                                VillagerStates::LookingAtObjectReaction);
	return 1;
}

uint8_t villager_reactions::ReactToFlyingObjectPriority(entt::entity villager, uint32_t reaction,
                                                        [[maybe_unused]] uint32_t other)
{
	const auto* found = reactions::Find(reaction);
	// (openblack, guard) a reaction always has its initiator here
	if (found == nullptr || !Entities().Valid(found->initiator))
	{
		return 0;
	}
	// a dancing villager notices it only when it comes at it; a thing without a body or a place, never
	if (Entities().AllOf<Dancer>(villager))
	{
		const auto* entry = PhysicsEntryOf(found->initiator);
		const auto* object = Entities().TryGet<const Transform>(found->initiator);
		const auto* self = Entities().TryGet<const Transform>(villager);
		if (entry == nullptr || object == nullptr || self == nullptr ||
		    !living_rules::FlyingAt(self->position, object->position, entry->body.velocity))
		{
			return 0;
		}
	}
	// amazed by a magic shield: only a thing strictly inside the shield's radius, measured from where the shield's
	// reaction is
	if (const auto cur = CurrentOf(villager); cur.type == openblack::Reaction::ReactToMagicShield)
	{
		const auto shield = villager_shield::ReactionObject(villager);
		const auto* current = reactions::Find(cur.id);
		if (shield == entt::null || current == nullptr)
		{
			return 0;
		}
		const auto* spell = Entities().TryGet<const components::Spell>(shield);
		const float radius = spell != nullptr ? spell->magnitude : 0.0f;
		const float distance =
		    town_queries::GetDistanceInMetres(ObjectCoords(found->initiator), ObjectCoords(current->initiator));
		if (!living_rules::ShelterSeesFlyer(distance, radius))
		{
			return 0;
		}
	}
	// not while it flies or lands itself; else the table's priority
	const auto top = villager::GetState(villager, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::Landed)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().reaction.at(TypeByte(openblack::Reaction::ReactToFlyingObject));
	return static_cast<uint8_t>(info.priority);
}

uint32_t villager_reactions::PointAtFlyingObject(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// the thing out of the physics: nothing changes
	const auto object = SlotObject(villager);
	const auto* entry = object != entt::null ? PhysicsEntryOf(object) : nullptr;
	const auto* reaction = reactions::Find(SlotReaction(villager));
	// (openblack, guard) the reaction is still there: ProcessReaction stops the villager before its state otherwise
	if (entry == nullptr || reaction == nullptr)
	{
		return 1;
	}
	// nearer to the reaction's thing than it flies in two seconds: it runs
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(reaction->initiator));
	const float speed = living_rules::FlyingSpeed(entry->body.velocity);
	if (living_rules::RespondToFlyingObject(distance, speed) == living_rules::FlyingObjectResponse::Run)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("point: {:.3f} m, speed {:.3f} -> run", distance, speed));
		}
		SetTopState(villager, VillagerStates::FleeingFromObjectReaction);
		return 1;
	}
	// else it turns to face the thing, and once that is no longer really in the air it goes back to what it was doing
	villager::LookAtPos(villager, ObjectCoords(object), 1);
	if (!ActuallyInTheAir(object))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "point: no longer in the air -> stop");
		}
		StopReactingAndSetState(villager);
	}
	return 1;
}

float villager_reactions::ScaffoldSiteWant(entt::entity scaffold)
{
	const auto& registry = Entities();
	const auto site = registry.Get<const components::Scaffold>(scaffold).site;
	if (site == entt::null)
	{
		return 1.0f;
	}
	const auto building = building_sites::GetBuilding(site);
	const bool abode = building != entt::null && registry.AllOf<components::Abode>(building);
	return town_desire::GetRawDesire(town_queries::GetTown(building),
	                                 abode ? TownDesireInfo::ForAbodes : TownDesireInfo::ForCivicBuilding);
}

void villager_reactions::UpdateHowImpressed(entt::entity villager, uint32_t reaction, bool draw)
{
	auto& registry = Entities();
	const auto* found = reactions::Find(reaction);
	if (found == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	// a reaction with no player, or a villager with no town, impresses nothing
	const auto* component = registry.TryGet<const Villager>(villager);
	if (!found->player.has_value() || component == nullptr || component->town == entt::null ||
	    !registry.Valid(component->town) || !registry.AllOf<Town>(component->town))
	{
		return;
	}
	const auto initiator = found->initiator;
	const auto* spell = registry.Valid(initiator) ? registry.TryGet<const components::Spell>(initiator) : nullptr;
	const auto* missionary =
	    registry.Valid(initiator) ? registry.TryGet<const components::MissionaryControl>(initiator) : nullptr;
	const auto& infos = Locator::infoConstants::value();
	const auto index = static_cast<size_t>(found->type);
	if (index >= infos.reaction.size())
	{
		return;
	}
	const auto& info = infos.reaction.at(index);
	const auto town = component->town;
	const auto player = *found->player;
	// the initiator's own value: a missionary's, a miracle's, a scaffold's, an object's; anything else (a town, a script
	// marker...) impresses by nothing, and the rest below still runs. Only a scaffold tires the town its own way
	float value = 0.0f;
	float boredomUpdate = infos.belief.defaultBoredomOfMe;
	if (missionary != nullptr)
	{
		value = MissionaryValue(villager, town, *found, *missionary, info);
	}
	else if (spell != nullptr)
	{
		value = MiracleImpressiveValue(villager, town, *found, *spell, info);
	}
	else if (registry.Valid(initiator) && registry.AllOf<components::Creature>(initiator))
	{
		value = CreatureImpressiveValue(villager, town, *found, info);
	}
	else if (registry.Valid(initiator) && registry.AllOf<components::Scaffold>(initiator))
	{
		value = ScaffoldImpressiveValue(villager, town, *found, info);
		boredomUpdate = magic::k_ScaffoldBoredomUpdate;
	}
	else if (registry.Valid(initiator) && script_type::IsObject(initiator) &&
	         !registry.AnyOf<components::ScriptMarker, components::Dance>(initiator))
	{
		value = ObjectImpressiveValue(villager, town, *found, info);
	}
	// the villager's share of its town's people
	const auto& stats = registry.Get<const Town>(town).stats;
	const float share = magic::TownShare(infos.town.populationForUnmodifiedBelief, stats.adults + stats.children);
	town_stores::AddToBelief(town, player, share * value, villager, draw, static_cast<int>(info.alignmentForSFX));
	// the player's alignment moves by the reaction's own amount, times the town's want when the reaction looks to one
	std::optional<float> want;
	if (info.correspondingTownDesireForAlignment != TownDesireInfo::None)
	{
		want = town_desire::GetRawDesire(town, info.correspondingTownDesireForAlignment);
	}
	effects::alignment::UpdateForImpression(player, magic::ImpressionAlignment(info.alignmentModifier, want));
	// the town tires of the kind of reaction, by the villager's share
	town_belief::AddToBoredomMultiplier(registry.Get<Town>(town).belief, index, boredomUpdate * share);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("impressed by {} type {}: {} x {} -> player {}", reaction, index, value, share,
		                                      static_cast<int>(player)));
	}
}

namespace
{
/// AddReaction, with the setup's object set before the state changes when `object` is given
void AddReactionWith(entt::entity villager, uint32_t reaction, VillagerStates state, std::optional<entt::entity> object)
{
	auto& registry = Entities();
	const auto* found = reactions::Find(reaction);
	const auto type = found != nullptr ? found->type : openblack::Reaction::None;
	// how impressed it is by r, the belief drawn above it
	villager_reactions::UpdateHowImpressed(villager, reaction, true);
	// no reaction yet -> StorePreviousState
	if (!villager_reactions::IsReacting(villager))
	{
		villager::StorePreviousState(villager);
	}
	// the object first, for a setup that sets it before the state changes: the object of the reaction it follows, if
	// any, becomes this one
	if (object.has_value())
	{
		if (auto* held = registry.TryGet<LivingReaction>(villager); held != nullptr)
		{
			held->object = *object;
		}
		else
		{
			registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.object = *object});
		}
	}
	// SetTopState(state): the exits run while the old reaction is still held
	villager_reactions::SetTopState(villager, state);
	// dancing -> out of the dance. TODO(dance): no villager dances in openblack
	if (!registry.Valid(villager))
	{
		return;
	}
	// the object set first stays, unless an exit stopped the villager reacting (which clears it)
	auto kept = entt::entity(entt::null);
	if (const auto* held = registry.TryGet<const LivingReaction>(villager); object.has_value() && held != nullptr)
	{
		kept = held->object;
	}
	// a switch from a miracle map: their StopReacting refreshes its record, the original's switch only unlinks: the
	// record's turn is put back
	if (IsReactingToMiracle(villager))
	{
		const auto old = CurrentOf(villager);
		const auto turn = reactions::RecordTurn(villager, TypeByte(old.type));
		villager_fire::StopReacting(villager);
		villager_teleport::StopReacting(villager);
		villager_shield::StopReacting(villager);
		villager_mourning::StopReacting(villager);
		reactions::RefreshRecord(villager, TypeByte(old.type), turn);
	}
	// the reaction is r; at the head of r's followers, one more follower
	auto& slot = registry.AssignOrReplace<LivingReaction>(villager);
	slot.reaction = reaction;
	slot.type = type;
	slot.object = kept;
	slot.joinOrder = Locator::reactionSystem::value().NextJoinOrder();
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager,
		                fmt::format("AddReaction {} type {} -> {}", reaction, static_cast<int>(type), static_cast<int>(state)));
	}
}
} // namespace

void villager_reactions::AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state)
{
	AddReactionWith(villager, reaction, state, std::nullopt);
}

void villager_reactions::AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state, entt::entity object)
{
	AddReactionWith(villager, reaction, state, object);
}

void villager_reactions::ProcessSlotReaction(entt::entity villager)
{
	auto& registry = Entities();
	const auto* slot = registry.TryGet<const LivingReaction>(villager);
	// no reaction -> nothing
	if (slot == nullptr || slot->reaction == 0)
	{
		return;
	}
	// the reaction not available -> StopReacting. (inferred) the reaction's own availability is
	// effects::reactions::IsAvailable (in the list, not shut down, initiator alive).
	// (approximate) the shut-down runs at BeginTurn's Prune, not when the initiator is deleted: an initiator destroyed
	// mid-turn reaches this test (StopReacting, no state reset) before its shut-down
	if (!reactions::IsAvailable(reactions::Find(slot->reaction)))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("ProcessReaction: reaction {} gone -> StopReacting", slot->reaction));
		}
		StopReacting(villager);
		return;
	}
	// the object none or not available -> StopReactingAndSetState
	const auto object = slot->object;
	if (object == entt::null || !fire::traits::IsAvailable(object))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "ProcessReaction: object gone -> StopReactingAndSetState");
		}
		StopReactingAndSetState(villager);
		return;
	}
	// elapsed = now - the record's turn of the type; the distance in metres from the villager to the object
	const auto t = TypeByte(slot->type);
	const auto* row = SlotTypeOf(slot->type);
	if (row == nullptr)
	{
		return;
	}
	const auto elapsed = static_cast<int32_t>(villager::CurrentTurn() - reactions::RecordTurn(villager, t));
	const float d = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), ObjectCoords(object));
	// the table's turnsToReact (object, type, d); a signed elapsed > turns -> stop
	const auto turns = static_cast<int32_t>(row->turnsToReact(villager, object, t, d));
	if (elapsed > turns)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("ProcessReaction: {} turns > {} -> StopReactingAndSetState", elapsed, turns));
		}
		StopReactingAndSetState(villager);
	}
}

uint32_t villager_reactions::SetTopState(entt::entity villager, VillagerStates state)
{
	// the walk a change leaves is kept, to go on with if the villager comes back to it
	return villager::SetTopState(villager, state);
}

void villager_reactions::PopFromPrevious(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	// the stored state's resume state (field0x20). Nothing stored is row 0, whose resume is 0 in info.dat:
	// SetTopState(0) = INVALID_STATE (which then returns 0 every turn), as the original
	const auto stored = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Previous)));
	const auto next = villager::state_info::ResumeState(villager::state_info::StateInfo(stored));
	// SetTopState; 0x2E (an exit refused) -> a raw store of 163 in index 0 (which also resets the turns in the state)
	const auto result = SetTopState(villager, next);
	action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	if (result == villager::k_ExitRefused)
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
		action->turnsSinceStateChange = 0;
	}
	// a raw store of 0 in index 2
	action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("PopFromPrevious stored {} -> resume {} = {:#x}", static_cast<int>(stored),
		                                      static_cast<int>(next), result));
	}
}

void villager_reactions::ResetStateAfterReacting(entt::entity villager)
{
	PopFromPrevious(villager);
	// the final state reactive (field0xb8) -> SetTopState(163)
	if (villager::state_info::IsReactive(villager::state_info::StateInfo(villager::GetFinalState(villager))))
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
}

void villager_reactions::StopReactingAndSetState(entt::entity villager)
{
	// ResetStateAfterReacting; still reacting -> StopReacting
	ResetStateAfterReacting(villager);
	if (IsReacting(villager))
	{
		StopReacting(villager);
	}
}

bool villager_reactions::ReactionValidate(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// the object and the type of the reaction it follows, kept by VillagerFire.cpp or VillagerTeleport.cpp
	auto object = entt::entity(entt::null);
	auto type = openblack::Reaction::None;
	if (villager_fire::ReactionObject(villager) != entt::null)
	{
		object = villager_fire::ReactionObject(villager);
		type = openblack::Reaction::ReactToFire;
	}
	else if (villager_teleport::ReactionObject(villager) != entt::null)
	{
		object = villager_teleport::ReactionObject(villager);
		type = openblack::Reaction::ReactToTeleport;
	}
	else if (villager_shield::ReactionObject(villager) != entt::null)
	{
		object = villager_shield::ReactionObject(villager);
		type = openblack::Reaction::ReactToMagicShield;
	}
	else if (SlotHeld(villager))
	{
		// the slot types (7 / 12), the last branch: with no slot the object stays none, as before
		object = SlotObject(villager);
		type = registry.Get<const LivingReaction>(villager).type;
	}
	// no object, or not available -> PopFromPrevious. The shield's object is
	// the spell itself, which is no fire object: its own availability test (and nothing holds a spell, so the in-hand
	// branch below cannot fire for it)
	if (type == openblack::Reaction::ReactToMagicShield)
	{
		const bool gone = !villager_shield::IsReactionObjectAvailable(villager);
		if (gone)
		{
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager, "ReactionValidate: the shield spell went -> PopFromPrevious");
			}
			PopFromPrevious(villager);
		}
		return !gone;
	}
	bool pop = object == entt::null || !fire::traits::IsAvailable(object);
	// ReactionInfo[type].whetherReactionFinishesIfInitiatorInHand and the object in the hand
	if (!pop)
	{
		const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
		pop = info.whetherReactionFinishesIfInitiatorInHand != 0 && fire::traits::InHand(object);
	}
	if (pop)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "ReactionValidate: the reaction's object went -> PopFromPrevious");
		}
		PopFromPrevious(villager);
	}
	return !pop;
}

bool villager_reactions::IsReacting(entt::entity villager)
{
	// the slot types' (components::LivingReaction; never held together with a miracle map)
	if (SlotHeld(villager))
	{
		return true;
	}
	// one reaction at a time: the fire's, the teleport's, the shield's or the mourning's (VillagerMourning.h)
	return villager_fire::IsReacting(villager) || villager_teleport::IsReacting(villager) ||
	       villager_shield::IsReacting(villager) || villager_mourning::IsReacting(villager);
}

void villager_reactions::StopReacting(entt::entity villager)
{
	// TOP 203 DANCE_WHILE_REACTING and dancing -> out of the dance. TODO(dance): 203 has no state function in
	// openblack, so no villager is there. Then the Living's StopReacting for the reaction it follows: the fire's, the
	// teleport's, the shield's or the mourning's (which also takes one off the reaction's follower count)
	if (villager::TraceOn(villager) && IsReacting(villager))
	{
		villager::Trace(villager, "StopReacting");
	}
	villager_fire::StopReacting(villager);
	villager_teleport::StopReacting(villager);
	villager_shield::StopReacting(villager);
	villager_mourning::StopReacting(villager);
	// the slot: out of the reaction's followers, its record of the type = now, the reaction cleared; the object is
	// cleared always
	auto& registry = Entities();
	if (const auto* slot = registry.TryGet<const LivingReaction>(villager); slot != nullptr)
	{
		if (slot->reaction != 0)
		{
			reactions::RefreshRecord(villager, TypeByte(slot->type), villager::CurrentTurn());
		}
		registry.Remove<LivingReaction>(villager);
	}
	// a villager panicking at a shield's fall no longer looks back at where it stood
	if (registry.AllOf<VillagerPanic>(villager))
	{
		registry.RemoveState<VillagerPanic>(villager);
	}
}

uint32_t villager_reactions::ExitReaction(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// the circle hug reset: no hugged object, turns to the object 0xFF. (approximate) openblack's
	// WallHugObjectReference
	registry.Remove<WallHugObjectReference>(villager);
	// whether next is a reactive state (field0xb8); not -> StopReacting
	const bool reactive = villager::state_info::IsReactive(villager::state_info::StateInfo(next));
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("ExitReaction({}) reactive {}", static_cast<int>(next), reactive ? 1 : 0));
	}
	if (!reactive)
	{
		StopReacting(villager);
	}
	return 1;
}
