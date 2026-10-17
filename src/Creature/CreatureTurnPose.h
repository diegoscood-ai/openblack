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
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/CreatureTurnPose.h"

namespace openblack::ecs::components
{
struct CreatureAnimation;
}

/// The creature's body as posed for the game turn (ecs::components::CreatureTurnPose): how a new pose takes the place of
/// the last one, the copy kept while the hand holds the creature, how far its animations step in a turn, the body drawn
/// between its last two poses, and the bones the game reads from it. Every sum and product is rounded to a float after
/// each operation, in the order written. Docs: docs/wiki/creature/pose-and-animation.md#the-body-posed-for-the-turn.
namespace openblack::creature_turn_pose
{
/// How many milliseconds the body's animations step in a turn of `turnMs`: the turn's own when it plays at the real
/// speed, else the turn's times (1.6 less (size x 0.5) x 0.85), cut to a whole number towards zero; twice that when
/// `doubled`
[[nodiscard]] int32_t Step(int32_t turnMs, float size, bool realSpeed, bool doubled);

/// How many milliseconds of a turn of `msPerTurn` the creature lives through at a playback scale (1 at the normal
/// speed, less while frozen): the scale held to 0..1, times the turn, cut to a whole number towards zero
[[nodiscard]] int32_t LivedMs(float playbackScale, int32_t msPerTurn);

/// What the creature's animations advance by in one turn, all from the one whole number of milliseconds it lives
struct TurnClock
{
	/// The face, the blink and the moves played at their own speed
	int32_t ms;
	/// The same in seconds: the breath and the head turning towards what it looks at
	float seconds;
	/// The body and the gesture, slower the bigger the creature (Step)
	int32_t step;
};

/// The turn's clock for `livedMs` (LivedMs) at a size
[[nodiscard]] TurnClock ClockFor(int32_t livedMs, float size, bool realSpeed, bool doubled);

/// The turn's clock for a creature living a turn of `msPerTurn` at a playback scale (1 at the normal speed, 1 less the
/// frozen amount while frozen) at a size: ClockFor of LivedMs, so the three clocks come from the one whole number
[[nodiscard]] TurnClock ClockOfTurn(float playbackScale, int32_t msPerTurn, float size, bool realSpeed, bool doubled);

/// Whether the creature has been posed yet
[[nodiscard]] bool HasPose(const ecs::components::CreatureTurnPose& pose);

/// A new pose: the current one becomes the previous one. The first time, the previous one is the new one too
void Advance(ecs::components::CreatureTurnPose& pose, std::vector<glm::mat4> posed);

/// The hand takes the creature off the game: its current pose is kept as it is until it is reconnected. Taken again,
/// the kept pose is the current one again
void Freeze(ecs::components::CreatureTurnPose& pose);

/// The creature is back in the game: the kept pose is dropped
void Reconnect(ecs::components::CreatureTurnPose& pose);

/// The pose the game reads: the kept one while there is one, else the current one; empty when it has no pose yet
[[nodiscard]] std::span<const glm::mat4> Safe(const ecs::components::CreatureTurnPose& pose);

/// Adds `src` weighted by `w` into `dst`, float by float: w x src, plus what `dst` holds unless it is the `first`.
/// Only the twelve floats of each bone's rotation and place are blended; the bottom row of `dst` is set to (0, 0, 0, 1).
/// Only the bones both have
void BlendInto(std::span<glm::mat4> dst, std::span<const glm::mat4> src, float w, bool first);

/// The body as drawn a share of the way through the turn: share x current + (1 - share) x previous, float by float
/// (BlendInto), not normalised again. The share is taken up to 2, so past the turn's end the body is drawn on beyond its
/// current pose. Empty while it has no pose
[[nodiscard]] std::vector<glm::mat4> Drawn(const ecs::components::CreatureTurnPose& pose, float share);

/// The bones of a body posed in its mesh's space, placed in the world by the body's matrix
[[nodiscard]] std::vector<glm::mat4> InWorld(std::span<const glm::mat4> bones, const glm::mat4& world);

/// The bones of a body placed in the world, seen from the body's drawn matrix `row`: row's inverse times each bone,
/// worked in double so that the bones keep their millimetres a few kilometres from the land's origin. Only the top three
/// rows of `row` are read, its bottom row taken as (0, 0, 0, 1). Empty when `row` cannot be inverted
[[nodiscard]] std::vector<glm::mat4> RelativeTo(const glm::mat4& row, std::span<const glm::mat4> world);

/// The bones a body is drawn with this frame, against its drawn matrix: those blended between its last two turn poses
/// when there are any, else those its animations posed this frame
[[nodiscard]] const std::vector<glm::mat4>& BonesDrawn(const ecs::components::CreatureAnimation& animation);

/// A bone's mirror from the creature file's table, the last number of the creature block for each of the body's
/// `boneCount` bones; none when the table does not cover the bones or names no bone
[[nodiscard]] std::optional<uint32_t> MirrorBone(std::span<const int32_t> fileTail, size_t boneCount, uint32_t bone);

/// The mean place of the bones at the ends of the body's tree under a bone. From `start` along the bones that follow it
/// under the same parent: a bone with nothing under it adds its place to the sum and is counted; one with bones under
/// it counts those the same way, from its first. The bones under a bone come in the order of their indices, the first
/// being the lowest. The sum, rounded after each add, is multiplied by 1 / how many were counted. None when the bones
/// or the parents do not cover `start`
[[nodiscard]] std::optional<glm::vec3> LeafMean(std::span<const uint32_t> parents, std::span<const glm::mat4> bones,
                                                uint32_t start);
} // namespace openblack::creature_turn_pose
