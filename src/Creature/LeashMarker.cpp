/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashMarker.h"

#include <algorithm>

#include "Creature/CreatureFootprints.h"
#include "Creature/LeashRules.h"

namespace openblack::leash_marker
{

namespace
{
/// The rows of the creature tables the marker reads, the smiley face's the last
constexpr size_t k_Rows = 18;
} // namespace

uint8_t FootprintCell(size_t speciesRow)
{
	return creature_footprints::k_SpeciesPrints.at(speciesRow < k_Rows ? speciesRow : 0).cell;
}

glm::vec2 RingHalfSize(const bookmark_pulse::Pulse& pulse)
{
	return bookmark_pulse::MarkerHalfSize(pulse);
}

glm::vec2 FootprintHalfSize(const bookmark_pulse::Pulse& pulse)
{
	const float stretch = (pulse.width * k_FootprintShare) / pulse.height;
	const float size = std::max(pulse.height * k_FootprintShare, bookmark_pulse::k_SmallestSize);
	return {size, stretch * size};
}

glm::vec3 Position(const std::optional<Thing>& thing, const glm::vec3& kept)
{
	return thing.has_value() ? creature_leash::TargetAbove(thing->place, thing->height) : kept;
}

} // namespace openblack::leash_marker
