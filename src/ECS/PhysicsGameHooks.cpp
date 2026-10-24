/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsGameHooks.h"

#include <optional>
#include <utility>

#include "Creature/CreatureMorph.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureCatchWalk.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureStruck.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Physics/Body.h"
#include "Physics/TempleHeart.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The mass a temple's heart's body is set up with
constexpr float k_HeartMass = 10000.0f;

/// A creature struck in the physics, by what its body and the striking body say and by what it is. Returns false when
/// it went away
bool CreatureReactToImpact(physics_hooks::World& world, DynamicsSystemInterface& dynamics, const PhysicsEntry& entry,
                           const ImpactInfo& impact)
{
	const auto& registry = std::as_const(world.Entities());
	const auto creature = entry.entity;
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body != nullptr)
	{
		// The mass goes by the size the body is drawn at and the shape it is to become, not the one drawn yet
		const auto target = creature_pose::TargetMorph(registry, creature);
		// the entry the body's hitter names in the physics list now
		const auto* hitter = ecs::physics::hitter::HitterEntry(dynamics.GetState().objects, entry);
		creature_struck::HurtCreature(
		    world, creature,
		    {
		        .struck = hitter != nullptr,
		        .hitter = impact.hitBy,
		        .hitterIsCreature = impact.hitBy != entt::null && registry.AllOf<Creature>(impact.hitBy),
		        .hitterCentre = hitter != nullptr ? hitter->body.Centre() : glm::vec3(0.0f),
		        .hitterPlayer = hitter != nullptr ? hitter->throwerPlayer : std::nullopt,
		        .ownPlayer = entry.throwerPlayer,
		        .force = entry.forceSum,
		        .impact = entry.impact,
		        .owner = body->owner,
		        .scriptControlled = registry.AllOf<ScriptControlled>(creature),
		        .drawnSize = creature_pose::DrawnSize(registry, creature),
		        .thinFat = target.thinFat,
		        .weakStrong = target.weakStrong,
		    });
	}
	return world.Entities().Valid(creature);
}

/// A temple's heart struck in the physics: it passes the blow on to a building or a homeless villager of its player's
/// towns, and only with none to take it is the heart itself harmed, by a thing still there that breaks buildings.
/// Returns false when the heart went away
bool StrikeHeart(physics_hooks::World& world, DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact)
{
	namespace temple_heart = openblack::physics::temple_heart;
	auto& registry = world.Entities();
	const auto heart = entry.entity;
	// Nothing at all without a body that struck it
	const auto* hitter = ecs::physics::hitter::HitterEntry(dynamics.GetState().objects, entry);
	if (hitter == nullptr)
	{
		return true;
	}
	const auto struck = impact.hitBy;
	const bool breaks = struck != entt::null && dynamics.PhysicallyDestroysAbodes(struck);
	// The player who threw it, which the harm is put down to
	const auto player = hitter->throwerPlayer;
	const auto target = world.HeartPassesBlowTo(heart, breaks);
	auto kind = temple_heart::TargetKind::Heart;
	if (target != heart && registry.Valid(target))
	{
		kind = registry.AllOf<Villager>(target) ? temple_heart::TargetKind::Villager : temple_heart::TargetKind::Building;
	}
	const auto outcome = temple_heart::Outcome({
	    .struck = true,
	    .hitterAvailable = struck != entt::null && ecs::IsAvailable(struck),
	    .breaksBuildings = breaks,
	    .target = kind,
	    .villagerInPhysics = kind == temple_heart::TargetKind::Villager && dynamics.Find(target) != nullptr,
	});
	// The heart keeps the turn it takes such a blow itself, before it is harmed
	if (auto* part = registry.TryGet<CitadelHeart>(heart); part != nullptr)
	{
		part->breakingBlowTurn = temple_heart::BreakingBlowTurn(outcome, game_clock::Turn(), part->breakingBlowTurn);
	}
	switch (outcome)
	{
	case temple_heart::StrikeOutcome::Nothing:
		break;
	case temple_heart::StrikeOutcome::BuildingStruck:
		static_cast<void>(world.StrikeBuilding(target, entry));
		break;
	case temple_heart::StrikeOutcome::VillagerFlung:
		// Flung straight up, spinning, by no one
		static_cast<void>(dynamics.AddObject(target, temple_heart::k_VillagerLaunch,
		                                     DynamicsSystemInterface::BodySpin {temple_heart::k_VillagerLaunchSpin}));
		break;
	case temple_heart::StrikeOutcome::HeartHarmed:
	{
		// A hit of that harm, of a metre, applied by the hand's player (none: no one's), through the heart's defence as
		// any effect on an object is
		auto values = effects::EffectValues {};
		values.numbers.at(static_cast<size_t>(effects::EffectValues::Number::Hit)) =
		    temple_heart::Harm(hitter->body.velocity, hitter->body.Mass());
		values.radius = temple_heart::k_HarmRadius;
		world.ApplyEffect(
		    heart, values,
		    magic::EffectSource {.player = player.value_or(PlayerNames::NEUTRAL), .playerless = !player.has_value()});
		break;
	}
	}
	return registry.Valid(heart);
}
} // namespace

