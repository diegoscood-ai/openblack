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
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Physics.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"

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

bool IsFlying(const Registry& registry, entt::entity thing)
{
	// the mark the physics keeps in step with its moving bodies and its carries, not a physics entry: a resting body
	// keeps its entry
	return registry.Valid(thing) && registry.AllOf<components::InPhysics>(thing);
}

void StandStill(components::CreatureAnimation& animation)
{
	animation.body = {};
	animation.slots.clear();
	// no face shown and none wanted; how long a wanted face was to be held runs on, as it does with none
	animation.face.current.reset();
	animation.face.wanted.reset();
	animation.face.timeMs = 0.0f;
	animation.face.cue = creature_face::Cue::None;
	animation.lookAt.reset();
	animation.yaw = {};
	animation.pitch = {};
}

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

float HeightAboveLand(const LandIslandInterface* island, const glm::vec3& position)
{
	return map_coords::FromWorld(island, position).altitude;
}

glm::vec3 AtHeightAboveLand(const LandIslandInterface* island, const glm::vec3& position, float height)
{
	auto coords = map_coords::FromWorld(island, position);
	coords.altitude = height;
	// only the height changes: the point across stays where it is
	return {position.x, map_coords::ToWorld(island, coords).y, position.z};
}

} // namespace openblack::ecs::script_set_position
