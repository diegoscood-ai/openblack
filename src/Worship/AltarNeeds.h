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

#include <span>
#include <vector>

// The desires the three need signs over a worship site's altar show, from what the site counts. Wiki:
// docs/wiki/magic/worship.md#the-altars-need-signs. The hunger sign's is the site's own desire for food
// (worship::site::CalculateDesireForFood).

namespace openblack::worship::altar_needs
{

/// What the dancers want of rest: each dancer's 1 - its life, added up group by group in the dance's order of its
/// groups, and in each group in its dancers' order (a float after every dancer), over how many dance; 0 with no one
/// dancing. `groups` holds each group's dancers' lives
[[nodiscard]] float AverageLifeDesired(std::span<const std::vector<float>> groups);

/// The rest sign's desire: the dancers' average held at 1 when it is more (at 1 or below, it stays as it is)
[[nodiscard]] float DesireForRest(float average);

/// The hiding sign's desire: the people at the site who are not in the dance, a hundredth each, at most 1. The count is
/// unsigned, so a site with fewer people than dancers counts a great many and shows the sign full
[[nodiscard]] float PeopleHidingIndicator(int32_t villagersAtSite, int32_t dancers);

} // namespace openblack::worship::altar_needs
