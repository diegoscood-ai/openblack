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

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// How the area a creature is kept within holds back the walks of a forced plan that gives itself its own steps. Each
/// walk is checked as the agenda gets it. Everything here is pure.
namespace openblack::creature_agenda_area
{

/// Whether a step walks the creature out of the area it is kept within: a walk to a point, or to a place about where the
/// creature stands as the agenda starts, that ends farther across the ground than the radius from the point it is kept
/// at (creature_leash::LeavesKeptArea)
[[nodiscard]] bool LeavesArea(const creature_mind::Step& step, glm::vec2 creature, glm::vec2 keptAt, float radius);

/// The steps of an agenda a forced plan gives itself, without the walks that would take the creature out of the area it
/// is kept within: the plan goes on with the other steps. When a walk left out is the main step, the step before it
/// becomes the main step
[[nodiscard]] std::vector<creature_mind::Step> WithoutWalksOut(std::vector<creature_mind::Step> agenda, glm::vec2 creature,
                                                               glm::vec2 keptAt, float radius);

} // namespace openblack::creature_agenda_area
