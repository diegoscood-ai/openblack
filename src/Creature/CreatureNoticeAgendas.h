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

#include <glm/vec2.hpp>

#include "CreatureIdleMind.h"

// What a creature does as it notices its player doing something it may copy. The table of deeds names, for each, one of
// five ways of noticing: a helpful, an aggressive, a neutral, a stealing or a playful deed. With empty hands it first
// points at the thing the deed was done to, amazed, following the thing itself with its head; then it turns to the
// camera and looks at it, a second at a time, with the kind's faces; then it plays the kind's clip. Pure agendas,
// tested on their own.

namespace openblack::creature_mind
{

enum class NoticeKind : uint8_t
{
	Helpful,
	Aggressive,
	Neutral,
	Stealing,
	Playful,
};

/// How long it points at the thing, for every kind but the aggressive one, and for that one
inline constexpr float k_NoticePointSeconds = 2.0f;
inline constexpr float k_NoticeAggressivePointSeconds = 0.8f;
/// How long each look at the camera lasts
inline constexpr float k_NoticeLookSeconds = 1.0f;
/// The clips the kinds end with: a helpful deed's one of the first two, one time in two each; an aggressive deed's
/// the third one time in three, then always the fourth
inline constexpr size_t k_NoticeHappyClip = 55;
inline constexpr size_t k_NoticeAmazedClip = 220;
inline constexpr size_t k_NoticeDazedClip = 218;
inline constexpr size_t k_NoticeAngryClip = 53;
inline constexpr size_t k_NoticeStealingClip = 60;
inline constexpr uint32_t k_NoticeHelpfulLots = 2;
inline constexpr uint32_t k_NoticeAggressiveLots = 3;

/// The agenda of noticing a deed of the kind done to the thing (by its entity's number). The point is left out when its
/// hand holds something, and the turn to the camera when it has no camera to turn to (no player); the looks at the camera
/// are the main steps. A helpful deed draws one random number in two, an aggressive one one in three, the others none.
[[nodiscard]] std::vector<Step> NoticeDeed(NoticeKind kind, uint32_t thing, bool handFull, std::optional<glm::vec2> camera,
                                           const Random& random);

} // namespace openblack::creature_mind
