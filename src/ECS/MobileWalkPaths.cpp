/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileWalkPaths.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/MobileWalkPath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::MobileWalkPath;
using components::Transform;

namespace
{
bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_WALK_PATH_TRACE") != nullptr;
	return trace;
}

/// SetPos takes MapCoords, fixed point: ftol(x * 6553.6) (0x6078A3), back to x * 1/6553.6 in
/// Game3DObject::SetPosition 0x63B6BD
float MapCoordsRound(float x)
{
	return static_cast<float>(static_cast<int32_t>(x * 6553.60009765625f)) * 0.000152587890625f;
}
} // namespace

bool StartMobileWalkPath(entt::entity entity, int32_t path, bool forward, float from, float to)
{
	auto track = LoadCameraTrack(path);
	if (track == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the list keeps the object once; its DataPath is replaced (the old one is leaked in the original)
	auto& walk = registry.AssignOrReplace<MobileWalkPath>(entity);
	walk.runner = std::make_unique<CameraWayRunner>(track->position);
	walk.track = std::move(track);
	walk.to = to;
	walk.forward = forward;
	walk.step = 100.0f;
	walk.unknown2C = 1.0f;
	// 0x60776B: fild duration, * from
	walk.current = static_cast<float>(walk.track->position.duration) * from;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} track {} forward {} from {} to {} ({} ms, {} points)",
		                   static_cast<uint32_t>(entity), path, forward, from, to, walk.track->position.duration,
		                   walk.track->position.points.size());
	}
	return true;
}

void ProcessMobileWalkPaths()
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	std::vector<entt::entity> done;
	registry.Each<MobileWalkPath, Transform>([&](entt::entity entity, MobileWalkPath& walk, Transform& transform) {
		const auto& track = *walk.track;
		const int32_t duration = track.position.duration; // Running(+4)->way->+0x10
		// 0x6077A2 / 0x6077F4: the sample, ftol of current (forward) or of duration - current, clamped to 0..duration
		int32_t sample = walk.forward ? static_cast<int32_t>(walk.current)
		                              : static_cast<int32_t>(static_cast<float>(duration) - walk.current);
		sample = std::clamp(sample, 0, duration);
		// fn_00844280 on the position way (its point is dropped), then fn_008439C0 on the focus way (Running +8) with
		// the segment and t it left
		walk.runner->Get(sample);
		const auto point = track.focus.Bezier(walk.runner->Segment(), walk.runner->Parameter());
		// fn_00607990: current / duration
		if (!(walk.current / static_cast<float>(duration) < walk.to))
		{
			// 0x607900: out of the list, without moving it this turn
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} done at {}", static_cast<uint32_t>(entity),
				                   walk.current);
			}
			done.push_back(entity);
			return;
		}
		walk.current += walk.step;
		if (static_cast<float>(duration) < walk.current)
		{
			walk.current = static_cast<float>(duration);
		}
		// SetPos(x, z, relative y = 0) then Game3DObject::SetPosition(coords, 0, 0, 0, 1.0): y = GetAltitude + 0. The
		// latter also resets the 3D object's rotation and scale, which the Whale's Draw sets again every frame
		// (ECS/Sharks.cpp), so only the position is kept here.
		const float x = MapCoordsRound(point.x);
		const float z = MapCoordsRound(point.z);
		transform.position = glm::vec3(x, island.GetHeightAt(glm::vec2(x, z)), z);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "WALK_PATH: object {} sample {} segment {} t {:.6f} focus ({:.4f}, {:.4f}, {:.4f}) -> ({:.4f}, "
			                   "{:.4f}, {:.4f})",
			                   static_cast<uint32_t>(entity), sample, walk.runner->Segment(), walk.runner->Parameter(),
			                   point.x, point.y, point.z, transform.position.x, transform.position.y, transform.position.z);
		}
	});
	for (const auto entity : done)
	{
		registry.Remove<MobileWalkPath>(entity);
	}
}

} // namespace openblack::ecs
