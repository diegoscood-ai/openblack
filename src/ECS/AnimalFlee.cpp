/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalWallHug.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Dances.h"
#include "ECS/Effects/CreatureReactionFacts.h"
#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Effects/VillagerCreatureRules.h"
#include "ECS/Map.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

/// The animals' reactions like the original (docs/wiki/beings/animal-life.md#reactions): the Animal handler of
/// ECS/Effects/Reactions (a reaction is spread over a spiral of map cells when made and when the turn's walk reaches it,
/// and each animal of a cell is given to this handler): the animal's score, its records (common to the Living, in the reactions
/// module) and the rule to switch from the reaction it already takes. Ported types: 28 flee from predator (a predator's
/// construction), 7 food (a food pile: the map's CREATE_POT, a pot the hand puts down), 9 flying object (every throw from the
/// hand), 6 a creature (run from as a thing that frightens it), 10 a fire (run from as a thing that frightens it, a burning
/// animal from its own). The villagers take theirs through the same module (VillagerReactions.cpp: fire and teleport).
namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Transform;

namespace
{
constexpr uint8_t k_FleeFromObject = 0;
constexpr uint8_t k_ReactToCreature = 6;
constexpr uint8_t k_ReactToFood = 7;
constexpr uint8_t k_ReactToFlyingObject = 9;
constexpr uint8_t k_ReactToFire = 10;
constexpr uint8_t k_FleeFromPredator = 28;

namespace reactions = effects::reactions;
using reactions::Reaction;

const ReactionInfo& Info(uint8_t type)
{
	return Locator::infoConstants::value().reaction.at(type);
}

uint8_t TypeOf(const Reaction& reaction)
{
	return static_cast<uint8_t>(reaction.type);
}

/// The living info's isReacting[type]
bool IsReactingTo(const GAnimalInfo& info, uint8_t type)
{
	return ReactsTo(info.isReacting, type);
}

/// The hunters of the lion family: the lion, tiger, leopard and wolf (and their puzzle pieces) and the ground flock
/// miracle's wolf, which is one of them too. Each makes the predator reaction as it is made, and runs only from a
/// stronger one
bool IsPredator(AnimalInfo type)
{
	return detail::HunterOf(type) != detail::Hunter::None || type == AnimalInfo::SpellWolf;
}

glm::vec2 PosOf(entt::entity entity)
{
	return detail::Xz(Locator::entitiesRegistry::value().Get<const Transform>(entity));
}

/// Where a thing is as its map coordinates hold it: x and z through 16.16, the height the land's there plus its
/// altitude
glm::vec3 PointOf(entt::entity entity)
{
	return map_coords::ToWorld(map_coords::FromWorld(Locator::entitiesRegistry::value().Get<const Transform>(entity).position));
}

/// the object's speed (u16): an animal's own, else 0
uint32_t SpeedOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* brain = registry.TryGet<const AnimalBrain>(object); brain != nullptr)
	{
		return brain->speed;
	}
	const auto* animal = registry.TryGet<const Animal>(object);
	return animal != nullptr ? static_cast<uint32_t>(detail::InfoOf(*animal).speedGroup.speedDefault) : 0;
}

/// The movement direction: a creature's ahead of its body (in the physics too); a thing in the physics by its velocity,
/// up and down too; else an animal's last step in whole map units, level, whether or not it moved this turn
glm::vec3 MovementOf(entt::entity object)
{
	if (creature_reaction_facts::IsCreature(object))
	{
		return creature_reaction_facts::MovementOf(object);
	}
	if (const auto* po = Locator::dynamicsSystem::value().Find(object); po != nullptr)
	{
		return po->body.velocity;
	}
	if (const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(object); brain != nullptr)
	{
		return {static_cast<float>(brain->step.x), 0.0f, static_cast<float>(brain->step.y)};
	}
	return glm::vec3(0.0f);
}

/// The final state: the top state if it is final, else the destination
uint8_t FinalStateOf(const AnimalBrain& brain)
{
	const auto& info = Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(brain.topState, 52));
	return info.field0xc != 0 ? brain.topState : brain.finalState;
}

