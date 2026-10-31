/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>

#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Systems/DynamicsSystemInterface.h"

namespace openblack::ecs
{

/// What the game's own kinds of thing do in the physics, beyond what any object does, each for its class
/// (DynamicsSystemInterface::ClassOf): villagers and animals fly when thrown or knocked, are hurt by hard knocks and
/// land, drown or die where they come down; trees, dead trees and pots go into the stores they hit, and trees take root
/// again or fall dead; scaffolds move with their workshop and become buildings; a creature stands as a heavy body.
class PhysicsGameHooks: public systems::PhysicsClassHooks
{
public:
	/// The game's kinds of thing in the game's own world
	PhysicsGameHooks();
	/// The same in the world given
	explicit PhysicsGameHooks(std::unique_ptr<physics_hooks::World> world);

	void StartPhysics(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry, bool fromHand) override;
	std::optional<bool> ReactToImpact(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry,
	                                  const ImpactInfo& impact) override;
	std::optional<entt::entity> EndPhysics(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry) override;
	[[nodiscard]] bool CallsBackInMap(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	void Moved(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	void DropSound(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	bool SetUpBody(systems::DynamicsSystemInterface& dynamics, entt::entity object, openblack::physics::Body& body) override;
	std::optional<float> BodyMassOf(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	std::optional<float> ObjectWeightOf(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	bool InitialisePhysicsWithoutBody(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	bool StartPhysicsInWorld(systems::DynamicsSystemInterface& dynamics, entt::entity object, glm::vec3 velocity,
	                         glm::vec3 spin) override;
	void EndPhysicsWithoutBody(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	bool InitialisePhysicsKnocked(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry) override;
	[[nodiscard]] bool CanBecomePhysicsObject(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	void ThrowFromHand(systems::DynamicsSystemInterface& dynamics, entt::entity object, bool dontReplant) override;
	void BeforeInitialisePhysicsFromHand(systems::DynamicsSystemInterface& dynamics, entt::entity object) override;
	void InitialisedPhysicsFromHand(systems::DynamicsSystemInterface& dynamics, entt::entity object, const PhysicsEntry& entry,
	                                bool dontReplant) override;
	void OfferToCatchingCreatures(systems::DynamicsSystemInterface& dynamics, entt::entity object,
	                              PhysicsEntry& entry) override;

private:
	std::unique_ptr<physics_hooks::World> _world;
};

} // namespace openblack::ecs
