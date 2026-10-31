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

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Animal;
struct CreatureCatching;
struct CreatureObjectAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureObjectActionSystem final: public CreatureObjectActionSystemInterface
{
public:
	void ProcessApproach() override;
	void ProcessTurn() override;
	void UpdateDraw(float turnFraction) override;
	void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const override;
	void UpdateHeldDraw() override;

	bool PickUp(entt::entity creature, entt::entity object) override;
	bool PickUpPortion(entt::entity creature, entt::entity source, PotInfo kind) override;
	bool PutDown(entt::entity creature) override;
	bool Discard(entt::entity creature) override;
	bool Lob(entt::entity creature) override;
	bool EatHeld(entt::entity creature) override;
	bool Keep(entt::entity creature, size_t animation) override;
	bool Throw(entt::entity creature, const glm::vec3& target) override;
	bool ThrowTaking(entt::entity creature, const glm::vec3& target, float flightSeconds) override;
	bool ThrowAtThing(entt::entity creature, const glm::vec3& target, float flightSeconds) override;
	bool Destroy(entt::entity creature, entt::entity target) override;
	bool Kick(entt::entity creature, entt::entity target) override;
	bool PointAt(entt::entity creature, const glm::vec3& point) override;
	bool PointAtFor(entt::entity creature, const glm::vec3& point, float seconds, std::optional<entt::entity> thing) override;
	void StopPointing(entt::entity creature) override;
	void Cancel(entt::entity creature) override;
	void Drop(entt::entity creature) override;
	bool Catch(entt::entity creature, entt::entity object) override;
	[[nodiscard]] bool IsCatching(entt::entity creature) const override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool mirrored) override;
	[[nodiscard]] std::optional<TakingFromHand> GetTakingFromHand(entt::entity creature) const override;
	void CallOffTake(entt::entity creature) override;
	[[nodiscard]] bool IsSetOnTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] std::optional<glm::vec3> TakePoint(entt::entity creature, glm::vec3 facing) const override;

	[[nodiscard]] State GetState(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> GetProgress(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetEating(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity object) const override;
	[[nodiscard]] bool CanPickUp(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanPickUpIgnoringMap(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanDestroy(entt::entity target) const override;
	[[nodiscard]] creature_object_actions::KindFacts KindOf(entt::entity object) const override { return KindFactsOf(object); }
	[[nodiscard]] bool CanThrowToImpress(entt::entity creature, entt::entity object) const override;

	/// How far the creature reaches to pick a thing up, in all three directions, at the size it is drawn at: from the
	/// hand's points in its pick-up animations, measured once for its body (components::CreaturePickUpReach), posed
	/// first when it has not been. None without its species' meshes
	[[nodiscard]] static std::optional<float> PickUpReach(entt::entity creature);
	/// A thing by the game's test of things in general against the creature (an animal that is not a bird, or a
	/// villager): its kind's weight, whether it is a worshipper of the creature's own player, and its own say
	[[nodiscard]] static bool PassesPickUpTest(entt::entity creature, entt::entity object, float kindWeight, bool ownWorshipper,
	                                           bool thingAllows, bool ignoringMap = false);
	/// CanPickUp, whether or not the thing is on the map when `ignoringMap`: by the rule of the thing's kind, and for a kind
	/// that takes it, by the test of things in general with the kind's weight
	[[nodiscard]] static bool PickUpTest(entt::entity creature, entt::entity object, bool ignoringMap);
	/// What kind of thing something is to a creature's hands, and what its kind says of its being picked up
	[[nodiscard]] static creature_object_actions::KindFacts KindFactsOf(entt::entity object);
	/// The weight the creature's body is to have: its size drawn, and the thin to fat and weak to strong it is becoming;
	/// nothing for what is not a creature
	[[nodiscard]] static float CreatureWeight(entt::entity creature);

	/// Something let go of at a point, with a velocity, by a creature or by nobody: out of the hand, drawn there at once,
	/// then into the physics objects, thrown at a target with the creature as its thrower or let go of as the hand lets
	/// go of things (on the map where it is when no body can be made for it)
	static void ReleaseHeld(entt::entity object, const glm::vec3& position, const glm::vec3& velocity,
	                        entt::entity creature = entt::null, bool atTarget = false);

	/// Whether the frames draw an action's animations: while it plays them, running or touching what it acts on
	[[nodiscard]] static bool IsDrawn(const components::CreatureObjectAction& action);
	/// Whether the hands' action keeps a catch from starting: one set about, walking up to it or playing, but not one
	/// over, nor one only asked for and waiting for the hands (none: nothing keeps it)
	[[nodiscard]] static bool KeepsFromCatching(const components::CreatureObjectAction* action);
	/// Whether an action is a take from the hand whose giving was called off before its moment, playing on to it
	[[nodiscard]] static bool IsCalledOffTake(const components::CreatureObjectAction& action);
	/// The animations an action is drawn playing a share of the way through the turn (0 to 1), at the rate of the size
	/// the creature is drawn at (the real speed when a script set it), each lasting as long as `durationsMs` says:
	/// pointing loops them, everything else holds their last frames
	[[nodiscard]] static std::vector<components::CreatureAnimation::Slot>
	SlotsAt(const components::CreatureObjectAction& action, float drawnSize, const std::array<float, 4>& durationsMs,
	        float share, bool realSpeed = false);

private:
	/// Starts an action on the creature in place of any it was doing, or records why it can't
	bool Start(entt::entity creature, components::CreatureObjectAction action);
	/// Starts the animations of an action on what it holds, or on the thing in front of it, once the body is free
	void BeginAtOnce(entt::entity creature, components::CreatureObjectAction& action);
	/// Lets go of what the creature holds, at a point, with a velocity, thrown at a target or not; turned by the hand's bone
	/// when it is put down, tossed away or thrown
	static void Release(entt::entity creature, const glm::vec3& position, const glm::vec3& velocity, bool atTarget = false,
	                    bool turnedByHand = false);
	/// What happens at an action's moment: something taken hold of, knocked down, let go of or eaten
	void Moment(entt::entity creature);
	/// The end of a meal, once the eating is over: what was eaten is used up, then judged, and the creature's mind told
	void EndMeal(entt::entity creature, entt::entity meal) const;
	/// A turn of each catch under way: its end when its animation is over, then the waiting, the step or the reach
	void ProcessCatches();
	/// The slots a catch is drawn with a share of the way through the turn: none while it waits, the step, or the four
	/// catching animations
	[[nodiscard]] static std::vector<components::CreatureAnimation::Slot>
	CatchSlotsAt(const components::CreatureCatching& catching, float share);
};

} // namespace openblack::ecs::systems