/// Whether an animal may take a reaction (the villager's is villager::IsAvailableForReaction): functional (an animal
/// always is), not controlled by a script, not in the dance editor (none in openblack), the final state not one of
/// the list, not dying and not on a structure (only moving on a structure sets it, not ported: never set)
bool IsAvailableForReaction(entt::entity entity, const AnimalBrain& brain)
{
	if ((brain.status & 1) != 0 || script_held::IsControlledByScript(entity))
	{
		return false;
	}
	switch (static_cast<AnimalState>(FinalStateOf(brain)))
	{
	case AnimalState::Dead:
	case AnimalState::Drowning:
	case AnimalState::Downed:
	case AnimalState::BeingEaten:
	case AnimalState::Dying:
	case AnimalState::WaitForAnimation:
	case static_cast<AnimalState>(5): // IN_DANCE
		return false;
	default:
		return true;
	}
}

/// the pot's info, or none
const GPotInfo* PotInfoOf(entt::entity pot)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const components::Pot>(pot);
	if (data == nullptr || data->type == PotInfo::_COUNT)
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().pot.at(static_cast<size_t>(data->type));
}

/// the table's priority function of each ported type
uint32_t Priority(uint8_t type, entt::entity entity, const AnimalBrain& brain, const GAnimalInfo& info, entt::entity initiator)
{
	auto& registry = Locator::entitiesRegistry::value();
	switch (type)
	{
	case k_FleeFromPredator:
	{
		// at its speed2 or slower a predator goes unnoticed; a predator only flees from a stronger one
		const auto* predator = registry.TryGet<const Animal>(initiator);
		if (predator == nullptr)
		{
			return 0;
		}
		const auto& predatorInfo = detail::InfoOf(*predator);
		if (IsPredator(registry.Get<const Animal>(entity).type))
		{
			return predatorInfo.strength > info.strength ? Info(type).priority : 0;
		}
		return SpeedOf(initiator) <= static_cast<uint32_t>(predatorInfo.speedGroup.speed2) ? 0 : Info(type).priority;
	}
	case k_ReactToFood:
	{
		// some food in it, not in the physics, near enough, and a hungry animal whose needsFoodTypes takes that food
		const auto* pot = registry.TryGet<const components::Pot>(initiator);
		const auto* potInfo = PotInfoOf(initiator);
		if (pot == nullptr || potInfo == nullptr || pot->amount == 0 ||
		    Locator::dynamicsSystem::value().Find(initiator) != nullptr)
		{
			return 0;
		}
		if (gutils::GetDistanceInMetres(PosOf(entity), PosOf(initiator)) > Info(type).maxDistanceToRunAwayFromObject)
		{
			return 0;
		}
		const bool hungry = info.hunger != 0 && brain.hunger >= static_cast<int32_t>(info.hunger);
		const bool eats = (static_cast<uint32_t>(potInfo->foodType) & static_cast<uint32_t>(info.needsFoodTypes)) != 0;
		return hungry && eats ? Info(type).priority : 0;
	}
	case k_ReactToCreature:
		// none while it dances
		return effects::creature_reaction::AnimalPriority(dances::DanceOf(registry, entity) != entt::null, Info(type).priority);
	case k_ReactToFlyingObject:
		// not while it flies or lands itself
		return brain.topState == static_cast<uint8_t>(AnimalState::Flying) ||
		               brain.topState == static_cast<uint8_t>(AnimalState::Landed)
		           ? 0
		           : Info(type).priority;
	case k_FleeFromObject:
	case k_ReactToFire:
		// the table's own priority, whatever the thing: a burning animal takes up its own fire too
		return Info(type).priority;
	default:
		return 0;
	}
}

/// The reaction's score (ECS/Effects/Reactions: Score) with the animal's isReacting and the table's priority function
uint32_t Score(uint8_t type, entt::entity entity, const AnimalBrain& brain, entt::entity initiator, float d)
{
	const auto& info = detail::InfoOf(Locator::entitiesRegistry::value().Get<const Animal>(entity));
	if (!IsReactingTo(info, type) || d > Info(type).maxReactionDistance)
	{
		return 0;
	}
	return reactions::Score(type, true, Priority(type, entity, brain, info, initiator), d);
}

