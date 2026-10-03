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
/// rocks and one per turn flies to an abode or street lantern found by fn_0052A670 (a 300-unit GUtils::Spiral where
/// each cell is searched only on GameRand(2) != 0 and a closer match ends the cell on GameRand(3) == 0: not strictly
/// the nearest); in the morning one per turn flies back to a tree or rock found the same way by fn_0052A7A0 and goes
/// to sleep.
void ProcessFireFliesTurn(const DayNightClock& clock);

/// FireFly::Draw 0x52AA90 / fn_0052ABC0, every frame: the orbit around the firefly's point, the distance fade and
/// the sprite (S_SpriteSheet3 frame 37, additive). `seconds` is game time (0 while paused).
void UpdateFireFlies(float seconds, const glm::vec3& camera);

/// Removes every firefly (new map)
void ClearFireFlies();

/// fn_0052B5A0 + the deletion of fn_0052B600 (GInterface::PlaceObjectInMagicHand 0x5DA6F0): a firefly whose MapCoords
/// equal the point's (x and z; a sleeping one sits exactly on its tree or rock) goes. True when there was one; the
/// spell reward (fn_0052B6F0) is Worship/FireFlyReward.cpp's.
bool TakeFireFlyAt(const glm::vec3& position);

} // namespace openblack::ecs
