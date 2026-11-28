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

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// The formulas of the flocking rule (Particles/Rules/Flock.cpp), for the tests. Wiki: docs/wiki/magic/flocks.md#flocks.

namespace openblack::particles
{
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::flocking
{
/// The flock's acceleration law: x = max(distance, 0.01) x ScaleModifier; type 0 -> 1, 1 -> x, 2 -> x^2;
/// 1 / that unless inverted
[[nodiscard]] float AccelerationLaw(float distance, int type, bool invert, float scaleModifier);

/// The flocking rule's banking: the atom's rotation (openblack's columns = the original's rows) from its new velocity
/// and the acceleration it just took: RotY(-pi / 2) x Rx(bank) x Rz(-pitch) x RotY(yaw), with yaw =
/// atan2(v.z, v.x), pitch = atan2(v.y, |v.xz|) x ReducePitchBy and bank = atan((a.z v.x - a.x v.z) / |v.xz| /
/// GravityForBanking) (0 with no horizontal speed). The local +Z axis points along the velocity.
[[nodiscard]] glm::mat3 BankingRotation(const glm::vec3& velocity, const glm::vec3& acceleration, float reducePitchBy,
                                        float gravityForBanking);

/// The follow-targets rule: whether a new follower with a target plays SoundCreate: always, or with SoundOneOnly only when it
/// is its collection's only atom (the first of the cast)
[[nodiscard]] bool PlaysCreateSound(bool soundOneOnly, std::size_t atomsInCollection);
/// The flocking rule's data on its collection, as a save writes it: the flock's own velocity, which it keeps between
/// steps; none for a rule of another class, not on the collection or that has not made it (a collection that has had
/// no atoms)
[[nodiscard]] std::optional<glm::vec3> FlockVelocityOf(const Collection& collection, const Modifier& rule);

/// For a load: the flock's velocity on its collection as the save kept it, noted as made; false, with nothing changed,
/// for a rule of another class or not on the collection
bool RestoreFlockVelocity(Collection& collection, const Modifier& rule, const glm::vec3& velocity);
} // namespace openblack::particles::flocking