PhysicsGameHooks::PhysicsGameHooks()
    : PhysicsGameHooks(std::make_unique<GamePhysicsHooksWorld>())
{
}

PhysicsGameHooks::PhysicsGameHooks(std::unique_ptr<physics_hooks::World> world)
    : _world(std::move(world))
{
}

void PhysicsGameHooks::StartPhysics(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, bool fromHand)
{
	switch (dynamics.ClassOf(entry.entity))
	{
	case PhysicsClass::Villager:
		villager_physics::VillagerStartPhysics(entry.entity, entry, fromHand);
		return;
	case PhysicsClass::Animal:
		if (fromHand)
		{
			// from the hand: FLYING, the species' THROWN clip
			ecs::animal_ai::InitialisePhysics(entry.entity);
		}
		// in the world its start ran before the body was made (StartPhysicsInWorld)
		return;
	case PhysicsClass::Scaffold:
		// after the generic InitialisePhysics
		scaffolds::OnInitialisePhysics(entry.entity, entry, fromHand);
		return;
	default:
		PhysicsClassHooks::StartPhysics(dynamics, entry, fromHand);
		return;
	}
}

std::optional<bool> PhysicsGameHooks::ReactToImpact(DynamicsSystemInterface& dynamics, PhysicsEntry& entry,
                                                    const ImpactInfo& impact)
{
	switch (dynamics.ClassOf(entry.entity))
	{
	case PhysicsClass::Villager:
		return villager_physics::VillagerReactToImpact(entry.entity, impact);
	case PhysicsClass::Animal:
		return villager_physics::AnimalReactToImpact(entry.entity, entry, impact);
	case PhysicsClass::Tree:
	case PhysicsClass::DeadTree:
		return _world->HandPart().TreeReactToImpact(entry.entity, entry);
	case PhysicsClass::Pot:
		return _world->HandPart().PotReactToImpact(entry.entity, entry, impact);
	case PhysicsClass::Creature:
		return CreatureReactToImpact(*_world, dynamics, entry, impact);
	default:
		if (_world->Entities().AllOf<CitadelHeart>(entry.entity))
		{
			return StrikeHeart(*_world, dynamics, entry, impact);
		}
		return PhysicsClassHooks::ReactToImpact(dynamics, entry, impact);
	}
}

std::optional<entt::entity> PhysicsGameHooks::EndPhysics(DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	switch (dynamics.ClassOf(entry.entity))
	{
	case PhysicsClass::Villager:
		return villager_physics::VillagerEndPhysicsOfClass(entry.entity, entry);
	case PhysicsClass::Animal:
		return villager_physics::AnimalEndPhysicsOfClass(entry.entity, entry);
	case PhysicsClass::Tree:
		return _world->HandPart().TreeEndPhysics(entry.entity, entry);
	case PhysicsClass::DeadTree:
	{
		// the object's end (calls DynamicsSystem::EndPhysicsAsObject itself), then the "wood here" reaction with the
		// player of its physics record: who threw it, directly or through what it hit, else none. A felled tree has the
		// object's end only
		const auto object = entry.entity;
		// put down gently by a town, it becomes its artefact first, as any fixed thing does
		object_physics::ConsiderArtefact(entry, object);
		const bool deleted = ecs::IsAvailable(object) && !dynamics.EndPhysicsAsObject(object);
		ecs::DeadTreeEndPhysicsReaction(object, entry.throwerPlayer);
		return deleted ? entt::entity {entt::null} : object;
	}
	case PhysicsClass::Pot:
		// a hand's pile becomes a land pile put down by the interface of its physics record: the player's who threw it,
		// directly or through what struck it, else none
		return _world->HandPart().PotEndPhysics(entry.entity, entry.throwerPlayer);
	case PhysicsClass::Scaffold:
		// calls DynamicsSystem::EndPhysicsAsObject itself
		return scaffolds::OnEndPhysics(entry.entity, entry);
	case PhysicsClass::Fragment:
		// calls DynamicsSystem::EndPhysicsAsObject itself; a big piece sticks to its building through the body's matrix
		return _world->BuildingDamage().PieceAtRest(entry.entity, entry.body.Axes(), entry.body.Centre());
	default:
		// a rock or other mobile static put down gently by a town becomes its artefact, before it goes back in the map
		if (_world->Entities().AllOf<MobileStatic>(entry.entity))
		{
			object_physics::ConsiderArtefact(entry, entry.entity);
		}
		return PhysicsClassHooks::EndPhysics(dynamics, entry);
	}
}

