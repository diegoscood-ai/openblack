/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// Where to find water to drink: the coast and the rivers (GUtils::FindNearestCoastalTo 0x74E2E0,
/// GStream::FindNearestPosTo 0x733D30, GUtils::FindNearestDrinkingWater 0x74E3A0) and the abode's cached drinking
/// water (Abode::FindNearestDrinkingWater 0x407020, Abode::GetNearestWaterPos 0x405FC0).
///
/// Positions are MapCoords given in world units: x and z the world position (the original keeps them as 16.16 fixed
/// cells, world * 6553.6 truncated, and so do these functions inside), y the height above the ground (MapCoords +8,
/// usually 0), not the world altitude.
///
/// No consumer yet: the shepherd (Villager::ShepherdMoveFlockToWater 0x768CC0), the abode's creation and the creature
/// need villager jobs and the creature. Tiger::CalculeLairPos (0x421470) calls FindNearestDrinkingWater(pos, 500) but
/// never uses the answer (see AnimalPredators.cpp).
namespace openblack::ecs::water_queries
{

/// Abode::Abode (0x4013E0): the new abode looks for drinking water this far
constexpr float k_AbodeDrinkingWaterRadius = 200.0f;
/// Villager::ShepherdMoveFlockToWater (0x768D16): the radius when the abode has no drinking water yet
constexpr float k_ShepherdDrinkingWaterRadius = 400.0f;
/// Tiger::CalculeLairPos (0x4214D2)
constexpr float k_TigerLairDrinkingWaterRadius = 500.0f;

/// GUtils::GetDistanceInMetres (0x74CD70) on two world points: ECS/GUtilsDistance (gutils::GetDistanceInMetres) with
/// the points truncated to MapCoords first
[[nodiscard]] float GetDistanceInMetres(glm::vec3 a, glm::vec3 b);

/// GUtils::FindNearestCoastalTo (0x74E2E0): walks the cells in a square spiral from `from` (GUtils::Spiral 0x74D7E0:
/// -x, -z, +x +x, +z +z, -x x3, -z x3...; each step moves the cell and keeps the fraction and y) until one is in the
/// game map and MapCoords::IsCoastal, or the walk gets farther than `radius` from `from` (it stops at the first such
/// cell, so the corners of the square are not all seen), at most 999999 steps
[[nodiscard]] std::optional<glm::vec3> FindNearestCoastalTo(const LandIslandInterface& island, glm::vec3 from,
                                                            float radius);
/// GStream::FindNearestPosTo (0x733D30): the river point (CREATE_STREAM_POINT) nearest to `from` in xz, strictly
/// closer than `radius`; y = the point's altitude minus the ground's there
[[nodiscard]] std::optional<glm::vec3> FindNearestStreamPosTo(const LandIslandInterface& island, glm::vec3 from,
                                                              float radius);
/// GUtils::FindNearestDrinkingWater (0x74E3A0): the nearest river point within `radius`, then (true either way) the
/// nearest coast that is not farther from `from` than that river point, if any; with no river, the coast within
/// `radius`. `out` changes only where something is found, like the original's out MapCoords.
bool FindNearestDrinkingWater(const LandIslandInterface& island, glm::vec3 from, glm::vec3& out, float radius);

/// Abode +0x7C bit 0 (found) and +0x80 (the MapCoords): the abode's drinking water
struct DrinkingWater
{
	bool found {false};
	glm::vec3 position {0.0f};
};
/// Abode::FindNearestDrinkingWater (0x407020): FindNearestDrinkingWater from the abode into its cache; the flag is the
/// answer (the position stays the old one when nothing is found)
bool FindNearestDrinkingWater(const LandIslandInterface& island, DrinkingWater& water, glm::vec3 abodePosition,
                              float radius);
/// Abode::GetNearestWaterPos (0x405FC0): the cached drinking water, if found
[[nodiscard]] std::optional<glm::vec3> GetNearestWaterPos(const DrinkingWater& water);

// With the Locator's island (no island: nothing is found)
[[nodiscard]] std::optional<glm::vec3> FindNearestCoastalTo(glm::vec3 from, float radius);
[[nodiscard]] std::optional<glm::vec3> FindNearestStreamPosTo(glm::vec3 from, float radius);
bool FindNearestDrinkingWater(glm::vec3 from, glm::vec3& out, float radius);

} // namespace openblack::ecs::water_queries
