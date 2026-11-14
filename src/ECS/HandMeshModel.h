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
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/LandMorph.h"
#include "ECS/HandMeshRay.h"

namespace openblack::ecs
{
class Registry;
namespace components
{
struct Transform;
}
} // namespace openblack::ecs

/// Where the hand's pick finds an object's mesh drawn, and the hand's own test of that mesh (ecs::hand_mesh_ray) along
/// a line: the same matrices for the cursor's pick and for the hand feeling the object. Defined with the hand's pick
/// (HandSystem::PickObjectAlongRay, in HandPlacement.cpp).
namespace openblack::ecs::hand_mesh_model
{

/// An object's origin as the pick takes it: one that follows the land stands on the ground under it, sunk by a pile's
/// sink, any other at its own position
[[nodiscard]] glm::vec3 GroundedPosition(const Registry& registry, entt::entity entity, const components::Transform& transform,
                                         const land_morph::Ground& ground);

/// The turn, the point and the matrix an object's mesh is drawn with, from its grounded origin: a one-shot orb is turned
/// to the camera every frame and moved by its facing offset
struct Drawn
{
	glm::mat3 rotation;
	glm::vec3 at;
	glm::mat4 model;
};
[[nodiscard]] Drawn ObjectModel(const Registry& registry, entt::entity entity, const components::Transform& transform,
                                glm::vec3 position);

/// A temple entrance's origin, on the land as the flattening under the temple left it, and the matrix its own mesh is
/// collided with: turned as its heart, at its own scale
[[nodiscard]] glm::vec3 EntrancePosition(const components::Transform& transform, const land_morph::Ground& ground);
[[nodiscard]] glm::mat4 EntranceModel(const components::Transform& transform, glm::vec3 position);

/// The hand's own test of an object's mesh along a line, the mesh where the pick finds it: an object's own mesh, or a
/// temple entrance's. None for an object with neither, or whose mesh is not loaded
[[nodiscard]] std::optional<hand_mesh_ray::Hit> Feel(entt::entity entity, glm::vec3 origin, glm::vec3 direction);

} // namespace openblack::ecs::hand_mesh_model
