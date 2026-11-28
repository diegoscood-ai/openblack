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

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{
/// What the glints on a target read of the objects they are given: whether the object is still there, the points of
/// its model they sit on, and the scale they are drawn at. A script highlight and a spell seed graphic have points;
/// any other object has none
class GlintTargetsInterface
{
public:
	virtual ~GlintTargetsInterface() = default;

	/// Where the object is while it is available; none once it is gone or being deleted
	[[nodiscard]] virtual std::optional<glm::vec3> ObjectPosition(entt::entity object) const = 0;
	/// How many points its model has: every vertex of every part, 0 without a model
	[[nodiscard]] virtual uint32_t TargetPointCount(entt::entity object) const = 0;
	/// The index-th point in the world, through the model's matrix as the glints read it; none past the last
	[[nodiscard]] virtual std::optional<glm::vec3> TargetPoint(entt::entity object, uint32_t index) const = 0;
	/// The object's own scale, which a new glint's size is multiplied by; 1 for an object with no points
	[[nodiscard]] virtual float TargetScale(entt::entity object) const = 0;
};
} // namespace openblack::ecs::systems
