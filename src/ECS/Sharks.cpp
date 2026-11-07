/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Sharks.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs
{

void RunSharkDebugHook()
{
	const char* test = std::getenv("OPENBLACK_TEST_SHARK");
	if (test == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	struct Walk
	{
		int32_t track;
		int32_t camera;
		int forward;
		float from;
		float to;
	};
	// Land 1, FollowUs (challenge.chl): Shark1 = CREATE(Whale, 5000, CONVERT_CAMERA_FOCUS(221)) walks WALK_PATH(Shark1,
	// forward, 21, 0.0, 1.0), Shark2 at CONVERT_CAMERA_FOCUS(230) walks track 20
	std::vector<Walk> walks;
	Walk one {0, 0, 1, 0.0f, 1.0f};
	if (std::sscanf(test, "%d,%d,%d,%f,%f", &one.track, &one.camera, &one.forward, &one.from, &one.to) >= 2)
	{
		walks.push_back(one);
	}
	else
	{
		walks.push_back({21, 221, 1, 0.0f, 1.0f});
		walks.push_back({20, 230, 1, 0.0f, 1.0f});
	}
	const auto& island = Locator::terrainSystem::value();
	for (const auto& walk : walks)
	{
		const auto camera = camera_edits::FindCamera(walk.camera);
		if (!camera.has_value())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Shark test: no Cam{} in camera.edt", walk.camera);
			continue;
		}
		// CREATE(Marker, 0, focus), GET_POSITION of it, CREATE(Whale, 5000, that): at GetAltitude there, angle 0, scale 1
		const glm::vec3 focus(camera->focus[0], camera->focus[1], camera->focus[2]);
		const glm::vec3 at(focus.x, island.GetHeightAt(glm::vec2(focus.x, focus.z)), focus.z);
		const auto entity = archetypes::SharkArchetype::Create(at, 0.0f, 1.0f);
		const bool started = Locator::walkPathSystem::value().Start(entity, walk.track, walk.forward != 0, walk.from, walk.to);
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Shark test: shark {} at Cam{} focus ({:.2f}, {:.2f}, {:.2f}), WALK_PATH track {} forward {} from {} "
		    "to {}{}",
		    static_cast<uint32_t>(entity), walk.camera, at.x, at.y, at.z, walk.track, walk.forward, walk.from, walk.to,
		    started ? "" : ": cannot load the track");
	}
}

} // namespace openblack::ecs
