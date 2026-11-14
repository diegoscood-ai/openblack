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

#include <glm/vec3.hpp>

#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/TempleDestructionWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::components
{
struct Temple;
}

namespace openblack::ecs::systems
{

class TempleDestructionSystem final: public TempleDestructionSystemInterface
{
public:
	/// In the game's own world
	TempleDestructionSystem();
	/// In a world of its own, as tests give it
	explicit TempleDestructionSystem(std::unique_ptr<temple_world::World> world);
	~TempleDestructionSystem() override;

	void Start(entt::entity heart) override;
	void DeleteByScript(entt::entity heart) override;
	void ProcessHeart(entt::entity heart) override;
	void EndTurn() override;
	entt::entity ActualObjectToEffect(entt::entity heart, bool breaksBuildings) override;

private:
	/// One turn of a heart being destroyed, in the order the game takes its steps; true at its end, when the heart is to
	/// go
	bool Step(entt::entity heart, components::Temple& temple);
	/// The beams across the heart while it beams, fired from a spot visual at its place
	void Beam(entt::entity heart, components::Temple& temple, glm::vec3 position);
	/// Whether a beam at the target (none: at a point of the heart itself) is due this turn, every two seconds
	bool BeamDue(components::Temple& temple, entt::entity target);
	/// The spot visual the heart's beams for blows and hurts are fired from; none when it could not be made
	entt::entity BeamSource(entt::entity heart, components::Temple& temple);
	/// A thing's point on the map, its height up
	[[nodiscard]] glm::vec3 TopOf(entt::entity thing) const;
	/// The heart passing a blow that breaks buildings on beams at what takes it, or at a point on itself with none
	void BeamAtTarget(entt::entity heart, components::Temple& temple, entt::entity target);
	void BeamAtItself(entt::entity heart, components::Temple& temple);
	/// Every turn, a hurt heart that would take a blow itself beams across itself, the oftener the less life it has
	void TransferredDamageEffect(entt::entity heart, components::Temple& temple);

	std::unique_ptr<temple_world::World> _world;
};

} // namespace openblack::ecs::systems
