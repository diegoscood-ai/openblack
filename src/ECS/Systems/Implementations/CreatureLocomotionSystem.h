/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureLocomotionSystem final: public CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float turnFraction) override;
	void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const override;

	/// The animations the legs play a share of the way through the turn (0 to 1): each track's time that far on from
	/// where it was as the turn started, wrapped round when it loops and held at its last frame when it doesn't, or the
	/// breath's point (`breathPhase`) for a track played as standing is
	[[nodiscard]] static std::vector<components::CreatureAnimation::Slot>
	SlotsAt(std::span<const components::CreatureLocomotion::Track> tracks, float breathPhase, float share);

	MoveResult MoveTo(entt::entity creature, glm::vec2 point, Pace pace, float minDistance, float maxDistance) override;
	MoveResult LeadTo(entt::entity creature, glm::vec2 point, float pull, float maxDistance) override;
	MoveResult WalkBack(entt::entity creature, glm::vec2 point, float hurry, float maxDistance) override;
	MoveResult MoveToObject(entt::entity creature, entt::entity target, Pace pace, float extra) override;
	MoveResult Follow(entt::entity creature, entt::entity target, float distance, Pace pace) override;
	MoveResult FleeFrom(entt::entity creature, glm::vec2 threat) override;
	bool TurnToFace(entt::entity creature, glm::vec2 point) override;
	void Stop(entt::entity creature) override;
	[[nodiscard]] bool IsMoving(entt::entity creature) const override;
	[[nodiscard]] bool IsValidPosition(glm::vec2 point, float radius) const override;

private:
	/// Starts a move for a fraction of top speed, the creature's state already gathered
	MoveResult StartMove(entt::entity creature, components::CreatureLocomotion& locomotion, glm::vec2 point, float fraction,
	                     float minDistance, float maxDistance);
};

} // namespace openblack::ecs::systems
