/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/fwd.hpp>

namespace openblack
{
class DayNightClock;
}

namespace openblack::ecs
{

/// FireFly::ProcessAll 0x52B7A0, once per game turn: in the evening up to 50 fireflies appear at random trees and
/// rocks and one per turn flies to the nearest house or street lantern; in the morning one per turn flies back to a
/// tree or rock and goes to sleep.
void ProcessFireFliesTurn(const DayNightClock& clock);

/// FireFly::Draw 0x52AA90 / fn_0052ABC0, every frame: the orbit around the firefly's point, the distance fade and
/// the sprite (S_SpriteSheet3 frame 37, additive). `seconds` is game time (0 while paused).
void UpdateFireFlies(float seconds, const glm::vec3& camera);

/// Removes every firefly (new map)
void ClearFireFlies();

} // namespace openblack::ecs
