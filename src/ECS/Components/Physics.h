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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
/// The object is in the physics: thrown, dropped, knocked or pushed, it flies, slides or floats there until it comes to
/// rest, and is out of the map's cells meanwhile. An object a tornado or a vortex carries is in the physics too, with no
/// body (it has CarriedByTornado as well). The dynamics system and the carry keep it in step with their lists: it is
/// there exactly while the object has a moving body or is carried. A resting body that only waits to be hit is not in
/// the physics
struct InPhysics
{
};

/// The pose a flying physics object is drawn at this frame (written into its 3D object's matrix):
/// between the body's pose at the start of the last game turn and at its end, by the turn fraction. Only on objects
/// that move in the physics (ECS/Systems/Implementations/DynamicsSystem), and on the held object while the hand's release
/// prediction is drawn (DynamicsSystem::SetPrediction); the drawing takes it before DrawPosition and Transform. The scale is
/// the body's, the one the physics took when the object went in, as the original's drawing scales the rows by it; the
/// Transform's where it has none (the prediction's, a creature's held object).
struct PhysicsDrawPose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	std::optional<float> scale;
};
} // namespace openblack::ecs::components
