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

#include "3D/MapCoords.h"
#include "Enums.h"

// The land's birds: what a creature may do to one, and the doves or bats about a temple. The pure rules: the systems
// apply them
namespace openblack::animals::birds
{

/// A creature can pick a bird up only while it is less than a metre above the land: one on the ground, as a dead one
inline constexpr float k_CreaturePickUpBelow = 1.0f;
[[nodiscard]] bool CreatureCanPickUp(float heightAboveLand);
/// A creature can stamp on a bird only while it is lower above the land than a quarter of the creature's height
inline constexpr float k_CreatureStompBelowShare = 0.25f;
[[nodiscard]] bool CreatureCanStompOn(float heightAboveLand, float creatureHeight);

// The temple's birds

/// The temple's flock is seen to every hundred turns of the game
inline constexpr uint32_t k_TempleFlockEvery = 100;
/// It wanders up to 30 m about the temple, its followers within 15 m of their leader
inline constexpr float k_TempleFlockReach = 30.0f;
inline constexpr float k_TempleFlockDistance = 15.0f;
/// Doves about a good temple, bats about an evil one: a player of no alignment at all counts as good
[[nodiscard]] AnimalInfo TempleBirdKind(float alignment);
/// How many birds a temple has: its most (the temple's table) by how far its player is from neutral, none while the
/// temple is unbuilt
[[nodiscard]] uint32_t TempleBirdCount(float alignment, uint32_t most, bool built);
/// The flock moves one bird towards its count each time it is seen to
enum class TempleFlockStep : uint8_t
{
	None,
	AddOne,
	RemoveOne,
};
[[nodiscard]] TempleFlockStep StepTowards(uint32_t members, uint32_t count);
/// The height the temple's birds fly about: as high as the temple's model stands, scaled, and 10 m more
[[nodiscard]] float TempleFlockHeight(float scale, float modelHeight);

// A bird's legs

/// The height of a bird's next leg: its last leg's goal height plus the kind's variance, less the draw of up to twice
/// the variance (GameFloatRand); kept only strictly between the flock's height plus the kind's least and plus its most,
/// else the flock's height. Its own height on the way does not count
[[nodiscard]] float NextLegAltitude(float lastGoalAltitude, float variance, float draw, float flockAltitude, float altitudeMin,
                                    float altitudeMax);

// Where a bird may go

/// The square a point lies in for a bird's way: its metres cut to whole ones, then to the middle of their 16 m square
/// (a sixteenth taken towards zero, so -15..15 m all fall in one square)
[[nodiscard]] int32_t WaySquareCentre(int32_t fixed);
/// Whether a bird may go from where it is to a point: the way is walked from the point's square towards the bird's, a
/// square at a time along whichever way lags behind the straight line (the longer one when they are level), and every
/// square passed after the point's own must lie over a cell of the map (0..511 each way) that a land block holds. Equal
/// squares need nothing
[[nodiscard]] bool LandAllTheWay(const map_coords::MapCoords& to, const map_coords::MapCoords& from,
                                 const std::function<bool(int32_t cellX, int32_t cellZ)>& hasBlock);

} // namespace openblack::animals::birds