/// The standard turns to react / before reacting again, longer when nearer
uint32_t Standard(uint32_t turns, const ReactionInfo& reaction, float d)
{
	const float range = 10.0f + reaction.maxReactionDistance;
	return static_cast<uint32_t>(static_cast<float>(turns) *
	                             (1.0f + 0.5f * reaction.howImportantIsDistance * (range - d) / range));
}

/// the table's turns to react: how long it reacts
uint32_t TurnsToReact(uint8_t type, const AnimalBrain& brain, entt::entity initiator, float d)
{
	const auto& reaction = Info(type);
	if (type == k_ReactToCreature)
	{
		// longer for a near creature and an impressive last action; an animal has no town
		return effects::creature_reaction::TurnsToReact(creature_reaction_facts::TurnsFactsOf(initiator, type, d, false));
	}
	if (type == k_FleeFromPredator)
	{
		// 0 unless an animal; 0 while it looks at a predator that
		// runs faster than 5 m/s at it
		if (!Locator::entitiesRegistry::value().AllOf<Animal>(initiator))
		{
			return 0;
		}
		const bool fast = detail::Metres(SpeedOf(initiator)) * 10.0f > 5.0f;
		if (brain.topState == static_cast<uint8_t>(AnimalState::LookingAtObjectReaction) && fast &&
		    effects::flee::ComingTowards(PointOf(Locator::entitiesRegistry::value().ToEntity(brain)), PointOf(initiator),
		                                 MovementOf(initiator)))
		{
			return 0;
		}
	}
	return Standard(reaction.numGameTurnsForNormalThingsToReact, reaction, d);
}

/// the table's turns before reacting again: how long before it reacts to that type again
uint32_t TurnsBeforeReactingAgain(uint8_t type, entt::entity entity, entt::entity initiator, float d)
{
	const auto& reaction = Info(type);
	if (type == k_ReactToCreature)
	{
		// (openblack, guard) the original takes the initiator for a creature without asking: anything else reacts again
		// at once. At once too while the creature does something impressive; an animal has no town
		if (!creature_reaction_facts::IsCreature(initiator))
		{
			return 0;
		}
		return effects::creature_reaction::TurnsBeforeReactingAgain(
		    creature_reaction_facts::TurnsFactsOf(initiator, type, d, true));
	}
	if (type == k_FleeFromPredator)
	{
		// 0 unless a slow animal at least the minimum distance away: its true distance in map coordinates, not the
		// reaction's spread one (a distance that is not a number is not that far)
		if (!Locator::entitiesRegistry::value().AllOf<Animal>(initiator) || detail::Metres(SpeedOf(initiator)) * 10.0f > 5.0f ||
		    !animals::FarEnoughFromPredator(gutils::GetDistanceInMetres(PosOf(entity), PosOf(initiator)),
		                                    reaction.minDistanceToRunAwayFromObject))
		{
			return 0;
		}
	}
	return Standard(reaction.numGameTurnsForNormalThingsBeforeReactingAgain, reaction, d);
}

/// The final state is stored as the previous one, then the reaction's state
void AddReaction(entt::entity entity, AnimalBrain& brain, const Reaction& reaction, AnimalState state)
{
	if (brain.reaction == 0)
	{
		brain.previousState = FinalStateOf(brain);
	}
	detail::SetTopState(entity, brain, state);
	brain.reaction = reaction.id;
	brain.joinOrder = openblack::ecs::effects::reactions::NextJoinOrder();
	brain.predator = reaction.initiator;
}

