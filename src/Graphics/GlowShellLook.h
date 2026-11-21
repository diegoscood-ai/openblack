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

/// The glowing shell the game lays over a model of its own, apart from the particle effects' (a creature's creed glows,
/// the citadel heart's shell as it is destroyed): its texture slides on by the time of each frame it is drawn, and it is
/// left out when it is far away. See particles::volume_glow for how the shell itself is drawn.
namespace openblack::glow_shell
{

/// A shell is left out beyond this many times its model's half diagonal, as it is scaled, from the camera
inline constexpr float k_FarCut = 300.0f;

/// The texture slid on across and up by `speed` sheets a second, for some milliseconds: each the seconds times its
/// speed added to how far it had slid, brought back to 0..1, every step a float
[[nodiscard]] glm::vec2 Slide(glm::vec2 slide, glm::vec2 speed, uint32_t milliseconds);

/// Whether a shell is too far to be drawn: its model's half diagonal times its scale times k_FarCut, squared, short of
/// the squared distance from the camera
[[nodiscard]] bool TooFar(float distanceSquared, float halfDiagonal, float scale);

} // namespace openblack::glow_shell