bool PhysicsGameHooks::CallsBackInMap(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Villager:
	case PhysicsClass::Animal:
	case PhysicsClass::Scaffold:
	case PhysicsClass::Fragment:
	case PhysicsClass::DeadTree:
		return true;
	default:
		return PhysicsClassHooks::CallsBackInMap(dynamics, object);
	}
}

void PhysicsGameHooks::Moved(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Tree:
		_world->HandPart().TreeMoved(object);
		return;
	default:
		PhysicsClassHooks::Moved(dynamics, object);
		return;
	}
}

void PhysicsGameHooks::DropSound(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Tree:
		// only on land
		if (sea_cells::IsLand(_world->Entities().Get<const Transform>(object).position))
		{
			_world->HandPart().TreeDropSound(object);
		}
		return;
	default:
		PhysicsClassHooks::DropSound(dynamics, object);
		return;
	}
}

bool PhysicsGameHooks::SetUpBody(DynamicsSystemInterface& dynamics, entt::entity object, openblack::physics::Body& body)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Creature:
		return creature_physics::SetUpBody(object, body);
	default:
		return PhysicsClassHooks::SetUpBody(dynamics, object, body);
	}
}

std::optional<float> PhysicsGameHooks::BodyMassOf(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Creature:
		// the creature's body weighs 1000 whatever its size, as the original sets it up: the body's mass is set once and
		// does not follow the creature's growth
		return creature_physics::k_Mass;
	default:
		if (_world->Entities().AllOf<CitadelHeart>(object))
		{
			return k_HeartMass; // a temple's heart's body, whatever its size
		}
		return PhysicsClassHooks::BodyMassOf(dynamics, object);
	}
}

std::optional<float> PhysicsGameHooks::ObjectWeightOf(DynamicsSystemInterface& dynamics, entt::entity object)
{
	// (not ported) a creature's own weight as an object, for catching and lifting: its body's 1000 stands for it
	if (dynamics.ClassOf(object) == PhysicsClass::Creature)
	{
		return creature_physics::k_Mass;
	}
	// a building (a storage pit too) weighs as any object, by its abode's info: scale^3 x its weight
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Abode>(object))
	{
		if (const auto* info = abodes::InfoOf(object); info != nullptr)
		{
			const float scale = registry.AllOf<components::Transform>(object)
			                        ? registry.Get<const components::Transform>(object).scale.x
			                        : 1.0f;
			return scale * scale * scale * info->weight;
		}
	}
	return PhysicsClassHooks::ObjectWeightOf(dynamics, object);
}

bool PhysicsGameHooks::StartPhysicsInWorld(DynamicsSystemInterface& dynamics, entt::entity object, glm::vec3 velocity,
                                           glm::vec3 spin)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Villager:
		return villager_physics::VillagerStartPhysicsInWorld(object, velocity, spin);
	case PhysicsClass::Animal:
		// the Living's: unless already flying, its state kept as its previous one (not when held), FLYING must take
		return ecs::animal_ai::InitialisePhysicsWithoutBody(object);
	default:
		return PhysicsClassHooks::StartPhysicsInWorld(dynamics, object, velocity, spin);
	}
}

bool PhysicsGameHooks::InitialisePhysicsWithoutBody(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Villager:
		return villager_physics::VillagerInitialisePhysicsWithoutBody(object);
	case PhysicsClass::Animal:
		// the Living's
		return ecs::animal_ai::InitialisePhysicsWithoutBody(object);
	default:
		return PhysicsClassHooks::InitialisePhysicsWithoutBody(dynamics, object);
	}
}

