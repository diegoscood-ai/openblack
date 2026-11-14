/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "InfoConstants.h"
#include "Magic/MagicTables.h"
#include "Particles/ParticleEffect.h"

// What the Magic window shows of the miracle tables and the particle types, as text. Free of the game's state, so it
// is tested with made-up tables.

namespace openblack::debug::magic_window
{

/// The section of info.dat a magic type's record is in
[[nodiscard]] std::string_view SectionName(magic::MagicInfoSection section);

/// The magic type's name in the tables, or its number when the tables have none
[[nodiscard]] std::string MagicName(const InfoConstants& info, MagicType type);
/// The seed's name in the tables, "none" for no seed
[[nodiscard]] std::string SeedName(const InfoConstants& info, SpellSeedType seed);

/// A timer of the tables: seconds, or no limit when negative
[[nodiscard]] std::string Seconds(float seconds);
/// A power-up level: the base level, or its number
[[nodiscard]] std::string PowerUpLevelName(int level);

/// A particle type with its number and the spell file it starts, if any
[[nodiscard]] std::string ParticleTypeLabel(ParticleType type);
[[nodiscard]] std::string_view DrawPathName(particles::DrawPath path);

/// A creature or villager an effect may be given to act on, and how far it is from the effect
struct TargetCandidate
{
	float distance;
	entt::entity entity;
};
/// The `count` nearest candidates, in the order they are given to the effect: its rules take the last given first, so
/// the nearest goes last
[[nodiscard]] std::vector<entt::entity> TargetsInGivingOrder(std::vector<TargetCandidate> candidates, size_t count);

} // namespace openblack::debug::magic_window
