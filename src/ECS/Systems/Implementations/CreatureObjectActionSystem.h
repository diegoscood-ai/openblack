/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/CreatureBody.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct CreatureObjectAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureObjectActionSystem final: public CreatureObjectActionSystemInterface
{
public:
	void ProcessTurn() override;
	void UpdateDraw(float turnFraction) override;
	void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const override;
	void UpdateHeldDraw() override;

	bool PickUp(entt::entity creature, entt::entity object) override;
	bool PutDown(entt::entity creature) override;
	bool Discard(entt::entity creature) override;
	bool Lob(entt::entity creature) override;
	bool EatHeld(entt::entity creature) override;
	bool Keep(entt::entity creature, size_t animation) override;
	bool Throw(entt::entity creature, const glm::vec3& target) override;
	bool Destroy(entt::entity creature, entt::entity target) override;
	bool PointAt(entt::entity creature, const glm::vec3& point) override;
	void Cancel(entt::entity creature) override;
	void Drop(entt::entity creature) override;

	[[nodiscard]] State GetState(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> GetProgress(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity object) const override;
	[[nodiscard]] bool CanPickUp(entt::entity object) const override;
	[[nodiscard]] bool CanDestroy(entt::entity target) const override;

	/// Something let go of at a point, with a velocity, by a creature or by nobody: out of the hand, drawn there at once,
	/// then into the physics objects, thrown at a target with the creature as its thrower or let go of as the hand lets
	/// go of things (on the map where it is when no body can be made for it)
	static void ReleaseHeld(entt::entity object, const glm::vec3& position, const glm::vec3& velocity,
	                        entt::entity creature = entt::null, bool atTarget = false);

	/// Whether the frames draw an action's animations: while it plays them, running or touching what it acts on
	[[nodiscard]] static bool IsDrawn(const components::CreatureObjectAction& action);
	/// The animations an action is drawn playing a share of the way through the turn (0 to 1), at the rate of the size
	/// the creature is drawn at, each lasting as long as `durationsMs` says: pointing loops them, everything else holds
	/// their last frames
	[[nodiscard]] static std::vector<components::CreatureAnimation::Slot>
	SlotsAt(const components::CreatureObjectAction& action, float drawnSize, const std::array<float, 4>& durationsMs,
	        float share);

private:
	/// Starts an action on the creature in place of any it was doing, or records why it can't
	bool Start(entt::entity creature, components::CreatureObjectAction action);
	/// Lets go of what the creature holds, at a point, with a velocity, thrown at a target or not
	static void Release(entt::entity creature, const glm::vec3& position, const glm::vec3& velocity, bool atTarget = false);
	/// What happens at an action's moment: something taken hold of, knocked down, let go of or eaten
	void Moment(entt::entity creature);
};

} // namespace openblack::ecs::systems
