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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/WaterRings.h"
#include "ECS/Components/DrawPosition.h"

/// How a shark is drawn between its turns and how its wake spreads
namespace openblack::ecs::shark_rules
{

/// A wake ring comes once more than this many milliseconds have gone since the last
inline constexpr int32_t k_WakeInterval = 50;
/// A shark's colour, 0x303070, which the game's light shades as it does every model's
inline constexpr glm::vec3 k_Colour {48.0f, 48.0f, 112.0f};
/// A shark is drawn cut at the sea: nothing of it below this height shows
inline constexpr float k_SeaLevel = 0.0f;

/// The way a shark faces, radians from +x towards +z: the way it moved this turn, or the way it faced if it didn't move
/// across the land
[[nodiscard]] float Heading(const glm::vec3& turnStart, const glm::vec3& position, float facing);

/// Where a shark is drawn `turnFraction` of the way through its turn, each end at the given height
[[nodiscard]] glm::vec3 Drawn(const glm::vec3& turnStart, float startHeight, const glm::vec3& position, float endHeight,
                              float turnFraction);

/// Places a shark's drawing where it is drawn this frame, facing its heading, upright (no slope shear). The renderer
/// reads the drawing's own pose, so it is set too: the shark is drawn between its turns every frame, never a turn at
/// a time
void PoseDrawing(components::DrawPosition& draw, const glm::vec3& drawn, const glm::mat3& rotation);

/// The wake's ring for a frame of `frameMilliseconds`, when one is due: on the water under `point`, turned with the
/// shark. Every shark's frames count towards the one timer, so two sharks share the rings between them.
[[nodiscard]] std::optional<water_rings::Ring> WakeRing(int32_t& timer, const glm::vec3& point, float heading,
                                                        int32_t frameMilliseconds);

} // namespace openblack::ecs::shark_rules
