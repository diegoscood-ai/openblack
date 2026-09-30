/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <glm/vec3.hpp>

// HandStateGrain (CHand +0x489C, hand state 8: the hand holds a spell seed; ctor 0x5B2B80, vtable 0x900B00). Its
// "grain" part raises and tilts the hand while a sprinkle miracle (food, wood, water) pours from it: UR_HandSprinkle
// starts it (fn_005B2F70), CHand::GameTurnUpdate steps it, HandStateHolding::Update reads it. Wiki:
// docs/bw1-notes/magic.md, "Comida y madera".

namespace openblack::ecs::systems::hand_grain
{

/// The ctor's key points (0, 0), (0.2, 1), (0.8, 1), (1, 0) and the second derivatives that fn_005B3760 (the Numerical
/// Recipes spline) gives them as a natural spline: +0x148 is 1, so the ctor passes 1e30 for both end slopes (0x5B2CF2),
/// and the raise peaks at 1.61 x HeightToRaise in the middle
struct Spline
{
	std::array<float, 4> x {0.0f, 0.2f, 0.8f, 1.0f};
	std::array<float, 4> y {0.0f, 1.0f, 1.0f, 0.0f};
	std::array<float, 4> y2 {};
};
/// fn_005B3760: the second derivatives (an end slope above 0.99e30: natural end)
[[nodiscard]] Spline BuildSpline(float yp1, float ypn);
/// The evaluation in fn_005B2DA0 (bisection, then the cubic; t itself when two key points share x)
[[nodiscard]] float Evaluate(const Spline& spline, float t);

/// fn_005B2F70 -> fn_005B3000 (clampHand, totalTime, heightToRaise, angleToRaise, loop): the raise starts at t = 0
/// from where the hand is now (CHand +0x78, kept at +0x1EC)
void Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop);
/// fn_005B2F10 -> fn_005B2F40: off, no clamp, height and tilt 0
void Stop();

/// fn_005B2D70 -> fn_005B2DA0 (first thing in CHand::GameTurnUpdate 0x46E4E0, dt = turn ms x 0.001): the last values
/// are kept for the interpolation, t += dt / TotalTime (past 1: back to 0 when looping, else Stop), height = v x
/// HeightToRaise (+0x118), tilt = v x AngleToRaise (+0x114)
void GameTurnUpdate(float dt);

/// HandStateGrain vt 0x18 (0x5B2D30) / vt 0x14 (0x5B2D50): the height and the tilt between the last two turns
/// (g_game +0x205D64, the fraction of the turn drawn)
[[nodiscard]] float Height();
[[nodiscard]] float Tilt();
/// HandStateGrain vt 0x1C (0x5B32A0): with ClampHand the hand's required position is the one it had when it started
[[nodiscard]] std::optional<glm::vec3> ClampedPosition();
[[nodiscard]] bool Active();
/// The raw state after the last turn (for the traces and the unit test): t, height, tilt
[[nodiscard]] glm::vec3 Debug();

/// The hand enters (0x5B3080: everything reset) or leaves (0x5B3290: off) the seed-holding state
void SetHoldingSeed(bool holding);

/// A land is loaded
void Reset();

} // namespace openblack::ecs::systems::hand_grain