void PhysicsGameHooks::EndPhysicsWithoutBody(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Pot:
		// a hand's pile of wood or food on land becomes a land pile, put down by no interface; any other pot ends as any
		// object does
		if (_world->HandPart().PotEndPhysics(object, std::nullopt) == object)
		{
			dynamics.EndPhysicsAsObject(object);
		}
		return;
	case PhysicsClass::DeadTree:
		// the object's end (with no player, a town artifact is not taken), then the "wood here" reaction with no player;
		// a felled tree has the object's end only
		dynamics.EndPhysicsAsObject(object);
		ecs::DeadTreeEndPhysicsReaction(object, std::nullopt);
		return;
	case PhysicsClass::Scaffold:
		// calls DynamicsSystem::EndPhysicsAsObject itself
		scaffolds::OnEndPhysics(object, false);
		return;
	case PhysicsClass::Fragment:
	{
		// its own flying-object reactions go first, then the fragment's end (the object's end inside). With no body, a
		// big piece would stick to its building through the object's own matrix
		ecs::animal_ai::EndReactionsOf(object);
		const auto& transform = _world->Entities().Get<const Transform>(object);
		const auto rows = transform.rotation;
		const auto position = transform.position;
		_world->BuildingDamage().PieceAtRest(object, rows, position);
		return;
	}
	default:
		PhysicsClassHooks::EndPhysicsWithoutBody(dynamics, object);
		return;
	}
}

bool PhysicsGameHooks::InitialisePhysicsKnocked(DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	switch (dynamics.ClassOf(entry.entity))
	{
	case PhysicsClass::Villager:
		return villager_physics::VillagerInitialisePhysicsKnocked(entry.entity);
	case PhysicsClass::Animal:
		return villager_physics::AnimalInitialisePhysicsKnocked(entry.entity);
	default:
		return PhysicsClassHooks::InitialisePhysicsKnocked(dynamics, entry);
	}
}

bool PhysicsGameHooks::CanBecomePhysicsObject(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Scaffold:
		return scaffolds::CanBecomePhysicsObject(object);
	default:
		return PhysicsClassHooks::CanBecomePhysicsObject(dynamics, object);
	}
}

void PhysicsGameHooks::BeforeInitialisePhysicsFromHand(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Scaffold:
		// one part before the generic object's InitialisePhysicsFromHand
		scaffolds::OnBeforeInitialisePhysicsFromHand(object);
		return;
	case PhysicsClass::Villager:
		// the Living's: a dancer leaves its dance, then its state kept as its previous one unless IN_HAND
		villager_physics::VillagerBeforeStartFromHand(object);
		return;
	case PhysicsClass::Animal:
		ecs::animal_ai::BeforeFlightFromHand(object);
		return;
	default:
		PhysicsClassHooks::BeforeInitialisePhysicsFromHand(dynamics, object);
		return;
	}
}

void PhysicsGameHooks::ThrowFromHand(DynamicsSystemInterface& dynamics, entt::entity object, bool dontReplant)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Villager:
		// a villager dropped by force stops being its own player's disciple
		villager::DiscipleThrownFromHand(object, dontReplant);
		return;
	default:
		PhysicsClassHooks::ThrowFromHand(dynamics, object, dontReplant);
		return;
	}
}

void PhysicsGameHooks::InitialisedPhysicsFromHand(DynamicsSystemInterface& dynamics, entt::entity object,
                                                  const PhysicsEntry& entry, bool dontReplant)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Scaffold:
		// one part after the generic object's InitialisePhysicsFromHand
		scaffolds::OnInitialisePhysicsFromHand(object, entry, dontReplant);
		return;
	default:
		PhysicsClassHooks::InitialisedPhysicsFromHand(dynamics, object, entry, dontReplant);
		return;
	}
}

void PhysicsGameHooks::OfferToCatchingCreatures([[maybe_unused]] DynamicsSystemInterface& dynamics, entt::entity object,
                                                PhysicsEntry& entry)
{
	// The throw's player: who threw it, directly or through what it hit
	creature_catch_walk::Offer(*_world, object,
	                           {.centre = entry.body.Centre(),
	                            .velocity = entry.body.velocity,
	                            .thrower = entry.thrower,
	                            .player = entry.throwerPlayer});
}