/// the table's start function
void StartReacting(entt::entity entity, AnimalBrain& brain, const Reaction& reaction)
{
	switch (TypeOf(reaction))
	{
	case k_FleeFromPredator:
		AddReaction(entity, brain, reaction, AnimalState::FleeingFromPredatorReaction);
		break;
	case k_ReactToFood:
		AddReaction(entity, brain, reaction, AnimalState::GotoFoodReaction);
		break;
	case k_FleeFromObject:
	case k_ReactToCreature:
	case k_ReactToFire:
		// it runs from a creature or a burning thing as from any frightening thing
		AddReaction(entity, brain, reaction, AnimalState::FleeingFromObjectReaction);
		break;
	case k_ReactToFlyingObject:
	{
		// it flees only when 2 x the object's speed beats the distance
		const auto* po = Locator::dynamicsSystem::value().Find(reaction.initiator);
		const float speed = po != nullptr ? glm::length(po->body.velocity) : 0.0f;
		if (animals::FleesFlyingObject(speed, gutils::GetDistanceInMetres(PosOf(entity), PosOf(reaction.initiator))))
		{
			AddReaction(entity, brain, reaction, AnimalState::FleeingFromObjectReaction);
		}
		break;
	}
	default:
		break;
	}
}

/// The reaction applied to one animal of the cell: the Animal handler of
/// ECS/Effects/Reactions (d = (|dz| + |dx|) / 2 to the initiator)
void AnimalReaction(entt::entity entity, const Reaction& reaction, float d)
{
	auto* brain = detail::BrainOf(entity);
	// the availability check made for every reaction type
	if (brain == nullptr || !IsAvailableForReaction(entity, *brain))
	{
		return;
	}
	const uint8_t type = TypeOf(reaction);
	const auto again = TurnsBeforeReactingAgain(type, entity, reaction.initiator, d);
	if (brain->reaction == 0)
	{
		if (Score(type, entity, *brain, reaction.initiator, d) > 0 && reactions::Records(entity, type, again, detail::Turn()))
		{
			reactions::MarkStarted(reaction.id, detail::Turn());
			StartReacting(entity, *brain, reaction);
		}
		return;
	}
	// already reacting: one of the same type only where the type table allows it, never the one it follows; the new
	// one must score more, and the old one must have lasted 10 s (1 s for a hand pick-up)
	const auto* current = reactions::Find(brain->reaction);
	if (current == nullptr || !reactions::IsAvailable(current) ||
	    !animals::MayWeighAnotherReaction(TypeOf(*current), current->id, type, reaction.id, reactions::SameTypeSwitch(type)))
	{
		return;
	}
	// the current reaction's distance: the reaction's position (the initiator's, as in ECS/Effects/Reactions), only
	// its cell, and the distance from the animal's MapCoords to that cell's centre, not to the initiator itself
	const auto at = map_coords::FromMetres(PosOf(current->initiator));
	const map_coords::JustMapXZ cell {map_coords::SignedCellOf(at.x), map_coords::SignedCellOf(at.z)};
	const float distance = gutils::GetDistanceInMetresToCell(map_coords::FromMetres(PosOf(entity)), cell);
	const float cur = static_cast<float>(Score(TypeOf(*current), entity, *brain, current->initiator, distance));
	const float now = static_cast<float>(Score(type, entity, *brain, reaction.initiator, d));
	const float seconds = static_cast<float>((detail::Turn() - reactions::RecordTurn(entity, TypeOf(*current))) / 10);
	if (!reactions::MaySwitch(cur, now, seconds, TypeOf(*current)))
	{
		return;
	}
	detail::SwitchReaction(entity, *brain, reaction);
}

/// A reaction of an animal-side initiator: made and spread (ECS/Effects/Reactions), with the player the original
/// passes: the predator's player, the pot's player, the thrower's player
uint32_t CreateReaction(entt::entity initiator, uint8_t type, std::optional<PlayerNames> player)
{
	if (!Locator::infoConstants::has_value() || !ecs::IsAvailable(initiator))
	{
		return 0;
	}
	return reactions::CreateReaction(initiator, static_cast<openblack::Reaction>(type), player, false);
}

/// Removes the reactions the object started [of one type, or all]
void RemoveReactions(entt::entity initiator, int type)
{
	if (type < 0)
	{
		reactions::RemoveAllReactionsInitiatedByObject(initiator);
	}
	else
	{
		reactions::RemoveAllReactionsOfTypeInitiatedBy(initiator, static_cast<openblack::Reaction>(type));
	}
}

