/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// What a creature makes of seeing its player's hand do something that shows a desire. Everything here is pure.
namespace openblack::creature_hand_sight
{

/// How near the way a creature looks the hand must be for it to see the hand: within three degrees' share of a turn,
/// the turn cut into 360 / 3 parts, as the game asks
inline constexpr float k_LookDegrees = 3.0f;

/// The seconds a desire takes to grow by 1 once the creature saw its player show it: its growing time over the divisor,
/// then kept between the species' least and most
[[nodiscard]] float IncreaseSecondsAfterSeen(float seconds, float divisor, float least, float most);

} // namespace openblack::creature_hand_sight
