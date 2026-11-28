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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class TreeArchetype
{
public:
	/// Whether a tree is still growing, and the turns left to its next growth step
	struct Growth
	{
		bool growing {false};
		uint16_t counter {0};
	};

	/// The growth a new tree starts with: it grows only when maxSize differs from the size it is made at, and then its
	/// first growth step falls on a random turn of [0, growTurns) (GameRand(growsAfterNumGameTurns), 0 for 0 without
	/// a draw); a tree that does not grow draws nothing
	[[nodiscard]] static Growth NewGrowth(TreeInfo type, float maxSize, float scale);

	/// A new tree, with the growth NewGrowth gives it
	static entt::entity Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
	                           float yAngleRadians, float maxSize, float scale);
	/// A tree with the growth it is given: draws no random number
	static entt::entity Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
	                           float yAngleRadians, float maxSize, float scale, Growth growth);
	TreeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
