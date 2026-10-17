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

#include <functional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// The electric arcs a lightning bolt leaves crawling over what it strikes. The bolt queues each object it strikes for
// the first time since it last looked for targets (once that search is a fifth of a second old), when the object has
// a model that is not animated and fewer than a hundred arcs live. UR_ObjectArcer, the rule of the always-on SF_OnFire
// utility effect, empties the queue once a turn, newest first: each object gets an atom with five points of its
// model, and every few ticks two of them become the ends of an arc whose ribbon is laid afresh each step along a curve
// leaving and arriving along their normals, jittered, so it writhes. The atom goes with its object, and its group's
// other rules take it away after a few seconds. Wiki:
// docs/wiki/magic/fireball-and-lightning.md#the-arcs-over-what-the-bolt-struck.

namespace openblack::psys::explode_object
{
struct SourceMesh;
}

namespace openblack::psys::object_arcer
{

/// An arc picks its ends among this many points of the object's model
inline constexpr size_t k_ArcPoints = 5;
/// No object is queued while this many arcs live
inline constexpr uint32_t k_MaxArcs = 100;

/// Whether a struck object is queued: it has a model, the model is not animated (its mesh has no bones), and fewer
/// than k_MaxArcs arcs live
[[nodiscard]] constexpr bool Queues(bool hasModel, bool animated, uint32_t liveArcs)
{
	return hasModel && !animated && liveArcs < k_MaxArcs;
}

/// A struck object, queued when Queues allows it; nothing without the particle system
void Queue(entt::entity object);
/// Empties the queue (a new land); the live arcs go with their atoms
void Clear();
/// How many objects wait; 0 without the particle system
[[nodiscard]] size_t QueuedCount();
/// How many arcs live; 0 without the particle system
[[nodiscard]] uint32_t LiveArcs();

/// The points an arc picks its ends among, in the world, with a direction at each
struct ArcPoints
{
	std::vector<glm::vec3> points;
	std::vector<glm::vec3> normals;
};
/// The points of a model placed by `axes` (its columns the model's axes, scaled) and `position`: one of the mesh's
/// sub-meshes at random (every one counts, whatever its detail level), one of its primitives at random, then
/// k_ArcPoints of that primitive's vertices at random, `rand(n)` giving 0 .. n - 1. A point is its vertex through the
/// matrix. Its normal goes through the whole matrix, the translation included, before it is made unit length (unless
/// it is zero): far from the world's origin it points along the model's position rather than off its surface, as
/// the original's does. Nothing when the mesh has no vertex at all, nor when the primitive picked has none (the
/// original reads past it)
[[nodiscard]] ArcPoints PickPoints(const explode_object::SourceMesh& mesh, const glm::mat3& axes, const glm::vec3& position,
                                   const std::function<int32_t(int32_t)>& rand);

} // namespace openblack::psys::object_arcer
