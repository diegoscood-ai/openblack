/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WalkPathSystem.h"

#include <cstdlib>

#include <algorithm>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/CameraTrack.h"
#include "3D/LandIslandInterface.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Registry.h"
#include "ECS/WalkingList.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_WALK_PATH_TRACE") != nullptr;
	return debug_env::TraceLogs(trace, "game");
}
/// A shark's part above the water is drawn only while it is on the walking list (the list's own draw); its part under
/// the water always
void ShowAboveWater(entt::entity thing, bool shown)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || !registry.AllOf<Shark>(thing))
	{
		return;
	}
	if (auto* cut = registry.TryGet<CutByPlane>(thing); cut != nullptr && cut->drawAbove != shown)
	{
		cut->drawAbove = shown;
		registry.SetDirty();
	}
}
} // namespace

bool WalkPathSystem::Start(entt::entity thing, int32_t number, bool forward, float from, float to)
{
	auto track = camera_edits::FindTrack(number);
	if (track == nullptr)
	{
		return false;
	}
	// the list keeps the thing once; its walk is replaced (the old one is leaked in the original). The step is 100 ms
	// of the track a turn, current = duration (as a float) * from
	auto walk = camera_track::StartWalk(*track, forward, from, to);
	auto& registry = Locator::entitiesRegistry::value();
	auto& path = registry.AssignOrReplace<WalkPath>(
	    thing, WalkPath {.number = number, .track = std::move(track), .walk = std::move(walk)});
	// into the list's head once; a shark on the list has its part above the water drawn as well
	ecs::walking_list::Enlist(_walking, thing);
	ShowAboveWater(thing, true);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} track {} forward {} from {} to {} ({} ms, {} points)",
		                   static_cast<uint32_t>(thing), number, forward, from, to, path.track->position.duration,
		                   path.track->position.points.size());
	}
	return true;
}

void WalkPathSystem::ProcessTurn()
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	std::vector<entt::entity> done;
	registry.Each<WalkPath, Transform>(
	    [&](entt::entity entity, WalkPath& path, Transform& transform) {
		    // The point of the track this turn, from its sample; out of the list once it has walked its share of the
		    // way, without moving it this turn; otherwise a step on, and the point on the land's grid
		    const auto at = camera_track::WalkTurn(path.walk, *path.track);
		    if (!at.has_value())
		    {
			    if (Trace())
			    {
				    SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} done at {}", static_cast<uint32_t>(entity),
				                       path.walk.current);
			    }
			    done.push_back(entity);
			    return;
		    }
		    // the position (x, z, relative y = 0), then the 3D object's (coords, no rotation, scale 1.0):
		    // y = GetAltitude + 0. The latter also resets the 3D object's rotation and scale, which the shark's draw sets
		    // again every frame (ECS/Sharks.cpp), so only the position is kept here.
		    // The position is only set: the object is not moved between the map cells as it walks, so it stays in the
		    // list of the cell it was in.
		    transform.position = glm::vec3(at->x, island.GetHeightAt(*at), at->y);
		    if (Trace())
		    {
			    SPDLOG_LOGGER_INFO(
			        spdlog::get("game"), "WALK_PATH: object {} current {} segment {} t {:.6f} -> ({:.4f}, {:.4f}, {:.4f})",
			        static_cast<uint32_t>(entity), path.walk.current, path.walk.runner.GetSegment(),
			        path.walk.runner.GetParameter(), transform.position.x, transform.position.y, transform.position.z);
		    }
	    },
	    entt::exclude<Unavailable>);
	for (const auto entity : done)
	{
		registry.Remove<WalkPath>(entity);
		ShowAboveWater(entity, false);
	}
	// out of the list at the walk's end, and once the thing is gone
	std::erase_if(_walking,
	              [&registry](entt::entity entity) { return !registry.Valid(entity) || !registry.AllOf<WalkPath>(entity); });
}
