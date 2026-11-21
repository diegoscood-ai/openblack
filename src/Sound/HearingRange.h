/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::audio
{

/// The furthest from the camera a placed sound effect may start. The bank's maximum distance for the sample counts
/// whether or not the bank's header applies it to the voice; a sample whose bank gives none (most of those that keep
/// the default distances) falls back on the play's own maximum distance, by default the voice's 9999.
[[nodiscard]] constexpr float SoundEffectStartRange(float bankMaxDistance, float playMaxDistance)
{
	return bankMaxDistance != 0.0f ? bankMaxDistance : playMaxDistance;
}

/// The furthest from the camera an animation effect (a knock, a crash, a footstep) may start: the bank's maximum
/// distance for the sample, with no fallback, so a sample whose bank gives none is only heard from right on top of it
[[nodiscard]] constexpr float AnimEffectStartRange(float bankMaxDistance)
{
	return bankMaxDistance;
}

/// Whether a placed sound this far from the camera may start, up to and including its range (the animation effects'
/// test: a distance that is not a number is refused)
[[nodiscard]] constexpr bool IsWithinStartRange(float distance, float range)
{
	return distance <= range;
}

/// The sound effects' test, on the squared distance: refused only when it is more than the squared range, so a sound
/// on the range's edge starts and a distance that is not a number is not refused, as the original game compares them
[[nodiscard]] constexpr bool IsWithinStartRangeSq(float distanceSq, float range)
{
	return !(distanceSq > range * range);
}

} // namespace openblack::audio
