/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>

/// The two main volumes kept for this computer (machine_setup), as the original's audio keeps them: read as the audio
/// starts and each time the options box shows, kept each time it goes and as the game ends
namespace openblack::audio::main_volumes
{

/// The folder they are kept in: the profiles', none in a run of the checks or without the profiles
[[nodiscard]] std::optional<std::filesystem::path> Folder();
/// The kept volumes taken into the configuration (machine_setup::ApplyVolumes); nothing when none are kept
void Read();
/// The configuration's volumes kept; whether they were written
bool Keep();

} // namespace openblack::audio::main_volumes
