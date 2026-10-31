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

#include <optional>

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The invisible game object each fireball atom carries. It burns
/// through its FireEffect; the PSys draws it. Not in the map cells (InsertMapObject is empty). Magic/Objects/MagicFireBall.
struct MagicFireBall
{
	int infoRow {0};                   ///< GMagicFireBallInfo[0..2]
	bool affectedByRain {true};        ///< not cast by a script
	uint32_t effect {0};               ///< the particles::manager effect of its atom
	uint32_t atomKey {0};              ///< which atom of that effect (the atom data the fireball was attached to)
	std::optional<PlayerNames> player; ///< GetPlayer: the atom manager's player or the current player
	bool seen {true};                  ///< refreshed by its atom this turn (the atom data keeps the turn)
	/// The radius the hand sees it with: its atom's rule scale times its base scale, after the atom's last step
	float radius {1.0f};
	/// Where the hand sees it: its atom's drawn position after the last step; none while the atom is not drawn yet or
	/// is faint (an alpha of 30 or less), when the hand passes through it
	std::optional<glm::vec3> handTarget;
};

} // namespace openblack::ecs::components
