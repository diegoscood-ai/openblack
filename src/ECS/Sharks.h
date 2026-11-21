/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs
{

/// OPENBLACK_TEST_SHARK: the shark part of Land 1's FollowUs, once the landscape exists: two sharks created at
/// CONVERT_CAMERA_FOCUS(221) and (230) walking the camera tracks 21 and 20 forward from 0 to 1, as the script does.
/// "track,camera[,forward[,from[,to]]]" makes a single one on that track at that camera's focus instead.
void RunSharkDebugHook();

} // namespace openblack::ecs
