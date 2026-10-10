/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object near the action's point when the click hit none and the point it searches around, the land behind the
// hand seen from the camera. Wiki: docs/bw1-notes/hand-and-interface.md, "Object under the cursor".

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/HandNearObject.h"

#include <limits>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void HandSystem::UpdatePointBehindHand() noexcept
{
	// Once a drawn frame, in the landscape draw: the box (min / max) of the hand's bones 1..n-1 (element 1 first, the
	// root left out), the camera's ray through its centre onto the land (with the plane y = 0 within 7500 m when the
	// land is missed): hit or not, and the point (x, 0, z)
	_pointBehindHand.reset();
	const auto* bones = GetBoneMatrices();
	if (bones == nullptr || bones->empty() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const glm::mat4 model = GetHandMatrix();
	glm::vec3 lo(std::numeric_limits<float>::max());
	glm::vec3 hi(std::numeric_limits<float>::lowest());
	for (size_t i = 1; i < bones->size(); ++i)
	{
		const glm::vec3 joint = glm::vec3(model * (*bones)[i][3]);
		lo = glm::min(lo, joint);
		hi = glm::max(hi, joint);
	}
	const glm::vec3 centre = (lo + hi) * 0.5f;
	const glm::vec3 camera = Locator::camera::value().GetOrigin();
	glm::vec2 hit;
	if (Locator::terrainSystem::value().RayCast(camera, centre, hit, camera))
	{
		_pointBehindHand = glm::vec3(hit.x, 0.0f, hit.y);
	}
}

std::optional<entt::entity> HandSystem::FindObjectNearMapCoord(glm::vec3 at) const noexcept
{
	namespace map_coords = openblack::map_coords;
	// nothing without the point behind the hand or with it at (0, 0, 0)
	if (!_pointBehindHand)
	{
		return std::nullopt;
	}
	// MapCoords c of that point: x, z = m x 6553.6 truncated toward zero, altitude = 0 - the ground height (so 0 only on a cell
	// at height 0)
	const glm::vec3 c = *_pointBehindHand;
	if (map_coords::ToFixed(c.x) == 0 && map_coords::ToFixed(c.z) == 0 &&
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(c.x, c.z)) == 0.0f)
	{
		return std::nullopt;
	}
	// c on the water: the shoal near the action's point (xz distance^2 < 4), then the fish farm of that shoal.
	// (approximate) ecs::FindFishFarmAt walks the farms, not the shoal list then the farm: they only differ where
	// shoals overlap
	// (inferred) !IsLand stands for the original's water test
	if (!hand_detail::IsLand(c))
	{
		if (const auto farm = ecs::FindFishFarmAt(at); farm)
		{
			return farm;
		}
	}
	// the nearest object but a fragment within 5 m of c, if the action's point is not nearer to c
	const auto& registry = Locator::entitiesRegistry::value();
	const auto lookup = [&registry](entt::entity object) -> std::optional<ecs::hand_near::Candidate> {
		if (!ecs::IsAvailable(object))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const Transform>(object);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		return ecs::hand_near::Candidate {.position = transform->position, .fragment = registry.AllOf<Fragment>(object)};
	};
	return ecs::hand_near::NearestToLandBehindHand(c, at, ecs::map_cells::ForEachInCell, lookup);
}
