/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// How a thing a creature holds sits in its hand: turned by the bone of the hand that holds it and hung below the hand's
/// grasp point by its height. docs/wiki/creature/pose-and-animation.md#which-pose-each-reader-takes
namespace openblack::creature_hold
{

/// The turn of a held thing and where its origin is
struct HeldPlacement
{
	/// Rows (glm's columns) of unit length; the thing's scale multiplies them where it is drawn
	glm::mat3 turn {1.0f};
	glm::vec3 position {0.0f};
};

/// The turn of a thing held in a hand whose bone has these rows (glm's columns): each row made of unit length, the up and
/// forward rows swapped, then the side row turned back, and the new forward row too in the right hand, the new up row in
/// the left
[[nodiscard]] glm::mat3 HeldTurn(const glm::mat3& handRows, bool leftHand);

/// The rows of a held thing at its scale: each row of its turn times the scale
[[nodiscard]] glm::mat3 ScaledRows(const glm::mat3& turn, float scale);

/// Where a held thing's origin is: below the grasp point along its up row at its scale, by its height times its hold
/// lowering. An up row of no length leaves it at the grasp point
[[nodiscard]] glm::vec3 HungBelow(const glm::vec3& grasp, const glm::vec3& scaledUp, float lowering);

/// The turn and the place of a held thing, from the rows of its hand's bone in the world, the hand's grasp point, its
/// scale and its height times its hold lowering
[[nodiscard]] HeldPlacement Place(const glm::mat3& handRows, const glm::vec3& grasp, bool leftHand, float scale,
                                  float lowering);

/// The angles about x, y and z that a held thing takes as the hand lets go of it: its rows at its scale taken apart into
/// a turn about y, then x, then z
[[nodiscard]] glm::vec3 AnglesAtRelease(const glm::mat3& handRows, bool leftHand, float scale);

} // namespace openblack::creature_hold
