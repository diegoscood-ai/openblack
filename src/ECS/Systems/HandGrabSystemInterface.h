/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{

/// The player's hand picking things up, carrying them and letting them go: putting them down where it is, or throwing
/// them with the speed its spring drags it at. Miracles in the hand and creatures held to it are the magic's and the
/// creature hand's.
class HandGrabSystemInterface
{
public:
	virtual ~HandGrabSystemInterface() = default;

	/// The hand is made to let go of what it holds, which is put down where it is and never planted again
	virtual void ForceDrop() = 0;
	/// A creature takes a thing the hand gave it: the hand lets go of it into the creature's hand, without dropping it.
	/// Whether the hand still held it to give.
	virtual bool HandOver(entt::entity /*object*/) { return false; }
	/// For the debug tools: the empty hand takes a thing at once, as if picked up from where it stands; and gives what it
	/// holds to a creature, as the Action button held over the creature does. Whether it could.
	virtual bool TakeIntoHand(entt::entity /*object*/) { return false; }
	virtual bool GiveHeldTo(entt::entity /*creature*/) { return false; }
	/// The creature the hand offers what it holds to, while it does; the creature reaching for it once given
	[[nodiscard]] virtual std::optional<entt::entity> GetOfferedCreature() const { return std::nullopt; }
	/// How far round what the hand holds is turned as it is offered, rolled with the hand about the line to the camera,
	/// in radians
	[[nodiscard]] virtual float GetOfferTurn() const { return 0.0f; }

	/// What the hand holds, none when it holds nothing of its own
	[[nodiscard]] virtual std::optional<entt::entity> GetHeld() const = 0;
};

} // namespace openblack::ecs::systems
