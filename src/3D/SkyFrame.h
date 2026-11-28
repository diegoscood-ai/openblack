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

#include "ECS/Components/Sky.h"

/// The sky of a frame: for now, the moon's phase taken from the date while it shows
namespace openblack::sky_frame
{

/// The date is read again for the moon's phase only once more than this has gone by on the machine's clock since it was
/// last read
inline constexpr int32_t k_MoonDateReadInterval = 2000;

/// A moon that shows this frame reads the date, at most once every two seconds of the machine's clock (ticks, in
/// milliseconds), and takes up the phase of the date it last read
void TakeMoonPhase(ecs::components::Moon& moon, int64_t unixTime, uint32_t ticks);

/// The date the moon's phase is taken from, in seconds since 1970: the override, or the computer's as last read
[[nodiscard]] int64_t MoonDate(const ecs::components::Moon& moon);

} // namespace openblack::sky_frame