/// Off the reaction, its record's turn refreshed, no reaction and no initiator
void StopReactingPlain(AnimalBrain& brain)
{
	if (const auto* reaction = reactions::Find(brain.reaction); reaction != nullptr)
	{
		reactions::RefreshRecord(Locator::entitiesRegistry::value().ToEntity(brain), TypeOf(*reaction), detail::Turn());
	}
	brain.reaction = 0;
	brain.predator = entt::null;
}

/// Back to the stored state's return state (animalStateTable field0x1c: DECIDE, or 0 for the moving states: then it
/// stays in the invalid state 0), then off the reaction
void StopReactingAndSetState(detail::Context& ctx)
{
	const auto& info = Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(ctx.brain.previousState, 52));
	detail::SetTopState(ctx, static_cast<AnimalState>(static_cast<uint8_t>(info.field0x1c)));
	if (ctx.brain.reaction != 0)
	{
		StopReactingPlain(ctx.brain);
	}
}

const Reaction* CurrentReaction(const detail::Context& ctx)
{
	const auto* reaction = reactions::Find(ctx.brain.reaction);
	return reactions::IsAvailable(reaction) ? reaction : nullptr;
}

bool InitiatorGone(const detail::Context& ctx)
{
	return ctx.brain.predator == entt::null || !detail::Available(ctx.brain.predator);
}

/// From a moving object: ~10 m sideways out of its path, on the side it stands on, +-4 m; from a still object:
/// straight away (on it: stays). Kept in map coordinates, as the original's goal is
glm::vec2 FleeingPosition(glm::vec3 me, glm::vec3 object, glm::vec3 movement, float distance)
{
	const glm::vec2 at(me.x, me.z);
	const glm::vec2 from(object.x, object.z);
	glm::vec2 p;
	if (glm::length(movement) > 0.0f)
	{
		const float randomX = game_random::GameFloatRand(effects::flee::k_FleeJitter);
		const float randomZ = game_random::GameFloatRand(effects::flee::k_FleeJitter);
		p = effects::flee::FleeingPositionFromMoving(at, from, {movement.x, movement.z}, distance, randomX, randomZ);
	}
	else
	{
		p = effects::flee::FleeingPositionFromStill(at, from, distance);
	}
	return {map_coords::Quantise(p.x), map_coords::Quantise(p.y)};
}

/// The working position: on the object's rim facing it, the two radii apart
/// (ecs::object::GetWorkingPos)
glm::vec2 WorkingPos(entt::entity object, entt::entity me)
{
	return map_coords::ToMetres(object::GetWorkingPos(object, me));
}
} // namespace

void SetupPotReaction(entt::entity pot)
{
	// once, until RemovePotReaction: the pot's own flag, set even when no reaction could be started and kept when the
	// reaction ends some other way; its info's associatedReaction
	auto* data = Locator::entitiesRegistry::value().TryGet<components::Pot>(pot);
	const auto* potInfo = PotInfoOf(pot);
	if (data == nullptr || data->reactionStarted || potInfo == nullptr || static_cast<int>(potInfo->associatedReaction) < 0)
	{
		return;
	}
	data->reactionStarted = true;
	// the pile's owner (a magic food or wood pile's; a map pot keeps the default, inferred)
	CreateReaction(pot, static_cast<uint8_t>(potInfo->associatedReaction), data->owner);
}

void RemovePotReaction(entt::entity pot)
{
	// picked up, emptied or deleted: only a pot that started its reaction loses its reactions, then every one it
	// started (a fire's or a flight's too)
	auto* data = Locator::entitiesRegistry::value().TryGet<components::Pot>(pot);
	if (data == nullptr || !data->reactionStarted)
	{
		return;
	}
	data->reactionStarted = false;
	RemoveReactions(pot, -1);
}

void SpreadFlyingObjectReaction(entt::entity object, PlayerNames thrower)
{
	CreateReaction(object, k_ReactToFlyingObject, thrower);
}

