/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// The heading the start of the hand's latest hold on a creature asked its body to turn to (towards the player's eye),
/// whether the body has turned there yet or not, and even when the turn was refused. Kept until the next hold; a
/// creature never held has none, which reads as 0. The creature interaction's camera turns by it
/// (creature_interact::HoldHeading). Not saved: a creature made or loaded starts without it
struct CreatureHeldHeading
{
	float heading {0.0f};
};

} // namespace openblack::ecs::components
