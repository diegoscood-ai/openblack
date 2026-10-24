/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object near the action's point when the click hit none and the point it searches around, the land behind the
// hand seen from the camera; and a hand demo's click found again on the recorded kind of thing. Wiki:
// docs/wiki/hand/picking.md#object-under-the-cursor, docs/wiki/project/debugging.md#hand-demos.

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/HandNearObject.h"

#include <limits>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "EngineConfig.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Input/HandDemo.h"
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
	if (bones == nullptr || bones->empty() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::pickingSystem::has_value())
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
	if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(camera, centre, camera))
	{
		_pointBehindHand = glm::vec3(hit->x, 0.0f, hit->z);
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

void HandSystem::RetargetDemoClick(const hand_demo::Record& click) noexcept
{
	if (!hand_demo::ClickCanRetarget(click.message) || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the thing under the hand, as the scripts see it; its sub-type is read only when its type is the recorded one
	std::optional<hand_demo::ThingUnderHand> underHand;
	if (_cursorObject && registry.Valid(*_cursorObject) && ecs::IsAvailable(*_cursorObject))
	{
		const auto type = ecs::script_type::TypeOf(*_cursorObject);
		underHand = hand_demo::ThingUnderHand {.type = type,
		                                       .subtype = static_cast<uint32_t>(type) == click.objectType
		                                                      ? ecs::script_type::SubtypeOf(*_cursorObject)
		                                                      : ecs::script_type::k_NoSubtype,
		                                       .at = ecs::object::MapCoordsOf(*_cursorObject)};
	}
	switch (hand_demo::TargetOfClick(click, underHand))
	{
	case hand_demo::ClickTarget::UnderHand:
		return;
	case hand_demo::ClickTarget::NoThing:
		// the click's object is cleared; the action's point stays
		_cursorObject.reset();
		_cursorObjectDistance.reset();
		return;
	case hand_demo::ClickTarget::NearestOfKind:
		break;
	}
	// the nearest thing of the recorded kind within the reach of the recorded place, measured to it (to the totem for a
	// worship site); the thing's sub-type is not read when any will do
	const auto place = hand_demo::RecordedPlace(click);
	const auto found = ecs::map_cells::FindNearForScript(
	    place,
	    [&click](entt::entity thing) {
		    const auto type = ecs::script_type::TypeOf(thing);
		    if (static_cast<uint32_t>(type) != click.objectType)
		    {
			    return false;
		    }
		    const auto subtype = click.objectSubtype == ecs::script_type::k_AnySubtype ? ecs::script_type::k_NoSubtype
		                                                                               : ecs::script_type::SubtypeOf(thing);
		    return hand_demo::TakesRecordedClick(
		        click, {.type = type, .subtype = subtype, .at = ecs::map_cells::ScriptDistancePoint(thing)});
	    },
	    hand_demo::k_RetargetReach);
	const bool isFound = found != entt::null;
	const auto outcome =
	    hand_demo::OutcomeOfFind(isFound, isFound && ecs::script_type::IsObject(found), isFound && Interactable(found));
	switch (outcome)
	{
	case hand_demo::FoundClick::Unchanged:
		// none found, or not an object: the click stays on what is under the hand
		return;
	case hand_demo::FoundClick::Thing:
		_cursorObject = found;
		_cursorObjectDistance = click.objectDistance;
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "A replayed click lands on entity {}, of its recorded kind", entt::to_integral(found));
		}
		return;
	case hand_demo::FoundClick::NoThing:
		// an object the hand cannot act on: the click lands on no thing. The original clears the click's point too, so
		// the press looks for a thing from the map's origin instead; with the vanilla bugs fixed the point stays where
		// the click was
		_cursorObject.reset();
		_cursorObjectDistance.reset();
		if (!Locator::config::has_value() || !Locator::config::value().fixVanillaBugs)
		{
			_interactionPoint = glm::vec3(0.0f);
		}
		return;
	}
}
