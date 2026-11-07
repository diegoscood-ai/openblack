/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Resources/CreatureFileWriter.h"

#include <exception>
#include <vector>

#include <MindFile.h>
#include <PhysiqueFile.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

namespace openblack::resources
{

bool WriteFile(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path, std::span<const uint8_t> bytes)
{
	try
	{
		auto stream = fileSystem.Open(path, filesystem::Stream::Mode::Write);
		if (stream == nullptr)
		{
			return false;
		}
		stream->Write(bytes.data(), bytes.size());
		return true;
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not write {}: {}", path.generic_string(), e.what());
		return false;
	}
}

std::filesystem::path BackupPath(const std::filesystem::path& path)
{
	auto backup = path;
	backup += ".bak";
	return backup;
}

bool KeepBackupOnce(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path)
{
	// The backup is openblack's own, a protection of the user's file: the original game only writes over it. It is
	// made once, from the file as the user had it, and then left alone
	const auto backup = BackupPath(path);
	if (!fileSystem.Exists(path) || fileSystem.Exists(backup))
	{
		return true;
	}
	std::vector<uint8_t> bytes;
	try
	{
		bytes = fileSystem.ReadAll(path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not read {} to back it up: {}", path.generic_string(), e.what());
		return false;
	}
	return WriteFile(fileSystem, backup, bytes);
}

bool SaveCreatureMind(filesystem::FileSystemInterface& fileSystem, CreatureMindManager& minds, const std::string& id,
                      const std::filesystem::path& path, const creaturemind::MindFileData& mind)
{
	// without its backup the user's file is not written over: it stays as it was
	if (!KeepBackupOnce(fileSystem, path))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "{} is not saved: its backup could not be made", path.generic_string());
		return false;
	}
	const auto bytes = creaturemind::Write(mind);
	if (!WriteFile(fileSystem, path, bytes))
	{
		return false;
	}
	minds.Erase(id);
	return true;
}

bool SaveCreaturePhysique(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path,
                          const creaturemind::PhysiqueFileData& physique)
{
	if (!KeepBackupOnce(fileSystem, path))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "{} is not saved: its backup could not be made", path.generic_string());
		return false;
	}
	return WriteFile(fileSystem, path, creaturemind::WritePhysique(physique));
}

} // namespace openblack::resources
