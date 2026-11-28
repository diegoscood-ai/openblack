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

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// How villagers make love out of doors, and the breeder disciple that brings it about
/// (docs/wiki/beings/villager-life.md#love-outside-the-home-and-the-breeder). Two villagers of an age and of opposite
/// sexes meet, kiss for a while, and the woman may fall pregnant. A breeder disciple ambles about a spot by a home of its
/// town, or by its food store, calling the villagers of the other sex to it, until its town is too far over full.
/// Positions are map coordinates (x and z); distances are metres. The pregnancy and the birth are VillagerBirth's.
namespace openblack::ecs::villager_breeding
{

/// Random numbers on the game's shared stream: a float from 0 up to a limit, and a whole number below a limit
using FloatRandom = std::function<float(float)>;
using IntRandom = std::function<uint32_t(uint32_t)>;

/// Two villagers going to make love meet this far apart, and kiss once their edges are at most this near
constexpr float k_MeetMateDistance = 0.562f;
constexpr float k_KissingGap = 0.57f;
/// A breeder breeds while its town's adults are fewer over their places than this many times the children's allowance
constexpr float k_BreederAdultShare = 4.0f;
/// A breeder makes for the nearest home of its town strictly within this many metres, else for its town's food store
constexpr float k_BreederHomeReach = 300.0f;
/// By a home: out from its door, turned up to an eighth of a turn either side, half its size to its size away
constexpr float k_BreederFan = 0.7853982f;
constexpr float k_BreederHomeShare = 0.5f;
/// By the store: three to five metres away
constexpr float k_BreederStoreLeast = 3.0f;
constexpr float k_BreederStoreRange = 2.0f;
/// A breeder wanders up to this far from its spot, then waits 30 to 69 turns
constexpr float k_WanderReach = 8.0f;
constexpr uint32_t k_WanderPauseLeast = 30;
constexpr uint32_t k_WanderPauseRange = 40;

/// The turns two villagers make love for: half of the mate's time and twice one's own, and one more
[[nodiscard]] uint16_t LoveTurns(uint32_t mateTime, uint32_t ownTime);

/// What a breeder weighs of its town: its children and adults against their places in its homes
struct TownHeadcount
{
	uint32_t children {0};
	uint32_t childPlaces {0};
	uint32_t adults {0};
	uint32_t adultPlaces {0};
};
/// A breeder breeds while its town's children are fewer over their places than the allowance, and its adults fewer over
/// theirs than four times it (the differences signed)
[[nodiscard]] bool BreederMayBreed(const TownHeadcount& town, float overAllowance);
/// A breeder's spot by a home: out from the home's door, the way from the home to the door turned by up to an eighth
/// either side (the side drawn first, then the turn, then the distance), half the home's radius to its radius away
[[nodiscard]] glm::ivec2 BreederSpotByHome(glm::ivec2 home, glm::ivec2 door, float homeRadius, const IntRandom& intRandom,
                                           const FloatRandom& floatRandom);
/// A breeder's spot with no home near: three to five metres from its town's food drop-off (the distance drawn first)
[[nodiscard]] glm::ivec2 BreederSpotByStore(glm::ivec2 store, const FloatRandom& random);
/// Where a wanderer walks next: up to eight metres from its wander area, the angle drawn first, then the distance
[[nodiscard]] map_coords::MapCoords WanderGoal(const map_coords::MapCoords& area, const FloatRandom& random);
/// The turns it then waits, drawn after the walk is set up
[[nodiscard]] uint16_t WanderPause(const IntRandom& random);

} // namespace openblack::ecs::villager_breeding
