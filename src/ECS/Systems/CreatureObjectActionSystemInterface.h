/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct CreatureAnimationInputs;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

/// What the creatures do with the things about them (see components::CreatureObjectAction): they walk up to things and
/// pick them up, hold them in their hands, look them over, eat them, put them down, toss them away or throw them, knock
/// them down, and point at them. The creatures' towns watch what they
/// do with fear or respect. Minds and the debug tools tell the creatures what to do.
class CreatureObjectActionSystemInterface
{
public:
	/// How the creature's last action went
	enum class State : uint8_t
	{
		/// It hasn't been told to do anything
		Idle,
		Busy,
		Done,
		Failed,
	};

	virtual ~CreatureObjectActionSystemInterface() = default;

	/// Once a game turn, after the minds and before the creatures move: what is let go of falls, and creatures set about
	/// what they act on, walking up to it, turning to it or starting the hands' animations. A walk asked here begins in
	/// this turn's move, and one that ends in it lets the hands start the turn after
	virtual void ProcessApproach() {}
	/// Once a game turn, after the creatures move: what is let go of falls, a turn to face what they act on that ended
	/// goes straight on into it, the actions play on by a turn, things are taken hold of and let go of at their
	/// moments, what is held rides in the hand, what they carry makes them stronger, and their towns' views of them
	/// change
	virtual void ProcessTurn() = 0;
	/// Once a frame, before the animation poses the bodies, by how far through the turn the frame is (0 to 1): the
	/// actions' animations are set a share of the way through the turn. It changes no state.
	virtual void UpdateDraw(float turnFraction) = 0;
	/// The animations UpdateDraw sets a creature playing at a share of the turn, written into `inputs` as UpdateDraw
	/// writes them into its animation: nothing for a creature that isn't playing an action. Nothing is changed
	virtual void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const = 0;
	/// Once a frame, after the animation has posed the bodies: what each creature holds is drawn in its hand where the
	/// body was posed this frame. It changes no state.
	virtual void UpdateHeldDraw() = 0;

