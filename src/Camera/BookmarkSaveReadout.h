/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>

#include "ECS/Systems/CameraBookmarkSystemInterface.h"

/// The debug readouts of what a saved game keeps of the camera's bookmarks and help: one line each, for the log
namespace openblack::bookmark_save_readout
{

/// Every set bookmark by its index with its place on the land in metres (and "on a thing" when it follows one), then the
/// one followed, the current one and whether the bookmarks are on; "none set" when no bookmark is
[[nodiscard]] std::string Describe(const ecs::systems::SavedBookmarks& saved);

/// The camera help's features, its auto-pitch angle and its auto-pitch height
[[nodiscard]] std::string DescribeCameraHelp(int32_t features, float autoPitchAngle, float autoPitchHeight);

} // namespace openblack::bookmark_save_readout
