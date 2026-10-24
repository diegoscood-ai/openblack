/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>

#include "3D/MapCoords.h"

namespace openblack
{
class DayNightClock;
}

namespace openblack::ecs
{

/// Once per game turn: in the evening up to 50 fireflies appear at random trees and rocks and one per turn flies to an
/// abode or street lantern found by a 300-unit spiral search (each cell is searched only on GameRand(2) != 0 and a
/// closer match ends the cell on GameRand(3) == 0: not strictly the nearest); in the morning one per turn flies back to
/// a tree or rock found the same way and goes to sleep.
void ProcessFireFliesTurn(const DayNightClock& clock);

/// Every frame: the orbit around the firefly's point, the distance fade and
/// the sprite (S_SpriteSheet3 frame 37, additive). `seconds` is game time (0 while paused).
void UpdateFireFlies(float seconds, const glm::vec3& camera);

/// Removes every firefly (new map)
void ClearFireFlies();

/// When an object is placed in the magic hand: a firefly whose map coordinates equal the object's (x, z and the
/// altitude; a sleeping one sits exactly on its tree or rock) goes. True when there was one; the spell reward is
/// Worship/FireFlyReward.cpp's.
bool TakeFireFlyAt(const map_coords::MapCoords& coords);

/// Where a firefly perches, wakes and goes to sleep: a thing that answers it is a rock (a rock, the bonfire, a piece
/// knocked off a building) or any kind of tree (a dead and a felled one too)
[[nodiscard]] bool IsFireFlyPerch(entt::entity object);

/// Where a firefly starts that does not start at a tree: the first thing of the fixed list from `start` on that answers
/// it is a rock; null when there is none after it (that firefly is not made)
[[nodiscard]] entt::entity FireFlyRockFrom(std::span<const entt::entity> fixed, size_t start);

} // namespace openblack::ecs

namespace openblack::ecs::fire_flies
{

/// What a firefly is doing
enum class State
{
	Asleep,     // 0: invisible at its tree or rock
	Hovering,   // 1: at the house
	FlyingHome, // 2
	FlyingOut,  // 3
};

/// How far a firefly's orbit swings out round its point: none asleep, all of it hovering, growing over the first fifth
/// of a flight out and dying over the last fifth of a flight home (`progress` 0 to 1 along the flight)
[[nodiscard]] float DriftAmplitude(State state, float progress);

} // namespace openblack::ecs::fire_flies
