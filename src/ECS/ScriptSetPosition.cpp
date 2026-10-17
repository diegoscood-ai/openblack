/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSetPosition.h"

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/MapCells.h"

namespace openblack::ecs::script_set_position
{

namespace
{
/// About Y alone: the Y axis stays up and the other two axes stay level
bool AboutYAlone(const glm::mat3& rotation)
{
	return rotation[1] == glm::vec3(0.0f, 1.0f, 0.0f) && rotation[0].y == 0.0f && rotation[2].y == 0.0f;
}
} // namespace

glm::mat3 UprightRotation(const glm::mat3& rotation)
{
	if (AboutYAlone(rotation))
	{
		return rotation;
	}
	return affine::AngleY(map_cells::detail::YAngleOf(rotation));
}

float FocusYAngle(const glm::vec3& position, const glm::vec3& target)
{
	// from its place in whole map units; the angle does not read the height
	const glm::vec3 from(map_coords::Quantise(position.x), position.y, map_coords::Quantise(position.z));
	return static_cast<float>(affine::GetYAngleBetween(from, target));
}

std::optional<glm::mat3> FocusRotation(const glm::mat3& rotation, float yAngle, bool keepsTilt)
{
	if (!keepsTilt)
	{
		return affine::AngleY(yAngle);
	}
	// a turn about Y alone has its x and z angles at 0
	if (!AboutYAlone(rotation))
	{
		return std::nullopt;
	}
	return affine::RotationYXZ(yAngle, 0.0f, 0.0f);
}

} // namespace openblack::ecs::script_set_position
