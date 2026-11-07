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

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The ice over a frozen body (a frozen creature, its eyes, the freeze phial): a second pass of the ice environment map,
// added over the body at the frozen amount. Every triangle takes one texture coordinate for its three corners, from its
// own face normal against the camera's axes, so the ice is flat-faceted, one shade a face. The axes are turned a little
// by a wiggle that moves on with the game time of each frame, so the facets shimmer slowly.
namespace openblack::creature_frozen_sheen
{

/// The frozen amount, 0 to 1, as a byte: times 255 in single precision, rounded to the nearest (half to even)
[[nodiscard]] uint8_t FrozenLevel(float frozen);

/// The alpha byte the ice is drawn with: the frozen level times 255, over 256
[[nodiscard]] uint8_t SheenAlpha(float frozen);

/// The colour, 0xAARRGGBB, the ice is drawn in: a piece's own colour (a frozen creature's eyeball or eyelid, the freeze
/// phial) times white with the sheen's alpha, each channel (c t) >> 8; a body's (no piece colour) white with that alpha
[[nodiscard]] uint32_t SheenColour(float frozen, std::optional<uint32_t> pieceColour);

/// The wiggle's three angles, each moved on by a frame of `frameGameMs` milliseconds of game time: the time times
/// 2 / 1000 radians, times 1, 1.1 and 1.3, each brought back into 0 to two pi. All three start at 0
[[nodiscard]] glm::vec3 StepWiggle(glm::vec3 angles, int32_t frameGameMs);

/// The rows of a turn (row-vector form: a direction times the rows)
using Rows = std::array<glm::vec3, 3>;

/// The wiggle's turn from its angles: a fiftieth of each angle's sine, the third about z, then the second about x, then
/// the first about y, worked in the original's order
[[nodiscard]] Rows WiggleTurn(glm::vec3 angles);

/// The two axes the face normals are measured against: the wiggle's first two rows, their x over the projection's x
/// scale and their y over its y scale (the clipping space back to the camera's), z as it is
struct Axes
{
	glm::vec3 x;
	glm::vec3 y;
};
[[nodiscard]] Axes FaceAxes(const Rows& wiggle, float projectionX, float projectionY);

/// A triangle's face normal from its corners, in their drawn order, in the clipping space (the camera's x and y times
/// the projection's scales, z the depth): (a - b) x (b - c), made one long with the 3D engine's inverse square root.
/// On a face that is drawn it points away from the eye
[[nodiscard]] glm::vec3 FaceNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c);

/// A face normal turned, if need be, to point away from the eye (its dot product with a corner not below 0), as the
/// drawn faces' normals are; for a normal whose winding is not known
[[nodiscard]] glm::vec3 AwayFromEye(glm::vec3 normal, glm::vec3 corner);

/// The texture coordinate of a whole face from its unit normal: for each axis, (axis . normal + 1) x 255 / 512
[[nodiscard]] glm::vec2 FaceUv(glm::vec3 normal, const Axes& axes);

} // namespace openblack::creature_frozen_sheen
