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
#include <glm/vec3.hpp>

#include "Enums.h"

/// The camera a creature sees: its own god's. The god at this machine looks through the game's camera; every other god
/// has the camera its interface reports, where its hand last was, looking at its citadel
namespace openblack::ecs::god_camera
{

struct Camera
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

/// The camera of a creature of a player: the game's camera for the player at this machine, and for a creature with no
/// player (openblack plays no network game, where such a creature sees none); for another god its reported camera, from
/// where it is towards the focus its citadel set. None when that god reports no camera or has no citadel, or there is no
/// game camera.
[[nodiscard]] std::optional<Camera> Choose(PlayerNames owner, PlayerNames local, const std::optional<Camera>& gameCamera,
                                           const std::optional<glm::vec3>& reportedOrigin,
                                           const std::optional<glm::vec3>& citadelFocus);

/// The camera a creature sees, as Choose, from the world
[[nodiscard]] std::optional<Camera> Of(entt::entity creature);

/// A god's citadel is made: its interface's camera looks at the citadel's place on the map grid, at its height there,
/// from above it, and keeps that until the hand moves it, after the citadel is gone too
void NoteCitadelMade(PlayerNames player, glm::vec3 place);

/// The camera a god's citadel set, kept after the citadel goes; none before it had one
[[nodiscard]] std::optional<Camera> CitadelSetOf(PlayerNames player);

} // namespace openblack::ecs::god_camera
