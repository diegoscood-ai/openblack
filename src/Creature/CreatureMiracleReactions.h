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

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "CreatureIdleMind.h"

// What a creature does about a miracle near it. Frightened by a nasty one, it may start back in fright, then runs away
// from where it struck. Curious about one (a nice one of its own player's, or a nasty one when it isn't afraid or is on
// the learning leash), it may first turn to it and point at it, then goes up to it, turns to face it and points at it,
// or waits there puzzled. Pure agendas, tested on their own.
//
// These are raffclar's renderings of the game's two actions, running away from a place and examining a place, which the
// plans of those actions build. The distance, the wait, the frightened start, the faces and the coin tosses are the
// game's, and so are the point the run goes to, picked as the plan is made, the looks at the place as it turns and
// points and the run's look on the way (see docs/wiki/creature/mimicry.md#pending).

namespace openblack::creature_mind
{

/// The creature arrives this many times its height from where a miracle struck
inline constexpr float k_MiracleApproachHeights = 5.0f;
/// It waits this long puzzled at a miracle it went to look at, in seconds
inline constexpr float k_PuzzledSeconds = 2.1f;
/// It points at the place a second when it points before going there, two when it points once there
inline constexpr float k_FirstPointSeconds = 1.0f;
inline constexpr float k_LastPointSeconds = 2.0f;
/// The frightened start it may give
inline constexpr size_t k_FrightenedAnimation = 61;

/// Where a creature runs to away from a place, going `extra` metres further than its species' distance; none when it
/// can't run
using RunAwayPicker = std::function<std::optional<glm::vec2>(glm::vec2 place, uint32_t extra)>;
/// Running away from where a frightening miracle struck: the start of fright one time in two, then the run to the point
/// picked away from the place, the start drawn first, then how much further than its species' distance it goes. None
/// when no point can be picked
[[nodiscard]] std::optional<std::vector<Step>> RunAwayFromMiracle(glm::vec2 point, float height, const Random& random,
                                                                  const RunAwayPicker& pickRunAwayPoint);
/// Going to look at where a miracle struck
[[nodiscard]] std::vector<Step> ExamineMiracle(glm::vec2 point, float height, const Random& random);

} // namespace openblack::creature_mind
