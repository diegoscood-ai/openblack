/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
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

/// How a recognised gesture's trail raises its light sheet and moves its sparkles
namespace openblack::particles::gesture_trail
{
/// The sparkles' collection flashes from this age on, and dies away over this long
inline constexpr float k_PulseStart = 2.4f;
inline constexpr float k_PulseLength = 2.1f;

/// The size of the gesture's shape, which scales its sparkles and its light sheet: a hundredth of the shape's length
[[nodiscard]] float ShapeScale(float shapeLength);
/// How far the sparkles have come from the drawn path to the shape: from 0 when they began to 1 after timeToIdeal
[[nodiscard]] float IdealShare(float age, float start, float timeToIdeal);
/// That share pulled towards a smooth start and end by interpGain
[[nodiscard]] float EasedIdealShare(float share, float interpGain);
/// The alpha of the sparkle a share `along` the trail when the sparkles have come `share` of the way and are at most
/// `peak`: the trail is revealed from both ends, which meet in the middle
[[nodiscard]] uint8_t RevealAlpha(float along, float share, float peak);
/// How strongly the sparkles wiggle at an age: a slow swell, which shrinks away over shrinkTime once dispersalTime has
/// passed
[[nodiscard]] float WiggleStrength(float age, float phaseSpeed, float dispersalTime, float shrinkTime);
/// The sparkles' collection's alpha at an age once it flashes: from `pulse` down to nothing over k_PulseLength; none
/// before k_PulseStart
[[nodiscard]] std::optional<float> CollectionPulseAlpha(float age, int pulse);

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

/// How the chain behind the drawing hand lays its links
namespace openblack::particles::gesture_chain
{
/// The chain's head goes this long after its emission stops
inline constexpr float k_HeadLinger = 5.0f;

/// How many links the chain owes for a step: one for the first, else the distance the head has moved since the last
/// link over minEmitDist, at most `count` links per dieAge seconds; none once it stops emitting
[[nodiscard]] float LinkStep(float emitted, const glm::vec3& lastEmitted, const glm::vec3& head, float minEmitDist,
                             size_t count, float dieAge, float dt, bool emitting);
/// The chain's head goes once it has stopped emitting for more than k_HeadLinger
[[nodiscard]] bool HeadGone(bool emitting, float age, float stopTime);
} // namespace openblack::particles::gesture_chain