void EndReactionsOf(entt::entity object)
{
	// the end of its physics: its own flying-object reactions go (the reacting animals stop, their state kept)
	RemoveReactions(object, k_ReactToFlyingObject);
}

void SpreadPredatorReaction(entt::entity predator)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Animal, Transform>(predator) || !IsPredator(registry.Get<const Animal>(predator).type))
	{
		return;
	}
	// the predator's permanent reaction 28, for the neutral player whoever's animal it is: its constructor asks the base
	// GetPlayer, not a spell animal's own
	CreateReaction(predator, k_FleeFromPredator, PlayerNames::NEUTRAL);
}

float SpeedInMetresPerSecond(entt::entity animal)
{
	// its speed (a distance a turn) in metres, ten turns a second
	return detail::Metres(SpeedOf(animal)) * 10.0f;
}

int32_t WalkingSpeed(entt::entity animal)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Animal>(animal);
	return data != nullptr ? static_cast<int32_t>(detail::InfoOf(*data).speedGroup.speed3) : 0;
}

glm::vec3 MovementDirection(entt::entity animal)
{
	return MovementOf(animal);
}

void RegisterReactionHandler()
{
	// an animal never ends its cell's walk: no reaction only impresses it
	reactions::SetLivingReactionHandler(reactions::LivingClass::Animal,
	                                    [](entt::entity entity, const reactions::Reaction& reaction, float d) {
		                                    AnimalReaction(entity, reaction, d);
		                                    return reactions::CellWalk::Next;
	                                    });
}

void ClearReactions()
{
	reactions::Clear();
	RegisterReactionHandler();
}

namespace detail
{
} // namespace detail

