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

// The hand's grain state (hand state 8: the hand holds a spell seed). It raises and tilts the hand while a sprinkle
// miracle (food, wood, water) pours from it: the sprinkle starts it, the hand's game turn steps it, the holding state
// reads it. The pour itself is magic::PourState (Magic/HandMotion.h). Wiki: docs/bw1-notes/miracles.md, "Food and
// wood".

namespace openblack::magic
{
struct PourPose;
}

namespace openblack::ecs::systems::hand_grain
{

/// The raise starts at t = 0 from where the hand is now (kept as the start position)
void Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop);
/// Off, no clamp, height and tilt 0
void Stop();

/// First thing in the hand's game turn (dt = turn ms x 0.001): the last values are kept for the interpolation,
/// t += dt / TotalTime (past 1: back to 0 when looping, else Stop), height = v x HeightToRaise, tilt = v x
/// AngleToRaise, v the natural spline through magic::k_PourKeyPoints (magic::StepPour)
void GameTurnUpdate(float dt);

/// The height and the tilt between the last two turns (by the fraction of the turn drawn)
[[nodiscard]] float Height();
[[nodiscard]] float Tilt();
/// The whole pose a fraction of the way from the last turn to the next: the height, the tilt and where the hand stays
[[nodiscard]] magic::PourPose PoseAt(float fraction);
/// With ClampHand the hand's required position is the one it had when it started
[[nodiscard]] std::optional<glm::vec3> ClampedPosition();
[[nodiscard]] bool Active();
/// The raw state after the last turn (for the traces and the unit test): t, height, tilt
[[nodiscard]] glm::vec3 Debug();

/// The hand enters (everything reset) or leaves (off) the seed-holding state
void SetHoldingSeed(bool holding);

/// A land is loaded
void Reset();

} // namespace openblack::ecs::systems::hand_grain
