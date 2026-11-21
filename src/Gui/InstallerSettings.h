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
#include <string>

/// The settings the game's installer keeps for the player, under the current user's key of the game in the Windows
/// registry, and the user name a Windows setup kept there. Away from Windows there are none. Wiki:
/// docs/wiki/video/playback.md#startup-and-loading-screen, docs/wiki/audio/voices.md#guidance-and-night-voices-in-openblack.
namespace openblack::gui::installer_settings
{

/// A number the installer keeps (a double word value), none when it is not there
[[nodiscard]] std::optional<uint32_t> Number(const char* name);

/// The user name an old Microsoft setup kept for the current user (the "User Info" key's DefName text, at most 511
/// bytes), made wide in the system's code page; empty when it is not there
[[nodiscard]] std::u16string SetupUserName();

} // namespace openblack::gui::installer_settings
