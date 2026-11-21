/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

namespace openblack::simulation
{

/// The effects of the frame move on by the game time of the frame, before the hand moves: the physics objects, the
/// trees and the wind, the fireflies, the explosions' marks, the reward chests, the villagers and the animals between
/// their turns with their animations, the creatures between turns, the sharks and the fish, the floating numbers, the
/// cinema bars and the scripts' words. `deltaTime` is the frame's real time, or the fixed step's.
void PlayFrameEffects(std::chrono::microseconds deltaTime);

} // namespace openblack::simulation