	/// Each returns whether the creature could start. Picking up and knocking down need empty hands, all those that act
	/// on what it holds need something held.
	virtual bool PickUp(entt::entity creature, entt::entity object) = 0;
	/// Makes a portion of food of a kind (wheat, fruit) from a source, as much as the creature's drawn size eats and no
	/// more than the source holds, and picks it up; the source loses what the portion holds as the hand takes hold
	virtual bool PickUpPortion(entt::entity creature, entt::entity source, PotInfo kind) = 0;
	virtual bool PutDown(entt::entity creature) = 0;
	virtual bool Discard(entt::entity creature) = 0;
	virtual bool Lob(entt::entity creature) = 0;
	virtual bool EatHeld(entt::entity creature) = 0;
	/// Strokes, shakes, smells or examines what it holds, by the animation of that
	virtual bool Keep(entt::entity creature, size_t animation) = 0;
	virtual bool Throw(entt::entity creature, const glm::vec3& target) = 0;
	/// Throws what it holds at a point, to take the given seconds to get there; without its own way of doing so, as a
	/// throw that takes as long as a fall
	virtual bool ThrowTaking(entt::entity creature, const glm::vec3& target, float /*flightSeconds*/)
	{
		return Throw(creature, target);
	}
	virtual bool Destroy(entt::entity creature, entt::entity target) = 0;
	/// Kicks a thing at its feet, which flies off ahead of it as the foot meets it; fails when the thing is in its own or
	/// the player's hand, or the species has no kick. Without its own way of kicking, it can't
	virtual bool Kick(entt::entity /*creature*/, entt::entity /*target*/) { return false; }
	virtual bool PointAt(entt::entity creature, const glm::vec3& point) = 0;
	/// Points for some seconds at a point, or at a thing wherever it goes
	virtual bool PointAtFor(entt::entity creature, const glm::vec3& point, float /*seconds*/,
	                        std::optional<entt::entity> /*thing*/)
	{
		return PointAt(creature, point);
	}
	/// A creature that points stops pointing now, its hold over; pointing that has not started yet, and anything else it
	/// does, go on
	virtual void StopPointing(entt::entity /*creature*/) {}
	/// Stops what it is doing, keeping hold of whatever it holds
	virtual void Cancel(entt::entity creature) = 0;
	/// Lets go of what it holds at once, dropping it from the hand
	virtual void Drop(entt::entity creature) = 0;
	/// Sets about catching something flying at it, its body free: whether it could start. Its hands' part is then done,
	/// and its body carries the catch on by itself (IsCatching)
	virtual bool Catch(entt::entity /*creature*/, entt::entity /*object*/) { return false; }
	/// Whether its body is taken up by a catch: turning to the thing, waiting, stepping across or reaching
	[[nodiscard]] virtual bool IsCatching(entt::entity /*creature*/) const { return false; }
	/// Reaches out to take a thing the player's hand gives it, with its left hand when mirrored: the thing leaves the
	/// hand for the creature's at the moment of taking, if the hand still gives it then
	virtual bool TakeFromHand(entt::entity /*creature*/, entt::entity /*object*/, bool /*mirrored*/) { return false; }
	/// While it reaches for a thing in the player's hand: the thing, and how many milliseconds its animation is from the
	/// moment it takes it (less than nothing before)
	struct TakingFromHand
	{
		entt::entity object {entt::null};
		float msFromTake {0.0f};
	};
	[[nodiscard]] virtual std::optional<TakingFromHand> GetTakingFromHand(entt::entity /*creature*/) const
	{
		return std::nullopt;
	}
	/// The giving is called off while it reaches for the thing, before it takes it: its hand plays on to the moment of
	/// taking and closes on nothing there, whatever else stops what it does
	virtual void CallOffTake(entt::entity /*creature*/) {}
	/// Whether its hands are set on taking from the player's hand: asked for, waiting for its body, or under way
	[[nodiscard]] virtual bool IsSetOnTakingFromHand(entt::entity /*creature*/) const { return false; }
	/// Where its right hand takes a thing from the player's hand, turned to face a point
	[[nodiscard]] virtual std::optional<glm::vec3> TakePoint(entt::entity /*creature*/, glm::vec3 /*facing*/) const
	{
		return std::nullopt;
	}

	[[nodiscard]] virtual State GetState(entt::entity creature) const = 0;
	/// How far through its animations the action is, 0 to 1, while it plays them
	[[nodiscard]] virtual std::optional<float> GetProgress(entt::entity creature) const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> GetHeld(entt::entity creature) const = 0;
	/// What the creature is eating: the thing of its eating action, from the start of that action while the thing is
	/// there, until another action takes its place. Nothing when it is doing something else
	[[nodiscard]] virtual std::optional<entt::entity> GetEating(entt::entity /*creature*/) const { return std::nullopt; }
	/// What something is worth to eat, if anything
	[[nodiscard]] virtual std::optional<float> FoodValueOf(entt::entity object) const = 0;
	/// Whether the creature can pick something up: things and villagers, not the land's fixtures nor other creatures;
	/// a bird by its own test, another animal by the game's test of things in general, against this creature
	[[nodiscard]] virtual bool CanPickUp(entt::entity creature, entt::entity object) const = 0;
	/// The same test, whether or not the thing is on the map, as for a thing the player's hand holds out to it
	[[nodiscard]] virtual bool CanPickUpIgnoringMap(entt::entity /*creature*/, entt::entity /*object*/) const { return false; }
	/// What kind of thing something is to a creature's hands, and what its kind says of its being picked up
	[[nodiscard]] virtual creature_object_actions::KindFacts KindOf(entt::entity /*object*/) const { return {}; }
	/// Whether a creature may throw a thing to impress, by its kind, its height and the creature's weight
	[[nodiscard]] virtual bool CanThrowToImpress(entt::entity /*creature*/, entt::entity /*object*/) const { return false; }
	/// Whether a creature can knock something down: trees, homes and things, statics and rocks among them
	[[nodiscard]] virtual bool CanDestroy(entt::entity target) const = 0;
};

} // namespace openblack::ecs::systems
