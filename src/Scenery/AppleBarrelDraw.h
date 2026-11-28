/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Particles/Creators/Mesh.h"

// The apple barrel's draw (Scenery/AppleBarrelRules.h): the barrel and its worm, drawn at once at the end of the land's
// object draw, each in the land's light and haze where the barrel stands, the worm posed by its clip. Neither is an
// object of the game, so they go to the renderer as two drawn meshes of their own (particles::mesh_atoms::Instance),
// as raffclar's draw of them (his d8b2574e8) on his own registry of drawn things.
// Wiki: docs/wiki/rendering/frame.md#the-apple-barrel

namespace openblack::apple_barrel
{

/// The barrel at its place and the worm at its own, both unturned and at their own size, taking the land's light alone
/// (no colour of their own) with the haze, at the barrel's place; the worm with its bones as its clip poses them
[[nodiscard]] std::array<particles::mesh_atoms::Instance, 2> DrawnMeshes(entt::id_type barrelMesh, entt::id_type wormMesh,
                                                                         const glm::vec3& barrel, const glm::vec3& worm,
                                                                         std::vector<glm::mat4> wormPose);

/// The barrel and the worm the apple barrel system placed this frame (none while no pit carries them), the worm posed at
/// its place in its clip; none if their meshes or the worm's clip did not load
[[nodiscard]] std::vector<particles::mesh_atoms::Instance> DrawnThisFrame();

} // namespace openblack::apple_barrel
