/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFeedback.h"

namespace openblack::creature_interact
{
enum class Holder : uint8_t;
} // namespace openblack::creature_interact

namespace openblack::ecs::systems
{

/// The player's hand on a creature (see components::HandOnCreature). The hand can't pick a creature up: an empty hand
/// pressed on the player's own creature takes hold of it (creature_hand::TakesPress) once the next turn locks it for the
/// hand. Resting on its body the hand strokes it, swept fast across it the hand slaps it, and the creature reacts to each
/// at once. When the hand lets go, how the creature was treated, from -1 to 1, goes to its mind at the next turn.
class CreatureHandSystemInterface
{
public:
	/// Where the hand is drawn while held to a creature, and how it is posed
	struct HandPose
	{
		glm::vec3 position;
		/// Resting on the body rather than beside it
		bool onBody;
		/// The hand's cycle: wiggling beside the body, caressing it or slapping
		creature_feedback::TouchCycle cycle;
		/// How much larger the hand is drawn, with the creature's size
		float scale;
		/// How many times this frame's change of cycle starts the hand's blend afresh
		uint8_t blendStarts {0};
	};
	/// A creature a line of sight meets, and how far along it
	struct CreatureHit
	{
		entt::entity creature;
		float distance;
	};
	/// How the hand treated the creature it let go of, for its mind
	struct Feedback
	{
		entt::entity creature;
		/// From -1 to 1
		float feedback;
	};

	virtual ~CreatureHandSystemInterface() = default;

	/// The hand's locked select reached the creature: whether the hand now holds it. A creature that does not let the
	/// hand hold it (MayHold) is refused.
	virtual bool Grab(entt::entity creature) = 0;
	/// Whether the creature lets the player's hand take hold of it to stroke and slap it
	[[nodiscard]] virtual bool MayHold(entt::entity creature) const = 0;
	/// Once a frame while held to a creature, with the line of sight through the cursor, the cursor on the screen, the
	/// screen's width and the hand's seconds of camera time: strokes and slaps, and where the hand is (kept for Pose)
	virtual std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                                       float screenWidth, float seconds) = 0;
	/// The hand lets go: the creature it held and how it was treated, which the hand sends on to its mind; nothing when it
	/// held none
	[[nodiscard]] virtual std::optional<Feedback> Release() = 0;
	/// Where the hand was drawn by the last Update while held to a creature; nothing when it is not held to one
	[[nodiscard]] virtual std::optional<HandPose> Pose() const = 0;

	/// The creature the hand is held to, if any
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The nearest creature along a line of sight, by its posed body
	[[nodiscard]] virtual std::optional<CreatureHit> CreatureAlong(const glm::vec3& rayOrigin,
	                                                               const glm::vec3& rayDirection) const = 0;
	/// The creature under the hand this frame, from the hand's own pick, as the hover and the press see it
	[[nodiscard]] virtual std::optional<entt::entity> CreatureUnderHand() const = 0;
	/// The hand's update stores the creature its pick found under it, or none
	virtual void SetCreatureUnderHand(std::optional<entt::entity> creature) = 0;
	/// As if the hand rested on a part of a creature's body, or swept across it at a height (as a share of its height)
	/// hard or gently: it takes hold of the creature, held by a command rather than the button, and strokes or slaps it
	/// as the button would. Returns whether the creature reacted, which is when the stroke or slap counts.
	virtual bool Stroke(entt::entity creature, creature_feedback::BodyPart part) = 0;
	virtual bool Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight) = 0;
	/// Whether the hand was put on the creature by a command, so the button letting go leaves it held
	[[nodiscard]] virtual bool IsHeldByCommand() const = 0;
	/// How the creature has been treated since the hand took hold, from -1 to 1
	[[nodiscard]] virtual float GetFeedbackSum() const = 0;
	/// While held to a creature the same, and after letting go the sum it let go with, until it takes hold again
	[[nodiscard]] virtual float GetLastFeedbackSum() const = 0;

	/// Once a frame while the hand interacts with a creature: the camera turns to look at it from in front, taking over
	/// the first frame from the camera that has the view if that one can be left (the player's own, Creature Mode's, a
	/// miracle's placed path or a script's dual camera: creature_interact::CanBeLeft), and follows it
	virtual void FollowWithCamera(entt::entity creature) = 0;
	/// The interaction is over: the camera it took over from has the view again from where the camera is, the player's
	/// flying back to where it was going, Creature Mode's following its creature again, a placed path or a dual camera
	/// moving it again. When the hold asked for the camera and did not get it, the camera that kept the view is taken
	/// off instead (creature_interact::AtTheEndOfTheHold), and the one under it has the view
	virtual void GiveCameraBack() = 0;
	/// While the interaction's camera has the view, the camera it took the view from
	[[nodiscard]] virtual std::optional<creature_interact::Holder> CameraTakenFrom() const = 0;
	/// The camera the end of the last hold that did not get the view took off (creature_interact::TakenOffAtTheEnd), if
	/// any: the debug readout
	[[nodiscard]] virtual std::optional<creature_interact::Holder> LastTakenOff() const = 0;
	/// The debug switch of the interaction camera, on by default
	virtual void SetInteractionCamera(bool on) = 0;
	[[nodiscard]] virtual bool InteractionCamera() const = 0;
};

} // namespace openblack::ecs::systems
