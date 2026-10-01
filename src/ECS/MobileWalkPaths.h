/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// The CHL WALK_PATH of a MobileObject (docs/bw1-notes/camera-tracks.md): the object follows the focus way of the
/// camera track "Track<path>" of Data\camera.edt, timed by its position way, 100 ms of the track per game turn.

/// fn_006076C0(path, from, to, forward), from GScript::WalkPath 0x6FBB50 for a thing that is not Living: into the list
/// g_game+0x205CD4 once, and a new DataPath (+0x64): the track, to, forward, step 100, current = from * duration.
/// False when the track cannot be read (the original then crashes on the null ScriptedCamera).
bool StartMobileWalkPath(entt::entity entity, int32_t path, bool forward, float from, float to);

/// GlobalGameLists::Process 0x5913ED, every game turn after Whale::ProcessAll: MobileObject::MoveAlongPath 0x607790
/// for each object of the list. OPENBLACK_WALK_PATH_TRACE=1 logs every turn's sample, segment, t and point.
void ProcessMobileWalkPaths();

} // namespace openblack::ecs
