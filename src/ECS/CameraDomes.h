/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>

#include "Camera/CameraZones.h"

namespace openblack::ecs
{
class Registry;
}

/// The domes over the creatures and the temples that keep the player's camera out (components::CameraDome, the sizes
/// from camera_zones), as the game makes and moves them
/// (docs/wiki/camera/player-camera.md#the-domes-over-creatures-and-temples)
namespace openblack::ecs::camera_domes
{

/// At the end of the creatures' turn, once their bodies are posed: each creature with its body's measures gets its
/// dome the first time, and has it moved and sized again every turn after, where its body is placed, from its body's
/// radius and its standing height at the size it is drawn at
void PlaceCreatures(Registry& registry);

/// A temple's dome, made as the temple is, over its place, 15 across and as high as its mesh; and sized again from its
/// new mesh when its outside changes. Nothing for a heart without a mesh
void PlaceTemple(Registry& registry, entt::entity heart);

/// Taken away as the temple is deleted
void RemoveTemple(Registry& registry, entt::entity heart);

/// Every dome there is, as the camera's exclusions
[[nodiscard]] std::vector<camera_zones::Exclusion> Exclusions(const Registry& registry);

} // namespace openblack::ecs::camera_domes
