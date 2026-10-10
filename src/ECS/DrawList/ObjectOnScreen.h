/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

/// The object draw list's on-screen test: whether an object's Draw saw it on the screen, and so whether the still-camera
/// passes call its Draw again. Pure maths on values, no Locator. docs/bw1-notes/original-frame.md, "Stage D".
namespace openblack::ecs::draw_list
{

/// The drawn camera as the on-screen test reads it in one frame
struct OnScreenView
{
	/// The world-to-clipping matrix: a point through it gives (X, Y, Z), Z the camera depth
	affine::AffineMatrix worldToClipping;
	/// This frame's drawn eye, after the camera's update
	glm::vec3 eye {0.0f};
	/// This frame's near clip. The caller computes it before the camera's update, so from the previous frame's drawn
	/// eye: the one-frame lag is the caller's, and this test only reads the value
	float nearClip {1.0f};
	/// Half the near plane's width: tan(fov / 2) times `nearClip`, rounded to a float once, from the same near clip
	float nearHalfWidth {1.0f};
	/// The viewport in pixels; half of each is the projection's scale
	glm::ivec2 screen {640, 480};
};

/// One object's inputs to the test
struct OnScreenInputs
{
	/// The bounding box's centre through the object's matrix
	glm::vec3 boxCentreWorld {0.0f};
	/// The object's own position (its matrix's translation), which the camera-inside test is made around
	glm::vec3 origin {0.0f};
	/// The bounding box's radius times the object's scale
	float radius {0.0f};
	/// The object is marked not to be drawn
	bool dontDraw {false};
};

/// Whether the object counts as on the screen at its Draw:
/// - marked not to be drawn: true;
/// - Z + r below the near clip, or NaN: false;
/// - the eye closer than r to the origin (strictly; a NaN distance goes on to the next test): true;
/// - otherwise, with i = 1 / Z worked out once and multiplied by, the disc of radius
///   rr = ((((r near) i) W) 0.5) / nearHalfWidth round the centre's sx = (X i + 1) (W 0.5), sy = (1 - Y i) (H 0.5):
///   false when sx + rr < 0, sx - rr > W, sy + rr < 0 or sy - rr > H
///   (a NaN fails the first and third, passes the second and fourth), else true, so the edges count as on the screen.
/// One float rounding per operation. The exact 1 / Z is the original's divide path; its other path's approximate
/// reciprocal is not ported. The distance cull that follows the test in the original (by level of detail) is not
/// ported either: no object that uses this test yet can vanish with distance
[[nodiscard]] bool OnScreen(const OnScreenInputs& object, const OnScreenView& view);

/// The draw list's Active flag for an object after a full pass called its Draw: on the screen at its Draw, or a human,
/// or a complex 3D object. An object with no 3D object passes false for both
[[nodiscard]] bool ActiveAfterDraw(bool onScreen, bool isHuman, bool isComplex);

} // namespace openblack::ecs::draw_list