namespace detail
{

bool IsReactionState(uint8_t state)
{
	// the states whose exit function is the reaction's exit
	switch (state)
	{
	case 6:
	case 7:
	case 8:
	case 9:
	case 19:
	case 20:
	case 21:
	case 22:
	case 25:
	case 26:
	case 30:
	case 49:
		return true;
	default:
		return false;
	}
}

void ExitReaction(AnimalBrain& brain, uint8_t next)
{
	// into a state with another exit function, the reaction is dropped (the state kept)
	if (brain.reaction != 0 && !IsReactionState(next))
	{
		StopReactingPlain(brain);
	}
}

void ProcessReaction(Context& ctx)
{
	if (ctx.brain.reaction == 0)
	{
		return;
	}
	const auto* reaction = CurrentReaction(ctx);
	if (reaction == nullptr)
	{
		StopReactingPlain(ctx.brain);
		return;
	}
	if (InitiatorGone(ctx))
	{
		StopReactingAndSetState(ctx);
		return;
	}
	// compared signed: a creature far enough gives a negative count, which ends the reaction at once
	const auto elapsed = static_cast<int32_t>(Turn() - effects::reactions::RecordTurn(ctx.entity, TypeOf(*reaction)));
	const float d = gutils::GetDistanceInMetres(Xz(ctx.transform), PosOf(ctx.brain.predator));
	if (elapsed > static_cast<int32_t>(TurnsToReact(TypeOf(*reaction), ctx.brain, ctx.brain.predator, d)))
	{
		StopReactingAndSetState(ctx);
	}
}

void SwitchReaction(entt::entity entity, AnimalBrain& brain, const Reaction& reaction)
{
	reactions::SetReactionDoneWhen(entity, TypeOf(reaction), Turn());
	StartReacting(entity, brain, reaction);
}

void FleeingFromPredatorReaction(Context& ctx)
{
	if (ctx.brain.predator == entt::null)
	{
		return;
	}
	if (InitiatorGone(ctx))
	{
		StopReactingAndSetState(ctx);
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PosOf(ctx.brain.predator);
	if (gutils::GetDistanceInMetres(me, at) > Info(k_FleeFromPredator).maxDistanceToRunAwayFromObject)
	{
		StopReactingAndSetState(ctx);
		return;
	}
	const auto p = FleeingPosition(PointOf(ctx.entity), PointOf(ctx.brain.predator), MovementOf(ctx.brain.predator), 10.0f);
	if (InBounds(p))
	{
		SetSpeed(ctx, static_cast<uint32_t>(ctx.info.speedGroup.speedFleeing));
		SetupMoveToPos(ctx, p, AnimalState::FleeingFromObjectReaction);
	}
}

void FleeingFromObjectReaction(Context& ctx)
{
	if (ctx.brain.predator == entt::null)
	{
		return;
	}
	const auto* reaction = CurrentReaction(ctx);
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PosOf(ctx.brain.predator);
	const float d = gutils::GetDistanceInMetres(me, at);
	if (reaction == nullptr || d > Info(TypeOf(*reaction)).maxDistanceToRunAwayFromObject)
	{
		StopReactingAndSetState(ctx);
		return;
	}
	// flee if it comes towards it (30, 30): the 30s are states (FLEEING_AND_LOOKING), the distance is
	// the reaction's minDistanceToRunAwayFromObject
	const auto movement = MovementOf(ctx.brain.predator);
	const auto mine = PointOf(ctx.entity);
	const auto its = PointOf(ctx.brain.predator);
	if (d > Info(TypeOf(*reaction)).minDistanceToRunAwayFromObject && !effects::flee::ComingTowards(mine, its, movement))
	{
		// it turns a step towards it
		SetTopState(ctx, AnimalState::FleeingAndLookingAtObjectReaction);
		ctx.brain.angle = animals::LookTowards(ctx.brain.angle, gutils::GetAngleFromXZ(me, at), animals::k_LookAtObjectTurn);
		FaceAngle(ctx.transform, ctx.brain.angle);
		return;
	}
	const auto p = FleeingPosition(mine, its, movement, 10.0f);
	if (InBounds(p))
	{
		// a LINEAR walk round the obstacles (ECS/AnimalWallHug.cpp)
		SetupMoveToWithHug(ctx, p, AnimalState::FleeingAndLookingAtObjectReaction);
	}
}

void FleeingAndLookingReaction(Context& ctx)
{
	// looks at the object while it stays near, a step a turn
	const auto* reaction = CurrentReaction(ctx);
	if (ctx.brain.predator == entt::null || reaction == nullptr)
	{
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PosOf(ctx.brain.predator);
	if (gutils::GetDistanceInMetres(me, at) > Info(TypeOf(*reaction)).maxDistanceToRunAwayFromObject)
	{
		StopReactingAndSetState(ctx);
		return;
	}
	ctx.brain.angle = animals::LookTowards(ctx.brain.angle, gutils::GetAngleFromXZ(me, at), animals::k_LookAtObjectTurn);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

void GotoFoodReaction(Context& ctx)
{
	// to the food's working point, then ARRIVES_AT_FOOD
	if (InitiatorGone(ctx))
	{
		StopReactingAndSetState(ctx);
		return;
	}
	// a LINEAR walk round the obstacles (ECS/AnimalWallHug.cpp)
	SetupMoveToWithHug(ctx, WorkingPos(ctx.brain.predator, ctx.entity), AnimalState::ArrivesAtFoodReaction);
}

void ArrivesAtFoodReaction(Context& ctx)
{
	// hunger 0 and RemoveResource(FOOD, min(50, amount))
	auto& registry = Locator::entitiesRegistry::value();
	const auto food = ctx.brain.predator;
	if (ecs::IsAvailable(food))
	{
		if (auto* pot = registry.TryGet<components::Pot>(food); pot != nullptr)
		{
			const auto* potInfo = PotInfoOf(food);
			if (potInfo != nullptr && potInfo->resourceType == ResourceType::Food)
			{
				ctx.brain.hunger = 0;
				const auto taken = std::min<uint32_t>(50u, pot->amount);
				pot->amount -= taken;
				if (pot->amount == 0)
				{
					// emptied, its reaction goes; the empty pile is deleted
					RemovePotReaction(food);
					StopReactingAndSetState(ctx);
					registry.Destroy(food);
					registry.SetDirty();
					return;
				}
				archetypes::PotArchetype::SetSize(food, true);
			}
		}
	}
	StopReactingAndSetState(ctx);
}

} // namespace detail
} // namespace openblack::ecs::animal_ai
