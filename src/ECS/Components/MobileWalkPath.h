/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "3D/CameraTracks.h"

namespace openblack::ecs::components
{
/// The DataPath (0x30 bytes, MobileObject +0x64) of a MobileObject walking a camera track (CHL WALK_PATH,
/// fn_006076C0), while it is in the list g_game+0x205CD4 (GlobalGameLists +0x130). ECS/MobileWalkPaths.h.
struct MobileWalkPath
{
	std::shared_ptr<const CameraTrack> track; ///< +0x14: ScriptedCamera::Create(path) 0x447060
	std::unique_ptr<CameraWayRunner> runner;  ///< the ScriptedCamera's Running on the position way (+4)
	float to {1.0f};                          ///< +0x1C: leaves the list once current / duration >= to
	bool forward {true};                      ///< +0x20: false walks the samples from the end
	float current {0.0f};                     ///< +0x24: from * duration, then + step each turn up to the duration
	float step {100.0f};                      ///< +0x28: 100 ms of the track per game turn (a turn is 100 ms)
	float unknown2C {1.0f};                   ///< +0x2C: 1, not read by MoveAlongPath
};
} // namespace openblack::ecs::components
