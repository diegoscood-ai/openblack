/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// The power-up bands on the player's hand: when the bands that fly from the camera onto the hand start, how the
/// bracelets sit along the arm and spin, the band shaken off, and the pulses of a charge from worship.
///
/// Pure values and functions, tested on their own; the hand's miracle look (magic::hand_fx) makes and draws the bands.
/// docs/wiki/magic/hand-casting.md#the-hand
namespace openblack::magic::hand_bands
{

/// A miracle reaching the hand: five bands fly onto it, a tenth of a second apart
inline constexpr int k_FlyInBands = 5;
/// The delay of the bands of a miracle still settling, in seconds
inline constexpr float k_DelayedStart = 2.4f;

/// When the i-th (1..5) band flying onto the hand starts, in seconds: i tenths, 2.4 s later when delayed
[[nodiscard]] float FlyInStart(int i, bool delayed) noexcept;
/// When a power-up level's band starts: at once, or 2.4 s later when delayed
[[nodiscard]] float PermanentStart(bool delayed) noexcept;

/// A band's spin, in radians a second, for its place 0 along the arm; each further place spins a fifth faster
inline constexpr float k_Spin = 12.0f;
/// The first band's height along the arm (the hand's root bone's z), and the step between bands
inline constexpr float k_Offset = 10.0f;
inline constexpr float k_Step = 40.0f;

/// The band's angle after a step of `seconds`: on by (1 + 0.2 x its place) x 12 radians a second, wrapped at a turn. A
/// step of 0 (the game paused) leaves it where it was
[[nodiscard]] float NextSpinAngle(float angle, int index, float seconds) noexcept;
/// How far along the arm the band of that place sits: 10 + 40 x its place
[[nodiscard]] float HeightAlongArm(int index) noexcept;

/// Shaking the miracle off: one band goes back to the camera over a second, its alpha from 5 to 50, reversed
inline constexpr float k_ShakeOffSeconds = 1.0f;
inline constexpr uint8_t k_ShakeOffAlpha0 = 5;
inline constexpr uint8_t k_ShakeOffAlpha1 = 50;

/// A charge from worship pulses the hand with a band: the first on the frame charging starts, then one each time the
/// charge's timer passes lerp(6, 0.3, charge) seconds
[[nodiscard]] bool ChargeBandDue(bool started, float timer, float charge) noexcept;
/// A charge band lasts lerp(3.5, 1, charge) seconds
[[nodiscard]] float ChargeBandSeconds(float charge) noexcept;
/// Its alpha goes from lerp(3, 5, charge) to lerp(15, 50, charge), each truncated
[[nodiscard]] uint8_t ChargeBandAlpha0(float charge) noexcept;
[[nodiscard]] uint8_t ChargeBandAlpha1(float charge) noexcept;

} // namespace openblack::magic::hand_bands
