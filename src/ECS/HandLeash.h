/******************************************************************************
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

/// The hand's part of tying the leash: when the hand takes the Action button's double click, and which thing it is on.
/// When a tap is remembered as the last thing clicked is hand_click::MarksThing. The leash's own rules are in
/// ecs::leash_tie. Wiki: docs/wiki/creature/leash.md#tying-the-leash-with-the-hand.
namespace openblack::ecs::hand_leash
{

/// What the hand is busy with when a double click comes
struct HandActivity
{
	/// The land is held (the camera)
	bool gripping {false};
	/// A bubble on the screen is gripped
	bool bubbleGrip {false};
	/// A tree is being tugged out of the ground
	bool tugging {false};
	/// The grab, between the press on a thing and its pick-up or its tap
	bool grabbing {false};
	/// A locked select: scooping a pile or a field, catching fish, or holding the creature
	bool lockedSelect {false};
	/// A press that puts down or throws what the hand holds when it is let go
	bool releaseArmed {false};
	/// A spell seed's apply, on release or locked
	bool seedApply {false};
};

/// Only the idle hand takes the double click, in place of its action press. Holding something is still idle. Any other
/// action lets the double click go
[[nodiscard]] constexpr bool TakesDoubleClick(const HandActivity& activity)
{
	return !activity.gripping && !activity.bubbleGrip && !activity.tugging && !activity.grabbing && !activity.lockedSelect &&
	       !activity.releaseArmed && !activity.seedApply;
}

/// The thing a double click is on: what is under the hand, else the nearest thing near where the hand acts, else none
/// (entt::null). `nearest()` gives a std::optional<entt::entity>, and is asked only when nothing is under the hand
template <typename Nearest>
[[nodiscard]] entt::entity TieTarget(std::optional<entt::entity> underHand, Nearest&& nearest)
{
	if (underHand.has_value())
	{
		return *underHand;
	}
	if (const std::optional<entt::entity> near = nearest(); near.has_value())
	{
		return *near;
	}
	return entt::null;
}

} // namespace openblack::ecs::hand_leash
