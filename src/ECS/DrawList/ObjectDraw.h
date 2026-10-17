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

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/DrawList/ObjectOnScreen.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Transform;
}

// The Draw the object draw list calls for an object with no Draw of its own: its mesh's on-screen test, the flag the
// list keeps for the still passes. It draws nothing: the renderer draws the meshes.
namespace openblack::ecs::draw_list
{

/// The on-screen inputs of a mesh's bounding box under the object's Transform: the box's centre through the object's
/// model, and half the box's diagonal times the largest scale. (approximate) openblack's model and radius, not the
/// original's box
[[nodiscard]] OnScreenInputs OnScreenInputsFrom(const components::Transform& transform, const AxisAlignedBoundingBox& box,
                                                bool dontDraw);

/// The object's on-screen inputs from its Transform, its Mesh's bounding box in the mesh cache and its DontDraw mark.
/// None when it has no Transform or Mesh, or the cache does not hold the mesh
[[nodiscard]] std::optional<OnScreenInputs> OnScreenInputsOf(const Registry& registry, entt::entity object);

/// The generic Draw's on-screen flag: the test's result, or true when there is no mesh to test, as a Draw that returns
/// before its test leaves the flag the list set
[[nodiscard]] bool DrawOnScreen(const std::optional<OnScreenInputs>& inputs, const OnScreenView& view);

} // namespace openblack::ecs::draw_list
