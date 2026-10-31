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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// What the game keeps for this computer rather than for a player: the settings the original kept in its setup's
/// registry key. They are kept in Setup.txt, next to the players' profiles, so a run of the checks, which keeps no
/// profiles, reads and writes none of them either
namespace openblack::machine_setup
{

/// The file the settings are kept in, in the profiles' folder
constexpr std::string_view k_SetupFile = "Setup.txt";

/// The kept settings; a setting never kept is none, and the game's own value stays
struct Setup
{
	/// The sound effects' main volume, 0 to 127 as the game uses it (any number is kept)
	std::optional<uint32_t> sampleMainVolume;
	/// The music's main volume
	std::optional<uint32_t> musicMainVolume;
};

/// The settings as they are kept on disk: a line for each kept one, its name and its value separated by a tab
[[nodiscard]] std::string Write(const Setup& setup);
/// The settings read back; lines that aren't understood, and values that aren't whole numbers that fit, are passed over
[[nodiscard]] Setup Read(std::string_view text);

/// The game's two main volumes as they are now, to be kept
[[nodiscard]] Setup FromVolumes(uint32_t sampleMainVolume, uint32_t musicMainVolume);
/// The kept volumes taken by the game, as its audio takes them: the effects' only up to 127 (a larger one is ignored
/// and the volume stays), the music's whatever it is; a volume not kept leaves the game's
void ApplyVolumes(const Setup& setup, uint32_t& sampleMainVolume, uint32_t& musicMainVolume);

/// The settings kept in a folder: none without a folder, a file, or one that can be read
[[nodiscard]] Setup Load(filesystem::FileSystemInterface& fileSystem, const std::optional<std::filesystem::path>& folder);
/// Keeps the settings in the folder, making it if it is not there; nothing without a folder. Whether they were written
bool Save(filesystem::FileSystemInterface& fileSystem, const std::optional<std::filesystem::path>& folder, const Setup& setup);

} // namespace openblack::machine_setup
