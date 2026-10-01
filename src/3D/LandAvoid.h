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
#include <filesystem>

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;

/// The creature's walkable mask, LandAvoid (0xD559B0, 512 x 512 bytes, [z][x], one per landscape cell): built once per
/// landscape by ValidateLandAvoid (0x6E7BA0, from GLandscape::Open 0x5E5541) and FloodAnalyse (0x6E7FA0, RoutePlan.cpp).
/// The creature may stand where it is 0 or 6 and at least 7.1 from the centre of any other nearby cell (fn_00483890):
/// it wades into the sea up to where the four corners of the cells are at altitude 0, and never swims.
///
/// No consumer yet (there is no creature movement); OPENBLACK_DUMP_LAND_AVOID writes it as a picture.
namespace land_avoid
{

enum Value : uint8_t
{
	k_Land = 0,        ///< land reached by the flood
	k_Avoid = 1,       ///< too steep or deep sea (all 4 corners at altitude 0) next to the reached land
	k_Unreachable = 2, ///< land the flood did not reach, and the too steep / deep cells away from the reached land
	k_Water = 6,       ///< reached land whose cell has the water bit (or no block): water the creature can walk in
};

/// fn_00483850: the creature's radius for most checks (Creature::InterfaceValidToGiveObject, the movement, the script's
/// PosValidForCreature)
constexpr float k_CreatureRadius = 7.1f;
/// fn_00483870 (LH3DCreature::InitialiseTurning)
constexpr float k_CreatureTurningRadius = 7.05f;

/// ValidateLandAvoid (0x6E7BA0) + FloodAnalyse (0x6E7FA0) for the island's landscape (the original's 512 x 512 becomes
/// GetCellsPerSide() squared, so BWLandEditor maps get their whole grid)
void Validate(const LandIslandInterface& island);
/// Drops the mask (no island)
void Clear();

/// LandAvoid[z][x]; k_Unreachable off the map or before Validate
[[nodiscard]] uint8_t At(int32_t x, int32_t z);
/// fn_00483890: the cell of `position` ((int)(x * 0.1), (int)(z * 0.1)) is in the map and 0 or 6, and no cell of the
/// 3 x 3 around it (in the map) that is not 0 or 6 has its centre (10 i + 5, 10 j + 5) closer than `radius` in xz
[[nodiscard]] bool IsPosValid(glm::vec3 position, float radius = k_CreatureRadius);

/// A picture of the mask, one pixel per cell (x right, z down): 0 green, 6 blue, 1 red, 2 grey (any other value
/// magenta). False if there is no mask or the file cannot be written.
bool DumpPng(const std::filesystem::path& path);
/// Test hook, after Validate: OPENBLACK_DUMP_LAND_AVOID=1 (land_avoid.png in the working directory) or =<file.png>
void DumpIfRequested();

} // namespace land_avoid
} // namespace openblack
