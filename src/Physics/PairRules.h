/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace openblack::physics
{

/// What decides whether one body's points are tested against another's faces in a step
struct PairMember
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
	bool resting {false};
	bool justSetUp {false};
	/// Set on what must not hit other objects (a villager's dropped load, building pieces)
	bool noObjectCollision {false};
	bool isVillager {false};
	/// Pushed by a villager, kicked or felled: villagers' bodies pass through it
	bool pushedByLiving {false};
	/// The object, and the one that threw it
	entt::entity object {entt::null};
	entt::entity thrower {entt::null};
};

/// Whether the first body's points are tested against the second's faces this step
[[nodiscard]] inline bool TestsPoints(const PairMember& a, const PairMember& b)
{
	if (a.noObjectCollision)
	{
		return false;
	}
	// Two resting bodies leave each other alone, unless one was just placed
	if (a.resting && b.resting && !a.justSetUp && !b.justSetUp)
	{
		return false;
	}
	if ((a.isVillager && b.pushedByLiving) || (b.isVillager && a.pushedByLiving))
	{
		return false;
	}
	// A thrown thing passes through whoever threw it
	if (a.thrower == b.object || b.thrower == a.object)
	{
		return false;
	}
	const float reach = a.radius + b.radius;
	const auto d = a.centre - b.centre;
	return glm::dot(d, d) < reach * reach;
}

} // namespace openblack::physics
