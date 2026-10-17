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

#include <vector>

#include <glm/vec2.hpp>

#include "CreatureIdleMind.h"

// How a creature casts a miracle at something. One time in five it first shows how it feels: angry before a lightning
// bolt, kind before a helpful miracle, playful before one it casts on another creature. Then it goes near the thing,
// gets away from it to a distance, turns to face it and draws the miracle's gesture, if it has one. Last it takes up
// the casting pose and casts the miracle as the pose's loop begins, holds it three seconds and lets it go as the pose
// ends.
//
// A lightning bolt is cast from fifty away, backing off to ten more than the creature's height; a helpful miracle from
// twice its height; a creature spell from five times its height, backing off to twice it.
//
// Food and water are cast otherwise, scattered from above what they are cast at: with no feeling shown first, no draw
// and no backing off, in the scattering pose. Food is cast from two away, kind as it goes and as it turns; water from
// twice its height, smiling as it turns and as it casts. Water on itself it casts once it has gone to a clear spot near
// it, smiling. Pure, tested on its own.

namespace openblack::creature_mind
{

/// The kinds of casting, by the miracles cast that way
enum class CastStyle : uint8_t
{
	/// The lightning bolts
	Lightning,
	/// The heals, and other miracles that help
	Helpful,
	/// The spells cast on another creature
	Playful,
};

/// It shows how it feels first one time in this many
inline constexpr uint32_t k_CastEmoteLots = 5;
/// The casting pose's start, loop and end
inline constexpr std::array<size_t, 3> k_CastPose {40, 41, 42};
/// The scattering pose's start, loop and end, which food and water are cast with
inline constexpr std::array<size_t, 3> k_ScatterPose {46, 47, 48};
/// Food is cast from this near what it is cast at, or from this near a sign that shows a need
inline constexpr float k_FoodGoNear = 2.0f;
inline constexpr float k_FoodGoNearSign = 20.0f;
/// Water on itself is cast in a clear area this many times its height wide
inline constexpr float k_WaterOnItselfClearWidth = 1.7f;
/// The miracle is held this long once cast
inline constexpr float k_CastHoldSeconds = 3.0f;
/// Turning to face what it casts at, it holds still this long once facing it
inline constexpr float k_CastSettleSeconds = 0.1f;
/// A lightning bolt is cast from this far, backing off to this much more than its height
inline constexpr float k_LightningCastDistance = 50.0f;
inline constexpr float k_LightningBackOff = 10.0f;

/// Casting a miracle of a magic type, by its number, with a gesture to draw (0 for none), at an object by its entity's
/// number, by a creature of a height
[[nodiscard]] std::vector<Step> CastAt(CastStyle style, uint32_t magicType, uint32_t gesture, uint32_t object, float height,
                                       const Random& random);

/// Casting food at an object by its entity's number, from so near it, with a gesture to draw first (0 for none). It is
/// food whatever the action that casts it
[[nodiscard]] std::vector<Step> CastFoodAt(uint32_t gesture, uint32_t object, float goNear);
/// Casting water at an object by its entity's number, by a creature of a height. It is plain water whatever the action
/// that casts it
[[nodiscard]] std::vector<Step> CastWaterAt(uint32_t object, float height);
/// Casting water on itself, by its entity's number, from a clear spot it goes to first, by a creature of a height
[[nodiscard]] std::vector<Step> CastWaterOnItself(glm::vec2 spot, uint32_t self, float height);

} // namespace openblack::creature_mind
