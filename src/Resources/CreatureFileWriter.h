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

#include <filesystem>
#include <span>
#include <string>

#include "Resources/ResourcesInterface.h"

namespace openblack::creaturemind
{
struct MindFileData;
struct PhysiqueFileData;
} // namespace openblack::creaturemind

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// Saving creature files: the bytes go out through the file system service, never straight to disk, and the mind
/// cache's copy of a mind file that is written is dropped, so that the next load through the cache reads it back
namespace openblack::resources
{

/// Writes the bytes to the path, making or replacing the file. False when it cannot be opened for writing
bool WriteFile(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path, std::span<const uint8_t> bytes);

/// The name a file's one-time backup takes: the file's name with ".bak" after it
[[nodiscard]] std::filesystem::path BackupPath(const std::filesystem::path& path);

/// Before a file is written over for the first time, its bytes are copied once to its backup (BackupPath), which is
/// never touched again once it exists. True when the file may be written over: there is no file yet, the backup is
/// there already, or it has just been made. False when the backup could not be made
bool KeepBackupOnce(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path);

/// Writes a mind file, after its one-time backup, and drops the cache's entry for it (`id`, the name it is loaded
/// under). False when the backup or the file cannot be written; the file and the cache are then left as they were
bool SaveCreatureMind(filesystem::FileSystemInterface& fileSystem, CreatureMindManager& minds, const std::string& id,
                      const std::filesystem::path& path, const creaturemind::MindFileData& mind);

/// Writes a physique file, after its one-time backup when there is a file already. False when the backup or the file
/// cannot be written; the file is then left as it was
bool SaveCreaturePhysique(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path,
                          const creaturemind::PhysiqueFileData& physique);

} // namespace openblack::resources
