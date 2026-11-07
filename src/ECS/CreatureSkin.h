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
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CaveCopyPick.h"
#include "ECS/HandMeshRay.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::creature_skin
{

/// A creature's skin as it is drawn this frame, in the world: each sub-mesh of its own mesh the game's mesh test looks
/// at (not a physics one, of the nearest level of detail), its corners posed by the bones as drawn (else the mesh's rest
/// pose) and pulled towards the species' shapes as the body is; none without a loaded mesh
[[nodiscard]] std::vector<cave_copy_pick::Part> DrawnSkin(const Registry& registry, entt::entity creature);

/// The hand's own test of a creature's skin as it is drawn, the one that places the empty hand on it: every triangle of
/// every part, as hand_mesh_ray tests a mesh (from either side, the nearest wins, the face's normal turned away from the
/// line's start); none when the line meets none
[[nodiscard]] std::optional<hand_mesh_ray::Hit> HandFeel(std::span<const cave_copy_pick::Part> skin, const glm::vec3& origin,
                                                         const glm::vec3& direction);

} // namespace openblack::ecs::creature_skin
