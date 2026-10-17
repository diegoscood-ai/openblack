/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

// UR_ScaleByCameraDist's maths: an effect a player owns sizes each atom of the rule's group by the atom's distance from
// the camera. Wiki: docs/wiki/physics/particles.md#the-camera-distance-scale.

namespace openblack::psys::camera_scale
{
/// The rule's properties, with their defaults
struct Range
{
	float distMin {10.0f};   ///< CameraDistMin
	float distMax {100.0f};  ///< CameraDistMax
	float scaleAtMin {0.1f}; ///< ScaleAtCameraDistMin
	float scaleAtMax {1.0f}; ///< ScaleAtCameraDistMax
};

/// Whether the rule runs this step. When it doesn't (the two distances are equal or either is not a number, or no
/// player owns the effect), it leaves every atom as it is and detaches from the collection
[[nodiscard]] bool Runs(const Range& range, bool ownedByPlayer);

/// The distance from the camera to a point, summed as the game does: (dz^2 + dy^2) + dx^2, each difference the camera's
/// minus the point's
[[nodiscard]] float CameraDistance(const glm::vec3& camera, const glm::vec3& point);

/// The atom's rule scale at a distance: scaleAtMin below distMin (a NaN too), scaleAtMax above distMax, else the
/// straight line between them, (d - distMin) / (distMax - distMin) x (scaleAtMax - scaleAtMin) + scaleAtMin. distMin
/// is tested first, so with distMin above distMax a distance below distMin takes scaleAtMin
[[nodiscard]] float ScaleAtDistance(const Range& range, float distance);
} // namespace openblack::psys::camera_scale
