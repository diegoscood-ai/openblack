/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Physics/Ground.h"

namespace openblack
{
class LandIslandInterface;
} // namespace openblack

namespace openblack::ecs
{

/// The island's land as the physics feels it: its height and the flat normal of its triangles, and which of its cells
/// are open sea. Without an island the land is flat at the sea's level, faces up and has no sea.
class PhysicsGround final: public openblack::physics::Ground
{
public:
	explicit PhysicsGround(const LandIslandInterface* land)
	    : _land(land)
	{
	}

	[[nodiscard]] float HeightAt(glm::vec2 xz) const override;
	[[nodiscard]] glm::vec3 NormalAt(glm::vec2 xz) const override;
	[[nodiscard]] bool IsSeaCell(glm::vec2 xz) const override;

private:
	const LandIslandInterface* _land;
};

/// The ground of the game's island, or of no island when there is none yet
[[nodiscard]] PhysicsGround GameGround();

} // namespace openblack::ecs
