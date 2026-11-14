/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsGround.h"

#include "3D/LandIslandInterface.h"
#include "ECS/SeaCells.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

float PhysicsGround::HeightAt(glm::vec2 xz) const
{
	return _land != nullptr ? _land->GetHeightAt(xz) : 0.0f;
}

glm::vec3 PhysicsGround::NormalAt(glm::vec2 xz) const
{
	return _land != nullptr ? _land->GetNormalAt(xz) : glm::vec3(0.0f, 1.0f, 0.0f);
}

bool PhysicsGround::IsSeaCell(glm::vec2 xz) const
{
	if (_land == nullptr)
	{
		return false;
	}
	// the cell rounded to the nearest; off the map or without a block it counts as water
	return !(sea_cells::AltitudeAt(*_land, sea_cells::RoundedCellOf(glm::vec3(xz.x, 0.0f, xz.y))) >= 1);
}

PhysicsGround openblack::ecs::GameGround()
{
	return PhysicsGround(Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr);
}
