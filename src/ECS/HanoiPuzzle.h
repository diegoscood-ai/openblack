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

#include <entt/entity/fwd.hpp>

/// The beach temple puzzle (PuzzleGame type 3): its pieces, its turn and the scripts' test of it. The rules themselves
/// are in ECS/HanoiRules.h. See docs/wiki/world/beach-temple-puzzle.md
namespace openblack::ecs::hanoi_puzzle
{

/// The puzzle made: its grid's steps from its angle and size, doubled, then the three bases in a row along its angle
/// and the four rings stacked on the first base, the largest at the bottom
void Start(entt::entity puzzle);
/// The test of a solution: each ring on the next larger one and the largest on neither of the first two bases. A
/// solved puzzle is played, and its rings can no longer be picked up or moved
[[nodiscard]] bool TestSolution(entt::entity puzzle);
/// One turn of an unsolved puzzle: a base off its place, or gone, is put back; a ring that went is made again; a ring
/// off its place and not in a hand goes onto the stack it is near, or, after a while, back to its place
void Process(entt::entity puzzle);
/// The puzzle has gone: its rings belong to no puzzle any more
void Forget(entt::entity puzzle);

/// Whether a piece of a beach temple puzzle refuses the hand: a base, a piece with another on it, a piece of no
/// puzzle, or any piece while its puzzle has had fewer than five calm turns. False for anything that is no piece
[[nodiscard]] bool RefusesPickUp(entt::entity thing);
/// A piece taken into a hand comes off the piece it rested on and loses its links; the largest ring says one of eight
/// lines. Nothing for anything that is no piece
void TakenInHand(entt::entity thing);
/// Whether a piece may become a physics object: a base never, a ring only while it rests on nothing. None for anything
/// that is no piece
[[nodiscard]] std::optional<bool> CanBecomePhysicsObject(entt::entity thing);

} // namespace openblack::ecs::hanoi_puzzle
