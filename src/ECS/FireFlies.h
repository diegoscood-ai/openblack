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
#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

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

/// Every frame, before the object draw list: no firefly is drawn unless the list draws it this frame
void HideFireFlies();

/// The object draw list's Draw of a firefly's object (components::FireFlyObject): an awake firefly within 300 m of the
/// camera, measured from the point its last drawing put it at, has its sprite's alpha set from that distance, then its
/// drift, clock and drawn point stepped (the orbit round its turn's place, `seconds` of game time, 0 while paused), and
/// its sprite (S_SpriteSheet3 frame 37, additive) drawn there. A firefly asleep or farther is neither drawn nor
/// stepped. False when the object is no firefly's
bool DrawFireFly(entt::entity object, float seconds, const glm::vec3& camera);

/// Removes every firefly (new map)
void ClearFireFlies();

/// How many fireflies there are
[[nodiscard]] size_t FireFlyCount();

/// A land script's firefly (CREATE_FIRE_FLY): a new one hidden at those map coordinates, made as the evening's are,
/// whatever the count (the fifty bound only the evening's top-up)
void PlaceFireFly(const map_coords::MapCoords& coords);

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
/// How long a flight out or home lasts: its length over three metres a second times the firefly's speed, at
/// least half a second
[[nodiscard]] float FlightSeconds(float metres, float speed);
/// How far along its flight a firefly is at that progress (0 to 1): eased in and out, (3 - (p + p)) p p in that order
[[nodiscard]] float Ease(float progress);
/// A firefly's alpha (of 255) by its squared distance from the camera: none asleep or past 300 m, 190 within
/// 100 m and fading by the squared distance in between, truncated to a whole step
[[nodiscard]] float Alpha(bool asleep, float squaredDistance);

} // namespace openblack::ecs::fire_flies

namespace openblack::ecs
{

/// What a saved game keeps of a firefly, as the original's record of one holds it: what it is doing, where it is and
/// where it was made, its home, the light and its last turn's place, the point it was last drawn at, its clock, drift,
/// progress, flight's length, speeds and six phases, its flags (bit 0: resting) and its creation number; and its object
/// in the map, whose place's flags the save writes (none for one read from a record)
struct SavedFireFly
{
	fire_flies::State state {fire_flies::State::Asleep};
	map_coords::MapCoords coords;
	map_coords::MapCoords madeAt;
	map_coords::MapCoords home;
	map_coords::MapCoords light;
	map_coords::MapCoords last;
	glm::vec3 drawn {0.0f};
	float clock {0.0f};
	float drift {0.0f};
	float progress {0.0f};
	float duration {0.5f};
	float speedA {1.0f};
	float speedB {1.0f};
	std::array<float, 6> phases {};
	uint8_t flags {1};
	uint32_t creationIndex {0};
	entt::entity object {entt::null};

	bool operator==(const SavedFireFly&) const = default;
};

/// The game's fireflies as a save keeps them, the list's head first
[[nodiscard]] std::vector<SavedFireFly> SavedFireFlies();
/// Whether the next nightfall tops the fireflies up
[[nodiscard]] bool FireFliesMaySpawn();
/// A loaded firefly at the list's tail, as the load reads the list: its saved values, its sprite made again, and, as
/// its creation does at the load's resolve, its home and its last turn's place where it is
void RestoreFireFly(const SavedFireFly& saved);
/// Whether the next nightfall tops the fireflies up, as the load reads it
void SetFireFliesMaySpawn(bool maySpawn);

} // namespace openblack::ecs
