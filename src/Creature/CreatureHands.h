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
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::creature
{
struct CreatureRig;
}
namespace openblack::ecs
{
class Registry;
}

/// Where a creature holds its hands as it casts a miracle, as the game finds them: halfway between the mean place of
/// the bones at the ends of its hand, its fingertips, and the same of the hand's mirror. They are read from the body as
/// posed for the game turn (creature_turn_pose::Safe), never from the body drawn between turns. Docs:
/// docs/wiki/creature/casting.md#casting-a-miracle-the-originals-steps.
namespace openblack::creature_hands
{
/// Halfway between the mean place of the ends under the hand bone and under its mirror (creature_turn_pose::LeafMean),
/// each sum and half a float; none when the bones or the parents do not cover either
[[nodiscard]] std::optional<glm::vec3> HandPoint(std::span<const uint32_t> parents, std::span<const glm::mat4> bones,
                                                 uint32_t hand, uint32_t mirror);

/// The way from the hands to a point, made one long as the game makes it, times 1 / sqrt((z² + y²) + x²); left as it is
/// when it has no length, a part that is not a number counting as none
[[nodiscard]] glm::vec3 Towards(glm::vec3 point, glm::vec3 hands);

/// A species' hand bone, the first of its action points, and its mirror from the creature file's table, for a body of
/// so many bones; none when the species has no action points or the table names no mirror
[[nodiscard]] std::optional<std::pair<uint32_t, uint32_t>> HandBones(const creature::CreatureRig& rig, size_t boneCount);

/// A creature's hands in the pose the game reads, its species' rig given; none before it is posed or for a body its
/// rig does not cover
[[nodiscard]] std::optional<glm::vec3> HandsOf(const ecs::Registry& registry, entt::entity creature,
                                               const creature::CreatureRig& rig);

/// Where the beams of a cast from above start: the places of the hand bone's mirror and of the hand bone themselves in
/// that pose, the mirror's first; none as for HandsOf
[[nodiscard]] std::optional<std::pair<glm::vec3, glm::vec3>> BeamPointsOf(const ecs::Registry& registry, entt::entity creature,
                                                                          const creature::CreatureRig& rig);
} // namespace openblack::creature_hands
