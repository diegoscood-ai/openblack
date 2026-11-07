/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Whether a creature's body is busy with something, from the parts of the game that move it
namespace openblack::creature_body
{

/// What moves a creature's body this turn, one part of the game each
struct BodyActivity
{
	/// Its body plays an action of its own: one that plays once, a start, loop and end such as sitting, or a dance
	/// (creature_layers::IsPlaying)
	bool playing {false};
	/// It plans a route, turns on the spot or walks (CreatureLocomotionSystemInterface::IsMoving)
	bool moving {false};
	/// It plays the animations that act on a thing (picking up, knocking down, throwing, kicking, pointing...) until they
	/// end (CreatureObjectActionSystemInterface::GetProgress), or it catches a thing flying at it (IsCatching). Walking
	/// or turning up to the thing first is its moving
	bool handling {false};
	/// Its fighter is in a fight state: from the duel's start to its finish, its blocks and casts, a faint, lying
	/// fainted and getting up (creature_fight::State other than Idle)
	bool fighting {false};
};

/// The body performs an action: anything but standing free. Turning on the spot and starting a fight wait for it to be
/// over, and a computer player leaves its creature alone until it is
[[nodiscard]] constexpr bool IsPerformingAction(const BodyActivity& body)
{
	return body.playing || body.moving || body.handling || body.fighting;
}

} // namespace openblack::creature_body
