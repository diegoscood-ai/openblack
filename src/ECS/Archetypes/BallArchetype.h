/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs::archetypes
{
/// The football, a mobile object of its own table row. The game walks its footballs once a turn, between the fire and
/// the reactions, but a football's own turn does nothing, so nothing here stands for it
class BallArchetype
{
public:
	/// A football at a point, unturned and at its own size, calling the creatures and animals to it as no one's
	static entt::entity Create(const glm::vec3& position);
	/// The script's CREATE of a football: made at a point, then given to the nearest town, however far, for its people
	/// to play with; a town that already has a football still about keeps that one. With no town at all it stays where
	/// it is, given to none, and the scripts' log says so
	static entt::entity CreateForNearestTown(const glm::vec3& position);
	BallArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
