/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

namespace openblack::magic::gestures
{
class Path;
struct RecognisedGesture;
} // namespace openblack::magic::gestures

namespace openblack::particles
{
/// A recognised gesture's trail as the particle system takes it: the stroke, the gesture's shape on the land, the hand
/// and the player whose colour it shows in
using GestureTrail = magic::gestures::RecognisedGesture;
} // namespace openblack::particles

/// How a recognised gesture's trail raises its light sheet
namespace openblack::particles::gesture_trail
{
/// The light sheet stands on this many points taken evenly along the shape, and its strength moves one point along this
/// often
inline constexpr int k_SheetPoints = 50;
inline constexpr float k_SheetShiftSeconds = 0.03f;

/// The light sheet's strength at an age, rising from nothing and falling back to it over its life
[[nodiscard]] float SheetStrength(float age, float lifetime);
/// The light sheet's points: k_SheetPoints of them along the gesture's shape, from its first point to its last, an even
/// share of its length apart
[[nodiscard]] std::vector<glm::vec3> SheetPoints(const magic::gestures::Path& ideal);
} // namespace openblack::particles::gesture_trail
