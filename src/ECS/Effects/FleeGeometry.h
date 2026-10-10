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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Where a living being runs to when something frightens it, whether that thing is coming at it, and how urgently it
// runs from a frightening miracle. Pure functions: the random draws come in as arguments. The animals keep their own
// copy of the flee point (ECS/AnimalFlee.cpp), with the same arithmetic.

namespace openblack::ecs::effects::flee
{
/// How far one run goes, in metres
inline constexpr float k_FleeStep = 10.0f;
/// The spread of the random part of a run across a moving thing's way, in metres (each axis draws 0 to 8, less 4)
inline constexpr float k_FleeJitter = 8.0f;
/// The thing is coming at it when its way is within this cosine of the line from it
inline constexpr float k_ComingTowardsCosine = 0.8f;
/// Within this many whole map units (the fast distance, about 91.5 metres) a frightening miracle is more urgent the
/// nearer it is
inline constexpr int32_t k_FleeUrgencyUnits = 600000;

/// From a still thing: `distance` straight away from it, in the plane; standing on it, it stays where it is
[[nodiscard]] glm::vec2 FleeingPositionFromStill(glm::vec2 me, glm::vec2 object, float distance);

/// From a moving thing: `distance` across its way on the side the living being stands on, each axis shifted by its
/// random draw (0 to 8) less 4. `randomX` is drawn first, then `randomZ`. A way that is straight up or down has no
/// side: only the random shift is left
[[nodiscard]] glm::vec2 FleeingPositionFromMoving(glm::vec2 me, glm::vec2 object, glm::vec2 movement, float distance,
                                                  float randomX, float randomZ);

/// Whether a thing at `object` moving along `movement` is coming at a living being at `me`, in three dimensions
/// (the heights count): the cosine between its way and the line from it to the living being is at least 0.8. A thing
/// that does not move, or stands exactly there, is not coming
[[nodiscard]] bool ComingTowards(glm::vec3 me, glm::vec3 object, glm::vec3 movement);

/// How urgently a living being runs from a frightening miracle: the reaction's priority, plus up to 100 more the
/// nearer it is within k_FleeUrgencyUnits (whole-number arithmetic, truncated), kept to a byte
[[nodiscard]] uint8_t FleeFromSpellPriority(uint32_t priority, int32_t fastDistance);
} // namespace openblack::ecs::effects::flee
