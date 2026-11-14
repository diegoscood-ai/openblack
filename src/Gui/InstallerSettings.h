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

#include <optional>

/// The settings the game's installer keeps for the player, under the current user's key of the game in the Windows
/// registry. Away from Windows there are none. Wiki: docs/wiki/video/playback.md#startup-and-loading-screen.
namespace openblack::gui::installer_settings
{

/// A number the installer keeps (a double word value), none when it is not there
[[nodiscard]] std::optional<uint32_t> Number(const char* name);

} // namespace openblack::gui::installer_settings
