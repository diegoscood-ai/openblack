/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/VillagerDrowning.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

void PhysicsClassHooks::StartPhysics([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] PhysicsEntry& entry,
                                     [[maybe_unused]] bool fromHand)
{
}

std::optional<bool> PhysicsClassHooks::ReactToImpact([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                     [[maybe_unused]] PhysicsEntry& entry,
                                                     [[maybe_unused]] const ImpactInfo& impact)
{
	return std::nullopt;
}

std::optional<entt::entity> PhysicsClassHooks::EndPhysics([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                          [[maybe_unused]] PhysicsEntry& entry)
{
	return std::nullopt;
}

bool PhysicsClassHooks::CallsBackInMap([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object)
{
	return false;
}

bool PhysicsClassHooks::HasSunk([[maybe_unused]] DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	return ecs::HasSunk(entry.entity);
}

void PhysicsClassHooks::Moved([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object) {}

void PhysicsClassHooks::DropSound([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object) {}

bool PhysicsClassHooks::SetUpBody([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object,
                                  [[maybe_unused]] openblack::physics::Body& body)
{
	return false;
}

std::optional<float> PhysicsClassHooks::BodyMassOf([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                   [[maybe_unused]] entt::entity object)
{
	return std::nullopt;
}

std::optional<float> PhysicsClassHooks::ObjectWeightOf([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                       [[maybe_unused]] entt::entity object)
{
	return std::nullopt;
}

bool PhysicsClassHooks::InitialisePhysicsWithoutBody([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                     [[maybe_unused]] entt::entity object)
{
	return true;
}

bool PhysicsClassHooks::StartPhysicsInWorld([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                            [[maybe_unused]] entt::entity object, [[maybe_unused]] glm::vec3 velocity,
                                            [[maybe_unused]] glm::vec3 spin)
{
	return true;
}

void PhysicsClassHooks::EndPhysicsWithoutBody(DynamicsSystemInterface& dynamics, entt::entity object)
{
	switch (dynamics.ClassOf(object))
	{
	case PhysicsClass::Villager:
	case PhysicsClass::Animal:
		villager_physics::EndPhysicsWithoutBody(object);
		return;
	case PhysicsClass::Tree:
		// the tree's end returns at once while it is carried
		return;
	default:
		dynamics.EndPhysicsAsObject(object);
		return;
	}
}

bool PhysicsClassHooks::InitialisePhysicsKnocked(DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	return dynamics.CanWakeKnockedProxy(entry.entity);
}

void PhysicsClassHooks::ThrowFromHand([[maybe_unused]] DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object,
                                      [[maybe_unused]] bool dontReplant)
{
}

bool PhysicsClassHooks::CanBecomePhysicsObject([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                               [[maybe_unused]] entt::entity object)
{
	return true;
}

void PhysicsClassHooks::BeforeInitialisePhysicsFromHand([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                        [[maybe_unused]] entt::entity object)
{
}

void PhysicsClassHooks::InitialisedPhysicsFromHand([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                   [[maybe_unused]] entt::entity object,
                                                   [[maybe_unused]] const PhysicsEntry& entry,
                                                   [[maybe_unused]] bool dontReplant)
{
}

void PhysicsClassHooks::OfferToCatchingCreatures([[maybe_unused]] DynamicsSystemInterface& dynamics,
                                                 [[maybe_unused]] entt::entity object, [[maybe_unused]] PhysicsEntry& entry)
{
}
