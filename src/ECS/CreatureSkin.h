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
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CaveCopyPick.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureSkinRay.h"
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

/// Where on its skins a line cut into a creature meets it, as an effect's cut finds it: the line from `from` along
/// `direction`, either way along it, against every triangle of each sub-mesh of its own mesh of the nearest level of
/// detail, its corners posed by the bones of the pose the game reads (creature_turn_pose::Safe) and shaped as the body
/// is; the nearest triangle whose place on it can be worked out across x and y (screen_pick::NearestSkinIntersection).
/// The texel is then read from the first sub-mesh's primitive of the same number, at the same corners, and the skin is
/// that primitive's. None when the line meets nothing, or the creature has no mesh or no pose yet
struct SkinPlace
{
	uint8_t u {0};
	uint8_t v {0};
	uint8_t skin {0};
};
[[nodiscard]] std::optional<SkinPlace> CutPlace(const Registry& registry, entt::entity creature, const glm::vec3& from,
                                                const glm::vec3& direction);

/// A spot on a creature's mesh, and one with its sub-mesh (creature_marks::MeshSpot, creature_marks::MeshCut)
using MeshSpot = creature_marks::MeshSpot;
using MeshCut = creature_marks::MeshCut;

/// A spot the line met on a creature's drawn skin (creature_skin_ray, on DrawnSkin's parts) as its mesh numbers it;
/// none when its mesh is not loaded or no longer holds the spot
[[nodiscard]] std::optional<MeshSpot> SpotOnMesh(const Registry& registry, entt::entity creature,
                                                 const creature_skin_ray::Spot& spot);

/// Where on a creature's skins a spot of a mesh is, as the game reads it: the corners of the primitive of the same
/// number in its own mesh's first sub-mesh, their texture coordinates taken u of the way along the first edge and v
/// along the second, in texels of a skin 256 wide, the skin that primitive's. None when its mesh is not loaded or has
/// no such primitive or corners
[[nodiscard]] std::optional<SkinPlace> PlaceOf(const Registry& registry, entt::entity creature, const MeshSpot& spot);

/// The spot a line meets on a creature's skin as CutPlace finds it, the line taken as it is given: its length is not
/// changed, so a short one is more often passed by as side-on
[[nodiscard]] std::optional<MeshCut> NearestCut(const Registry& registry, entt::entity creature, const glm::vec3& from,
                                                const glm::vec3& line);
/// A spot's point and inward normal under the pose the game reads, as CutPlace poses the body: the first corner plus u
/// of the first edge and v of the second, and the second edge crossed with the first, not made a unit vector. None when
/// the creature has no mesh or no pose yet, or its mesh no longer holds the spot
struct SpotFrame
{
	glm::vec3 point {0.0f};
	glm::vec3 inward {0.0f};
};
[[nodiscard]] std::optional<SpotFrame> FrameOf(const Registry& registry, entt::entity creature, const MeshCut& cut);

} // namespace openblack::ecs::creature_skin
